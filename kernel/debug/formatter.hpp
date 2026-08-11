#ifndef DOOM_OS_KERNEL_DEBUG_FORMATTER_HPP_
#define DOOM_OS_KERNEL_DEBUG_FORMATTER_HPP_

// =================================================================================================
// Cpp stdlib files
// =================================================================================================

#include <concepts>
#include <type_traits>

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/debug/emit.hpp"

namespace kernel::debug {
namespace detail {
using kernel::core::i64;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

// =================================================================================================
// Integer classification
//
// Deliberately narrower than std::is_integral: bool and plain char are excluded because both
// have their own formatting, and must not be dispatched to the integer emitters below.
// =================================================================================================

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

// =================================================================================================
// Formattable categories
//
// Disjoint by construction, so a type matches exactly one formatter and there is no overload set
// to reason about. The integer concepts above already exclude bool and plain char, which is why
// those two get categories of their own rather than falling in with the integers.
// =================================================================================================

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

// =================================================================================================
// Formatter
//
// Maps a type to the emitter that renders it, and nothing else - whether a given spec makes
// sense for that emitter is the emitter's own static_assert, so the rule lives in one place.
//
// Teaching kprint a new type means specialising formatter<T>; the primary template exists to
// say so rather than to fail with a wall of overload candidates.
// =================================================================================================

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
            emit_unsigned_decimal<Spec>(static_cast<u64>(value));
        } else if constexpr (Spec.presentation_value == presentation::HEX_LOWER ||
                             Spec.presentation_value == presentation::HEX_UPPER) {
            emit_hex_integer<Spec>(static_cast<u64>(value));
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
            emit_signed_decimal<Spec>(static_cast<i64>(value));
        } else if constexpr (Spec.presentation_value == presentation::HEX_LOWER ||
                             Spec.presentation_value == presentation::HEX_UPPER) {
            emit_hex_integer<Spec>(static_cast<u64>(static_cast<i64>(value)));
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
        emit_fixed_float<Spec>(static_cast<long double>(value));
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
            emit_pointer_value<Spec>(static_cast<const volatile void *>(value));
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
        emit_pointer_value<Spec>(static_cast<const volatile void *>(value));
    }
};

// =================================================================================================
// Dispatch
//
// decay collapses the cases the old overload set spelled out one by one: arrays of char become
// char pointers, references and cv-qualifiers fall away, so each type reaches exactly one
// formatter specialisation.
// =================================================================================================

template <format_spec Spec, typename T>
inline void emit_value(const T &value)
{
    formatter<std::decay_t<decltype(value)>>::template emit<Spec>(value);
}

}  // namespace detail
}  // namespace kernel::debug

#endif  // DOOM_OS_KERNEL_DEBUG_FORMATTER_HPP_
