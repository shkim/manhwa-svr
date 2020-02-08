#include "stdafx.h"
#include "dbglog.h"

#include "TitleVolume.h"

#include "extern/unzip/unzip.h"

class ZipFile : public ArcFile
{
public:
	ZipFile();
	virtual ~ZipFile();

	virtual bool Open(const char* pszFilename);
	virtual void Close();
	virtual PicFile* GetFile(int index);
	virtual int GetCount()
	{
		return (int) m_files.size();
	}

	struct FileInfo
	{
		FileInfo(UINT s, uLong p) : size(s), pos(p) {}

		UINT size;	// uncompressed size
		uLong pos;	// pos in zip directory;
		//std::string name;
	};

private:
	unzFile m_uzf;
	std::vector<FileInfo> m_files;
};

ZipFile::ZipFile()
{
	m_uzf = NULL;
}

ZipFile::~ZipFile()
{
	ASSERT(m_uzf == NULL);
}

bool ZipFile::Open(const char* pszFilename)
{
	m_uzf = unzOpen(pszFilename);
	if (m_uzf == NULL)
		return false;

	int err;
	unz_global_info gi;
	unzGetGlobalInfo(m_uzf, &gi);

	ASSERT(m_files.empty());
	m_files.reserve(gi.number_entry);

	unz_file_info file_info;
	unz_file_pos file_pos;
	char filename_inzip[256];

	for (UINT i=0;;)
	{
		err = unzGetCurrentFileInfo(m_uzf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
		if (err != UNZ_OK)
		{
			LogE("ZIP error %d with zipfile in unzGetCurrentFileInfo", err);
			break;
		}

		if(file_info.flag & 1)
		{
			// encrypted zip file must not enter here
			LogE("Encrypted zip file: %s", filename_inzip);
			err = UNZ_INTERNALERROR;
			break;
		}

		unzGetFilePos(m_uzf, &file_pos);

		m_files.emplace_back(file_info.uncompressed_size, file_pos.pos_in_zip_directory);

		//LogConsole("%d: ZipFile: %s (%d, %d)\n", i, filename_inzip, file_pos.pos_in_zip_directory, file_pos.num_of_file);

		if (++i < gi.number_entry)
		{
			err = unzGoToNextFile(m_uzf);
			if (err != UNZ_OK)
			{
				LogE("ZIP error %d with zipfile in unzGoToNextFile %d,%d", err, i, gi.number_entry);
				break;
			}
		}
		else
		{
			break;
		}
	}

	if (err != UNZ_OK)
	{
		ASSERT(!"Zip failed.");
		return false;
	}

	return true;
}

void ZipFile::Close()
{
	if (m_uzf)
	{
		unzClose(m_uzf);
		m_uzf = NULL;
	}
}

PicFile* ZipFile::GetFile(int index)
{
	ASSERT(m_uzf != NULL);

	if (index < 0 || index >= m_files.size())
		return NULL;

	unz_file_pos file_pos;
	file_pos.pos_in_zip_directory = m_files[index].pos;
	file_pos.num_of_file = index;
	int err = unzGoToFilePos(m_uzf, &file_pos);
	if (err == UNZ_OK)
	{
		err = unzOpenCurrentFile(m_uzf);
		if (UNZ_OK == err)
		{
			const int fileSize = m_files[index].size;
			PicFile* pRet = new PicFile(fileSize);
			err = unzReadCurrentFile(m_uzf, pRet->GetBits(), fileSize);
			if (err != fileSize)
			{
				LogE("unzRead failed (req=%d, read=%d)", fileSize, err);
			}

			unzCloseCurrentFile(m_uzf);
			return pRet;
		}
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////

bool GetZipFileInfo(int nVolumeId, std::string& outStr)
{
	std::string zipPathname;
	g_volMan.GetArcPathname(nVolumeId, zipPathname);

	unzFile uzf = unzOpen(zipPathname.c_str());
	if (uzf == NULL)
		return false;

	int err;
	unz_global_info gi;
	unzGetGlobalInfo(uzf, &gi);

	unz_file_info file_info;
	char filename_inzip[256];

	char* pSlash;
	char buff[512];
	outStr.clear();
	outStr.append("{\"entries\":[");

	for (UINT i = 0;;)
	{
		err = unzGetCurrentFileInfo(uzf, &file_info, filename_inzip, sizeof(filename_inzip), NULL, 0, NULL, 0);
		if (err != UNZ_OK)
		{
			LogE("GetZipFileInfo(%s): unzGetCurrentFileInfo err=%d", zipPathname.c_str(), err);
			break;
		}

		if (file_info.flag & 1)
		{
			// encrypted zip file must not enter here
			LogE("GetZipFileInfo(%s): Encrypted file=%s", zipPathname.c_str(), filename_inzip);
			err = UNZ_INTERNALERROR;
			break;
		}

		while (pSlash = strchr(filename_inzip, '\\'))
		{
			*pSlash = '/';
		}

		_snprintf_s(buff, 512, _TRUNCATE, "[\"%s\",%ld]", filename_inzip, file_info.uncompressed_size);
		outStr.append(buff);
		
		if (++i < gi.number_entry)
		{
			outStr.append(",\n");

			err = unzGoToNextFile(uzf);
			if (err != UNZ_OK)
			{
				LogE("GetZipFileInfo(%s): unzGoToNextFile (%d/%d) err=%d", zipPathname.c_str(), i, gi.number_entry, err);
				break;
			}
		}
		else
		{
			break;
		}
	}

	unzClose(uzf);

	if (err != UNZ_OK)
	{
		return false;
	}

	long fileSize = 0;
	FILE* fp;
	fopen_s(&fp, zipPathname.c_str(), "rb");
	if (fp != NULL)
	{
		fseek(fp, 0, SEEK_END);
		fileSize = ftell(fp);
		fclose(fp);
	}

	_snprintf_s(buff, 512, _TRUNCATE, "],\n\"file_size\":%ld}", fileSize);
	outStr.append(buff);

	return true;
}

//////////////////////////////////////////////////////////////////////////

ArcFile* ArcFile::OpenZipFile(const char* pszFilename)
{
	ZipFile* pArc = new ZipFile();
	if (pArc->Open(pszFilename))
	{
		return pArc;
	}

	LogE("ZipFile open failed: %s\n", pszFilename);

	delete pArc;
	return NULL;
}
