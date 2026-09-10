#ifndef DOOM_OS_KERNEL_DEBUG_KPANIC_HPP_
#define DOOM_OS_KERNEL_DEBUG_KPANIC_HPP_

#include "arch/x86_64/cpu/registers.hpp"
#include "kernel/core/types.hpp"
#include "kernel/debug/kprint.hpp"

namespace kernel::debug {

using kernel::core::u64;

namespace detail {

inline constexpr const char *PANIC_PREFIX = "\x1b[1;31m[panic]\x1b[0m";

}  // namespace detail

// =================================================================================================
// Panic register frame
// =================================================================================================

struct panic_register_frame {
#define FIELD(name, asm_name) u64 name;

    DOOM_OS_X86_GPRS(FIELD)

    u64 rip;
    u64 rflags;

    DOOM_OS_X86_SEGS(FIELD)
    DOOM_OS_X86_CRS(FIELD)

#undef FIELD
};

[[noreturn]] void kpanic(const char *message, void *frame, const char *file, int line,
                         const char *function) noexcept;

template <detail::fixed_string FMT, typename... Args>
[[noreturn]] inline void kpanic_fmt(void *frame, const char *file, int line, const char *function,
                                    const Args &...args) noexcept
{
    asm volatile("cli" ::: "memory");

    detail::emit_c_string(detail::PANIC_PREFIX);
    detail::emit_char(' ');
    kprintln_ct<FMT>(args...);

    kpanic(nullptr, frame, file, line, function);
}

}  // namespace kernel::debug

// =================================================================================================
// Panic register capture
// =================================================================================================

#define DOOM_OS_KPANIC_GPR_LINE(name, asm_name) "mov %%" asm_name ", %[out_" #name "]\n"

#define DOOM_OS_KPANIC_SEG_LINE(name, asm_name) \
    "mov %%" asm_name                           \
    ", %%ax\n"                                  \
    "movzwq %%ax, %%rax\n"                      \
    "mov %%rax, %[out_" #name "]\n"

#define DOOM_OS_KPANIC_CR_LINE(name, asm_name) \
    "mov %%" asm_name                          \
    ", %%rax\n"                                \
    "mov %%rax, %[out_" #name "]\n"

#define DOOM_OS_KPANIC_OUT(name, asm_name) , [out_##name] "=m"(__panic_frame.name)

#define DOOM_OS_KPANIC_CAPTURE()                    \
    asm volatile(                                   \
        DOOM_OS_X86_GPRS(DOOM_OS_KPANIC_GPR_LINE)   \
        "leaq 0f(%%rip), %%rax\n"                   \
        "mov %%rax, %[out_rip]\n"                   \
        "pushfq\n"                                  \
        "popq %%rax\n"                              \
        "mov %%rax, %[out_rflags]\n"                \
        DOOM_OS_X86_SEGS(DOOM_OS_KPANIC_SEG_LINE)   \
        DOOM_OS_X86_CRS(DOOM_OS_KPANIC_CR_LINE)     \
        "0:\n"                                      \
        : [out_rip] "=m"(__panic_frame.rip)         \
          , [out_rflags] "=m"(__panic_frame.rflags) \
          DOOM_OS_X86_GPRS(DOOM_OS_KPANIC_OUT)      \
          DOOM_OS_X86_SEGS(DOOM_OS_KPANIC_OUT)      \
          DOOM_OS_X86_CRS(DOOM_OS_KPANIC_OUT)       \
        :                                           \
        : "rax", "memory"                           \
    )

// =================================================================================================
// Panic macros
// =================================================================================================

#define KPANIC_0()                                                                      \
    do {                                                                                \
        ::kernel::debug::panic_register_frame __panic_frame{};                          \
        DOOM_OS_KPANIC_CAPTURE();                                                       \
        ::kernel::debug::kpanic(nullptr, &__panic_frame, __FILE__, __LINE__, __func__); \
    } while (0)

#define KPANIC_1(message)                                                                 \
    do {                                                                                  \
        ::kernel::debug::panic_register_frame __panic_frame{};                            \
        DOOM_OS_KPANIC_CAPTURE();                                                         \
        ::kernel::debug::kpanic((message), &__panic_frame, __FILE__, __LINE__, __func__); \
    } while (0)

#define KPANIC_FMT(fmt__, ...)                                                     \
    do {                                                                           \
        ::kernel::debug::panic_register_frame __panic_frame{};                     \
        DOOM_OS_KPANIC_CAPTURE();                                                  \
        ::kernel::debug::kpanic_fmt<::kernel::debug::detail::fixed_string{fmt__}>( \
            &__panic_frame, __FILE__, __LINE__, __func__, __VA_ARGS__);            \
    } while (0)

// Panic with a frame that was built elsewhere rather than captured here. Used by the fault
// path, where the registers worth reporting belong to the interrupted context, not to the
// handler that is about to print them.
#define KPANIC_WITH_FRAME(frame__, fmt__, ...)                                     \
    do {                                                                           \
        ::kernel::debug::kpanic_fmt<::kernel::debug::detail::fixed_string{fmt__}>( \
            &(frame__), __FILE__, __LINE__, __func__, __VA_ARGS__);                \
    } while (0)

#define KPANIC_SELECT(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, \
                      NAME, ...)                                                                 \
    NAME

#define KPANIC(...)                                                                             \
    KPANIC_SELECT(_ __VA_OPT__(, ) __VA_ARGS__, KPANIC_FMT, KPANIC_FMT, KPANIC_FMT, KPANIC_FMT, \
                  KPANIC_FMT, KPANIC_FMT, KPANIC_FMT, KPANIC_FMT, KPANIC_FMT, KPANIC_FMT,       \
                  KPANIC_FMT, KPANIC_FMT, KPANIC_FMT, KPANIC_FMT, KPANIC_FMT, KPANIC_1,         \
                  KPANIC_0)(__VA_ARGS__)

#endif  // DOOM_OS_KERNEL_DEBUG_KPANIC_HPP_
