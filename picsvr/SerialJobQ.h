#pragma once

class JobItem
{
public:
	virtual ~JobItem() {}
	virtual void ProcessJob() = 0;
};

// job processing loop is executed on main thread (call EnterLoop)
class SerialJobProcessor
{
public:
	bool Create();
	void Destroy();

	void EnterLoop();
	void FireShutdown();

	void AddJob_CacheUpdateUsed(CachePicFile::PicKeyType& key);
	void AddJob_CacheInsertNew(CachePicFile::PicKeyType& key, PicFile* pPic);

	void AddJob_AddToPurgeList(TitleVolume* pVolume);
	void AddJob_Purge();

	inline bool IsRunning() const { return m_bRunning; }

private:
	void AddJob(JobItem* pJob);

	bool m_bRunning;

	tbb::concurrent_bounded_queue<JobItem*> m_jobs;

	std::vector<TitleVolume*> m_vecPurgeVolumes;

	//std::mutex m_mutexShutdown;
	//std::condition_variable m_cvShutdown;

	friend class Job_AddToPurgeList;
	friend class Job_Purge;
};


extern SerialJobProcessor g_serialJQ;