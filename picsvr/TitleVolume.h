#pragma once

#include "PathFilenamer.h"

class TitleVolume;

#ifdef _WIN32
using namespace concurrency;
#else
using namespace pplx;
#endif

class PicFile
{
public:
	PicFile(int fileSize);

	int AddRef();
	void Release();
	void SetId(TitleVolume* pParent, int nPageId);

	void Reply(const web::http::http_request& request);

	//inline web::http::http_response& GetHttpResponse() { return m_response; }	

	inline TitleVolume* GetParent() const
	{
		return m_pParent;
	}

	inline int GetPageId() const
	{
		return m_nPageId;
	}

	inline BYTE* GetBits()
	{
		return m_pFileData;
	}

	inline int GetFileSize() const
	{
		return m_nFileSize;
	}

private:
	~PicFile();

	std::atomic<int> m_nRefCount;
	BYTE* m_pFileData;
	int m_nFileSize;

	TitleVolume* m_pParent;
	int m_nPageId;

	//web::http::http_response m_response;

	friend class TitleVolume;
};

class CachePicFile
{
public:
	CachePicFile();
	~CachePicFile();
	
	void SetMaxSizeMB(int nCacheSizeMB);
	void Clear();
	PicFile* FindPicFile(int nVolumeId, int nPageId);
	bool Add(int nVolumeId, int nPageId, PicFile* pPic);

	typedef uint64_t PicKeyType;
	void UpdateKeyUsed(PicKeyType key);
	void InsertNewKey(PicKeyType key, PicFile* pPic);

	void _DumpLruState();
private:

	tbb::concurrent_hash_map<PicKeyType, PicFile*> m_mapPics;

	std::atomic<uint64_t> m_allCachedSize;
	uint64_t m_nMaxCacheSize; // in bytes

	// LRU cache
	typedef std::list<PicKeyType> KeyListType;
	typedef std::map<PicKeyType, std::pair<PicFile*, KeyListType::iterator>> KeyMapType;
	KeyListType m_listKeys;
	KeyMapType m_mapKey2Pair;

	//std::mutex m_mutex;
	//std::map<uint64_t, PicFile*> m_pics;
};

class ArcFile
{
public:
	virtual ~ArcFile() {}

	virtual bool Open(const char* pszFilename) = 0;
	virtual void Close() = 0;
	virtual PicFile* GetFile(int index) = 0;
	virtual int GetCount() = 0;

	static ArcFile* OpenZipFile(const char* pszFilename);
};

struct ParamGetPic
{
	utility::string_t sessionId;
	int nVolumeId;
	int nPageId;
};

class TitleVolume
{
public:
	TitleVolume(int nVolumeId);
	~TitleVolume();

	void Release();

	bool OpenArc();
	void CloseArc();

	PicFile* GetPicFile(int nPageId);
	void RemovePicFile(PicFile* pPic);
	bool CanPurge();
	bool HasSessionCache(const utility::string_t& sessionId);
	void AddSessionCache(const utility::string_t& sessionId);

	inline int GetVolumeId() const
	{
		return m_nVolumeId;
	}

private:
	int m_nVolumeId;
	std::atomic<int> m_nRefCount;
	ArcFile* m_pArc;
	bool m_bArcOpenError;

	std::mutex m_mutexGetPic;
	tbb::concurrent_hash_map<utility::string_t, int> m_sessionCache;
	//concurrency::task_completion_event<ArcFile*> m_tceArcReady;
};

class ArcUploadHandler;

class VolumeManager
{
public:
	VolumeManager();
	~VolumeManager();

	void Destroy();
	int GetMaxVolumeId();
	void SetMaxVolumeId(int nVolumeId);
	TitleVolume* GetVolume(int nVolumeId);
	void RemoveVolume(TitleVolume* pVolume);

	void GetArcPathname(int nVolumeId, std::string& outStr);
	bool SetDataDir(const PathFilenamer::PathChar* pszDataDir);

	inline const PathFilenamer::PathChar* GetDataDir() const
	{
		return m_strDataDir.c_str();
	}

	ArcUploadHandler* RegisterArcUploader(int nVolumeId);
	void UnregisterArcUploader(ArcUploadHandler* pHandler);

private:
	tbb::concurrent_hash_map<int, TitleVolume*> m_mapVolumes;
	//typedef task<TitleVolume*> VolumeTask;
	//tbb::concurrent_hash_map<int, VolumeTask> m_mapVolumeTasks;
	int m_nMaxVolumeId;

	std::mutex m_mutexUploader;	
	std::map<int, ArcUploadHandler*> m_uploaders;
	//std::map<int, VolumeTask> m_volumeTasks;
	PathFilenamer::PathString m_strDataDir;
};

ArcFile* GuessAndOpenArcFile(const char* pszFilename);

extern CachePicFile g_cachePics;
extern VolumeManager g_volMan;
