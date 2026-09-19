#include "application.h"
#ifdef PLATFORM_WINDOWS
int wmain(int argc, wchar_t* argv[])
#else
int main(int argc, char* argv[])
#endif
{
    return app_main(argc, (platform_string*)argv);
}