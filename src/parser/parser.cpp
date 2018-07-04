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

Value parse(boost::filesystem::path const& filepath)
{
    auto source = readFile(filepath);
    for (int i = static_cast<int>(source.size())-1; 0 <= i; --i) {
        if ('\0' != source[i]) {
            source.resize(i+1);
            break;
        }
    }
    return parse(source);
}

Value parse(char const* source_, std::size_t length)
{
    Enviroment env(source_, length);

    bool isGetLine = true;
    Line line(nullptr, 0, 0);
    while (!env.source.isEof()) {
        if(isGetLine) {
            line = env.source.getLine(true);
        }
        isGetLine = true;
        auto workLine = line;

        // There is a possibility that The current mode may be discarded
        //   due to such reasons as mode switching.
        // For this reason, it holds the current mode as a local variable.
        auto pMode = env.currentMode();
        auto result = pMode->parse(env, workLine);
        if (IParseMode::Result::Redo == result) {
            isGetLine = false;
        }
    }

    if (2 <= env.scopeStack.size()) {
        while (2 <=  env.scopeStack.size()) {
            env.popScope();
        }
    }

    auto& object = env.currentScope().value().get<Value::object>();
    cout << "member count=" << object.members.size() << endl;
    for (auto& [name, value] : object.members) {
        cout << "Type of " << name << " is " << Value::toString(value.type) << ":";
        switch (value.type) {
        case Value::Type::String: cout << "'" << value.get<Value::string>() << "'" << endl; break;
        case Value::Type::Number: cout << value.get<Value::number>() << endl;  break;
        case Value::Type::Array:
            cout << endl;
            for (auto& element : value.get<Value::array>()) {
                cout << "  " << "Type is " << Value::toString(element.type);
                if (Value::Type::String == element.type) {
                    cout << ": '" << element.toString() << "'"<< endl;
                } else {
                    cout << ": " << element.toString() << endl;
                }
            }
            cout << endl;
            break;
        case Value::Type::Object:
            cout << endl;
            for (auto& [childName, childValue] : value.get<Value::object>().members) {
                cout << "  " << "Type of " << childName << " is " << Value::toString(childValue.type) << ":";
                switch (childValue.type) {
                case Value::Type::String: cout << "'" << childValue.get<Value::string>() << "'" << endl; break;
                case Value::Type::Number: cout << childValue.get<Value::number>() << endl; break;
                default:
                    cout << childValue.toString() << endl;
                }
            }
            cout << endl;
            break;
        case Value::Type::ObjectDefined:
            cout << endl;
            for (auto&[childName, childValue] : value.get<ObjectDefined>().members) {
                cout << "  " << "Member Type of " << childName << " is " << Value::toString(childValue.type);
                if (Value::Type::None != childValue.defaultValue.type) {
                    cout << " by default=" << childValue.defaultValue.toString();
                }
                cout << endl;
            }
            break;
        default:
            cout << "(unknown)" << endl;
            break;
        }
    }
    ErrorHandle error;
    auto& arr3 = env.currentScope().value().getChild("array3", error).get<Value::array>();
    auto& elements = arr3.at(3).get<Value::array>();
    for (auto& e : elements) {
        cout << e.toString() << endl;
    }
    return std::move(env.currentScope().value());
}

void showValue(Value const& value)
{
    switch (value.type) {
    case Value::Type::String: cout << "String: '" << value.get<Value::string>() << "'" << endl; break;
    case Value::Type::Number: cout << "Number: " << value.get<Value::number>() << endl; break;
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
            if (Value::Type::String == member.second.type
                || Value::Type::Number == member.second.type) {
                showValue(member.second);
            } else {
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
        size_t endPos;
        auto line = Line(nestName.c_str(), 0, nestName.size());
        auto nameList = parseName(endPos, line, 0);

        ErrorHandle error;
        Value const* pValue = &rootValue;
        for (auto&& name : nameList) {
            auto& child = pValue->getChild(name.to_string(), error);
            if (error) {
                break;
            }
            pValue = &child;
        }
        if (error) {
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