#pragma once

#include <fstream>
#include <boost/filesystem.hpp>

namespace watagashi
{

std::string readFile(const boost::filesystem::path& filepath);
bool createDirectory(const boost::filesystem::path& path);

std::vector<std::string> split(const std::string& str, char delimiter);

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



}