#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <list>
#include <unordered_set>
#include <unordered_map>

#include <boost/filesystem.hpp>
#include <boost/variant.hpp>

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
	virtual ~IItem(){}

	virtual void evalVariables(const SpecialVariables& variables) = 0;
	virtual void adaptTemplateItem(const TemplateItem& templateItem) = 0;

	virtual const TemplateItem* findTemplateItem(const std::string& /*name*/, TemplateItemType /*itemType*/)const{
		return nullptr;
	}

};

struct UserValue : public IItem
{
	std::vector<TemplateItem> defineTemplates;
	std::unordered_map<std::string, std::string> defineVariables;

	const TemplateItem* findTemplateItem(const std::string& name, TemplateItemType itemType)const override;

	void evalVariables(const SpecialVariables& variables)override;
	void adaptTemplateItem(const TemplateItem& templateItem)override;
};

struct FileToProcess : public IItem
{
	std::string filepath;
	std::string templateName;
	std::vector<std::string> compileOptions; 
	std::string preprocess;
	std::string postprocess;
	UserValue userValue;
	
	const TemplateItem* findTemplateItem(const std::string& name, TemplateItemType itemType)const override {
		return this->userValue.findTemplateItem(name, itemType);
	}
	
	void evalVariables(const SpecialVariables& variables)override;
	void adaptTemplateItem(const TemplateItem& templateItem)override;

	void exePreprocess(const boost::filesystem::path& filepath)const;
	void exePostprocess(const boost::filesystem::path& filepath)const;
};

struct FileFilter : public IItem
{
	std::string templateName;
	std::unordered_set<std::string> extensions;
	std::vector<FileToProcess> filesToProcess;
	UserValue userValue;
	
	const TemplateItem* findTemplateItem(const std::string& name, TemplateItemType itemType)const override {
		return this->userValue.findTemplateItem(name, itemType);
	}
	
	void evalVariables(const SpecialVariables& variables)override;
	void adaptTemplateItem(const TemplateItem& templateItem)override;
	
	bool checkExtention(const boost::filesystem::path& path)const;
	const FileToProcess* findFileToProcessPointer(const boost::filesystem::path& filepath, const boost::filesystem::path& currentPath)const;
};

/// @brief Compile target file including directory
struct TargetDirectory : public IItem
{
	std::string path;
	std::string templateName;
	std::vector<FileFilter> fileFilters;
	std::vector<std::string> ignores;
	UserValue userValue;
	
	const TemplateItem* findTemplateItem(const std::string& name, TemplateItemType itemType)const override {
		return this->userValue.findTemplateItem(name, itemType);
	}
	bool isIgnorePath(const boost::filesystem::path& targetPath, const boost::filesystem::path& currentPath)const;	
	bool filterPath(const boost::filesystem::path& path)const;
	const FileFilter* getFilter(const boost::filesystem::path& path)const;
	
	void evalVariables(const SpecialVariables& variables)override;
	void adaptTemplateItem(const TemplateItem& templateItem)override;
};

/// @brief Output configuration
struct OutputConfig : public IItem
{
	std::string name;
	std::string outputPath;
	std::string intermediatePath;
	UserValue userValue;

	const TemplateItem* findTemplateItem(const std::string& name, TemplateItemType itemType)const override {
		return this->userValue.findTemplateItem(name, itemType);
	}
	void evalVariables(const SpecialVariables& variables)override;
	void adaptTemplateItem(const TemplateItem& templateItem)override;
};

struct Project;

/// @brief Building settings
struct BuildSetting : public IItem
{
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
	UserValue userValue;

	const TemplateItem* findTemplateItem(const std::string& name, TemplateItemType itemType)const override {
		return this->userValue.findTemplateItem(name, itemType);
	}
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
};

/// @brief Config file root
struct Project : public IItem
{
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
	UserValue userValue;
	
	const TemplateItem* findTemplateItem(const std::string& name, TemplateItemType itemType)const override {
		return this->userValue.findTemplateItem(name, itemType);
	}
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
};

struct CompileProcess : public IItem
{
	enum Type
	{
		eUnknown,
		eTerminal,
		eBuildIn,
	};
	
	Type type;
	std::string content;
	UserValue userValue;

	CompileProcess& setType(Type type){
		this->type = type;
		return *this;
	}

	CompileProcess& setContent(const std::string& content){
		this->content = content;
		return *this;
	}
	
	const TemplateItem* findTemplateItem(const std::string& name, TemplateItemType itemType)const override {
		return this->userValue.findTemplateItem(name, itemType);
	}
	virtual void evalVariables(const SpecialVariables& variables)override;
	virtual void adaptTemplateItem(const TemplateItem& templateItem)override;

	static Type toType(const std::string& typeStr);
	static std::string toStr(Type type);
};

struct CompileTask : public IItem
{
	std::string command;
	std::string inputOption;
	std::string outputOption;
	std::string commandPrefix;
	std::string commandSuffix;
	std::vector<CompileProcess> preprocesses;
	std::vector<CompileProcess> postprocesses;
	UserValue userValue;

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
	
	const TemplateItem* findTemplateItem(const std::string& name, TemplateItemType itemType)const override {
		return this->userValue.findTemplateItem(name, itemType);
	}
	virtual void evalVariables(const SpecialVariables& variables)override;
	virtual void adaptTemplateItem(const TemplateItem& templateItem)override;
};

struct CompileTaskGroup : public IItem
{
	CompileTask compileObj;
	CompileTask linkObj;
	UserValue userValue;

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
	
	const TemplateItem* findTemplateItem(const std::string& name, TemplateItemType itemType)const override {
		return this->userValue.findTemplateItem(name, itemType);
	}
	virtual void evalVariables(const SpecialVariables& variables)override;
	virtual void adaptTemplateItem(const TemplateItem& templateItem)override;
};

struct CustomCompiler : public IItem
{
	std::string name;
	CompileTaskGroup exe;
	CompileTaskGroup staticLib;
	CompileTaskGroup sharedLib;
	UserValue userValue;
	
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
	
	const TemplateItem* findTemplateItem(const std::string& name, TemplateItemType itemType)const override {
		return this->userValue.findTemplateItem(name, itemType);
	}
	void evalVariables(const SpecialVariables& variables)override;
	void adaptTemplateItem(const TemplateItem& templateItem)override;
};

struct RootConfig : public IItem
{
	std::unordered_map<std::string, Project> projects;
	std::unordered_map<std::string, CustomCompiler> customCompilers;
	UserValue userValue;
	
	const Project& findProject(const std::string& name) const;
	const CustomCompiler& findCustomCompiler(const std::string& name) const;

	const TemplateItem* findTemplateItem(const std::string& name, TemplateItemType itemType)const override {
		return this->userValue.findTemplateItem(name, itemType);
	}
	void evalVariables(const SpecialVariables& variables)override;
	void adaptTemplateItem(const TemplateItem& templateItem)override;
};

struct TemplateItem : public IItem
{
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

	void evalVariables(const SpecialVariables& variables)override;
	void adaptTemplateItem(const TemplateItem& templateItem)override;
	
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

};

}
