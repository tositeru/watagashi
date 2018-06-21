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

namespace watagashi
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
};

namespace data
{
    struct Project;
    struct Compiler;
}

class Builder_ final
{
    Builder_(Builder_ const&) = delete;
    Builder_& operator=(Builder_ const&) = delete;

public:
    Builder_(data::Project const& project, ProgramOptions const& options);

    void addCompiler(data::Compiler const& compiler);

    void build()const;
    void clean()const;
    void rebuild()const;
    void listupFiles()const;
    void install()const;

public:
    data::Project const& project()const;
    ProgramOptions const& options()const;
    data::Compiler const& getCompiler(std::string const& name)const;

private:
    data::Project const& mProject;
    ProgramOptions const& mOptions;
    std::unordered_map<std::string, data::Compiler> mCompilerMap;
};

}
