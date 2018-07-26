#pragma once

#include <queue>
#include <memory>
#include <mutex>

#include <boost/filesystem.hpp>

namespace watagashi
{

class Builder;
namespace data
{
struct Compiler;
}

struct Process
{
    enum class BuildResult {
        Success,
        Skip,
        Failed,
    };

    Builder const& builder;
    data::Compiler const& compiler;
    boost::filesystem::path inputFilepath;
    boost::filesystem::path outputFilepath;

    Process(
        Builder const& builder,
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
    std::unique_ptr<Process> serveProcess_();
    void notifyEndOfProcess(Process::BuildResult result);

    bool isFinish()const;

public:
    size_t successCount()const { return this->mSuccessCount; }
    size_t skipCount()const { return this->mSkipLinkCount; }
    size_t failedCount()const { return this->mFailedCount; }
    
private:
    std::mutex mMutex;
    std::queue<std::unique_ptr<Process>> mpProcess_Queue;
    size_t mProcessSum;
    size_t mEndProcessSum;
    
    size_t mSuccessCount;
    size_t mSkipLinkCount;
    size_t mFailedCount;
};

}
