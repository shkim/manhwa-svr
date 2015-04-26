#if !defined(_WIN32) && !defined(__UNIX_COMPATIBILITY_LAYER_H__)
#define __UNIX_COMPATIBILITY_LAYER_H__

#define _T(x)		x
#define _ttoi		atoi

typedef const wchar_t* LPCWSTR;
typedef const char* LPCSTR;

typedef char TCHAR;
typedef LPCSTR LPCTSTR;

typedef unsigned char BYTE;
typedef unsigned int UINT;
typedef unsigned int DWORD;

#define _vsnprintf vsnprintf
#define _snprintf snprintf
#define _sntprintf snprintf
#define _stricmp strcasecmp
#define _tprintf printf

#endif
