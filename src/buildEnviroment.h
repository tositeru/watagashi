#pragma once

#include <string>
#include <list>
#include <unordered_map>
#include <memory>

#include <boost/filesystem.hpp>

#include "config.h"
#include "builder.h"
#include "specialVariables.h"

namespace watagashi
{

class BuildEnviroment
{
public:
    enum class HIERARCHY : size_t
    {
        eRootConfig,
        eProject,
        eBuildSetting,
        eTargetDirectory,
        eFileFilter,
        eFileToProcess,
    };
    
    enum class TaskType
    {
        eCompile,
        eLink,
    };
    
public:
    BuildEnviroment();
    ~BuildEnviroment();

    void clear();
    
    void init(
        const std::shared_ptr<config::RootConfig> pRootConfig,
        const std::shared_ptr<ProgramOptions> pOptions);

    void clearTargetDirectory();
    void clearFileFilter();
    void clearFileToProcess();
    void clearTargetEnviroment();
    
    void setupTargetDirectory(const config::TargetDirectory& targetDir);
    void setupFileFilter(const config::FileFilter& fileFilter);
    void setupFileToProcess(
        const config::FileToProcess& fileToProcess);
    void setupTargetEnviroment(
        const SpecialVariables::TargetEnviromentScope::InitDesc& desc);

    void checkDependenceProjects()const;

    bool preprocessCompiler(
        TaskType taskType,
        const boost::filesystem::path& inputFilepath,
        const boost::filesystem::path& outputFilepath)const;
    
    bool postprocessCompiler(
        TaskType taskType,
        const boost::filesystem::path& inputFilepath,
        const boost::filesystem::path& outputFilepath)const;

    std::string createCompileCommand(
        const boost::filesystem::path& inputFilepath,
        const boost::filesystem::path& outputFilepath)const;
    
    std::string createLinkCommand(
        const std::vector<std::string>& linkTargets,
        const boost::filesystem::path& outputFilepath)const;
    
public:
    const config::RootConfig& rootConfig()const;
    const config::Project& project()const;
    const config::BuildSetting& buildSetting()const;
    const config::TargetDirectory& targetDirectory()const;
    const config::FileFilter& fileFilter()const;
    const config::FileToProcess& fileToProcess()const;

    bool isExistItem(HIERARCHY hierarchy)const;
    
    const SpecialVariables& variables()const;
    
    boost::filesystem::path configFilepath()const;
    boost::filesystem::path configFileDirectory()const;
    
private:
    bool adaptTemplateItem(config::IItem* pInOut, const std::string& templateName, config::TemplateItemType itemType)const;
    const config::TemplateItem* findTemplateItem(const std::string& name, config::TemplateItemType itemType)const;
    
    bool setupIItem(config::IItem* pInOut, const std::string& templateName, config::TemplateItemType itemType, std::function<void()> variableInitializer, bool isPushHierarchy=true);

    const config::CompileTaskGroup& findCompileTaskGroup(config::Project::Type projectType, const std::string& compiler)const;

    bool processCompiler(
        const std::vector<config::CompileProcess>& processes,
        const boost::filesystem::path& inputFilepath,
        const boost::filesystem::path& outputFilepath)const;

    bool runBuildInProcess(
        const std::string& content,
        const boost::filesystem::path& inputFilepath,
        const boost::filesystem::path& outputFilepath)const;

    
private:
    static const std::unordered_map<std::string, config::CustomCompiler> sBuildInCustomCompilers;

    std::shared_ptr<ProgramOptions> mpOpts;
    SpecialVariables mVariables;
    
    std::shared_ptr<config::RootConfig> mpRootConfig;
    std::shared_ptr<config::Project> mpProject;
    std::shared_ptr<config::BuildSetting> mpBuildSetting;
    std::shared_ptr<config::TargetDirectory> mpTargetDir;
    std::shared_ptr<config::FileFilter> mpFileFilter;
    std::shared_ptr<config::FileToProcess> mpFileToProcess;
    
    std::list<const config::IItem*> mpItemHierarchy;
};

}
