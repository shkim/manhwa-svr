#ifndef __DEBUG_LOG_H__
#define __DEBUG_LOG_H__

///////////////////////////////////////////////////////////////////////////////
// Debug Information Trace Helper

#ifdef _WIN32

#ifdef _DEBUG
	#include <crtdbg.h>

	void INIT_CRT_DEBUG_REPORT();
	TCHAR* GetLastErrorMsg();
	
#else	// _DEBUG
	#define INIT_CRT_DEBUG_REPORT()
	#define GetLastErrorMsg()	""
#endif

void OutputDebugPrint(LPCWSTR szFormat, ...);

#else

#define INIT_CRT_DEBUG_REPORT()

#endif

//////////////////////////////////////////////////////////////////////////

#include "PathFilenamer.h"

class LogManager;
class ConfigSection;

class LogFile
{
public:
	LogFile(LogManager* pParent);
	~LogFile();

	bool Open(const PathFilenamer::PathChar* pszPathname);
	void Close();
	void Log(const char* pszLine);

private:
	LogManager* m_pParent;
	PathFilenamer::PathString m_filename;
	concurrency::streams::basic_ostream<char> m_ostrm;
	//concurrency::task<concurrency::streams::basic_ostream<char>> m_task;
};

class LogManager
{
public:
	enum LogLevels
	{
		Verbose =0,
		Debug2,
		Info,
		Warning,
		Error,

		MAX_LEVEL_COUNT
	};

	bool Open(ConfigSection* pLogSect);
	void Close();

	void Log(int level, const char* pszFormat, ...);

private:
	bool m_bLogConsole;
	LogFile* m_aLogLevels[MAX_LEVEL_COUNT];
	std::map<std::string, LogFile*> m_files;
};

extern LogManager g_logman;

#define LogConsoleA	printf
#define LogConsoleT	_tprintf
#define LogV(...)	g_logman.Log(LogManager::Verbose, __VA_ARGS__)
#define LogD(...)	g_logman.Log(LogManager::Debug2, __VA_ARGS__)
#define LogI(...)	g_logman.Log(LogManager::Info, __VA_ARGS__)
#define LogW(...)	g_logman.Log(LogManager::Warning, __VA_ARGS__)
#define LogE(...)	g_logman.Log(LogManager::Error, __VA_ARGS__)

#endif //__DEBUG_LOG_H__

