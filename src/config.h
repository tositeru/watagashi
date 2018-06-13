#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <list>
#include <unordered_set>
#include <unordered_map>

#include <boost/filesystem.hpp>
#include <boost/variant.hpp>

#include "json11/json11.hpp"
#include "errorReceiver.h"

namespace watagashi {
class SpecialVariables;
}

namespace watagashi::config
{

struct TemplateItem;

enum TemplateItemType {
	eUnknown,
	eProject,
	eBuildSetting,
	eTargetDirectory,
	eFileFilter,
	eFileToProcess,
	eCustomCompiler,
	eCompileTask,
	eCompileTaskGroup,
	eCompileProcess,
};


struct IItem
{
	inline static std::string KEY_DEFINE_TEMPLATES = "defineTemplates";
	inline static std::string KEY_DEFINE_VARIABLES = "defineVariables";
	
	std::vector<TemplateItem> defineTemplates;
	std::unordered_map<std::string, std::string> defineVariables;
	
	virtual ~IItem(){}
	bool parse(const json11::Json& data, ErrorReceiver& errorReceiver) ;
	bool dump(json11::Json& out);

	const TemplateItem* findTemplateItem(const std::string& name, TemplateItemType itemType)const;

	virtual void evalVariables(const SpecialVariables& variables)
	{}
	virtual void adaptTemplateItem(const TemplateItem& templateItem)
	{}
	
protected:
	virtual bool parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver) = 0;
	virtual bool dumpImpl(json11::Json& out) = 0;

	void copyIItemMember(const IItem& right);

private:
	bool parseDefineTemplates(const json11::Json& data, ErrorReceiver& errorReceiver);
	bool parseDefineVariables(const json11::Json& data, ErrorReceiver& errorReceiver);
	
protected:
	bool hasKey(const json11::Json& data, const std::string& key)const;
	bool parseInteger(int& out, const std::string& key, const json11::Json& data, ErrorReceiver& errorReceiver, bool enableErrorMessage=true);	
	bool parseString(std::string& out, const std::string& key, const json11::Json& data, ErrorReceiver& errorReceiver, bool enableErrorMessage=true);
	bool parseName(std::string& out, const json11::Json& data, ErrorReceiver& errorReceiver);
	bool isArray(const std::string& key, const json11::Json& data, ErrorReceiver& errorReceiver);
	bool parseStringArray(std::vector<std::string>& out, const std::string& key, const json11::Json& data, ErrorReceiver& errorReceiver, bool enableErrorMessage=true);
	bool parseOptionArray(std::vector<std::string>& out, const std::string key, const json11::Json& data, ErrorReceiver& errorReceiver);

	template<typename EnumType>
	bool parseStrToEnum(
		EnumType& out,
		const std::string key,
		std::function<EnumType(const std::string&)> converter,
		EnumType invalidType,
		const json11::Json& data,
		ErrorReceiver& errorReceiver)
	{
		auto& keyEnum = data[key];
		if(!keyEnum.is_string()) {
			errorReceiver.receive(ErrorReceiver::eError, keyEnum.rowInFile(), "\"" + key + R"(" key must be string!)");
			return false;
		}
		auto enumStr = keyEnum.string_value();
		out = converter(enumStr);
		if(invalidType == out) {
			errorReceiver.receive(ErrorReceiver::eError, keyEnum.rowInFile(), "\"" + key + R"(" is unknown type! enum=")" + enumStr + "\"");
			return false;
		}
		return true;
	}

};

struct CommonValue : public IItem
{
	std::vector<TemplateItem> defineTemplates;
	std::unordered_map<std::string, std::string> defineVariables;

	virtual void evalVariables(const SpecialVariables& variables)
	{}
	virtual void adaptTemplateItem(const TemplateItem& templateItem)
	{}
	
protected:
	virtual bool parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver){}
	virtual bool dumpImpl(json11::Json& out){}
};

struct FileToProcess : public IItem
{
	inline static std::string KEY_FILEPATH = "filepath";
	inline static std::string KEY_TEMPLATE = "template";
	inline static std::string KEY_COMPILE_OPTIONS = "compileOptions";
	inline static std::string KEY_PREPROCESS = "preprocess";
	inline static std::string KEY_POSTPROCESS = "postprocess";
	
	std::string filepath;
	std::string templateName;
	std::vector<std::string> compileOptions; 
	std::string preprocess;
	std::string postprocess;
	
	virtual void evalVariables(const SpecialVariables& variables)override;
	virtual void adaptTemplateItem(const TemplateItem& templateItem)override;

	void exePreprocess(const boost::filesystem::path& filepath)const;
	void exePostprocess(const boost::filesystem::path& filepath)const;

protected:
	virtual bool parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver) override;
	virtual bool dumpImpl(json11::Json& out)override;	
	
};

struct FileFilter : public IItem
{
	inline static const std::string KEY_TEMPLATE = "template";
	inline static const std::string KEY_EXTENSIONS = "extensions";
	inline static const std::string KEY_FILES_TO_PROCESS = "filesToProcess";

	std::string templateName;
	std::unordered_set<std::string> extensions;
	std::vector<FileToProcess> filesToProcess;
	
	virtual void evalVariables(const SpecialVariables& variables)override;
	virtual void adaptTemplateItem(const TemplateItem& templateItem)override;
	
	bool checkExtention(const boost::filesystem::path& path)const;
	const FileToProcess* findFileToProcessPointer(const boost::filesystem::path& filepath, const boost::filesystem::path& currentPath)const;

protected:
	virtual bool parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver) override;
	virtual bool dumpImpl(json11::Json& out)override;	
	
private:
	bool parseExtensions(const json11::Json& data, ErrorReceiver& errorReceiver);
	bool parseFilesToProcess(const json11::Json& data, ErrorReceiver& errorReceiver);
};

/// @brief Compile target file including directory
struct TargetDirectory : public IItem
{
	inline static const std::string KEY_PATH = "path";
	inline static const std::string KEY_TEMPLATE = "template";
	inline static const std::string KEY_FILE_FILTERS = "fileFilters";
	inline static const std::string KEY_IGNORES = "ignores";
	
	std::string path;
	std::string templateName;
	std::vector<FileFilter> fileFilters;
	std::vector<std::string> ignores;
	
	bool isIgnorePath(const boost::filesystem::path& targetPath, const boost::filesystem::path& currentPath)const;	
	bool filterPath(const boost::filesystem::path& path)const;
	const FileFilter* getFilter(const boost::filesystem::path& path)const;
	
	virtual void evalVariables(const SpecialVariables& variables)override	;
	virtual void adaptTemplateItem(const TemplateItem& templateItem)override;

protected:
	virtual bool parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver) override;
	virtual bool dumpImpl(json11::Json& out)override;	

private:
	bool parseFileFilters(const json11::Json& data, ErrorReceiver& errorReceiver);
	bool parseIgnores(const json11::Json& data, ErrorReceiver& errorReceiver);
};

/// @brief Output configuration
struct OutputConfig : public IItem
{
	inline static const std::string KEY_NAME = "name";
	inline static const std::string KEY_OUTPUT_PATH = "outputPath";
	inline static const std::string KEY_INTERMEDIATE_PATH = "intermediatePath";

	std::string name;
	std::string outputPath;
	std::string intermediatePath;

	virtual void evalVariables(const SpecialVariables& variables)override;

protected:
	virtual bool parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver) override;
	virtual bool dumpImpl(json11::Json& out)override;	

private:
	bool parseName(const json11::Json& data, ErrorReceiver& errorReceiver);
};

class Project;

/// @brief Building settings
struct BuildSetting : public IItem
{
	inline static const std::string KEY_NAME = "name";
	inline static const std::string KEY_TEMPLATE = "template";
	inline static const std::string KEY_COMPILER = "compiler";
	inline static const std::string KEY_TARGET_DIRECTORIES = "targetDirectories";
	inline static const std::string KEY_COMPILE_OPTIONS = "compileOptions";
	inline static const std::string KEY_INCLUDE_DIRECTORIES = "includeDirectories";
	inline static const std::string KEY_LINK_LIBRARIES = "linkLibraries";
	inline static const std::string KEY_LIBRARY_DIRECTORIES = "libraryDirectories";
	inline static const std::string KEY_LINK_OPTIONS = "linkOptions";
	inline static const std::string KEY_DEPENDENCES = "dependences";
	inline static const std::string KEY_OUTPUT_CONFIG = "outputConfig";
	inline static const std::string KEY_PREPROCESS = "preprocess";
	inline static const std::string KEY_LINK_PREPROCESS = "linkPreprocess";
	inline static const std::string KEY_POSTPROCESS = "postprocess";

	std::string name;
	std::string templateName;
	std::string compiler;
	std::vector<TargetDirectory> targetDirectories;
	std::vector<std::string> compileOptions;
	std::vector<std::string> includeDirectories;
	std::vector<std::string> linkLibraries;
	std::vector<std::string> libraryDirectories;
	std::vector<std::string> linkOptions;
	std::vector<std::string> dependences;
	OutputConfig outputConfig;
	std::string preprocess;
	std::string linkPreprocess;
	std::string postprocess;

	virtual void evalVariables(const SpecialVariables& variables)override;
	virtual void adaptTemplateItem(const TemplateItem& templateItem)override;
	
	void exePreprocess()const;
	void exeLinkPreprocess()const;
	void exePostprocess()const;

	boost::filesystem::path makeIntermediatePath(
		const boost::filesystem::path& configFilepath,
		const Project& project)const;
	
	boost::filesystem::path makeOutputFilepath(
		const boost::filesystem::path& configFilepath,
		const Project& project)const;

protected:
	virtual bool parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver) override;
	virtual bool dumpImpl(json11::Json& out)override;	
	
private:
	bool parseName(const json11::Json& data, ErrorReceiver& errorReceiver);
	bool parseCompiler(const json11::Json& data, ErrorReceiver& errorReceiver);
	bool parseIncludeDirectories(const json11::Json& data, ErrorReceiver& errorReceiver);
	bool parseTargetDirectories(const json11::Json& data, ErrorReceiver& errorReceiver);
	bool parseCompileOptions(const json11::Json& data, ErrorReceiver& errorReceiver);
	bool parseLinkLibraries(const json11::Json& data, ErrorReceiver& errorReceiver);	
	bool parseLibraryDirectories(const json11::Json& data, ErrorReceiver& errorReceiver);	
	bool parseLinkOptions(const json11::Json& data, ErrorReceiver& errorReceiver);
	bool parseDependences(const json11::Json& data, ErrorReceiver& errorReceiver);
	bool parseOutputConfig(const json11::Json& data, ErrorReceiver& errorReceiver);
};

/// @brief Config file root
struct Project : public IItem
{
	inline static const std::string KEY_NAME = "name";
	inline static const std::string KEY_TEMPLATE = "template";
	inline static const std::string KEY_TYPE = "type";
	inline static const std::string KEY_BUILD_SETTINGS = "buildSettings";
	inline static const std::string KEY_VERSION = "version";
	inline static const std::string KEY_MINOR_NUMBER = "minorNumber";
	inline static const std::string KEY_RELEASE_NUMBER = "releaseNumber";

	enum Type {
		eUnknown,
		eExe,
		eStaticLib,
		eSharedLib,
	};
	std::string name;
	std::string templateName;
	Type type;
	std::unordered_map<std::string, BuildSetting> buildSettings;
	int version = 0;
	int minorNumber = 0;
	int releaseNumber = 0;
	
	virtual void evalVariables(const SpecialVariables& variables)override;
	virtual void adaptTemplateItem(const TemplateItem& templateItem)override;
	
	const BuildSetting& findBuildSetting(const std::string& name)const;
	boost::filesystem::path makeIntermediatePath(
		const boost::filesystem::path& configFilepath,
		const std::string& intermediateDir,
		const std::string& buildSettingName)const;
	boost::filesystem::path makeOutputFilepath(
		const boost::filesystem::path& configFilepath,
		const std::string& outputDir,
		const std::string& buildSettingName)const;
	
	static Type toType(const std::string& typeStr);
	static std::string toStr(Type type);
	
protected:
	virtual bool parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver) override;
	virtual bool dumpImpl(json11::Json& out)override;	
	
private:
	bool parseName(const json11::Json& data, ErrorReceiver& errorReceiver);
	bool parseType(const json11::Json& data, ErrorReceiver& errorReceiver);
	bool parseBuildSettings(const json11::Json& data, ErrorReceiver& errorReceiver);
};

struct CompileProcess : public IItem
{
	inline static const std::string KEY_TYPE = "type";
	inline static const std::string KEY_CONTENT = "content";

	enum Type
	{
		eUnknown,
		eTerminal,
		eBuildIn,
	};
	
	Type type;
	std::string content;


	CompileProcess& setType(Type type){
		this->type = type;
		return *this;
	}

	CompileProcess& setContent(const std::string& content){
		this->content = content;
		return *this;
	}
	
	virtual void evalVariables(const SpecialVariables& variables)override;
	virtual void adaptTemplateItem(const TemplateItem& templateItem)override;

	static Type toType(const std::string& typeStr);
	static std::string toStr(Type type);
	
protected:
	virtual bool parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver) override;
	virtual bool dumpImpl(json11::Json& out)override;	

};

struct CompileTask : public IItem
{
	inline static const std::string KEY_COMMAND = "command";
	inline static const std::string KEY_INPUT_OPTION = "inputOption";
	inline static const std::string KEY_OUTPUT_OPTION = "outputOption";
	inline static const std::string KEY_COMMAND_PREFIX = "commandPrefix";
	inline static const std::string KEY_COMMAND_SUFFIX = "commandSuffix";
	inline static const std::string KEY_PREPROCESSES = "preprocesses";
	inline static const std::string KEY_POSTPROCESSES = "postprocesses";

	std::string command;
	std::string inputOption;
	std::string outputOption;
	std::string commandPrefix;
	std::string commandSuffix;
	std::vector<CompileProcess> preprocesses;
	std::vector<CompileProcess> postprocesses;


	CompileTask& setCommand(const std::string& command)
	{
		this->command = command;
		return *this;
	}

	CompileTask& setInputAndOutputOption(const std::string& input, const std::string& output)
	{
		this->inputOption = input;
		this->outputOption = output;
		return *this;
	}

	CompileTask& setCommandPrefix(const std::string& prefix)
	{
		this->commandPrefix = prefix;
		return *this;
	}
	
	CompileTask& setCommandSuffix(const std::string& suffix)
	{
		this->commandSuffix = suffix;
		return *this;
	}

	CompileTask& setPreprocesses(const std::vector<CompileProcess>& processes)
	{
		this->preprocesses = processes;
		return *this;
	}

	CompileTask& setPostprocesses(const std::vector<CompileProcess>& processes)
	{
		this->postprocesses = processes;
		return *this;
	}
	
	virtual void evalVariables(const SpecialVariables& variables)override;
	virtual void adaptTemplateItem(const TemplateItem& templateItem)override;
	
protected:
	virtual bool parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver) override;
	virtual bool dumpImpl(json11::Json& out)override;	

};

struct CompileTaskGroup : public IItem
{
	inline static const std::string KEY_COMPILE_OBJ = "compileObj";
	inline static const std::string KEY_LINK_OBJ = "linkObj";
	
	CompileTask compileObj;
	CompileTask linkObj;

	CompileTaskGroup& setCompileObj(const CompileTask& task)
	{
		this->compileObj = task;
		return *this;
	}
	
	CompileTaskGroup& setLinkObj(const CompileTask& task)
	{
		this->linkObj = task;
		return *this;
	}
	
	virtual void evalVariables(const SpecialVariables& variables)override;
	virtual void adaptTemplateItem(const TemplateItem& templateItem)override;

protected:
	virtual bool parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver) override;
	virtual bool dumpImpl(json11::Json& out)override;
};

struct CustomCompiler : public IItem
{
	inline static const std::string KEY_NAME = "name";
	inline static const std::string KEY_EXE = "exe";
	inline static const std::string KEY_STATIC = "static";
	inline static const std::string KEY_SHARED = "shared";
	
	std::string name;
	CompileTaskGroup exe;
	CompileTaskGroup staticLib;
	CompileTaskGroup sharedLib;
	
	CustomCompiler& setName(const std::string& name)
	{
		this->name = name;
		return *this;
	}
	
	CustomCompiler& setExe(const CompileTaskGroup& taskGroup)
	{
		this->exe = taskGroup;
		return *this;
	}

	CustomCompiler& setStaticLib(const CompileTaskGroup& taskGroup)
	{
		this->staticLib = taskGroup;
		return *this;
	}

	CustomCompiler& setSharedLib(const CompileTaskGroup& taskGroup)
	{
		this->sharedLib = taskGroup;
		return *this;
	}
	
	virtual void evalVariables(const SpecialVariables& variables)override;
	virtual void adaptTemplateItem(const TemplateItem& templateItem)override;

protected:
	virtual bool parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver) override;
	virtual bool dumpImpl(json11::Json& out)override;
};

struct RootConfig : public IItem
{
	inline static const std::string KEY_PROJECTS = "projects";
	inline static const std::string KEY_CUSTOM_COMPILERS = "customCompilers";
	
	std::unordered_map<std::string, Project> projects;
	std::unordered_map<std::string, CustomCompiler> customCompilers;
	
	const Project& findProject(const std::string& name) const;
	const CustomCompiler& findCustomCompiler(const std::string& name) const;

protected:
	virtual bool parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver) override;
	virtual bool dumpImpl(json11::Json& out)override;	

private:
	bool parseProjects(const json11::Json& data, ErrorReceiver& errorReceiver);
};

struct TemplateItem : public IItem
{
	inline static std::string KEY_ITEM_TYPE = "itemType";
	inline static std::string KEY_NAME = "name";
	
	using TemplateVariant = boost::variant<
		boost::blank,
		Project,
		BuildSetting,
		TargetDirectory,
		FileFilter,
		FileToProcess,
		CustomCompiler,
		CompileTask,
		CompileTaskGroup,
		CompileProcess
		>;
	
	TemplateItemType itemType;
	std::string name;
	TemplateVariant item;
	
	TemplateItem()=default;
	
	template<typename Item>
	TemplateItem(TemplateItemType itemType, const Item& item)
		: itemType(itemType)
		, item(item)
	{}
	
	static TemplateItemType toItemType(const std::string& typeStr);
	static std::string toStr(TemplateItemType type);

	const Project& project()const;
	const BuildSetting& buildSetting()const;
	const TargetDirectory& targetDirectory()const;
	const FileFilter& fileFilter()const;
	const FileToProcess& fileToProcess()const;
	const CustomCompiler& customCompiler()const;
	const CompileTask& compileTask()const;
	const CompileTaskGroup& compileTaskGroup()const;
	const CompileProcess& compileProcess()const;

protected:
	virtual bool parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver) override;
	virtual bool dumpImpl(json11::Json& out)override;	

private:
	bool parseItemType(const json11::Json& data, ErrorReceiver& errorReceiver);

};

/// @brief 
/// @exception std::runtime_error
void parse(RootConfig& out, const json11::Json& configJson);

}
