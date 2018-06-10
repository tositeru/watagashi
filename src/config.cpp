#include "config.h"

#include <iostream>
#include <regex>
#include <boost/filesystem.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/algorithm/transform.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/adaptor/filtered.hpp>

#include "exception.hpp"
#include "errorReceiver.h"
#include "utility.h"
#include "specialVariables.h"

using namespace std;
namespace fs = boost::filesystem;

namespace watagasi::config
{

static void insertToJsonObj(
	json11::Json::object& out,
	const std::string& key,
	const json11::Json& jsonObj,
	bool isInsert)
{
	if(isInsert) {
		out.insert({key, jsonObj});
	}
}

static void exeCommand(
	const std::string& command,
	const std::string& exceptionMessage)
{
	if("" != command) {
		auto ret = std::system(command.c_str());
		if(0 != ret) {
			AWESOME_THROW(std::runtime_error) << exceptionMessage;
		}
	}
}

static bool matchFilepath(
	const std::string& patternStr,
	const fs::path& filepath,
	const fs::path& standardPath)
{
	auto relativeTargetPath = fs::relative(filepath, standardPath);

	if(patternStr.front() == '@') {
		// regular expressions check
		std::regex regex(patternStr.substr(1));
		if( std::regex_search(relativeTargetPath.string(), regex) ) {
			return true;
		}
	} else {
		fs::path patternPath(patternStr);
		patternPath = fs::relative(standardPath/patternPath);
		if(patternStr.back() == '/') {
			//directory check
			auto checkPath = fs::relative(filepath, patternPath);
			if( std::string::npos != checkPath.string().find("../") ) {
				return true;
			}
		} else {
			//filename check
			if( fs::equivalent(patternPath, fs::relative(filepath)) ) {
				return true;
			}
		}
	}
	return false;
}

//******************************************************************************
//
//	IItem
//
//******************************************************************************

bool IItem::parse(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	isOK = this->parseImpl(data, errorReceiver);
	
	this->parseDefineTemplates(data, errorReceiver);
	this->parseDefineVariables(data, errorReceiver);
	
	return isOK;
}

bool IItem::dump(json11::Json& out)
{
	return this->dumpImpl(out);
}

bool IItem::parseDefineTemplates(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	if(json11::Json() == data[IItem::KEY_DEFINE_TEMPLATES]) {
		return true;
	}
	
	if(!this->isArray(IItem::KEY_DEFINE_TEMPLATES, data, errorReceiver)) {
		return false;
	}
	
	auto& templates = data[IItem::KEY_DEFINE_TEMPLATES].array_items();
	this->defineTemplates.clear();
	this->defineTemplates.reserve(templates.size());
	for(auto&& t : templates) {
		TemplateItem item;
		if(!item.parse(t, errorReceiver)) {
			return false;
		}
		this->defineTemplates.push_back(std::move(item));
	}
	return true;
}

bool IItem::parseDefineVariables(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	this->defineVariables.clear();
	
	auto& variablesJson = data[IItem::KEY_DEFINE_VARIABLES];
	if(json11::Json() == variablesJson) {
		return true;
	}
	if(!variablesJson.is_object()) {
		errorReceiver.receive(ErrorReceiver::eError, variablesJson.rowInFile(), "\"" + IItem::KEY_DEFINE_VARIABLES + "\" must be object!");
	}
	
	for(auto&& it : variablesJson.object_items()) {
		if(!it.second.is_string()) {
			errorReceiver.receive(ErrorReceiver::eWarnning, variablesJson.rowInFile(), "\"" + IItem::KEY_DEFINE_VARIABLES + "\" value be must string!");
			continue;
		}
		this->defineVariables.insert({it.first, it.second.string_value()});
	}
	
	return true;
}

const TemplateItem* IItem::findTemplateItem(const std::string& name, TemplateItemType itemType)const
{
	auto it = boost::range::find_if(this->defineTemplates, [&](const TemplateItem& item){
		return item.name == name && item.itemType == itemType;
	});
	return this->defineTemplates.end() == it ? nullptr : &(*it);
}

void IItem::copyIItemMember(const IItem& right)
{
	this->defineTemplates = right.defineTemplates;
	this->defineVariables = right.defineVariables;
}

bool IItem::hasKey(const json11::Json& data, const std::string& key)const
{
	return json11::Json() != data[key];
}

bool IItem::parseInteger(int& out, const std::string& key, const json11::Json& data, ErrorReceiver& errorReceiver, bool enableErrorMessage)
{
	if(!data[key].is_number()) {
		if(enableErrorMessage) {
			errorReceiver.receive(ErrorReceiver::eError, data[key].rowInFile(), "\"" + key + R"(" key must be integer!)");
		}
		return false;
	}
	
	out = data[key].int_value();
	return true;	
}

bool IItem::parseString(
	std::string& out,
	const std::string& key,
	const json11::Json& data,
	ErrorReceiver& errorReceiver,
	bool enableErrorMessage)
{
	if(!data[key].is_string()) {
		if(enableErrorMessage) {
			errorReceiver.receive(ErrorReceiver::eError, data[key].rowInFile(), "\"" + key + R"(" key must be string!)");
		}
		return false;
	}
	
	out = data[key].string_value();
	return true;
}

bool IItem::parseName(std::string& out, const json11::Json& data, ErrorReceiver& errorReceiver)
{
	if(!this->parseString(out, "name", data, errorReceiver)) {
		return false;
	}
	if("" == out) {
		errorReceiver.receive(ErrorReceiver::eError, data["name"].rowInFile(), R"("name" key is not empty)");
		return false;
	}
	return true;
}

bool IItem::isArray(const std::string& key, const json11::Json& data, ErrorReceiver& errorReceiver)
{
	auto& keyValue = data[key];
	if(!keyValue.is_array()) {
		errorReceiver.receive(ErrorReceiver::eError, keyValue.rowInFile(), "\"" + key + R"(" key must be array!)");
		return false;
	}
	return true;
}

bool IItem::parseStringArray(
	std::vector<std::string>& out,
	const std::string& key,
	const json11::Json& data,
	ErrorReceiver& errorReceiver,
	bool enableErrorMessage)
{
	auto& stringArray = data[key];
	if(!stringArray.is_array()) {
		if(enableErrorMessage) {
			errorReceiver.receive(ErrorReceiver::eError, stringArray.rowInFile(), "\"" + key + R"(" key must be array! this element is ignored.)");
		}
		return false;
	}

	auto& items = stringArray.array_items();
	out.clear();
	out.reserve(items.size());
	for(auto& option : items) {
		if(!option.is_string()) {
			errorReceiver.receive(ErrorReceiver::eWarnning, option.rowInFile(), "\"" + key + R"(" element must be string.)");
			continue;
		}
		auto& str = option.string_value();
		if(str.empty()) {
			errorReceiver.receive(ErrorReceiver::eWarnning, option.rowInFile(), R"(Ignores empty string in ")" + key + "\".");
			continue;
		}
		out.push_back(str);
	}
	return true;
}

bool IItem::parseOptionArray(
	std::vector<std::string>& out,
	const std::string key,
	const json11::Json& data,
	ErrorReceiver& errorReceiver)
{
	std::vector<std::string> rawData;
	if(!IItem::parseStringArray(rawData, key, data, errorReceiver)) {
		return false;
	}
	
	out.clear();
	out.reserve(rawData.size());
	for(auto&& dir : rawData) {
		for(auto&& o : split(dir, ' ')) {
			out.emplace_back(o);
		}
	}
	out.shrink_to_fit();
	return true;
}

//******************************************************************************
//
//	FileToProcess
//
//******************************************************************************

bool FileToProcess::parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	if(IItem::hasKey(data, this->KEY_FILEPATH)) {
		isOK &= IItem::parseString(this->filepath, this->KEY_FILEPATH, data, errorReceiver);
	}
	
	IItem::parseString(this->preprocess, this->KEY_PREPROCESS, data, errorReceiver, false);
	IItem::parseString(this->postprocess, this->KEY_POSTPROCESS, data, errorReceiver, false);
	IItem::parseStringArray(this->compileOptions, this->KEY_COMPILE_OPTIONS, data, errorReceiver, false);
	if(IItem::hasKey(data, this->KEY_TEMPLATE)) {
		IItem::parseString(this->templateName, this->KEY_TEMPLATE, data, errorReceiver, false);
	}

	return isOK;
}

bool FileToProcess::dumpImpl(json11::Json& out)
{
	json11::Json::object obj;
	insertToJsonObj(obj, this->KEY_FILEPATH, json11::Json(this->filepath), !this->filepath.empty());
	insertToJsonObj(obj, this->KEY_PREPROCESS, json11::Json(this->preprocess), !this->preprocess.empty());
	insertToJsonObj(obj, this->KEY_POSTPROCESS, json11::Json(this->postprocess), !this->postprocess.empty());
	insertToJsonObj(obj, this->KEY_COMPILE_OPTIONS, json11::Json(this->compileOptions), !this->compileOptions.empty());
	insertToJsonObj(obj, this->KEY_TEMPLATE, json11::Json(this->templateName), !this->templateName.empty());
	out = obj;
	return true;
}

void FileToProcess::evalVariables(const SpecialVariables& variables)
{
	this->filepath = variables.parse(this->filepath);
	this->preprocess = variables.parse(this->preprocess);
	this->postprocess = variables.parse(this->postprocess);

	auto parseString = [&](const std::string& op){
		return variables.parse(op);
	};
	boost::range::transform(this->compileOptions
		, this->compileOptions.begin(), parseString);
	
	this->templateName = variables.parse(this->templateName);
}

void FileToProcess::adaptTemplateItem(const TemplateItem& templateItem)
{
	if(TemplateItemType::eFileToProcess != templateItem.itemType) {
		cerr << "template \"" << templateItem.name << "\" is not fileToProcess type. ignore this template..." << endl;
		return ;
	}
	auto& item = templateItem.fileToProcess();

	// enable append elements
	auto appendElement = [](auto* pOut, const auto& src){
		if(!src.empty()) {
			pOut->reserve(pOut->size() + src.size());
			boost::copy(src, std::back_inserter(*pOut));
		}
	};
	appendElement(&this->compileOptions, item.compileOptions);
	
	// enable override elements
	if(this->filepath.empty() && !item.filepath.empty()) {
		this->filepath = item.filepath;
	}
	if(this->preprocess.empty() && !item.preprocess.empty()) {
		this->preprocess = item.preprocess;
	}
	if(this->postprocess.empty() && !item.postprocess.empty()) {
		this->postprocess = item.postprocess;
	}
	
	// ignore elements
}

void FileToProcess::exePreprocess(const fs::path& filepath)const
{
	exeCommand(this->preprocess, "Failed \"" + filepath.string() + "\" preprocess..");
}

void FileToProcess::exePostprocess(const fs::path& filepath)const
{
	exeCommand(this->postprocess, "Failed \"" + filepath.string() + "\" postprocess..");
}

//
//	struct FileFilter
//
	
bool FileFilter::parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	isOK &= this->parseExtensions(data, errorReceiver);
	isOK &= this->parseFilesToProcess(data, errorReceiver);
	
	if(IItem::hasKey(data, this->KEY_TEMPLATE)) {
		IItem::parseString(this->templateName, this->KEY_TEMPLATE, data, errorReceiver, false);
	}
	return isOK;
}

bool FileFilter::dumpImpl(json11::Json& out)
{
	json11::Json::object obj;
	if(!this->extensions.empty())
	{
		json11::Json::array jsonExts;
		jsonExts.reserve(this->extensions.size());
		for(auto& e : this->extensions) {
			jsonExts.push_back(e);
		}
		obj.insert({FileFilter::KEY_EXTENSIONS, json11::Json(jsonExts)});
	}
	insertToJsonObj(obj, this->KEY_TEMPLATE, json11::Json(this->templateName), !this->templateName.empty());
	out = obj;
	return true;
}

bool FileFilter::checkExtention(const fs::path& path)const
{
	auto ext = path.extension();
	auto str = ext.string();
	return this->extensions.end() != this->extensions.find(str.c_str()+1);
}
	
bool FileFilter::parseExtensions(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	if(!IItem::hasKey(data, FileFilter::KEY_EXTENSIONS)) {
		return true;
	}
	
	if(!IItem::isArray(this->KEY_EXTENSIONS, data, errorReceiver)) {
		return false;
	}
	
	this->extensions.clear();
	for(auto& ext : data[this->KEY_EXTENSIONS].array_items()) {
		if(!ext.is_string()) {
			errorReceiver.receive(ErrorReceiver::eError, ext.rowInFile(), "\"" + this->KEY_EXTENSIONS + R"(" key must be array! this element is ignored.)");
			continue;
		}
		auto& str = ext.string_value();
		if(str.empty()) {
			errorReceiver.receive(ErrorReceiver::eWarnning, ext.rowInFile(), R"(Ignores empty string in ")" + this->KEY_EXTENSIONS + "\".");
			continue;
		}
		this->extensions.insert(str);
	}
	return true;
}

bool FileFilter::parseFilesToProcess(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	if(!IItem::hasKey(data, FileFilter::KEY_FILES_TO_PROCESS)) {
		return true;
	}
	
	if(!IItem::isArray(this->KEY_FILES_TO_PROCESS, data, errorReceiver)) {
		return false;
	}
	
	auto& jsonItems = data[this->KEY_FILES_TO_PROCESS].array_items();
	this->filesToProcess.clear();
	this->filesToProcess.reserve(jsonItems.size());
	boost::for_each(
		jsonItems,
		[&](const json11::Json& item){
			FileToProcess file;
			if(!file.parse(item, errorReceiver)) {
				return;
			}
			this->filesToProcess.push_back(file);
		});
	return true;
}

void FileFilter::evalVariables(const SpecialVariables& variables)
{
	auto parseString = [&](const std::string& op){
		return variables.parse(op);
	};
	decltype(this->extensions) extensions;
	extensions.reserve(this->extensions.size());
	for(auto&& ext : this->extensions) {
		extensions.insert(variables.parse(ext));
	}
	this->extensions = std::move(extensions);
}

void FileFilter::adaptTemplateItem(const TemplateItem& templateItem)
{
	if(TemplateItemType::eFileFilter != templateItem.itemType) {
		cerr << "template \"" << templateItem.name << "\" is not fileFilter type. ignore this template..." << endl;
		return ;
	}
	auto& item = templateItem.fileFilter();

	// enable append elements
	for(auto&& ext : item.extensions) {
		this->extensions.insert(ext);
	}
	
	// enable override elements
	
	// ignore elements
	if(!item.filesToProcess.empty()) {
		cerr << "ignore filesToProcess in FileFilter template." << endl;		
	}
}

const FileToProcess* FileFilter::findFileToProcessPointer(
	const fs::path& filepath,
	const fs::path& currentPath)const
{
	auto it = boost::find_if(this->filesToProcess,
		[&](const FileToProcess& fileToProcess){
			return matchFilepath(fileToProcess.filepath, filepath, currentPath);
		});
	return (this->filesToProcess.end() != it) ? &(*it) : nullptr;
}

//
//	struct TargetDirectory
//
	
bool TargetDirectory::isIgnorePath(const fs::path& targetPath, const fs::path& currentPath)const
{
	auto standardPath = currentPath/this->path;
	
	for(auto&& ignoreStr : this->ignores) {
		if(matchFilepath(ignoreStr, targetPath, standardPath)) {
			return true;
		}
	}
	return false;
}
	
bool TargetDirectory::filterPath(const fs::path& path)const
{
	return nullptr != this->getFilter(path);
}

const FileFilter* TargetDirectory::getFilter(const boost::filesystem::path& path)const
{
	for(auto&& filter : this->fileFilters) {
		if(filter.checkExtention(path))
			return &filter;
	}
	return nullptr;
}

bool TargetDirectory::parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	if(IItem::hasKey(data, this->KEY_PATH)) {
		if(!IItem::parseString(this->path, this->KEY_PATH, data, errorReceiver)){
			return false;
		}
	}
		
	bool isOK = true;
	isOK &= this->parseFileFilters(data, errorReceiver);
	isOK &= this->parseIgnores(data, errorReceiver);

	if(IItem::hasKey(data, this->KEY_TEMPLATE)) {
		IItem::parseString(this->templateName, this->KEY_TEMPLATE, data, errorReceiver, false);
	}
	return isOK;
}

bool TargetDirectory::dumpImpl(json11::Json& out)
{
	json11::Json::object obj;
	insertToJsonObj(obj, this->KEY_PATH, json11::Json(this->path), !this->path.empty());
	insertToJsonObj(obj, this->KEY_IGNORES, json11::Json(this->ignores), !this->ignores.empty());

	if(!this->fileFilters.empty())
	{
		json11::Json::array jsonFilters;
		jsonFilters.reserve(this->fileFilters.size());
		for(auto& filter : this->fileFilters) {
			json11::Json j;
			if(!filter.dump(j)) {
				continue;
			}
			jsonFilters.push_back(j);
		}
		obj.insert({this->KEY_FILE_FILTERS, json11::Json(jsonFilters)});	
	}
	insertToJsonObj(obj, this->KEY_TEMPLATE, json11::Json(this->templateName), !this->templateName.empty());
	out = obj;
	return true;
}
	
bool TargetDirectory::parseFileFilters(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	if(!IItem::hasKey(data, this->KEY_FILE_FILTERS)) {
		return true;
	}
	
	if(!IItem::isArray(this->KEY_FILE_FILTERS, data, errorReceiver)) {
		return false;
	}
	
	auto& fileFiltersJson = data[this->KEY_FILE_FILTERS].array_items();
	this->fileFilters.clear();
	this->fileFilters.reserve(fileFiltersJson.size());
	for(auto& filterJson : fileFiltersJson) {
		FileFilter child;
		if(!child.parse(filterJson, errorReceiver)) {
			continue;
		}
		this->fileFilters.push_back(child);
	}
	
	return true;
}
	
bool TargetDirectory::parseIgnores(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	this->ignores.clear();
	if(!IItem::hasKey(data, this->KEY_IGNORES)) {
		return true;
	}
	
	if(!IItem::isArray(this->KEY_IGNORES, data, errorReceiver)) {
		return false;
	}
	auto& ignoresJson = data[this->KEY_IGNORES].array_items();
	this->ignores.reserve(ignoresJson.size());
	for(auto& ig : ignoresJson) {
		if(!ig.is_string()) {
			errorReceiver.receive(ErrorReceiver::eError, ig.rowInFile(), R"("ignores" key must be array! this element is ignored.)");
			continue;
		}
		auto& str = ig.string_value();
		if(str.empty()) {
			errorReceiver.receive(ErrorReceiver::eWarnning, ig.rowInFile(), R"(Ignores empty string in "ignores".)");
			continue;
		}
		this->ignores.push_back(str);
	}
	
	return true;
}

void TargetDirectory::evalVariables(const SpecialVariables& variables)
{
	this->path = variables.parse(this->path);
	this->fileFilters.reserve(this->fileFilters.size());
	for(auto&& filter : this->fileFilters) {
		filter.evalVariables(variables);
	}

	boost::range::transform(this->ignores
		, this->ignores.begin()
		, [&](const std::string& op){ return variables.parse(op); });
}

void TargetDirectory::adaptTemplateItem(const TemplateItem& templateItem)
{
	if(TemplateItemType::eTargetDirectory != templateItem.itemType) {
		cerr << "template \"" << templateItem.name << "\" is not targetDirectory type. ignore this template..." << endl;
		return ;
	}
	auto& item = templateItem.targetDirectory();

	// enable append elements
	auto appendElement = [](auto* pOut, const auto& src){
		if(!src.empty()) {
			pOut->reserve(pOut->size() + src.size());
			boost::copy(src, std::back_inserter(*pOut));
		}
	};
	appendElement(&this->ignores, item.ignores);
	
	// enable override elements
	if(this->path.empty() && !item.path.empty()) {
		this->path = item.path;
	}
	
	// ignore elements
	if(!item.fileFilters.empty()) {
		cerr << "ignore fileFilters in targetDirectory template." << endl;		
	}
}

//
//	struct OutputConfig
//

bool OutputConfig::parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	if(!this->parseName(data, errorReceiver)) {
		return false;
	}
		
	bool isOK = true;
	if(IItem::hasKey(data, OutputConfig::KEY_OUTPUT_PATH)) {
		isOK &= IItem::parseString(this->outputPath, OutputConfig::KEY_OUTPUT_PATH, data, errorReceiver);
	}
	if(IItem::hasKey(data, OutputConfig::KEY_INTERMEDIATE_PATH)) {
		isOK &= IItem::parseString(this->intermediatePath, OutputConfig::KEY_INTERMEDIATE_PATH, data, errorReceiver);
	}
	
	return isOK;
}

bool OutputConfig::dumpImpl(json11::Json& out)
{
	json11::Json::object obj;
	insertToJsonObj(obj, this->KEY_NAME, json11::Json(this->name), !this->name.empty());
	insertToJsonObj(obj, this->KEY_OUTPUT_PATH, json11::Json(this->outputPath), !this->outputPath.empty());
	insertToJsonObj(obj, this->KEY_INTERMEDIATE_PATH, json11::Json(this->intermediatePath), !this->intermediatePath.empty());
	out = json11::Json(obj);
	return true;
}

bool OutputConfig::parseName(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	return IItem::parseName(this->name, data, errorReceiver);
}

void OutputConfig::evalVariables(const SpecialVariables& variables)
{
	this->name = variables.parse(this->name);
	this->outputPath = variables.parse(this->outputPath);
	this->intermediatePath = variables.parse(this->intermediatePath);
}

//
//	struct BuildSetting
//

bool BuildSetting::parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	if(!this->parseName(data, errorReceiver)) {
		return false;
	}
	
	bool isOK = true;		
	isOK &= this->parseCompiler(data, errorReceiver);
	isOK &= this->parseTargetDirectories(data, errorReceiver);
	isOK &= this->parseCompileOptions(data, errorReceiver);
	isOK &= this->parseIncludeDirectories(data, errorReceiver);
	isOK &= this->parseLibraryDirectories(data, errorReceiver);
	isOK &= this->parseLinkLibraries(data, errorReceiver);
	isOK &= this->parseLinkOptions(data, errorReceiver);
	{
		isOK &= this->parseOutputConfig(data, errorReceiver);
	}
	
	if(IItem::hasKey(data, this->KEY_TEMPLATE)) {
		IItem::parseString(this->templateName, this->KEY_TEMPLATE, data, errorReceiver, false);
	}
	IItem::parseString(this->preprocess, this->KEY_PREPROCESS, data, errorReceiver, false);
	IItem::parseString(this->linkPreprocess, this->KEY_LINK_PREPROCESS, data, errorReceiver, false);
	IItem::parseString(this->postprocess, this->KEY_POSTPROCESS, data, errorReceiver, false);
	this->parseDependences(data, errorReceiver);
	
	return isOK;
}

bool BuildSetting::dumpImpl(json11::Json& out)
{
	json11::Json::object obj;
	insertToJsonObj(obj, this->KEY_NAME, json11::Json(this->name), !this->name.empty());
	insertToJsonObj(obj, this->KEY_COMPILER, json11::Json(this->compiler), !this->compiler.empty());
	insertToJsonObj(obj, this->KEY_INCLUDE_DIRECTORIES, json11::Json(this->includeDirectories), !this->includeDirectories.empty());
	insertToJsonObj(obj, this->KEY_COMPILE_OPTIONS, json11::Json(this->compileOptions), !this->compileOptions.empty());
	insertToJsonObj(obj, this->KEY_LIBRARY_DIRECTORIES, json11::Json(this->libraryDirectories), !this->libraryDirectories.empty());
	insertToJsonObj(obj, this->KEY_LINK_LIBRARIES, json11::Json(this->linkLibraries), !this->linkLibraries.empty());
	insertToJsonObj(obj, this->KEY_LINK_OPTIONS, json11::Json(this->linkOptions), !this->linkOptions.empty());
	if(!this->targetDirectories.empty())
	{
		json11::Json::array jsonTargetDirs;
		jsonTargetDirs.reserve(this->targetDirectories.size());
		for(auto& target : this->targetDirectories) {
			json11::Json j;
			if(!target.dump(j)) {
				continue;
			}
			jsonTargetDirs.push_back(j);
		}
		obj.insert({this->KEY_TARGET_DIRECTORIES, jsonTargetDirs});
	}
	{
		json11::Json jsonConfig;
		if(this->outputConfig.dump(jsonConfig)) {
			obj.insert({this->KEY_OUTPUT_CONFIG, jsonConfig});
		}
	}
	insertToJsonObj(obj, this->KEY_TEMPLATE, json11::Json(this->templateName), !this->templateName.empty());
	insertToJsonObj(obj, this->KEY_PREPROCESS, json11::Json(this->preprocess), !this->preprocess.empty());
	insertToJsonObj(obj, this->KEY_LINK_PREPROCESS, json11::Json(this->linkPreprocess), !this->linkPreprocess.empty());
	insertToJsonObj(obj, this->KEY_POSTPROCESS, json11::Json(this->postprocess), !this->postprocess.empty());
	insertToJsonObj(obj, this->KEY_DEPENDENCES, json11::Json(this->dependences), !this->dependences.empty());
	out = json11::Json(obj);
	return true;
}
	
bool BuildSetting::parseName(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	return IItem::parseName(this->name, data, errorReceiver);
}
	
bool BuildSetting::parseCompiler(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	if(!IItem::hasKey(data, this->KEY_COMPILER)) {
		return true;
	}
	return IItem::parseString(this->compiler, this->KEY_COMPILER, data, errorReceiver);
}
	
bool BuildSetting::parseTargetDirectories(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	this->targetDirectories.clear();
	if(!IItem::hasKey(data, this->KEY_TARGET_DIRECTORIES)) {
		return true;
	}
	auto& keyTargets = data[this->KEY_TARGET_DIRECTORIES];
	if(!keyTargets.is_array()) {
		errorReceiver.receive(ErrorReceiver::eError, keyTargets.rowInFile(), "\"" + this->KEY_TARGET_DIRECTORIES + R"(" key must be array!)");
		return false;
	}
	
	auto& targets = keyTargets.array_items();
	this->targetDirectories.reserve(targets.size());
	for(auto& t : targets) {
		TargetDirectory child;
		if(!child.parse(t, errorReceiver)) {
			continue;
		}
		this->targetDirectories.push_back(std::move(child));						
	}
	return true;
}

bool BuildSetting::parseCompileOptions(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	this->compileOptions.clear();
	if(!IItem::hasKey(data, this->KEY_COMPILE_OPTIONS)) {
		return true;
	}
	
	return IItem::parseOptionArray(this->compileOptions, this->KEY_COMPILE_OPTIONS, data, errorReceiver);
}

bool BuildSetting::parseIncludeDirectories(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	this->includeDirectories.clear();
	if(!IItem::hasKey(data, this->KEY_INCLUDE_DIRECTORIES)) {
		return true;
	}
	
	return IItem::parseOptionArray(this->includeDirectories, this->KEY_INCLUDE_DIRECTORIES, data, errorReceiver);
}

bool BuildSetting::parseLibraryDirectories(
	const json11::Json& data,
	ErrorReceiver& errorReceiver)
{
	this->libraryDirectories.clear();
	if(!IItem::hasKey(data, this->KEY_LIBRARY_DIRECTORIES)) {
		return true;
	}

	return IItem::parseOptionArray(this->libraryDirectories, this->KEY_LIBRARY_DIRECTORIES, data, errorReceiver);
}

bool BuildSetting::parseLinkLibraries(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	this->linkLibraries.clear();
	if(!IItem::hasKey(data, this->KEY_LINK_LIBRARIES)) {
		return true;
	}
	
	return IItem::parseOptionArray(this->linkLibraries, this->KEY_LINK_LIBRARIES, data, errorReceiver);
}

bool BuildSetting::parseLinkOptions(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	this->linkOptions.clear();
	if(!IItem::hasKey(data, this->KEY_LINK_OPTIONS)) {
		return true;
	}
	
	return IItem::parseOptionArray(this->linkOptions, this->KEY_LINK_OPTIONS, data, errorReceiver);
}

bool BuildSetting::parseDependences(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	this->dependences.clear();
	return IItem::parseStringArray(this->dependences, this->KEY_DEPENDENCES, data, errorReceiver, false);
}
	
bool BuildSetting::parseOutputConfig(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	auto& keyOutputConfig = data[this->KEY_OUTPUT_CONFIG];
	if(!IItem::hasKey(data, this->KEY_OUTPUT_CONFIG)) {
		return true;
	}
	if(!keyOutputConfig.is_object()) {
		errorReceiver.receive(ErrorReceiver::eError, keyOutputConfig.rowInFile(), "\"" + this->KEY_OUTPUT_CONFIG + R"(" key must be object!)");
		return false;
	}
	
	this->outputConfig.parse(keyOutputConfig.object_items(), errorReceiver);
	return true;
}

boost::filesystem::path BuildSetting::makeIntermediatePath(
	const boost::filesystem::path& configFilepath,
	const Project& project)const
{
	// outputPath is <configFilepath.parent_path()>/<intermediatePath>/<configFileStem>_<projectName>_<buildSettingName>/
	auto outputPath = configFilepath.parent_path();
	outputPath /= (this->outputConfig.intermediatePath.empty()
		? "intermediate" : this->outputConfig.intermediatePath);
	outputPath /= configFilepath.stem();
	outputPath += ("_" +project.name + "_" + this->name);
	if(!createDirectory(outputPath)) {
		auto msg = "Failed to create output directory. path=" + outputPath.parent_path().string();
		AWESOME_THROW(std::runtime_error) << msg;
	}
	return outputPath;
}

boost::filesystem::path BuildSetting::makeOutputFilepath(
	const boost::filesystem::path& configFilepath,
	const Project& project)const
{
	// outputPath is <configFilepath.parent_path()>/<outputPath>/<configFilepathStem>_<projectName>_<buildSettingName>/outputFilename
	auto outputPath = fs::path(configFilepath).parent_path();
	outputPath /= (this->outputConfig.outputPath.empty()
		? "output" : this->outputConfig.outputPath);
	outputPath /= configFilepath.stem();
	outputPath += ("_" + project.name+"_"+this->name);
	if(!createDirectory(outputPath)) {
		auto msg = "Failed to create output directory. path=" + outputPath.parent_path().string();
		AWESOME_THROW(std::runtime_error) << msg;
	}
	
	outputPath = fs::canonical(outputPath, "./") / this->outputConfig.name;
	outputPath = fs::relative(outputPath, fs::initial_path());
	if(Project::eSharedLib == project.type) {
		std::string soname = "lib" + this->outputConfig.name + ".so." + std::to_string(project.version);
		outputPath = outputPath.parent_path()/soname;
	}else if(Project::eStaticLib == project.type) {
		std::string arName = "lib" + this->outputConfig.name + ".a";
		outputPath = outputPath.parent_path()/arName;
	}
	return outputPath;
}

void BuildSetting::exePreprocess()const
{
	exeCommand(this->preprocess, "Failed preprocess..");
}

void BuildSetting::exeLinkPreprocess()const
{
	exeCommand(this->linkPreprocess, "Failed link preprocess..");
}

void BuildSetting::exePostprocess()const
{
	exeCommand(this->postprocess, "Failed postprocess..");
}

void BuildSetting::evalVariables(const SpecialVariables& variables)
{
	this->compiler = variables.parse(this->compiler);
	
	auto parseString = [&](const std::string& op){
		return variables.parse(op);
	};
	boost::range::transform(this->compileOptions, this->compileOptions.begin(), parseString);
	boost::range::transform(this->linkLibraries, this->linkLibraries.begin(), parseString);
	boost::range::transform(this->libraryDirectories, this->libraryDirectories.begin(), parseString);
	boost::range::transform(this->linkOptions, this->linkOptions.begin(), parseString);

	this->outputConfig.evalVariables(variables);
	this->templateName = variables.parse(this->templateName);
	this->preprocess = variables.parse(this->preprocess);
	this->linkPreprocess = variables.parse(this->linkPreprocess);
	this->postprocess = variables.parse(this->postprocess);
}

void BuildSetting::adaptTemplateItem(const TemplateItem& templateItem)
{
	if(TemplateItemType::eBuildSetting != templateItem.itemType) {
		cerr << "template \"" << templateItem.name << "\" is not buildSetting type. ignore this template..." << endl;
		return ;
	}
	auto& item = templateItem.buildSetting();
	
	// enable append elements
	auto appendElement = [](auto* pOut, const auto& src){
		if(!src.empty()) {
			pOut->reserve(pOut->size() + src.size());
			boost::copy(src, std::back_inserter(*pOut));
		}
	};
	appendElement(&this->compileOptions, item.compileOptions);
	appendElement(&this->linkLibraries, item.linkLibraries);
	appendElement(&this->libraryDirectories, item.libraryDirectories);
	appendElement(&this->linkOptions, item.linkOptions);

	// enable override elements
	if(this->compiler.empty() && !item.compiler.empty()) {
		this->compiler = item.compiler;
	}
	if(this->preprocess.empty() && !item.preprocess.empty()) {
		this->preprocess = item.preprocess;
	}
	if(this->linkPreprocess.empty() && !item.linkPreprocess.empty()) {
		this->linkPreprocess = item.linkPreprocess;
	}
	if(this->postprocess.empty() && !item.postprocess.empty()) {
		this->postprocess = item.postprocess;
	}

	// TODO define something similar to this function in OutputConfig?
	if(this->outputConfig.name.empty() && !item.outputConfig.name.empty()) {
		this->outputConfig.name = item.outputConfig.name;
	}
	if(this->outputConfig.outputPath.empty() && !item.outputConfig.outputPath.empty()) {
		this->outputConfig.outputPath = item.outputConfig.outputPath;
	}
	if(this->outputConfig.intermediatePath.empty() && !item.outputConfig.intermediatePath.empty()) {
		this->outputConfig.intermediatePath = item.outputConfig.intermediatePath;
	}
	
	// ignore elements
	if(!item.targetDirectories.empty()) {
		cerr << "ignore targetDirectories in buildSetting template." << endl;		
	}
}

//
//	struct Project
//
bool Project::parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	if(!this->parseName(data, errorReceiver)) {
		return false;
	}
	
	bool isOK = true;
	isOK &= this->parseType(data, errorReceiver);
	isOK &= this->parseBuildSettings(data, errorReceiver);
		
	// below option key
	if(IItem::hasKey(data, this->KEY_TEMPLATE)) {
		IItem::parseString(this->templateName, this->KEY_TEMPLATE, data, errorReceiver, false);
	}
	IItem::parseInteger(this->version, this->KEY_VERSION, data, errorReceiver, false);
	IItem::parseInteger(this->minorNumber, this->KEY_MINOR_NUMBER, data, errorReceiver, false);
	IItem::parseInteger(this->releaseNumber, this->KEY_RELEASE_NUMBER, data, errorReceiver, false);

	return isOK;
}

bool Project::dumpImpl(json11::Json& out)
{
	json11::Json::object obj;
	insertToJsonObj(obj, this->KEY_NAME, json11::Json(this->name), !this->name.empty());
	insertToJsonObj(obj, this->KEY_TYPE, json11::Json(Project::toStr(this->type)), this->type != Project::eUnknown);
	if(!this->buildSettings.empty())
	{
		json11::Json::array jsonBuildSettings;
		jsonBuildSettings.reserve(this->buildSettings.size());
		for(auto& [key, settings] : this->buildSettings) {
			json11::Json j;
			if(!settings.dump(j)) {
				continue;
			}
			jsonBuildSettings.push_back(j);
		}
		obj.insert({Project::KEY_BUILD_SETTINGS, jsonBuildSettings});
	}
	
	insertToJsonObj(obj, this->KEY_TEMPLATE, json11::Json(this->templateName), !this->templateName.empty());
	insertToJsonObj(obj, this->KEY_VERSION, json11::Json(this->version), this->version != 0);
	insertToJsonObj(obj, this->KEY_MINOR_NUMBER, json11::Json(this->minorNumber), this->minorNumber != 0);
	insertToJsonObj(obj, this->KEY_RELEASE_NUMBER, json11::Json(this->releaseNumber), this->releaseNumber != 0);
	
	out = json11::Json(obj);
	return true;
}

void Project::evalVariables(const SpecialVariables& variables)
{
}

void Project::adaptTemplateItem(const TemplateItem& templateItem)
{
	if(TemplateItemType::eProject != templateItem.itemType) {
		cerr << "template \"" << templateItem.name << "\" is not project type. ignore this template..." << endl;
		return ;
	}
	auto& item = templateItem.project();

	// enable append elements
	
	// enable override elements
	if(this->eUnknown == this->type && this->eUnknown != item.type) {
		this->type = item.type;
	}
	if(this->version <= 0 && 0 < item.version) {
		this->version = item.version;
	}
	if(this->minorNumber <= 0 && 0 < item.minorNumber) {
		this->minorNumber = item.minorNumber;
	}
	if(this->releaseNumber <= 0 && 0 < item.releaseNumber) {
		this->releaseNumber = item.releaseNumber;
	}
	
	// ignore elements
	if(!item.buildSettings.empty()) {
		cerr << "ignore buildSetting in project template." << endl;
	}
}

const BuildSetting& Project::findBuildSetting(const std::string& name)const
{
	auto buildSettingIt = this->buildSettings.find(name);
	if(this->buildSettings.end() == buildSettingIt) {
		AWESOME_THROW(std::runtime_error) << "\"" + name + "\" build settings don't found";
	}
	return buildSettingIt->second;
}

fs::path Project::makeIntermediatePath(
	const boost::filesystem::path& configFilepath,
	const std::string& intermediateDir,
	const std::string& buildSettingName)const
{
	auto& buildSetting = this->findBuildSetting(buildSettingName);
	return buildSetting.makeIntermediatePath(configFilepath, *this);	
}

fs::path Project::makeOutputFilepath(
	const boost::filesystem::path& configFilepath,
	const std::string& outputDir,
	const std::string& buildSettingName)const
{ 
	auto& buildSetting = this->findBuildSetting(buildSettingName);
	return buildSetting.makeOutputFilepath(configFilepath, *this);	
}

Project::Type Project::toType(const std::string& typeStr)
{
	static const unordered_map<std::string, Type> sTable = {
		{"exe", eExe},
		{"static", eStaticLib},
		{"shared", eSharedLib},
	};
	auto s = typeStr;
	std::transform(s.cbegin(), s.cend(), s.begin(), [](char c){ return std::tolower(c); });	
	auto it = sTable.find(s);
	if(sTable.end() == it) {
		return eUnknown;
	} else {
		return it->second;
	}
}
	
std::string Project::toStr(Type type)
{
	switch(type) {
	case eExe: 		return "exe";
	case eStaticLib: 	return "static";
	case eSharedLib: 	return "shared";
	default:		return "(unknown)";
	}
}

bool Project::parseName(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	return IItem::parseName(this->name, data, errorReceiver);
}
		
bool Project::parseType(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	if(!IItem::hasKey(data, Project::KEY_TYPE)) {
		return true;
	}
	return IItem::parseStrToEnum<Type>(this->type, Project::KEY_TYPE, toType, eUnknown, data, errorReceiver);
}
		
bool Project::parseBuildSettings(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	if(!IItem::hasKey(data, Project::KEY_BUILD_SETTINGS)) {
		return true;
	}
	auto& keyBuildSettings = data[Project::KEY_BUILD_SETTINGS];
	if(!keyBuildSettings.is_array()) {
		errorReceiver.receive(ErrorReceiver::eError, keyBuildSettings.rowInFile(), "\"" + Project::KEY_BUILD_SETTINGS + R"(" key must be array!)");
		return false;
	}
	
	auto& buildSettingsJson = keyBuildSettings.array_items();
	for(auto& buildSettingJson : buildSettingsJson) {
		BuildSetting child;
		if(!child.parse(buildSettingJson, errorReceiver)) {
			continue;
		}
		auto it = this->buildSettings.find(child.name);
		if(this->buildSettings.end() != it) {
			errorReceiver.receive(ErrorReceiver::eError, buildSettingJson.rowInFile(), R"(The same name build settings exist! name=")" + child.name + "\"");
			continue;
		}
		this->buildSettings.insert({child.name, child});
	}
	
	return true;
}

//******************************************************************************
//
// struct CompileProcess
//
//******************************************************************************
void CompileProcess::evalVariables(const SpecialVariables& variables)
{
	this->content = variables.parse(this->content);
}

void CompileProcess::adaptTemplateItem(const TemplateItem& templateItem)
{
	if(TemplateItemType::eCompileProcess != templateItem.itemType) {
		cerr << "template \"" << templateItem.name << "\" is not compileProcess type. ignore this template..." << endl;
		return ;
	}
	auto& item = templateItem.compileProcess();
	
	// enable append elements
	
	// enable override elements
	if(this->type == Type::eUnknown && item.type != Type::eUnknown) {
		this->type = item.type;
	}
	if(this->content.empty() && !item.content.empty()) {
		this->content = item.content;
	}
	
	// ignore elements
}

CompileProcess::Type CompileProcess::toType(const std::string& typeStr)
{
	static const std::unordered_map<std::string, Type> sTable = {
		{"terminal", eTerminal},
		{"buildin", eBuildIn},
	};
	auto s = typeStr;
	std::transform(s.cbegin(), s.cend(), s.begin(), [](char c){ return std::tolower(c); });
	auto it = sTable.find(s);
	if(sTable.end() == it) {
		return eUnknown;
	} else {
		return it->second;
	}
}

std::string CompileProcess::toStr(Type type)
{
	switch(type) {
	case Type::eTerminal: 	return "terminal";
	case Type::eBuildIn: 	return "buildIn";
	default:					return "(unknown)";
	}
}

bool CompileProcess::parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	if(!IItem::parseStrToEnum<Type>(this->type, this->KEY_TYPE, this->toType, eUnknown, data, errorReceiver)){
		return false;
	}
	
	bool isOK = true;
	isOK &= IItem::parseString(this->content, this->KEY_CONTENT, data, errorReceiver);
	return isOK;
}

bool CompileProcess::dumpImpl(json11::Json& out)
{
	json11::Json::object obj;

	insertToJsonObj(obj, this->KEY_TYPE, this->toStr(this->type), this->type != CompileProcess::eUnknown);
	insertToJsonObj(obj, this->KEY_CONTENT, content, !this->content.empty());
	
	out = json11::Json(obj);
	return true;
}

//******************************************************************************
//
// struct CompileTask
//
//******************************************************************************	

void CompileTask::evalVariables(const SpecialVariables& variables)
{
	this->command = variables.parse(this->command);
	this->inputOption = variables.parse(this->inputOption);
	this->outputOption = variables.parse(this->outputOption);
	this->commandPrefix = variables.parse(this->commandPrefix);
	this->commandSuffix = variables.parse(this->commandSuffix);
	for(auto&& p : this->preprocesses) {
		p.evalVariables(variables);
	}
	for(auto&& p : this->postprocesses) {
		p.evalVariables(variables);
	}
}

void CompileTask::adaptTemplateItem(const TemplateItem& templateItem)
{
	if(TemplateItemType::eCompileTask != templateItem.itemType) {
		cerr << "template \"" << templateItem.name << "\" is not compileTask type. ignore this template..." << endl;
		return ;
	}
	auto& item = templateItem.compileTask();

	// enable append elements
	
	// enable override elements
	if(this->command.empty() && !item.command.empty()) {
		this->command = item.command;
	}
	if(this->inputOption.empty() && !item.inputOption.empty()) {
		this->inputOption = item.inputOption;
	}
	if(this->outputOption.empty() && !item.outputOption.empty()) {
		this->outputOption = item.outputOption;
	}
	if(this->commandPrefix.empty() && !item.commandPrefix.empty()) {
		this->commandPrefix = item.commandPrefix;
	}
	if(this->commandSuffix.empty() && !item.commandSuffix.empty()) {
		this->commandSuffix = item.commandSuffix;
	}
	
	// ignore elements	
	//this->preprocesses.adaptTemplateItem(TemplateItem(TemplateItemType::eCompileProcess, item.preprocess));
	//this->postprocesses.adaptTemplateItem(TemplateItem(TemplateItemType::eCompileProcess, item.postprocess));
}

bool CompileTask::parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	if(!IItem::parseString(this->command, this->KEY_COMMAND, data, errorReceiver)) {
		return false;
	}
	
	IItem::parseString(this->inputOption, this->KEY_INPUT_OPTION, data, errorReceiver);
	IItem::parseString(this->outputOption, this->KEY_OUTPUT_OPTION, data, errorReceiver);
	IItem::parseString(this->commandPrefix, this->KEY_COMMAND_PREFIX, data, errorReceiver);
	IItem::parseString(this->commandSuffix, this->KEY_COMMAND_SUFFIX, data, errorReceiver);

	bool isOK = true;
	if(IItem::isArray(this->KEY_PREPROCESSES, data, errorReceiver)) {
		auto& jsonPreproccesses = data[this->KEY_PREPROCESSES].array_items();
		this->preprocesses.reserve(jsonPreproccesses.size());
		for(auto&& jsonProcess : jsonPreproccesses) {
			CompileProcess process;
			isOK &= process.parse(jsonProcess, errorReceiver);
			this->preprocesses.emplace_back(process);
		}
	}
	if(IItem::isArray(this->KEY_PREPROCESSES, data, errorReceiver)) {
		auto& jsonPostproccesses = data[this->KEY_POSTPROCESSES].array_items();
		this->postprocesses.reserve(jsonPostproccesses.size());
		for(auto&& jsonProcess : jsonPostproccesses) {
			CompileProcess process;
			isOK &= process.parse(jsonProcess, errorReceiver);
			this->postprocesses.emplace_back(process);
		}
	}
	
	return isOK;
}

bool CompileTask::dumpImpl(json11::Json& out)
{
	json11::Json::object obj;
	insertToJsonObj(obj, this->KEY_COMMAND, command, !this->command.empty());
	insertToJsonObj(obj, this->KEY_INPUT_OPTION, inputOption, !this->inputOption.empty());
	insertToJsonObj(obj, this->KEY_OUTPUT_OPTION, outputOption, !this->outputOption.empty());
	insertToJsonObj(obj, this->KEY_COMMAND_PREFIX, commandPrefix, !this->commandPrefix.empty());
	insertToJsonObj(obj, this->KEY_COMMAND_SUFFIX, commandSuffix, !this->commandSuffix.empty());

	if(!this->preprocesses.empty())
	{
		json11::Json::array processes;
		processes.reserve(this->preprocesses.size());
		for(auto& process : this->preprocesses) {
			json11::Json j;
			if(!process.dump(j)) return false;
			processes.emplace_back(j);
		}
		obj.insert({this->KEY_PREPROCESSES, processes});
	}
	if(!this->postprocesses.empty())
	{
		json11::Json::array processes;
		processes.reserve(this->postprocesses.size());
		for(auto& process : this->postprocesses) {
			json11::Json j;
			if(!process.dump(j)) return false;
			processes.emplace_back(j);
		}
		obj.insert({this->KEY_POSTPROCESSES, processes});
	}
	
	out = json11::Json(obj);
	return true;
}

//******************************************************************************
//
// struct CompileTaskGroup
//
//******************************************************************************	
void CompileTaskGroup::evalVariables(const SpecialVariables& variables)
{
	this->compileObj.evalVariables(variables);
	this->linkObj.evalVariables(variables);
}

void CompileTaskGroup::adaptTemplateItem(const TemplateItem& templateItem)
{
	if(TemplateItemType::eCompileTaskGroup != templateItem.itemType) {
		cerr << "template \"" << templateItem.name << "\" is not compileTask type. ignore this template..." << endl;
		return ;
	}
	auto& item = templateItem.compileTaskGroup();

	// enable append elements
	
	// enable override elements
	this->compileObj.adaptTemplateItem(TemplateItem(TemplateItemType::eCompileTask, item.compileObj));
	this->linkObj.adaptTemplateItem(TemplateItem(TemplateItemType::eCompileTask, item.linkObj));
	
	// ignore elements
}

bool CompileTaskGroup::parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	isOK &= this->compileObj.parse(data, errorReceiver);
	isOK &= this->linkObj.parse(data, errorReceiver);
	return isOK;
}

bool CompileTaskGroup::dumpImpl(json11::Json& out)
{
	json11::Json::object obj;
	
	json11::Json json;
	if(!this->compileObj.dump(json)) { return false; }
	obj.insert({this->KEY_COMPILE_OBJ, json});
	
	if(!this->linkObj.dump(json)) { return false; }
	obj.insert({this->KEY_LINK_OBJ, json});
		
	out = json11::Json(obj);
	return true;	
}

//******************************************************************************
//
// struct CustomCompiler
//
//******************************************************************************	
void CustomCompiler::evalVariables(const SpecialVariables& variables)
{
	this->exe.evalVariables(variables);
	this->staticLib.evalVariables(variables);
	this->sharedLib.evalVariables(variables);
}

void CustomCompiler::adaptTemplateItem(
	const TemplateItem& templateItem)
{
	if(TemplateItemType::eCustomCompiler != templateItem.itemType) {
		cerr << "template \"" << templateItem.name << "\" is not customCompiler type. ignore this template..." << endl;
		return ;
	}
	auto& item = templateItem.customCompiler();

	// enable append elements
	
	// enable override elements
	this->exe.adaptTemplateItem(TemplateItem(TemplateItemType::eCompileTaskGroup, item.exe));
	this->staticLib.adaptTemplateItem(TemplateItem(TemplateItemType::eCompileTaskGroup, item.staticLib));
	this->sharedLib.adaptTemplateItem(TemplateItem(TemplateItemType::eCompileTaskGroup, item.sharedLib));
	
	// ignore elements
}

bool CustomCompiler::parseImpl(
	const json11::Json& data,
	ErrorReceiver& errorReceiver)
{
	if(!IItem::parseName(this->name, data, errorReceiver)){
		return false;
	}
	
	bool isOK = true;
	isOK &= this->exe.parse(data, errorReceiver);
	isOK &= this->staticLib.parse(data, errorReceiver);
	isOK &= this->sharedLib.parse(data, errorReceiver);
	return isOK;
}

bool CustomCompiler::dumpImpl(
	json11::Json& out)
{
	json11::Json::object obj;
	insertToJsonObj(obj, this->KEY_NAME, this->name, !this->name.empty());
	
	json11::Json json;
	if(!this->exe.dump(json)) { return false; }
	obj.insert({this->KEY_EXE, json});
	
	if(!this->staticLib.dump(json)) { return false; }
	obj.insert({this->KEY_STATIC, json});
	
	if(!this->sharedLib.dump(json)) { return false; }
	obj.insert({this->KEY_SHARED, json});
	
	out = json11::Json(obj);
	return true;	
}

//******************************************************************************
//
// struct RootConfig
//
//******************************************************************************
bool RootConfig::parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	isOK &= parseProjects(data, errorReceiver);
		
	return isOK;
}

bool RootConfig::dumpImpl(json11::Json& out)
{
	json11::Json::object obj;
	if(!this->projects.empty())
	{
		json11::Json::array jsonProjects;
		jsonProjects.reserve(this->projects.size());
		for(auto& [key, project] : this->projects) {
			json11::Json j;
			if(!project.dump(j)) {
				continue;
			}
			jsonProjects.push_back(j);
		}
		obj.insert({RootConfig::KEY_PROJECTS, jsonProjects});
	}
	out = json11::Json(obj);
	return true;
}

const Project& RootConfig::findProject(const std::string& name) const
{
	auto projectIt = this->projects.find(name);
	if(this->projects.end() == projectIt) {
		AWESOME_THROW(std::runtime_error) << "\"" << name << "\" project don't found";
	}
	return projectIt->second;
}

bool RootConfig::parseProjects(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	auto& keyProjects = data[RootConfig::KEY_PROJECTS];
	if( !keyProjects.is_array() ) {
		errorReceiver.receive(ErrorReceiver::eError, keyProjects.rowInFile(), "\"" + RootConfig::KEY_PROJECTS + R"(" key must be array!)");
		return false;
	}
	
	bool isOK = true;
	for(auto& projJson : keyProjects.array_items()) {
		Project child;
		bool isOKChild = child.parse(projJson, errorReceiver);
		isOK &= isOKChild;
		if (!isOKChild) {
			continue;
		}
		auto it = this->projects.find(child.name);
		if(this->projects.end() != it) {
			errorReceiver.receive(ErrorReceiver::eError, projJson.rowInFile(), R"(The same name project exist! name=")" + child.name + "\"");
			continue;
		}
		this->projects.insert({ child.name, child });
	}
	return isOK;
}

//******************************************************************************
//
//	TemplateItem
//
//******************************************************************************

bool TemplateItem::parseImpl(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	if(!IItem::parseName(this->name, data, errorReceiver)) {
		return false;
	}
	
	if(!this->parseItemType(data, errorReceiver)) {
		errorReceiver.receive(ErrorReceiver::eError, data[this->KEY_ITEM_TYPE].rowInFile(), "Failed to parse defineTemplate item...");
		return false;
	}
	
	switch(this->itemType) {
	case eProject: 				this->item = Project(); break;
	case eBuildSetting: 		this->item = BuildSetting(); break;
	case eTargetDirectory:		this->item = TargetDirectory(); break;
	case eFileFilter:			this->item = FileFilter(); break;
	case eFileToProcess:		this->item = FileToProcess(); break;
	default:
		errorReceiver.receive(ErrorReceiver::eError, data[this->KEY_ITEM_TYPE].rowInFile(), "Unknwon itemType...");
		return false;
	}

	struct ParseItem : boost::static_visitor<bool>
	{
		const json11::Json& data;
		ErrorReceiver& errorReceiver;
		ParseItem(const json11::Json& data, ErrorReceiver& errorReceiver)
			: data(data)
			, errorReceiver(errorReceiver)
		{}
		
		bool operator()(boost::blank& blank)const { return false; }
		
		bool operator()(IItem& item)const { return item.parse(data, errorReceiver); }
		
		/*
		bool operator()(Project& item)const { return item.parse(data, errorReceiver); }
		bool operator()(BuildSetting& item)const { return item.parse(data, errorReceiver); }
		bool operator()(TargetDirectory& item)const { return item.parse(data, errorReceiver); }
		bool operator()(FileFilter& item)const { return item.parse(data, errorReceiver); }
		bool operator()(FileToProcess& item)const { return item.parse(data, errorReceiver); }
		*/
	};
	
	if(!boost::apply_visitor(ParseItem(data, errorReceiver), this->item)) {
		errorReceiver.receive(ErrorReceiver::eError, data.rowInFile(), "Failed to parse defineTemplate item...");
		return false;
	}
	
	return true;
}

bool TemplateItem::dumpImpl(json11::Json& out)
{
	struct DumpItem : boost::static_visitor<bool>
	{
		json11::Json& out;
		DumpItem(json11::Json& out)
			: out(out)
		{}
		
		bool operator()(boost::blank& blank)const { return false; }
		//template<typename Item>
		bool operator()(IItem& item)const { return item.dump(out); }
		
		/*
		bool operator()(Project& item)const { return item.dump(out); }
		bool operator()(BuildSetting& item)const { return item.dump(out); }
		bool operator()(TargetDirectory& item)const { return item.dump(out); }
		bool operator()(FileFilter& item)const { return item.dump(out); }
		bool operator()(FileToProcess& item)const { return item.dump(out); }
		*/
	};

	if(!boost::apply_visitor(DumpItem(out), this->item)) {
		return false;
	}
	
	//TODO Jsonから直接キーを挿入できるようにする
	json11::Json::object obj = out.object_items();
	insertToJsonObj(obj, this->KEY_NAME, this->name, !this->name.empty());
	insertToJsonObj(obj, this->KEY_ITEM_TYPE, this->toStr(this->itemType), this->itemType != TemplateItemType::eUnknown);
	
	out = json11::Json(obj);
	return true;
}

bool TemplateItem::parseItemType(const json11::Json& data, ErrorReceiver& errorReceiver)
{
	return IItem::parseStrToEnum<TemplateItemType>(this->itemType, TemplateItem::KEY_ITEM_TYPE,
		this->toItemType, TemplateItemType::eUnknown, data, errorReceiver);
}

const Project& TemplateItem::project()const
{
	if(TemplateItemType::eProject != this->itemType) {
		AWESOME_THROW(std::runtime_error) << "item is not Project.";
	}
	return boost::get<Project>(this->item);
}

const BuildSetting& TemplateItem::buildSetting()const
{
	if(TemplateItemType::eBuildSetting != this->itemType) {
		AWESOME_THROW(std::runtime_error) << "item is not BuildSetting.";
	}
	return boost::get<BuildSetting>(this->item);
}

const TargetDirectory& TemplateItem::targetDirectory()const
{
	if(TemplateItemType::eTargetDirectory!= this->itemType) {
		AWESOME_THROW(std::runtime_error) << "item is not TargetDirectory.";
	}
	return boost::get<TargetDirectory>(this->item);
}

const FileFilter& TemplateItem::fileFilter()const
{
	if(TemplateItemType::eFileFilter!= this->itemType) {
		AWESOME_THROW(std::runtime_error) << "item is not FileFilter.";
	}
	return boost::get<FileFilter>(this->item);
}

const FileToProcess& TemplateItem::fileToProcess()const
{
	if(TemplateItemType::eFileToProcess!= this->itemType) {
		AWESOME_THROW(std::runtime_error) << "item is not FileToProcess.";
	}
	return boost::get<FileToProcess>(this->item);
}

const CustomCompiler& TemplateItem::customCompiler()const
{
	if(TemplateItemType::eCustomCompiler!= this->itemType) {
		AWESOME_THROW(std::runtime_error) << "item is not CustomCompiler.";
	}
	return boost::get<CustomCompiler>(this->item);
}

const CompileTask& TemplateItem::compileTask()const
{
	if(TemplateItemType::eCompileTask!= this->itemType) {
		AWESOME_THROW(std::runtime_error) << "item is not CompileTask.";
	}
	return boost::get<CompileTask>(this->item);
}

const CompileTaskGroup& TemplateItem::compileTaskGroup()const
{
	if(TemplateItemType::eCompileTaskGroup!= this->itemType) {
		AWESOME_THROW(std::runtime_error) << "item is not CompileTaskGroup.";
	}
	return boost::get<CompileTaskGroup>(this->item);
}

const CompileProcess& TemplateItem::compileProcess()const
{
	if(TemplateItemType::eCompileProcess!= this->itemType) {
		AWESOME_THROW(std::runtime_error) << "item is not CompileProcess.";
	}
	return boost::get<CompileProcess>(this->item);
}

TemplateItemType TemplateItem::toItemType(const std::string& typeStr)
{
	static const unordered_map<std::string, TemplateItemType> sTable = {
		{"project", eProject},
		{"buildsetting", eBuildSetting},
		{"targetdirectory", eTargetDirectory},
		{"filefilter", eFileFilter},
		{"filetoprocess", eFileToProcess},
		{"customcompiler", eCustomCompiler},
		{"compiletask", eCompileTask},
		{"complietaskgroup", eCompileTaskGroup},
		{"compileprocess", eCompileProcess},
	};

	auto s = typeStr;
	std::transform(s.cbegin(), s.cend(), s.begin(), [](char c){ return std::tolower(c); });
	auto it = sTable.find(s);
	if(sTable.end() == it) {
		return eUnknown;
	} else {
		return it->second;
	}
}

std::string TemplateItem::toStr(TemplateItemType type)
{
	switch(type) {
	case eProject: 				return "project";
	case eBuildSetting:	 		return "buildSetting";
	case eTargetDirectory: 	return "targetdirectory";
	case eFileFilter: 			return "filefilter";
	case eFileToProcess: 		return "filetoprocess";
	case eCustomCompiler:		return "customcompiler";
	case eCompileTask:			return "compiletask";
	case eCompileTaskGroup:	return "complietaskgroup";
	case eCompileProcess:		return "compileprocess";
	default:						return "(unknown)";
	}
}

//
//
//

void parse(RootConfig& out, const json11::Json& configJson)
{
	ErrorReceiver errorReceiver;
	out.parse(configJson, errorReceiver);
	if(errorReceiver.hasError()) {
		errorReceiver.print();
		AWESOME_THROW(std::runtime_error) << "Exsit error in config file!";
	}
	errorReceiver.print(ErrorReceiver::eWarnning);
}

}
