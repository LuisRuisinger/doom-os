// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/debug/kprint.hpp"

// =================================================================================================
// Application lifecycle
//
// The kernel has one application-facing symbol: doom_os_start_application. This file decides what
// that means for the configured image. With an application linked, main is a strong unresolved
// reference from a direct object, so an archive-only application can satisfy it without a C shim or
// --whole-archive. Without an application, no reference to main is emitted.
// =================================================================================================

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
