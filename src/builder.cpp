#include "builder.h"

#include <thread>
#include <iostream>
#include <functional>
#include <regex>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/adaptor/map.hpp>

#include "exception.hpp"
#include "config.h"
#include "programOptions.h"
#include "utility.h"
#include "includeFileAnalyzer.h"
#include "specialVariables.h"
#include "buildEnviroment.h"
#include "processServer.h"

#include "data.h"

using namespace std;
namespace fs = boost::filesystem;

namespace watagashi
{

struct MakeFileAndFilterPair
{
    const config::TargetDirectory& targetDir;

    MakeFileAndFilterPair(const config::TargetDirectory& targetDir)
        : targetDir(targetDir)
    {}

    struct Pair {
        const fs::directory_entry& dirEntry;
        const config::FileFilter* pFileFilter;

        Pair(
            const fs::directory_entry& dirEntry,
            const config::FileFilter* pFileFilter)
            : dirEntry(dirEntry)
            , pFileFilter(pFileFilter)
        {}

        fs::path makeOutputFilepath(const fs::path& directory)const
        {
            auto outputFilepath =
                (fs::canonical(directory, fs::initial_path())
                    / dirEntry.path().filename())
                .replace_extension(".o");
            outputFilepath = fs::relative(outputFilepath, fs::initial_path());
            return outputFilepath;
        }
    };

    Pair operator()(const fs::directory_entry& dirEntry)const
    {
        auto p = dirEntry.path();
        auto pFilter = this->targetDir.getFilter(p);
        return Pair(dirEntry, pFilter);
    }
};

struct TargetFilter
{
    const config::TargetDirectory& targetDir;
    bool enableLog = true;
    fs::path currentPath;
    std::string testRegexPattern; // for --check-regex option of 'listup' task    

    TargetFilter(const config::TargetDirectory& targetDir, const fs::path& currentPath)
        : targetDir(targetDir)
        , currentPath(currentPath)
        , testRegexPattern("")
    {}

    bool operator()(const fs::directory_entry& entry)const {
        auto p = entry.path();
        auto pFilter = this->targetDir.getFilter(p);
        auto pair = MakeFileAndFilterPair::Pair(entry, pFilter);
        return (*this)(pair);
    }

    bool operator()(const MakeFileAndFilterPair::Pair& pair)const {
        if (nullptr == pair.pFileFilter) {
            return false;
        }

        auto p = pair.dirEntry.path();
        if (this->targetDir.isIgnorePath(p, this->currentPath)) {
            if (this->enableLog) {
                cout << "ignore " << fs::relative(p, fs::initial_path()) << endl;
            }
            return false;
        }

        if (this->testRegexPattern.empty()) {
            return true;
        } else {
            // for --check-regex option of 'listup' task    
            std::regex r(this->testRegexPattern);
            auto relativePath = fs::relative(p, this->currentPath);
            if (std::regex_search(relativePath.string(), r)) {
                return true;
            }
            return false;
        }
    }
};

void Builder::build(
    const std::shared_ptr<config::RootConfig>& pRootConfig,
    const std::shared_ptr<ProgramOptions>& pOpt)
{
    BuildEnviroment env;
    env.init(pRootConfig, pOpt);

    env.checkDependenceProjects();

    auto& project = env.project();
    auto& buildSetting = env.buildSetting();

    buildSetting.exePreprocess();

    bool isFinish = false;
    ProcessServer processServer;
    std::vector<std::thread> threads;
    threads.resize(pOpt->threadCount - 1);
    for (auto&& t : threads) {
        t = std::thread([&]() {
            while (!isFinish) {
                if (auto pProcess = processServer.serveProcess()) {
                    auto result = pProcess->compile();
                    processServer.notifyEndOfProcess(result);
                    if (Process::BuildResult::eFailed == result) {
                        isFinish = true;
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        });
    }

    Finally fin([&]() {
        isFinish = true;
        for (auto&& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    });

    std::vector<std::string> linkTargets;
    for (auto&& rawTargetDir : buildSetting.targetDirectories) {
        auto targetDirEnv = env;
        targetDirEnv.setupTargetDirectory(rawTargetDir);

        auto r = buildInDirectory(linkTargets, targetDirEnv, processServer);
        if (BuildResult::eFAILED == r) {
            break;
        }
    }

    while (!isFinish) {
        auto pProcess = processServer.serveProcess();
        if (nullptr == pProcess) {
            break;
        }
        auto result = pProcess->compile();
        processServer.notifyEndOfProcess(result);
        if (Process::BuildResult::eFailed == result) {
            break;
        }
    }
    isFinish = true;

    for (auto&& t : threads) {
        t.join();
    }
    threads.clear();

    auto outputPath = buildSetting.makeOutputFilepath(env.configFilepath(), project);
    bool isLink = (0 == processServer.failedCount() && 1 <= processServer.successCount());
    isLink |= !fs::exists(outputPath);
    if (isLink) {
        this->linkObjs(env, linkTargets);
        buildSetting.exePostprocess();
    } else {
        cout << "skip link" << endl;
    }

    env.clear();
}

void Builder::clean(
    const std::shared_ptr<config::RootConfig>& pRootConfig,
    const std::shared_ptr<ProgramOptions>& pOpt)
{
    BuildEnviroment env;
    env.init(pRootConfig, pOpt);

    auto& project = env.project();
    auto& buildSetting = env.buildSetting();

    auto& outputConfig = buildSetting.outputConfig;

    auto intermediatePath = buildSetting.makeIntermediatePath(
        pOpt->configFilepath,
        project);
    if (fs::exists(intermediatePath)) {
        fs::remove_all(intermediatePath);
    }

    auto outputPath = buildSetting.makeOutputFilepath(
        pOpt->configFilepath,
        project);
    if (fs::exists(outputPath)) {
        fs::remove_all(outputPath);
    }
}

void Builder::listupFiles(
    const std::shared_ptr<config::RootConfig>& pRootConfig,
    const std::shared_ptr<ProgramOptions>& pOpt)
{
    BuildEnviroment env;
    env.init(pRootConfig, pOpt);

    auto& project = env.project();
    auto& buildSetting = env.buildSetting();

    fs::path currentPath = pOpt->configFilepath;
    currentPath = currentPath.parent_path();
    for (auto& rawTargetDir : buildSetting.targetDirectories) {
        BuildEnviroment targetDirEnv = env;
        targetDirEnv.setupTargetDirectory(rawTargetDir);
        auto& targetDir = targetDirEnv.targetDirectory();

        fs::path srcDirectory = fs::relative(fs::canonical(currentPath / targetDir.path), fs::initial_path());

        TargetFilter filterFile(targetDir, currentPath);
        filterFile.enableLog = false;
        filterFile.testRegexPattern = pOpt->regexPattern;
        boost::copy(
            fs::recursive_directory_iterator(srcDirectory)
            | boost::adaptors::filtered(filterFile)
            | boost::adaptors::transformed([&](const fs::path& p) { return p.string(); })
            , std::ostream_iterator<std::string>(cout, "\n"));
    }
}

void Builder::install(
    const std::shared_ptr<config::RootConfig>& pRootConfig,
    const std::shared_ptr<ProgramOptions>& pOpt)
{
    BuildEnviroment env;
    env.init(pRootConfig, pOpt);

    auto& project = env.project();
    auto& buildSetting = env.buildSetting();

    auto outputPath = buildSetting.makeOutputFilepath(
        pOpt->configFilepath,
        project);

    if (!fs::exists(outputPath)) {
        AWESOME_THROW(std::runtime_error) << "Failed to install... Don't exist \"" << outputPath.string() << "\".";
    }

    fs::path installPath;
    if (pOpt->installPath.empty()) {
        installPath = "/usr/local/bin";
        installPath /= outputPath.filename();
    } else {
        installPath = pOpt->installPath;
    }
    installPath = fs::absolute(installPath);
    if (!fs::exists(installPath.parent_path())) {
        fs::create_directories(installPath);
    }

    fs::copy_file(outputPath, installPath, fs::copy_option::overwrite_if_exists);
}

void Builder::showProjects(
    const std::shared_ptr<config::RootConfig>& pRootConfig,
    const std::shared_ptr<ProgramOptions>& pOpt)
{
    boost::for_each(pRootConfig->projects
        | boost::adaptors::map_values
        , [](const config::Project& project) {
        cout << project.name << endl;
        boost::for_each(project.buildSettings
            | boost::adaptors::map_values
            , [](const config::BuildSetting& buildSetting) {
            cout << "-- " << buildSetting.name << endl;
        });
    });
}

Builder::BuildResult Builder::buildInDirectory(
    std::vector<std::string>& outLinkTargets,
    BuildEnviroment& env,
    ProcessServer& processServer)
{
    auto& project = env.project();
    auto& buildSetting = env.buildSetting();
    auto& targetDir = env.targetDirectory();
    if (targetDir.fileFilters.empty()) {
        AWESOME_THROW(std::runtime_error) << "Don't exsit fileFilter in TargetDirectory \"" << targetDir.path << "\"";
    }

    auto configFileDir = env.configFileDirectory();
    fs::path srcDirectory(configFileDir / targetDir.path);
    TargetFilter filterTarget(targetDir, configFileDir);

    auto& standardCompileOptions = buildSetting.compileOptions;

    auto intermediatePath = buildSetting.makeIntermediatePath(
        env.configFilepath(), project);

    bool isFailedCompile = false;
    bool isLink = false;
    auto buildFile = [&](const MakeFileAndFilterPair::Pair& pair) {
        auto path = fs::relative(pair.dirEntry.path(), fs::initial_path());
        auto outputFilepath = pair.makeOutputFilepath(intermediatePath);
        outLinkTargets.push_back(outputFilepath.string());

        auto targetEnv = env;
        Finally fin([&]() {
            targetEnv.clearFileFilter();
        });

        targetEnv.setupFileFilter(*pair.pFileFilter);
        if (auto* pFileToProcess = pair.pFileFilter->findFileToProcessPointer(path, srcDirectory)) {
            targetEnv.setupFileToProcess(*pFileToProcess);
        }
        SpecialVariables::TargetEnviromentScope::InitDesc desc;
        desc.inputFilepath = path;
        desc.outputFilepath = outputFilepath;
        targetEnv.setupTargetEnviroment(desc);

        auto pProcess = std::make_unique<Process>();
        pProcess->env = env;
        pProcess->inputFilepath = path;
        pProcess->outputFilepath = outputFilepath;
        processServer.addProcess(std::move(pProcess));
    };

    try {
        auto intermediatePath = buildSetting.makeIntermediatePath(env.configFilepath(), project);
        if (!createDirectory(intermediatePath)) {
            AWESOME_THROW(std::runtime_error)
                << "Failed to create intermediate directory. path=" << intermediatePath.string();
        }

        boost::for_each(
            fs::recursive_directory_iterator(srcDirectory)
            | boost::adaptors::transformed(MakeFileAndFilterPair(targetDir))
            | boost::adaptors::filtered(filterTarget)
            , buildFile);
        if (isFailedCompile) {
            return BuildResult::eFAILED;
        } else {
            return isLink ? BuildResult::eSUCCESS : BuildResult::eSKIP_LINK;
        }
    } catch (fs::filesystem_error& e) {
        cerr << e.what() << endl;
        cerr << "Stop build in " << srcDirectory << "." << endl;
        return BuildResult::eFAILED;
    } catch (std::runtime_error& e) {
        cerr << e.what() << endl;
        cerr << "Stop build in " << srcDirectory << "." << endl;
        return BuildResult::eFAILED;
    }
}

void Builder::linkObjs(
    BuildEnviroment& env,
    const std::vector<std::string>& linkTargets)
{
    auto& project = env.project();
    auto& buildSetting = env.buildSetting();

    //fs::path currentPath = buildDesc.programOptions.configFilepath;
    auto configFilepath = env.configFilepath();

    auto outputPath = buildSetting.makeOutputFilepath(configFilepath, project);

    buildSetting.exeLinkPreprocess();

    SpecialVariables::TargetEnviromentScope::InitDesc desc;
    desc.inputFilepath = "";
    desc.outputFilepath = outputPath;
    env.setupTargetEnviroment(desc);
    Finally fin([&]() {
        env.clearTargetEnviroment();
    });

    if (!env.preprocessCompiler(BuildEnviroment::TaskType::eLink, "", outputPath)) {
        return;
    }
    auto cmdStr = env.createLinkCommand(linkTargets, outputPath);
    cout << cmdStr << endl;
    auto result = std::system(cmdStr.c_str());
    if (0 == result) {
        env.postprocessCompiler(BuildEnviroment::TaskType::eLink, "", outputPath);
    }

}

Builder_::Builder_(data::Project const& project, ProgramOptions const& options)
    : mProject(project)
    , mOptions(options)
{}

void Builder_::addCompiler(data::Compiler const& compiler)
{
    auto it = this->mCompilerMap.find(compiler.name);
    if (this->mCompilerMap.end() != it) {
        AWESOME_THROW(std::runtime_error)
            << "Don't add compiler of the same name. name='" << compiler.name << "'";
    }

    this->mCompilerMap.insert({ compiler.name, compiler });
}

void Builder_::addCompiler(data::Compiler && compiler)
{
    auto it = this->mCompilerMap.find(compiler.name);
    if (this->mCompilerMap.end() != it) {
        AWESOME_THROW(std::runtime_error)
            << "Don't add compiler of the same name. name='" << compiler.name << "'";
    }
    this->mCompilerMap.insert({ compiler.name, std::move(compiler) });
}

void runThread(bool& isFinish, ProcessServer& processServer, bool isMainThread)
{
    while (!isFinish) {
        if (auto pProcess = processServer.serveProcess_()) {
            auto result = pProcess->compile();
            processServer.notifyEndOfProcess(result);
            if (Process_::BuildResult::Failed == result) {
                isFinish = true;
            }
        } else {
            if (isMainThread) {
                isFinish = true;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }
}

void Builder_::build()const
{
    cout << fs::initial_path() << endl;

    if (this->mCompilerMap.count(this->mProject.compiler) <= 0) {
        cerr << "Don't exsit '"<< this->mProject.compiler << "' compiler." << endl;
        return;
    }
    auto& compiler = this->mCompilerMap.find(this->mProject.compiler)->second;
    auto& taskBundle = data::getTaskBundle(compiler, this->mProject.type);

    if (!runCommand(this->parseVariables(this->mProject.postprocess, Scope()))) {
        cerr << "Failed preprocess..." << endl;
        return;
    }

    bool isFinish = false;
    ProcessServer processServer;
    std::vector<std::thread> threads(this->mOptions.threadCount - 1);
    for (auto&& t : threads) {
        t = std::thread(runThread, std::ref(isFinish), std::ref(processServer), false);
    }
    Finally fin([&]() {
        isFinish = true;
        for (auto&& t : threads) {
            if (t.joinable()) { t.join(); }
        }
    });

    createDirectory(this->mProject.makeIntermediatePath());
    std::vector<fs::path> linkTargets;
    linkTargets.reserve(this->mProject.targets.size());
    for (auto& target : this->mProject.targets) {
        auto outputFilepath = this->mProject.makeIntermediatePath(target).replace_extension(".o");
        auto pProcess = std::make_unique<Process_>(*this, compiler, this->mProject.rootDirectory/target, outputFilepath);
        processServer.addProcess(std::move(pProcess));

        linkTargets.push_back(outputFilepath);
    }

    runThread(isFinish, processServer, true);
    for (auto&& t : threads) {
        t.join();
    }
    threads.clear();

    auto outputPath = this->mProject.makeOutputFilepath();
    bool isLink = (0 == processServer.failedCount() && 1 <= processServer.successCount());
    isLink |= !fs::exists(outputPath);
    if (isLink) {
        auto outputFilepath = this->mProject.makeOutputFilepath();
        createDirectory(outputFilepath.parent_path());
        Scope scope;
        scope.outputFilepath = outputFilepath;
        if (!runCommand(this->parseVariables(this->mProject.linkPreprocess, scope))) {
            cerr << "Failed link preprocess..." << endl;
        }

        auto linkCmd = data::makeLinkCommand(taskBundle.linkObjs, outputFilepath, linkTargets, this->mProject);
        cout << "running: " << linkCmd << endl;
        if (!runCommand(linkCmd)) {
            cerr << "Failed to link." << endl;
            return;
        }
    } else {
        cout << "skip link" << endl;
    }

    if (!runCommand(this->parseVariables(this->mProject.postprocess, Scope()))) {
        cerr << "Failed postprocess..." << endl;
    }
}

void Builder_::clean()const
{
    boost::system::error_code ec;
    {// remove all intermediate files
        auto path = this->mProject.makeIntermediatePath();
        if (fs::exists(path)) {
            fs::remove_all(path, ec);
            if (boost::system::errc::success != ec) {
                AWESOME_THROW(std::runtime_error)
                    << "Failed to remove intermediate directory";
            }
        }
    }
    {// remove output file
        auto filepath = this->mProject.makeOutputFilepath();
        filepath;
        if (fs::exists(filepath)) {
            fs::remove_all(filepath, ec);
            if (boost::system::errc::success != ec) {
                AWESOME_THROW(std::runtime_error)
                    << "Failed to remove output file";
            }
        }
    }

    cout << "success clean task" << endl;
}

void Builder_::rebuild()const
{
    this->clean();
    this->build();
}

void Builder_::listupFiles()const
{
    for (auto& target : this->mProject.targets) {
        cout << target << endl;
    }
}

void Builder_::install()const
{
    AWESOME_THROW(std::runtime_error)
        << "don't yet implement...";
}

data::Project const& Builder_::project()const
{
    return this->mProject;
}

ProgramOptions const& Builder_::options()const
{
    return this->mOptions;
}

std::unordered_set<std::string> findVariables(const std::string& str)
{
    std::unordered_set<std::string> result;
    if (std::string::npos == str.find('$')) {
        return result;
    }
    std::regex pattern(R"(\$\{([a-zA-Z0-9_.]+)\})");
    std::smatch match;
    auto it = str.cbegin(), end = str.cend();
    while (std::regex_search(it, end, match, pattern)) {
        result.insert(match[1]);
        it = match[0].second;
    }
    return result;
}

std::unordered_map<std::string, std::function<std::string(Builder_::Scope const&scope)>> const evalVariableMap =
{
    { "OUTPUT_FILE_NAME", [](Builder_::Scope const&scope){ return scope.outputFilepath.stem().string(); } },
    { "OUTPUT_FILE_DIR",  [](Builder_::Scope const&scope) { return scope.outputFilepath.parent_path().string(); } },
};

std::string Builder_::parseVariables(std::string const& str, Scope const& scope)const
{
    auto variables = findVariables(str);
    auto result = str;
    for (auto const& var : variables) {
        std::string value = "";
        auto it = evalVariableMap.find(var);
        if (evalVariableMap.end() == it) {
            cerr << "Miss '" << var << "'..." << endl;
        }
        value = it->second(scope);

        auto keyward = "${" + var + "}";
        std::string::size_type pos = 0;
        while (std::string::npos != (pos = result.find(keyward, pos))) {
            result.replace(pos, keyward.size(), value);
            pos += keyward.size();
        }
    }
    return result;
}

data::Compiler const& Builder_::getCompiler(std::string const& name)const
{
    auto it = this->mCompilerMap.find(name);
    if (this->mCompilerMap.end() == it) {
        AWESOME_THROW(std::runtime_error)
            << "Not found '" << name << "' compiler...";
    }
    return it->second;
}

}
