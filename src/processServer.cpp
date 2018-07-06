#include "processServer.h"

#include <iostream>

#include "utility.h"
#include "builder.h"
#include "data.h"

using namespace std;
namespace fs = boost::filesystem;

namespace watagashi
{

//--------------------------------------------------------------------------------------
//
//  class Process_
//
//--------------------------------------------------------------------------------------

Process::Process(
    Builder const& builder,
    data::Compiler const& compiler,
    boost::filesystem::path const& inputFilepath,
    boost::filesystem::path const& outputFilepath)
    : builder(builder)
    , compiler(compiler)
    , inputFilepath(inputFilepath)
    , outputFilepath(outputFilepath)
{}

Process::BuildResult Process::compile()const
{
    //cout << "run " << this->inputFilepath << endl;
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

    // check match Filter
    data::FileFilter const* pFileFilter = nullptr;
    for (auto&& filter : project.fileFilters) {
        if (matchFilepath(filter.targetKeyward, inputFilepath, project.rootDirectory)) {
            pFileFilter = &filter;
            break;
        }
    }
    if (pFileFilter) {
        auto result = data::runProcesses(pFileFilter->preprocess, runData);
        if (data::TaskProcess::Result::Success != result) {
            return preprocessResult == data::TaskProcess::Result::Skip
                ? BuildResult::Skip
                : BuildResult::Failed;
        }
    }

    auto cmd = data::makeCompileCommand(taskBundle.compileObj, this->inputFilepath, this->outputFilepath, project, pFileFilter);
    cout << "running: " << cmd << endl;
    if (!runCommand(cmd)) {
        return BuildResult::Failed;
    }

    if (pFileFilter) {
        auto result = data::runProcesses(pFileFilter->postprocess, runData);
        if (data::TaskProcess::Result::Success != result) {
            return preprocessResult == data::TaskProcess::Result::Skip
                ? BuildResult::Skip
                : BuildResult::Failed;
        }
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

    this->mpProcess_Queue.emplace(std::move(pProcess));
    ++this->mProcessSum;
}

std::unique_ptr<Process> ProcessServer::serveProcess_()
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

void ProcessServer::notifyEndOfProcess(Process::BuildResult result)
{
    std::lock_guard<std::mutex> lock(this->mMutex);
    ++this->mEndProcessSum;

    switch (result) {
    case Process::BuildResult::Success:
        ++this->mSuccessCount;
        break;
    case Process::BuildResult::Skip:
        ++this->mSkipLinkCount;
        break;
    case Process::BuildResult::Failed:
        ++this->mFailedCount;
        break;
    }
}

bool ProcessServer::isFinish()const
{
    return this->mProcessSum == this->mEndProcessSum;
}

}
