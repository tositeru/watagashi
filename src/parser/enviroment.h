#pragma once

#include <vector>

#include "source.h"
#include "indent.h"
#include "parseMode.h"
#include "scope.h"

namespace parser
{

struct Enviroment
{
    Source source;
    Indent indent;
    std::vector<std::shared_ptr<IParseMode>> modeStack;
    std::vector<Scope> scopeStack;

    explicit Enviroment(char const* source_, std::size_t length);

    void pushMode(std::shared_ptr<IParseMode> pMode);
    void popMode();

    void pushScope(Scope && scope);
    void pushScope(Scope const& scope);
    void popScope();

    std::shared_ptr<IParseMode> currentMode();
    Scope& currentScope();
    Scope const& currentScope() const;
    int compareIndentLevel(int level);
    Scope& globalScope();
    Scope const& globalScope()const;
};

}