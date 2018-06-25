#pragma once

#include <string>
#include <vector>
#include <unordered_set>

#include <boost/filesystem.hpp>

#include "utility.h"

namespace watagashi::data
{

struct TaskProcess
{
    enum class Type {
        Terminal,
        BuildIn,
    };

    enum class Result {
        Success,
        Skip,
        Failed,
    };
    struct RunData
    {
        boost::filesystem::path inputFilepath;
        boost::filesystem::path outputFilepath;
        std::unordered_set<boost::filesystem::path> includeDirectories;
    };

    Type type;
    std::string content;

    Result run(RunData const& data)const;

    TaskProcess() = default;
    TaskProcess(Type type, std::string&& content);
    TaskProcess&& setTypeAndContent(Type type, std::string&& content);
};

struct Task
{
    std::string command;
    std::string inputOption;
    std::string outputOption;
    std::string optionPrefix;
    std::string optionSuffix;

    std::vector<TaskProcess> preprocesses;
    std::vector<TaskProcess> postprocesses;

    Task() = default;
    Task(std::string const& command);
    Task&& setCommand(std::string const& command);
    Task&& setInputAndOutputOption(std::string const& input, std::string const& output);
    Task&& setOptionPrefix(std::string&& options);
    Task&& setOptionSuffix(std::string&& options);
    Task&& setPreprocesses(std::vector<TaskProcess>&& processes);
    Task&& setPostprocesses(std::vector<TaskProcess>&& processes);

};

struct TaskBundle
{
    Task compileObj;
    Task linkObjs;

    TaskBundle() = default;
    TaskBundle&& setCompileObj(Task&& task);
    TaskBundle&& setlinkObjs(Task&& task);
};

struct Compiler
{
    std::string name;

    TaskBundle exe;
    TaskBundle static_;
    TaskBundle shared;

    Compiler() = default;
    Compiler(std::string&& name);
    Compiler&& setName(std::string&& name);
    Compiler&& setExe(TaskBundle&& bundle);
    Compiler&& setStatic(TaskBundle&& bundle);
    Compiler&& setShared(TaskBundle&& bundle);
};

struct FileFilter
{
    std::string targetKeyward;
    std::vector<TaskProcess> preprocess;
    std::vector<TaskProcess> postprocess;

    // for compile
    std::vector<std::string> compileOptions;
    std::vector<boost::filesystem::path> includeDirectories;

};

struct Project
{
    enum class Type {
        Exe,
        Static,
        Shared,
    };

    std::string name;
    Type type;
    int version = 0;
    int minorNumber = 0;
    int releaseNumber = 0;
    std::string compiler;
    std::string outputName;
    boost::filesystem::path outputPath;
    boost::filesystem::path intermediatePath;
    //std::vector<std::string> dependences; //外部で処理する
    boost::filesystem::path rootDirectory;

    std::unordered_set<boost::filesystem::path> targets;
    std::vector<FileFilter> fileFilters;

    // for compile
    std::unordered_set<std::string> compileOptions;
    std::unordered_set<boost::filesystem::path> includeDirectories;

    // for link 
    std::unordered_set<std::string> linkOptions;
    std::unordered_set<std::string> linkLibraries;
    std::unordered_set<boost::filesystem::path> libraryDirectories;

    // process
    std::string preprocess;
    std::string linkPreprocess;
    std::string postprocess;


public:
    boost::filesystem::path makeOutputFilepath()const;

    boost::filesystem::path makeIntermediatePath()const;
    boost::filesystem::path makeIntermediatePath(boost::filesystem::path const& filepath)const;
};

TaskProcess::Result runProcesses(std::vector<TaskProcess> const& processes, TaskProcess::RunData const& data);

std::string makeCompileCommand(
    Task const& task,
    boost::filesystem::path const& inputFilepath,
    boost::filesystem::path const& outputFilepath,
    std::unordered_set<std::string> const& options,
    std::unordered_set<boost::filesystem::path> const& includeDirectories);

std::string makeCompileCommand(
    Task const& task,
    boost::filesystem::path const& inputFilepath,
    boost::filesystem::path const& outputFilepath,
    Project const& project);

std::string makeLinkCommand(
    Task const& task,
    boost::filesystem::path const& outputFilepath,
    std::vector<boost::filesystem::path> const& targets,
    std::unordered_set<std::string> const& options,
    std::unordered_set<std::string> const& libraries,
    std::unordered_set<boost::filesystem::path> const& libraryDirectories);

std::string makeLinkCommand(
    Task const& task,
    boost::filesystem::path const& outputFilepath,
    std::vector<boost::filesystem::path> const& targets,
    Project const& project);

TaskBundle const& getTaskBundle(Compiler const& compiler, Project::Type type);

}