#include "enviroment.h"

#include <boost/range/adaptor/reversed.hpp>

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

Value* Enviroment::searchValue(std::list<std::string> const& nestName, bool doGetParent)
{
    auto constThis = const_cast<Enviroment const*>(this);
    return const_cast<Value*>(constThis->searchValue(nestName, doGetParent));
}

Value const* Enviroment::searchValue(std::list<std::string> const& nestName, bool doGetParent)const
{
    std::string errorMessage;
    auto pValue = this->searchValue(nestName, doGetParent, &errorMessage);
    if (pValue == nullptr) {
        AWESOME_THROW(std::invalid_argument) << errorMessage;
    }
    return pValue;
}

Value* Enviroment::searchValue(std::list<std::string> const& nestName, bool doGetParent, std::string* pOutErrorMessage)
{
    auto constThis = const_cast<Enviroment const*>(this);
    return const_cast<Value*>(constThis->searchValue(nestName, doGetParent, pOutErrorMessage));
}

Value const* Enviroment::searchValue(std::list<std::string> const& nestName, bool doGetParent, std::string* pOutErrorMessage)const
{
    if (nestName.empty()) {
        if (pOutErrorMessage) {
            *pOutErrorMessage = "An empty nestName was passed...";
        }
        return nullptr;
    }

    Value const* pResult = nullptr;
    // Find the starting point of the appropriate place.
    auto rootName = nestName.front();
    for (auto&& pScope : boost::adaptors::reverse(this->scopeStack)) {
        pResult = pScope->searchVariable(rootName);
        if (pResult) {
            break;
        }
    }
    if (nullptr == pResult) {
        if (!this->externObj.isExsitChild(rootName)) {
            if (pOutErrorMessage) {
                std::stringstream message;
                message << "Don't found '" << rootName << "' in enviroment.";
                *pOutErrorMessage = message.str();
            }
            return nullptr;
        }
        pResult = &this->externObj.getChild(rootName);
    }

    // Find a assignment destination at starting point.
    if (2 <= nestName.size()) {
        auto nestNameIt = ++nestName.begin();
        auto endIt = nestName.end();
        if (doGetParent) {
            --endIt;
        }
        for (; endIt != nestNameIt; ++nestNameIt) {
            if (!pResult->isExsitChild(*nestNameIt)) {
                // error code
                if (pOutErrorMessage) {
                    auto it = nestName.begin();
                    auto name = *it;
                    for (++it; nestNameIt != it; ++it) {
                        name = "." + (*it);
                    }
                    std::stringstream message;
                    if (Value::Type::Object == pResult->type) {
                        message << "Don't found '" << *nestNameIt << "' in '" << name << "'.";
                    } else {
                        message << *nestNameIt << "' in '" << name << "' not equal Object.";
                    }
                    *pOutErrorMessage = message.str();
                }
                return nullptr;
            }

            pResult = &pResult->getChild(*nestNameIt);
        }
    }
    return pResult;
}


ObjectDefined const* Enviroment::searchObjdectDefined(std::list<boost::string_view> const& nestName)const
{
    if (1 == nestName.size()) {
        if ("Array" == *nestName.begin()) {
            return &Value::arrayDefined;
        } else if ("Object" == *nestName.begin()) {
            return &Value::emptyObjectDefined;
        }
    }
    auto nestNameList = toStringList(nestName);
    Value const* pValue = this->searchValue(nestNameList, false);
    if (Value::Type::ObjectDefined != pValue->type) {
        throw MakeException<ScopeSearchingException>()
            << "Value is not ObjectDefined." << MAKE_EXCEPTION;
    }
    return &pValue->get<ObjectDefined>();
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
    return level - static_cast<int>(this->scopeStack.size());
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
