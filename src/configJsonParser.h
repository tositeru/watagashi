#pragma once

#include "config.h"
#include "errorReceiver.h"
#include "json11/json11.hpp"
#include "exception.hpp"

namespace watagashi
{
	class ErrorReceiver;
}

namespace watagashi::config::json
{

template<typename T>
class ItemParser final
{
	static_assert(sizeof(T) < 0, "Please implement for T");
public:
	// Japanese -> ファイル内の全構文エラーを一回で検出するために例外は使っていない。
	// I don't use a explicit exception to detect all syntex error in config file.
	static bool parse(T& out, json11::Json const& data, ErrorReceiver& errorReceiver);
	static json11::Json dump(T const& src);
};

#define DEFINE_PARSER(ItemType) \
	template<> class ItemParser<ItemType> { \
	public: \
		static bool parse(ItemType& out, json11::Json const& data, ErrorReceiver& errorReceiver); \
		static json11::Json dump(ItemType const& src); \
	}

DEFINE_PARSER(UserValue);
DEFINE_PARSER(FileToProcess);
DEFINE_PARSER(FileFilter);
DEFINE_PARSER(TargetDirectory);
DEFINE_PARSER(OutputConfig);
DEFINE_PARSER(BuildSetting);
DEFINE_PARSER(Project);
DEFINE_PARSER(CompileProcess);
DEFINE_PARSER(CompileTask);
DEFINE_PARSER(CompileTaskGroup);
DEFINE_PARSER(CustomCompiler);
DEFINE_PARSER(RootConfig);
DEFINE_PARSER(TemplateItem);
DEFINE_PARSER(boost::blank);

template<typename ItemType>
bool parse(ItemType& out, const json11::Json& configJson)
{
	ErrorReceiver errorReceiver;
	bool isSuccess = ItemParser<ItemType>::parse(out, configJson, errorReceiver);
	if(errorReceiver.hasError()) {
		errorReceiver.print();
		std::cerr << "Exsit error in config file!" << std::endl;
	}
	errorReceiver.print(ErrorReceiver::eWarnning);
	return isSuccess;
}

template<typename ItemType>
json11::Json dump(ItemType const& item)
{
	return ItemParser<ItemType>::dump(item);
}

}
