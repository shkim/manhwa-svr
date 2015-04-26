#pragma once

class PathFilenamer
{
public:
#ifdef _UNICODE
#define StrLastChar wcsrchr
	typedef std::wstring PathString;
#else
#define StrLastChar strrchr
	typedef std::string PathString;
#endif

	typedef PathString::value_type PathChar;

	inline PathFilenamer() {}

	inline PathFilenamer(const PathChar* src)
	{
		m_curDir = src;
	}

	inline void operator=(const PathChar* src)
	{
		m_curDir = src;
	}

	inline void operator=(const PathFilenamer& src)
	{
		m_curDir = src.m_curDir;
	}

	// returns the basename of the input filename
	//	ex) /abc/def/xyz.ext ==> 'xyz.ext'
	static const PathChar* GetBaseFilename(const PathChar* pszPathname)
	{
		const TCHAR* pSlash = StrLastChar(pszPathname, '/');

		if (pSlash == NULL)
			pSlash = StrLastChar(pszPathname, '\\');

		if (pSlash == NULL)
			return pszPathname;
		else
			return &pSlash[1];
	}

	static bool IsAbsPath(const PathChar* pszDir)
	{
#ifdef _WIN32
		return (pszDir[1] == ':' || (pszDir[0] == '\\' && pszDir[1] == '\\'));
#else
		return (pszDir[0] == '/');
#endif
	}

	static const wchar_t* A2W(const char* pszUTF8);
	static const char* W2A(const wchar_t* pszWide);
#ifdef _UNICODE
	static inline const wchar_t* A2T(const char* pszUTF8) { return A2W(pszUTF8); }
	static inline const char* T2A(const wchar_t* pszWide) { return W2A(pszWide); }
#else
	static inline const char* A2T(const char* pszUTF8) { return pszUTF8; }
	static inline const char* T2A(const char* pszWide) { return pszWide; }
#endif

	void ChangeDirectory(const PathChar* pszDir, bool bIsFilename = false)
	{
		if (IsAbsPath(pszDir))
		{
			// Absolute path: reset current
			m_curDir = pszDir;
		}
		else
		{
			// Relative path: change direcoty
			Append(m_curDir, pszDir);
		}

		// if pszDir contains the filename component (at tail), chop it!
		if (bIsFilename)
		{
			size_t slash = m_curDir.rfind('/');
			if (slash != PathString::npos)
			{
				m_curDir.resize(slash);
			}
			else
			{
				slash = m_curDir.rfind('\\');
				if (slash != PathString::npos)
				{
					m_curDir.resize(slash);
				}
			}
		}
	}

	const PathChar* GetPathname(const PathChar* pszFilename)
	{
		m_retBuff = m_curDir;
		Append(m_retBuff, pszFilename);

		return m_retBuff.c_str();
	}

	inline const PathChar* GetCurrentDirectory()
	{
		return m_curDir.c_str();
	}

	void FromCurrentDirectory()
	{
		PathChar buff[260];

#ifdef _WIN32
		::GetCurrentDirectory(260, buff);
#else
		getcwd(buff, 260);
#endif
		m_curDir = buff;
	}

#ifdef _WIN32
	void FromExeDirectory(HMODULE hModule)
	{
		PathChar buff[260];
		GetModuleFileName(hModule, buff, 260);
		m_curDir = buff;
	}
#endif

private:

#ifdef _WIN32
	static const TCHAR DIR_SEPARATOR = '\\';
#else
	static const TCHAR DIR_SEPARATOR = '/';
#endif

	static void Append(PathString& baseStr, const PathChar* pszDir)
	{
		size_t len = baseStr.length();
		if (len == 0)
		{
			ASSERT(!"Append(pszBase == '')");
			baseStr = pszDir;
			return;
		}
		else
		{
			// if pszBase ends with / or \, erase it.
			for (;;)
			{
				PathChar ch = baseStr.back();
				if (ch != '/' && ch != '\\')
					break;

				baseStr.pop_back();
			}
		}

		PathChar* aNameStack[32];
		PathChar joinBuff[260];
		memcpy(joinBuff, baseStr.c_str(), (baseStr.length() + 1) * sizeof(PathChar));

		int nBaseDepth = 0;
		PathChar* pDst = joinBuff;
		while (*pDst != 0)
		{
			if (*pDst == '/' || *pDst == '\\')
			{
				aNameStack[nBaseDepth] = pDst;
				nBaseDepth++;
			}

			pDst++;
		}

		const PathChar* pSrc = pszDir;
		for (;;)
		{
			if (*pSrc == '/' || *pSrc == '\\' || *pSrc == 0)
			{
				size_t len = pSrc - pszDir;

				if (len == 0)
				{
					// maybe "//" or "\\\\" ==> skip
				}
				else if (len == 1 && *pszDir == '.')
				{
					// skip "."
				}
				else if (len == 2 && (pszDir[0] == '.' && pszDir[1] == '.'))
				{
					// chdir ..
					nBaseDepth--;
					pDst = aNameStack[nBaseDepth];
					*pDst = 0;
				}
				else
				{
					// append
					*pDst = DIR_SEPARATOR;
					aNameStack[nBaseDepth] = pDst;
					nBaseDepth++;

					memcpy(&pDst[1], pszDir, len * sizeof(PathChar));
					pDst += 1 + len;
					*pDst = 0;
				}

				pszDir = &pSrc[1];
			}

			if (*pSrc == 0)
				break;

			pSrc++;
		}

		baseStr = joinBuff;
	}

private:
	PathString m_curDir;
	PathString m_retBuff;
};
