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

Value parse(boost::filesystem::path const& filepath, ParserDesc const& desc)
{
    auto source = readFile(filepath);
    for (int i = static_cast<int>(source.size())-1; 0 <= i; --i) {
        if ('\0' != source[i]) {
            source.resize(i+1);
            break;
        }
    }
    return parse(source, desc);
}

Value parse(char const* source_, std::size_t length, ParserDesc const& desc)
{
    Enviroment env(source_, length);
    env.externObj = desc.externObj;

    bool isGetLine = true;
    Line line(nullptr, 0, 0);
    while (!env.source.isEof()) {
        if(isGetLine) {
            line = env.source.getLine(true);
        }
        isGetLine = true;
        try {
            auto workLine = line;

            // There is a possibility that The current mode may be discarded
            //   due to such reasons as mode switching.
            // For this reason, it holds the current mode as a local variable.
            auto pMode = env.currentMode();
            switch (pMode->preprocess(env, workLine)) {
            case IParseMode::Result::NextLine:  isGetLine = true;  continue;
            case IParseMode::Result::Redo:      isGetLine = false;  continue;
            default: break;
            }

            // the code below is necessary because it may change the state of the mode stack in preprocessing.
            pMode = env.currentMode();
            switch (pMode->parse(env, workLine)) {
            case IParseMode::Result::NextLine:  isGetLine = true;  continue;
            case IParseMode::Result::Redo:      isGetLine = false;  continue;
            default: break;
            }

        } catch (ParserException& e) {
            cerr << e.what() << "\n"
                << "line=" << env.source.row() << " : " << line.string_view() << endl;
            isGetLine = true;
        } catch(boost::exception& e) {
            if (ParserException const* p = dynamic_cast<ParserException const *>(&e)) {
                ExceptionHandlerSetter::handleBoostException(e);
                cerr << "line=" << env.source.row() << " : " << line.string_view() << endl;
            } else{
                ExceptionHandlerSetter::handleBoostException(e);
            }
            isGetLine = true;
        } catch (...) {
            cerr << "occur unknown error... \n"
                << "line=" << env.source.row() << " : " << line.string_view() << endl;
            isGetLine = true;
        }
    }
    try {
        if (2 <= env.scopeStack.size()) {
            while (2 <=  env.scopeStack.size()) {
                closeTopScope(env);
            }
        }
    } catch (ParserException& e) {
        cerr << env.source.row() << ": " << e.what() << "\n"
            << line.string_view() << endl;
    }

    return std::move(env.currentScope().value());
}

void showValue(Value const& value)
{
    switch (value.type) {
    case Value::Type::Bool:     cout << "Bool: " << value.toString() << endl; break;
    case Value::Type::String:   cout << "String: '" << value.get<Value::string>() << "'" << endl; break;
    case Value::Type::Number:   cout << "Number: " << value.get<Value::number>() << endl; break;
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