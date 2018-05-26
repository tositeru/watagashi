#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <boost/filesystem.hpp>

#include "programOptions.h"

namespace boost::filesystem {
	class path;
}

namespace watagasi
{
struct ProgramOptions;
class SpecialVariables;
class BuildEnviroment;
class ProcessServer;

namespace config {
	struct RootConfig;
	struct Project;
	struct BuildSetting;
	struct TargetDirectory;
	class ItemHierarchy;
}
	
class Builder
{	
public:
	void build(const std::shared_ptr<config::RootConfig>& pRootConfig, const std::shared_ptr<ProgramOptions>& pOpt);
	void clean(const std::shared_ptr<config::RootConfig>& pRootConfig, const std::shared_ptr<ProgramOptions>& pOpt);
	void listupFiles(const std::shared_ptr<config::RootConfig>& pRootConfig, const std::shared_ptr<ProgramOptions>& pOpt);
	void install(const std::shared_ptr<config::RootConfig>& pRootConfig, const std::shared_ptr<ProgramOptions>& pOpt);
	void showProjects(const std::shared_ptr<config::RootConfig>& pRootConfig, const std::shared_ptr<ProgramOptions>& pOpt);
	
private:
	enum class BuildResult {
		eSUCCESS,
		eSKIP_LINK,
		eFAILED,
	};
	BuildResult buildInDirectory(
		std::vector<std::string>& outLinkTargets,
		BuildEnviroment& env,
		ProcessServer& processServer);
		
	void linkObjs(
		BuildEnviroment& env,
		const std::vector<std::string>& linkTargets);
	
	void checkOutputPath(
		const std::shared_ptr<config::RootConfig>& pRootConfig,
		const std::shared_ptr<ProgramOptions>& pOpt);
};

}
