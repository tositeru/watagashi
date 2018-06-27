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
static boost::optional<boost::string_view> pickupName(Line const& line, size_t start);
static std::list<boost::string_view> parseName(size_t& tailPos, Line const& line, size_t start);
static OperatorType parseOperator(size_t& outTailPos, Line const& line, size_t start);

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

    if (line.length() <= 0) {
        return;
    }

    size_t p = 0;
    auto nestNames = parseName(p, line, 0);
    if (nestNames.empty()) {
        cerr << env.source.row() << ": syntax error!! found invalid character.\n"
            << line.string_view() << endl;
        return;
    }

    auto opType = parseOperator(p, line, p);
    if (OperatorType::Unknown == opType) {
        cerr << env.source.row() << ": syntax error!! found unknown operater.\n"
            << line.string_view() << endl;
        return;
    }

    cout << env.source.row() << "," << env.indent.currentLevel() << ":"
        << " name=";
    for (auto&& n : nestNames) {
        cout << n << ".";
    }
    cout << " op=" << toString(opType) << endl;
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
            auto p = line.incrementPos(static_cast<size_t>(2), [](auto line, auto p) { return isCommentChar(line.get(p)); });
            env.pushMode(std::make_shared<MultiLineCommentParseMode>(static_cast<int>(p)));
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

boost::optional<boost::string_view> pickupName(Line const& line, size_t start)
{
    // search including illegal characters
    auto p = line.incrementPos(start, [](auto line, auto p) {
        auto c = line.get(p);
        return !(isSpace(c) || isChildOrderAccessorString(c) || isParentOrderAccessorChar(c));
    });
    auto nameStr = Line(line.get(start), 0, p - start);
    if ( nameStr.find(0, [](auto line, auto p) { return !isNameChar(line.get(p)); }) ) {
        return boost::none;
    }
    return nameStr.string_view();
}

struct INameAccessorParseTraits
{
    enum class Type {
        None,
        ParentOrder,
        ChildOrder,
    };

    virtual Type type()const = 0;
    virtual bool isAccessKeyward(Line const&, size_t) const= 0;
    virtual void push(std::list<boost::string_view>&, boost::string_view const&) const = 0;
    virtual size_t skipAccessChars(Line const&, size_t) const = 0;
};

struct ParentOrderAccessorParseTraits final : public INameAccessorParseTraits
{
    Type type()const override { return Type::ParentOrder; }
    bool isAccessKeyward(Line const& line, size_t p) const override{
        return isParentOrderAccessorChar(line.get(p));
    }
    void push(std::list<boost::string_view>& out, boost::string_view const& name) const override {
        out.push_back(name);
    }
    size_t skipAccessChars(Line const& line, size_t p) const override {
        return line.incrementPos(p, [](auto line, auto p) { return !isNameChar(line.get(p)); });
    }

    static ParentOrderAccessorParseTraits const& instance() {
        static ParentOrderAccessorParseTraits const inst;
        return inst;
    }
};

struct ChildOrderAccessorParseTraits final : public INameAccessorParseTraits
{
    Type type()const override { return Type::ChildOrder; }
    bool isAccessKeyward(Line const& line, size_t p) const override {
        return isChildOrderAccessorString(boost::string_view(line.get(p), line.length() - p));
    }
    void push(std::list<boost::string_view>& out, boost::string_view const& name) const override {
        out.push_front(name);
    }
    size_t skipAccessChars(Line const& line, size_t p) const override {
        return line.incrementPos(p, [](auto line, auto p) { return !isSpace(line.get(p)); });
    }

    static ChildOrderAccessorParseTraits const& instance() {
        static ChildOrderAccessorParseTraits const inst;
        return inst;
    }
};

std::list<boost::string_view> parseName(size_t& tailPos, Line const& line, size_t start)
{
    size_t p = start;
    auto firstNameView = pickupName(line, p);
    if (!firstNameView) {
        return {};
    }
    p += firstNameView->length();
    tailPos = p;

    INameAccessorParseTraits const* pAccessorParser = nullptr;
    p = line.skipSpace(p);
    if (isParentOrderAccessorChar(line.get(p))) {
        // the name of the parent order
        pAccessorParser = &ParentOrderAccessorParseTraits::instance();
    } else if (isChildOrderAccessorString(line.substr(p, 2))) {
        // the name of the children order
        pAccessorParser = &ChildOrderAccessorParseTraits::instance();
    } else {
        return { *firstNameView };
    }
    p = pAccessorParser->skipAccessChars(line, p);

    std::list<boost::string_view> result;
    result.push_back(*firstNameView);
    while (!line.isEndLine(p)) {
        p = line.skipSpace(p);
        auto debugLine = Line(line.get(p), 0, line.length() - p);
        auto nameView = pickupName(line, p);
        if (!nameView) {
            return {};
        }
        pAccessorParser->push(result, *nameView);
        p += nameView->length();
        p = line.skipSpace(p);

        if (!pAccessorParser->isAccessKeyward(line, p)) {
            break;
        }
        p = pAccessorParser->skipAccessChars(line, p);
    }

    tailPos = p;
    return result;
}

OperatorType parseOperator(size_t& outTailPos, Line const& line, size_t start)
{
    start = line.skipSpace(start);
    auto p = line.incrementPos(start, [](auto line, auto p) { return !isSpace(line.get(p)); });
    auto opType = toOperatorType(line.substr(start, p - start));
    outTailPos = p;
    return opType;
}

}