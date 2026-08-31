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

#include "kernel/core/reflect.hpp"
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
// Reflected values
//
// reflect answers what fields a type has, and these three specialisations are the whole of its
// contact with kprint. A flags struct or an error enum then formats without a formatter of its
// own, which is what stops this file growing a specialisation per struct as the kernel does.
//
// Names come out of __PRETTY_FUNCTION__ as a pointer and a length into the middle of that
// signature, so they are not NUL-terminated. Every path below carries the length explicitly:
// handing name.data() to a C-string emitter runs off the end of the name and prints the rest of
// the mangled signature.
// =================================================================================================

// Defined below, in the dispatch section. Declared here because the aggregate formatter recurses
// through it to reach its fields.
template <format_spec Spec, typename T>
inline void emit_value(const T &value);

template <typename T>
concept reflect_string_value = std::same_as<T, reflect::str_view>;

template <typename T>
concept enum_value = std::is_enum_v<T>;

// str_view keeps its storage private, so it is not an aggregate and never reaches this one -
// which is what keeps it disjoint from reflect_string_value above.
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
            formatter<underlying>::template emit<Spec>(static_cast<underlying>(value));
        } else {
            const reflect::str_view name = reflect::enum_name(value);

            // No enumerator names this value: a cast, a mask, or a field that was corrupted.
            // Printing the number beats printing nothing, and this is a panic path.
            if (name.empty()) {
                formatter<underlying>::template emit<Spec>(static_cast<underlying>(value));
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

            // The spec reaches the leaves rather than the braces: {:#018X} on a register frame
            // is what makes every register in it print as padded hex.
            emit_value<Spec>(field);
        });

        emit_char('}');
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
