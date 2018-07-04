#include <iostream>

#include <boost/filesystem.hpp>

#include "utility.h"
#include "config.h"
#include "configJsonParser.h"
#include "builder.h"
#include "programOptions.h"
#include "exception.hpp"
#include "specialVariables.h"
#include "includeFileAnalyzer.h"
#include "data.h"

#include "parser/parser.h"
#include "parser/value.h"

using namespace json11;
using namespace std;
using namespace watagashi;

static data::Compiler createClangCppCompiler();
static data::Compiler createGccCppCompiler();

ExceptionHandlerSetter exceptionHandlerSetter;

int main(int argn, char** args)
{
    try {
        auto options = watagashi::ProgramOptions();
        if (!options.parse(argn, args)) {
            return 0;
        }

        parser::Value projectDefined;
        projectDefined = parser::ObjectDefined();
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

        parser::Value directoryDefiend = parser::ObjectDefined();
        directoryDefiend.addMember("path", parser::MemberDefined(parser::Value::Type::String, parser::Value::none));
        directoryDefiend.addMember("extensions", parser::MemberDefined(parser::Value::Type::Array, parser::Value::none));
        directoryDefiend.addMember("ignores", parser::MemberDefined(parser::Value::Type::Array, parser::Value::none));

        parser::Value fileFiltersDefined = parser::ObjectDefined();
        fileFiltersDefined.addMember("filepath", parser::MemberDefined(parser::Value::Type::String, parser::Value::none));
        fileFiltersDefined.addMember("compileOptions", parser::MemberDefined(parser::Value::Type::Array, parser::Value::array()));
        fileFiltersDefined.addMember("includeDirectories", parser::MemberDefined(parser::Value::Type::Array, parser::Value::array()));
        fileFiltersDefined.addMember("preprocess", parser::MemberDefined(parser::Value::Type::Array, parser::Value::array()));
        fileFiltersDefined.addMember("postprocess", parser::MemberDefined(parser::Value::Type::Array, parser::Value::array()));

        parser::ParserDesc desc;
        desc.externObj.addMember("Project", projectDefined);
        desc.externObj.addMember("Directory",directoryDefiend);
        desc.externObj.addMember("FileFilter", fileFiltersDefined);

        auto value = parser::parse(boost::filesystem::path("../newConfig.watagashi"), desc);
        parser::confirmValueInInteractive(value);

        if (false) {
            data::Project project;
            project.name = "test";
            project.type = data::Project::Type::Exe;
            project.compiler = "clang++";
            project.outputName = project.name;
            project.outputPath = "output";
            project.intermediatePath = "intermediate";
            project.preprocess = "echo Preprocess";
            project.linkPreprocess = "echo run Link Preprocess";
            project.postprocess = "echo run Postprocess";
            project.rootDirectory = "../";
            project.targets = {
                "testSrc/main.cpp",
                "testSrc/A.cpp",
            };

            Builder_ builder(project, options);
            builder.addCompiler(createClangCppCompiler());
            builder.addCompiler(createGccCppCompiler());
            builder.rebuild();
            builder.listupFiles();
        }
    } catch (...) {
        ExceptionHandlerSetter::terminate();
        return 1;
    }

    return 0;
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