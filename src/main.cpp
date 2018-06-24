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

#include "parser.h"

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
        if(!options.parse(argn, args)) {
            return 0;
        }

        parser::parse(boost::filesystem::path("../newConfig.watagashi"));

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