#include "stdafx.h"
#include "readconf.h"
#include "dbglog.h"


#ifdef _WIN32

static wchar_t a2w_buff[MAX_PATH];

const wchar_t* PathFilenamer::A2W(const char* pszUTF8)
{
	MultiByteToWideChar(CP_UTF8, 0, pszUTF8, -1, a2w_buff, MAX_PATH);
	return a2w_buff;
}

const char* PathFilenamer::W2A(const wchar_t* pszWide)
{
	char* buff = (char*)a2w_buff;
	WideCharToMultiByte(CP_UTF8, 0, pszWide, -1, buff, MAX_PATH, NULL, NULL);
	return buff;
}

#else

static int _UnicodeToUTF8(int c, BYTE* p)
{
	if(c < 0x80)
	{
		*p = c;
		return 1;
	}
	else if(c < 0x800)
	{
		p[0] = 0xC0 | (c>>6);
		p[1] = 0x80 | (c & 0x3F);
		return 2;
	}
	else if(c < 0x10000)
	{
		p[0] = 0xE0 | (c>>12);
		p[1] = 0x80 | ((c>>6) & 0x3F);
		p[2] = 0x80 | (c & 0x3F);
		return 3;
	}
	//else if(c < 0x200000)
	else if(c <= 0x10FFFF)
	{
		p[0] = 0xF0 | (c>>18);
		p[1] = 0x80 | ((c>>12) & 0x3F);
		p[2] = 0x80 | ((c>>6) & 0x3F);
		p[3] = 0x80 | (c & 0x3F);
		return 4;
	}

	ASSERT(!"invalid unicode");
	return 0;
}

int _WideToUtf8(char* pDstUTF8, int cchMaxDst, const wchar_t* pszSrcUnicode)
{
	wchar_t ch;
	BYTE* pOut = (BYTE*) pDstUTF8;

	int lenStr = 0;

	while((ch = *pszSrcUnicode++) != 0)
	{
		int len = _UnicodeToUTF8(ch, &pOut[lenStr]);
		ASSERT(len > 0);
		lenStr += len;
	}

	pOut[lenStr] = 0;
	return lenStr;
}
	
int _Utf8ToWide(wchar_t* pDstUnicode, int cchMaxDst, const char* pszSrcUTF8)
{
	unsigned int ch0, ch1, ch2, ch3;
	wchar_t* uni0 = pDstUnicode;

	while(ch0 = *pszSrcUTF8++)
	{
		if(ch0 < 0x80)
		{
			*pDstUnicode++ = ch0;
		}
		else if((ch0 & 0xE0) == 0xC0)
		{
			ch1 = *pszSrcUTF8++;
			*pDstUnicode++ = ((ch0 & 0x1F) << 6) | (ch1 & 0x3F);
		}
		else if((ch0 & 0xF0) == 0xE0)
		{
			ch1 = *pszSrcUTF8++;
			ch2 = *pszSrcUTF8++;
			*pDstUnicode++ = ((ch0 & 0x0F) << 12) | ((ch1 & 0x3F) << 6) | (ch2 & 0x3F);
		}
		else if((ch0 & 0xF8) == 0xF0)
		{
			ch1 = *pszSrcUTF8++;
			ch2 = *pszSrcUTF8++;
			ch3 = *pszSrcUTF8++;
			*pDstUnicode++ = ((ch0 & 0x07) << 18) | ((ch1 & 0x3F) << 12)
				| ((ch2 & 0x3F) << 6) | (ch3 & 0x3F);
		}
		else 
		{
			ASSERT(!"invalid utf8");
			return -1;
		}
	}

	*pDstUnicode = 0;
	return (int)(pDstUnicode - uni0);
}

static wchar_t a2w_buff[512];

const wchar_t* PathFilenamer::A2W(const char* pszUTF8)
{
	_Utf8ToWide(a2w_buff, 512, pszUTF8);
	return a2w_buff;
}

const char* PathFilenamer::W2A(const wchar_t* pszWide)
{
	_WideToUtf8((char*)a2w_buff, 512, pszWide);
	return (char*)a2w_buff;
}

#endif

char *TrimString(char *psz)
{
	char *s, *e;

	if (psz == NULL) return NULL;

	s = psz;
	while (*s == ' ' || *s == '\t') s++;

	for (e = s; *e != '\0'; e++);

	do *e-- = '\0'; while (*e == ' ' || *e == '\t' || *e == '\r' || *e == '\n');

	return (s > e) ? NULL : s;
}

StringSplitter::StringSplitter(const char* pszText, const char* pszDelimiters)
{
	Reset(pszText, pszDelimiters);
}

void StringSplitter::Reset(const char* pszText, const char* pszDelimiters)
{
	m_pCurrent = pszText;
	m_pEnd = (pszText == NULL) ? NULL : (pszText + strlen(pszText));
	m_pszDelimiters = pszDelimiters == NULL ? " \t,;" : pszDelimiters;
}

bool StringSplitter::IsDelimiterChar(const char ch)
{
	const char* d = m_pszDelimiters;

	do
	{
		if (ch == *d)
			return true;
	} while (*d++ != 0);

	return false;
}

bool StringSplitter::GetNext(const char** ppszSplitted, int* pLength)
{
	if (m_pCurrent < m_pEnd)
	{
		const char* p = m_pCurrent;

		while ((p < m_pEnd) && IsDelimiterChar(*p))
			p++;

		if (p >= m_pEnd)
			goto _overEnd;

		*ppszSplitted = p;

		while (!IsDelimiterChar(*p))
			p++;

		m_pCurrent = p;
		*pLength = (int)(p - (*ppszSplitted));

		return true;
	}
	else
	{
	_overEnd:
		*ppszSplitted = "";
		*pLength = 0;
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////////

LineDispenser::LineDispenser(const char* pszInput)
{
	m_pCurrent = pszInput;
	m_pEnd = m_pCurrent + strlen(pszInput);
}

bool LineDispenser::GetNext(const char** ppszLine, int* pLength)
{
	if (m_pCurrent < m_pEnd)
	{
		const char* p = m_pCurrent;
		*ppszLine = p;

		int nCRLF = 0;
		while (p < m_pEnd)
		{
			if (*p == '\r')
			{
				if (nCRLF & 1)
					break;

				nCRLF |= 1;
			}
			else if (*p == '\n')
			{
				if (nCRLF & 2)
					break;

				nCRLF |= 2;
			}
			else if (nCRLF != 0)
				break;

			p++;

			if (nCRLF == 3)
				break;
		}

		m_pCurrent = p;

		if (nCRLF == 3)
			p -= 2;
		else if (nCRLF != 0)
			p -= 1;

		*pLength = (int)(p - (*ppszLine));
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////

void ConfigSection::Add(const char* pszName, const char* pszValue)
{
	TPair pair;

	pair.pszName = pszName;
	pair.pszValue = pszValue;

	m_pairs.push_back(pair);
}

int ConfigSection::GetKeyCount()
{
	return (int)m_pairs.size();
}

bool ConfigSection::GetPair(int iKey, const char** ppKeyOut, const char** ppValueOut)
{
	if (iKey >= 0 && (unsigned)iKey < m_pairs.size())
	{
		TPair& pair = m_pairs.at(iKey);

		if (ppKeyOut != NULL)
			*ppKeyOut = pair.pszName;
		if (ppValueOut != NULL)
			*ppValueOut = pair.pszValue;

		return true;
	}

	return false;

}

const char* ConfigSection::Find(const char* name)
{
	for (size_t i = 0; i < m_pairs.size(); i++)
	{
		TPair& pair = m_pairs.at(i);

		if (_stricmp(pair.pszName, name) == 0)
		{
			return pair.pszValue;
		}
	}

	return NULL;
}

const char* ConfigSection::GetString(const char* name)
{
	return Find(name);
}

int ConfigSection::GetInteger(const char* name, int nDefault)
{
	const char* value = Find(name);
	if (value == NULL)
		return nDefault;

	return atoi(value);
}

float ConfigSection::GetFloat(const char* name, float fDefault)
{
	const char* value = Find(name);
	if (value == NULL)
		return fDefault;

	return (float)atof(value);
}

bool ConfigSection::GetBoolean(const char* name, bool bDefault)
{
	const char* value = Find(name);
	if (value == NULL)
		return bDefault;

	if (_stricmp(value, "true") == 0 || _stricmp(value, "yes") == 0 || _stricmp(value, "1") == 0)
		return true;

	return false;
}

const PathFilenamer::PathChar* ConfigSection::GetFilename(const char* name)
{
	const char* file = Find(name);

	if (file != NULL)
	{
		const PathFilenamer::PathChar* tfile = PathFilenamer::A2T(file);

		if (PathFilenamer::IsAbsPath(tfile))
		{
			// absolute path
			return tfile;
		}

		return m_pParent->m_path.GetPathname(tfile);
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////////

ConfigFile::ConfigFile()
{
	m_pRawFile = NULL;
}

ConfigFile::~ConfigFile()
{
	for (size_t i = 0; i < m_sections.size(); i++)
	{
		TSection& s = m_sections.at(i);
		delete s.pSection;
	}

	if (m_pRawFile != NULL)
	{
		free(m_pRawFile);
	}
}

bool ConfigFile::Load(LPCTSTR pszFilename)
{
	bool bSuccess = false;

#ifdef _WIN32
	DWORD dwRead;
	HANDLE hFile;

	hFile = CreateFile(pszFilename, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		LogConsoleT(_T("file open failed: %s\n"), pszFilename);
		return false;
	}

	DWORD dwFileSize = GetFileSize(hFile, NULL);
#else
	FILE* fp = fopen(pszFilename, "rt");

	if (fp == NULL)
	{
		LogConsole("file open failed: %s\n", pszFilename);
		return false;
	}

	fseek(fp, 0, SEEK_END);
	UINT dwFileSize = ftell(fp);
#endif

	if (dwFileSize > 0)
	{
		m_pRawFile = (char*)malloc(dwFileSize + 1);
		if (m_pRawFile != NULL)
		{
#ifdef _WIN32
			ReadFile(hFile, m_pRawFile, dwFileSize, &dwRead, NULL);
#else
			rewind(fp);
			fread(m_pRawFile, dwFileSize, 1, fp);
#endif
			m_pRawFile[dwFileSize] = 0;
			bSuccess = ParseConfFile();

			m_path.FromCurrentDirectory();
			m_path.ChangeDirectory(pszFilename, true);
		}
		else
		{
			LogConsoleA("out of memory\n");
		}
	}
	else
	{
		LogConsoleA("file size is 0.\n");
	}

#ifdef _WIN32
	CloseHandle(hFile);
#else
	fclose(fp);
#endif

	return bSuccess;
}

ConfigSection* ConfigFile::OpenNewSection(const char* pszName)
{
	if (pszName == NULL)
	{
		LogConsoleA("No section name specified.\n");
		return NULL;
	}

	if (GetSection(pszName) != NULL)
	{
		LogConsoleA("Section '%s' is duplicated.\n", pszName);
		return NULL;
	}

	TSection sect;

	sect.pszName = pszName;
	sect.pSection = new ConfigSection(this);
	m_sections.push_back(sect);

	return sect.pSection;
}

class MultilineValueConcat
{
	ConfigSection* m_pSect;
	const char* m_pszName;
	char* m_pszValue;
	char* m_pLastPos;

public:
	MultilineValueConcat()
	{
		m_pSect = NULL;
	}

	inline bool IsOnGoing() const { return (m_pSect != NULL); }

	void Start(ConfigSection* pSect, const char* pszName, char* pValuePos)
	{
		m_pSect = pSect;
		m_pszName = pszName;
		m_pszValue = pValuePos;
		m_pLastPos = pValuePos;
	}

	void Append(const char* pszLine)
	{
		size_t len = strlen(pszLine);
		ASSERT(len > 0);
		memmove(m_pLastPos, pszLine, len);
		m_pLastPos[len] = '\n';
		m_pLastPos += len + 1;
	}

	void Finish()
	{
		if (m_pszValue < m_pLastPos)
		{
			m_pLastPos[-1] = 0;
			m_pSect->Add(m_pszName, m_pszValue);
		}

		m_pSect = NULL;
	}
};

bool ConfigFile::ParseConfFile()
{
	char szOrigLineBuff[256];
	const char* _pszLine;
	char *pszLine, *pEqual, *pColon, *pName, *pValue;
	int nLength;
	MultilineValueConcat mvc;

	LineDispenser ld(m_pRawFile);
	ConfigSection* pSect = NULL;
	szOrigLineBuff[255] = 0;

	while (ld.GetNext(&_pszLine, &nLength))
	{
		pszLine = const_cast<char*>(_pszLine);
		pszLine[nLength] = 0;
		pszLine = TrimString(pszLine);

		if (mvc.IsOnGoing())
		{
			if (pszLine == NULL)
			{
				// multi-line value ended
				mvc.Finish();
				continue;
			}

			if (_pszLine[0] == ' ' || _pszLine[0] == '\t')
			{
				// multi-line value continues
				mvc.Append(pszLine);
				continue;
			}

			mvc.Finish();
		}

		if (pszLine == NULL || pszLine[0] == ';' || pszLine[0] == '#')
			continue;

		if (pszLine[0] == '[')
		{
			// open new section
			char* pCloser = strchr(pszLine, ']');
			if (pCloser == NULL)
			{
				LogConsoleA("Section name closing brace (]) not found: %s\n", pszLine);
				return false;
			}

			*pCloser = '\0';
			pSect = OpenNewSection(TrimString(&pszLine[1]));
			continue;
		}

		//StringCchCopyA(szOrigLineBuff, 256, pszLine);
		strncpy(szOrigLineBuff, pszLine, 255);

		pEqual = strchr(pszLine, '=');
		if (pEqual == NULL)
		{
			pColon = strchr(pszLine, ':');
			if (pColon != NULL)
			{
				*pColon = 0;
				pValue = TrimString(&pColon[1]);
				if (pValue == NULL)
					goto _validSyntax;
			}

		_invSyntax:
			LogConsoleA("Invalid syntax: %s\n", szOrigLineBuff);
			return false;
		}
		else
		{
			pColon = NULL;
			*pEqual = 0;
			pValue = TrimString(&pEqual[1]);
		}

	_validSyntax:
		pName = TrimString(pszLine);
		if (pName == NULL)
			goto _invSyntax;

		if (pSect == NULL)
		{
			LogConsoleA("Section must be defined before any variable statement.\n");
			return false;
		}

		if (pColon != NULL)
		{
			mvc.Start(pSect, pName, &pColon[1]);
		}
		else if (pValue != NULL)
		{
			pSect->Add(pName, pValue);
		}
	}

	if (mvc.IsOnGoing())
		mvc.Finish();

	return true;
}

ConfigSection* ConfigFile::GetSection(const char* name)
{
	if (name != NULL)
	{
		for (size_t i = 0; i < m_sections.size(); i++)
		{
			TSection& sect = m_sections.at(i);
			if (_stricmp(sect.pszName, name) == 0)
				return sect.pSection;
		}
	}

	if (!m_sections.empty())
	{
		TSection& sect = m_sections.at(0);
		return sect.pSection;
	}

	return NULL;
}
