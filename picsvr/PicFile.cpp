#include "stdafx.h"
#include "dbglog.h"
#include "TitleVolume.h"
#include "SerialJobQ.h"

using namespace web::http;
using namespace concurrency;

static std::atomic_int s_nTotalPicCount;

PicFile::PicFile(int fileSize) : m_nRefCount(1)//, m_response(status_codes::OK)
{
	m_pFileData = (BYTE*)malloc(fileSize);
	m_nFileSize = fileSize;

	++s_nTotalPicCount;
	//Concurrency::streams::rawptr_buffer<uint8_t> rawbuf(m_pFileData, fileSize, std::ios::in);
	//m_response.set_body(rawbuf, U("image/jpeg"));
}

PicFile::~PicFile()
{
	--s_nTotalPicCount;
	ASSERT(m_nRefCount == 0);
	free(m_pFileData);
	LogD("PicFile(%d) %d bytes deleted", m_nPageId, m_nFileSize);
}

void PicFile::SetId(TitleVolume* pParent, int nPageId)
{
	m_pParent = pParent;
	m_nPageId = nPageId;
}

int PicFile::AddRef()
{
	//LogV("PicFile(%d)::AddRef(before=%d)", m_nPageId, m_nRefCount);
	return (++m_nRefCount);
}

void PicFile::Release()
{
	//LogV("PicFile(%d)::Release(before=%d)", m_nPageId, m_nRefCount);
	if (--m_nRefCount == 0)
	{
		m_pParent->Release();
		
		delete this;
		return;
	}

	ASSERT(m_nRefCount >= 0);
}

void PicFile::Reply(const web::http::http_request& request)
{
	if (AddRef() > 1)
	{
		Concurrency::streams::rawptr_buffer<uint8_t> rawbuf(m_pFileData, m_nFileSize, std::ios::in);
		request.reply(status_codes::OK, rawbuf.create_istream(), m_nFileSize, U("image/jpeg")).then([this] { Release(); });
	}
	else
	{
		LogE("PicFile.Reply: already dead object?");
		Release();
		request.reply(status_codes::NoContent, U("Cache miss"));
	}
}

//////////////////////////////////////////////////////////////////////////

CachePicFile::CachePicFile()
{
	s_nTotalPicCount = 0;
	m_allCachedSize = 0;
}

CachePicFile::~CachePicFile()
{
	ASSERT(m_mapPics.empty());
	ASSERT(m_mapKey2Pair.empty());
	ASSERT(m_listKeys.empty());
}

void CachePicFile::SetMaxSizeMB(int nCacheSizeMB)
{
	m_nMaxCacheSize = nCacheSizeMB * 1024 * 1024;
	LogI("CacheSize: %lld bytes", m_nMaxCacheSize);
}

void CachePicFile::Clear()
{
	for (KeyMapType::iterator itr = m_mapKey2Pair.begin(); itr != m_mapKey2Pair.end(); ++itr)
	{
		itr->second.first->Release();
	}

	m_mapKey2Pair.clear();
	m_listKeys.clear();
	m_mapPics.clear();
}

#define MAKE_PICFILE_KEY(_vol,_pge)		(PicKeyType)(((uint64_t)_vol<<32)|_pge)

PicFile* CachePicFile::FindPicFile(int nVolumeId, int nPageId)
{
	PicKeyType key = MAKE_PICFILE_KEY(nVolumeId, nPageId);

	tbb::concurrent_hash_map<PicKeyType, PicFile*>::const_accessor ca;
	if (m_mapPics.find(ca, key))
	{
		g_serialJQ.AddJob_CacheUpdateUsed(key);
		return ca->second;
	}
	
	return NULL;
}

bool CachePicFile::Add(int nVolumeId, int nPageId, PicFile* pPic)
{
	PicKeyType key = MAKE_PICFILE_KEY(nVolumeId, nPageId);

	tbb::concurrent_hash_map<PicKeyType, PicFile*>::accessor acc;
	if (m_mapPics.insert(acc, key))
	{
		acc->second = pPic;
		g_serialJQ.AddJob_CacheInsertNew(key, pPic);
		return true;
	}

	LogE("Impossible??? Cache.Add failed (%d,%d)", nVolumeId, nPageId);
	return false;
}

void CachePicFile::UpdateKeyUsed(PicKeyType key)
{
	KeyMapType::iterator itr = m_mapKey2Pair.find(key);
	if (itr != m_mapKey2Pair.end())
	{
		// found, update LRU history
		m_listKeys.splice(m_listKeys.end(), m_listKeys, itr->second.second);
	}
	else
	{
		LogE("??? cache key not found (%llx)", key);
	}
}

void CachePicFile::InsertNewKey(PicKeyType key, PicFile* pPic)
{
	ASSERT(m_mapKey2Pair.find(key) == m_mapKey2Pair.end());

	KeyListType::iterator itr = m_listKeys.insert(m_listKeys.end(), key);
	m_mapKey2Pair[key] = std::make_pair(pPic, itr);

	m_allCachedSize += pPic->GetFileSize();
	LogV("CacheSize +now: %lld", m_allCachedSize.load());

	while (m_allCachedSize > m_nMaxCacheSize)
	{
		const KeyMapType::iterator itrEvict = m_mapKey2Pair.find(m_listKeys.front());
		ASSERT(itrEvict != m_mapKey2Pair.end());

		tbb::concurrent_hash_map<PicKeyType, PicFile*>::accessor acc;
		if (m_mapPics.find(acc, itrEvict->first))
		{
			ASSERT(itrEvict->second.first == acc->second);
			PicFile* pEvictPic = acc->second;
			if (m_mapPics.erase(acc))
			{
				m_allCachedSize -= pEvictPic->GetFileSize();
				pEvictPic->Release();
				LogV("CacheSize -now: %lld", m_allCachedSize.load());

				m_mapKey2Pair.erase(itrEvict);
				m_listKeys.pop_front();
			}
			else
			{
				LogE("CachePicInsert: m_mapPics.erase failed");
				break;
			}
		}
		else
		{
			LogE("CachePicInsert: m_mapPics.find failed");
			break;
		}
	}
}

void CachePicFile::_DumpLruState()
{
	size_t tbbSize = m_mapPics.size();
	size_t stlSize = m_listKeys.size();
	size_t stlSize2 = m_mapPics.size();
	LogD("LRU size: %d,%d,%d, totalPic=%d", tbbSize, stlSize, stlSize2, s_nTotalPicCount.load());

	int i = 0;
	for (KeyListType::iterator itr = m_listKeys.begin(); itr != m_listKeys.end(); ++itr)
	{
		PicKeyType key = *itr;
		UINT volu = (UINT)(key >> 32);
		UINT page = (UINT)(key & 0xFFFFFFF);
		LogD("LRU %d: %d,%d", ++i, volu, page);
	}
}
