#include "config.h"

#include <iostream>
#include <regex>
#include <boost/filesystem.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/transform.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/adaptor/filtered.hpp>

#include "exception.hpp"
#include "errorReceiver.h"
#include "utility.h"
#include "specialVariables.h"

using namespace std;
namespace fs = boost::filesystem;

namespace watagashi::config
{

static void exeCommand(
    const std::string& command,
    const std::string& exceptionMessage)
{
    if("" != command) {
        auto ret = std::system(command.c_str());
        if(0 != ret) {
            AWESOME_THROW(std::runtime_error) << exceptionMessage;
        }
    }
}

static bool matchFilepath(
    const std::string& patternStr,
    const fs::path& filepath,
    const fs::path& standardPath)
{
    auto relativeTargetPath = fs::relative(filepath, standardPath);

    if(patternStr.front() == '@') {
        // regular expressions check
        std::regex regex(patternStr.substr(1));
        if( std::regex_search(relativeTargetPath.string(), regex) ) {
            return true;
        }
    } else {
        fs::path patternPath(patternStr);
        patternPath = fs::relative(standardPath/patternPath);
        if(patternStr.back() == '/') {
            //directory check
            auto checkPath = fs::relative(filepath, patternPath);
            if( std::string::npos != checkPath.string().find("../") ) {
                return true;
            }
        } else {
            //filename check
            if( fs::equivalent(patternPath, fs::relative(filepath)) ) {
                return true;
            }
        }
    }
    return false;
}

//******************************************************************************
//
//    IItem
//
//******************************************************************************

//******************************************************************************
//
//    UserValue
//
//******************************************************************************
const TemplateItem* UserValue::findTemplateItem(const std::string& name, TemplateItemType itemType)const
{
    auto it = boost::range::find_if(this->defineTemplates, [&](const TemplateItem& item){
        return item.name == name && item.itemType == itemType;
    });
    return this->defineTemplates.end() == it ? nullptr : &(*it);
}

void UserValue::evalVariables(const SpecialVariables& variables)
{ }

void UserValue::adaptTemplateItem(const TemplateItem& templateItem)
{ }

//******************************************************************************
//
//    FileToProcess
//
//******************************************************************************

void FileToProcess::evalVariables(const SpecialVariables& variables)
{
    this->filepath = variables.parse(this->filepath);
    this->preprocess = variables.parse(this->preprocess);
    this->postprocess = variables.parse(this->postprocess);

    auto parseString = [&](const std::string& op){
        return variables.parse(op);
    };
    boost::range::transform(this->compileOptions
        , this->compileOptions.begin(), parseString);
    
    this->templateName = variables.parse(this->templateName);
}

void FileToProcess::adaptTemplateItem(const TemplateItem& templateItem)
{
    if(TemplateItemType::eFileToProcess != templateItem.itemType) {
        cerr << "template \"" << templateItem.name << "\" is not fileToProcess type. ignore this template..." << endl;
        return ;
    }
    auto& item = templateItem.fileToProcess();

    // enable append elements
    auto appendElement = [](auto* pOut, const auto& src){
        if(!src.empty()) {
            pOut->reserve(pOut->size() + src.size());
            boost::copy(src, std::back_inserter(*pOut));
        }
    };
    appendElement(&this->compileOptions, item.compileOptions);
    
    // enable override elements
    if(this->filepath.empty() && !item.filepath.empty()) {
        this->filepath = item.filepath;
    }
    if(this->preprocess.empty() && !item.preprocess.empty()) {
        this->preprocess = item.preprocess;
    }
    if(this->postprocess.empty() && !item.postprocess.empty()) {
        this->postprocess = item.postprocess;
    }
    
    // ignore elements
}

void FileToProcess::exePreprocess(const fs::path& filepath)const
{
    exeCommand(this->preprocess, "Failed \"" + filepath.string() + "\" preprocess..");
}

void FileToProcess::exePostprocess(const fs::path& filepath)const
{
    exeCommand(this->postprocess, "Failed \"" + filepath.string() + "\" postprocess..");
}

//
//    struct FileFilter
//

bool FileFilter::checkExtention(const fs::path& path)const
{
    auto ext = path.extension();
    auto str = ext.string();
    return this->extensions.end() != this->extensions.find(str.c_str()+1);
}

void FileFilter::evalVariables(const SpecialVariables& variables)
{
    auto parseString = [&](const std::string& op){
        return variables.parse(op);
    };
    decltype(this->extensions) extensions;
    extensions.reserve(this->extensions.size());
    for(auto&& ext : this->extensions) {
        extensions.insert(variables.parse(ext));
    }
    this->extensions = std::move(extensions);
}

void FileFilter::adaptTemplateItem(const TemplateItem& templateItem)
{
    if(TemplateItemType::eFileFilter != templateItem.itemType) {
        cerr << "template \"" << templateItem.name << "\" is not fileFilter type. ignore this template..." << endl;
        return ;
    }
    auto& item = templateItem.fileFilter();

    // enable append elements
    for(auto&& ext : item.extensions) {
        this->extensions.insert(ext);
    }
    
    // enable override elements
    
    // ignore elements
    if(!item.filesToProcess.empty()) {
        cerr << "ignore filesToProcess in FileFilter template." << endl;        
    }
}

const FileToProcess* FileFilter::findFileToProcessPointer(
    const fs::path& filepath,
    const fs::path& currentPath)const
{
    auto it = boost::find_if(this->filesToProcess,
        [&](const FileToProcess& fileToProcess){
            return matchFilepath(fileToProcess.filepath, filepath, currentPath);
        });
    return (this->filesToProcess.end() != it) ? &(*it) : nullptr;
}

//
//    struct TargetDirectory
//
    
bool TargetDirectory::isIgnorePath(const fs::path& targetPath, const fs::path& currentPath)const
{
    auto standardPath = currentPath/this->path;
    
    for(auto&& ignoreStr : this->ignores) {
        if(matchFilepath(ignoreStr, targetPath, standardPath)) {
            return true;
        }
    }
    return false;
}
    
bool TargetDirectory::filterPath(const fs::path& path)const
{
    return nullptr != this->getFilter(path);
}

const FileFilter* TargetDirectory::getFilter(const boost::filesystem::path& path)const
{
    for(auto&& filter : this->fileFilters) {
        if(filter.checkExtention(path))
            return &filter;
    }
    return nullptr;
}


void TargetDirectory::evalVariables(const SpecialVariables& variables)
{
    this->path = variables.parse(this->path);
    this->fileFilters.reserve(this->fileFilters.size());
    for(auto&& filter : this->fileFilters) {
        filter.evalVariables(variables);
    }

    boost::range::transform(this->ignores
        , this->ignores.begin()
        , [&](const std::string& op){ return variables.parse(op); });
}

void TargetDirectory::adaptTemplateItem(const TemplateItem& templateItem)
{
    if(TemplateItemType::eTargetDirectory != templateItem.itemType) {
        cerr << "template \"" << templateItem.name << "\" is not targetDirectory type. ignore this template..." << endl;
        return ;
    }
    auto& item = templateItem.targetDirectory();

    // enable append elements
    auto appendElement = [](auto* pOut, const auto& src){
        if(!src.empty()) {
            pOut->reserve(pOut->size() + src.size());
            boost::copy(src, std::back_inserter(*pOut));
        }
    };
    appendElement(&this->ignores, item.ignores);
    
    // enable override elements
    if(this->path.empty() && !item.path.empty()) {
        this->path = item.path;
    }
    
    // ignore elements
    if(!item.fileFilters.empty()) {
        cerr << "ignore fileFilters in targetDirectory template." << endl;        
    }
}

//
//    struct OutputConfig
//

void OutputConfig::evalVariables(const SpecialVariables& variables)
{
    this->name = variables.parse(this->name);
    this->outputPath = variables.parse(this->outputPath);
    this->intermediatePath = variables.parse(this->intermediatePath);
}

void OutputConfig::adaptTemplateItem(const TemplateItem& templateItem)
{
}

//
//    struct BuildSetting
//

boost::filesystem::path BuildSetting::makeIntermediatePath(
    const boost::filesystem::path& configFilepath,
    const Project& project)const
{
    // outputPath is <configFilepath.parent_path()>/<intermediatePath>/<configFileStem>_<projectName>_<buildSettingName>/
    auto outputPath = configFilepath.parent_path();
    outputPath /= (this->outputConfig.intermediatePath.empty()
        ? "intermediate" : this->outputConfig.intermediatePath);
    outputPath /= configFilepath.stem();
    outputPath += ("_" +project.name + "_" + this->name);
    if(!createDirectory(outputPath)) {
        auto msg = "Failed to create output directory. path=" + outputPath.parent_path().string();
        AWESOME_THROW(std::runtime_error) << msg;
    }
    return outputPath;
}

boost::filesystem::path BuildSetting::makeOutputFilepath(
    const boost::filesystem::path& configFilepath,
    const Project& project)const
{
    // outputPath is <configFilepath.parent_path()>/<outputPath>/<configFilepathStem>_<projectName>_<buildSettingName>/outputFilename
    auto outputPath = fs::path(configFilepath).parent_path();
    outputPath /= (this->outputConfig.outputPath.empty()
        ? "output" : this->outputConfig.outputPath);
    outputPath /= configFilepath.stem();
    outputPath += ("_" + project.name+"_"+this->name);
    if(!createDirectory(outputPath)) {
        auto msg = "Failed to create output directory. path=" + outputPath.parent_path().string();
        AWESOME_THROW(std::runtime_error) << msg;
    }
    
    outputPath = fs::canonical(outputPath, "./") / this->outputConfig.name;
    outputPath = fs::relative(outputPath, fs::initial_path());
    if(Project::eSharedLib == project.type) {
        std::string soname = "lib" + this->outputConfig.name + ".so." + std::to_string(project.version);
        outputPath = outputPath.parent_path()/soname;
    }else if(Project::eStaticLib == project.type) {
        std::string arName = "lib" + this->outputConfig.name + ".a";
        outputPath = outputPath.parent_path()/arName;
    }
    return outputPath;
}

void BuildSetting::exePreprocess()const
{
    exeCommand(this->preprocess, "Failed preprocess..");
}

void BuildSetting::exeLinkPreprocess()const
{
    exeCommand(this->linkPreprocess, "Failed link preprocess..");
}

void BuildSetting::exePostprocess()const
{
    exeCommand(this->postprocess, "Failed postprocess..");
}

void BuildSetting::evalVariables(const SpecialVariables& variables)
{
    this->compiler = variables.parse(this->compiler);
    
    auto parseString = [&](const std::string& op){
        return variables.parse(op);
    };
    boost::range::transform(this->compileOptions, this->compileOptions.begin(), parseString);
    boost::range::transform(this->linkLibraries, this->linkLibraries.begin(), parseString);
    boost::range::transform(this->libraryDirectories, this->libraryDirectories.begin(), parseString);
    boost::range::transform(this->linkOptions, this->linkOptions.begin(), parseString);

    this->outputConfig.evalVariables(variables);
    this->templateName = variables.parse(this->templateName);
    this->preprocess = variables.parse(this->preprocess);
    this->linkPreprocess = variables.parse(this->linkPreprocess);
    this->postprocess = variables.parse(this->postprocess);
}

void BuildSetting::adaptTemplateItem(const TemplateItem& templateItem)
{
    if(TemplateItemType::eBuildSetting != templateItem.itemType) {
        cerr << "template \"" << templateItem.name << "\" is not buildSetting type. ignore this template..." << endl;
        return ;
    }
    auto& item = templateItem.buildSetting();
    
    // enable append elements
    auto appendElement = [](auto* pOut, const auto& src){
        if(!src.empty()) {
            pOut->reserve(pOut->size() + src.size());
            boost::copy(src, std::back_inserter(*pOut));
        }
    };
    appendElement(&this->compileOptions, item.compileOptions);
    appendElement(&this->linkLibraries, item.linkLibraries);
    appendElement(&this->libraryDirectories, item.libraryDirectories);
    appendElement(&this->linkOptions, item.linkOptions);

    // enable override elements
    if(this->compiler.empty() && !item.compiler.empty()) {
        this->compiler = item.compiler;
    }
    if(this->preprocess.empty() && !item.preprocess.empty()) {
        this->preprocess = item.preprocess;
    }
    if(this->linkPreprocess.empty() && !item.linkPreprocess.empty()) {
        this->linkPreprocess = item.linkPreprocess;
    }
    if(this->postprocess.empty() && !item.postprocess.empty()) {
        this->postprocess = item.postprocess;
    }

    // TODO define something similar to this function in OutputConfig?
    if(this->outputConfig.name.empty() && !item.outputConfig.name.empty()) {
        this->outputConfig.name = item.outputConfig.name;
    }
    if(this->outputConfig.outputPath.empty() && !item.outputConfig.outputPath.empty()) {
        this->outputConfig.outputPath = item.outputConfig.outputPath;
    }
    if(this->outputConfig.intermediatePath.empty() && !item.outputConfig.intermediatePath.empty()) {
        this->outputConfig.intermediatePath = item.outputConfig.intermediatePath;
    }
    
    // ignore elements
    if(!item.targetDirectories.empty()) {
        cerr << "ignore targetDirectories in buildSetting template." << endl;        
    }
}

//
//    struct Project
//
void Project::evalVariables(const SpecialVariables& variables)
{
}

void Project::adaptTemplateItem(const TemplateItem& templateItem)
{
    if(TemplateItemType::eProject != templateItem.itemType) {
        cerr << "template \"" << templateItem.name << "\" is not project type. ignore this template..." << endl;
        return ;
    }
    auto& item = templateItem.project();

    // enable append elements
    
    // enable override elements
    if(this->eUnknown == this->type && this->eUnknown != item.type) {
        this->type = item.type;
    }
    if(this->version <= 0 && 0 < item.version) {
        this->version = item.version;
    }
    if(this->minorNumber <= 0 && 0 < item.minorNumber) {
        this->minorNumber = item.minorNumber;
    }
    if(this->releaseNumber <= 0 && 0 < item.releaseNumber) {
        this->releaseNumber = item.releaseNumber;
    }
    
    // ignore elements
    if(!item.buildSettings.empty()) {
        cerr << "ignore buildSetting in project template." << endl;
    }
}

const BuildSetting& Project::findBuildSetting(const std::string& name)const
{
    auto buildSettingIt = this->buildSettings.find(name);
    if(this->buildSettings.end() == buildSettingIt) {
        AWESOME_THROW(std::runtime_error) << "\"" + name + "\" build settings don't found";
    }
    return buildSettingIt->second;
}

fs::path Project::makeIntermediatePath(
    const boost::filesystem::path& configFilepath,
    const std::string& intermediateDir,
    const std::string& buildSettingName)const
{
    auto& buildSetting = this->findBuildSetting(buildSettingName);
    return buildSetting.makeIntermediatePath(configFilepath, *this);    
}

fs::path Project::makeOutputFilepath(
    const boost::filesystem::path& configFilepath,
    const std::string& outputDir,
    const std::string& buildSettingName)const
{ 
    auto& buildSetting = this->findBuildSetting(buildSettingName);
    return buildSetting.makeOutputFilepath(configFilepath, *this);    
}

Project::Type Project::toType(const std::string& typeStr)
{
    static const unordered_map<std::string, Type> sTable = {
        {"exe", eExe},
        {"static", eStaticLib},
        {"shared", eSharedLib},
    };
    auto s = typeStr;
    std::transform(s.cbegin(), s.cend(), s.begin(), [](char c){ return ::tolower(c); });    
    auto it = sTable.find(s);
    if(sTable.end() == it) {
        return eUnknown;
    } else {
        return it->second;
    }
}
    
std::string Project::toStr(Type type)
{
    switch(type) {
    case eExe:         return "exe";
    case eStaticLib:     return "static";
    case eSharedLib:     return "shared";
    default:        return "(unknown)";
    }
}

//******************************************************************************
//
// struct CompileProcess
//
//******************************************************************************
void CompileProcess::evalVariables(const SpecialVariables& variables)
{
    this->content = variables.parse(this->content);
}

void CompileProcess::adaptTemplateItem(const TemplateItem& templateItem)
{
    if(TemplateItemType::eCompileProcess != templateItem.itemType) {
        cerr << "template \"" << templateItem.name << "\" is not compileProcess type. ignore this template..." << endl;
        return ;
    }
    auto& item = templateItem.compileProcess();
    
    // enable append elements
    
    // enable override elements
    if(this->type == Type::eUnknown && item.type != Type::eUnknown) {
        this->type = item.type;
    }
    if(this->content.empty() && !item.content.empty()) {
        this->content = item.content;
    }
    
    // ignore elements
}

CompileProcess::Type CompileProcess::toType(const std::string& typeStr)
{
    static const std::unordered_map<std::string, Type> sTable = {
        {"terminal", eTerminal},
        {"buildin", eBuildIn},
    };
    auto s = typeStr;
    std::transform(s.cbegin(), s.cend(), s.begin(), [](char c){ return ::tolower(c); });
    auto it = sTable.find(s);
    if(sTable.end() == it) {
        return eUnknown;
    } else {
        return it->second;
    }
}

std::string CompileProcess::toStr(Type type)
{
    switch(type) {
    case Type::eTerminal:     return "terminal";
    case Type::eBuildIn:     return "buildIn";
    default:                    return "(unknown)";
    }
}

//******************************************************************************
//
// struct CompileTask
//
//******************************************************************************    

void CompileTask::evalVariables(const SpecialVariables& variables)
{
    this->command = variables.parse(this->command);
    this->inputOption = variables.parse(this->inputOption);
    this->outputOption = variables.parse(this->outputOption);
    this->commandPrefix = variables.parse(this->commandPrefix);
    this->commandSuffix = variables.parse(this->commandSuffix);
    for(auto&& p : this->preprocesses) {
        p.evalVariables(variables);
    }
    for(auto&& p : this->postprocesses) {
        p.evalVariables(variables);
    }
}

void CompileTask::adaptTemplateItem(const TemplateItem& templateItem)
{
    if(TemplateItemType::eCompileTask != templateItem.itemType) {
        cerr << "template \"" << templateItem.name << "\" is not compileTask type. ignore this template..." << endl;
        return ;
    }
    auto& item = templateItem.compileTask();

    // enable append elements
    
    // enable override elements
    if(this->command.empty() && !item.command.empty()) {
        this->command = item.command;
    }
    if(this->inputOption.empty() && !item.inputOption.empty()) {
        this->inputOption = item.inputOption;
    }
    if(this->outputOption.empty() && !item.outputOption.empty()) {
        this->outputOption = item.outputOption;
    }
    if(this->commandPrefix.empty() && !item.commandPrefix.empty()) {
        this->commandPrefix = item.commandPrefix;
    }
    if(this->commandSuffix.empty() && !item.commandSuffix.empty()) {
        this->commandSuffix = item.commandSuffix;
    }
    
    // ignore elements    
    //this->preprocesses.adaptTemplateItem(TemplateItem(TemplateItemType::eCompileProcess, item.preprocess));
    //this->postprocesses.adaptTemplateItem(TemplateItem(TemplateItemType::eCompileProcess, item.postprocess));
}

//******************************************************************************
//
// struct CompileTaskGroup
//
//******************************************************************************    
void CompileTaskGroup::evalVariables(const SpecialVariables& variables)
{
    this->compileObj.evalVariables(variables);
    this->linkObj.evalVariables(variables);
}

void CompileTaskGroup::adaptTemplateItem(const TemplateItem& templateItem)
{
    if(TemplateItemType::eCompileTaskGroup != templateItem.itemType) {
        cerr << "template \"" << templateItem.name << "\" is not compileTask type. ignore this template..." << endl;
        return ;
    }
    auto& item = templateItem.compileTaskGroup();

    // enable append elements
    
    // enable override elements
    this->compileObj.adaptTemplateItem(TemplateItem(TemplateItemType::eCompileTask, item.compileObj));
    this->linkObj.adaptTemplateItem(TemplateItem(TemplateItemType::eCompileTask, item.linkObj));
    
    // ignore elements
}

//******************************************************************************
//
// struct CustomCompiler
//
//******************************************************************************    
void CustomCompiler::evalVariables(const SpecialVariables& variables)
{
    this->exe.evalVariables(variables);
    this->staticLib.evalVariables(variables);
    this->sharedLib.evalVariables(variables);
}

void CustomCompiler::adaptTemplateItem(
    const TemplateItem& templateItem)
{
    if(TemplateItemType::eCustomCompiler != templateItem.itemType) {
        cerr << "template \"" << templateItem.name << "\" is not customCompiler type. ignore this template..." << endl;
        return ;
    }
    auto& item = templateItem.customCompiler();

    // enable append elements
    
    // enable override elements
    this->exe.adaptTemplateItem(TemplateItem(TemplateItemType::eCompileTaskGroup, item.exe));
    this->staticLib.adaptTemplateItem(TemplateItem(TemplateItemType::eCompileTaskGroup, item.staticLib));
    this->sharedLib.adaptTemplateItem(TemplateItem(TemplateItemType::eCompileTaskGroup, item.sharedLib));
    
    // ignore elements
}

//******************************************************************************
//
// struct RootConfig
//
//******************************************************************************

const Project& RootConfig::findProject(const std::string& name) const
{
    auto projectIt = this->projects.find(name);
    if(this->projects.end() == projectIt) {
        AWESOME_THROW(std::runtime_error) << "\"" << name << "\" project don't found";
    }
    return projectIt->second;
}

void RootConfig::evalVariables(const SpecialVariables& variables)
{
}

void RootConfig::adaptTemplateItem(const TemplateItem& templateItem)
{
}

//******************************************************************************
//
//    TemplateItem
//
//******************************************************************************

void TemplateItem::evalVariables(const SpecialVariables& variables)
{
}

void TemplateItem::adaptTemplateItem(const TemplateItem& templateItem)
{
}

const Project& TemplateItem::project()const
{
    if(TemplateItemType::eProject != this->itemType) {
        AWESOME_THROW(std::runtime_error) << "item is not Project.";
    }
    return boost::get<Project>(this->item);
}

const BuildSetting& TemplateItem::buildSetting()const
{
    if(TemplateItemType::eBuildSetting != this->itemType) {
        AWESOME_THROW(std::runtime_error) << "item is not BuildSetting.";
    }
    return boost::get<BuildSetting>(this->item);
}

const TargetDirectory& TemplateItem::targetDirectory()const
{
    if(TemplateItemType::eTargetDirectory!= this->itemType) {
        AWESOME_THROW(std::runtime_error) << "item is not TargetDirectory.";
    }
    return boost::get<TargetDirectory>(this->item);
}

const FileFilter& TemplateItem::fileFilter()const
{
    if(TemplateItemType::eFileFilter!= this->itemType) {
        AWESOME_THROW(std::runtime_error) << "item is not FileFilter.";
    }
    return boost::get<FileFilter>(this->item);
}

const FileToProcess& TemplateItem::fileToProcess()const
{
    if(TemplateItemType::eFileToProcess!= this->itemType) {
        AWESOME_THROW(std::runtime_error) << "item is not FileToProcess.";
    }
    return boost::get<FileToProcess>(this->item);
}

const CustomCompiler& TemplateItem::customCompiler()const
{
    if(TemplateItemType::eCustomCompiler!= this->itemType) {
        AWESOME_THROW(std::runtime_error) << "item is not CustomCompiler.";
    }
    return boost::get<CustomCompiler>(this->item);
}

const CompileTask& TemplateItem::compileTask()const
{
    if(TemplateItemType::eCompileTask!= this->itemType) {
        AWESOME_THROW(std::runtime_error) << "item is not CompileTask.";
    }
    return boost::get<CompileTask>(this->item);
}

const CompileTaskGroup& TemplateItem::compileTaskGroup()const
{
    if(TemplateItemType::eCompileTaskGroup!= this->itemType) {
        AWESOME_THROW(std::runtime_error) << "item is not CompileTaskGroup.";
    }
    return boost::get<CompileTaskGroup>(this->item);
}

const CompileProcess& TemplateItem::compileProcess()const
{
    if(TemplateItemType::eCompileProcess!= this->itemType) {
        AWESOME_THROW(std::runtime_error) << "item is not CompileProcess.";
    }
    return boost::get<CompileProcess>(this->item);
}

TemplateItemType TemplateItem::toItemType(const std::string& typeStr)
{
    static const unordered_map<std::string, TemplateItemType> sTable = {
        {"project", eProject},
        {"buildsetting", eBuildSetting},
        {"targetdirectory", eTargetDirectory},
        {"filefilter", eFileFilter},
        {"filetoprocess", eFileToProcess},
        {"customcompiler", eCustomCompiler},
        {"compiletask", eCompileTask},
        {"complietaskgroup", eCompileTaskGroup},
        {"compileprocess", eCompileProcess},
    };

    auto s = typeStr;
    std::transform(s.cbegin(), s.cend(), s.begin(), [](char c){ return ::tolower(c); });
    auto it = sTable.find(s);
    if(sTable.end() == it) {
        return eUnknown;
    } else {
        return it->second;
    }
}

std::string TemplateItem::toStr(TemplateItemType type)
{
    switch(type) {
    case eProject:                 return "project";
    case eBuildSetting:             return "buildSetting";
    case eTargetDirectory:     return "targetdirectory";
    case eFileFilter:             return "filefilter";
    case eFileToProcess:         return "filetoprocess";
    case eCustomCompiler:        return "customcompiler";
    case eCompileTask:            return "compiletask";
    case eCompileTaskGroup:    return "complietaskgroup";
    case eCompileProcess:        return "compileprocess";
    default:                        return "(unknown)";
    }
}

}
