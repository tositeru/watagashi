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

    auto& object = boost::get<Value::object>(env.currentScope().value.data);
    cout << "member count=" << object.size() << endl;
    for (auto& [name, value] : object) {
        cout << "Type of " << name << " is " << Value::toString(value.type) << ":";
        switch (value.type) {
        case Value::Type::String: cout << "'" << boost::get<Value::string>(value.data) << "'" << endl; break;
        case Value::Type::Number: cout << boost::get<Value::number>(value.data) << endl;  break;
        case Value::Type::Array:
            cout << endl;
            for (auto& element : boost::get<Value::array>(value.data)) {
                cout << "  " << "Type is " << Value::toString(element.type) << endl;
            }
            cout << endl;
            break;
        case Value::Type::Object:
            cout << endl;
            for (auto& [childName, childValue] : boost::get<Value::object>(value.data)) {
                cout << "  " << "Type of " << childName << " is " << Value::toString(childValue.type) << ":";
                switch (childValue.type) {
                case Value::Type::String: cout << "'" << boost::get<Value::string>(childValue.data) << "'" << endl; break;
                case Value::Type::Number: cout << boost::get<Value::number>(childValue.data) << endl; break;
                default:
                    cout << endl;
                }
            }
            cout << endl;
            break;
        }
    }
}

}