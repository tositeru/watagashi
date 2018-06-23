#pragma once 

#include <vector>
#include <string>
#include <unordered_set>
#include <boost/filesystem.hpp>

namespace watagashi
{

class IncludeFileAnalyzer
{
public:
    static bool sCheckUpdateTime(
        boost::filesystem::path const& inputFilepath,
        boost::filesystem::path const& outputFilepath,
        std::vector<std::string> const& includeDirectories);

    static bool sCheckUpdateTime(
        boost::filesystem::path const& inputFilepath,
        boost::filesystem::path const& outputFilepath,
        std::unordered_set<boost::filesystem::path> const& includeDirectories);

public:
    IncludeFileAnalyzer() = delete;
    ~IncludeFileAnalyzer() = delete;

};

}
