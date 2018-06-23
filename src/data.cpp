#include "data.h"

#include "exception.hpp"
#include "includeFileAnalyzer.h"

namespace watagashi::data
{

//--------------------------------------------------------------------------------------
//
//  struct TaskProcess
//
//--------------------------------------------------------------------------------------
static TaskProcess::Result runBuildInPocess(std::string const& content, TaskProcess::RunData const& data)
{
    if ("checkUpdate" == content) {
        return IncludeFileAnalyzer::sCheckUpdateTime(data.inputFilepath, data.outputFilepath, data.includeDirectories)
            ? TaskProcess::Result::Success
            : TaskProcess::Result::Skip;
    }

    AWESOME_THROW(std::invalid_argument) << "unknown content... content=" << content;
    return TaskProcess::Result::Failed;
}

TaskProcess::Result TaskProcess::run(RunData const& data)const
{
    switch (this->type) {
    case Type::Terminal:
        return runCommand(this->content)
            ? TaskProcess::Result::Success
            : TaskProcess::Result::Failed;
        break;
    case Type::BuildIn:
        return runBuildInPocess(this->content, data);
        break;
    default:
        AWESOME_THROW(std::invalid_argument)
            << "yet don't implement.";
    }
    return Result::Failed;
}

TaskProcess::Result runProcesses(std::vector<TaskProcess> const& processes, TaskProcess::RunData const& data)
{
    for (auto& process : processes) {
        auto r = process.run(data);
        if (TaskProcess::Result::Success != r) {
            return r;
        }
    }
    return TaskProcess::Result::Success;
}

//--------------------------------------------------------------------------------------
//
//  struct Task
//
//--------------------------------------------------------------------------------------

std::string makeCompileCommand(
    Task const& task,
    boost::filesystem::path const& inputFilepath,
    boost::filesystem::path const& outputFilepath,
    std::unordered_set<std::string> const& options,
    std::unordered_set<boost::filesystem::path> const& includeDirectories)
{
    std::stringstream cmd;
    cmd << task.command;

    // option prefix
    if (!task.optionPrefix.empty()) {
        cmd << " " << task.optionPrefix;
    }

    // input option
    if (!task.inputOption.empty()) {
        cmd << " " << task.inputOption;
    }
    cmd << " " << inputFilepath;

    // output option
    if (!task.outputOption.empty()) {
        cmd << " " << task.outputOption;
    }
    cmd << " " << outputFilepath;

    // options
    for (auto& op : options) {
        cmd << " " << op;
    }

    // include directories
    for (auto& dir : includeDirectories) {
        cmd << " -I=" << dir;
    }

    // option suffix
    if (!task.optionSuffix.empty()) {
        cmd << " " << task.optionSuffix;
    }

    return cmd.str();
}

std::string makeCompileCommand(
    Task const& task,
    boost::filesystem::path const& inputFilepath,
    boost::filesystem::path const& outputFilepath,
    Project const& project)
{
    return makeCompileCommand(task, inputFilepath, outputFilepath, project.compileOptions, project.includeDirectories);
}

std::string makeLinkCommand(
    Task const& task,
    boost::filesystem::path const& outputFilepath,
    std::vector<boost::filesystem::path> const& targets,
    std::unordered_set<std::string> const& options,
    std::unordered_set<std::string> const& libraries,
    std::unordered_set<boost::filesystem::path> const& libraryDirectories)
{
    std::stringstream cmd;
    cmd << task.command;

    // option prefix
    if (!task.optionPrefix.empty()) {
        cmd << " " << task.optionPrefix;
    }
    //TODO 変数対応 共有ライブラリだと必要になってくる

    //output option
    if (!task.outputOption.empty()) {
        cmd << " " << task.outputOption;
    }
    cmd << " " << outputFilepath.string();

    // input option
    if (!targets.empty()) {
        auto inputOption = task.inputOption.empty() ? "" : task.inputOption;
        for (auto& t : targets) {
            cmd << " " << inputOption << t.string();
        }
    }

    // options
    for (auto& op : options) {
        cmd << " " << op;
    }

    // link libraries
    for (auto& l : libraries) {
        cmd << " -l" << l;
    }

    // library directories
    for (auto& dir : libraryDirectories) {
        cmd << " -L" << dir.string();
    }

    // option suffix
    if (!task.optionSuffix.empty()) {
        cmd << " " << task.optionSuffix;
    }

    return cmd.str();
}

std::string makeLinkCommand(
    Task const& task,
    boost::filesystem::path const& outputFilepath,
    std::vector<boost::filesystem::path> const& targets,
    Project const& project)
{
    return makeLinkCommand(task, outputFilepath, targets,
        project.linkOptions, project.linkLibraries, project.libraryDirectories);
}

//--------------------------------------------------------------------------------------
//
//  struct TaskProcess
//
//--------------------------------------------------------------------------------------

TaskProcess::TaskProcess(Type type, std::string&& content)
    : type(type)
    , content(std::move(content))
{}

TaskProcess&& TaskProcess::setTypeAndContent(Type type_, std::string&& content_)
{
    this->type = type_;
    this->content = std::move(content_);
    return std::move(*this);
}

//--------------------------------------------------------------------------------------
//
//  struct Task
//
//--------------------------------------------------------------------------------------
Task::Task(std::string const& command)
    : command(command)
{}

Task&& Task::setCommand(std::string const& command_)
{
    this->command = std::move(command_);
    return std::move(*this);
}

Task&& Task::setInputAndOutputOption(std::string const& input_, std::string const& output_)
{
    this->inputOption = std::move(input_);
    this->outputOption = std::move(output_);
    return std::move(*this);
}

Task&& Task::setOptionPrefix(std::string&& options_)
{
    this->optionPrefix = std::move(options_);
    return std::move(*this);
}

Task&& Task::setOptionSuffix(std::string&& options_)
{
    this->optionSuffix = std::move(options_);
    return std::move(*this);
}

Task&& Task::setPreprocesses(std::vector<TaskProcess>&& processes_)
{
    this->preprocesses = std::move(processes_);
    return std::move(*this);
}

Task&& Task::setPostprocesses(std::vector<TaskProcess>&& processes_)
{
    this->postprocesses = std::move(processes_);
    return std::move(*this);
}

//--------------------------------------------------------------------------------------
//
//  struct TaskBundle
//
//--------------------------------------------------------------------------------------
TaskBundle&& TaskBundle::setCompileObj(Task&& task_)
{
    this->compileObj = std::move(task_);
    return std::move(*this);
}

TaskBundle&& TaskBundle::setlinkObjs(Task&& task_)
{
    this->linkObjs = std::move(task_);
    return std::move(*this);
}

//--------------------------------------------------------------------------------------
//
//  struct Compiler
//
//--------------------------------------------------------------------------------------
Compiler::Compiler(std::string&& name)
    : name(std::move(name))
{}

Compiler&& Compiler::setName(std::string&& name_)
{
    this->name = std::move(name_);
    return std::move(*this);
}

Compiler&& Compiler::setExe(TaskBundle&& bundle_)
{
    this->exe = std::move(bundle_);
    return std::move(*this);
}

Compiler&& Compiler::setStatic(TaskBundle&& bundle_)
{
    this->static_ = std::move(bundle_);
    return std::move(*this);
}

Compiler&& Compiler::setShared(TaskBundle&& bundle_)
{
    this->shared = std::move(bundle_);
    return std::move(*this);
}

//--------------------------------------------------------------------------------------
//
//  struct Project
//
//--------------------------------------------------------------------------------------
boost::filesystem::path Project::makeOutputFilepath()const
{
    auto path = this->rootDirectory / this->outputPath;

    switch (this->type) {
    default: [[fallthrough]];
    case Type::Exe:
        path /= this->outputName;
        break;
    case Type::Static:
        path /= "lib" + this->outputName + ".a";
        break;
    case Type::Shared:
        path /= "lib" + this->outputName + ".so." + std::to_string(this->version);
        break;
    }
    return path;
}

boost::filesystem::path Project::makeIntermediatePath()const
{
    boost::system::error_code ec;
    auto path = this->rootDirectory / this->intermediatePath / this->name;
    path = boost::filesystem::relative(path, ec);
    if (boost::system::errc::success != ec) {
        AWESOME_THROW(std::runtime_error)
            << "Failed to make intermediatePath. path=" << path;
    }
    return path;
}

boost::filesystem::path Project::makeIntermediatePath(boost::filesystem::path const& filepath)const
{
    return this->makeIntermediatePath() / filepath;
}

//--------------------------------------------------------------------------------------
//
//  utility functions
//
//--------------------------------------------------------------------------------------

TaskBundle const& getTaskBundle(Compiler const& compiler, Project::Type type)
{
    switch (type) {
    case Project::Type::Exe: return compiler.exe;
    case Project::Type::Static: return compiler.static_;
    case Project::Type::Shared: return compiler.shared;
    default:
        AWESOME_THROW(std::invalid_argument)
            << "Don't implement yet...";
    }
    return compiler.exe;
}

}