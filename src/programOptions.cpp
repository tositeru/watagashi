#include "programOptions.h"
 
#include <iostream>
#include <vector>
#include <sstream>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/range/algorithm/transform.hpp>

#include "utility.h"

using namespace std;
namespace fs = boost::filesystem;

namespace watagashi
{

bool ProgramOptions::parse(int argv, char** args)
{
    namespace po = boost::program_options;
    
    try {
        std::vector<std::string> variables;
        
        po::options_description installOptions(
            R"("install" task options)" "\n"
            R"(this task copy output file to specified path by --install-path)" "\n"
            R"(Usage) watagasi install -p <project name>)"
        );
        installOptions.add_options()
            ("output,o", po::value<std::string>(&this->installPath), "copy to path.")
        ;
        po::options_description listupOptions(
            R"("listup" task options)" "\n"
            R"(this task list up build target files.)" "\n"
            R"(Usage) watagsi listup -p <project name>)"
        );
        po::options_description all(
            R"(Watagasi is negative list based c/c++ build system.)" "\n"
            R"(Usage) watagasi <task> [options])" "\n"
            R"(common options)"
        );
        all.add_options()
            ("help,h", "show this.")
            ("task", po::value<std::string>(&this->task)->default_value("build"), R"(run task. choose to "build", "clean", "rebuild", "listup", "show", "install" or "interactive".)")
            ("config,c", po::value<std::string>(&this->configFilepath)->default_value("build.watagashi"), "use config file.")
            ("project,p", po::value<std::string>(&this->targetProject), "target project name")
            ("thread-count,t", po::value<int>(&this->threadCount)->default_value(1), "thread count.")
            ("variable,V", po::value<std::vector<std::string>>(&variables), "define variable. this option can be multiple. the defined variable is applied with the highest priority.")
        ;
        all.add(installOptions)
            .add(listupOptions);
        
        po::positional_options_description p;
        p.add("task", 1);
        
        po::variables_map vm;
        po::store(po::command_line_parser(argv, args)
            .options(all).positional(p).run(), vm);
        po::notify(vm);
        
        if(vm.count("help")) {
            cout << all << endl;
            return false;
        }
        
        if (!vm.count("project")
            && TaskType::ShowProjects != this->taskType()
            && TaskType::Interactive != this->taskType()) {
            cerr << "error: must specify --project(-p) option" << endl;
            return false;
        }
        
        boost::range::transform(this->task, this->task.begin(), [](char c){ return static_cast<char>(::tolower(c)); });
        
        this->rootDirectories = fs::path(this->configFilepath).parent_path().string();
        if(this->rootDirectories.empty()) {
            this->rootDirectories = "./";
        }
        
        this->userDefinedVaraibles.clear();
        if(!variables.empty()) {
            for(auto&& v : variables) {
                auto strings = split(v, '=');
                if(strings.size() < 2) {
                    cerr << "found invalid variable in command line.";
                    if(1 == strings.size()) cerr << " name=" << strings[0];
                    cerr << endl;
                    continue;
                }
                this->userDefinedVaraibles.insert({strings[0], strings[1]});
            }
        }

    } catch(std::exception& e) {
        cerr << e.what() << endl;
        cerr << "Failed parse program options..." << endl;
        return false;
    } catch(...) {
        cerr << "Failed parse program options for unknown reasion..." << endl;
        return false;
    }
    return true;
}

ProgramOptions::TaskType ProgramOptions::taskType()const
{
    static const std::unordered_map<std::string, TaskType> sTable = {
        {"build", TaskType::Build},
        {"clean", TaskType::Clean},
        {"rebuild", TaskType::Rebuild},
        {"listup", TaskType::ListupFiles},
        {"install", TaskType::Install},
        {"show", TaskType::ShowProjects},
        {"interactive", TaskType::Interactive}
    };
    auto it = sTable.find(this->task);
    if(sTable.end() == it) {
        return TaskType::Unknown;
    }
    return it->second;
}

}
