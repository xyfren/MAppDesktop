#include "MApp.h"

int __cdecl wmain(int argc, wchar_t *argv[])
{
	SetConsoleOutputCP(CP_UTF8);

    
	MApp p_app;;
	return p_app.run();
	
}