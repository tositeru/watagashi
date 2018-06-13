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
	std::string targetBuildSetting;
	int threadCount;
	std::string regexPattern;
	std::unordered_map<std::string, std::string> userDefinedVaraibles;
	std::string installPath;
	
	std::string currentPath;
	
	bool parse(int argv, char** args);
	
	enum TaskType {
		eTASK_UNKNOWN,
		eTASK_BUILD,
		eTASK_CLEAN,
		eTASK_REBUILD,
		eTASK_LISTUP_FILES,
		eTASK_INSTALL,
		eTASK_SHOW_PROJECTS,
		eTASK_CREATE,
	};
	
	TaskType taskType()const;
};	
}
