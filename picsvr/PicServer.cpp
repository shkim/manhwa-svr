#include "stdafx.h"

#ifdef _WIN32
#include <agents.h>
//#include <iostream>
#else
#endif

using namespace web;
using namespace http;
using namespace utility;
using namespace concurrency;
using namespace web::http;
using namespace web::http::client;
using namespace http::experimental::listener;

#include "TitleVolume.h"
#include "SerialJobQ.h"
#include "readconf.h"
#include "dbglog.h"

CachePicFile g_cachePics;
VolumeManager g_volMan;
LogManager g_logman;
SerialJobProcessor g_serialJQ;
static utility::string_t s_strApiBaseUrl;

static void ReplyBadRequest(http_request& request)
{
	request.reply(status_codes::BadRequest, U("Invalid path."));
}

static void ReplyInvalidUri(http_request& request)
{
	request.reply(status_codes::NotFound, U("Unknown URI."));
}

#if 0
static void ReplyAsync(const http_request& request)
{
	// A task completion event that is set when a timer fires.
	task_completion_event<void> tce;

	const int timeout = 5*1000;

	// Create a non-repeating timer.
	auto fire_once = new timer<int>(timeout, 0, nullptr, false);
	// Create a call object that sets the completion event after the timer fires.
	auto callback = new call<int>([tce](int)
	{
		printf("callback closuer called\n");
		tce.set();
	});

	// Connect the timer to the callback and start the timer.
	fire_once->link_target(callback);
	fire_once->start();

	// Create a task that completes after the completion event is set.
	task<void> event_set(tce);

	// Create a continuation task that cleans up resources and 
	// and return that continuation task. 
	event_set.then([callback, fire_once, request]()
	{
		printf("event_set.then called\n");

		WCHAR buff[128];
		StringCchPrintf(buff, 128, U("hup... %d"), rand());
		request.reply(status_codes::OK, buff);

		delete callback;
		delete fire_once;
	});

	printf("ReplyAsync returns %d\n", rand());
}
#endif

static void HandleGetPic(http_request request, const std::shared_ptr<ParamGetPic>& param)
{
	if (param->nVolumeId <= 0 || param->nVolumeId > g_volMan.GetMaxVolumeId())
	{
		request.reply(status_codes::NotFound, U("Invalid Volume ID"));
		return;
	}

	TitleVolume* pVolume = g_volMan.GetVolume(param->nVolumeId);
	if (pVolume == NULL)
	{
		request.reply(status_codes::NotFound, U("Volume not found"));
		return;
	}

	auto predicate = [request, pVolume, param](bool addSessionCache) {
		if (addSessionCache)
		{
			pVolume->AddSessionCache(param->sessionId);
		}

		PicFile* pFile = g_cachePics.FindPicFile(param->nVolumeId, param->nPageId);
		if (pFile != NULL)
		{
			LogD("Hit pic %d cache.", param->nPageId);
			pFile->Reply(request);
			return;
		}

		pFile = pVolume->GetPicFile(param->nPageId);
		if (pFile != NULL)
		{
			pFile->Reply(request);
		}
		else
		{
			request.reply(status_codes::NotFound, U("Picture not found"));
		}
	};

	if (pVolume->HasSessionCache(param->sessionId))
	{
		//_tprintf(_T("HitSessionCache: %s for Vol%d\n"), param->sessionId.c_str(), param->nVolumeId);
		predicate(false);
	}
	else
	{
		_tprintf(_T("QuerySession: %s for Vol%d\n"), param->sessionId.c_str(), param->nVolumeId);

		TCHAR szURL[128];
		_sntprintf(szURL, 128, U("%s/v1/drm/%s/%d"), s_strApiBaseUrl.c_str(), param->sessionId.c_str(), param->nVolumeId);
		
		http_client client(szURL);
		client.request(methods::GET).then([request, predicate](http_response resp)
		{
			const string_t& drmRes = resp.extract_string().get();
			if (!drmRes.compare(U("OK")))
			{
				predicate(true);
			}
			else
			{
				request.reply(status_codes::Unauthorized, U("Unauthorized"));
			}
		});
	}
}

extern bool GetZipFileInfo(int nVolumeId, std::string& outStr);

static void HandleGetFileList(http_request& request, int nVolumeId)
{
	std::string json;
	if (!GetZipFileInfo(nVolumeId, json))
	{
		request.reply(status_codes::NotFound, U("ZIP file failed.")); 
		return;
	}

	if (nVolumeId > g_volMan.GetMaxVolumeId())
	{
		g_volMan.SetMaxVolumeId(nVolumeId);
	}

	uint8_t* rawptr = const_cast<uint8_t*>((uint8_t*)json.c_str());
	Concurrency::streams::rawptr_buffer<uint8_t> rawbuf(rawptr, json.length(), std::ios::in);
	request.reply(status_codes::OK, rawbuf.create_istream(), json.length(), U("application/json")).wait();
}

static void HandleRestGet(http_request request)
{
	const auto& reluri = request.relative_uri();
	LogConsoleT(_T("GET: %s\n"), reluri.path().c_str());
	auto paths = uri::split_path(uri::decode(reluri.path()));

	if (paths.empty())
	{
		ReplyBadRequest(request);
		return;
	}

	const string_t& firstToken = paths.at(0);
	if (!firstToken.compare(U("pic")))
	{
		if (paths.size() >= 4)
		{
			ParamGetPic param;
			param.sessionId = paths.at(1);
			param.nVolumeId = _ttoi(paths.at(2).c_str());
			param.nPageId = _ttoi(paths.at(3).c_str());
			HandleGetPic(request, std::make_shared<ParamGetPic>(param));
			return;
		}
	}
	else if (!firstToken.compare(U("admin")))
	{
		if (paths.size() >= 3)
		{
			if (!paths.at(1).compare(U("GetFileList")))
			{
				int nVolumeId = _ttoi(paths.at(2).c_str());
				HandleGetFileList(request, nVolumeId);
				return;
			}
		}
	}

	ReplyInvalidUri(request);
}

extern void HandleVolumeUpload(http_request& request, int nVolumeId);

static void HandleRestPost(http_request request)
{
	const auto& reluri = request.relative_uri();
	LogConsoleT(_T("POST: %s\n"), reluri.path().c_str());
	auto paths = uri::split_path(uri::decode(reluri.path()));

	if (paths.empty())
	{
		ReplyBadRequest(request);
		return;
	}

	const string_t& firstToken = paths.at(0);
	if (!firstToken.compare(U("admin")))
	{
		if (paths.size() >= 4
		&& !paths.at(1).compare(U("volume"))
		&& !paths.at(2).compare(U("upload")))
		{
			int nVolumeId = _ttoi(paths.at(3).c_str());
			HandleVolumeUpload(request, nVolumeId);
			return;
		}
	}

	ReplyInvalidUri(request);
}

void HandleRestOptions(http_request request)
{
	const auto& reluri = request.relative_uri();
	LogConsoleT(_T("OPTIONS: %s\n"), reluri.path().c_str());
	auto paths = uri::split_path(uri::decode(reluri.path()));

	if (paths.empty())
	{
		ReplyBadRequest(request);
		return;
	}

	// TODO: show request headers

	/* request:
	
	Origin: http://urizip.ogp.kr
	Access-Control-Request-Method: POST
	Access-Control-Request-Headers: cache-control,pragma

	*/

	http_response response(status_codes::OK);
	auto& headers = response.headers();
	headers.add(U("Access-Control-Allow-Origin"), U("*"));
	/*
	headers.add(U("Access-Control-Allow-Methods"), U("POST, GET, OPTIONS"));
	headers.add(U("Access-Control-Allow-Headers"), U("X-PINGOTHER"));
	headers.add(U("Access-Control-Max-Age"), U("1728000"));
	headers.add(U(""), U(""));
*/
	response.set_body("");
	request.reply(response);

	//ReplyInvalidUri(request);
}

#define SEC_PURGE_TIMER		180

#ifdef _WIN32
static BOOL WINAPI CtrlHandler(DWORD nSignal)
#else
static void OnSignal(int nSignal)
#endif
{
	//LogConsole("Got console signal: %d\n", nSignal);
#ifndef _WIN32
	if (nSignal == SIGALRM)
	{
		// purge timer
		g_serialJQ.AddJob_Purge();
		alarm(SEC_PURGE_TIMER);
		return;
	}
#endif
	g_serialJQ.FireShutdown();
#ifdef _WIN32
	return TRUE;
#endif
}

static void ServerMain(ConfigSection* pSect)
{
	int nCacheSizeMB = pSect->GetInteger("CacheSizeMB", 512);
	g_cachePics.SetMaxSizeMB(nCacheSizeMB);

	const char* pszApiSvr = pSect->GetString("ApiBaseUrl");
	if (pszApiSvr == NULL)
	{
		LogE("ApiBaseUrl is not specified. (check ini file)");
		return;
	}

	s_strApiBaseUrl = PathFilenamer::A2T(pszApiSvr);
	LogI("API BaseURL: %s", PathFilenamer::T2A(s_strApiBaseUrl.c_str()));

	if (!g_volMan.SetDataDir(pSect->GetFilename("DataDir")))
	{
		return;
	}

	// get max volume id number
	{
		TCHAR szURL[128];
		_sntprintf(szURL, 128, U("%s/admin/getMaxVolume"), s_strApiBaseUrl.c_str());
		http_client client(szURL);
		client.request(methods::GET).then([](task<http_response> responseTask)
		{
			try
			{
				http_response resp = responseTask.get();

				const web::json::value& rj = resp.extract_json().get();
				const web::json::value& resCode = rj.at(U("result"));
				if (resCode.is_string() && resCode.as_string().compare(U("OK")) == 0)
				{
					const web::json::value& data = rj.at(U("data"));
					if (data.is_object())
					{
						const web::json::value& maxId = data.at(U("max_id"));
						if (maxId.is_integer())
						{
							int nMaxId = maxId.as_integer();
							if (nMaxId > 0)
							{
								g_volMan.SetMaxVolumeId(nMaxId);
								return;
							}
						}
					}
				}

				LogE("Invalid result from initial getMaxVolume");
			}
			catch (const http_exception &ex)
			{
				LogE("getMaxVolume failed: %s\n", ex.error_code().message().c_str());				
			}

			g_serialJQ.FireShutdown();
		});
	}

	int nWebPort = pSect->GetInteger("ListenPort", 8000);

	uri_builder uri;
	uri.set_scheme(U("http"));
#if defined(_WIN32) && defined(_DEBUG)
	uri.set_host(U("localhost"));
#else
	uri.set_host(U("*"));
#endif
	uri.set_port(nWebPort);

	http_listener* pRestApiListener = new http_listener(uri.to_uri());
	pRestApiListener->support(methods::GET, std::bind(HandleRestGet, std::placeholders::_1));
	pRestApiListener->support(methods::POST, std::bind(HandleRestPost, std::placeholders::_1));
	pRestApiListener->support(methods::OPTIONS, std::bind(HandleRestOptions, std::placeholders::_1));
	pRestApiListener
		->open()
		.then([nWebPort]() { LogI("Start listening on port %d...", nWebPort); })
		.wait();

#ifdef _WIN32
	SetConsoleCtrlHandler(CtrlHandler, TRUE);

	call<int> timerCaller([](int x) { g_serialJQ.AddJob_Purge();  });
	timer<int> purgeTimer(1000 * SEC_PURGE_TIMER, NULL, &timerCaller, true);
	purgeTimer.start();

#else
	struct sigaction sa;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NODEFER;// | SA_RESETHAND;
	sa.sa_handler = OnSignal;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGALRM, &sa, NULL);
	alarm(SEC_PURGE_TIMER);
#endif

	/*		long allocReqNum;
	void* my_pointer = malloc(10);
	_CrtIsMemoryBlock(my_pointer, 10, &allocReqNum, NULL, NULL);
	printf("allocReqNum: %d\n", allocReqNum);
	//_CrtSetBreakAlloc(733);
	*/

	g_serialJQ.EnterLoop();
	LogD("Gracefully quit from SerialJQ Loop");

	pRestApiListener->close().wait();
	delete pRestApiListener;
}

int main(int argc, char* argv[])
{
	INIT_CRT_DEBUG_REPORT();
	//_CrtSetBreakAlloc(2073);

	ConfigFile ini;
	if (!ini.Load(_T("picsvr.ini")))
	{
		LogConsoleA("Config file loading failed.\n");
		return -1;
	}


	ConfigSection* pSect = ini.GetSection(NULL);
	if (!g_logman.Open(pSect))
	{
		g_logman.Close();
		return -1;
	}

	try
	{
		ServerMain(pSect);

		g_cachePics._DumpLruState();

		g_cachePics.Clear();
		g_volMan.Destroy();
		g_serialJQ.Destroy();
	}
	catch (std::exception const & e)
	{
		std::wcout << e.what() << std::endl;
	}

	g_logman.Close();
	return 0;
}

