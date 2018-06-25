#include "normal.h"

#include <iostream>

#include <boost/optional.hpp>

#include "../parserUtility.h"
#include "../line.h"
#include "../enviroment.h"

#include "multiLineComment.h"

using namespace std;

namespace parser
{

ErrorHandle evalIndent(Enviroment& env, Line& line);
static CommentType evalComment(Enviroment& env, Line& line);

void NormalParseMode::parse(Enviroment& env, Line& line)
{
    auto commentType = evalComment(env, line);
    if (CommentType::MultiLine == commentType) {
        return;
    }

    if (auto error = evalIndent(env, line)) {
        cerr << error.message() << endl;
        return;
    }

    cout << env.source.row() << "," << env.indent.currentLevel() << ":" << line.string() << endl;
}

ErrorHandle evalIndent(Enviroment& env, Line& line)
{
    auto indent = line.getIndent();
    auto level = env.indent.calLevel(indent); 

    if (-1 == level) {
        if (!line.find(0, [](auto line, auto p) {  return !isSpace(line.get(p)); })) {
            // skip if blank line
            return {};
        }

        if (0 == env.indent.currentLevel()) {
            env.indent.setUnit(indent);
        } else {
            return MakeErrorHandle(env.source.row()) << "invalid indent";
        }
    } else {
        if (1 == level) {
            env.indent.setUnit(indent);
        }
        env.indent.setLevel(level);
    }

    line.resize(indent.size(), 0);
    return {};
}

CommentType evalComment(Enviroment& env, Line& line)
{
    if (isCommentChar(line.get(0))) {
        if (2 <= line.length() && isCommentChar(line.get(1))) {
            //if multiple line comment
            int p = line.incrementPos(2, [](auto line, auto p) { return isCommentChar(line.get(p)); });
            env.pushMode(std::make_shared<MultiLineCommentParseMode>(p));
            return CommentType::MultiLine;
        } else {
            //if single line comment
            line.resize(0, line.length());
            return CommentType::SingleLine;
        }
    } else if (isCommentChar(line.rget(0))) {
        // check comment at the end of the line.
        // count '#' at the end of line.
        size_t p = line.incrementPos(1, [](auto line, auto p) {
            return isCommentChar(line.rget(p));
        });

        auto const KEYWARD_COUNT = p;
        // search pair '###...'
        size_t count = 0;
        for (count = 0; !line.isEndLine(p); ++p) {
            if (count == KEYWARD_COUNT
                && false == isCommentChar(line.rget(p))) {
                break;
            }

            count = isCommentChar(line.rget(p)) ? count + 1 : 0;
        }
        if (count == KEYWARD_COUNT) {
            // remove space characters before comment.
            p = line.incrementPos(p, [](auto line, auto p) {
                return isSpace(line.rget(p));
            });
            line.resize(0, p);
        }
        return CommentType::EndOfLine;
    }
    return CommentType::None;
}

}