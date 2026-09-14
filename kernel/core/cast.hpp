#ifndef DOOM_OS_KERNEL_CORE_CAST_HPP_
#define DOOM_OS_KERNEL_CORE_CAST_HPP_

#include <cstddef>
#include <type_traits>
#include <utility>

// =================================================================================================
// RTTI Configuration
// =================================================================================================

#if defined(__cpp_rtti) || defined(__GXX_RTTI)
#    define DOOM_OS_CAST_HAS_RTTI 1
#else
#    define DOOM_OS_CAST_HAS_RTTI 0
#endif

#ifndef DOOM_OS_CAST_ENABLE_RTTI
#    define DOOM_OS_CAST_ENABLE_RTTI DOOM_OS_CAST_HAS_RTTI
#endif

#if DOOM_OS_CAST_ENABLE_RTTI && !DOOM_OS_CAST_HAS_RTTI
#    error "DOOM_OS_CAST_ENABLE_RTTI requires compiler RTTI support (-frtti)"
#endif

namespace kernel::core::cast {

inline constexpr bool has_rtti = DOOM_OS_CAST_HAS_RTTI != 0;
inline constexpr bool rtti_enabled = DOOM_OS_CAST_ENABLE_RTTI != 0;

namespace detail {

template <typename T>
using object_t = std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>;

template <typename T>
concept complete = requires { sizeof(T); };

template <typename T>
concept possibly_polymorphic = std::is_class_v<T> && (!complete<T> || std::is_polymorphic_v<T>);

template <typename To, typename From>
concept static_castable = requires(From &&f) { static_cast<To>(std::forward<From>(f)); };

template <typename To, typename From>
concept reinterpret_castable = requires(From &&f) { reinterpret_cast<To>(std::forward<From>(f)); };

template <typename To, typename From>
concept permitted_reinterpret =
    reinterpret_castable<To, From> &&
    !(possibly_polymorphic<object_t<To>> && possibly_polymorphic<object_t<From>>);

#if DOOM_OS_CAST_ENABLE_RTTI

template <typename To, typename From>
concept dynamic_castable = requires(From &&f) { dynamic_cast<To>(std::forward<From>(f)); };

template <typename To, typename From>
concept checked_downcastable = static_castable<To, From> && dynamic_castable<To, From> &&
                               (!std::is_same_v<object_t<To>, object_t<From>>) &&
                               std::is_base_of_v<object_t<From>, object_t<To>>;

template <typename To, typename From>
constexpr To checked_pointer(From *source) noexcept
{
    if (source == nullptr) {
        return nullptr;
    }

    auto *result = dynamic_cast<To>(source);
    using source_view = const volatile std::remove_cv_t<From> *;

    // Verify valid subobject and trap on cast mismatch
    if (result == nullptr || static_cast<source_view>(result) != source) {
        __builtin_trap();
    }

    return result;
}

template <typename To, typename From>
constexpr To checked_downcast(From &&value) noexcept
{
    if constexpr (std::is_pointer_v<To>) {
        return checked_pointer<To>(value);
    } else {
        using target_pointer = std::add_pointer_t<std::remove_reference_t<To>>;
        auto *result = checked_pointer<target_pointer>(__builtin_addressof(value));
        return static_cast<To>(*result);
    }
}

template <typename To, typename From>
constexpr To dynamic_cast_or_trap(From &&value) noexcept
{
    if constexpr (std::is_pointer_v<To>) {
        return dynamic_cast<To>(value);
    } else {
        using target_pointer = std::add_pointer_t<std::remove_reference_t<To>>;
        auto *result = dynamic_cast<target_pointer>(__builtin_addressof(value));
        if (result == nullptr) {
            __builtin_trap();
        }
        return static_cast<To>(*result);
    }
}

#endif  // DOOM_OS_CAST_ENABLE_RTTI

}  // namespace detail

// =================================================================================================
// Dispatch Pipeline
// =================================================================================================

template <typename To, typename From>
[[nodiscard]] constexpr To auto_cast(From &&value) noexcept
{
    if constexpr (std::is_void_v<To>) {
        static_cast<void>(value);
    }
#if DOOM_OS_CAST_ENABLE_RTTI
    // 1. Checked polymorphic downcast (traps on invalid type instead of throwing)
    else if constexpr (detail::checked_downcastable<To, From>) {
        return detail::checked_downcast<To>(std::forward<From>(value));
    }
#endif
    // 2. Standard conversions, upcasts, and non-polymorphic casts
    else if constexpr (detail::static_castable<To, From>) {
        return static_cast<To>(std::forward<From>(value));
    }
#if DOOM_OS_CAST_ENABLE_RTTI
    // 3. Dynamic cross-casting (when static_cast is invalid)
    else if constexpr (detail::dynamic_castable<To, From>) {
        return detail::dynamic_cast_or_trap<To>(std::forward<From>(value));
    }
#endif
    // 4. Pointer/integer reinterpretation (blocks accidental polymorphic slicing)
    else if constexpr (detail::permitted_reinterpret<To, From>) {
        return reinterpret_cast<To>(std::forward<From>(value));
    } else {
        static_assert(!sizeof(From), "Invalid cast: no compatible conversion strategy available.");
    }
}

// Infix tag carrier
template <typename To>
struct as_tag {};

template <typename From, typename To>
[[nodiscard]] constexpr To operator->*(From &&from, as_tag<To>) noexcept
{
    return auto_cast<To>(std::forward<From>(from));
}

}  // namespace kernel::core::cast

#ifdef as
#    undef as
#endif

#define as(...)                                  \
    ->*::kernel::core::cast::as_tag<__VA_ARGS__> \
    {                                            \
    }

// =================================================================================================
// Compile-Time Tests
// =================================================================================================

static_assert(3.5 as(int) == 3);
static_assert((3.5 as(int) + 2) == 5);

#endif  // DOOM_OS_KERNEL_CORE_CAST_HPP_
