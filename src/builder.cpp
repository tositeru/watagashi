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
#include "programOptions.h"
#include "utility.h"
#include "includeFileAnalyzer.h"
#include "processServer.h"

#include "data.h"

using namespace std;
namespace fs = boost::filesystem;

namespace watagashi
{

Builder::Builder(data::Project const& project, ProgramOptions const& options)
    : mProject(project)
    , mOptions(options)
{}

void Builder::addCompiler(data::Compiler const& compiler)
{
    auto it = this->mCompilerMap.find(compiler.name);
    if (this->mCompilerMap.end() != it) {
        AWESOME_THROW(std::runtime_error)
            << "Don't add compiler of the same name. name='" << compiler.name << "'";
    }

    this->mCompilerMap.insert({ compiler.name, compiler });
}

void Builder::addCompiler(data::Compiler && compiler)
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
            if (Process::BuildResult::Failed == result) {
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

void Builder::build()const
{
    cout << fs::initial_path() << endl;

    if (this->mCompilerMap.count(this->mProject.compiler) <= 0) {
        cerr << "Don't exsit '"<< this->mProject.compiler << "' compiler." << endl;
        return;
    }
    auto& compiler = this->mCompilerMap.find(this->mProject.compiler)->second;
    auto& taskBundle = data::getTaskBundle(compiler, this->mProject.type);

    if (!runCommand(this->parseVariables(this->mProject.preprocess, Scope()))) {
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
        auto pProcess = std::make_unique<Process>(*this, compiler, this->mProject.rootDirectory/target, outputFilepath);
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
    isLink |= !fs::exists(outputPath) && processServer.failedCount() <= 0;
    if (isLink) {
        auto outputFilepath = this->mProject.makeOutputFilepath();
        createDirectory(outputFilepath.parent_path());
        Scope scope;
        scope.outputFilepath = outputFilepath;
        if (!runCommand(this->parseVariables(this->mProject.linkPreprocess, scope))) {
            cerr << "Failed link preprocess..." << endl;
            return;
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
        return;
    }
    cout << "!! Complete build !!" << endl;
}

void Builder::clean()const
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

void Builder::rebuild()const
{
    this->clean();
    this->build();
}

void Builder::listupFiles()const
{
    for (auto& target : this->mProject.targets) {
        cout << target << endl;
    }
}

void Builder::install()const
{
    AWESOME_THROW(std::runtime_error)
        << "don't yet implement...";
}

data::Project const& Builder::project()const
{
    return this->mProject;
}

ProgramOptions const& Builder::options()const
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

std::unordered_map<std::string, std::function<std::string(Builder::Scope const&scope)>> const evalVariableMap =
{
    { "OUTPUT_FILE_NAME", [](Builder::Scope const&scope){ return scope.outputFilepath.stem().string(); } },
    { "OUTPUT_FILE_DIR",  [](Builder::Scope const&scope) { return scope.outputFilepath.parent_path().string(); } },
};

std::string Builder::parseVariables(std::string const& str, Scope const& scope)const
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

data::Compiler const& Builder::getCompiler(std::string const& name)const
{
    auto it = this->mCompilerMap.find(name);
    if (this->mCompilerMap.end() == it) {
        AWESOME_THROW(std::runtime_error)
            << "Not found '" << name << "' compiler...";
    }
    return it->second;
}

}
