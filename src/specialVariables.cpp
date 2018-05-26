#include "specialVariables.h"

#include <iostream>
#include <regex>
#include <boost/filesystem.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include "config.h"
#include "programOptions.h"
#include "utility.h"

using namespace std;
namespace fs = boost::filesystem;

#ifdef USE_CLANG_LIBTOOLING

#ifndef BUILD_IN_CLANG_PATH
#define BUILD_IN_CLANG_PATH "/usr/local/lib/clang/7.0.0/"
#endif

#endif

namespace watagasi
{

//========================================================================
//
//	struct Scope
//
//========================================================================

void SpecialVariables::Scope::clear()
{
	this->variables.clear();
}

std::string SpecialVariables::Scope::parse(
	const std::string& str,
	const std::unordered_set<std::string>& foundVarialbes)const
{
	auto result = str;
	boost::range::for_each(
		foundVarialbes
		| boost::adaptors::filtered([&](const std::string& var){
			return 1 <= this->variables.count(var);
		}),
		[&](const std::string& foundVariables){
			auto& value = this->variables.find(foundVariables)->second;
			auto keyward = "${" + foundVariables + "}";
			std::string::size_type pos = 0;
			while(std::string::npos != (pos = result.find(keyward, pos))) {
				result.replace(pos, keyward.size(), value);
				pos += keyward.size();
			}
		});
	return result;
}

void SpecialVariables::Scope::setUserDefinedVariables(const config::IItem* pItem)
{
	this->setUserDefinedVariables(pItem->defineVariables);
}

void SpecialVariables::Scope::setUserDefinedVariables(const std::unordered_map<std::string, std::string>& useDefinedVariables)
{
	boost::range::copy(
		useDefinedVariables
		, std::inserter(this->variables, this->variables.end()));
}

//========================================================================
//
//	struct CommandLineScope
//
//========================================================================

void SpecialVariables::CommandLineScope::init(const std::unordered_map<std::string, std::string>& userDefinedVariables)
{
	this->variables.clear();
	this->setUserDefinedVariables(userDefinedVariables);
}

//========================================================================
//
//	struct GlobalScope
//
//========================================================================

void SpecialVariables::GlobalScope::init(
	const config::RootConfig& rootConfig,
	const ProgramOptions& programOptions)
{
	this->variables.clear();
	this->setUserDefinedVariables(&rootConfig);
	
	this->variables.insert({"ROOT_PATH", fs::absolute(fs::path(programOptions.configFilepath)).parent_path().string() + "/"});
}

//========================================================================
//
//	struct ProjectScope
//
//========================================================================

void SpecialVariables::ProjectScope::init(const config::Project& project)
{
	this->variables.clear();
	this->setUserDefinedVariables(&project);
	this->variables.insert({"PROJECT_NAME", project.name});
	this->variables.insert({"PROJECT_TYPE", config::Project::toStr(project.type)});
 	
#ifdef USE_CLANG_LIBTOOLING
	auto pClangPath = std::getenv("CLANG_PATH");
	this->variables.insert({"CLANG_PATH", nullptr == pClangPath ? BUILD_IN_CLANG_PATH : pClangPath });
	if(pClangPath == nullptr) {
		cout << "use build in clang path=" << BUILD_IN_CLANG_PATH << endl;
	}
#endif
}

//========================================================================
//
//	struct BuildSettingScope
//
//========================================================================

void SpecialVariables::BuildSettingScope::init(const config::BuildSetting& buildSetting)
{
	this->variables.clear();
	this->setUserDefinedVariables(&buildSetting);

	this->variables.insert({"BUILD_SETTING_NAME", buildSetting.name});
	this->variables.insert({"COMPILER", buildSetting.compiler});

	auto& outputConfig = buildSetting.outputConfig;
	this->variables.insert({"OUTPUT_NAME", outputConfig.name});
	this->variables.insert({"OUTPUT_PATH", outputConfig.outputPath});
	this->variables.insert({"INTERMEDIATE_PATH", outputConfig.intermediatePath});
}

//========================================================================
//
//	struct TargetDirectoryScope
//
//========================================================================

void SpecialVariables::TargetDirectoryScope::init(const config::TargetDirectory& targetDir)
{
	this->variables.clear();
	this->setUserDefinedVariables(&targetDir);

	this->variables.insert({"TARGET_DIRECTORY", targetDir.path});
}

//========================================================================
//
//	struct FileFilterScope
//
//========================================================================

void SpecialVariables::FileFilterScope::init(
	const config::FileFilter& filter)
{
	this->variables.clear();
	this->setUserDefinedVariables(&filter);
}

//========================================================================
//
//	struct FileToProcessScope
//
//========================================================================

void SpecialVariables::FileToProcessScope::init(
	const config::FileToProcess& process)
{
	this->variables.clear();
	this->setUserDefinedVariables(&process);
}

//========================================================================
//
//	struct TargetEnviromentScope
//
//========================================================================

void SpecialVariables::TargetEnviromentScope::init(
	const TargetEnviromentScope::InitDesc& desc)
{
	this->variables.clear();

	this->variables.insert({"INPUT_FILE_PATH", desc.inputFilepath.string()});
	this->variables.insert({"INPUT_FILE_NAME", desc.inputFilepath.filename().string()});
	this->variables.insert({"INPUT_FILE_STEM", desc.inputFilepath.stem().string()});
	this->variables.insert({"INPUT_FILE_DIR",  desc.inputFilepath.parent_path().string()});

	this->variables.insert({"OUTPUT_FILE_PATH", desc.outputFilepath.string()});
	this->variables.insert({"OUTPUT_FILE_NAME", desc.outputFilepath.filename().string()});
	this->variables.insert({"OUTPUT_FILE_STEM", desc.outputFilepath.stem().string()});
	this->variables.insert({"OUTPUT_FILE_DIR",  desc.outputFilepath.parent_path().string()});
}

//========================================================================
//
//	class SpecialVariables
//
//========================================================================

SpecialVariables::SpecialVariables()
	: mpRootConfig(nullptr)
	, mpOptions(nullptr)
{}

SpecialVariables::~SpecialVariables()
{}

void SpecialVariables::clear()
{
	this->mpGlobalScope.reset();
	this->mpProjectScope.reset();
	this->mpBuildSettingScope.reset();
	this->mpTargetDirectoryScope.reset();
	this->mpFileFilterScope.reset();
	this->mpFileToProcessScope.reset();
	this->mpCommandLineScope.reset();

	this->mpRootConfig = nullptr;
	this->mpOptions = nullptr;
}

void SpecialVariables::clearScope(ScopeType scopeType)
{
	switch(scopeType) {
	case eGlobalScope:
		this->mpGlobalScope.reset();
		[[fallthrough]];
	case eProjectScope:
		this->mpProjectScope.reset();
		[[fallthrough]];
	case eBuildSettingScope:
		this->mpBuildSettingScope.reset();
		[[fallthrough]];
	case eTargetDirectoryScope:
		this->mpTargetDirectoryScope.reset();
		[[fallthrough]];
	case eFileFilterScope:
		this->mpFileFilterScope.reset();
		[[fallthrough]];
	case eFileToProcessScope:
		this->mpFileToProcessScope.reset();
		[[fallthrough]];
	case eTargetEnviromentScope:
		this->mpTargetEnvScope.reset();
		break;
	case eCommandLineScope:
		this->mpCommandLineScope.reset();
		break;
	default:
		break;
	}
}

void SpecialVariables::initCommandLine(const std::unordered_map<std::string, std::string>& userDefinedVariables)
{
	this->mpCommandLineScope = std::make_shared<decltype(this->mpCommandLineScope)::element_type>();
	this->mpCommandLineScope->init(userDefinedVariables);
}

void SpecialVariables::initGlobal(
	std::shared_ptr<config::RootConfig>& pRootConfig,
	std::shared_ptr<ProgramOptions>& pProgramOptions)
{
	if(nullptr == pRootConfig || nullptr == pProgramOptions)
	{
		throw std::invalid_argument("SpecialVariables::initGlobal(): pRootConfig or pProgramOptions is nullptr...");
	}
	
	this->mpGlobalScope = std::make_shared<decltype(this->mpGlobalScope)::element_type>();
	this->mpGlobalScope->init(*pRootConfig, *pProgramOptions);

	this->mpRootConfig = pRootConfig;
	this->mpOptions = pProgramOptions;
}

void SpecialVariables::initProject(const config::Project& project)
{
	this->mpProjectScope = std::make_shared<decltype(this->mpProjectScope)::element_type>();
	this->mpProjectScope->init(project);
}

void SpecialVariables::initBuildSetting(const config::BuildSetting& buildSetting)
{
	this->mpBuildSettingScope = std::make_shared<decltype(this->mpBuildSettingScope)::element_type>();
	this->mpBuildSettingScope->init(buildSetting);
}

void SpecialVariables::initTargetDirectory(const config::TargetDirectory& targetDir)
{
	this->mpTargetDirectoryScope = std::make_shared<decltype(this->mpTargetDirectoryScope)::element_type>();
	this->mpTargetDirectoryScope->init(targetDir);
}

void SpecialVariables::initFileFilter(const config::FileFilter& filter)
{
	this->mpFileFilterScope = std::make_shared<decltype(this->mpFileFilterScope)::element_type>();
	this->mpFileFilterScope->init(filter);
}

void SpecialVariables::initFileToProcess(
	const config::FileToProcess& process)
{
	this->mpFileToProcessScope = std::make_shared<decltype(this->mpFileToProcessScope)::element_type>();
	this->mpFileToProcessScope->init(process);
}

void SpecialVariables::initTargetEnviroment(
	const TargetEnviromentScope::InitDesc& desc)
{
	this->mpTargetEnvScope = std::make_shared<decltype(this->mpTargetEnvScope)::element_type>();
	this->mpTargetEnvScope->init(desc);
}

std::string SpecialVariables::parse(
	const std::string& str)const
{
	auto findVariables = [](const std::string& str){
		std::unordered_set<std::string> result;
		if(std::string::npos == str.find('$')) {
			return result;
		}
		std::regex pattern(R"(\$\{([a-zA-Z0-9_.]+)\})");
		std::smatch match;
		auto it = str.cbegin(), end = str.cend();
		while(std::regex_search(it, end, match, pattern)) {
			result.insert(match[1]);
			it = match[0].second;
		}
		return result;
	};
	auto foundVariables = findVariables(str);
	if(foundVariables.empty()) {
		return str;
	}
	std::string result = str;
	if(this->mpCommandLineScope) {
		result = this->mpCommandLineScope->parse(result, foundVariables);
	}
	if(this->mpTargetEnvScope) {
		result = this->mpTargetEnvScope->parse(result, foundVariables);
	}
	if(this->mpFileToProcessScope) {
		result = this->mpFileToProcessScope->parse(result, foundVariables);
	}
	if(this->mpTargetDirectoryScope) {
		result = this->mpTargetDirectoryScope->parse(result, foundVariables);
	}
	if(this->mpBuildSettingScope) {
		result = this->mpBuildSettingScope->parse(result, foundVariables);
	}
	if(this->mpProjectScope) {
		result = this->mpProjectScope->parse(result, foundVariables);
	}
	if(this->mpGlobalScope) {
		result = this->mpGlobalScope->parse(result, foundVariables);
	}
	
	if(this->mpOptions && this->mpRootConfig) {
		foundVariables = findVariables(result);
		if(!foundVariables.empty()) {
			result = this->parseReferenceVariables(result, foundVariables, *this->mpRootConfig);
		}
	}
	return result;
}

std::string SpecialVariables::resolveReferenceVariable(
	const std::string& variable,
	const config::RootConfig& rootConfig)const
{
	auto splitVariable = split(variable, '.');
	try {
		if(splitVariable.size() == 2) {
			auto project = rootConfig.findProject(splitVariable[0]);
			using referenceFunc = std::function<std::string(const config::Project&)>;
			static const std::unordered_map<std::string, referenceFunc> sTable = {
				{"name", [](const config::Project& project){ return project.name; } },
				{"type", [](const config::Project& project){ return project.toStr(project.type); } },
			};
			auto it = sTable.find(splitVariable[1]);
			if(sTable.end() == it) {
				throw "\"" + splitVariable[1] + "\" is unknwon keyward..."s;
			}
			return it->second(project);
		} else if(splitVariable.size() == 3) {
			auto project = rootConfig.findProject(splitVariable[0]);
			auto buildSetting = project.findBuildSetting(splitVariable[1]);
			using referenceFunc = std::function<
				std::string(
					const ProgramOptions& options,
					const config::Project&,
					const config::BuildSetting&)>;
			static const std::unordered_map<std::string, referenceFunc> sTable = {
				{"name", [](const ProgramOptions& options, const config::Project& project, const config::BuildSetting& setting){ return setting.name; } },
				{"outputFilepath", [](const ProgramOptions& options, const config::Project& project, const config::BuildSetting& buildSetting){ return buildSetting.makeOutputFilepath(fs::path(options.configFilepath).parent_path().string(), project).string(); } },
				{"outputFileDir", [](const ProgramOptions& options, const config::Project& project, const config::BuildSetting& buildSetting){ return buildSetting.makeOutputFilepath(fs::path(options.configFilepath).parent_path().string(), project).parent_path().string() + "/"; } },
			};
			auto it = sTable.find(splitVariable[2]);
			if(sTable.end() == it) {
				throw "\"" + splitVariable[2] + "\" is unknwon keyward..."s;
			}
			return it->second(*this->mpOptions, project, buildSetting);
		} else {
			throw "syntex error in reference variable. var={" + variable + "}";
		}
	} catch(const std::string& message) {
		throw std::runtime_error(message);
	}
}

std::string SpecialVariables::parseReferenceVariables(
	const std::string& str,
	const std::unordered_set<std::string>& foundVariables,
	const config::RootConfig& rootConfig)const
{
	auto result = str;
	boost::range::for_each(
		foundVariables,
		[&](const std::string& foundVariable){
			auto value = this->resolveReferenceVariable(foundVariable, rootConfig);
			
			auto keyward = "${" + foundVariable + "}";
			std::string::size_type pos = 0;
			while(std::string::npos != (pos = result.find(keyward, pos))) {
				result.replace(pos, keyward.size(), value);
				pos += keyward.size();
			}
		});
	return result;
}

}
