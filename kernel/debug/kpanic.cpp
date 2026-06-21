// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/debug/kpanic.hpp"
#include "kernel/debug/kprint.hpp"

namespace kernel::debug {

// =================================================================================================
// Halt
// =================================================================================================

[[noreturn]] static void halt() {
    for (;;) {
        asm volatile("cli; hlt");
    }
}

// =================================================================================================
// Kernel panic
// =================================================================================================

[[noreturn]] void kpanic(const char* message) {
    KPRINTLN("[panic] {}", message);
    halt();
}

[[noreturn]] void kpanic_with_code(const char* message, u64 code) {
    KPRINTLN("[panic] {}", message);
    KPRINTLN("[panic] code={:x}", code);
    halt();
}

} // namespace kernel::debug