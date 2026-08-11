// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/debug/kprint.hpp"

#include "kernel/debug/console.hpp"

namespace kernel::debug {
namespace detail {
using kernel::core::i32;
using kernel::core::i64;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::uptr;
using kernel::core::usize;

namespace {

constexpr const char *NULL_TEXT = "<null>";

}  // namespace

// =================================================================================================
// Transport
//
// The only two functions that touch the sink. Everything below builds on them, so no formatting
// code has an opinion about where the bytes go.
// =================================================================================================

void emit_char(char value)
{
    console_write(&value, 1);
}

void emit_bytes(const char *value, usize length)
{
    if (value == nullptr) {
        emit_c_string(NULL_TEXT);
        return;
    }

    console_write(value, length);
}

// =================================================================================================
// Formatting
// =================================================================================================

void emit_c_string(const char *value)
{
    if (value == nullptr) {
        console_write(NULL_TEXT, c_string_length(NULL_TEXT));
        return;
    }

    console_write(value, c_string_length(value));
}

void emit_decimal_u64(u64 value)
{
    char  buffer[20];
    usize index = 0;

    if (value == 0) {
        emit_char('0');
        return;
    }

    while (value != 0) {
        buffer[index++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }

    // Produced least significant digit first, so emit it back to front.
    while (index > 0)
        emit_char(buffer[--index]);
}

void emit_decimal_i64(i64 value)
{
    if (value < 0) {
        emit_char('-');

        // Negating the most negative value overflows, so step in from it first.
        const u64 magnitude = static_cast<u64>(-(value + 1)) + 1;

        emit_decimal_u64(magnitude);
        return;
    }

    emit_decimal_u64(static_cast<u64>(value));
}

void emit_hex_u64(u64 value)
{
    static constexpr char DIGITS[] = "0123456789ABCDEF";

    char  buffer[18] = {'0', 'x'};
    usize index = 2;

    for (i32 shift = 60; shift >= 0; shift -= 4)
        buffer[index++] = DIGITS[static_cast<u8>((value >> shift) & 0xFULL)];

    console_write(buffer, index);
}

void emit_pointer(const volatile void *value)
{
    emit_hex_u64(static_cast<u64>(reinterpret_cast<uptr>(value)));
}

}  // namespace detail
}  // namespace kernel::debug
