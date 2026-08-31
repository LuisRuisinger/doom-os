// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/debug/kprint.hpp"

// =================================================================================================
// Null application
//
// What an image with no application linked against it calls. Weak, so that any translation unit
// defining main - which every C program already does - replaces it at link time without the
// kernel naming an entry point of its own.
//
// It lives here rather than in the kernel because it stands in for the application, not for a
// service the kernel provides. The kernel calls main; it does not supply one.
// =================================================================================================

__attribute__((weak)) int main(int argc, char **argv)
{
    static_cast<void>(argc);
    static_cast<void>(argv);

    KPRINTLN("[app] no application linked");

    return 0;
}
