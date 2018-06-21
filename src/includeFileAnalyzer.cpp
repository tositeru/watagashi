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

bool IncludeFileAnalyzer::sCheckUpdateTime(
    const boost::filesystem::path& inputPath,
    const boost::filesystem::path& outputPath,
    const std::vector<std::string>& includePaths)
{
    if( fs::exists(outputPath) ) {
        std::unordered_set<std::string> includeFiles;
        IncludeFileAnalyzer includeFileAnalyzer;
        includeFileAnalyzer.analysis(&includeFiles, inputPath, includePaths);
        includeFiles.insert(inputPath.string());
        
        bool isCompile = false;
        for(auto&& filepath : includeFiles) {
            if( fs::last_write_time(outputPath) < fs::last_write_time(filepath)) {
                isCompile = true;
                break;
            }
        } 
        if(!isCompile) {
            return false;
        }
    }
    return true;
}

void IncludeFileAnalyzer::analysis(
    std::unordered_set<std::string>* pInOut,
    const boost::filesystem::path& sourceFilePath,
    const std::vector<std::string>& includePaths)const
{
    std::ifstream in(sourceFilePath.string());
    if(in.bad()) {
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
    while(std::regex_search(it, end, match, pattern)) {
        it = match[0].second;
        switch(match[1].str()[0]) {
        case '<': {
            for(auto& includeDir : includePaths) {
                auto path = fs::absolute(includeDir)/match[2].str();
                if(!fs::exists(path)) {
                    continue;
                }
                auto pathStr = path.string();
                if(pInOut->count(pathStr)) {
                    continue;
                }
                foundIncludeFiles.insert(pathStr);
                pInOut->insert(pathStr);
                break;
            }
            break;
        }
        case '"': {
            auto path = fs::absolute(sourceFilePath.parent_path()/match[2].str());
            if( !fs::exists(path) ) {
                break;
            }
            auto pathStr = path.string();
            if(pInOut->count(pathStr)) {
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
        [&](const fs::path& path){
            this->analysis(pInOut, path, includePaths);
        });
}

}
