#include "configJsonParser.h"

#include <functional>

#include "utility.h"

using namespace std::literals::string_literals;

namespace watagashi::config::json
{

//
//	Parser Utility Functions
//

template<typename T>
json11::Json toJson(T const& item){ return item; }

#define DEFINE_TO_JSON(ConfigItem) \
	template<> json11::Json toJson<ConfigItem>(ConfigItem const& item){ return ItemParser<ConfigItem>::dump(item); }
DEFINE_TO_JSON(UserValue);
DEFINE_TO_JSON(FileToProcess);
DEFINE_TO_JSON(FileFilter);
DEFINE_TO_JSON(TargetDirectory);
DEFINE_TO_JSON(OutputConfig);
DEFINE_TO_JSON(BuildSetting);
DEFINE_TO_JSON(Project);
DEFINE_TO_JSON(CompileProcess);
DEFINE_TO_JSON(CompileTask);
DEFINE_TO_JSON(CompileTaskGroup);
DEFINE_TO_JSON(CustomCompiler);
DEFINE_TO_JSON(RootConfig);
DEFINE_TO_JSON(TemplateItem);


static bool hasKey(
	json11::Json const& data,
	std::string const& key)
{
	return json11::Json() != data[key];
}

template<typename JsonType>
std::string typeString()
{
	static_assert(sizeof(JsonType) < 0);
}

template<> std::string typeString<int>(){ return "integer"; }
template<> std::string typeString<double>(){ return "double"; }
template<> std::string typeString<bool>(){ return "bool"; }
template<> std::string typeString<std::string>(){ return "string"; }
template<> std::string typeString<json11::Json::array>(){ return "array"; }
template<> std::string typeString<json11::Json::object>(){ return "object"; }
template<> std::string typeString<std::string const&>(){ return "string"; }
template<> std::string typeString<json11::Json::array const&>(){ return "array"; }
template<> std::string typeString<json11::Json::object const&>(){ return "object"; }

template<typename JsonType>
std::string makeInvalidKeyTypeMessage(std::string const& key)
{
	return "\"" + key + R"(" key must be )" + typeString<JsonType>() + "!";
}

enum class ParseOption
{
	None,
	SkipIfNoKeyExist,
};

template<typename JsonType>
bool isType(
	std::string const& key,
	json11::Json const& data,
	ErrorReceiver& errorReceiver,
	ParseOption option=ParseOption::None)
{
	if(ParseOption::SkipIfNoKeyExist == option && !hasKey(data, key)) {
		return true;
	}
	
	using RawJsonType = std::remove_const_t<std::remove_reference_t<JsonType>>;
	auto& keyValue = data[key];
	if(!keyValue.is_type<RawJsonType>()) {
		errorReceiver.receive(ErrorReceiver::eError, keyValue.rowInFile(), makeInvalidKeyTypeMessage<RawJsonType>(key));
		return false;
	}
	return true;
}

template<typename JsonType>
bool parseValue(
	JsonType& out,
	std::string const& key,
	json11::Json const& data,
	ErrorReceiver& errorReceiver,
	ParseOption option=ParseOption::None)
{
	if(ParseOption::SkipIfNoKeyExist == option && !hasKey(data, key)) {
		return true;
	}
	
	using RawJsonType = std::remove_const_t<std::remove_reference_t<JsonType>>;
	auto& d = data[key];
	if(!isType<RawJsonType>(key, data, errorReceiver, option)) {
		return false;
	}
	out = d.value<JsonType>();
	return true;
}

template<typename JsonType>
bool parseArrayItems(
	std::vector<JsonType>& out,
	std::string const& key,
	JsonType const& ignoreItem,
	json11::Json const& data,
	ErrorReceiver& errorReceiver,
	ParseOption option=ParseOption::None)
{
	if(ParseOption::SkipIfNoKeyExist == option && !hasKey(data, key)) {
		return true;
	}
	
	using RawJsonType = std::remove_const_t<std::remove_reference_t<JsonType>>;
	if(!isType<json11::Json::array>(key, data, errorReceiver, option)) {
		return false;
	}
	
	auto& items = data[key].array_items();
	std::vector<RawJsonType> result;	
	result.reserve(items.size());
	for(auto& op : items) {
		if(!op.is_type<RawJsonType>()) {
			errorReceiver.receive(ErrorReceiver::eWarnning, op.rowInFile(),
				"'" + key + R"(' element must be )" + typeString<JsonType>() + ".");
			continue;
		}
		auto v = op.value<JsonType>();
		if(ignoreItem == v) {
			continue;
		}
		result.push_back(v);
	}
	out = std::move(result);
	return true;
}

template<typename JsonType>
bool parseArrayItems(
	std::unordered_set<JsonType>& out,
	std::string const& key,
	JsonType const& ignoreItem,
	json11::Json const& data,
	ErrorReceiver& errorReceiver,
	ParseOption option=ParseOption::None)
{
	if(ParseOption::SkipIfNoKeyExist == option && !hasKey(data, key)) {
		return true;
	}
	
	using RawJsonType = std::remove_const_t<std::remove_reference_t<JsonType>>;
	if(!isType<json11::Json::array>(key, data, errorReceiver, option)) {
		return false;
	}
	
	auto& items = data[key].array_items();
	std::unordered_set<RawJsonType> result;	
	for(auto& op : items) {
		if(!op.is_type<RawJsonType>()) {
			errorReceiver.receive(ErrorReceiver::eWarnning, op.rowInFile(),
				"'" + key + R"(' element must be )" + typeString<JsonType>() + ".");
			continue;
		}
		auto v = op.value<JsonType>();
		if(ignoreItem == v) {
			continue;
		}
		result.insert(v);
	}
	out = std::move(result);
	return true;
}

bool parseOptionArray(
	std::vector<std::string>& out,
	const std::string key,
	const json11::Json& data,
	ErrorReceiver& errorReceiver,
	ParseOption option=ParseOption::None)
{
	if(ParseOption::SkipIfNoKeyExist == option && !hasKey(data, key)) {
		return true;
	}
	
	std::vector<std::string> rawData;
	if(!parseArrayItems(rawData, key, ""s, data, errorReceiver, option)) {
		return false;
	}
	
	decltype(rawData) result;
	result.reserve(rawData.size());
	for(auto&& dir : rawData) {
		for(auto&& o : split(dir, ' ')) {
			result.emplace_back(std::move(o));
		}
	}
	result.shrink_to_fit();
	out = std::move(result);
	return true;
}

template<typename ConfigItem> 
bool parseConfigItem(
	ConfigItem& out,
	std::string const& key,
	json11::Json const& data,
	ErrorReceiver& errorReceiver,
	ParseOption option=ParseOption::None)
{
	if(ParseOption::SkipIfNoKeyExist == option && !hasKey(data, key)) {
		return true;
	}

	std::remove_reference_t<ConfigItem> item;
	if(!ItemParser<ConfigItem>::parse(item, data[key], errorReceiver)) {
		return false;;
	}
	out = std::move(item);
	return true;
}

template<typename ConfigItem> 
bool parseConfigItemArray(
	std::vector<ConfigItem>& out,
	std::string const& key,
	json11::Json const& data,
	ErrorReceiver& errorReceiver,
	ParseOption option=ParseOption::None)
{
	if(ParseOption::SkipIfNoKeyExist == option && !hasKey(data, key)) {
		return true;
	}
	
	std::vector<json11::Json::object> array_;
	if(!parseArrayItems(array_, key, {}, data, errorReceiver, option) ) {
		return false;
	}

	bool isOK = true;
	std::vector<ConfigItem> result;
	result.reserve(array_.size());
	for(auto&& element : array_) {
		std::remove_reference_t<ConfigItem> item;
		if(!ItemParser<ConfigItem>::parse(item,
				json11::Json(std::move(element)), errorReceiver)) {
			isOK = false;
			continue;
		}
		result.push_back(std::move(item));
	}
	out = std::move(result);
	return isOK;
}

template<typename JsonType>
bool parseObject(
	std::unordered_map<std::string, JsonType>& out,
	std::string const& key,
	json11::Json const& data,
	ErrorReceiver& errorReceiver,
	ParseOption option=ParseOption::None)
{
	if(ParseOption::SkipIfNoKeyExist == option && !hasKey(data, key)) {
		return true;
	}
	
	json11::Json::object object;
	auto isSuccess = parseValue(object, key, data, errorReceiver, option);
	if(!isSuccess) {
		return false;
	}
	std::remove_reference_t<decltype(out)> result;
	for(auto const& [key, value] : object) {
		if(!value.is_type<JsonType>()) {
			errorReceiver.receive(ErrorReceiver::eWarnning, data.rowInFile(),
				"\"" + key + "\" value be must " + typeString<JsonType>() + "!");
			continue;
		}
		result.insert({key, value.value<JsonType>()});
	}
	out = std::move(result);
	return true;
}

template<typename EnumType>
bool parseStrToEnum(
	EnumType& out,
	const std::string key,
	std::function<EnumType(const std::string&)> converter,
	EnumType invalidType,
	const json11::Json& data,
	ErrorReceiver& errorReceiver,
	ParseOption option=ParseOption::None)
{
	if(ParseOption::SkipIfNoKeyExist == option && !hasKey(data, key)) {
		return true;
	}
	
	auto& keyEnum = data[key];
	if(!keyEnum.is_string()) {
		errorReceiver.receive(ErrorReceiver::eError, keyEnum.rowInFile(), "\"" + key + R"(" key must be string!)");
		return false;
	}
	auto enumStr = keyEnum.string_value();
	auto result = converter(enumStr);
	if(invalidType == result) {
		errorReceiver.receive(ErrorReceiver::eError, keyEnum.rowInFile(), "\"" + key + R"(" is unknown type! enum=")" + enumStr + "\"");
		return false;
	}
	out = result;
	return true;
}

void insertToJsonObj(
	json11::Json::object& out,
	bool doInsert,
	std::string const& key,
	json11::Json const& value)
{
	if(doInsert) {
		out.insert({key, value});
	}
}

template<typename T>
json11::Json::array toJsonArray(
	std::vector<T> const& src)
{
	json11::Json::array result;
	result.reserve(src.size());
	for(auto const& item : src) {
		result.push_back(toJson(item));
	}
	return result;
}

template<typename T>
json11::Json::array toJsonArray(
	std::unordered_map<std::string, T> const& src)
{
	json11::Json::array result;
	result.reserve(src.size());
	for(auto const& [key, value] : src) {
		result.push_back(toJson(value));
	}
	return result;
}

template<typename T>
json11::Json::object toJsonObject(
	std::unordered_map<std::string, T> const& src,
	std::function<json11::Json(T const& item)> predicate = toJson<T>)
{
	json11::Json::object result;
	for(auto const& [key, value] : src) {
		result.insert({key, predicate(value)});
	}
	return result;
}

//--------------------------------------------------------------------
//
//  typename UserValue
//
//--------------------------------------------------------------------
struct UserValueKey {
	inline static std::string const DefineTemplates = "defineTemplates";
	inline static std::string const DefineVariables = "defineVariables";
};

bool ItemParser<UserValue>::parse(
	UserValue& out,
	json11::Json const& data,
	ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	UserValue result;
	isOK &= parseConfigItemArray(result.defineTemplates,
		UserValueKey::DefineTemplates,
		data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseObject<std::string>(result.defineVariables,
		UserValueKey::DefineVariables,
		data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	out = std::move(result);
	return isOK;
}

json11::Json ItemParser<UserValue>::dump(
	UserValue const& src)
{
	json11::Json::object obj;
	insertToJsonObj(obj, !src.defineTemplates.empty(),
		UserValueKey::DefineTemplates,
		toJsonArray(src.defineTemplates)
	);
	insertToJsonObj(obj, !src.defineVariables.empty(),
		UserValueKey::DefineVariables, toJsonObject(src.defineVariables));
	return json11::Json(std::move(obj));
}

//--------------------------------------------------------------------
//
//  typename FileToProcess
//
//--------------------------------------------------------------------
struct FileToProcessKey {
	inline static std::string const Filepath = "filepath";
	inline static std::string const Template = "template";
	inline static std::string const CompileOptions = "compileOptions";
	inline static std::string const Preprocess = "preprocess";
	inline static std::string const Postprocess = "postprocess";
};

bool ItemParser<FileToProcess>::parse(
	FileToProcess& out,
	json11::Json const& data,
	ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	FileToProcess result;
	isOK &= parseValue(result.filepath, FileToProcessKey::Filepath, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.templateName, FileToProcessKey::Template, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseOptionArray(result.compileOptions, FileToProcessKey::CompileOptions, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.preprocess, FileToProcessKey::Preprocess, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.postprocess, FileToProcessKey::Postprocess, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= ItemParser<UserValue>::parse(result.userValue, data, errorReceiver);
	out = std::move(result);
	return isOK;
}

json11::Json ItemParser<FileToProcess>::dump(
	FileToProcess const& src)
{
	auto userValueJson = ItemParser<UserValue>::dump(src.userValue);
	json11::Json::object obj = userValueJson.object_items();
	insertToJsonObj(obj, !src.filepath.empty(), FileToProcessKey::Filepath, src.filepath);
	insertToJsonObj(obj, !src.templateName.empty(), FileToProcessKey::Template, src.templateName);
	insertToJsonObj(obj, !src.compileOptions.empty(), FileToProcessKey::CompileOptions, toJsonArray(src.compileOptions));
	insertToJsonObj(obj, !src.preprocess.empty(), FileToProcessKey::Preprocess, src.preprocess);
	insertToJsonObj(obj, !src.postprocess.empty(), FileToProcessKey::Postprocess, src.postprocess);
	return json11::Json(std::move(obj));
}

//--------------------------------------------------------------------
//
//  typename FileFilter
//
//--------------------------------------------------------------------
struct FileFilterKey {
	inline static const std::string Template = "template";
	inline static const std::string Extensions = "extensions";
 	inline static const std::string FilesToProcess = "filesToProcess";
};

bool ItemParser<FileFilter>::parse(
	FileFilter& out,
	json11::Json const& data,
	ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	FileFilter result;
	isOK &= parseArrayItems(result.extensions,
			FileFilterKey::Extensions, ""s, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseConfigItemArray(result.filesToProcess,
		FileFilterKey::FilesToProcess, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.templateName, FileFilterKey::Template,
		data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= ItemParser<UserValue>::parse(result.userValue, data, errorReceiver);

	out = std::move(result);
	return isOK;
}

json11::Json ItemParser<FileFilter>::dump(
	FileFilter const& src)
{
	auto userValueJson = ItemParser<UserValue>::dump(src.userValue);
	json11::Json::object obj = userValueJson.object_items();
	insertToJsonObj(obj, !src.extensions.empty(), FileFilterKey::Extensions, src.extensions);
	insertToJsonObj(obj, !src.templateName.empty(), FileFilterKey::Template, src.templateName);
	insertToJsonObj(obj, !src.filesToProcess.empty(),
			FileFilterKey::FilesToProcess,
			toJsonArray(src.filesToProcess)
		);
	return json11::Json(std::move(obj));
}

//--------------------------------------------------------------------
//
//  typename TargetDirectory
//
//--------------------------------------------------------------------
struct TargetDirectoryKey {
	inline static const std::string Path = "path";
	inline static const std::string Template = "template";
	inline static const std::string FileFilters = "fileFilters";
	inline static const std::string Ignores = "ignores";
};

bool ItemParser<TargetDirectory>::parse(
	TargetDirectory& out,
	json11::Json const& data,
	ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	TargetDirectory result;
	isOK &= parseValue(result.path, TargetDirectoryKey::Path, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.templateName, TargetDirectoryKey::Template, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseConfigItemArray(result.fileFilters, TargetDirectoryKey::FileFilters, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseArrayItems(result.ignores, TargetDirectoryKey::Ignores, ""s, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= ItemParser<UserValue>::parse(result.userValue, data, errorReceiver);
	out = std::move(result);
	return isOK;
}

json11::Json ItemParser<TargetDirectory>::dump(
	TargetDirectory const& src)
{
	auto userValueJson = ItemParser<UserValue>::dump(src.userValue);
	json11::Json::object obj = userValueJson.object_items();
	insertToJsonObj(obj, !src.path.empty(), TargetDirectoryKey::Path, src.path);
	insertToJsonObj(obj, !src.templateName.empty(), TargetDirectoryKey::Template, src.templateName);
	insertToJsonObj(obj, !src.fileFilters.empty(), TargetDirectoryKey::FileFilters, toJsonArray(src.fileFilters));
	insertToJsonObj(obj, !src.ignores.empty(), TargetDirectoryKey::Ignores, toJsonArray(src.ignores));	
	return json11::Json(std::move(obj));
}

//--------------------------------------------------------------------
//
//  typename OutputConfig
//
//--------------------------------------------------------------------
struct OutputConfigKey {
	inline static const std::string Name = "name";
	inline static const std::string OutputPath = "outputPath";
	inline static const std::string IntermediatePath = "intermediatePath";
};

bool ItemParser<OutputConfig>::parse(
	OutputConfig& out,
	json11::Json const& data,
	ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	OutputConfig result;
	isOK &= parseValue(result.name, OutputConfigKey::Name, data, errorReceiver);
	isOK &= parseValue(result.outputPath, OutputConfigKey::OutputPath, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.intermediatePath, OutputConfigKey::IntermediatePath, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= ItemParser<UserValue>::parse(result.userValue, data, errorReceiver);
	out = std::move(result);
	return isOK;
}

json11::Json ItemParser<OutputConfig>::dump(
	OutputConfig const& src)
{
	auto userValueJson = ItemParser<UserValue>::dump(src.userValue);
	json11::Json::object obj = userValueJson.object_items();
	insertToJsonObj(obj, !src.name.empty(), OutputConfigKey::Name, src.name);
	insertToJsonObj(obj, !src.outputPath.empty(), OutputConfigKey::OutputPath, src.outputPath);
	insertToJsonObj(obj, !src.intermediatePath.empty(), OutputConfigKey::IntermediatePath, src.intermediatePath);
	return json11::Json(std::move(obj));
}

//--------------------------------------------------------------------
//
//  typename BuildSetting
//
//--------------------------------------------------------------------
struct BuildSettingKey {
	inline static const std::string Name = "name";
	inline static const std::string Template = "template";
	inline static const std::string Compiler = "compiler";
	inline static const std::string TargetDirectories = "targetDirectories";
	inline static const std::string CompileOptions = "compileOptions";
	inline static const std::string IncludeDirectories = "includeDirectories";
	inline static const std::string LinkLibraries = "linkLibraries";
	inline static const std::string LibraryDirectories = "libraryDirectories";
	inline static const std::string LinkOptions = "linkOptions";
	inline static const std::string Dependences = "dependences";
	inline static const std::string OutputConfig = "outputConfig";
	inline static const std::string Preprocess = "preprocess";
	inline static const std::string LinkPreprocess = "linkPreprocess";
	inline static const std::string Postprocess = "postprocess";
};

bool ItemParser<BuildSetting>::parse(
	BuildSetting& out,
	json11::Json const& data,
	ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	BuildSetting result;
	isOK &= parseValue(result.name, BuildSettingKey::Name, data, errorReceiver);
	isOK &= parseValue(result.templateName, BuildSettingKey::Template, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.compiler, BuildSettingKey::Compiler, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseConfigItemArray(result.targetDirectories, BuildSettingKey::TargetDirectories, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseOptionArray(result.compileOptions, BuildSettingKey::CompileOptions, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseArrayItems(result.includeDirectories, BuildSettingKey::IncludeDirectories, ""s, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseOptionArray(result.linkLibraries, BuildSettingKey::LinkLibraries, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseArrayItems(result.libraryDirectories, BuildSettingKey::LibraryDirectories, ""s, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseOptionArray(result.linkOptions, BuildSettingKey::LinkOptions, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseArrayItems(result.dependences, BuildSettingKey::Dependences, ""s, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseConfigItem(result.outputConfig, BuildSettingKey::OutputConfig, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.preprocess, BuildSettingKey::Preprocess, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.linkPreprocess, BuildSettingKey::LinkPreprocess, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.postprocess, BuildSettingKey::Postprocess, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= ItemParser<UserValue>::parse(result.userValue, data, errorReceiver);

	out = std::move(result);
	return isOK;
}

json11::Json ItemParser<BuildSetting>::dump(
	BuildSetting const& src)
{
	auto userValueJson = ItemParser<UserValue>::dump(src.userValue);
	json11::Json::object obj = userValueJson.object_items();
	insertToJsonObj(obj, !src.name.empty(), BuildSettingKey::Name, src.name);
	insertToJsonObj(obj, !src.templateName.empty(), BuildSettingKey::Template, src.templateName);
	insertToJsonObj(obj, !src.compiler.empty(), BuildSettingKey::Compiler, src.compiler);
	insertToJsonObj(obj, !src.targetDirectories.empty(), BuildSettingKey::TargetDirectories, toJsonArray(src.targetDirectories));
	insertToJsonObj(obj, !src.compileOptions.empty(), BuildSettingKey::CompileOptions, toJsonArray(src.compileOptions));
	insertToJsonObj(obj, !src.includeDirectories.empty(), BuildSettingKey::IncludeDirectories, toJsonArray(src.includeDirectories));
	insertToJsonObj(obj, !src.linkLibraries.empty(), BuildSettingKey::LinkLibraries, toJsonArray(src.linkLibraries));
	insertToJsonObj(obj, !src.libraryDirectories.empty(), BuildSettingKey::LibraryDirectories, toJsonArray(src.libraryDirectories));
	insertToJsonObj(obj, !src.linkOptions.empty(), BuildSettingKey::LinkOptions, toJsonArray(src.linkOptions));
	insertToJsonObj(obj, !src.dependences.empty(), BuildSettingKey::Dependences, toJsonArray(src.dependences));
	insertToJsonObj(obj, true, BuildSettingKey::OutputConfig, toJson(src.outputConfig));
	insertToJsonObj(obj, !src.preprocess.empty(), BuildSettingKey::Preprocess, src.preprocess);
	insertToJsonObj(obj, !src.linkPreprocess.empty(), BuildSettingKey::LinkPreprocess, src.linkPreprocess);
	insertToJsonObj(obj, !src.postprocess.empty(), BuildSettingKey::Postprocess, src.postprocess);
	return json11::Json(std::move(obj));
}

//--------------------------------------------------------------------
//
//  typename Project
//
//--------------------------------------------------------------------
struct ProjectKey {
	inline static const std::string Name = "name";
	inline static const std::string Template = "template";
	inline static const std::string Type = "type";
	inline static const std::string BuildSettings = "buildSettings";
	inline static const std::string Version = "version";
	inline static const std::string MinorNumber = "minorNumber";
	inline static const std::string ReleaseNumber = "releaseNumber";
};

bool ItemParser<Project>::parse(
	Project& out,
	json11::Json const& data,
	ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	Project result;
	isOK &= parseValue(result.name, ProjectKey::Name, data, errorReceiver);
	isOK &= parseValue(result.templateName, ProjectKey::Template, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseStrToEnum<Project::Type>(result.type, ProjectKey::Type, Project::toType, Project::eUnknown, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.minorNumber, ProjectKey::MinorNumber, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.releaseNumber, ProjectKey::ReleaseNumber, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= ItemParser<UserValue>::parse(result.userValue, data, errorReceiver);

	std::vector<BuildSetting> buildSettings;
	isOK &= parseConfigItemArray(buildSettings, ProjectKey::BuildSettings, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	for(auto&& item : buildSettings) {
		result.buildSettings.insert({item.name, std::move(item)});
	}

	out = std::move(result);
	return isOK;
}

json11::Json ItemParser<Project>::dump(
	Project const& src)
{
	auto userValueJson = ItemParser<UserValue>::dump(src.userValue);
	json11::Json::object obj = userValueJson.object_items();
	insertToJsonObj(obj, !src.name.empty(), ProjectKey::Name, src.name);
	insertToJsonObj(obj, !src.templateName.empty(), ProjectKey::Template, src.templateName);
	insertToJsonObj(obj, src.type != Project::eUnknown, ProjectKey::Type, Project::toStr(src.type));
	insertToJsonObj(obj, !src.buildSettings.empty(), ProjectKey::BuildSettings, toJsonArray(src.buildSettings));
	insertToJsonObj(obj, src.version != 0, ProjectKey::Version, src.version);
	insertToJsonObj(obj, src.minorNumber != 0, ProjectKey::MinorNumber, src.minorNumber);
	insertToJsonObj(obj, src.releaseNumber != 0, ProjectKey::ReleaseNumber, src.releaseNumber);
	return json11::Json(std::move(obj));
}

//--------------------------------------------------------------------
//
//  typename CompileProcess
//
//--------------------------------------------------------------------
struct CompileProcessKey {
	inline static const std::string Type = "type";
	inline static const std::string Content = "content";
};

bool ItemParser<CompileProcess>::parse(
	CompileProcess& out,
	json11::Json const& data,
	ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	CompileProcess result;
	isOK &= parseStrToEnum<decltype(result.type)>(result.type, CompileProcessKey::Type, CompileProcess::toType, CompileProcess::eUnknown, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.content, CompileProcessKey::Content, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= ItemParser<UserValue>::parse(result.userValue, data, errorReceiver);
	out = std::move(result);
	return isOK;
}

json11::Json ItemParser<CompileProcess>::dump(
	CompileProcess const& src)
{
	auto userValueJson = ItemParser<UserValue>::dump(src.userValue);
	json11::Json::object obj = userValueJson.object_items();
	insertToJsonObj(obj, src.type != CompileProcess::eUnknown, CompileProcessKey::Type, CompileProcess::toStr(src.type));
	insertToJsonObj(obj, !src.content.empty(), CompileProcessKey::Content, src.content);
	return json11::Json(std::move(obj));
}

//--------------------------------------------------------------------
//
//  typename CompileTask
//
//--------------------------------------------------------------------
struct CompileTaskKey {
	inline static const std::string Command = "command";
	inline static const std::string InputOption = "inputOption";
	inline static const std::string OutputOption = "outputOption";
	inline static const std::string CommandPrefix = "commandPrefix";
	inline static const std::string CommandSuffix = "commandSuffix";
	inline static const std::string Preprocesses = "preprocesses";
	inline static const std::string Postprocesses = "postprocesses";
};

bool ItemParser<CompileTask>::parse(
	CompileTask& out,
	json11::Json const& data,
	ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	CompileTask result;
	isOK &= parseValue(result.command, CompileTaskKey::Command, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.inputOption, CompileTaskKey::InputOption, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.outputOption, CompileTaskKey::OutputOption, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.commandPrefix, CompileTaskKey::CommandPrefix, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseValue(result.commandSuffix, CompileTaskKey::CommandSuffix, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseConfigItemArray(result.preprocesses, CompileTaskKey::Preprocesses, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= parseConfigItemArray(result.postprocesses, CompileTaskKey::Postprocesses, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	isOK &= ItemParser<UserValue>::parse(result.userValue, data, errorReceiver);
	out = std::move(result);
	return isOK;
}

json11::Json ItemParser<CompileTask>::dump(
	CompileTask const& src)
{
	auto userValueJson = ItemParser<UserValue>::dump(src.userValue);
	json11::Json::object obj = userValueJson.object_items();
	insertToJsonObj(obj, !src.command.empty(), CompileTaskKey::Command, src.command);
	insertToJsonObj(obj, !src.inputOption.empty(), CompileTaskKey::InputOption, src.inputOption);
	insertToJsonObj(obj, !src.outputOption.empty(), CompileTaskKey::OutputOption, src.outputOption);
	insertToJsonObj(obj, !src.commandPrefix.empty(), CompileTaskKey::CommandPrefix, src.commandPrefix);
	insertToJsonObj(obj, !src.commandSuffix.empty(), CompileTaskKey::CommandSuffix, src.commandSuffix);
	insertToJsonObj(obj, !src.preprocesses.empty(), CompileTaskKey::Preprocesses, toJsonArray(src.preprocesses));
	insertToJsonObj(obj, !src.postprocesses.empty(), CompileTaskKey::Postprocesses, toJsonArray(src.postprocesses));
	return json11::Json(std::move(obj));
}

//--------------------------------------------------------------------
//
//  typename CompileTaskGroup
//
//--------------------------------------------------------------------
struct CompileTaskGroupKey {
	inline static const std::string CompileObj = "compileObj";
	inline static const std::string LinkObj = "linkObj";	
};

bool ItemParser<CompileTaskGroup>::parse(
	CompileTaskGroup& out,
	json11::Json const& data,
	ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	CompileTaskGroup result;
	isOK &= parseConfigItem(result.compileObj, CompileTaskGroupKey::CompileObj, data, errorReceiver);
	isOK &= parseConfigItem(result.linkObj, CompileTaskGroupKey::LinkObj, data, errorReceiver);
	isOK &= ItemParser<UserValue>::parse(result.userValue, data, errorReceiver);
	out = std::move(result);
	return isOK;
}

json11::Json ItemParser<CompileTaskGroup>::dump(
	CompileTaskGroup const& src)
{
	auto userValueJson = ItemParser<UserValue>::dump(src.userValue);
	json11::Json::object obj = userValueJson.object_items();
	insertToJsonObj(obj, true, CompileTaskGroupKey::CompileObj, toJson(src.compileObj));
	insertToJsonObj(obj, true, CompileTaskGroupKey::LinkObj, toJson(src.linkObj));
	return json11::Json(std::move(obj));
}

//--------------------------------------------------------------------
//
//  typename CustomCompiler
//
//--------------------------------------------------------------------
struct CustomCompilerKey {
	inline static const std::string Name = "name";
	inline static const std::string Exe = "exe";
	inline static const std::string Static = "static";
	inline static const std::string Shared = "shared";
};

bool ItemParser<CustomCompiler>::parse(
	CustomCompiler& out,
	json11::Json const& data,
	ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	CustomCompiler result;
	isOK &= parseValue(result.name, CustomCompilerKey::Name, data, errorReceiver);
	isOK &= parseConfigItem(result.exe, CustomCompilerKey::Exe, data, errorReceiver);
	isOK &= parseConfigItem(result.staticLib, CustomCompilerKey::Static, data, errorReceiver);
	isOK &= parseConfigItem(result.sharedLib, CustomCompilerKey::Shared, data, errorReceiver);
	isOK &= ItemParser<UserValue>::parse(result.userValue, data, errorReceiver);
	out = std::move(result);
	return isOK;
}

json11::Json ItemParser<CustomCompiler>::dump(
	CustomCompiler const& src)
{
	auto userValueJson = ItemParser<UserValue>::dump(src.userValue);
	json11::Json::object obj = userValueJson.object_items();
	insertToJsonObj(obj, !src.name.empty(), CustomCompilerKey::Name, src.name);
	insertToJsonObj(obj, true, CustomCompilerKey::Exe, toJson(src.exe));
	insertToJsonObj(obj, true, CustomCompilerKey::Static, toJson(src.staticLib));
	insertToJsonObj(obj, true, CustomCompilerKey::Shared, toJson(src.sharedLib));
	return json11::Json(std::move(obj));
}

//--------------------------------------------------------------------
//
//  typename RootConfig
//
//--------------------------------------------------------------------
struct RootConfigKey {
	inline static const std::string Projects = "projects";
	inline static const std::string CustomCompilers = "customCompilers";
};

bool ItemParser<RootConfig>::parse(
	RootConfig& out,
	json11::Json const& data,
	ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	RootConfig result;
	std::vector<Project> projects;
	isOK &= parseConfigItemArray(projects, RootConfigKey::Projects, data, errorReceiver);
	for(auto&& item : projects) {
		result.projects.insert({item.name, std::move(item)});
	}
	std::vector<CustomCompiler> customCompilers;
	isOK &= parseConfigItemArray(customCompilers, RootConfigKey::CustomCompilers, data, errorReceiver, ParseOption::SkipIfNoKeyExist);
	for(auto&& item : customCompilers) {
		result.customCompilers.insert({item.name, std::move(item)});
	}
	isOK &= ItemParser<UserValue>::parse(result.userValue, data, errorReceiver);
	out = std::move(result);
	return isOK;
}

json11::Json ItemParser<RootConfig>::dump(
	RootConfig const& src)
{
	auto userValueJson = ItemParser<UserValue>::dump(src.userValue);
	json11::Json::object obj = userValueJson.object_items();
	insertToJsonObj(obj, !src.projects.empty(), RootConfigKey::Projects, toJsonArray(src.projects));
	insertToJsonObj(obj, !src.customCompilers.empty(), RootConfigKey::CustomCompilers, toJsonArray(src.customCompilers));
	return json11::Json(std::move(obj));
}


//--------------------------------------------------------------------
//
//  typename TemplateItem
//
//--------------------------------------------------------------------
struct TemplateItemKey {
	inline static std::string ItemType = "itemType";
	inline static std::string Name = "name";
};


struct ParseItem : boost::static_visitor<bool>
{
	const json11::Json& data;
	ErrorReceiver& errorReceiver;
	ParseItem(const json11::Json& data, ErrorReceiver& errorReceiver)
		: data(data)
		, errorReceiver(errorReceiver)
	{}
	
	template<typename ItemType>
	bool operator()(ItemType& item)const {
		return ItemParser<ItemType>::parse(item, data, errorReceiver);
	}
};

bool ItemParser<TemplateItem>::parse(
	TemplateItem& out,
	json11::Json const& data,
	ErrorReceiver& errorReceiver)
{
	bool isOK = true;
	TemplateItem result;
	isOK &= parseStrToEnum<decltype(result.itemType)>(result.itemType, TemplateItemKey::ItemType, TemplateItem::toItemType, TemplateItemType::eUnknown, data, errorReceiver);
	isOK &= parseValue(result.name, TemplateItemKey::Name, data, errorReceiver);
	
	switch(result.itemType) {
	case eProject: 				result.item = Project(); break;
	case eBuildSetting: 		result.item = BuildSetting(); break;
	case eTargetDirectory:		result.item = TargetDirectory(); break;
	case eFileFilter:			result.item = FileFilter(); break;
	case eFileToProcess:		result.item = FileToProcess(); break;
	default:
		errorReceiver.receive(ErrorReceiver::eError, data[TemplateItemKey::ItemType].rowInFile(), "Unknwon itemType...");
		return false;
	}	
	if(!boost::apply_visitor(ParseItem(data, errorReceiver), result.item)) {
		errorReceiver.receive(ErrorReceiver::eError, data.rowInFile(), "Failed to parse defineTemplate item...");
		return false;
	}

	out = std::move(result);
	return isOK;
}

struct DumpItem : boost::static_visitor<json11::Json>
{
	template<typename ItemType>
	json11::Json operator()(ItemType const& item)const {
		return ItemParser<ItemType>::dump(item);
	}
};

json11::Json ItemParser<TemplateItem>::dump(
	TemplateItem const& src)
{
	auto itemJson = boost::apply_visitor(DumpItem(), src.item);
	if(json11::Json() == itemJson) {
		return json11::Json();
	}

	json11::Json::object obj = itemJson.object_items();
	insertToJsonObj(obj, src.itemType != TemplateItemType::eUnknown, TemplateItemKey::ItemType, TemplateItem::toStr(src.itemType));
	insertToJsonObj(obj, !src.name.empty(), TemplateItemKey::Name, src.name);
	return json11::Json(std::move(obj));
}

//--------------------------------------------------------------------
//
//  typename boost::blank (dummy)
//
//--------------------------------------------------------------------
bool ItemParser<boost::blank>::parse(boost::blank& out, json11::Json const& data, ErrorReceiver& errorReceiver)
{ return false; }

json11::Json ItemParser<boost::blank>::dump(boost::blank const& src)
{
	return {};
}

}
