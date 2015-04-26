#include "stdafx.h"
#include "TitleVolume.h"
#include "SerialJobQ.h"
#include "dbglog.h"

class Job_CacheUpdateUsed : public JobItem
{
public:
	CachePicFile::PicKeyType m_key;

	virtual void ProcessJob()
	{
		g_cachePics.UpdateKeyUsed(m_key);
	}
};

void SerialJobProcessor::AddJob_CacheUpdateUsed(CachePicFile::PicKeyType& key)
{
	Job_CacheUpdateUsed* job = new Job_CacheUpdateUsed();
	job->m_key = key;

	AddJob(job);
}

//////////////////////////////////////////////////////////////////////////

class Job_CacheInsertNew : public JobItem
{
public:
	CachePicFile::PicKeyType m_key;
	PicFile* m_pPic;

	virtual void ProcessJob()
	{
		g_cachePics.InsertNewKey(m_key, m_pPic);
	}
};

void SerialJobProcessor::AddJob_CacheInsertNew(CachePicFile::PicKeyType& key, PicFile* pPic)
{
	Job_CacheInsertNew* job = new Job_CacheInsertNew();
	job->m_key = key;
	job->m_pPic = pPic;

	AddJob(job);
}

//////////////////////////////////////////////////////////////////////////

class Job_AddToPurgeList : public JobItem
{
public:
	TitleVolume* m_pVolume;

	virtual void ProcessJob()
	{
		g_serialJQ.m_vecPurgeVolumes.push_back(m_pVolume);
	}
};

void SerialJobProcessor::AddJob_AddToPurgeList(TitleVolume* pVolume)
{
	Job_AddToPurgeList* pJob = new Job_AddToPurgeList();
	pJob->m_pVolume = pVolume;
	
	AddJob(pJob);
}

//////////////////////////////////////////////////////////////////////////

class Job_Purge : public JobItem
{
public:
	virtual void ProcessJob()
	{
		//g_cachePics._DumpLruState();

		if (!g_serialJQ.m_vecPurgeVolumes.empty())
		{
			for (std::vector<TitleVolume*>::iterator itr = g_serialJQ.m_vecPurgeVolumes.begin();
				itr != g_serialJQ.m_vecPurgeVolumes.end(); ++itr)
			{
				TitleVolume* pVolume = *itr;
				if (pVolume->CanPurge())
				{
					g_volMan.RemoveVolume(pVolume);
				}
			}

			g_serialJQ.m_vecPurgeVolumes.clear();
		}
	}
};

void SerialJobProcessor::AddJob_Purge()
{
	Job_Purge* pJob = new Job_Purge();
	AddJob(pJob);
}

//////////////////////////////////////////////////////////////////////////

bool SerialJobProcessor::Create()
{
	m_jobs.set_capacity(256);
	m_bRunning = true;
	return true;
}

void SerialJobProcessor::Destroy()
{
	JobItem* pJob;
	while (m_jobs.try_pop(pJob))
	{
		if (pJob != NULL)
			delete pJob;
	}
}

void SerialJobProcessor::FireShutdown()
{
	LogD("SerialJobProcessor::FireShutdown() called");
	m_bRunning = false;
	m_jobs.abort();

	//m_cvShutdown.notify_one();
}

void SerialJobProcessor::EnterLoop()
{
	JobItem* pJob;

	m_bRunning = true;
	while (m_bRunning)
	{
		try
		{
			pJob = NULL;
			m_jobs.pop(pJob);
		}
		catch (...)
		{			
LogConsoleA("Got m_jobs.pop exception\n");
			continue;
		}

		if (pJob != NULL)
		{
			pJob->ProcessJob();
			delete pJob;
		}
	}

/*
	std::unique_lock<std::mutex> ulShutdown(m_mutexShutdown);
	m_cvShutdown.wait(ulShutdown, []{ return m_shutdownFired; });

	LogConsole("Shutdown gracefully...\n");
	ulShutdown.unlock();
*/
}

void SerialJobProcessor::AddJob(JobItem* pJob)
{
	m_jobs.push(pJob);
}
