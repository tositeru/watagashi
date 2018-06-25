#include "utility.h"

#include <sstream>
#include <iostream>

#ifdef WIN32
#include <Windows.h>
#include <Dbghelp.h>
#pragma comment(lib, "Dbghelp.lib")

#endif

namespace fs = boost::filesystem;
using namespace std;

std::string readFile(const fs::path& filepath)
{
    if(!fs::exists(filepath)) {
        return "";
    }
    
    std::uintmax_t length = fs::file_size(filepath);    
    std::string buf;
    buf.resize(length);
    
    std::ifstream in(filepath.string());
    in.read(&buf[0], length);
    
    return buf;
}

bool createDirectory(const fs::path& path)
{
    if(!fs::exists(path)) {
        if(!fs::create_directories(path)) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> split(const std::string& str, char delimiter)
{
    std::istringstream stream(str);
    
    std::string field;
    std::vector<std::string> result;
    while(std::getline(stream, field, delimiter)) {
        result.push_back(field);
    }
    return result;
}

bool runCommand(char const* command)
{
    return 0 == std::system(command);
}

std::string demangle(char const* name)
{
#if defined(_WIN32) || defined(WIN32) || defined(BOOST_WINDOWS)
    std::string result;
    result.resize(std::strlen(name) * 2);// Enough buffers are necessary
    DWORD length = UnDecorateSymbolName(name, &result[0], (DWORD)result.size(), UNDNAME_COMPLETE);
    if (0 == length) {
        throw std::runtime_error("");
    }
    result.resize(length);
    return result;
#else
    int status;
    char* p = abi::__cxa_demangle(ti.name(), nullptr, nullptr, &status);
    if (!p) {
        switch (status) {
        case -1: cerr << ("A memory allocation failure occurred.") << endl;
        case -2: cerr << (std::string(name) + " is not a valid name under the C++ ABI mangling rules.") << endl;
        case -3: cerr << ("demangle(): One of the arguments is invalid.") << endl;
        default: cerr << ("demangle(): unknown error.") << endl;
        }
        return "";
    }
std:string result = p;
    free(p);
    return result;
#endif
}
