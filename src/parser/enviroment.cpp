#include "enviroment.h"

#include "../exception.hpp"
#include "mode/normal.h"

namespace parser
{

Enviroment::Enviroment(char const* source_, std::size_t length)
    : source(source_, length)
    , indent()
{
    this->modeStack.push_back(std::make_shared<NormalParseMode>());

    Scope initialScope(std::list<std::string>{ std::string("@@GLOBAL") }, Value().init(Value::Type::Object));
    this->scopeStack.push_back(initialScope);
}

void Enviroment::pushMode(std::shared_ptr<IParseMode> pMode)
{
    this->modeStack.push_back(std::move(pMode));
}

void Enviroment::popMode()
{
    if (this->modeStack.size() <= 1) {
        AWESOME_THROW(std::runtime_error)
            << "invalid the mode stack operation. Never empty the mode stack.";
    }
    this->modeStack.pop_back();
}

void Enviroment::pushScope(Scope && scope)
{
    this->scopeStack.push_back(std::move(scope));
}

void Enviroment::pushScope(Scope const& scope)
{
    this->scopeStack.push_back(scope);
}

void Enviroment::popScope()
{
    if (this->scopeStack.size() <= 1) {
        AWESOME_THROW(std::runtime_error)
            << "invalid the mode stack operation. Never empty the mode stack.";
    }
    this->scopeStack.pop_back();
}

std::shared_ptr<IParseMode> Enviroment::currentMode() {
    return this->modeStack.back();
}

Scope& Enviroment::currentScope()
{
    return this->scopeStack.back();
}

Scope const& Enviroment::currentScope() const
{
    return this->scopeStack.back();
}

int Enviroment::compareIndentLevel(int level)
{
    level += 1;
    if (level == this->scopeStack.size()) {
        return 0;
    }
    return level < this->scopeStack.size() ? -1 : 1;
}

Scope& Enviroment::globalScope()
{
    return *this->scopeStack.begin();
}

Scope const& Enviroment::globalScope()const
{
    return *this->scopeStack.begin();
}

}
