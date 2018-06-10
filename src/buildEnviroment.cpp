#include "buildEnviroment.h"

#include <list>
#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/adaptor/filtered.hpp>

#include "utility.h"
#include "includeFileAnalyzer.h"
#include "exception.hpp"

using namespace std;
namespace fs = boost::filesystem;

namespace watagasi
{

const std::unordered_map<std::string, config::CustomCompiler>
	BuildEnviroment::sBuildInCustomCompilers =
{
	{
		"clang++",
		config::CustomCompiler()
			.setName("clang++")
			.setExe( config::CompileTaskGroup()
				.setCompileObj( config::CompileTask()
					.setCommand("clang++")
					.setInputAndOutputOption("", "-o")
					.setCommandPrefix("-c")
					.setPreprocesses( {
						config::CompileProcess()
							.setType(config::CompileProcess::eBuildIn)
							.setContent("checkUpdate")
					})
				)
				.setLinkObj( config::CompileTask()
					.setCommand("clang++")
					.setInputAndOutputOption("", "-o")
				)
			)
			.setStaticLib( config::CompileTaskGroup()
				.setCompileObj( config::CompileTask()
					.setCommand("clang++")
					.setInputAndOutputOption("", "-o")
					.setCommandPrefix("-c")
					.setPreprocesses( {
						config::CompileProcess()
							.setType(config::CompileProcess::eBuildIn)
							.setContent("checkUpdate")
					})
				)
				.setLinkObj( config::CompileTask()
					.setCommand("ar")
					.setInputAndOutputOption("", "-o")
					.setCommandPrefix("rcs")
				)
			)
			.setSharedLib( config::CompileTaskGroup()
				.setCompileObj( config::CompileTask()
					.setCommand("clang++")
					.setInputAndOutputOption("", "-o")
					.setCommandPrefix("-c -fPIC")
					.setPreprocesses( {
						config::CompileProcess()
							.setType(config::CompileProcess::eBuildIn)
							.setContent("checkUpdate")
					})
				)
				.setLinkObj( config::CompileTask()
					.setCommand("clang++")
					.setInputAndOutputOption("", "-o")
					.setCommandPrefix("-shared -Wl,-soname,${OUTPUT_FILE_NAME}")
					.setPostprocesses( {
						config::CompileProcess()
							.setType(config::CompileProcess::eTerminal)
							.setContent("ldconfig -n ${OUTPUT_FILE_DIR}")
					})
				)
			)
	},
	{
		"g++",
		config::CustomCompiler()
			.setName("g++")
			.setExe( config::CompileTaskGroup()
				.setCompileObj( config::CompileTask()
					.setCommand("g++")
					.setInputAndOutputOption("", "-o")
					.setCommandPrefix("-c")
					.setPreprocesses( {
						config::CompileProcess()
							.setType(config::CompileProcess::eBuildIn)
							.setContent("checkUpdate")
					})
				)
				.setLinkObj( config::CompileTask()
					.setCommand("g++")
					.setInputAndOutputOption("", "-o")
				)
			)
			.setStaticLib( config::CompileTaskGroup()
				.setCompileObj( config::CompileTask()
					.setCommand("g++")
					.setInputAndOutputOption("", "-o")
					.setCommandPrefix("-c")
					.setPreprocesses( {
						config::CompileProcess()
							.setType(config::CompileProcess::eBuildIn)
							.setContent("checkUpdate")
					})
				)
				.setLinkObj( config::CompileTask()
					.setCommand("ar")
					.setInputAndOutputOption("", "-o")
					.setCommandPrefix("rcs")
				)
			)
			.setSharedLib( config::CompileTaskGroup()
				.setCompileObj( config::CompileTask()
					.setCommand("g++")
					.setInputAndOutputOption("", "-o")
					.setCommandPrefix("-c -fPIC")
					.setPreprocesses( {
						config::CompileProcess()
							.setType(config::CompileProcess::eBuildIn)
							.setContent("checkUpdate")
					})
				)
				.setLinkObj( config::CompileTask()
					.setCommand("g++")
					.setInputAndOutputOption("", "-o")
					.setCommandPrefix("-shared -Wl,-soname,${OUTPUT_FILE_NAME}")
					.setPostprocesses( {
						config::CompileProcess()
							.setType(config::CompileProcess::eTerminal)
							.setContent("ldconfig -n ${OUTPUT_FILE_DIR}")
					})
				)
			)
	}
};


BuildEnviroment::BuildEnviroment()
{
}

BuildEnviroment::~BuildEnviroment()
{
}

void BuildEnviroment::clear()
{
	this->mpItemHierarchy.clear();
	this->mVariables.clear();
	this->mpOpts.reset();
	this->mpRootConfig.reset();
	this->mpProject.reset();
	this->mpBuildSetting.reset();
	this->mpTargetDir.reset();
	this->mpFileFilter.reset();
	this->mpFileToProcess.reset();
}

void BuildEnviroment::init(
	const std::shared_ptr<config::RootConfig> pRootConfig,
	const std::shared_ptr<ProgramOptions> pOptions)
{
	this->clear();
	
	this->mpRootConfig = pRootConfig;
	this->mpOpts = pOptions;
	
	{
		this->mVariables.initCommandLine(
			this->mpOpts->userDefinedVaraibles);
		this->mVariables.initGlobal(
			this->mpRootConfig, this->mpOpts);
		this->mpItemHierarchy.push_back(this->mpRootConfig.get());
	}

	this->mpProject = std::make_shared<decltype(this->mpProject)::element_type>(
		this->mpRootConfig->findProject(this->mpOpts->targetProject));
	if(!this->setupIItem( this->mpProject.get(), this->mpProject->templateName, config::TemplateItemType::eProject,
		[&](){ this->mVariables.initProject(*this->mpProject); }) )
	{
		AWESOME_THROW(std::runtime_error) << "failed to adapt template for project.";
	}

	this->mpBuildSetting = std::make_shared<decltype(this->mpBuildSetting)::element_type>(
		this->mpProject->findBuildSetting(this->mpOpts->targetBuildSetting));
	if(!this->setupIItem( this->mpBuildSetting.get(), this->mpBuildSetting->templateName, config::TemplateItemType::eBuildSetting,
		[&](){ this->mVariables.initBuildSetting(*this->mpBuildSetting); }) )
	{
		AWESOME_THROW(std::runtime_error) << "failed to adapt template for buildSetting.";
	}
	
	for(auto&& dir : this->mpBuildSetting->includeDirectories) {
		auto path = fs::path(dir);
		if(path.is_relative()) {
			dir = (this->configFileDirectory() / path).string();
		}
	}
	for(auto&& dir : this->mpBuildSetting->libraryDirectories) {
		auto path = fs::path(dir);
		if(path.is_relative()) {
			dir = (this->configFileDirectory() / dir).string();
		}
	}
}

void BuildEnviroment::clearTargetDirectory()
{
	this->clearFileFilter();
	
	while(static_cast<size_t>(HIERARCHY::eTargetDirectory) < this->mpItemHierarchy.size()) {
		this->mpItemHierarchy.pop_back();
	}
	this->mVariables.clearScope(SpecialVariables::eTargetDirectoryScope);
	this->mpTargetDir.reset();
}

void BuildEnviroment::clearFileFilter()
{
	this->clearFileToProcess();
	
	while(static_cast<size_t>(HIERARCHY::eFileFilter) < this->mpItemHierarchy.size()) {
		this->mpItemHierarchy.pop_back();
	}
	this->mVariables.clearScope(SpecialVariables::eFileFilterScope);
	this->mpFileFilter.reset();
}

void BuildEnviroment::clearFileToProcess()
{
	this->clearTargetEnviroment();
	
	while(static_cast<size_t>(HIERARCHY::eFileToProcess) < this->mpItemHierarchy.size()) {
		this->mpItemHierarchy.pop_back();
	}
	this->mVariables.clearScope(SpecialVariables::eFileToProcessScope);
	this->mpFileToProcess.reset();
}

void BuildEnviroment::clearTargetEnviroment()
{
	this->mVariables.clearScope(SpecialVariables::eTargetEnviromentScope);	
}

bool BuildEnviroment::setupIItem(
	config::IItem* pInOut,
	const std::string& templateName,
	config::TemplateItemType itemType,
	std::function<void()> variableInitializer,
	bool isPushHierarchy)
{
	if(!this->adaptTemplateItem(pInOut, templateName, itemType)) {
		return false;
	}
	
	variableInitializer();
	pInOut->evalVariables(this->mVariables);
	if(isPushHierarchy) {
		this->mpItemHierarchy.push_back(pInOut);
	}
	return true;
}

void BuildEnviroment::setupTargetDirectory(
	const config::TargetDirectory& targetDir)
{
	if(static_cast<size_t>(HIERARCHY::eTargetDirectory) != this->mpItemHierarchy.size()) {
		AWESOME_THROW(std::runtime_error) << "invalid item hierarchy...";
	}

	this->mpTargetDir = std::make_shared<
		decltype(this->mpTargetDir)::element_type>(targetDir);
	if(!this->setupIItem( this->mpTargetDir.get(), this->mpTargetDir->templateName, config::TemplateItemType::eTargetDirectory,
		[&](){ this->mVariables.initTargetDirectory(*this->mpTargetDir); }) )
	{
		AWESOME_THROW(std::runtime_error) << "failed to adapt template for targetDirectory.";
	}
	
	for(auto&& filter : this->mpTargetDir->fileFilters) {
		if(!this->setupIItem( &filter, filter.templateName, config::TemplateItemType::eFileFilter,
			[](){}, false) )
		{
			AWESOME_THROW(std::runtime_error) << "failed to adapt template for fileFilter.";
		}
	}
}

void BuildEnviroment::setupFileFilter(const config::FileFilter& fileFilter)
{
	if(static_cast<size_t>(HIERARCHY::eFileFilter) != this->mpItemHierarchy.size()) {
		AWESOME_THROW(std::runtime_error) << "BuildEnviroment::setupFileFilter(): invalid item hierarchy...";
	}

	this->mpFileFilter = std::make_shared<
		decltype(this->mpFileFilter)::element_type>(fileFilter);
	if(!this->setupIItem( this->mpFileFilter.get(), this->mpFileFilter->templateName, config::TemplateItemType::eFileFilter,
		[&](){ this->mVariables.initFileFilter(*this->mpFileFilter); }) )
	{
		AWESOME_THROW(std::runtime_error) << "failed to adapt template for fileFilter.";
	}
}

void BuildEnviroment::setupFileToProcess(
	const config::FileToProcess& fileToProcess)
{
	if(static_cast<size_t>(HIERARCHY::eFileToProcess) != this->mpItemHierarchy.size()) {
		AWESOME_THROW(std::runtime_error) << "BuildEnviroment::setupFileToProcess(): invalid item hierarchy...";
	}
	
	this->mpFileToProcess = std::make_shared<
		decltype(this->mpFileToProcess)::element_type>(fileToProcess);
	if(!this->setupIItem( this->mpFileToProcess.get(), this->mpFileToProcess->templateName, config::TemplateItemType::eFileToProcess,
		[&](){ this->mVariables.initFileToProcess(*this->mpFileToProcess); }) )
	{
		AWESOME_THROW(std::runtime_error) << "failed to adapt template for fileToProcess.";
	}
}

void BuildEnviroment::setupTargetEnviroment(
	const SpecialVariables::TargetEnviromentScope::InitDesc& desc)
{
	this->mVariables.initTargetEnviroment(desc);
}

bool BuildEnviroment::adaptTemplateItem(
	config::IItem* pInOut,
	const std::string& templateName,
	config::TemplateItemType itemType)const
{
	if(templateName.empty()) {
		return true;
	}
	auto pTemplateItem = this->findTemplateItem(templateName, itemType);
	if(nullptr == pTemplateItem) {
		cerr << "!!! ERROR !!! Not found template. name=" << templateName << endl;
		return false;
	}
	pInOut->adaptTemplateItem(*pTemplateItem);
	return true;
}

const config::TemplateItem* BuildEnviroment::findTemplateItem(
	const std::string& name,
	config::TemplateItemType itemType)const
{
	std::list<const config::TemplateItem*> items;
	boost::range::copy(
		this->mpItemHierarchy
		| boost::adaptors::reversed
		| boost::adaptors::transformed([&](const config::IItem* pItem) {
			return pItem->findTemplateItem(name, itemType);
		})
		| boost::adaptors::filtered([](const config::TemplateItem* pTemplateItem){
			return nullptr != pTemplateItem;
		})
		, std::back_inserter(items));
	
	if(items.empty()) {
		return nullptr;
	} else {
		return *items.begin();
	}
}

bool BuildEnviroment::preprocessCompiler(
	TaskType taskType,
	const boost::filesystem::path& inputFilepath,
	const boost::filesystem::path& outputFilepath)const
{
	if(!this->isExistItem(HIERARCHY::eBuildSetting)) {
		AWESOME_THROW(std::runtime_error) << "BuildEnviroment::preprocessCompiler(): not initialize...";
	}
	
	auto& compileTaskGroup = this->findCompileTaskGroup(this->mpProject->type, this->mpBuildSetting->compiler);
	const config::CompileTask* pCompileTask = nullptr;
	switch(taskType) {
	case TaskType::eCompile: 	pCompileTask = &compileTaskGroup.compileObj; break;
	case TaskType::eLink: 		pCompileTask = &compileTaskGroup.linkObj; break;
	}
	return this->processCompiler(pCompileTask->preprocesses, inputFilepath, outputFilepath);
}

bool BuildEnviroment::postprocessCompiler(
	TaskType taskType,
	const boost::filesystem::path& inputFilepath,
	const boost::filesystem::path& outputFilepath)const
{
	if(!this->isExistItem(HIERARCHY::eBuildSetting)) {
		AWESOME_THROW(std::runtime_error) << "BuildEnviroment::postprocessCompiler(): not initialize...";
	}
	
	auto& compileTaskGroup = this->findCompileTaskGroup(this->mpProject->type, this->mpBuildSetting->compiler);
	const config::CompileTask* pCompileTask = nullptr;
	switch(taskType) {
	case TaskType::eCompile: 	pCompileTask = &compileTaskGroup.compileObj; break;
	case TaskType::eLink: 		pCompileTask = &compileTaskGroup.linkObj; break;
	}
	return this->processCompiler(pCompileTask->postprocesses, inputFilepath, outputFilepath);
}

bool BuildEnviroment::processCompiler(
	const std::vector<config::CompileProcess>& processes,
	const boost::filesystem::path& inputFilepath,
	const boost::filesystem::path& outputFilepath)const
{
	bool isOK = true;	
	boost::range::for_each(
		processes,
		[&](const config::CompileProcess& process) {
			auto content = this->mVariables.parse(process.content);
			switch(process.type) {
			case config::CompileProcess::eTerminal: {
					cout << content << endl;
					int ret = std::system(content.c_str());
					isOK &= (ret == 0);
					break;
				}
			case config::CompileProcess::eBuildIn:
				isOK &= runBuildInProcess(content, inputFilepath, outputFilepath);
				break;
			default:
				std::runtime_error("BuildEnviroment::preprocessCompiler(): invalid CompileProcess::Type...");
				break;
			}
		});
	return isOK;
}

bool BuildEnviroment::runBuildInProcess(
	const std::string& content,
	const boost::filesystem::path& inputFilepath,
	const boost::filesystem::path& outputFilepath)const
{
	if("checkUpdate" == content) {
		return IncludeFileAnalyzer::sCheckUpdateTime(inputFilepath, outputFilepath, this->mpBuildSetting->includeDirectories);
		//return IncludeFileAnalyzer::sCheckUpdateTime(inputFilepath, outputFilepath, this->mBuildSetting.compileOptions);
	}
	
	AWESOME_THROW(std::invalid_argument) << "unknown content... content=" << content;
	return false;
}

std::string BuildEnviroment::createCompileCommand(
	const boost::filesystem::path& inputFilepath,
	const boost::filesystem::path& outputFilepath)const
{
	if(!this->isExistItem(HIERARCHY::eBuildSetting)) {
		AWESOME_THROW(std::runtime_error) << "not initialize...";
	}
	
	auto& compileTaskGroup = this->findCompileTaskGroup(this->mpProject->type, this->mpBuildSetting->compiler);
	auto compileTask = compileTaskGroup.compileObj;
	compileTask.evalVariables(this->mVariables);
	
	std::stringstream cmd;
	cmd << compileTask.command;

	if(!compileTask.commandPrefix.empty()) {
		cmd << " " << compileTask.commandPrefix;
	}
	
	if(!compileTask.inputOption.empty()) {
		cmd << " " << compileTask.inputOption;
	}
	cmd << " " << inputFilepath.string();
		
	if(!compileTask.outputOption.empty()) {
		cmd << " " << compileTask.outputOption;
	}
	cmd << " " << outputFilepath.string();
	
	// write options
	cmd << " ";
	boost::copy(
		this->mpBuildSetting->compileOptions,
		std::ostream_iterator<std::string>(cmd, " "));

	cmd << " ";
	boost::copy(
		this->mpBuildSetting->includeDirectories
		| boost::adaptors::transformed([&](const std::string& path){
			return "-I=" + path;
		}),
		std::ostream_iterator<std::string>(cmd, " "));

	if(this->isExistItem(HIERARCHY::eFileToProcess)) {
		auto& fileToProcess = this->fileToProcess();
		if(!fileToProcess.compileOptions.empty()) {
			cmd << " ";
			boost::copy(
				fileToProcess.compileOptions,
				std::ostream_iterator<std::string>(cmd, " "));			
		}
	}
	
	if(!compileTask.commandSuffix.empty()) {
		cmd << " " << compileTask.commandSuffix;
	}
	
	return cmd.str();
}

std::string BuildEnviroment::createLinkCommand(
	const std::vector<std::string>& linkTargets,
	const boost::filesystem::path& outputFilepath)const
{
	if(!this->isExistItem(HIERARCHY::eBuildSetting)) {
		AWESOME_THROW(std::runtime_error) << "not initialize...";
	}
	
	auto& compileTaskGroup = this->findCompileTaskGroup(this->mpProject->type, this->mpBuildSetting->compiler);
	auto compileTask = compileTaskGroup.linkObj;
	compileTask.evalVariables(this->mVariables);
	
	std::stringstream linkCmd;
	linkCmd << compileTask.command;
	
	if(!compileTask.commandPrefix.empty()) {
		linkCmd << " " << compileTask.commandPrefix;
	}

	if(!compileTask.outputOption.empty()) {
		linkCmd << " " << compileTask.outputOption;
	}
	linkCmd << " " << outputFilepath.string();
	
	if(!compileTask.inputOption.empty()) {
		linkCmd << " " << compileTask.inputOption;
	}

	// input files
	linkCmd << " ";
	boost::copy(
		linkTargets
		| boost::adaptors::transformed([&](const std::string& file){
			return (compileTask.inputOption.empty())
				? file
				: compileTask.inputOption + " " + file;
		}),
		std::ostream_iterator<std::string>(linkCmd, " "));

	// link options
	boost::copy(
		this->mpBuildSetting->linkOptions,
		std::ostream_iterator<std::string>(linkCmd, " "));

	if(config::Project::eStaticLib != this->mpProject->type) {		
		// link libraries
		linkCmd << " ";
		boost::copy(
			this->mpBuildSetting->linkLibraries
			| boost::adaptors::transformed([](const std::string& lib){ return "-l" + lib; }),
			std::ostream_iterator<std::string>(linkCmd, " "));			
		
		//link dependence project
		if(!this->mpBuildSetting->dependences.empty()){
			linkCmd << " ";
			boost::copy(
				this->mpBuildSetting->dependences
				| boost::adaptors::transformed([&](const std::string& dependence){
					auto keywards = split(dependence, '.');
					if(2 != keywards.size()) {
						AWESOME_THROW(std::runtime_error)
							<< "\"" << dependence << "\" "
							<< "invalid keyward... please \"<project name>.<buildSetting name>\"";
					}
					auto& dependenceProject = this->mpRootConfig->findProject(keywards[0]);
					if(config::Project::eExe == dependenceProject.type) {
						return ""s;
					}
					
					auto& dependenceBuildSetting = dependenceProject.findBuildSetting(keywards[1]);
					auto outputPath = dependenceBuildSetting.makeOutputFilepath(this->configFilepath(), dependenceProject);
					return "-l" + outputPath.string();
				}),
				std::ostream_iterator<std::string>(linkCmd, " "));			
		}
		
		// library directories
		linkCmd << " ";
		boost::copy(
			this->mpBuildSetting->libraryDirectories
			| boost::adaptors::transformed([](const std::string& dir){ return "-L=" + dir; }),
			std::ostream_iterator<std::string>(linkCmd, " "));
	}
	
	if(!compileTask.commandSuffix.empty()) {
		linkCmd << " " << compileTask.commandSuffix;
	}

	return linkCmd.str();
}

const config::CompileTaskGroup& BuildEnviroment::findCompileTaskGroup(
	config::Project::Type projectType,
	const std::string& compiler)const
{
	auto it = this->mpRootConfig->customCompilers.find(compiler);
	const config::CustomCompiler* pCompiler = nullptr;
	if(this->mpRootConfig->customCompilers.end() != it) {
		pCompiler = &it->second;
	} else {
		auto it = this->sBuildInCustomCompilers.find(compiler);
		if(this->sBuildInCustomCompilers.end() != it) {
			pCompiler = &it->second;
		}
	}
	
	if(nullptr == pCompiler) {
		AWESOME_THROW(std::invalid_argument)
			<< "don't found customCompiler... name=" << compiler;
	}

	switch(projectType) {
	case config::Project::eExe: return pCompiler->exe;
	case config::Project::eStaticLib: return pCompiler->staticLib;
	case config::Project::eSharedLib: return pCompiler->sharedLib;
	}
	
	AWESOME_THROW(std::invalid_argument)
		<< "unknown project type... type=" << config::Project::toStr(projectType);
}

void BuildEnviroment::checkDependenceProjects()const
{
	auto& rootConfig = this->rootConfig();
	auto& buildSetting = *this->mpBuildSetting;
	
	for(auto&& dependence : buildSetting.dependences) {
		auto keywards = split(dependence, '.');
		if(2 != keywards.size()) {
			AWESOME_THROW(std::runtime_error)
				<< "\"" << dependence << "\" "
				<< "invalid keyward... please \"<project name>.<buildSetting name>\"";
		}
		
		auto& dependenceProject = rootConfig.findProject(keywards[0]);
		auto& dependenceBuildSetting = dependenceProject.findBuildSetting(keywards[1]);
		
		std::stringstream cmd;
		cmd << "watagasi build -c "
			<< this->configFilepath().string()
			<< " -p " << keywards[0]
			<< " -b " << keywards[1];
		 auto ret = std::system(cmd.str().c_str());
		 if(0 != ret) {
		 	AWESOME_THROW(std::runtime_error) << "Failed to build dependence project...";
		 }
	}
}

const config::RootConfig& BuildEnviroment::rootConfig()const
{
	if(nullptr == this->mpRootConfig) {
		AWESOME_THROW(std::runtime_error) << "rootConfig is nullptr";
	}
	return *this->mpRootConfig;
}

const config::Project& BuildEnviroment::project()const
{
	if(!this->isExistItem(HIERARCHY::eProject)) {
		AWESOME_THROW(std::runtime_error) << "invalid item hierarchy...";
	}
	return *this->mpProject;
}

const config::BuildSetting& BuildEnviroment::buildSetting()const
{
	if(!this->isExistItem(HIERARCHY::eBuildSetting)) {
		AWESOME_THROW(std::runtime_error) << "invalid item hierarchy...";
	}
	return *this->mpBuildSetting;
}

const config::TargetDirectory& BuildEnviroment::targetDirectory()const
{
	if(!this->isExistItem(HIERARCHY::eTargetDirectory)) {
		AWESOME_THROW(std::runtime_error) << "invalid item hierarchy...";
	}
	return *this->mpTargetDir;
}

const config::FileFilter& BuildEnviroment::fileFilter()const
{
	if(!this->isExistItem(HIERARCHY::eFileFilter)) {
		AWESOME_THROW(std::runtime_error) << "invalid item hierarchy...";
	}
	return *this->mpFileFilter;
}

const config::FileToProcess& BuildEnviroment::fileToProcess()const
{
	if(!this->isExistItem(HIERARCHY::eFileToProcess)) {
		AWESOME_THROW(std::runtime_error) << "invalid item hierarchy...";
	}
	return *this->mpFileToProcess;
}

bool BuildEnviroment::isExistItem(HIERARCHY hierarchy)const
{
	return static_cast<size_t>(hierarchy) < this->mpItemHierarchy.size();
}

const SpecialVariables& BuildEnviroment::variables()const
{
	return this->mVariables;
}

boost::filesystem::path BuildEnviroment::configFilepath()const
{
	return this->mpOpts->configFilepath;
}

boost::filesystem::path BuildEnviroment::configFileDirectory()const
{
	return this->configFilepath().parent_path();
}

}
