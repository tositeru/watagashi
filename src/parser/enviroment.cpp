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

std::shared_ptr<IParseMode> Enviroment::currentMode() {
    return this->modeStack.back();
}

}
