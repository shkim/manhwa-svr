#pragma once

#ifdef _WIN32
#include "targetver.h"
#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#else
#include "unix_compat.h"
#endif

#include <stdio.h>

#include <map>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <utility>

#include <tbb/concurrent_hash_map.h>
#include <tbb/concurrent_vector.h>
#include <tbb/concurrent_queue.h>

//#include <cpprest/basic_types.h>
#include <cpprest/http_client.h>
#include <cpprest/http_listener.h>
#include <cpprest/filestream.h>
#include <cpprest/rawptrstream.h>
#include <cpprest/asyncrt_utils.h>
#include <cpprest/json.h>
#include <cpprest/uri.h>

// TODO: reference additional headers your program requires here


#ifdef _DEBUG
	#include <assert.h>

	#define ASSERT(f)			assert(f)
	#define VERIFY(f)			ASSERT(f)
	#define TRACE				_TRACE

	void _TRACE(LPCWSTR szFormat, ...);
	void _TRACE(LPCSTR szFormat, ...);

#else	// _DEBUG
	#define ASSERT(f)			((void)0)
	#define VERIFY(f)			((void)(f))
	#define TRACE				1 ? (void)0 : _TRACE

	static inline void _TRACE(LPCWSTR, ...) { }
	static inline void _TRACE(LPCSTR, ...) { }
#endif
