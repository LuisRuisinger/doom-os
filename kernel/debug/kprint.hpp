#ifndef DOOM_OS_KERNEL_DEBUG_KPRINT_HPP_
#define DOOM_OS_KERNEL_DEBUG_KPRINT_HPP_

#include "kernel/debug/formatter.hpp"

namespace kernel::debug {

namespace detail {

using kernel::core::usize;

template <usize target_index>
inline constexpr bool invalid_argument_index_v = false;

template <usize target_index, format_spec Spec>
inline void emit_nth_arg()
{
    static_assert(invalid_argument_index_v<target_index>, "kprint argument index out of range");
}

template <usize target_index, format_spec Spec, typename T, typename... Rest>
inline void emit_nth_arg(const T &value, const Rest &...rest)
{
    if constexpr (target_index == 0) {
        emit_value<Spec>(value);
    } else {
        emit_nth_arg<target_index - 1, Spec>(rest...);
    }
}

template <fixed_string fmt, usize offset, usize arg_index, typename... Args>
inline void emit_format(const Args &...args)
{
    if constexpr (offset >= fmt.length()) {
        return;
    } else if constexpr (format_char_at<fmt>(offset) == '{' &&
                         format_char_at<fmt>(offset + 1) == '{') {
        emit_char('{');
        emit_format<fmt, offset + 2, arg_index>(args...);
    } else if constexpr (format_char_at<fmt>(offset) == '}' &&
                         format_char_at<fmt>(offset + 1) == '}') {
        emit_char('}');
        emit_format<fmt, offset + 2, arg_index>(args...);
    } else if constexpr (format_char_at<fmt>(offset) == '{') {
        constexpr format_field field = parse_field_at<fmt>(offset);

        static_assert(field.valid, "invalid kprint replacement field");

        emit_nth_arg<arg_index, field.spec>(args...);
        emit_format<fmt, offset + field.length, arg_index + 1>(args...);
    } else {
        constexpr usize run_length = literal_run_length<fmt, offset>();

        if constexpr (run_length > 0) {
            emit_bytes(fmt.data() + offset, run_length);
            emit_format<fmt, offset + run_length, arg_index>(args...);
        } else {
            emit_char(format_char_at<fmt>(offset));
            emit_format<fmt, offset + 1, arg_index>(args...);
        }
    }
}

}  // namespace detail

template <detail::fixed_string FMT, typename... Args>
inline void kprint_ct(const Args &...args)
{
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
inline void kprintln_ct(const Args &...args)
{
    kprint_ct<FMT>(args...);
    detail::emit_char('\n');
}

}  // namespace kernel::debug

#define KPRINT(fmt__, ...) \
    ::kernel::debug::kprint_ct<::kernel::core::utils::fixed_string{fmt__}>(__VA_ARGS__)

#define KPRINTLN(fmt__, ...) \
    ::kernel::debug::kprintln_ct<::kernel::core::utils::fixed_string{fmt__}>(__VA_ARGS__)

#endif  // DOOM_OS_KERNEL_DEBUG_KPRINT_HPP_
