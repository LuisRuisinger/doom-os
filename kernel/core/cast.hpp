#ifndef DOOM_OS_KERNEL_CORE_CAST_HPP_
#define DOOM_OS_KERNEL_CORE_CAST_HPP_

// =================================================================================================
// Cpp stdlib files
// =================================================================================================

#include <stddef.h>

#include <type_traits>
#include <utility>

// =================================================================================================
// `x as T`
// =================================================================================================
//
// An infix cast that picks the cast itself:
//
//     void *aligned = ((ptr as uintptr_t + (align - 1)) & ~(align - 1)) as void *;
//
// Order, per (source, target) pair, decided at compile time - see auto_cast:
//   1. static_cast where well-formed. A polymorphic downcast is additionally checked against
//      dynamic_cast where RTTI exists; the kernel has none, so there it is a plain static_cast.
//   2. dynamic_cast where only that is well-formed (cross-casts, casts across a virtual base).
//      Never in the kernel: under -fno-rtti dynamic_cast is never well-formed.
//   3. reinterpret_cast: integer/pointer and pointer/pointer punning. Refused between two
//      polymorphic class types - that is a cross-cast, which step 2 would have handled with RTTI
//      and which must not quietly become pointer punning without it. Spell reinterpret_cast.
//   4. otherwise a static_assert.
//
// How the target type gets out of a bare type-id: `as` expands to `->* tag{} ->* new (tag{}) T`.
// The first ->* binds the operand, the second receives the placement-new expression. The tag
// overload of operator new is noexcept and returns nullptr, so the new-expression evaluates to a
// `T*` null pointer and initialises nothing ([expr.new]); its only job is carrying T. The
// operator->* deduces T from that pointer. Nothing survives -O2.
//
// The alternative front end, `->* &as_t::operator T` with a conversion function template, is the
// one usually posted. GCC rejects it: `&as_t::operator T` is an unresolved overload set to GCC and
// deduction from it fails ([temp.deduct.call]/6); only Clang treats the conversion-type-id as
// naming a single specialisation. The kernel is built with x86_64-elf-g++, hence new.
//
// Consequences of going through new:
//   - the target must be a type `new T` accepts: scalars, pointers, default-constructible classes.
//     No reference targets - cast the address instead - and no classes without a default
//     constructor;
//   - the expression is not a constant expression;
//   - the target greedily absorbs trailing declarators: `x as int * 2` is a cast to int*.
//     Parenthesise: `(x as int) * 2`. Binary operators other than * and & are safe, which is why
//     `ptr as uintptr_t + (align - 1)` parses as intended - and ->* binds tighter than +.
//
// `as` is a global macro. No identifier in the tree spells `as` today; keep it that way.

namespace kernel::core::cast {

struct tag {};

namespace detail {

template <class...>
inline constexpr bool always_false_v = false;

#if defined(__cpp_rtti)
inline constexpr bool has_rtti = true;
#else
inline constexpr bool has_rtti = false;
#endif

template <class T>
using raw_t = std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>;

// =================================================================================================
// Checked downcast
// =================================================================================================
//
// static_cast; where RTTI exists the dynamic type is verified first. The kernel builds with
// -fno-rtti, so there this is the static_cast alone - the check is for hosted builds that include
// this header.

template <class To, class From>
constexpr To checked_downcast(From&& f)
{
    if constexpr (has_rtti) {
        if constexpr (std::is_pointer_v<std::remove_reference_t<To>>) {
            if (!(f == nullptr || dynamic_cast<To>(f) == static_cast<To>(f))) {
                __builtin_trap();
            }
        } else {
            using ptr = std::add_pointer_t<std::remove_reference_t<To>>;
            if (dynamic_cast<ptr>(__builtin_addressof(f)) != static_cast<ptr>(__builtin_addressof(f))) {
                __builtin_trap();
            }
        }
    }

    return static_cast<To>(std::forward<From>(f));
}

// =================================================================================================
// Strategy selection
// =================================================================================================

template <class To, class From>
constexpr decltype(auto) auto_cast(From&& f)
{
    constexpr bool can_static = requires { static_cast<To>(std::forward<From>(f)); };
    constexpr bool can_dynamic = requires { dynamic_cast<To>(std::forward<From>(f)); };
    constexpr bool can_reinterpret = requires { reinterpret_cast<To>(std::forward<From>(f)); };
    constexpr bool cross_cast = std::is_polymorphic_v<raw_t<From>> && std::is_polymorphic_v<raw_t<To>>;

    // Both static and dynamic: upcast/identity is free, downcast is checked, anything else
    // (polymorphic pointer to void*) is static.
    if constexpr (can_static && can_dynamic) {
        if constexpr (std::is_base_of_v<raw_t<To>, raw_t<From>>) {
            return static_cast<To>(std::forward<From>(f));
        } else if constexpr (std::is_base_of_v<raw_t<From>, raw_t<To>>) {
            return checked_downcast<To>(std::forward<From>(f));
        } else {
            return static_cast<To>(std::forward<From>(f));
        }
    }
    // Static only: arithmetic, void*, non-polymorphic hierarchies, explicit constructors.
    else if constexpr (can_static) {
        return static_cast<To>(std::forward<From>(f));
    }
    // Dynamic only: cross-casts, downcasts across a virtual base.
    else if constexpr (can_dynamic) {
        return dynamic_cast<To>(std::forward<From>(f));
    }
    // Reinterpret: integer/pointer, pointer/pointer - but not a cross-cast, see the header comment.
    else if constexpr (can_reinterpret && !cross_cast) {
        return reinterpret_cast<To>(std::forward<From>(f));
    } else {
        static_assert(always_false_v<To>, "as: no cast applies");
    }
}

}  // namespace detail

// =================================================================================================
// Front end
// =================================================================================================

template <class From>
struct bound {
    From v;

    template <class To>
    constexpr decltype(auto) operator->*(To *) const
    {
        return detail::auto_cast<To>(std::forward<From>(v));
    }
};

template <class From>
constexpr bound<From &&> operator->*(From &&v, tag) noexcept
{
    return {std::forward<From>(v)};
}

}  // namespace kernel::core::cast

// The carrier for `new (tag) T`: noexcept and null, so the new-expression constructs nothing and
// yields (T*)nullptr. Not an allocator; the kernel has no global operator new and this is not one.
inline void *operator new(size_t, ::kernel::core::cast::tag) noexcept
{
    return nullptr;
}

#define as ->* ::kernel::core::cast::tag{} ->* new (::kernel::core::cast::tag{})

#endif  // DOOM_OS_KERNEL_CORE_CAST_HPP_
