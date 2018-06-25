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
}

}