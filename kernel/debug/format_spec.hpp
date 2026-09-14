#ifndef DOOM_OS_KERNEL_DEBUG_FORMAT_SPEC_HPP_
#define DOOM_OS_KERNEL_DEBUG_FORMAT_SPEC_HPP_

// =================================================================================================
// Cpp stdlib files
// =================================================================================================

#include <utility>

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/cast.hpp"
#include "kernel/core/types.hpp"

namespace kernel::debug {
namespace detail {
using kernel::core::i64;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

// Parsing only: every declaration below is consteval and nothing here emits a byte.

// =================================================================================================
// Fixed string
// =================================================================================================

template <usize N>
struct fixed_string {
    char data[N];

    constexpr explicit fixed_string(const char (&value)[N])
        : fixed_string(value, std::make_index_sequence<N>{})
    {
    }

    static constexpr usize size()
    {
        return N;
    }

    static constexpr usize length()
    {
        return N == 0 ? 0 : N - 1;
    }

    constexpr char operator[](usize index) const
    {
        return data[index];
    }

private:
    template <usize... Is>
    constexpr fixed_string(const char (&value)[N], std::index_sequence<Is...>)
        : data{value[Is]...}
    {
    }
};

template <usize N>
fixed_string(const char (&)[N]) -> fixed_string<N>;

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

consteval format_spec default_format_spec()
{
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

consteval format_field invalid_format_field()
{
    return format_field{
        .valid = false,
        .length = 0,
        .spec = default_format_spec(),
    };
}

consteval bool is_format_digit(char value)
{
    return value >= '0' && value <= '9';
}

consteval usize format_digit_value(char value)
{
    return (value - '0') as(usize);
}

consteval bool is_format_align(char value)
{
    return value == '<' || value == '>' || value == '^';
}

consteval format_align parse_format_align(char value)
{
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
consteval char format_char_at(usize index)
{
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
consteval format_field parse_field_at(usize offset)
{
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
consteval format_parse_result parse_format()
{
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
consteval usize literal_run_length()
{
    usize index = I;

    while (index < FMT.length()) {
        if (const char c = FMT[index]; c == '{' || c == '}') {
            break;
        }

        ++index;
    }

    return index - I;
}

}  // namespace detail
}  // namespace kernel::debug

#endif  // DOOM_OS_KERNEL_DEBUG_FORMAT_SPEC_HPP_
