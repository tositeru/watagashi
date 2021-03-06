#pragma once

#include <string>
#include <fstream>
#include <boost/filesystem.hpp>

std::string readFile(const boost::filesystem::path& filepath);
bool createDirectory(const boost::filesystem::path& path);

std::vector<std::string> split(const std::string& str, char delimiter);

bool runCommand(char const* command);
bool matchFilepath(const std::string& patternStr, const boost::filesystem::path& filepath, const boost::filesystem::path& standardPath);

inline bool runCommand(std::string const& command) {
    return runCommand(command.c_str());
}

class Finally
{
    std::function<void()> mPred;
public:
    explicit Finally(std::function<void()> pred)
        : mPred(pred)
    {}
    Finally(const Finally&)=delete;
    void operator=(const Finally&)=delete;
    
    ~Finally()
    {
        this->mPred();
    }
};

namespace std
{

template<> struct hash<boost::filesystem::path>
{
    size_t operator()(boost::filesystem::path const& right) const {
        return hash<std::string>()(right.string());
    }
};

}

std::string demangle(char const* name);
inline std::string demangle(std::type_info const& type_info)
{
    return demangle(type_info.name());
}
