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

    this->scopeStack.push_back( std::make_shared<NormalScope>(
          std::list<std::string>{ std::string("@@GLOBAL") }
        , Value().init(Value::Type::Object))
    );
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

void Enviroment::pushScope(std::shared_ptr<IScope> pScope)
{
    this->scopeStack.push_back(pScope);
}

void Enviroment::popScope()
{
    if (this->scopeStack.size() <= 1) {
        AWESOME_THROW(std::runtime_error)
            << "invalid the mode stack operation. Never empty the mode stack.";
    }
    this->scopeStack.pop_back();
}

void Enviroment::closeTopScope()
{
    // note: At the timing of closing the scope,
    //  we assign values to appropriate places.
    // etc) object member, array element or other places.

    auto pCurrentScope = this->currentScopePointer();
    this->popScope();
    pCurrentScope->close(*this);
}

std::shared_ptr<IParseMode> Enviroment::currentMode() {
    return this->modeStack.back();
}

IScope& Enviroment::currentScope()
{
    return *this->scopeStack.back();
}

IScope const& Enviroment::currentScope() const
{
    return *this->scopeStack.back();
}

std::shared_ptr<IScope>& Enviroment::currentScopePointer()
{
    return this->scopeStack.back();
}

std::shared_ptr<IScope> const& Enviroment::currentScopePointer() const
{
    return this->scopeStack.back();
}

int Enviroment::compareIndentLevel(int level)
{
    level += 1;
    if (level == this->scopeStack.size()) {
        return 0;
    }
    return level - this->scopeStack.size();
}

IScope& Enviroment::globalScope()
{
    return **this->scopeStack.begin();
}

IScope const& Enviroment::globalScope()const
{
    return **this->scopeStack.begin();
}

}
