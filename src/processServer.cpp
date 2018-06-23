#include "processServer.h"

#include <iostream>

#include "utility.h"
#include "builder.h"
#include "data.h"

using namespace std;
namespace fs = boost::filesystem;

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

//--------------------------------------------------------------------------------------
//
//  class Process_
//
//--------------------------------------------------------------------------------------

Process_::Process_(
    Builder_ const& builder,
    data::Compiler const& compiler,
    boost::filesystem::path const& inputFilepath,
    boost::filesystem::path const& outputFilepath)
    : builder(builder)
    , compiler(compiler)
    , inputFilepath(inputFilepath)
    , outputFilepath(outputFilepath)
{}

Process_::BuildResult Process_::compile()const
{
    auto& project = builder.project();
    auto& taskBundle = data::getTaskBundle(compiler, project.type);
    auto& task = taskBundle.compileObj;

    createDirectory(this->outputFilepath.parent_path());

    data::TaskProcess::RunData runData;
    runData.inputFilepath = this->inputFilepath;
    runData.outputFilepath = this->outputFilepath;
    runData.includeDirectories = project.includeDirectories;
    auto preprocessResult = data::runProcesses(task.preprocesses, runData);
    if (data::TaskProcess::Result::Success != preprocessResult) {
        return preprocessResult == data::TaskProcess::Result::Skip
            ? BuildResult::Skip
            : BuildResult::Failed;
    }

    auto cmd = data::makeCompileCommand(taskBundle.compileObj, this->inputFilepath, this->outputFilepath, project);
    cout << "running: " << cmd << endl;
    if (!runCommand(cmd)) {
        return BuildResult::Failed;
    }

    auto postprocessResult = data::runProcesses(task.postprocesses, runData);
    if (data::TaskProcess::Result::Success != postprocessResult) {
        return postprocessResult == data::TaskProcess::Result::Skip
            ? BuildResult::Skip
            : BuildResult::Failed;
    }

    return BuildResult::Success;
}

//--------------------------------------------------------------------------------------
//
//  class ProcessServer
//
//--------------------------------------------------------------------------------------
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

void ProcessServer::addProcess(std::unique_ptr<Process_> pProcess)
{
    std::lock_guard<std::mutex> lock(this->mMutex);

    this->mpProcess_Queue.emplace(std::move(pProcess));
    ++this->mProcessSum;
}

std::unique_ptr<Process_> ProcessServer::serveProcess_()
{
    std::lock_guard<std::mutex> lock(this->mMutex);
    if (!this->mpProcess_Queue.empty()) {
        auto process = std::move(this->mpProcess_Queue.front());
        this->mpProcess_Queue.pop();
        return std::move(process);
    } else {
        return nullptr;
    }
}

void ProcessServer::notifyEndOfProcess(Process_::BuildResult result)
{
    std::lock_guard<std::mutex> lock(this->mMutex);
    ++this->mEndProcessSum;

    switch (result) {
    case Process_::BuildResult::Success:
        ++this->mSuccessCount;
        break;
    case Process_::BuildResult::Skip:
        ++this->mSkipLinkCount;
        break;
    case Process_::BuildResult::Failed:
        ++this->mFailedCount;
        break;
    }
}

bool ProcessServer::isFinish()const
{
    return this->mProcessSum == this->mEndProcessSum;
}

}
