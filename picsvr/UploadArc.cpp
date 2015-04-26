#include "stdafx.h"
#include "dbglog.h"
#include "TitleVolume.h"
#include "cpprest/producerconsumerstream.h"

using namespace web::http;
using namespace Concurrency::streams;

#define READBUFF_SIZE		(8*1024)
#define MAX_BOUNDARY_SIZE	128

struct ReadBuff
{
	BYTE buff[READBUFF_SIZE];
};

class ArcUploadHandler
{
	enum States
	{
		STATE_CHECK_BOUNDARY,
		STATE_READ_HEADER,
		STATE_CONTENT,
		STATE_FINISHED
	};

public:
	ArcUploadHandler(int nVolumeId)
	{
		m_nVolumeId = nVolumeId;
		m_state = STATE_CHECK_BOUNDARY;
		m_iBuffPtr = 0;
	}

	pplx::task<bool> FeedBodyData(std::shared_ptr<ReadBuff> rbuf, size_t len);
	
	void SetBoundary(utility::string_t& boundary)
	{
		size_t nBoundaryLength = 2 + boundary.length();
		m_boundary.resize(nBoundaryLength);
		m_boundary[0] = '-';
		m_boundary[1] = '-';
		for (size_t i = 2; i < nBoundaryLength; i++)
		{
			m_boundary[i] = (BYTE) boundary[i -2];
		}
	}

	inline int GetVolumeId() const { return m_nVolumeId; }

	inline bool HasError() const
	{
		return !m_errorMsg.empty();
	}

private:
	size_t GetSafeContentLength(BYTE* pBody, size_t len);
	void ProcessHeader(char* pszLine);
	void PrepareContentBegin();
	pplx::task<bool> AppendPartContent(std::shared_ptr<ReadBuff>, BYTE* ptr, size_t len);
	void ClosePartContent();


	int m_nVolumeId;
	int m_state;
	size_t m_iBuffPtr;
	//size_t m_nBoundaryLength;
	size_t m_cbContentWritten;
	std::vector<BYTE> m_boundary;
	utility::string_t m_errorMsg;
	basic_ostream<uint8_t> m_outFileStrm;
	BYTE m_buffer[READBUFF_SIZE * 2];

	struct
	{
		std::string ctype;
		std::string disposition;
		std::string name;
		std::string filename;
	} m_partInfo;		
};

size_t ArcUploadHandler::GetSafeContentLength(BYTE* pBody, size_t len)
{
	size_t offset = 0;
	while (offset < len)
	{
		if (pBody[offset++] != '\r')
		{
			continue;
		}

		if (pBody[offset] == '\n')
		{
			size_t remain = len - (offset + 1);
			size_t chklen = (remain < m_boundary.size()) ? remain : m_boundary.size();

			if (!memcmp(&pBody[offset + 1], &m_boundary[0], chklen))
			{
				// it's boundary
				return (offset - 1);
			}
		}
	}

	ASSERT(offset <= len);
	return offset;
}

char* StripQuote(char* psz)
{
	if (psz[0] == '\"')
	{
		char *p = psz;
		while (*p != 0) p++;

		--p;
		if (*p == '\"')
		{
			*p = 0;
			return &psz[1];
		}
	}

	return psz;
}

char *TrimString(char *psz);

void ArcUploadHandler::ProcessHeader(char* pszLine)
{
	char* pColon = strchr(pszLine, ':');

	if (pColon == NULL)
	{
		LogConsoleA("Invalid header line: %s\n", pszLine);
		return;
	}

	*pColon++ = 0;

	if (!strcmp(pszLine, "Content-Disposition"))
	{
		char* pSC1 = strchr(pColon, ';');
		if (pSC1 == NULL)
		{
			m_partInfo.disposition = TrimString(pColon);
		}
		else
		{
			*pSC1++ = 0;
			m_partInfo.disposition = TrimString(pColon);

			for (;;)
			{
				char* pSC2 = strchr(pSC1, ';');
				if (pSC2 != NULL)
				{
					*pSC2 = 0;
				}

				char* pKV = TrimString(pSC1);
				char* pEQ = strchr(pKV, '=');
				if (pEQ != NULL)
				{
					*pEQ = 0;
					char* key = TrimString(pKV);
					char* val = TrimString(&pEQ[1]);

					if (!strcmp(key, "name"))
					{
						m_partInfo.name = StripQuote(val);
					}
					else if (!strcmp(key, "filename"))
					{
						m_partInfo.filename = StripQuote(val);
					}
					else
					{
						LogConsoleA("Unknown content disposition key: %s\n", key);
					}
				}

				if (pSC2 == NULL)
					break;

				pSC1 = pSC2 + 1;
			}
		}
	}
	else if (!strcmp(pszLine, "Content-Type"))
	{
		m_partInfo.ctype = TrimString(pColon);
	}
}

void ArcUploadHandler::PrepareContentBegin()
{
	LogConsoleA("Header ended (%s,name=%s,fn=%s,ct=%s), open content file...\n", m_partInfo.disposition.c_str(),
		m_partInfo.name.c_str(), m_partInfo.filename.c_str(), m_partInfo.ctype.c_str());

	m_outFileStrm = file_stream<uint8_t>::open_ostream(U("R:\\uptest.txt")).get();
	m_cbContentWritten = 0;
}

pplx::task<bool> ArcUploadHandler::AppendPartContent(std::shared_ptr<ReadBuff> rbuf, BYTE* ptr, size_t len)
{
	ASSERT(m_outFileStrm.is_valid());
	//printf("outStream post write %p, %d\n", rbuf->buff, len);
	rawptr_buffer<BYTE> rawbuf(ptr, len, std::ios::in);
	m_cbContentWritten += len;
	return m_outFileStrm.write(rawbuf, len).then([rbuf, len](size_t written)->pplx::task<bool> {
		//printf("outStream %p, %d written (req=%d)\n", rbuf->buff, written, len);
		return pplx::task_from_result(len == written);
	});
}

void ArcUploadHandler::ClosePartContent()
{
	LogConsoleA("File written=%d bytes\n", m_cbContentWritten);
	m_outFileStrm.close().get();	
	//m_cbContentWritten = 0;
}

pplx::task<bool> ArcUploadHandler::FeedBodyData(std::shared_ptr<ReadBuff> rbuf, size_t len)
{
	BYTE* pBody = rbuf->buff;

	if (m_state == STATE_CONTENT)
	{
		if (m_iBuffPtr > 0)
		{
			ASSERT(m_iBuffPtr < MAX_BOUNDARY_SIZE);
			memmove(&rbuf->buff[m_iBuffPtr], rbuf->buff, m_iBuffPtr);
			memcpy(rbuf->buff, m_buffer, m_iBuffPtr);
			m_iBuffPtr = 0;
		}

		size_t used = GetSafeContentLength(pBody, len);
		if (used == len)
		{
			// use all
			return AppendPartContent(rbuf, pBody, len);
		}

		if (!AppendPartContent(rbuf, pBody, used).get())
		{
			printf("file written fail?\n");
			return pplx::task_from_result(false);
		}

		pBody += used;
		len -= used;

		if (len >= (m_boundary.size() + 2))
		{
			ClosePartContent();

			m_state = STATE_CHECK_BOUNDARY;
			ASSERT(pBody[0] == '\r' && pBody[1] == '\n');
			pBody += 2;
			len -= 2;
		}
		else
		{
			return pplx::task_from_result(true);
		}
	}

	if (m_iBuffPtr + len > sizeof(m_buffer))
	{
		// too long part header? => invalid content
		m_errorMsg = U("Too long part header");
		return pplx::task_from_result(false);
	}

	memcpy(&m_buffer[m_iBuffPtr], pBody, len);
	m_iBuffPtr += len;

	if (m_state == STATE_CHECK_BOUNDARY)
	{
		if (m_iBuffPtr < (m_boundary.size() + 2))
		{
			// more data needed
			LogConsoleA("more data needed (current=%d)\n", m_iBuffPtr);
			return pplx::task_from_result(true);
		}
	
		if (0 != memcmp(&m_buffer[0], &m_boundary[0], m_boundary.size()))
		{
			m_errorMsg = U("Invalid boundary");
			ASSERT(0);
			return pplx::task_from_result(false);
		}

		pBody = &m_buffer[m_boundary.size()];

		if (pBody[0] == '-' && pBody[1] == '-')
		{
			// all done
			m_state = STATE_FINISHED;
			return pplx::task_from_result(false);
		}

		ASSERT(pBody[0] == '\r' && pBody[1] == '\n');
		pBody += 2;
		len = m_iBuffPtr - (m_boundary.size() + 2);		
		m_state = STATE_READ_HEADER;
		m_partInfo.ctype.clear();
		m_partInfo.disposition.clear();
		m_partInfo.name.clear();
		m_partInfo.filename.clear();
	}
	else
	{
		pBody = &m_buffer[0];
		len = m_iBuffPtr;
	}

	ASSERT(m_state == STATE_READ_HEADER);
	char* pszLine = (char*)pBody;
	BYTE* pLimit = &pBody[len];
	while (pBody < pLimit)
	{
		if (*pBody == '\r')
		{
			ASSERT(pBody[1] == '\n');
			*pBody = 0;
			pBody = &pBody[2];

			if (pszLine[0] == 0)
			{
				// end of headers
				m_state = STATE_CONTENT;
				PrepareContentBegin();
				break;
			}

			ProcessHeader(pszLine);
			pszLine = (char*)pBody;
		}
		else
		{
			++pBody;
		}
	}

	len = pLimit - pBody;
	if (m_state == STATE_CONTENT)
	{
		m_iBuffPtr = 0;

		if (len > 0)
		{
			memcpy(rbuf->buff, pBody, len);
			return FeedBodyData(rbuf, len);
		}
	}
	else
	{
		memmove(m_buffer, pBody, len);
		m_iBuffPtr = len;
	}

	return pplx::task_from_result(true);
}

//////////////////////////////////////////////////////////////////////////

ArcUploadHandler* VolumeManager::RegisterArcUploader(int nVolumeId)
{
	std::lock_guard<std::mutex> lock(m_mutexUploader);

	auto itr = m_uploaders.find(nVolumeId);
	if (itr != m_uploaders.end())
	{
		LogE("Uploader(vol=%d) already registered.\n", nVolumeId);
		return NULL;
	}

	ArcUploadHandler* pUploader = new ArcUploadHandler(nVolumeId);
	m_uploaders[nVolumeId] = pUploader;

	return pUploader;
}

void VolumeManager::UnregisterArcUploader(ArcUploadHandler* pHandler)
{
	std::lock_guard<std::mutex> lock(m_mutexUploader);

	int nVolumeId = pHandler->GetVolumeId();
	auto itr = m_uploaders.find(nVolumeId);
	if (itr == m_uploaders.end())
	{
		LogE("Uploader(vol=%d) was not registered.\n", nVolumeId);
		return;;
	}

	delete pHandler;
	m_uploaders.erase(itr);
}

void HandleVolumeUpload(http_request& request, int nVolumeId)
{
	//auto resp_task = request.reply(status_codes::OK, );
	//request.set_progress_handler([&](message_direction::direction direction, utility::size64_t so_far)
	//{
	//	printf("progress_handler %d, %ull\n", direction, so_far);
	//});

	auto& headers = request.headers();

	// TODO:
	//headers.find(U("Origin:http://urizip.ogp.kr"));
	//headers.find(U("Cache-Control:no-cache"));
	//headers.find(U("Pragma:no-cache"));

	auto contentType = headers.content_type();
	if (contentType.find(U("multipart/form-data")) != 0)
	{
		// invalid content type?
		request.reply(status_codes::BadRequest, U("Invalid Content-type."));
		return;
	}

	size_t iBoundary = contentType.find(U("boundary="));
	if (iBoundary == contentType.npos)
	{
		// boundary not found
		request.reply(status_codes::BadRequest, U("Invalid boundary."));
		return;
	}

	ArcUploadHandler* pHandler = g_volMan.RegisterArcUploader(nVolumeId);
	if (pHandler == NULL)
	{
		request.reply(status_codes::InternalError, U("Upload conflict."));
		return;
	}

	pHandler->SetBoundary(contentType.substr(iBoundary + 9));

	auto processRequestBody = [request, pHandler]()
	{
		std::shared_ptr<ReadBuff> rbuf = std::make_shared<ReadBuff>();

		return request.body().streambuf().getn(rbuf->buff, READBUFF_SIZE - MAX_BOUNDARY_SIZE).then([rbuf, pHandler](size_t len)->pplx::task<bool> {

			if (len == 0)
			{
				printf("read len=0\n");
				return pplx::task_from_result(false);
			}

			ASSERT(len <= (READBUFF_SIZE - MAX_BOUNDARY_SIZE));
			return pHandler->FeedBodyData(rbuf, len);
		});
	};

	auto recvloop = pplx::details::do_while(processRequestBody);
	recvloop.then([request, pHandler](bool r2)
	{
		printf("r2=%d\n", r2);
		http_response response(status_codes::OK);
		auto& headers = response.headers();
		headers.add(U("Access-Control-Allow-Origin"), U("*"));
		//response.set_body(json_content);
		response.set_body(U("Hello?"));
		request.reply(response);

		g_volMan.UnregisterArcUploader(pHandler);
	});
}
