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
class ProcessServer;

namespace data
{
    struct Project;
    struct Compiler;
}

class Builder final
{
    Builder(Builder const&) = delete;
    Builder& operator=(Builder const&) = delete;

public:
    struct Scope
    {
        boost::filesystem::path inputFilepath;
        boost::filesystem::path outputFilepath;

    };

public:
    Builder(data::Project const& project, ProgramOptions const& options);

    void addCompiler(data::Compiler const& compiler);
    void addCompiler(data::Compiler && compiler);

    void build()const;
    void clean()const;
    void rebuild()const;
    void listupFiles()const;
    void install()const;

public:
    data::Project const& project()const;
    ProgramOptions const& options()const;
    data::Compiler const& getCompiler(std::string const& name)const;

    std::string parseVariables(std::string const& str, Scope const& scope)const;

private:
    data::Project const& mProject;
    ProgramOptions const& mOptions;
    std::unordered_map<std::string, data::Compiler> mCompilerMap;
};

}
