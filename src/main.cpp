#include <iostream>

#include <boost/filesystem.hpp>

#include "utility.h"
#include "config.h"
#include "builder.h"
#include "programOptions.h"

#include "specialVariables.h"

using namespace json11;
using namespace std;

void createConfigFile(const watagasi::ProgramOptions& opts);

#include "includeFileAnalyzer.h"

int main(int argn, char** args)
{
	auto pOpts = std::make_shared<watagasi::ProgramOptions>();
	try {
		if(!pOpts->parse(argn, args)) {
			return 0;
		}
		
		if(watagasi::ProgramOptions::eTASK_CREATE == pOpts->taskType()) {
			createConfigFile(*pOpts);
			return 0;
		}
	} catch(std::exception& e) {
		cerr << e.what() << endl;
		return 1;
	}	
	
	const auto& configFilepath = pOpts->configFilepath;
	std::string json_str = watagasi::readFile(configFilepath);
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
	try {
		auto pRootConfig = std::make_shared<watagasi::config::RootConfig>();
		watagasi::config::parse(*pRootConfig, configJson); 
		
		watagasi::Builder builder;
		switch(pOpts->taskType()) {
		case watagasi::ProgramOptions::eTASK_BUILD:
			builder.build(pRootConfig, pOpts);
			break;
		case watagasi::ProgramOptions::eTASK_CLEAN:
			builder.clean(pRootConfig, pOpts);
			break;
		case watagasi::ProgramOptions::eTASK_REBUILD:
			builder.clean(pRootConfig, pOpts);
			builder.build(pRootConfig, pOpts);
			break;
		case watagasi::ProgramOptions::eTASK_LISTUP_FILES:
			builder.listupFiles(pRootConfig, pOpts);
			break;
		case watagasi::ProgramOptions::eTASK_INSTALL:
			builder.install(pRootConfig, pOpts);
			break;
		case watagasi::ProgramOptions::eTASK_SHOW_PROJECTS:
			builder.showProjects(pRootConfig, pOpts);
			break;
		default:
			throw std::runtime_error("unknown task type.");
		}
	} catch(std::runtime_error& e) {
		cerr << e.what() << endl;
		return 1;
	} catch(std::invalid_argument& e) {
		cerr << "Application error: " << e.what() << endl;
		return 1;
	} catch(std::exception& e) {
		cerr << "Unknown exception: " << e.what() << endl;
		return 1;
	}

	return 0;
}

void createConfigFile(const watagasi::ProgramOptions& opts)
{
	using namespace watagasi;
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
	
	json11::Json output;
	if( !rootConfig.dump(output) ) {
		std::runtime_error("failed to dump config file...");
	}
	
	cout << R"(config filepath (suffix ".watagashi") > )";
	boost::filesystem::path configFilepath;
	cin >> configFilepath;
	configFilepath += ".watagashi";
	if(fs::exists(configFilepath)) {
		cout << configFilepath << "is exsit. do override? (Y/N) >";
		std::string c;
		cin >> c;
		if(std::tolower(c[0]) != 'y') {
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
