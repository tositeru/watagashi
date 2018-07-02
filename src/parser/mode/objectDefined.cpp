#include "objectDefined.h"

#include <iostream>

#include "../line.h"
#include "../enviroment.h"

using namespace std;

namespace parser
{

IParseMode::Result ObjectDefinedParseMode::parse(Enviroment& env, Line& line)
{
    auto commentType = evalComment(env, line);
    if (CommentType::MultiLine == commentType) {
        return Result::Next;
    }

    int level;
    if (auto error = evalIndent(env, line, level)) {
        cerr << error.message() << endl;
        return Result::Next;
    }

    if (line.length() <= 0) {
        return Result::Next;
    }

    int resultCompared = env.compareIndentLevel(level);
    if (0 < resultCompared) {
        cerr << env.source.row() << ": syntax error!! An indent above current scope depth is described.\n"
            << line.string_view() << endl;
        return Result::Next;
    }

    if (resultCompared < 0) {
        while (0 != env.compareIndentLevel(level)) {
            // close top scope.
            if (auto error = closeTopScope(env)) {
                cerr << error.message() << "\n"
                    << line.string_view() << endl;
            }
        }
        if (env.currentMode().get() != this) {
            return Result::Redo;
        }
    }

}

}