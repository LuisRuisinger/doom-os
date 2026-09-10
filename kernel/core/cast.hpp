#ifndef DOOM_OS_KERNEL_CORE_CAST_HPP_
#define DOOM_OS_KERNEL_CORE_CAST_HPP_

// =================================================================================================
// Cpp stdlib files
// =================================================================================================

#include <stddef.h>

#include <type_traits>
#include <utility>

// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/core/types.hpp"

// =================================================================================================
// `x as T`
// =================================================================================================
//
// An infix cast: `value as Target`. The bare form picks the cast itself - static_cast when that is
// well-formed, a checked static_cast for a polymorphic downcast, dynamic_cast where only that is
// valid, and reinterpret_cast as the last resort for pointer/integer punning. Wrapping the target
// in a strategy tag pins one cast and refuses everything else:
//
//     p  as u8*                      automatic (see auto_cast for the order)
//     p  as to<u8*>                  static_cast only
//     p  as reinterpret<u8*>         reinterpret_cast only
//     b  as down<derived*>           static_cast + is_base_of check (+ RTTI check where RTTI exists)
//     d  as up<base&>                implicit conversion only (boost::implicit_cast)
//     b  as dynamic<derived*>        dynamic_cast only, requires RTTI - unavailable in the kernel
//
// How the target type gets out of a bare type-id: `as` expands to `->* tag{} ->* new (tag{}) T`.
// The first ->* binds the operand, the second receives the placement-new expression. The tag
// overload of operator new is noexcept and returns nullptr, so the new-expression evaluates to a
// `T*` null pointer and initialises nothing ([expr.new]); its only job is carrying T. The
// operator->* deduces T from that pointer and dispatches on it. Nothing survives -O2.
//
// The alternative front end, `->* &as_t::operator T` with a conversion function template, is the
// one usually posted. GCC rejects it: `&as_t::operator T` is an unresolved overload set to GCC and
// deduction from it fails ([temp.deduct.call]/6); only Clang treats the conversion-type-id as
// naming a single specialisation. The kernel is built with x86_64-elf-g++, hence new.
//
// Consequences of going through new:
//   - a bare target must be a type `new T` accepts: scalars, pointers, default-constructible
//     classes. Reference targets and classes without a default constructor need a tag
//     (`x as to<base&>`);
//   - the expression is not a constant expression;
//   - a bare target greedily absorbs trailing declarators: `x as int * 2` is a cast to int*.
//     Parenthesise, or use a tag - `>` closes the type.
//
// `as` is a global macro. No identifier in the tree spells `as` today; keep it that way.

namespace kernel::core::cast {

// =================================================================================================
// Strategy tags
// =================================================================================================

template <class T>
struct to {};  // static_cast

template <class T>
struct reinterpret {};  // reinterpret_cast

template <class T>
struct dynamic {};  // dynamic_cast

template <class T>
struct down {};  // checked polymorphic downcast (boost::polymorphic_downcast)

template <class T>
struct up {};  // implicit conversion (boost::implicit_cast)

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
// static_cast, with the base/derived relation checked at compile time and, where RTTI exists, the
// dynamic type checked at runtime. The kernel builds with -fno-rtti, so there it is the static
// check only; the runtime check is for hosted builds that include this header.

template <class To, class From>
constexpr To checked_downcast(From&& f)
{
    static_assert(std::is_pointer_v<std::remove_reference_t<To>> || std::is_reference_v<To>,
                  "as: down<> targets a pointer or reference");
    static_assert(std::is_base_of_v<raw_t<From>, raw_t<To>> && !std::is_same_v<raw_t<From>, raw_t<To>>,
                  "as: down<> target is not derived from the source");

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
// Automatic strategy
// =================================================================================================
//
// Order: static_cast, dynamic_cast, reinterpret_cast. Under -fno-rtti, dynamic_cast is never
// well-formed, so a polymorphic downcast is an unchecked static_cast and a cross-cast has no valid
// strategy - which is why reinterpret_cast is refused between two class types: without it a
// sibling cast would silently become pointer punning in the kernel and a dynamic_cast in a hosted
// build. Class-to-class punning is spelled `reinterpret<>`.

template <class To, class From>
constexpr decltype(auto) auto_cast(From&& f)
{
    constexpr bool can_static = requires { static_cast<To>(std::forward<From>(f)); };
    constexpr bool can_dynamic = requires { dynamic_cast<To>(std::forward<From>(f)); };
    constexpr bool can_reinterpret = requires { reinterpret_cast<To>(std::forward<From>(f)); };

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
    // Reinterpret: integer/pointer, pointer/pointer - but not class/class, see above.
    else if constexpr (can_reinterpret && !(std::is_class_v<raw_t<From>> && std::is_class_v<raw_t<To>>)) {
        return reinterpret_cast<To>(std::forward<From>(f));
    } else {
        static_assert(always_false_v<To>, "as: no cast strategy applies; spell one with a tag");
    }
}

// =================================================================================================
// Policies
// =================================================================================================

template <class Spelled>
struct policy {  // bare target: automatic
    template <class From>
    static constexpr decltype(auto) cast(From&& f)
    {
        return auto_cast<Spelled>(std::forward<From>(f));
    }
};

template <class To>
struct policy<to<To>> {
    template <class From>
    static constexpr To cast(From&& f)
    {
        static_assert(requires { static_cast<To>(std::forward<From>(f)); }, "as: to<> is not a static_cast");
        return static_cast<To>(std::forward<From>(f));
    }
};

template <class To>
struct policy<reinterpret<To>> {
    template <class From>
    static To cast(From&& f)
    {
        static_assert(requires { reinterpret_cast<To>(std::forward<From>(f)); },
                      "as: reinterpret<> is not a reinterpret_cast");
        return reinterpret_cast<To>(std::forward<From>(f));
    }
};

template <class To>
struct policy<dynamic<To>> {
    template <class From>
    static To cast(From&& f)
    {
        static_assert(has_rtti, "as: dynamic<> needs RTTI, which the kernel is built without");
        static_assert(!has_rtti || requires { dynamic_cast<To>(std::forward<From>(f)); },
                      "as: dynamic<> is not a dynamic_cast");
        return dynamic_cast<To>(std::forward<From>(f));
    }
};

template <class To>
struct policy<down<To>> {
    template <class From>
    static constexpr To cast(From&& f)
    {
        return checked_downcast<To>(std::forward<From>(f));
    }
};

template <class To>
struct policy<up<To>> {
    template <class From>
    static constexpr To cast(From&& f)
    {
        static_assert(std::is_convertible_v<From, To>, "as: up<> is not an implicit conversion");
        return std::forward<From>(f);
    }
};

}  // namespace detail

// =================================================================================================
// Front end
// =================================================================================================

template <class From>
struct bound {
    From v;

    template <class Spelled>
    constexpr decltype(auto) operator->*(Spelled *) const
    {
        return detail::policy<Spelled>::cast(std::forward<From>(v));
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
