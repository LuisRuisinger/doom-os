#ifndef DOOM_OS_KERNEL_CORE_REFLECT_HPP_
#define DOOM_OS_KERNEL_CORE_REFLECT_HPP_

// =================================================================================================
// reflect
//
// Single point where the third-party reflect is pulled in, so that every translation unit sees
// the same configuration.
//
// The namespace is deliberately not chosen here. result and reflect both default to lsr and both
// put their freestanding fallbacks in lsr::detail - index_sequence, make_index_sequence_impl and
// is_named_enum_value are defined by each - so the two headers cannot share a translation unit
// until one of them moves. REFLECT_NAMESPACE does that, and the build defines it for every
// translation unit rather than this header defining it, because a namespace that varied per TU
// would quietly produce two different reflect::str_view types across one link.
//
// Include this header rather than <reflect/reflect.hpp> directly.
// =================================================================================================

#ifndef REFLECT_NAMESPACE
#    error "REFLECT_NAMESPACE must be defined by the build; see CMakeLists.txt"
#endif

#include <reflect/enum.hpp>
#include <reflect/reflect.hpp>

#endif  // DOOM_OS_KERNEL_CORE_REFLECT_HPP_
