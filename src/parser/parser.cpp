#include "parser.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include <boost/utility/string_view.hpp>

#include "../utility.h"

#include "parserUtility.h"
#include "source.h"
#include "indent.h"
#include "line.h"
#include "enviroment.h"

using namespace std;

namespace parser
{

//----------------------------------------------------------------------------------
//
//  common functions
//
//----------------------------------------------------------------------------------

ParseResult parse(boost::filesystem::path const& filepath, ParserDesc const& desc)
{
    auto source = readFile(filepath);
    for (int i = static_cast<int>(source.size())-1; 0 <= i; --i) {
        if ('\0' != source[i]) {
            source.resize(i+1);
            break;
        }
    }
    ParserDesc usedDesc = desc;
    if (usedDesc.location.empty()) {
        usedDesc.location = Location(filepath, 0);
    }
    return parse(source, usedDesc);
}

bool tryParseLine(Enviroment& env, Line line, std::function<bool()> predicate)
{
    bool isGetLine = false;
    try {
        isGetLine = predicate();

    } catch (ParserException& e) {
        cerr << "line=" << env.calCurrentRow() << " : " << line.string_view() << "\n"
            << e.what() << endl;
        isGetLine = true;
    } catch (boost::exception& e) {
        cerr << "line=" << env.calCurrentRow() << " : " << line.string_view() << endl;
        ExceptionHandlerSetter::handleBoostException(e);
        isGetLine = true;
    } catch (...) {
        cerr << "line=" << env.calCurrentRow() << " : " << line.string_view() << "\n"
             << "occur unknown error..." << endl;
        isGetLine = true;
    }
    return isGetLine;
}

ParseResult parse(char const* source_, std::size_t length, ParserDesc const& desc)
{
    Enviroment env(source_, length);
    env.externObj = desc.externObj;
    env.globalScope().value() = desc.globalObj;
    env.location = desc.location;

    parse(env);

    ParseResult result;
    result.globalObj = std::move(env.globalScope().value());
    result.returnValues = std::move(env.returnValues);
    return std::move(result);
}

void parse(Enviroment& env)
{
    bool isGetLine = true;
    Line line(nullptr, 0, 0);
    while (!env.source.isEof()) {
        if(isGetLine) {
            line = env.source.getLine(true);
        }
        isGetLine = tryParseLine(env, line, [&]() {
            auto workLine = line;

            // There is a possibility that The current mode may be discarded
            //   due to such reasons as mode switching.
            // For this reason, it holds the current mode as a local variable.
            auto pMode = env.currentMode();
            switch (pMode->preprocess(env, workLine)) {
            case IParseMode::Result::NextLine:  return true;
            case IParseMode::Result::Redo:      return false;
            default: break;
            }

            // the code below is necessary because it may change the state of the mode stack in preprocessing.
            pMode = env.currentMode();
            switch (pMode->parse(env, workLine)) {
            case IParseMode::Result::NextLine:  return true;
            case IParseMode::Result::Redo:      return false;
            default: break;
            }

            return true;
        });
    }
    tryParseLine(env, Line(nullptr, 0, 0), [&]() {
        if (2 <= env.scopeStack.size()) {
            while (2 <= env.scopeStack.size()) {
                env.closeTopScope();
            }
        }
        return true;
    });
}

void showValue(Value const& value)
{
    switch (value.type) {
    case Value::Type::Bool:     cout << "Bool: " << value.toString() << endl; break;
    case Value::Type::String:   cout << "String: '" << value.get<Value::string>() << "'" << endl; break;
    case Value::Type::Number:   cout << "Number: " << value.get<Value::number>() << endl; break;
    case Value::Type::Function: cout << "Function:" << value.toString() << endl; break;
    case Value::Type::Array:
    {
        auto& arr = value.get<Value::array>();
        cout << "Array: size=" << arr.size() << endl;
        for (auto&& element : arr) {
            cout << "  ";
            if (Value::Type::String == element.type
                || Value::Type::Number == element.type) {
                showValue(element);
            } else {
                cout << Value::toString(element.type) << endl;
            }
        }
        break;
    }
    case Value::Type::Object:
    {
        auto& obj = value.get<Value::object>();
        cout << "Object: member count=" << obj.members.size() << endl;
        for (auto&& member : obj.members) {
            cout << "  " << member.first << " = ";
            switch (member.second.type) {
            case Value::Type::String: [[fallthrough]];
            case Value::Type::Number: [[fallthrough]];
            case Value::Type::Bool:
                showValue(member.second);
                break;
            default:
                cout << Value::toString(member.second.type) << endl;
            }
        }
        break;
    }
    case Value::Type::ObjectDefined:
    {
        auto& defined = value.get<ObjectDefined>();
        cout << "ObjectDefined: member count=" << defined.members.size() << endl;
        for (auto&& member : defined.members) {
            cout << "  " << member.first << ": " << Value::toString(member.second.type);
            if (Value::Type::None != member.second.defaultValue.type) {
                cout << " exist default value";
            }
            cout << endl;
        }
        break;
    }
    }
}

void confirmValueInInteractive(Value const& rootValue)
{
    cout << "confirm value mode" << endl;
    cout << "enter ':show' when you want to confirm all root scope value." << endl;
    cout << "enter ':fin' when you want to quit." << endl;
    bool isLoop = true;
    while (isLoop) {
        cout << ">";
        std::string nestName;
        cin >> nestName;
        if (":fin" == nestName) {
            isLoop = false;
            continue;
        }
        if (":show" == nestName) {
            showValue(rootValue);
            continue;
        }

        auto line = Line(nestName.c_str(), 0, nestName.size());
        auto [nameList, endPos] = parseName(line, 0);

        bool doFound = true;
        Value const* pValue = &rootValue;
        for (auto&& nameView : nameList) {
            auto name = nameView.to_string();
            if (!pValue->isExsitChild(name)) {
                doFound = false;
                break;
            }
            auto& child = pValue->getChild(name);
            pValue = &child;
        }
        if (!doFound) {
            cout << "!!error!! Don't found '" << nestName << "'" << endl;
            if (nullptr != pValue) {
                cout << "show parent value" << endl;
                showValue(*pValue);
            }
            continue;
        }

        showValue(*pValue);
    }
}

}