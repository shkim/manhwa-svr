#include "stdafx.h"
#include "dbglog.h"
#include "TitleVolume.h"
#include "SerialJobQ.h"

using namespace concurrency;

TitleVolume::TitleVolume(int nVolumeId)
{
	m_nVolumeId = nVolumeId;
	m_pArc = NULL;
	m_bArcOpenError = false;
}

TitleVolume::~TitleVolume()
{
	LogI("Volume(%d) destroy.", m_nVolumeId);
	if (m_pArc != NULL)
	{
		CloseArc();
	}

	ASSERT(m_pArc == NULL && m_nRefCount == 0);
}

void TitleVolume::Release()
{
	//LogV("TitleVolume::Release(before=%d)", m_nRefCount);
	if (--m_nRefCount == 0)
	{
		//LogI("Volume(%d) refCnt=0, add to purge list");
		g_serialJQ.AddJob_AddToPurgeList(this);
	}
}

bool TitleVolume::CanPurge()
{
	return (m_nRefCount == 0);
}

bool TitleVolume::OpenArc()
{
	if (m_bArcOpenError)
	{
		LogE("OpenArc(%d) failed again.", m_nVolumeId);
		return false;
	}

	std::string zipPathname;
	g_volMan.GetArcPathname(m_nVolumeId, zipPathname);

	m_pArc = ArcFile::OpenZipFile(zipPathname.c_str());

	if (m_pArc == NULL)
	{
		LogE("Volume(%d) OpenArc failed.", m_nVolumeId);
		m_bArcOpenError = true;
		return false;
	}

	LogD("Volume(%d) opened Arc file.", m_nVolumeId);
	return true;
}

void TitleVolume::CloseArc()
{
	if (m_pArc != NULL)
	{
		LogD("Volume(%d) closed Arc.", m_nVolumeId);
		m_pArc->Close();
		delete m_pArc;
		m_pArc = NULL;
		m_bArcOpenError = false;
	}
}

PicFile* TitleVolume::GetPicFile(int nPageId)
{
	//LogD("TitleVolume::GetPicFile(%d) called", nPageId);
	std::lock_guard<std::mutex> lock(m_mutexGetPic);

	PicFile* pPic;

	if (m_pArc == NULL)
	{
		if (!OpenArc())
			return NULL;
	}

	pPic = g_cachePics.FindPicFile(m_nVolumeId, nPageId);
	if (pPic != NULL)
	{
		LogD("TitleVolume::GetPicFile(%d): hit cache", nPageId);
		return pPic;
	}

	pPic = m_pArc->GetFile(nPageId);
	if (pPic == NULL)
	{
		LogD("GetFile(%d:%d) failed", m_nVolumeId, nPageId);
		return NULL;
	}

	pPic->SetId(this, nPageId);
	++m_nRefCount;

	if (g_cachePics.Add(m_nVolumeId, nPageId, pPic))
	{
		LogD("Cache.AddPic(%d,%d)", m_nVolumeId, nPageId);
	}
	else
	{
		ASSERT(!"Cache.AddPic failed");
	}

	return pPic;
}

bool TitleVolume::HasSessionCache(const utility::string_t& sessionId)
{
	if (sessionId.compare(U("shkim=handsome")) == 0)
		return true;

	tbb::concurrent_hash_map<utility::string_t, int>::const_accessor ca;
	return m_sessionCache.find(ca, sessionId);
}

void TitleVolume::AddSessionCache(const utility::string_t& sessionId)
{
	tbb::concurrent_hash_map<utility::string_t, int>::accessor acc;
	if (m_sessionCache.insert(acc, sessionId))
	{
		acc->second = 0;
	}
	else
	{
		LogE("AddSessionCache: insert failed for session %s", PathFilenamer::T2A(sessionId.c_str()));
	}
}

//////////////////////////////////////////////////////////////////////////

VolumeManager::VolumeManager()
{
	m_nMaxVolumeId = 100;
	//m_mapVolumeTasks.reserve(128);
}

VolumeManager::~VolumeManager()
{
}

void VolumeManager::Destroy()
{
	for (tbb::concurrent_hash_map<int, TitleVolume*>::iterator itr = m_mapVolumes.begin();
		itr != m_mapVolumes.end(); ++itr)
	{
		delete itr->second;
	}

	m_mapVolumes.clear();
}

int VolumeManager::GetMaxVolumeId()
{
	return m_nMaxVolumeId;
}

void VolumeManager::SetMaxVolumeId(int nVolumeId)
{
	m_nMaxVolumeId = nVolumeId;
	LogI("MaxVolumeId updated to %d", m_nMaxVolumeId);
}

bool VolumeManager::SetDataDir(const PathFilenamer::PathChar* pszDataDir)
{
#ifdef _WIN32
	if (_taccess(pszDataDir, 0) < 0)
#else
	if (access(pszDataDir, 0) < 0)
#endif
	{
		LogE("DataDir not found: %s", PathFilenamer::T2A(pszDataDir));
		return false;
	}

	m_strDataDir = pszDataDir;
	return true;
}

void VolumeManager::GetArcPathname(int nVolumeId, std::string& outStr)
{
	PathFilenamer::PathChar fname[128];

	int nFolderNum = nVolumeId / 1000;
#ifdef _UNICODE
	_snwprintf(fname, 128, L"%d/%d.zip", nFolderNum, nVolumeId);
#else
	snprintf(fname, 128, "%d/%d.zip", nFolderNum, nVolumeId);
#endif

	PathFilenamer dataDir(m_strDataDir.c_str());
	const char* pszFilename = PathFilenamer::T2A(dataDir.GetPathname(fname));

	outStr = pszFilename;

}

TitleVolume* VolumeManager::GetVolume(int nVolumeId)
{
	int retryCount = 0;

_retry:
	tbb::concurrent_hash_map<int, TitleVolume*>::const_accessor ca;
	if (m_mapVolumes.find(ca, nVolumeId))
	{
		return ca->second;
	}

	tbb::concurrent_hash_map<int, TitleVolume*>::accessor acc;
	if (m_mapVolumes.insert(acc, nVolumeId))
	{
		TitleVolume* pVolume = new TitleVolume(nVolumeId);
		acc->second = pVolume;
		return pVolume;
	}

	if (++retryCount < 8)
	{
		LogD("GetVolume(%d): retry %d ...", nVolumeId, retryCount);

		std::chrono::milliseconds dura(100);
		std::this_thread::sleep_for(dura);

		goto _retry;
	}
	
	LogE("GetVolume(%d) finally failed.", nVolumeId);
	return NULL;
}

void VolumeManager::RemoveVolume(TitleVolume* pVolume)
{
	tbb::concurrent_hash_map<int, TitleVolume*>::accessor acc;
	if (m_mapVolumes.find(acc, pVolume->GetVolumeId()))
	{
		if (m_mapVolumes.erase(acc))
		{
			delete pVolume;
			return;
		}
		else
		{
			LogE("VolumeManager::RemoveVolume map erase(%d) failed.", pVolume->GetVolumeId());
		}
	}
	else
	{
		LogE("VolumeManager::RemoveVolume find(%d) failed.", pVolume->GetVolumeId());
	}
}

/*
void VolumeManager::AddToPurgeList(TitleVolume* pVolume)
{
	if (g_serialJQ.IsRunning())
	{
		m_vecPurgeVolumes.push_back(pVolume);
	}
}

void VolumeManager::PurgeOldVolumes()
{
	LogConsole("Check purge...\n");

	if (m_vecPurgeVolumes.empty())
		return;

	for (tbb::concurrent_vector<TitleVolume*>::iterator itr = m_vecPurgeVolumes.begin();
		itr != m_vecPurgeVolumes.end(); ++itr)
	{
		TitleVolume* pVolume = *itr;
		if (pVolume->CanPurge())
		{
			tbb::concurrent_hash_map<int, VolumeTask>::accessor acc;
			if (m_mapVolumeTasks.find(acc, pVolume->GetVolumeId()))
			{
				VolumeTask vt = acc->second;
				if (m_mapVolumeTasks.erase(acc))
				{
					LogI("Volume(%d) purged", pVolume->GetVolumeId());
				}
				else
				{
					ASSERT(!"Purge volume not erased?");
				}
			}
			else
			{
				ASSERT(!"volumeTask not found (TO purge)");
			}
		}
	}

	m_vecPurgeVolumes.clear();
}
*/
