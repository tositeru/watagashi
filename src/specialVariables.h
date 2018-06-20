#pragma once

#include <string>
#include <regex>
#include <unordered_map>
#include <memory>

#include "config.h"

namespace watagashi
{

struct ProgramOptions;

class SpecialVariables
{
public:
	struct Scope
	{
		std::unordered_map<std::string, std::string> variables;
		
		void clear();
		
		std::string parse(
			const std::string& str,
			const std::unordered_set<std::string>& foundVarialbes)const;
	protected:
		void setUserDefinedVariables(const std::unordered_map<std::string, std::string>& useDefinedVariables);
	};

	struct CommandLineScope : public Scope
	{
		void init(const std::unordered_map<std::string, std::string>& userDefinedVariables);
	};

	struct GlobalScope : public Scope
	{
		void init(
			const config::RootConfig& rootConfig,
			const ProgramOptions& programOptions);
	};
	
	struct ProjectScope : public Scope
	{
		void init(const config::Project& project);
	};

	struct BuildSettingScope : public Scope
	{
		void init(const config::BuildSetting& buildSetting);
	};

	struct TargetDirectoryScope : public Scope
	{
		void init(const config::TargetDirectory& targetDir);
	};

	struct FileFilterScope : public Scope
	{
		void init(const config::FileFilter& filter);
	};

	struct FileToProcessScope : public Scope
	{
		void init(const config::FileToProcess& process);
	};

	struct TargetEnviromentScope : public Scope
	{
		struct InitDesc {
			boost::filesystem::path inputFilepath;
			boost::filesystem::path outputFilepath;
		};
		
		void init(const InitDesc& desc);
	};
	
	enum ScopeType
	{
		eCommandLineScope,
		eGlobalScope,
		eProjectScope,
		eBuildSettingScope,
		eTargetDirectoryScope,
		eFileFilterScope,
		eFileToProcessScope,
		eTargetEnviromentScope,
	};
	
public:
	SpecialVariables();
	~SpecialVariables();

	void clear();
	
	void clearScope(ScopeType scopeType);

	void initCommandLine(const std::unordered_map<std::string, std::string>& userDefinedVariables);
	void initGlobal(std::shared_ptr<config::RootConfig>& pRootConfig, std::shared_ptr<ProgramOptions>& pProgramOptions);
	void initProject(const config::Project& project);
	void initBuildSetting(const config::BuildSetting& buildSetting);
	void initTargetDirectory(const config::TargetDirectory& targetDir);
	void initFileFilter(const config::FileFilter& filter);
	void initFileToProcess(const config::FileToProcess& process);
	void initTargetEnviroment(const TargetEnviromentScope::InitDesc& desc);
		
	std::string parse(const std::string& str)const;
	
public:
/*	const ProjectScope& projectScope()const {
		return this->mProjectScope;
	}
*/
private:
	std::string parseReferenceVariables(const std::string& str, const std::unordered_set<std::string>& foundVariables, const config::RootConfig& rootConfig)const;
	std::string resolveReferenceVariable(const std::string& variable, const config::RootConfig& rootConfig)const;
	
private:
	std::shared_ptr<GlobalScope> mpGlobalScope;
	std::shared_ptr<ProjectScope> mpProjectScope;
	std::shared_ptr<BuildSettingScope> mpBuildSettingScope;
	std::shared_ptr<TargetDirectoryScope> mpTargetDirectoryScope;
	std::shared_ptr<FileFilterScope> mpFileFilterScope;
	std::shared_ptr<FileToProcessScope> mpFileToProcessScope;
	std::shared_ptr<TargetEnviromentScope> mpTargetEnvScope;
	std::shared_ptr<CommandLineScope> mpCommandLineScope;

	std::shared_ptr<const config::RootConfig> mpRootConfig;	
	std::shared_ptr<const ProgramOptions> mpOptions;
};

}
