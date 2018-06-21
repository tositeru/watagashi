#include "processServer.h"

#include <iostream>

using namespace std;

namespace watagashi
{

Process::BuildResult Process::compile()
{
    if(!this->env.preprocessCompiler(BuildEnviroment::TaskType::eCompile, this->inputFilepath, outputFilepath)) {
        cout << "skip compile " << this->inputFilepath << endl;
        return BuildResult::eSkip;
    }
            
    // file to preprocess
    if(this->env.isExistItem(BuildEnviroment::HIERARCHY::eFileToProcess)) {
        this->env.fileToProcess().exePreprocess(this->inputFilepath);
    }
    
    // compile .obj file
    
    auto cmdStr = this->env.createCompileCommand(this->inputFilepath, this->outputFilepath);
    cout << "running: " << cmdStr << endl;
    
    auto result = std::system(cmdStr.c_str());
    if(0 != result) {
        return BuildResult::eFailed;
    } else {
        if(this->env.isExistItem(BuildEnviroment::HIERARCHY::eFileToProcess)) {
            this->env.fileToProcess().exePostprocess(this->inputFilepath);
        }
        
        if(!this->env.postprocessCompiler(BuildEnviroment::TaskType::eCompile, this->inputFilepath, this->outputFilepath)) {
            return BuildResult::eFailed;
        }
        return BuildResult::eSuccess;
    }
}

ProcessServer::ProcessServer()
    : mProcessSum(0u)
    , mEndProcessSum(0u)
    , mSuccessCount(0)
    , mSkipLinkCount(0)
    , mFailedCount(0)
{ }

ProcessServer::~ProcessServer()
{
}

void ProcessServer::addProcess(std::unique_ptr<Process> pProcess)
{
    std::lock_guard<std::mutex> lock(this->mMutex);
    
    this->mpProcessQueue.emplace(std::move(pProcess));
    ++this->mProcessSum;
}

std::unique_ptr<Process> ProcessServer::serveProcess()
{
    std::lock_guard<std::mutex> lock(this->mMutex);
    if(!this->mpProcessQueue.empty()) {    
        auto process = std::move(this->mpProcessQueue.front());
        this->mpProcessQueue.pop();
        return std::move(process);
    } else {
        return nullptr;
    }
}

void ProcessServer::notifyEndOfProcess(Process::BuildResult result)
{
    std::lock_guard<std::mutex> lock(this->mMutex);
    ++this->mEndProcessSum;
    
    switch(result) {
    case Process::BuildResult::eSuccess:
        ++this->mSuccessCount;
        break;
    case Process::BuildResult::eSkip:
        ++this->mSkipLinkCount;
        break;
    case Process::BuildResult::eFailed:
        ++this->mFailedCount;
        break;
    }
}

bool ProcessServer::isFinish()const
{
    return this->mProcessSum == this->mEndProcessSum;
}

}
