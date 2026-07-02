// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/runtime/init.hpp"

namespace kernel::runtime {

using init_function = void (*)();

// =================================================================================================
// Linker-provided symbols
// =================================================================================================

extern "C" init_function __init_array_start[];
extern "C" init_function __init_array_end[];

extern "C" init_function __fini_array_start[];
extern "C" init_function __fini_array_end[];

// =================================================================================================
// Global constructors
// =================================================================================================

void call_global_constructors() {
    for (init_function *fn = __init_array_start; fn != __init_array_end; ++fn)
        (*fn)();
}

// =================================================================================================
// Global destructors
// =================================================================================================

void call_global_destructors() {
    for (init_function *fn = __fini_array_end; fn != __fini_array_start;) {
        --fn;
        (*fn)();
    }
}

}  // namespace kernel::runtime