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

    constexpr fixed_string(const char (&value)[N]) {
        for (usize index = 0; index < N; ++index) {
            data[index] = value[index];
        }
    }

    static constexpr usize size() { return N; }

    static constexpr usize length() { return N == 0 ? 0 : N - 1; }

    constexpr char operator[](usize index) const { return data[index]; }
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

enum class presentation : u8 {
    default_value,
    hex,
    pointer,
};

struct format_parse_result {
    bool  valid;
    usize field_count;
};

template <fixed_string fmt>
consteval char format_char_at(usize index) {
    if (index >= fmt.length()) {
        return '\0';
    }

    return fmt[index];
}

template <fixed_string fmt>
consteval format_parse_result parse_format() {
    usize field_count = 0;
    usize index = 0;

    while (index < fmt.length()) {
        const char c = fmt[index];

        if (c == '{') {
            if (index + 1 < fmt.length() && fmt[index + 1] == '{') {
                index += 2;
                continue;
            }

            if (index + 1 < fmt.length() && fmt[index + 1] == '}') {
                ++field_count;
                index += 2;
                continue;
            }

            if (index + 3 < fmt.length() && fmt[index + 1] == ':' && fmt[index + 3] == '}' &&
                (fmt[index + 2] == 'x' || fmt[index + 2] == 'p')) {
                ++field_count;
                index += 4;
                continue;
            }

            return format_parse_result{
                .valid = false,
                .field_count = field_count,
            };
        }

        if (c == '}') {
            if (index + 1 < fmt.length() && fmt[index + 1] == '}') {
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

template <fixed_string fmt, usize offset>
consteval usize literal_run_length() {
    usize index = offset;

    while (index < fmt.length()) {
        const char c = fmt[index];

        if (c == '{' || c == '}') {
            break;
        }

        ++index;
    }

    return index - offset;
}

// =================================================================================================
// Value emission
// =================================================================================================

template <presentation presentation_value>
inline void emit_value(char value) {
    if constexpr (presentation_value == presentation::default_value) {
        backend_emit_char(value);
    } else {
        backend_emit_hex_u64(static_cast<u64>(static_cast<unsigned char>(value)));
    }
}

template <presentation presentation_value>
inline void emit_value(bool value) {
    if constexpr (presentation_value == presentation::default_value) {
        backend_emit_c_string(value ? "true" : "false");
    } else {
        backend_emit_hex_u64(value ? 1ULL : 0ULL);
    }
}

template <presentation presentation_value>
inline void emit_value(const char *value) {
    if constexpr (presentation_value == presentation::default_value) {
        backend_emit_c_string(value);
    } else {
        backend_emit_pointer(value);
    }
}

template <presentation presentation_value>
inline void emit_value(char *value) {
    if constexpr (presentation_value == presentation::default_value) {
        backend_emit_c_string(value);
    } else {
        backend_emit_pointer(value);
    }
}

template <presentation presentation_value, usize N>
inline void emit_value(const char (&value)[N]) {
    if constexpr (presentation_value == presentation::default_value) {
        backend_emit_c_string(value);
    } else {
        backend_emit_pointer(value);
    }
}

template <presentation presentation_value, usize N>
inline void emit_value(char (&value)[N]) {
    if constexpr (presentation_value == presentation::default_value) {
        backend_emit_c_string(value);
    } else {
        backend_emit_pointer(value);
    }
}

template <presentation presentation_value>
inline void emit_value(decltype(nullptr)) {
    backend_emit_pointer(nullptr);
}

template <presentation presentation_value, typename T>
inline void emit_value(T *value) {
    backend_emit_pointer(static_cast<const volatile void *>(value));
}

template <presentation presentation_value, typename T>
inline void emit_value(const T &value)
    requires is_unsigned_integer_v<T>
{
    if constexpr (presentation_value == presentation::default_value) {
        backend_emit_decimal_u64(static_cast<u64>(value));
    } else {
        backend_emit_hex_u64(static_cast<u64>(value));
    }
}

template <presentation presentation_value, typename T>
inline void emit_value(const T &value)
    requires is_signed_integer_v<T>
{
    if constexpr (presentation_value == presentation::default_value) {
        backend_emit_decimal_i64(static_cast<i64>(value));
    } else {
        backend_emit_hex_u64(static_cast<u64>(static_cast<i64>(value)));
    }
}

// =================================================================================================
// Argument selection
// =================================================================================================

template <usize target_index>
inline constexpr bool invalid_argument_index_v = false;

template <usize target_index, presentation presentation_value>
inline void emit_nth_arg() {
    static_assert(invalid_argument_index_v<target_index>, "kprint argument index out of range");
}

template <usize target_index, presentation presentation_value, typename T, typename... Rest>
inline void emit_nth_arg(const T &value, const Rest &...rest) {
    if constexpr (target_index == 0) {
        emit_value<presentation_value>(value);
    } else {
        emit_nth_arg<target_index - 1, presentation_value>(rest...);
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
    } else if constexpr (format_char_at<fmt>(offset) == '{' &&
                         format_char_at<fmt>(offset + 1) == '}') {
        emit_nth_arg<arg_index, presentation::default_value>(args...);
        emit_format<fmt, offset + 2, arg_index + 1>(args...);
    } else if constexpr (format_char_at<fmt>(offset) == '{' &&
                         format_char_at<fmt>(offset + 1) == ':' &&
                         format_char_at<fmt>(offset + 2) == 'x' &&
                         format_char_at<fmt>(offset + 3) == '}') {
        emit_nth_arg<arg_index, presentation::hex>(args...);
        emit_format<fmt, offset + 4, arg_index + 1>(args...);
    } else if constexpr (format_char_at<fmt>(offset) == '{' &&
                         format_char_at<fmt>(offset + 1) == ':' &&
                         format_char_at<fmt>(offset + 2) == 'p' &&
                         format_char_at<fmt>(offset + 3) == '}') {
        emit_nth_arg<arg_index, presentation::pointer>(args...);
        emit_format<fmt, offset + 4, arg_index + 1>(args...);
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

template <detail::fixed_string fmt, typename... Args>
inline void kprint_ct(const Args &...args) {
    constexpr detail::format_parse_result parse_result = detail::parse_format<fmt>();

    static_assert(parse_result.valid,
                  "invalid kprint format string; valid fields are {}, {:x}, {:p}; escape braces as "
                  "{{ and }}");

    static_assert(parse_result.field_count == sizeof...(Args),
                  "kprint argument count does not match replacement field count");

    detail::emit_format<fmt, 0, 0>(args...);
}

template <detail::fixed_string fmt, typename... Args>
inline void kprintln_ct(const Args &...args) {
    kprint_ct<fmt>(args...);
    detail::backend_emit_char('\n');
}
}  // namespace kernel::debug

#define KPRINT(fmt, ...) ::kernel::debug::kprint_ct<fmt>(__VA_ARGS__)

#define KPRINTLN(fmt, ...) ::kernel::debug::kprintln_ct<fmt>(__VA_ARGS__)

#endif  // DOOM_OS_KERNEL_DEBUG_KPRINT_HPP_