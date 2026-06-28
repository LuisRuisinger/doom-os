#include "kernel/debug/kprint.hpp"

#include "../arch/x86_64/serial/serial.hpp"

namespace kernel::debug {
namespace detail {
using kernel::core::i32;
using kernel::core::i64;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::uptr;
using kernel::core::usize;

// =================================================================================================
// Backend
// =================================================================================================

void backend_emit_char(char value) { arch::x86_64::serial::write_char(value); }

void backend_emit_bytes(const char *value, usize length) {
    if (value == nullptr) {
        arch::x86_64::serial::write("<null>");
        return;
    }

    for (usize index = 0; index < length; ++index) {
        arch::x86_64::serial::write_char(value[index]);
    }
}

void backend_emit_c_string(const char *value) {
    if (value == nullptr) {
        arch::x86_64::serial::write("<null>");
        return;
    }

    arch::x86_64::serial::write(value);
}

void backend_emit_decimal_u64(u64 value) {
    char  buffer[32];
    usize index = 0;

    if (value == 0) {
        arch::x86_64::serial::write_char('0');
        return;
    }

    while (value != 0) {
        buffer[index++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        arch::x86_64::serial::write_char(buffer[--index]);
    }
}

void backend_emit_decimal_i64(i64 value) {
    if (value < 0) {
        arch::x86_64::serial::write_char('-');

        const u64 magnitude = static_cast<u64>(-(value + 1)) + 1;
        backend_emit_decimal_u64(magnitude);
        return;
    }

    backend_emit_decimal_u64(static_cast<u64>(value));
}

void backend_emit_hex_u64(u64 value) {
    static constexpr char digits[] = "0123456789ABCDEF";

    arch::x86_64::serial::write("0x");

    for (i32 shift = 60; shift >= 0; shift -= 4) {
        const u8 nibble = static_cast<u8>((value >> shift) & 0xFULL);
        arch::x86_64::serial::write_char(digits[nibble]);
    }
}

void backend_emit_pointer(const volatile void *value) {
    backend_emit_hex_u64(static_cast<u64>(reinterpret_cast<uptr>(value)));
}
}  // namespace detail

// =================================================================================================
// Public API
// =================================================================================================

void kprint_init() { arch::x86_64::serial::init(); }
}  // namespace kernel::debug