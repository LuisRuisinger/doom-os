
#include "kernel/debug/kprint.hpp"

#ifndef DOOM_OS_HAS_APPLICATION
#    define DOOM_OS_HAS_APPLICATION 0
#endif

extern "C" [[noreturn]] void _exit(int status);

#if DOOM_OS_HAS_APPLICATION
extern "C" int main(int argc, char **argv);
#endif

extern "C" [[noreturn]] void doom_os_start_application()
{
#if DOOM_OS_HAS_APPLICATION
    _exit(main(0, nullptr));
#else
    KPRINTLN("[app] no application linked");
    _exit(0);
#endif
}
