#pragma once

#include <string>
#include <unordered_map>

namespace watagashi
{ 
struct ProgramOptions
{
    std::string task;
    std::string configFilepath;
    std::string targetProject;
    int threadCount;
    std::unordered_map<std::string, std::string> userDefinedVaraibles;
    std::string installPath;
    
    std::string rootDirectories;
    
    bool parse(int argv, char** args);
    
    enum class TaskType {
        Unknown,
        Build,
        Clean,
        Rebuild,
        ListupFiles,
        Install,
        ShowProjects,
        Interactive,
    };
    
    TaskType taskType()const;
};    
}
