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
		const boost::filesystem::path& inputPath,
		const boost::filesystem::path& outputPath,
		const std::vector<std::string>& includePaths);

public:
	IncludeFileAnalyzer() = default;
	~IncludeFileAnalyzer() = default;

	void analysis(
		std::unordered_set<std::string>* pInOut,
		const boost::filesystem::path& sourceFilePath,
		const std::vector<std::string>& includePaths)const;
};

}
