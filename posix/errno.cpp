// =================================================================================================
// POSIX ABI files
// =================================================================================================

#include "posix/abi.hpp"

// One thread of control, one errno. libc-shaped callers reach it through errno-location helpers,
// which posix/stubs.cpp points here.
extern "C" {
int errno = 0;
}
