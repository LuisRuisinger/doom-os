#ifndef DOOM_OS_KERNEL_DEBUG_EMIT_HPP_
#define DOOM_OS_KERNEL_DEBUG_EMIT_HPP_

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/cast.hpp"
#include "kernel/debug/format_spec.hpp"

namespace kernel::debug {
namespace detail {
using kernel::core::i64;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

// Rendering: takes a value and a parsed spec, produces bytes. Knows nothing about which
// C++ type it came from - that mapping lives in formatter.hpp.

// =================================================================================================
// Transport and formatting
// =================================================================================================

void emit_char(char value);
void emit_bytes(const char *value, usize length);
void emit_c_string(const char *value);
void emit_decimal_u64(u64 value);
void emit_decimal_i64(i64 value);
void emit_hex_u64(u64 value);
void emit_pointer(const volatile void *value);

// =================================================================================================
// Raw formatting helpers
// =================================================================================================

inline constexpr char HEX_DIGITS_LOWER[] = "0123456789abcdef";
inline constexpr char HEX_DIGITS_UPPER[] = "0123456789ABCDEF";

template <format_spec Spec>
inline constexpr bool unsupported_format_spec_v = false;

inline void emit_repeated(char value, usize count)
{
    for (usize index = 0; index < count; ++index) {
        emit_char(value);
    }
}

inline void copy_bytes(char *dst, const char *src, usize length)
{
    for (usize index = 0; index < length; ++index) {
        dst[index] = src[index];
    }
}

inline usize c_string_length(const char *value)
{
    usize length = 0;

    while (value[length] != '\0') {
        ++length;
    }

    return length;
}

inline usize format_u64_decimal(u64 value, char *buffer)
{
    char  reverse[20];
    usize length = 0;

    do {
        reverse[length] = ('0' + value % 10) as(char);
        value /= 10;
        ++length;
    } while (value != 0);

    for (usize index = 0; index < length; ++index) {
        buffer[index] = reverse[length - index - 1];
    }

    return length;
}

inline usize format_u64_hex(u64 value, char *buffer, bool uppercase)
{
    const char *digits = uppercase ? HEX_DIGITS_UPPER : HEX_DIGITS_LOWER;

    char  reverse[16];
    usize length = 0;

    do {
        reverse[length] = digits[value & 0xFULL];
        value >>= 4;
        ++length;
    } while (value != 0);

    for (usize index = 0; index < length; ++index) {
        buffer[index] = reverse[length - index - 1];
    }

    return length;
}

inline u64 signed_magnitude_i64(i64 value)
{
    if (value >= 0) {
        return value as(u64);
    }

    return (-(value + 1)) as(u64) + 1ULL;
}

template <format_spec Spec>
inline void emit_padded(const char *prefix, usize prefix_length, const char *body,
                        usize body_length, bool numeric)
{
    const usize content_length = prefix_length + body_length;
    const usize width = Spec.has_width ? Spec.width : 0;
    const usize padding = width > content_length ? width - content_length : 0;

    format_align align = Spec.align;

    if (align == format_align::DEFAULT) {
        align = numeric ? format_align::RIGHT : format_align::LEFT;
    }

    if (numeric && Spec.zero_pad && align == format_align::RIGHT) {
        emit_bytes(prefix, prefix_length);
        emit_repeated('0', padding);
        emit_bytes(body, body_length);
        return;
    }

    if (align == format_align::LEFT) {
        emit_bytes(prefix, prefix_length);
        emit_bytes(body, body_length);
        emit_repeated(Spec.fill, padding);
        return;
    }

    if (align == format_align::CENTER) {
        const usize left_padding = padding / 2;
        const usize right_padding = padding - left_padding;

        emit_repeated(Spec.fill, left_padding);
        emit_bytes(prefix, prefix_length);
        emit_bytes(body, body_length);
        emit_repeated(Spec.fill, right_padding);
        return;
    }

    emit_repeated(Spec.fill, padding);
    emit_bytes(prefix, prefix_length);
    emit_bytes(body, body_length);
}

// =================================================================================================
// Numeric emission
// =================================================================================================

template <format_spec Spec>
inline void emit_unsigned_decimal(u64 value)
{
    static_assert(!Spec.has_precision, "precision is not supported for integer formats");
    static_assert(!Spec.alternate, "alternate form is not supported for decimal integers");

    char        body[20];
    const usize body_length = format_u64_decimal(value, body);

    char  prefix_buffer[1];
    usize prefix_length = 0;

    if constexpr (Spec.sign == format_sign::PLUS) {
        prefix_buffer[0] = '+';
        prefix_length = 1;
    } else if constexpr (Spec.sign == format_sign::SPACE) {
        prefix_buffer[0] = ' ';
        prefix_length = 1;
    }

    emit_padded<Spec>(prefix_buffer, prefix_length, body, body_length, true);
}

template <format_spec Spec>
inline void emit_signed_decimal(i64 value)
{
    static_assert(!Spec.has_precision, "precision is not supported for integer formats");
    static_assert(!Spec.alternate, "alternate form is not supported for decimal integers");

    char        body[20];
    const usize body_length = format_u64_decimal(signed_magnitude_i64(value), body);

    char  prefix_buffer[1];
    usize prefix_length = 0;

    if (value < 0) {
        prefix_buffer[0] = '-';
        prefix_length = 1;
    } else if constexpr (Spec.sign == format_sign::PLUS) {
        prefix_buffer[0] = '+';
        prefix_length = 1;
    } else if constexpr (Spec.sign == format_sign::SPACE) {
        prefix_buffer[0] = ' ';
        prefix_length = 1;
    }

    emit_padded<Spec>(prefix_buffer, prefix_length, body, body_length, true);
}

template <format_spec Spec>
inline void emit_hex_integer(u64 value)
{
    static_assert(!Spec.has_precision, "precision is not supported for integer formats");

    constexpr bool uppercase = Spec.presentation_value == presentation::HEX_UPPER;

    char        body[16];
    const usize body_length = format_u64_hex(value, body, uppercase);

    char  prefix_buffer[2];
    usize prefix_length = 0;

    if constexpr (Spec.alternate) {
        prefix_buffer[0] = '0';
        prefix_buffer[1] = 'x';
        prefix_length = 2;
    }

    emit_padded<Spec>(prefix_buffer, prefix_length, body, body_length, true);
}

template <format_spec Spec>
inline void emit_pointer_value(const volatile void *value)
{
    static_assert(!Spec.has_precision, "precision is not supported for pointer formats");

    const u64 address = value as(u64);

    if constexpr (Spec.presentation_value == presentation::HEX_LOWER ||
                  Spec.presentation_value == presentation::HEX_UPPER) {
        emit_hex_integer<Spec>(address);
    } else if constexpr (Spec.presentation_value == presentation::DEFAULT ||
                         Spec.presentation_value == presentation::POINTER) {
        constexpr format_spec pointer_spec{
            .presentation_value = presentation::HEX_LOWER,
            .align = Spec.align,
            .sign = format_sign::DEFAULT,
            .fill = Spec.fill,
            .alternate = true,
            .zero_pad = Spec.zero_pad,
            .has_width = Spec.has_width,
            .width = Spec.width,
            .has_precision = false,
            .precision = 0,
        };

        emit_hex_integer<pointer_spec>(address);
    } else {
        static_assert(unsupported_format_spec_v<Spec>, "unsupported format specifier for pointer");
    }
}

// =================================================================================================
// Float emission
// =================================================================================================

template <format_spec Spec>
inline void emit_float_special(const char *body, usize body_length, bool negative)
{
    char  prefix_buffer[1];
    usize prefix_length = 0;

    if (negative) {
        prefix_buffer[0] = '-';
        prefix_length = 1;
    } else if constexpr (Spec.sign == format_sign::PLUS) {
        prefix_buffer[0] = '+';
        prefix_length = 1;
    } else if constexpr (Spec.sign == format_sign::SPACE) {
        prefix_buffer[0] = ' ';
        prefix_length = 1;
    }

    emit_padded<Spec>(prefix_buffer, prefix_length, body, body_length, true);
}

template <format_spec Spec>
inline void emit_fixed_float(long double value)
{
    static_assert(Spec.presentation_value == presentation::DEFAULT ||
                      Spec.presentation_value == presentation::FIXED,
                  "unsupported format specifier for floating-point value");

    constexpr usize precision = Spec.has_precision ? Spec.precision : 6;

    bool negative = value < 0.0L;

    if (value != value) {
        emit_float_special<Spec>("nan", 3, false);
        return;
    }

    if (negative) {
        value = -value;
    }

    const long double infinity_test = value - value;

    if (infinity_test != infinity_test) {
        emit_float_special<Spec>("inf", 3, negative);
        return;
    }

    if (value > 18446744073709551615.0L) {
        emit_float_special<Spec>("ovf", 3, negative);
        return;
    }

    long double rounding = 0.5L;

    for (usize index = 0; index < precision; ++index) {
        rounding /= 10.0L;
    }

    value += rounding;

    const u64 integer_part = value as(u64);
    long double                    fractional_part = value - integer_part as(long double);

    char        integer_buffer[20];
    const usize integer_length = format_u64_decimal(integer_part, integer_buffer);

    char  body[64];
    usize body_length = 0;

    copy_bytes(body + body_length, integer_buffer, integer_length);
    body_length += integer_length;

    if constexpr (precision > 0 || Spec.alternate) {
        body[body_length] = '.';
        ++body_length;
    }

    for (usize index = 0; index < precision; ++index) {
        fractional_part *= 10.0L;

        const u8 digit = fractional_part as(u8);
        body[body_length] = ('0' + digit) as(char);
        ++body_length;

        fractional_part -= digit as(long double);
    }

    char  prefix_buffer[1];
    usize prefix_length = 0;

    if (negative) {
        prefix_buffer[0] = '-';
        prefix_length = 1;
    } else if constexpr (Spec.sign == format_sign::PLUS) {
        prefix_buffer[0] = '+';
        prefix_length = 1;
    } else if constexpr (Spec.sign == format_sign::SPACE) {
        prefix_buffer[0] = ' ';
        prefix_length = 1;
    }

    emit_padded<Spec>(prefix_buffer, prefix_length, body, body_length, true);
}

// =================================================================================================
// String emission
// =================================================================================================

template <format_spec Spec>
inline void emit_string_value(const char *value)
{
    static_assert(Spec.presentation_value == presentation::DEFAULT ||
                      Spec.presentation_value == presentation::STRING,
                  "unsupported format specifier for string");

    const char *actual_value = value == nullptr ? "(null)" : value;
    usize       length = c_string_length(actual_value);

    if constexpr (Spec.has_precision) {
        if (length > Spec.precision) {
            length = Spec.precision;
        }
    }

    emit_padded<Spec>("", 0, actual_value, length, false);
}

template <format_spec Spec>
inline void emit_char_value(char value)
{
    static_assert(!Spec.has_precision, "precision is not supported for character formats");

    if constexpr (Spec.presentation_value == presentation::DEFAULT ||
                  Spec.presentation_value == presentation::CHARACTER) {
        emit_padded<Spec>("", 0, &value, 1, false);
    } else if constexpr (Spec.presentation_value == presentation::DECIMAL) {
        emit_unsigned_decimal<Spec>((value as(unsigned char)) as(u64));
    } else if constexpr (Spec.presentation_value == presentation::HEX_LOWER ||
                         Spec.presentation_value == presentation::HEX_UPPER) {
        emit_hex_integer<Spec>((value as(unsigned char)) as(u64));
    } else {
        static_assert(unsupported_format_spec_v<Spec>, "unsupported format specifier for char");
    }
}

}  // namespace detail
}  // namespace kernel::debug

#endif  // DOOM_OS_KERNEL_DEBUG_EMIT_HPP_
