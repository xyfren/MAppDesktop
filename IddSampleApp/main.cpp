#include "MApp.h"
#include "boost/locale.hpp"

int __cdecl wmain(int argc, wchar_t *argv[])
{
	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);
	boost::locale::generator gen;
	std::locale loc = gen("en_US.UTF-8");
	std::locale::global(loc);
	std::cout.imbue(loc);

	MApp p_app;;
	return p_app.run();
	
}