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

using namespace json11;
using namespace std;
using namespace watagashi;

void createConfigFile(const watagashi::ProgramOptions& opts);
static data::Compiler createClangCppCompiler();
static data::Compiler createGccCppCompiler();

ExceptionHandlerSetter exceptionHandlerSetter;

int main(int argn, char** args)
{
    auto pOpts = std::make_shared<watagashi::ProgramOptions>();
    try {
        if(!pOpts->parse(argn, args)) {
            return 0;
        }
        
        if(watagashi::ProgramOptions::eTASK_CREATE == pOpts->taskType()) {
            createConfigFile(*pOpts);
            return 0;
        }

        {
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

            Builder_ builder(project, *pOpts);
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

    const auto& configFilepath = pOpts->configFilepath;
    std::string json_str = watagashi::readFile(configFilepath);
    if(json_str.empty()) {
        cerr << "failed reading \"" << configFilepath << "\"" << endl;
        return 1;
    }
    
    std::string error;
    auto configJson = Json::parse(json_str, error);
    if(!error.empty()) {
        cerr << "failed parse json \"" << configFilepath << "\"" << endl;
        cerr << error << endl;
        return 1;
    }

    // parse
    auto pRootConfig = std::make_shared<watagashi::config::RootConfig>();
    //watagashi::config::parse(*pRootConfig, configJson);
    if(!watagashi::config::json::parse(*pRootConfig, configJson)){
        return 1;
    }
    
    watagashi::Builder builder;
    switch(pOpts->taskType()) {
    case watagashi::ProgramOptions::eTASK_BUILD:
        builder.build(pRootConfig, pOpts);
        break;
    case watagashi::ProgramOptions::eTASK_CLEAN:
        builder.clean(pRootConfig, pOpts);
        break;
    case watagashi::ProgramOptions::eTASK_REBUILD:
        builder.clean(pRootConfig, pOpts);
        builder.build(pRootConfig, pOpts);
        break;
    case watagashi::ProgramOptions::eTASK_LISTUP_FILES:
        builder.listupFiles(pRootConfig, pOpts);
        break;
    case watagashi::ProgramOptions::eTASK_INSTALL:
        builder.install(pRootConfig, pOpts);
        break;
    case watagashi::ProgramOptions::eTASK_SHOW_PROJECTS:
        builder.showProjects(pRootConfig, pOpts);
        break;
    default:
        AWESOME_THROW(std::runtime_error) << "unknown task type.";
    }

    return 0;
}

void createConfigFile(const watagashi::ProgramOptions& opts)
{
    using namespace watagashi;
    namespace fs = boost::filesystem;
    
    config::RootConfig rootConfig;
    config::Project project;
    config::BuildSetting buildSetting;
    buildSetting.targetDirectories.push_back({});
    auto& targetDir = buildSetting.targetDirectories[0];
    targetDir.fileFilters.push_back({});
    auto& fileFilter = targetDir.fileFilters[0];

    {
        
        cout << "project name > ";
        cin >> project.name;
        do {
            cout << "project type (exe,static,shared) > ";
            std::string str;
            cin >> str;
            project.type = project.toType(str);
            if(config::Project::eUnknown == project.type) {
                cerr << "unknown type input..." << endl;
            }
        } while(config::Project::eUnknown == project.type);
    }
    {
        cout << "buildSetting name > ";
        cin >> buildSetting.name;
        buildSetting.outputConfig.name = project.name;
        cout << "use compiler > ";
        cin >> buildSetting.compiler;
    }
    {
        cout << "target directory > ";
        cin >> targetDir.path;
    }        
    {
        do {
            cout << "target extension (exit by enter '.') > ";    
            std::string str;
            cin >> str;
            if(str == ".") break;
            fileFilter.extensions.insert(str);
        } while(true);
    }
    project.buildSettings.insert({buildSetting.name, buildSetting});
    rootConfig.projects.insert({project.name, project});

/*    
    json11::Json output;
    if( !rootConfig.dump(output) ) {
        std::runtime_error("failed to dump config file...");
    }
*/
    auto output = watagashi::config::json::dump(rootConfig);
    cout << R"(config filepath (suffix ".watagashi") > )";
    boost::filesystem::path configFilepath;
    cin >> configFilepath;
    configFilepath += ".watagashi";
    if(fs::exists(configFilepath)) {
        cout << configFilepath << "is exsit. do override? (Y/N) >";
        std::string c;
        cin >> c;
        if(::tolower(c[0]) != 'y') {
            cout << "suspension create config file." << endl;
            return ;
        }
    }
    
    std::ofstream out(configFilepath.string());
    if(out.fail()){
        std::runtime_error("failed to write config file...");
    }
    out << output.dump();
    out.close();
    
    cout << "complete create config file." << endl;
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