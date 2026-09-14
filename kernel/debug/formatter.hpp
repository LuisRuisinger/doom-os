#ifndef DOOM_OS_KERNEL_DEBUG_FORMATTER_HPP_
#define DOOM_OS_KERNEL_DEBUG_FORMATTER_HPP_

#include <concepts>
#include <type_traits>

#include "kernel/core/cast.hpp"
#include "kernel/core/reflect.hpp"
#include "kernel/debug/emit.hpp"

namespace kernel::debug {

namespace detail {

using kernel::core::i64;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

template <typename T>
struct is_signed_integer_base : std::false_type {};

template <>
struct is_signed_integer_base<signed char> : std::true_type {};
template <>
struct is_signed_integer_base<short> : std::true_type {};
template <>
struct is_signed_integer_base<int> : std::true_type {};
template <>
struct is_signed_integer_base<long> : std::true_type {};
template <>
struct is_signed_integer_base<long long> : std::true_type {};

template <typename T>
inline constexpr bool is_signed_integer_v = is_signed_integer_base<std::remove_cvref_t<T>>::value;

template <typename T>
struct is_unsigned_integer_base : std::false_type {};

template <>
struct is_unsigned_integer_base<unsigned char> : std::true_type {};
template <>
struct is_unsigned_integer_base<unsigned short> : std::true_type {};
template <>
struct is_unsigned_integer_base<unsigned int> : std::true_type {};
template <>
struct is_unsigned_integer_base<unsigned long> : std::true_type {};
template <>
struct is_unsigned_integer_base<unsigned long long> : std::true_type {};

template <typename T>
inline constexpr bool is_unsigned_integer_v =
    is_unsigned_integer_base<std::remove_cvref_t<T>>::value;

template <typename T>
concept character_value = std::same_as<T, char>;

template <typename T>
concept boolean_value = std::same_as<T, bool>;

template <typename T>
concept unsigned_value = is_unsigned_integer_v<T>;

template <typename T>
concept signed_value = is_signed_integer_v<T>;

template <typename T>
concept floating_value = std::is_floating_point_v<T>;

template <typename T>
concept c_string_value = std::same_as<T, char *> || std::same_as<T, const char *>;

template <typename T>
concept null_value = std::same_as<T, decltype(nullptr)>;

// A char pointer is a string, not an address, unless the spec asks for one.
template <typename T>
concept pointer_value = std::is_pointer_v<T> && !c_string_value<T>;

template <typename T>
struct unsupported_formatted_type : std::false_type {};

template <typename T>
struct formatter {
    static_assert(unsupported_formatted_type<T>::value,
                  "kprint cannot format this type; specialise "
                  "kernel::debug::detail::formatter<T> for it");
};

template <character_value T>
struct formatter<T> {
    template <format_spec Spec>
    static void emit(char value)
    {
        emit_char_value<Spec>(value);
    }
};

template <boolean_value T>
struct formatter<T> {
    template <format_spec Spec>
    static void emit(bool value)
    {
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
};

template <unsigned_value T>
struct formatter<T> {
    template <format_spec Spec>
    static void emit(T value)
    {
        if constexpr (Spec.presentation_value == presentation::DEFAULT ||
                      Spec.presentation_value == presentation::DECIMAL) {
            emit_unsigned_decimal<Spec>(value as(u64));
        } else if constexpr (Spec.presentation_value == presentation::HEX_LOWER ||
                             Spec.presentation_value == presentation::HEX_UPPER) {
            emit_hex_integer<Spec>(value as(u64));
        } else {
            static_assert(unsupported_format_spec_v<Spec>,
                          "unsupported format specifier for unsigned integer");
        }
    }
};

template <signed_value T>
struct formatter<T> {
    template <format_spec Spec>
    static void emit(T value)
    {
        if constexpr (Spec.presentation_value == presentation::DEFAULT ||
                      Spec.presentation_value == presentation::DECIMAL) {
            emit_signed_decimal<Spec>(value as(i64));
        } else if constexpr (Spec.presentation_value == presentation::HEX_LOWER ||
                             Spec.presentation_value == presentation::HEX_UPPER) {
            emit_hex_integer<Spec>((value as(i64)) as(u64));
        } else {
            static_assert(unsupported_format_spec_v<Spec>,
                          "unsupported format specifier for signed integer");
        }
    }
};

template <floating_value T>
struct formatter<T> {
    template <format_spec Spec>
    static void emit(T value)
    {
        emit_fixed_float<Spec>(value as(long double));
    }
};

template <c_string_value T>
struct formatter<T> {
    template <format_spec Spec>
    static void emit(const char *value)
    {
        if constexpr (Spec.presentation_value == presentation::POINTER ||
                      Spec.presentation_value == presentation::HEX_LOWER ||
                      Spec.presentation_value == presentation::HEX_UPPER) {
            emit_pointer_value<Spec>(value as(const volatile void *));
        } else {
            emit_string_value<Spec>(value);
        }
    }
};

template <null_value T>
struct formatter<T> {
    template <format_spec Spec>
    static void emit(decltype(nullptr))
    {
        emit_pointer_value<Spec>(nullptr);
    }
};

template <pointer_value T>
struct formatter<T> {
    template <format_spec Spec>
    static void emit(T value)
    {
        emit_pointer_value<Spec>(value as(const volatile void *));
    }
};

template <format_spec Spec, typename T>
inline void emit_value(const T &value);

template <typename T>
concept reflect_string_value = std::same_as<T, reflect::str_view>;

template <typename T>
concept enum_value = std::is_enum_v<T>;

template <typename T>
concept reflectable_value = reflect::reflectable<T>;

template <reflect_string_value T>
struct formatter<T> {
    template <format_spec Spec>
    static void emit(reflect::str_view value)
    {
        static_assert(Spec.presentation_value == presentation::DEFAULT ||
                          Spec.presentation_value == presentation::STRING,
                      "unsupported format specifier for string");

        usize length = value.size();

        if constexpr (Spec.has_precision) {
            if (length > Spec.precision) {
                length = Spec.precision;
            }
        }

        emit_padded<Spec>("", 0, value.data(), length, false);
    }
};

template <enum_value T>
struct formatter<T> {
    template <format_spec Spec>
    static void emit(T value)
    {
        using underlying = std::underlying_type_t<T>;

        // An explicit numeric spec asks for the value, not the name.
        if constexpr (Spec.presentation_value == presentation::DECIMAL ||
                      Spec.presentation_value == presentation::HEX_LOWER ||
                      Spec.presentation_value == presentation::HEX_UPPER) {
            formatter<underlying>::template emit<Spec>(value as(underlying));
        } else {
            const reflect::str_view name = reflect::enum_name(value);

            if (name.empty()) {
                formatter<underlying>::template emit<Spec>(value as(underlying));
                return;
            }

            formatter<reflect::str_view>::template emit<Spec>(name);
        }
    }
};

template <reflectable_value T>
struct formatter<T> {
    template <format_spec Spec>
    static void emit(const T &value)
    {
        static_assert(!Spec.has_precision, "precision is not supported for aggregate formats");

        emit_char('{');

        bool first = true;

        reflect::for_each_field(value, [&](reflect::str_view name, const auto &field) {
            if (!first) {
                emit_bytes(", ", 2);
            }

            first = false;

            emit_bytes(name.data(), name.size());
            emit_char('=');

            emit_value<Spec>(field);
        });

        emit_char('}');
    }
};

template <format_spec Spec, typename T>
inline void emit_value(const T &value)
{
    formatter<std::decay_t<decltype(value)>>::template emit<Spec>(value);
}

}  // namespace detail

}  // namespace kernel::debug

#endif  // DOOM_OS_KERNEL_DEBUG_FORMATTER_HPP_
