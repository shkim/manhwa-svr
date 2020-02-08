#include "stdafx.h"
#include "dbglog.h"
#include "readconf.h"
#include <stdarg.h>

#ifdef _DEBUG

#ifdef _WIN32

void INIT_CRT_DEBUG_REPORT()
{
/*
	int	DbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
//	DbgFlag |= _CRTDBG_LEAK_CHECK_DF;
//		DbgFlag |= _CRTDBG_ALLOC_MEM_DF | _CRTDBG_DELAY_FREE_MEM_DF;
		DbgFlag |= _CRTDBG_DELAY_FREE_MEM_DF;
//		DbgFlag &= ~_CRTDBG_CHECK_ALWAYS_DF;
	_CrtSetDbgFlag(DbgFlag);
*/
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
}

bool IsValidAddress(const void *p, UINT nBytes, BOOL bReadWrite)
{
	return _CrtIsValidPointer(p, nBytes, bReadWrite) == TRUE;
}

bool IsValidString(LPCTSTR psz, int nLength)
{
	if(psz == NULL)	return false;
	return (::IsBadStringPtr(psz, nLength) == 0);
}

#ifdef _UNICODE

/*static void _OutputCounter()
{
	static int s_counter = 0;
	char buff[16];
	StringCchPrintfA(buff, 16, "%d:", ++s_counter);
	OutputDebugStringA(buff);
}*/

void _TRACE(LPCWSTR szFormat, ...)
{
	WCHAR szBuffer[1024];

	va_list ap;
	va_start(ap, szFormat);
	
	StringCbVPrintfW(szBuffer, sizeof(szBuffer), szFormat, ap);

	va_end(ap);

//	_OutputCounter();
	OutputDebugStringW(szBuffer);
}

void _TRACE(LPCSTR pszFormat, ...)
{
	CHAR szBuffer[1024];
	
	va_list ap;
	va_start(ap, pszFormat);
	
	StringCbVPrintfA(szBuffer, sizeof(szBuffer), pszFormat, ap);
	
	va_end(ap);
	
//	_OutputCounter();
	OutputDebugStringA(szBuffer);
}

#else

void _TRACE(LPCTSTR szFormat, ...)
{
	TCHAR szBuffer[1024];
	
	va_list ap;
	va_start(ap, szFormat);
	
	_vsntprintf(szBuffer, sizeof(szBuffer), szFormat, ap);
	
	va_end(ap);
	
	OutputDebugString(szBuffer);
}

#endif	// _UNICODE

TCHAR* GetLastErrorMsg()
{
	static TCHAR buff[256];

	FormatMessage( 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		buff, sizeof(buff)/sizeof(buff[0]), NULL);

	return buff;
}

void OutputDebugPrint(LPCWSTR szFormat, ...)
{
	WCHAR szBuffer[1024];

	va_list ap;
	va_start(ap, szFormat);
	
	StringCbVPrintfW(szBuffer, sizeof(szBuffer), szFormat, ap);

	va_end(ap);

	OutputDebugStringW(szBuffer);
}

#endif	// _WIN32

#endif	// _DEBUG

//////////////////////////////////////////////////////////////////////////

LogFile::LogFile(LogManager* pParent)
{
	m_pParent = pParent;
}

LogFile::~LogFile()
{
}

bool LogFile::Open(const PathFilenamer::PathChar* pszPathname)
{
	m_filename = pszPathname; 

	_tprintf(_T("Open Log file: %s\n"), m_filename.c_str());
	m_ostrm = concurrency::streams::file_stream<char>::open_ostream(pszPathname, std::ios_base::app).get();

	return m_ostrm.is_valid();
}

void LogFile::Close()
{
	m_ostrm.close().wait();
	_tprintf(_T("Log file closed: %s\n"), m_filename.c_str());
}

void LogFile::Log(const char* pszLine)
{
	std::string logline(pszLine);
	m_ostrm.print(logline);// .then([logline](size_t written){ printf("%s", logline.c_str()); });
}

//////////////////////////////////////////////////////////////////////////

bool LogManager::Open(ConfigSection* pLogSect)
{
	static const char* LevelLogNames[MAX_LEVEL_COUNT] = { "LogV", "LogD", "LogI", "LogW", "LogE" };

	m_bLogConsole = pLogSect->GetBoolean("LogConsole", false);
	LogI("LogConsole=%d", m_bLogConsole);

	const PathFilenamer::PathChar* pszLogDir = pLogSect->GetFilename("LogDir");
	
	PathFilenamer logDir;
	if (pszLogDir == NULL)
		logDir.FromCurrentDirectory();
	else
		logDir = pszLogDir;

	for (int i = 0; i < MAX_LEVEL_COUNT; i++)
	{
		const char* filename = pLogSect->GetString(LevelLogNames[i]);
		if (filename == NULL)
		{
			m_aLogLevels[i] = NULL;
			continue;
		}

		std::map<std::string, LogFile*>::iterator itr = m_files.find(filename);
		if (itr != m_files.end())
		{
			m_aLogLevels[i] = itr->second;
			continue;
		}		

		LogFile* pFile = new LogFile(this);
		if (!pFile->Open(logDir.GetPathname(PathFilenamer::A2T(filename)) ))
		{
			delete pFile;
			LogConsoleA("Log file open failed: %s\n", filename);
			return false;
		}

		m_files[filename] = pFile;
		m_aLogLevels[i] = pFile;
	}

	return true;
}

void LogManager::Close()
{
	for (std::map<std::string, LogFile*>::iterator itr = m_files.begin(); itr != m_files.end(); ++itr)
	{
		LogFile* pFile = itr->second;
		pFile->Close();
		delete pFile;
	}

	m_files.clear();
}

void LogManager::Log(int level, const char* pszFormat, ...)
{
	static const char LevelInitials[MAX_LEVEL_COUNT] = { 'V', 'D', 'I', 'W', 'E' };

	ASSERT(level >= 0 && level < MAX_LEVEL_COUNT);
	if (m_aLogLevels[level] == NULL)
	{
		if (!m_bLogConsole)
			return;
	}

	char szBuffer[2048];
	size_t len;

#ifdef _WIN32
	SYSTEMTIME tm;
	GetLocalTime(&tm);
	len = wsprintfA(szBuffer, "%c/%02d-%02d %02d:%02d:%02d ",
		LevelInitials[level], tm.wMonth, tm.wDay, tm.wHour, tm.wMinute, tm.wSecond);
#else
	time_t tt = time(NULL);
	struct tm* lt = localtime(&tt);
	sprintf(szBuffer, "%c/%02d-%02d %02d:%02d:%02d ",
		LevelInitials[level], lt->tm_mon + 1, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);
	len = strlen(szBuffer);
#endif

	va_list ap;
	va_start(ap, pszFormat);
	_vsnprintf_s(&szBuffer[len], sizeof(szBuffer) - len, _TRUNCATE, pszFormat, ap);
	va_end(ap);

	if (m_bLogConsole)
	{
		puts(szBuffer);

		if (m_aLogLevels[level] == NULL)
			return;
	}

	len = strlen(szBuffer);
	szBuffer[len] = '\n';
	szBuffer[len + 1] = 0;
	
	m_aLogLevels[level]->Log(szBuffer);
}
