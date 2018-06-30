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

void parse(boost::filesystem::path const& filepath)
{
    auto source = readFile(filepath);
    for (int i = static_cast<int>(source.size())-1; 0 <= i; --i) {
        if ('\0' != source[i]) {
            source.resize(i+1);
            break;
        }
    }
    parse(source);
}

void parse(char const* source_, std::size_t length)
{
    Enviroment env(source_, length);

    while (!env.source.isEof()) {
        auto rawLine = env.source.getLine(true);
        auto workLine = rawLine;

        // There is a possibility that The current mode may be discarded
        //   due to such reasons as mode switching.
        // For this reason, it holds the current mode as a local variable.
        auto pMode = env.currentMode();
        pMode->parse(env, workLine);

    }

    if (2 <= env.scopeStack.size()) {
        while (2 <=  env.scopeStack.size()) {
            env.popScope();
        }
    }

    auto& object = env.currentScope().value.get<Value::object>();
    cout << "member count=" << object.size() << endl;
    for (auto& [name, value] : object) {
        cout << "Type of " << name << " is " << Value::toString(value.type) << ":";
        switch (value.type) {
        case Value::Type::String: cout << "'" << value.get<Value::string>() << "'" << endl; break;
        case Value::Type::Number: cout << value.get<Value::number>() << endl;  break;
        case Value::Type::Array:
            cout << endl;
            for (auto& element : value.get<Value::array>()) {
                cout << "  " << "Type is " << Value::toString(element.type)
                    << ": " << element.toString() << endl;
            }
            cout << endl;
            break;
        case Value::Type::Object:
            cout << endl;
            for (auto& [childName, childValue] : value.get<Value::object>()) {
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
        default:
            cout << "(unknown)" << endl;
            break;
        }
    }
    ErrorHandle error;
    auto& arr3 = env.currentScope().value.getChild("array3", error).get<Value::array>();
    auto& elements = arr3.at(3).get<Value::array>();
    for (auto& e : elements) {
        cout << e.toString() << endl;
    }
}

}