#ifndef DOOM_OS_KERNEL_DEBUG_KPRINT_HPP_
#define DOOM_OS_KERNEL_DEBUG_KPRINT_HPP_

#include "kernel/core/types.hpp"
#include "kernel/utils/traits.hpp"

namespace kernel::debug {
namespace detail {
using kernel::core::i64;
using kernel::core::is_signed_integer_v;
using kernel::core::is_unsigned_integer_v;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

// =================================================================================================
// Fixed string
// =================================================================================================

template <usize N>
struct fixed_string {
    char data[N];

    constexpr explicit fixed_string(const char (&value)[N])
        : fixed_string(value, kernel::core::make_index_sequence<N>{}) {}

    static constexpr usize size() { return N; }

    static constexpr usize length() { return N == 0 ? 0 : N - 1; }

    constexpr char operator[](usize index) const { return data[index]; }

   private:
    template <usize... Is>
    constexpr fixed_string(const char (&value)[N], kernel::core::index_sequence<Is...>)
        : data{value[Is]...} {}
};

template <usize N>
fixed_string(const char (&)[N]) -> fixed_string<N>;

// =================================================================================================
// Backend
// =================================================================================================

void backend_emit_char(char value);
void backend_emit_bytes(const char *value, usize length);
void backend_emit_c_string(const char *value);
void backend_emit_decimal_u64(u64 value);
void backend_emit_decimal_i64(i64 value);
void backend_emit_hex_u64(u64 value);
void backend_emit_pointer(const volatile void *value);

// =================================================================================================
// Format model
// =================================================================================================

inline constexpr usize MAX_FORMAT_WIDTH = 128;
inline constexpr usize MAX_FLOAT_PRECISION = 18;

enum class presentation : u8 {
    DEFAULT,
    DECIMAL,
    HEX_LOWER,
    HEX_UPPER,
    POINTER,
    FIXED,
    STRING,
    CHARACTER,
};

enum class format_align : u8 {
    DEFAULT,
    LEFT,
    RIGHT,
    CENTER,
};

enum class format_sign : u8 {
    DEFAULT,
    PLUS,
    SPACE,
};

struct format_spec {
    presentation presentation_value;
    format_align align;
    format_sign  sign;
    char         fill;
    bool         alternate;
    bool         zero_pad;
    bool         has_width;
    usize        width;
    bool         has_precision;
    usize        precision;
};

struct format_field {
    bool        valid;
    usize       length;
    format_spec spec;
};

struct format_parse_result {
    bool  valid;
    usize field_count;
};

consteval format_spec default_format_spec() {
    return format_spec{
        .presentation_value = presentation::DEFAULT,
        .align = format_align::DEFAULT,
        .sign = format_sign::DEFAULT,
        .fill = ' ',
        .alternate = false,
        .zero_pad = false,
        .has_width = false,
        .width = 0,
        .has_precision = false,
        .precision = 0,
    };
}

consteval format_field invalid_format_field() {
    return format_field{
        .valid = false,
        .length = 0,
        .spec = default_format_spec(),
    };
}

consteval bool is_format_digit(char value) { return value >= '0' && value <= '9'; }

consteval usize format_digit_value(char value) { return static_cast<usize>(value - '0'); }

consteval bool is_format_align(char value) { return value == '<' || value == '>' || value == '^'; }

consteval format_align parse_format_align(char value) {
    if (value == '<') {
        return format_align::LEFT;
    }

    if (value == '>') {
        return format_align::RIGHT;
    }

    if (value == '^') {
        return format_align::CENTER;
    }

    return format_align::DEFAULT;
}

template <fixed_string FMT>
consteval char format_char_at(usize index) {
    if (index >= FMT.length()) {
        return '\0';
    }

    return FMT[index];
}

// Supported field grammar:
//
//     {}
//     {:spec}
//
// Supported spec subset:
//
//     [[fill]align][sign][#][0][width][.precision][type]
//
// Supported align:
//
//     <   left
//     >   right
//     ^   center
//
// Supported sign:
//
//     +
//         emit + for positive numeric values
//
//     space
//         emit space for positive numeric values
//
// Supported flags:
//
//     #
//         alternate form. For hex, emits 0x / 0X.
//
//     0
//         numeric zero padding after sign/prefix.
//
// Supported types:
//
//     d   decimal integer
//     x   lowercase hex integer
//     X   uppercase hex integer
//     p   pointer
//     f   fixed float
//     s   string
//     c   character
//
// Examples:
//
//     {}
//     {:d}
//     {:x}
//     {:#x}
//     {:016x}
//     {:#018x}
//     {:p}
//     {:018p}
//     {:.3f}
//     {:10.3f}
//     {:>16s}
//     {:08d}
//
template <fixed_string FMT>
consteval format_field parse_field_at(usize offset) {
    if (offset + 1 >= FMT.length() || FMT[offset] != '{') {
        return invalid_format_field();
    }

    if (FMT[offset + 1] == '}') {
        return format_field{
            .valid = true,
            .length = 2,
            .spec = default_format_spec(),
        };
    }

    if (FMT[offset + 1] != ':') {
        return invalid_format_field();
    }

    format_spec spec = default_format_spec();
    usize       index = offset + 2;

    if (index >= FMT.length()) {
        return invalid_format_field();
    }

    if (index + 1 < FMT.length() && is_format_align(FMT[index + 1])) {
        spec.fill = FMT[index];
        spec.align = parse_format_align(FMT[index + 1]);
        index += 2;
    } else if (is_format_align(FMT[index])) {
        spec.align = parse_format_align(FMT[index]);
        ++index;
    }

    if (index < FMT.length() && FMT[index] == '+') {
        spec.sign = format_sign::PLUS;
        ++index;
    } else if (index < FMT.length() && FMT[index] == ' ') {
        spec.sign = format_sign::SPACE;
        ++index;
    }

    if (index < FMT.length() && FMT[index] == '#') {
        spec.alternate = true;
        ++index;
    }

    if (index < FMT.length() && FMT[index] == '0') {
        spec.zero_pad = true;

        if (spec.align == format_align::DEFAULT) {
            spec.align = format_align::RIGHT;
        }

        ++index;
    }

    while (index < FMT.length() && is_format_digit(FMT[index])) {
        spec.has_width = true;
        spec.width = spec.width * 10 + format_digit_value(FMT[index]);
        ++index;
    }

    if (spec.has_width && spec.width > MAX_FORMAT_WIDTH) {
        return invalid_format_field();
    }

    if (index < FMT.length() && FMT[index] == '.') {
        ++index;

        if (index >= FMT.length() || !is_format_digit(FMT[index])) {
            return invalid_format_field();
        }

        spec.has_precision = true;

        while (index < FMT.length() && is_format_digit(FMT[index])) {
            spec.precision = spec.precision * 10 + format_digit_value(FMT[index]);
            ++index;
        }

        if (spec.precision > MAX_FLOAT_PRECISION) {
            return invalid_format_field();
        }
    }

    if (index < FMT.length() && FMT[index] != '}') {
        const char type = FMT[index];

        if (type == 'd') {
            spec.presentation_value = presentation::DECIMAL;
        } else if (type == 'x') {
            spec.presentation_value = presentation::HEX_LOWER;
        } else if (type == 'X') {
            spec.presentation_value = presentation::HEX_UPPER;
        } else if (type == 'p') {
            spec.presentation_value = presentation::POINTER;
        } else if (type == 'f') {
            spec.presentation_value = presentation::FIXED;
        } else if (type == 's') {
            spec.presentation_value = presentation::STRING;
        } else if (type == 'c') {
            spec.presentation_value = presentation::CHARACTER;
        } else {
            return invalid_format_field();
        }

        ++index;
    }

    if (index >= FMT.length() || FMT[index] != '}') {
        return invalid_format_field();
    }

    return format_field{
        .valid = true,
        .length = index - offset + 1,
        .spec = spec,
    };
}

template <fixed_string FMT>
consteval format_parse_result parse_format() {
    usize field_count = 0;
    usize index = 0;

    while (index < FMT.length()) {
        const char c = FMT[index];

        if (c == '{') {
            if (index + 1 < FMT.length() && FMT[index + 1] == '{') {
                index += 2;
                continue;
            }

            const format_field field = parse_field_at<FMT>(index);

            if (field.valid) {
                ++field_count;
                index += field.length;
                continue;
            }

            return format_parse_result{
                .valid = false,
                .field_count = field_count,
            };
        }

        if (c == '}') {
            if (index + 1 < FMT.length() && FMT[index + 1] == '}') {
                index += 2;
                continue;
            }

            return format_parse_result{
                .valid = false,
                .field_count = field_count,
            };
        }

        ++index;
    }

    return format_parse_result{
        .valid = true,
        .field_count = field_count,
    };
}

template <fixed_string FMT, usize I>
consteval usize literal_run_length() {
    usize index = I;

    while (index < FMT.length()) {
        if (const char c = FMT[index]; c == '{' || c == '}') {
            break;
        }

        ++index;
    }

    return index - I;
}

// =================================================================================================
// Raw formatting helpers
// =================================================================================================

inline constexpr char HEX_DIGITS_LOWER[] = "0123456789abcdef";
inline constexpr char HEX_DIGITS_UPPER[] = "0123456789ABCDEF";

template <format_spec Spec>
inline constexpr bool unsupported_format_spec_v = false;

inline void emit_repeated(char value, usize count) {
    for (usize index = 0; index < count; ++index) {
        backend_emit_char(value);
    }
}

inline void copy_bytes(char *dst, const char *src, usize length) {
    for (usize index = 0; index < length; ++index) {
        dst[index] = src[index];
    }
}

inline usize c_string_length(const char *value) {
    usize length = 0;

    while (value[length] != '\0') {
        ++length;
    }

    return length;
}

inline usize format_u64_decimal(u64 value, char *buffer) {
    char  reverse[20];
    usize length = 0;

    do {
        reverse[length] = static_cast<char>('0' + value % 10);
        value /= 10;
        ++length;
    } while (value != 0);

    for (usize index = 0; index < length; ++index) {
        buffer[index] = reverse[length - index - 1];
    }

    return length;
}

inline usize format_u64_hex(u64 value, char *buffer, bool uppercase) {
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

inline u64 signed_magnitude_i64(i64 value) {
    if (value >= 0) {
        return static_cast<u64>(value);
    }

    return static_cast<u64>(-(value + 1)) + 1ULL;
}

template <format_spec Spec>
inline void emit_padded(const char *prefix, usize prefix_length, const char *body,
                        usize body_length, bool numeric) {
    const usize content_length = prefix_length + body_length;
    const usize width = Spec.has_width ? Spec.width : 0;
    const usize padding = width > content_length ? width - content_length : 0;

    format_align align = Spec.align;

    if (align == format_align::DEFAULT) {
        align = numeric ? format_align::RIGHT : format_align::LEFT;
    }

    if (numeric && Spec.zero_pad && align == format_align::RIGHT) {
        backend_emit_bytes(prefix, prefix_length);
        emit_repeated('0', padding);
        backend_emit_bytes(body, body_length);
        return;
    }

    if (align == format_align::LEFT) {
        backend_emit_bytes(prefix, prefix_length);
        backend_emit_bytes(body, body_length);
        emit_repeated(Spec.fill, padding);
        return;
    }

    if (align == format_align::CENTER) {
        const usize left_padding = padding / 2;
        const usize right_padding = padding - left_padding;

        emit_repeated(Spec.fill, left_padding);
        backend_emit_bytes(prefix, prefix_length);
        backend_emit_bytes(body, body_length);
        emit_repeated(Spec.fill, right_padding);
        return;
    }

    emit_repeated(Spec.fill, padding);
    backend_emit_bytes(prefix, prefix_length);
    backend_emit_bytes(body, body_length);
}

// =================================================================================================
// Numeric emission
// =================================================================================================

template <format_spec Spec>
inline void emit_unsigned_decimal(u64 value) {
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
inline void emit_signed_decimal(i64 value) {
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
inline void emit_hex_integer(u64 value) {
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
inline void emit_pointer_value(const volatile void *value) {
    static_assert(!Spec.has_precision, "precision is not supported for pointer formats");

    const u64 address = reinterpret_cast<u64>(value);

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
inline void emit_float_special(const char *body, usize body_length, bool negative) {
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
inline void emit_fixed_float(long double value) {
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

    const u64   integer_part = static_cast<u64>(value);
    long double fractional_part = value - static_cast<long double>(integer_part);

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

        const u8 digit = static_cast<u8>(fractional_part);
        body[body_length] = static_cast<char>('0' + digit);
        ++body_length;

        fractional_part -= static_cast<long double>(digit);
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
inline void emit_string_value(const char *value) {
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
inline void emit_char_value(char value) {
    static_assert(!Spec.has_precision, "precision is not supported for character formats");

    if constexpr (Spec.presentation_value == presentation::DEFAULT ||
                  Spec.presentation_value == presentation::CHARACTER) {
        emit_padded<Spec>("", 0, &value, 1, false);
    } else if constexpr (Spec.presentation_value == presentation::DECIMAL) {
        emit_unsigned_decimal<Spec>(static_cast<u64>(static_cast<unsigned char>(value)));
    } else if constexpr (Spec.presentation_value == presentation::HEX_LOWER ||
                         Spec.presentation_value == presentation::HEX_UPPER) {
        emit_hex_integer<Spec>(static_cast<u64>(static_cast<unsigned char>(value)));
    } else {
        static_assert(unsupported_format_spec_v<Spec>, "unsupported format specifier for char");
    }
}

// =================================================================================================
// Value emission
// =================================================================================================

template <format_spec Spec>
inline void emit_value(char value) {
    emit_char_value<Spec>(value);
}

template <format_spec Spec>
inline void emit_value(bool value) {
    static_assert(!Spec.has_precision, "precision is not supported for bool formats");

    if constexpr (Spec.presentation_value == presentation::DEFAULT) {
        emit_string_value<Spec>(value ? "true" : "false");
    } else if constexpr (Spec.presentation_value == presentation::DECIMAL) {
        emit_unsigned_decimal<Spec>(value ? 1ULL : 0ULL);
    } else if constexpr (Spec.presentation_value == presentation::HEX_LOWER ||
                         Spec.presentation_value == presentation::HEX_UPPER) {
        emit_hex_integer<Spec>(value ? 1ULL : 0ULL);
    } else {
        static_assert(unsupported_format_spec_v<Spec>, "unsupported format specifier for bool");
    }
}

template <format_spec Spec>
inline void emit_value(const char *value) {
    if constexpr (Spec.presentation_value == presentation::POINTER ||
                  Spec.presentation_value == presentation::HEX_LOWER ||
                  Spec.presentation_value == presentation::HEX_UPPER) {
        emit_pointer_value<Spec>(static_cast<const volatile void *>(value));
    } else {
        emit_string_value<Spec>(value);
    }
}

template <format_spec Spec>
inline void emit_value(char *value) {
    if constexpr (Spec.presentation_value == presentation::POINTER ||
                  Spec.presentation_value == presentation::HEX_LOWER ||
                  Spec.presentation_value == presentation::HEX_UPPER) {
        emit_pointer_value<Spec>(static_cast<const volatile void *>(value));
    } else {
        emit_string_value<Spec>(value);
    }
}

template <format_spec Spec, usize N>
inline void emit_value(const char (&value)[N]) {
    if constexpr (Spec.presentation_value == presentation::POINTER ||
                  Spec.presentation_value == presentation::HEX_LOWER ||
                  Spec.presentation_value == presentation::HEX_UPPER) {
        emit_pointer_value<Spec>(static_cast<const volatile void *>(&value[0]));
    } else {
        emit_string_value<Spec>(value);
    }
}

template <format_spec Spec, usize N>
inline void emit_value(char (&value)[N]) {
    if constexpr (Spec.presentation_value == presentation::POINTER ||
                  Spec.presentation_value == presentation::HEX_LOWER ||
                  Spec.presentation_value == presentation::HEX_UPPER) {
        emit_pointer_value<Spec>(static_cast<const volatile void *>(&value[0]));
    } else {
        emit_string_value<Spec>(value);
    }
}

template <format_spec Spec>
inline void emit_value(decltype(nullptr)) {
    emit_pointer_value<Spec>(nullptr);
}

template <format_spec Spec, typename T>
inline void emit_value(T *value) {
    emit_pointer_value<Spec>(static_cast<const volatile void *>(value));
}

template <format_spec Spec, typename T>
inline void emit_value(const T &value)
    requires is_unsigned_integer_v<T>
{
    if constexpr (Spec.presentation_value == presentation::DEFAULT ||
                  Spec.presentation_value == presentation::DECIMAL) {
        emit_unsigned_decimal<Spec>(static_cast<u64>(value));
    } else if constexpr (Spec.presentation_value == presentation::HEX_LOWER ||
                         Spec.presentation_value == presentation::HEX_UPPER) {
        emit_hex_integer<Spec>(static_cast<u64>(value));
    } else {
        static_assert(unsupported_format_spec_v<Spec>,
                      "unsupported format specifier for unsigned integer");
    }
}

template <format_spec Spec, typename T>
inline void emit_value(const T &value)
    requires is_signed_integer_v<T>
{
    if constexpr (Spec.presentation_value == presentation::DEFAULT ||
                  Spec.presentation_value == presentation::DECIMAL) {
        emit_signed_decimal<Spec>(static_cast<i64>(value));
    } else if constexpr (Spec.presentation_value == presentation::HEX_LOWER ||
                         Spec.presentation_value == presentation::HEX_UPPER) {
        emit_hex_integer<Spec>(static_cast<u64>(static_cast<i64>(value)));
    } else {
        static_assert(unsupported_format_spec_v<Spec>,
                      "unsupported format specifier for signed integer");
    }
}

template <format_spec Spec>
inline void emit_value(float value) {
    emit_fixed_float<Spec>(static_cast<long double>(value));
}

template <format_spec Spec>
inline void emit_value(double value) {
    emit_fixed_float<Spec>(static_cast<long double>(value));
}

template <format_spec Spec>
inline void emit_value(long double value) {
    emit_fixed_float<Spec>(value);
}

// =================================================================================================
// Argument selection
// =================================================================================================

template <usize target_index>
inline constexpr bool invalid_argument_index_v = false;

template <usize target_index, format_spec Spec>
inline void emit_nth_arg() {
    static_assert(invalid_argument_index_v<target_index>, "kprint argument index out of range");
}

template <usize target_index, format_spec Spec, typename T, typename... Rest>
inline void emit_nth_arg(const T &value, const Rest &...rest) {
    if constexpr (target_index == 0) {
        emit_value<Spec>(value);
    } else {
        emit_nth_arg<target_index - 1, Spec>(rest...);
    }
}

// =================================================================================================
// Compile-time format engine
// =================================================================================================

template <fixed_string fmt, usize offset, usize arg_index, typename... Args>
inline void emit_format(const Args &...args) {
    if constexpr (offset >= fmt.length()) {
        return;
    } else if constexpr (format_char_at<fmt>(offset) == '{' &&
                         format_char_at<fmt>(offset + 1) == '{') {
        backend_emit_char('{');
        emit_format<fmt, offset + 2, arg_index>(args...);
    } else if constexpr (format_char_at<fmt>(offset) == '}' &&
                         format_char_at<fmt>(offset + 1) == '}') {
        backend_emit_char('}');
        emit_format<fmt, offset + 2, arg_index>(args...);
    } else if constexpr (format_char_at<fmt>(offset) == '{') {
        constexpr format_field field = parse_field_at<fmt>(offset);

        static_assert(field.valid, "invalid kprint replacement field");

        emit_nth_arg<arg_index, field.spec>(args...);
        emit_format<fmt, offset + field.length, arg_index + 1>(args...);
    } else {
        constexpr usize run_length = literal_run_length<fmt, offset>();

        if constexpr (run_length > 0) {
            backend_emit_bytes(&fmt.data[offset], run_length);
            emit_format<fmt, offset + run_length, arg_index>(args...);
        } else {
            backend_emit_char(format_char_at<fmt>(offset));
            emit_format<fmt, offset + 1, arg_index>(args...);
        }
    }
}

}  // namespace detail

// =================================================================================================
// Public API
// =================================================================================================

void kprint_init();

template <detail::fixed_string FMT, typename... Args>
inline void kprint_ct(const Args &...args) {
    constexpr detail::format_parse_result parse_result = detail::parse_format<FMT>();

    static_assert(parse_result.valid,
                  "invalid kprint format string; supported fields are {}, {:d}, {:x}, {:X}, "
                  "{:#x}, {:016x}, {:#018x}, {:p}, {:018p}, {:.3f}, {:s}, {:c}; escape braces as "
                  "{{ and }}");

    static_assert(parse_result.field_count == sizeof...(Args),
                  "kprint argument count does not match replacement field count");

    detail::emit_format<FMT, 0, 0>(args...);
}

template <detail::fixed_string FMT, typename... Args>
inline void kprintln_ct(const Args &...args) {
    kprint_ct<FMT>(args...);
    detail::backend_emit_char('\n');
}

}  // namespace kernel::debug

#define KPRINT(fmt__, ...) \
    ::kernel::debug::kprint_ct<::kernel::debug::detail::fixed_string{fmt__}>(__VA_ARGS__)

#define KPRINTLN(fmt__, ...) \
    ::kernel::debug::kprintln_ct<::kernel::debug::detail::fixed_string{fmt__}>(__VA_ARGS__)

#endif  // DOOM_OS_KERNEL_DEBUG_KPRINT_HPP_