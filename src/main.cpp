#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/adaptor/filtered.hpp>

#include "utility.h"
#include "builder.h"
#include "programOptions.h"
#include "exception.hpp"
#include "includeFileAnalyzer.h"
#include "data.h"
#include "parser/parser.h"
#include "parser/value.h"
#include "parser/parserUtility.h"

using namespace std;
using namespace watagashi;
namespace fs = boost::filesystem;

static data::Project createProject(parser::Value const& externObj, watagashi::ProgramOptions const& options);
static void setBuilder(Builder &builder, parser::Value const& configData);
static void definedBuildInData(parser::Value& externObj);
static data::Compiler createClangCppCompiler();
static data::Compiler createGccCppCompiler();
static void showProjects(parser::Value const& configData);

ExceptionHandlerSetter exceptionHandlerSetter;

int main(int argn, char** args)
{
    try {
        auto options = watagashi::ProgramOptions();
        if (!options.parse(argn, args)) {
            return 0;
        }
        auto taskType = options.taskType();

        parser::ParserDesc desc;
        definedBuildInData(desc.externObj);
        auto configData = parser::parse(boost::filesystem::path(options.configFilepath), desc);

        switch (taskType) {
        case ProgramOptions::TaskType::Interactive:
            parser::confirmValueInInteractive(configData);
            return 0;
        case ProgramOptions::TaskType::ShowProjects:
            showProjects(configData);
            return 0;
        }

        data::Project project = createProject(configData, options);
        Builder builder(project, options);
        setBuilder(builder, configData);
        builder.addCompiler(createClangCppCompiler());
        builder.addCompiler(createGccCppCompiler());

        switch (options.taskType()) {
        case ProgramOptions::TaskType::Build:       builder.build(); break;
        case ProgramOptions::TaskType::Clean:       builder.clean(); break;
        case ProgramOptions::TaskType::Rebuild:     builder.rebuild(); break;
        case ProgramOptions::TaskType::ListupFiles: builder.listupFiles(); break;
        case ProgramOptions::TaskType::Install:     builder.install(); break;
        default:
            cerr << "unknwon task type..." << endl;
            return 1;
        }

    } catch (...) {
        ExceptionHandlerSetter::terminate();
        return 1;
    }

    return 0;
}

void showProjects(parser::Value const& configData)
{
    cout << "project list" << endl;
    auto& configObj = configData.get<parser::Value::object>();
    for (auto& [name, value] : configObj.members) {
        if (parser::Value::Type::Object != value.type) {
            continue;
        }

        auto& obj = value.get<parser::Value::object>();
        if ("Project" != obj.pDefined->name) {
            continue;
        }
        cout << "  " << name << endl;
    }
}

bool checkExtention(const fs::path& path, std::unordered_set<std::string> const& extensions)
{
    auto ext = path.extension();
    auto str = ext.string();
    return extensions.end() != extensions.find(str.c_str() + 1);
}

data::Project createProject(parser::Value const& configData, watagashi::ProgramOptions const& options)
{
    using namespace parser;
    std::string projectName = options.targetProject;
    auto projectValue = configData.getChild(projectName);
    if (parser::Value::Type::Object != projectValue.type) {
        AWESOME_THROW(std::runtime_error)
            << "don't found project '" << projectName << "' or project type is not 'Object'...";
    }

    auto getString = [&](std::string const& name, std::string const& defaultValue, Value const& value) {
        auto& str = value.getChild(name).get<Value::string>();
        if (str.empty()) {
            return defaultValue;
        }
        return str;
    };

    auto getNumber = [&](std::string const& name, auto const& defaultValue, Value const& value) {
        auto& num = value.getChild(name).get<Value::number>();
        if (num == 0.0) {
            return defaultValue;
        }
        return static_cast<decltype(defaultValue)>(num);
    };

    auto getStringArray = [&](auto& out, std::string const& name, Value const& value) {
        auto& arr = value.getChild(name).get<Value::array>();
        for (auto&& e : arr) {
            if (Value::Type::String != e.type) {
                continue;
            }
            auto& str = e.get<Value::string>();
            out.insert(str);
        }
    };

    data::Project project;
    project.name = projectName;
    project.rootDirectory = options.rootDirectories; // TODO
    project.type = data::Project::toType(projectValue.getChild("type").get<Value::string>());
    project.compiler = projectValue.getChild("compiler").get<Value::string>();
    project.outputName = getString("outputName", project.name, projectValue);
    project.outputPath = getString("outputPath", "output", projectValue);
    project.intermediatePath = getString("intermediatePath", "intermediate", projectValue);
    getStringArray(project.compileOptions, "compileOptions", projectValue);
    getStringArray(project.includeDirectories, "includeDirectories", projectValue);
    getStringArray(project.linkOptions, "linkOptions", projectValue);
    getStringArray(project.linkLibraries, "linkLibraries", projectValue);
    getStringArray(project.libraryDirectories, "libraryDirectories", projectValue);
    project.version = getNumber("version", static_cast<int>(0), projectValue);
    project.minorNumber = getNumber("minorNumber", static_cast<int>(0), projectValue);
    project.releaseNumber = getNumber("releaseNumber", static_cast<int>(0), projectValue);
    project.preprocess = getString("preprocess", "", projectValue);
    project.linkPreprocess = getString("linkPreprocess", "", projectValue);
    project.postprocess = getString("postprocess", "", projectValue);

    {
        auto& arr = projectValue.getChild("targets").get<Value::array>();
        for (auto&& e : arr) {
            switch (e.type) {
            case Value::Type::Object:
            {
                auto& directory = e.get<Value::object>();

                std::unordered_set<std::string> extensions;
                for (auto&& ext : directory.members["extensions"].get<Value::array>()) {
                    if (Value::Type::String != ext.type) {
                        continue;
                    }
                    extensions.insert(ext.get<Value::string>());
                }
                std::vector<std::string> ignores;
                for (auto&& ignorePattern : directory.members["ignores"].get<Value::array>()) {
                    if (Value::Type::String != ignorePattern.type) {
                        continue;
                    }
                    ignores.push_back(ignorePattern.get<Value::string>());
                }

                boost::filesystem::path path = directory.members["path"].get<Value::string>();
                boost::for_each(
                    boost::filesystem::recursive_directory_iterator(project.rootDirectory / path)
                    | boost::adaptors::filtered([&](boost::filesystem::path const& path) {
                       if (!checkExtention(path, extensions)) {
                           return false;
                       }
                       for (auto&& ignorePattern : ignores) {
                           if (matchFilepath(ignorePattern, path, project.rootDirectory)) {
                               return false;
                           }
                       }
                       return true;
                    })
                    , [&](boost::filesystem::path const& filepath) {
                        project.targets.insert(fs::relative(filepath, project.rootDirectory));
                    }
                );

                break;
            }
            case Value::Type::String:
                project.targets.insert(e.get<Value::string>());
                break;
            default:
                break;
            }
        }
    }
    {
        auto& arr = projectValue.getChild("fileFilters").get<Value::array>();
        for (auto&& e : arr) {
            if (Value::Type::Object != e.type) {
                continue;
            }
            data::FileFilter fileFilter;
            fileFilter.targetKeyward = getString("filepath", "", e);
            getStringArray(fileFilter.compileOptions, "compileOptions", e);
            getStringArray(fileFilter.includeDirectories, "includeDirectories", e);
            auto process = getString("preprocess", "", e);
            if (!process.empty()) {
                data::TaskProcess taskProcess;
                taskProcess.setTypeAndContent(data::TaskProcess::Type::Terminal, std::move(process));
                fileFilter.preprocess.push_back(taskProcess);
            }
            process = getString("postprocess", "", e);
            if (!process.empty()) {
                data::TaskProcess taskProcess;
                taskProcess.setTypeAndContent(data::TaskProcess::Type::Terminal, std::move(process));
                fileFilter.postprocess.push_back(taskProcess);
            }
            project.fileFilters.push_back(fileFilter);
        }
    }
    
    return project;
}

void setBuilder(Builder &builder, parser::Value const& configData)
{

}

void definedBuildInData(parser::Value& externObj)
{
    parser::Value projectDefined;
    projectDefined = parser::ObjectDefined("Project");
    projectDefined.addMember("type", parser::MemberDefined(parser::Value::Type::String, parser::Value::none));
    projectDefined.addMember("targets", parser::MemberDefined(parser::Value::Type::Array, parser::Value::none));
    projectDefined.addMember("compiler", parser::MemberDefined(parser::Value::Type::String, parser::Value::none));
    projectDefined.addMember("outputName", parser::MemberDefined(parser::Value::Type::String, ""s));
    projectDefined.addMember("outputPath", parser::MemberDefined(parser::Value::Type::String, ""s));
    projectDefined.addMember("intermediatePath", parser::MemberDefined(parser::Value::Type::String, ""s));
    projectDefined.addMember("fileFilters", parser::MemberDefined(parser::Value::Type::Array, parser::Value::array()));
    projectDefined.addMember("compileOptions", parser::MemberDefined(parser::Value::Type::Array, parser::Value::array()));
    projectDefined.addMember("includeDirectories", parser::MemberDefined(parser::Value::Type::Array, parser::Value::array()));
    projectDefined.addMember("linkOptions", parser::MemberDefined(parser::Value::Type::Array, parser::Value::array()));
    projectDefined.addMember("linkLibraries", parser::MemberDefined(parser::Value::Type::Array, parser::Value::array()));
    projectDefined.addMember("libraryDirectories", parser::MemberDefined(parser::Value::Type::Array, parser::Value::array()));
    projectDefined.addMember("version", parser::MemberDefined(parser::Value::Type::Number, 0.0));
    projectDefined.addMember("minorNumber", parser::MemberDefined(parser::Value::Type::Number, 0.0));
    projectDefined.addMember("releaseNumber", parser::MemberDefined(parser::Value::Type::Number, 0.0));
    projectDefined.addMember("preprocess", parser::MemberDefined(parser::Value::Type::String, ""s));
    projectDefined.addMember("linkPreprocess", parser::MemberDefined(parser::Value::Type::String, ""s));
    projectDefined.addMember("postprocess", parser::MemberDefined(parser::Value::Type::String, ""s));

    parser::Value directoryDefiend = parser::ObjectDefined("Directory");
    directoryDefiend.addMember("path", parser::MemberDefined(parser::Value::Type::String, parser::Value::none));
    directoryDefiend.addMember("extensions", parser::MemberDefined(parser::Value::Type::Array, parser::Value::none));
    directoryDefiend.addMember("ignores", parser::MemberDefined(parser::Value::Type::Array, parser::Value::none));

    parser::Value fileFiltersDefined = parser::ObjectDefined("FileFilter");
    fileFiltersDefined.addMember("filepath", parser::MemberDefined(parser::Value::Type::String, parser::Value::none));
    fileFiltersDefined.addMember("compileOptions", parser::MemberDefined(parser::Value::Type::Array, parser::Value::array()));
    fileFiltersDefined.addMember("includeDirectories", parser::MemberDefined(parser::Value::Type::Array, parser::Value::array()));
    fileFiltersDefined.addMember("preprocess", parser::MemberDefined(parser::Value::Type::Array, parser::Value::array()));
    fileFiltersDefined.addMember("postprocess", parser::MemberDefined(parser::Value::Type::Array, parser::Value::array()));

    externObj.addMember("Project", projectDefined);
    externObj.addMember("Directory", directoryDefiend);
    externObj.addMember("FileFilter", fileFiltersDefined);
}

data::Compiler createClangCppCompiler()
{
    return data::Compiler("clang++")
        .setExe(data::TaskBundle()
            .setCompileObj(data::Task("clang++")
                .setInputAndOutputOption("", "-o")
                .setOptionPrefix("-c")
                .setPreprocesses({
                    data::TaskProcess(data::TaskProcess::Type::BuildIn, "checkUpdate")
                    })
            )
            .setlinkObjs(data::Task("clang++")
                .setInputAndOutputOption("", "-o")
            )
        )
        .setStatic(data::TaskBundle()
            .setCompileObj(data::Task("clang++")
                .setInputAndOutputOption("", "-o")
                .setCommand("-c")
                .setPreprocesses({
                    data::TaskProcess(data::TaskProcess::Type::BuildIn, "checkUpdate")
                    })
            )
            .setlinkObjs(data::Task("ar")
                .setInputAndOutputOption("", "-o")
                .setOptionPrefix("rcs")
            )
        )
        .setShared(data::TaskBundle()
            .setCompileObj(data::Task("clang++")
                .setInputAndOutputOption("", "-o")
                .setOptionPrefix("-c -fPIC")
                .setPreprocesses({
                    data::TaskProcess(data::TaskProcess::Type::BuildIn, "checkUpdate")
                    })
            )
            .setlinkObjs(data::Task("clang++")
                .setInputAndOutputOption("", "-o")
                .setOptionPrefix("-shared -Wl,-soname,${OUTPUT_FILE_NAME}")
                .setPreprocesses({
                    data::TaskProcess(data::TaskProcess::Type::Terminal, "ldconfig -n ${OUTPUT_FILE_DIR}")
                    })
            )
        );
}

data::Compiler createGccCppCompiler()
{
    return data::Compiler("g++")
        .setExe(data::TaskBundle()
            .setCompileObj(data::Task("g++")
                .setInputAndOutputOption("", "-o")
                .setOptionPrefix("-c")
                .setPreprocesses({
                    data::TaskProcess(data::TaskProcess::Type::BuildIn, "checkUpdate")
                    })
            )
            .setlinkObjs(data::Task("g++")
                .setInputAndOutputOption("", "-o")
            )
        )
        .setStatic(data::TaskBundle()
            .setCompileObj(data::Task("g++")
                .setInputAndOutputOption("", "-o")
                .setCommand("-c")
                .setPreprocesses({
                    data::TaskProcess(data::TaskProcess::Type::BuildIn, "checkUpdate")
                    })
            )
            .setlinkObjs(data::Task("ar")
                .setInputAndOutputOption("", "-o")
                .setOptionPrefix("rcs")
            )
        )
        .setShared(data::TaskBundle()
            .setCompileObj(data::Task("g++")
                .setInputAndOutputOption("", "-o")
                .setOptionPrefix("-c -fPIC")
                .setPreprocesses({
                    data::TaskProcess(data::TaskProcess::Type::BuildIn, "checkUpdate")
                    })
            )
            .setlinkObjs(data::Task("g++")
                .setInputAndOutputOption("", "-o")
                .setOptionPrefix("-shared -Wl,-soname,${OUTPUT_FILE_NAME}")
                .setPreprocesses({
                    data::TaskProcess(data::TaskProcess::Type::Terminal, "ldconfig -n ${OUTPUT_FILE_DIR}")
                    })
            )
        );
}