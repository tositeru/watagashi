#pragma once

#include <queue>
#include <memory>
#include <mutex>

#include <boost/filesystem.hpp>

#include "buildEnviroment.h"

namespace watagashi
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

struct Process_
{
    enum class BuildResult {
        Success,
        Skip,
        Failed,
    };

    Builder_ const& builder;
    data::Compiler const& compiler;
    boost::filesystem::path inputFilepath;
    boost::filesystem::path outputFilepath;

    Process_(
        Builder_ const& builder,
        data::Compiler const& compiler,
        boost::filesystem::path const& inputFilepath,
        boost::filesystem::path const& outputFilepath);
    BuildResult compile()const;
};

class ProcessServer
{    
public:
    ProcessServer();
    ~ProcessServer();
    
    void addProcess(std::unique_ptr<Process> pProcess);
    std::unique_ptr<Process> serveProcess();
    void notifyEndOfProcess(Process::BuildResult result);

    void addProcess(std::unique_ptr<Process_> pProcess);
    std::unique_ptr<Process_> serveProcess_();
    void notifyEndOfProcess(Process_::BuildResult result);

    bool isFinish()const;

public:
    size_t successCount()const { return this->mSuccessCount; }
    size_t skipCount()const { return this->mSkipLinkCount; }
    size_t failedCount()const { return this->mFailedCount; }
    
private:
    std::mutex mMutex;
    std::queue<std::unique_ptr<Process>> mpProcessQueue;
    std::queue<std::unique_ptr<Process_>> mpProcess_Queue;
    size_t mProcessSum;
    size_t mEndProcessSum;
    
    size_t mSuccessCount;
    size_t mSkipLinkCount;
    size_t mFailedCount;
};

}
