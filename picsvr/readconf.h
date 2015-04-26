#pragma once

#include "PathFilenamer.h"

class ConfigFile;

class ConfigSection
{
public:
	ConfigSection(ConfigFile* pParent) : m_pParent(pParent) {}

	const char* GetString(const char* name);
	int GetInteger(const char* name, int nDefault);
	bool GetBoolean(const char* name, bool bDefault);
	float GetFloat(const char* key, float fDefault);
	const PathFilenamer::PathChar* GetFilename(const char* name);

	int GetKeyCount();
	bool GetPair(int iKey, const char** ppKeyOut, const char** ppValueOut);

	void Add(const char* pszName, const char* pszValue);

private:
	const char* Find(const char* name);

	struct TPair
	{
		const char* pszName;
		const char* pszValue;
	};

	std::vector<TPair> m_pairs;
	ConfigFile* m_pParent;
};

class ConfigFile
{
public:
	ConfigFile();
	~ConfigFile();

	bool Load(LPCTSTR pszFilename);
	ConfigSection* GetSection(const char* name);

	inline const PathFilenamer::PathChar* GetConfigDir() { return m_path.GetCurrentDirectory(); }

private:
	bool ParseConfFile();
	ConfigSection* OpenNewSection(const char* pszName);

	char* m_pRawFile;

	struct TSection
	{
		const char* pszName;
		ConfigSection* pSection;
	};

	PathFilenamer m_path;
	std::vector<TSection> m_sections;

	friend class ConfigSection;
};

//////////////////////////////////////////////////////////////////////////////

class StringSplitter
{
public:
	StringSplitter(const char* pszInput, const char* pszDelimiters = NULL);
	void Reset(const char* pszInput, const char* pszDelimiters = NULL);
	bool GetNext(const char** ppszSplitted, int* pLength);

private:
	bool IsDelimiterChar(const char ch);

	const char* m_pCurrent;
	const char* m_pEnd;
	const char* m_pszDelimiters;
};

class LineDispenser
{
public:
	LineDispenser(const char* pszInput);
	bool GetNext(const char** ppszLine, int* pLength);

private:
	const char* m_pCurrent;
	const char* m_pEnd;
};
