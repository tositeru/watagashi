#include "includeFileAnalyzer.h"

#include <iostream>
#include <regex>
#include <boost/range/algorithm/for_each.hpp>

#include "exception.hpp"

using namespace std;
namespace fs = boost::filesystem;

namespace watagashi
{

//=============================================================================
//
//    class IncludeFileAnalyzer
//
//=============================================================================

template<typename Container>
void analysis(
    std::unordered_set<std::string>* pInOut,
    boost::filesystem::path const& sourceFilePath,
    Container& includeDirectories)
{
    std::ifstream in(sourceFilePath.string());
    if (in.bad()) {
        AWESOME_THROW(std::invalid_argument) << "Failed to open " << sourceFilePath.string() << "...";
    }

    std::unordered_set<std::string> foundIncludeFiles;

    auto fileSize = fs::file_size(sourceFilePath);
    std::string fileContent;
    fileContent.resize(fileSize);
    in.read(&fileContent[0], fileSize);

    std::regex pattern(R"(#include\s*([<"])([\w./\\]+)[>"])");
    std::smatch match;
    auto it = fileContent.cbegin(), end = fileContent.cend();
    while (std::regex_search(it, end, match, pattern)) {
        it = match[0].second;
        switch (match[1].str()[0]) {
        case '<':
        {
            for (auto& includeDir : includeDirectories) {
                auto path = fs::absolute(includeDir) / match[2].str();
                if (!fs::exists(path)) {
                    continue;
                }
                auto pathStr = path.string();
                if (pInOut->count(pathStr)) {
                    continue;
                }
                foundIncludeFiles.insert(pathStr);
                pInOut->insert(pathStr);
                break;
            }
            break;
        }
        case '"':
        {
            auto path = fs::absolute(sourceFilePath.parent_path() / match[2].str());
            if (!fs::exists(path)) {
                break;
            }
            auto pathStr = path.string();
            if (pInOut->count(pathStr)) {
                break;
            }
            foundIncludeFiles.insert(pathStr);
            pInOut->insert(pathStr);
            break;
        }
        default:
            break;
        }
    }

    fileContent.clear();
    fileContent.shrink_to_fit();

    boost::range::for_each(
        foundIncludeFiles,
        [&](const fs::path& path) {
        analysis(pInOut, path, includeDirectories);
    });
}

template<typename Container>
bool checkUpdateTime(
    boost::filesystem::path const& inputFilepath,
    boost::filesystem::path const& outputFilepath,
    Container const& includeDirectories)
{
    if (fs::exists(outputFilepath)) {
        std::unordered_set<std::string> includeFiles;
        analysis(&includeFiles, inputFilepath, includeDirectories);
        includeFiles.insert(inputFilepath.string());

        bool isCompile = false;
        for (auto&& filepath : includeFiles) {
            if (fs::last_write_time(outputFilepath) < fs::last_write_time(filepath)) {
                isCompile = true;
                break;
            }
        }
        if (!isCompile) {
            return false;
        }
    }
    return true;
}

bool IncludeFileAnalyzer::sCheckUpdateTime(
    boost::filesystem::path const& inputFilepath,
    boost::filesystem::path const& outputFilepath,
    std::vector<std::string> const& includeDirectories)
{
    return checkUpdateTime(inputFilepath, outputFilepath, includeDirectories);
}

bool IncludeFileAnalyzer::sCheckUpdateTime(
    boost::filesystem::path const& inputFilepath,
    boost::filesystem::path const& outputFilepath,
    std::unordered_set<boost::filesystem::path> const& includeDirectories)
{
    return checkUpdateTime(inputFilepath, outputFilepath, includeDirectories);
}


}
