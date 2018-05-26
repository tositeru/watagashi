#pragma once

#include <queue>
#include <memory>
#include <mutex>

#include <boost/filesystem.hpp>

#include "buildEnviroment.h"

namespace watagasi
{

struct Process
{
	enum class BuildResult {
		eSuccess,
		eSkip,
		eFailed,
	};

	BuildEnviroment env;
	boost::filesystem::path inputFilepath;
	boost::filesystem::path outputFilepath;
	
	BuildResult compile();
};

class ProcessServer
{	
public:
	ProcessServer();
	~ProcessServer();
	
	void addProcess(std::unique_ptr<Process> pProcess);
	std::unique_ptr<Process> serveProcess();
	
	void notifyEndOfProcess(Process::BuildResult result);
	bool isFinish()const;

public:
	size_t successCount()const { return this->mSuccessCount; }
	size_t skipCount()const { return this->mSkipLinkCount; }
	size_t failedCount()const { return this->mFailedCount; }
	
private:
	std::mutex mMutex;
	std::queue<std::unique_ptr<Process>> mpProcessQueue;
	size_t mProcessSum;
	size_t mEndProcessSum;
	
	size_t mSuccessCount;
	size_t mSkipLinkCount;
	size_t mFailedCount;
};

}
