#include "utility.h"

#include <sstream>
 
namespace fs = boost::filesystem;

namespace watagasi
{

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

}