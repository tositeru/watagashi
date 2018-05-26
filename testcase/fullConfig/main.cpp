#include <iostream>

#include <boost/filesystem.hpp>
#include <library.h>

using namespace std;
namespace fs = boost::filesystem;

int main()
{
	fs::path p("./");
	cout << fs::absolute(p) << endl;
	library::helloWatagasi();	
	return 0;
}
