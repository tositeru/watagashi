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
#ifdef USE_CLANG_LIBTOOLING
	static bool sCheckUpdateTime(
		const boost::filesystem::path& inputPath,
		const boost::filesystem::path& outputPath,
		const std::vector<std::string>& compileOptions);
#endif

	static bool sCheckUpdateTime(
		const boost::filesystem::path& inputPath,
		const boost::filesystem::path& outputPath,
		const std::vector<std::string>& includePaths);

public:
	IncludeFileAnalyzer() = default;
	~IncludeFileAnalyzer() = default;

#ifdef USE_CLANG_LIBTOOLING
	std::unordered_set<std::string> analysis(
		const boost::filesystem::path& sourceFilePath,
		const std::vector<std::string>& compileOptions)const;
#endif
	void analysis(
		std::unordered_set<std::string>* pInOut,
		const boost::filesystem::path& sourceFilePath,
		const std::vector<std::string>& includePaths)const;
		
private:
#ifdef USE_CLANG_LIBTOOLING
	std::vector<const char*> makeLibtoolingCommand(
		const std::string& filepath,
		const std::vector<std::string>& compileOptions,
		const std::string& clangIncludePath)const;
#endif

};

}
