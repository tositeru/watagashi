#pragma once

#include <vector>

#include "source.h"
#include "indent.h"
#include "parseMode.h"
#include "scope.h"
#include "location.h"

namespace parser
{

struct Enviroment
{
    Source source;
    Indent indent;
    std::vector<std::shared_ptr<IParseMode>> modeStack;
    std::vector<std::shared_ptr<IScope>> scopeStack;

    Value externObj;
    Location location;
    std::vector<Value> arguments;
    std::vector<Value> returnValues;
    enum class Status {
        StandBy,
        Run,
        Suspension,
        Completion,
    };
    Status status;

    explicit Enviroment(char const* source_, std::size_t length);
    explicit Enviroment(std::string const& source_);
    Enviroment(Enviroment const&) = delete;
    Enviroment(Enviroment &&) = default;
    Enviroment& operator=(Enviroment const&) = delete;
    Enviroment& operator=(Enviroment &&) = default;

    void pushMode(std::shared_ptr<IParseMode> pMode);
    void popMode();

    void pushScope(std::shared_ptr<IScope> pScope);

    void popScope();
    void closeTopScope();

    Value* searchValue(std::list<std::string> const& nestName, bool doGetParent);
    Value const* searchValue(std::list<std::string> const& nestName, bool doGetParent)const;
    Value* searchValue(std::list<std::string> const& nestName, bool doGetParent, std::string* pOutErrorMessage);
    Value const* searchValue(std::list<std::string> const& nestName, bool doGetParent, std::string* pOutErrorMessage)const;
    Value const* searchTypeObject(std::list<boost::string_view> const& nestName)const;

    size_t calCurrentRow()const;

    std::shared_ptr<IParseMode> currentMode();
    IScope& currentScope();
    IScope const& currentScope() const;
    std::shared_ptr<IScope>& currentScopePointer();
    std::shared_ptr<IScope> const& currentScopePointer() const;
    int compareIndentLevel(int level);
    IScope& globalScope();
    IScope const& globalScope()const;
};

}