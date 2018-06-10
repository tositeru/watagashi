#include "includeFileAnalyzer.h"

#include <iostream>
#include <regex>
#include <boost/range/algorithm/for_each.hpp>

#ifdef USE_CLANG_LIBTOOLING
#include <clang/Driver/Options.h>
#include <clang/AST/AST.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/Frontend/ASTConsumers.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#endif

#include "exception.hpp"

using namespace std;
namespace fs = boost::filesystem;

namespace watagasi
{

#ifdef USE_CLANG_LIBTOOLING
// link options when use clang libtooling
//"`llvm-config --ldflags`",
//"-lclangTooling -lclangFrontendTool -lclangFrontend -lclangDriver -lclangSerialization -lclangCodeGen -lclangParse -lclangSema -lclangStaticAnalyzerFrontend -lclangStaticAnalyzerCheckers -lclangStaticAnalyzerCore -lclangAnalysis -lclangARCMigrate -lclangRewrite -lclangRewriteFrontend -lclangEdit -lclangAST -lclangLex -lclangBasic -lclang",
//"`llvm-config --libs all`",
//"`llvm-config --system-libs all`"

class SearchIncludeFilesAction : public clang::PreprocessOnlyAction
{
public:
	struct OutputData
	{
		std::unordered_set<std::string> includePaths;
	};
	
	class Factory : public clang::tooling::FrontendActionFactory
	{
	private:
		std::shared_ptr<OutputData> mpOutputData;
		
	public:
		Factory(std::shared_ptr<OutputData>& pOutputData)
			: mpOutputData(pOutputData)
		{}
		
		clang::FrontendAction* create()override
		{
			return new SearchIncludeFilesAction(this->mpOutputData);
		}
	};
	
	class FindIncludeFiles : public clang::PPCallbacks
	{
		std::shared_ptr<OutputData>& mpOutputData;
		
	public:
		FindIncludeFiles(std::shared_ptr<OutputData>& pOutputData)
			: mpOutputData(pOutputData)
		{}
		
		void InclusionDirective(
			clang::SourceLocation hashLoc,
			const clang::Token& includeToken,
			StringRef fileName,
			bool isAngled,
			clang::CharSourceRange filenameType,
			const clang::FileEntry* file,
			StringRef searchPath,
			StringRef relativePath,
			const clang::Module* imported)
		{
			fs::path p = fs::absolute(file->getName().str());
			this->mpOutputData->includePaths.insert(p.string());
		}
	};

	class DummyASTConsumer : public clang::ASTConsumer
	{
	public:
		explicit DummyASTConsumer(clang::CompilerInstance* CI)
		{ }
	};

public:
	static llvm::cl::OptionCategory myToolCategory;

public:
	SearchIncludeFilesAction(std::shared_ptr<OutputData>& pOutputData)
		: mpOutputData(pOutputData)
	{}
	
	bool BeginSourceFileAction(clang::CompilerInstance &ci)override
	{
		auto& pp = ci.getPreprocessor();
		
		auto pFindIncludeFileCallback = std::make_unique<FindIncludeFiles>(this->mpOutputData);
		pp.addPPCallbacks(std::move(pFindIncludeFileCallback));
		return true;
	}
	
	void EndSourceFileAction()override
	{ }
	
	virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& CI, StringRef file)override
	{
		return std::make_unique<DummyASTConsumer>(&CI);
	}
	
private:
	std::shared_ptr<OutputData> mpOutputData;

};

//
//	clang::tooling:;CommonComandParserだと複数回実行すると、
//	以前解析した結果が残ったままになったので、それを回避するために作成
//
using namespace llvm;
class MyCommandParser
{
public:
	MyCommandParser(int& argc, const char** argv, llvm::cl::OptionCategory& category)
	{
	  llvm::Error Err = this->init(argc, argv, category);
		if (Err) {
		llvm::report_fatal_error(
		    "MyCommandParser: failed to parse command-line arguments. " +
		    llvm::toString(std::move(Err)));
		}
	}

	Error init(
   			int &argc,
   			const char **argv,
   			cl::OptionCategory &Category,
   			const char *Overview = nullptr)
	{
		cl::NumOccurrencesFlag OccurrencesFlag = llvm::cl::OneOrMore;
		static cl::opt<bool> Help("h", cl::desc("Alias for -help"), cl::Hidden,
		                        cl::sub(*cl::AllSubCommands));
		
		static cl::opt<std::string> BuildPath("p", cl::desc("Build path"),
		                                    cl::Optional, cl::cat(Category),
		                                    cl::sub(*cl::AllSubCommands));
		
		cl::list<std::string> SourcePaths(
		  cl::Positional, cl::desc("<source0> [... <sourceN>]"), OccurrencesFlag,
		  cl::cat(Category), cl::sub(*cl::AllSubCommands));
		static cl::list<std::string> ArgsAfter(
		  "extra-arg",
		  cl::desc("Additional argument to append to the compiler command line"),
		  cl::cat(Category), cl::sub(*cl::AllSubCommands));
		
		static cl::list<std::string> ArgsBefore(
		  "extra-arg-before",
		  cl::desc("Additional argument to prepend to the compiler command line"),
		  cl::cat(Category), cl::sub(*cl::AllSubCommands));
		
		cl::ResetAllOptionOccurrences();
		
		cl::HideUnrelatedOptions(Category);
		
		std::string ErrorMessage;
		this->mpCompilations =
		  clang::tooling::FixedCompilationDatabase::loadFromCommandLine(argc, argv, ErrorMessage);
		if (!ErrorMessage.empty())
		ErrorMessage.append("\n");
		llvm::raw_string_ostream OS(ErrorMessage);
		// Stop initializing if command-line option parsing failed.
		if (!cl::ParseCommandLineOptions(argc, argv, Overview, &OS)) {
			SourcePaths.removeArgument();
			OS.flush();
			return llvm::make_error<llvm::StringError>("[MyOptionsParser]: " +
			                                               ErrorMessage,
			                                           llvm::inconvertibleErrorCode());
		}
		SourcePaths.removeArgument();
		
		cl::PrintOptionValues();
		
		this->mSourcePathList = SourcePaths;
		if ((OccurrencesFlag == cl::ZeroOrMore || OccurrencesFlag == cl::Optional) &&
		  	this->mSourcePathList.empty()) {
			return llvm::Error::success();
		}
		
		if (!this->mpCompilations) {
			if (!BuildPath.empty()) {
			  this->mpCompilations =
			      clang::tooling::CompilationDatabase::autoDetectFromDirectory(BuildPath, ErrorMessage);
			} else {
			  this->mpCompilations = clang::tooling::CompilationDatabase::autoDetectFromSource(SourcePaths[0],
			                                                           ErrorMessage);
			}
			if (!this->mpCompilations) {
			  llvm::errs() << "Error while trying to load a compilation database:\n"
			               << ErrorMessage << "Running without flags.\n";
			  this->mpCompilations.reset(
			      new clang::tooling::FixedCompilationDatabase(".", std::vector<std::string>()));
			}
		}
		auto AdjustingCompilations =
		  llvm::make_unique<clang::tooling::ArgumentsAdjustingCompilations>(
		      std::move(this->mpCompilations));
		this->mAdjuster =
		  getInsertArgumentAdjuster(ArgsBefore, clang::tooling::ArgumentInsertPosition::BEGIN);
		this->mAdjuster = clang::tooling::combineAdjusters(
		  std::move(this->mAdjuster),
		  getInsertArgumentAdjuster(ArgsAfter, clang::tooling::ArgumentInsertPosition::END));
		AdjustingCompilations->appendArgumentsAdjuster(this->mAdjuster);
		this->mpCompilations = std::move(AdjustingCompilations);
		return llvm::Error::success();
	}
	
	clang::tooling::CompilationDatabase& getCompilations() { return *this->mpCompilations.get(); }
	const std::vector<std::string>& getSourcePathList()const { return this->mSourcePathList; }
	clang::tooling::ArgumentsAdjuster getArgumentsAdjuster() { return this->mAdjuster; }
	
private:
	std::unique_ptr<clang::tooling::CompilationDatabase> mpCompilations;
	std::vector<std::string> mSourcePathList;
	clang::tooling::ArgumentsAdjuster mAdjuster;

};

llvm::cl::OptionCategory SearchIncludeFilesAction::myToolCategory("watagasi");
#endif

//=============================================================================
//
//	class IncludeFileAnalyzer
//
//=============================================================================

#ifdef USE_CLANG_LIBTOOLING

bool IncludeFileAnalyzer::sCheckUpdateTime(
	const fs::path& inputPath,
	const fs::path& outputPath,
	const std::vector<std::string>& compileOptions)
{
	if( fs::exists(outputPath) ) {
		IncludeFileAnalyzer includeFileAnalyzer;
		auto includeFiles = includeFileAnalyzer.analysis(inputPath, compileOptions);
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

std::unordered_set<std::string> IncludeFileAnalyzer::analysis(
	const fs::path& sourceFilePath,
	const std::vector<std::string>& compileOptions)const
{
	auto filepath = sourceFilePath.string();
	auto pClangPath = std::getenv("CLANG_PATH");
	std::string clangIncludePath = nullptr == pClangPath ? BUILD_IN_CLANG_PATH : pClangPath;
	clangIncludePath += "/include";
	auto cmds = this->makeLibtoolingCommand(filepath, compileOptions, clangIncludePath);

	int optionCount = static_cast<int>(cmds.size());
	MyCommandParser op(optionCount, cmds.data(), SearchIncludeFilesAction::myToolCategory);
	
	auto pOutputData = std::make_shared<SearchIncludeFilesAction::OutputData>();
	clang::tooling::ClangTool tool(op.getCompilations(), op.getSourcePathList());
	int result = tool.run(std::make_unique<SearchIncludeFilesAction::Factory>(pOutputData).get());
	
	if(0 != result) {
		AWESOME_THROW(std::runtime_error) << "Failed analysis independent include file.";
	}
	return pOutputData->includePaths;
}

std::vector<const char*> IncludeFileAnalyzer::makeLibtoolingCommand(
	const std::string& filepath,
	const std::vector<std::string>& compileOptions,
	const std::string& clangIncludePath)const
{
	std::vector<const char*> cmds;
	cmds.reserve(7 + compileOptions.size());
	cmds.push_back("dummy");
	cmds.push_back(filepath.c_str());
	cmds.push_back("--");
	cmds.push_back("-Wunused-command-line-argument");
	cmds.push_back("-Wno-invalid-pp-token");
	cmds.push_back("-I");
	cmds.push_back(clangIncludePath.c_str());
	for(auto&& opt : compileOptions) {
		cmds.push_back(opt.c_str());
	}
	return cmds;
}
#endif

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
