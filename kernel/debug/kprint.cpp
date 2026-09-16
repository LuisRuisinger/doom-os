
#include "kernel/debug/kprint.hpp"

#include "kernel/core/array.hpp"
#include "kernel/core/cast.hpp"
#include "kernel/debug/console.hpp"

namespace kernel::debug {

namespace detail {

using kernel::core::i32;
using kernel::core::i64;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;
using kernel::core::utils::array;

namespace {

constexpr const char *NULL_TEXT = "<null>";

}  // namespace

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
    array<char, 20> buffer;
    usize           index = 0;

    if (value == 0) {
        emit_char('0');
        return;
    }

    while (value != 0) {
        buffer[index++] = ('0' + (value % 10)) as(char);
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
        const u64 magnitude = (-(value + 1)) as(u64) + 1;

        emit_decimal_u64(magnitude);
        return;
    }

    emit_decimal_u64(value as(u64));
}

void emit_hex_u64(u64 value)
{
    static constexpr char DIGITS[] = "0123456789ABCDEF";

    array<char, 18> buffer{'0', 'x'};
    usize           index = 2;

    for (i32 shift = 60; shift >= 0; shift -= 4)
        buffer[index++] = DIGITS[((value >> shift) & 0xFULL) as(u8)];

    console_write(buffer.data(), index);
}

void emit_pointer(const volatile void *value)
{
    emit_hex_u64(value as(u64));
}

}  // namespace detail

}  // namespace kernel::debug
