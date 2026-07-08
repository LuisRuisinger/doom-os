#ifndef DOOM_OS_KERNEL_DEBUG_KPANIC_HPP_
#define DOOM_OS_KERNEL_DEBUG_KPANIC_HPP_

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

#define DOOM_OS_KPANIC_GPRS(X) \
    X(rax, "rax")              \
    X(rbx, "rbx")              \
    X(rcx, "rcx")              \
    X(rdx, "rdx")              \
    X(rsi, "rsi")              \
    X(rdi, "rdi")              \
    X(rbp, "rbp")              \
    X(rsp, "rsp")              \
    X(r8, "r8")                \
    X(r9, "r9")                \
    X(r10, "r10")              \
    X(r11, "r11")              \
    X(r12, "r12")              \
    X(r13, "r13")              \
    X(r14, "r14")              \
    X(r15, "r15")

#define DOOM_OS_KPANIC_SEGS(X) \
    X(cs, "cs")                \
    X(ds, "ds")                \
    X(es, "es")                \
    X(fs, "fs")                \
    X(gs, "gs")                \
    X(ss, "ss")

#define DOOM_OS_KPANIC_CRS(X) \
    X(cr0, "cr0")             \
    X(cr2, "cr2")             \
    X(cr3, "cr3")             \
    X(cr4, "cr4")

struct panic_register_frame {
#define FIELD(name, asm_name) u64 name;

    DOOM_OS_KPANIC_GPRS(FIELD)

    u64 rip;
    u64 rflags;

    DOOM_OS_KPANIC_SEGS(FIELD)
    DOOM_OS_KPANIC_CRS(FIELD)

#undef FIELD
};

[[noreturn]] void kpanic(const char *message, void *frame, const char *file, int line,
                         const char *function) noexcept;

template <detail::fixed_string FMT, typename... Args>
[[noreturn]] inline void kpanic_fmt(void *frame, const char *file, int line, const char *function,
                                    const Args &...args) noexcept {
    asm volatile("cli" ::: "memory");

    detail::backend_emit_c_string(detail::PANIC_PREFIX);
    detail::backend_emit_char(' ');
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

#define DOOM_OS_KPANIC_CAPTURE() \
    asm volatile(                                                                  \
        DOOM_OS_KPANIC_GPRS(DOOM_OS_KPANIC_GPR_LINE)                               \
        "leaq 0f(%%rip), %%rax\n"                                                  \
        "mov %%rax, %[out_rip]\n"                                                  \
        "pushfq\n"                                                                 \
        "popq %%rax\n"                                                             \
        "mov %%rax, %[out_rflags]\n"                                               \
        DOOM_OS_KPANIC_SEGS(DOOM_OS_KPANIC_SEG_LINE)                               \
        DOOM_OS_KPANIC_CRS(DOOM_OS_KPANIC_CR_LINE)                                 \
        "0:\n"                                                                     \
        : [out_rip] "=m"(__panic_frame.rip)                                        \
          , [out_rflags] "=m"(__panic_frame.rflags)                                \
          DOOM_OS_KPANIC_GPRS(DOOM_OS_KPANIC_OUT)                                  \
          DOOM_OS_KPANIC_SEGS(DOOM_OS_KPANIC_OUT)                                  \
          DOOM_OS_KPANIC_CRS(DOOM_OS_KPANIC_OUT)                                   \
        :                                                                          \
        : "rax", "memory"                                                          \
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

#define KPANIC_SELECT(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, \
                      NAME, ...)                                                                 \
    NAME

#define KPANIC(...)                                                                             \
    KPANIC_SELECT(_ __VA_OPT__(, ) __VA_ARGS__, KPANIC_FMT, KPANIC_FMT, KPANIC_FMT, KPANIC_FMT, \
                  KPANIC_FMT, KPANIC_FMT, KPANIC_FMT, KPANIC_FMT, KPANIC_FMT, KPANIC_FMT,       \
                  KPANIC_FMT, KPANIC_FMT, KPANIC_FMT, KPANIC_FMT, KPANIC_FMT, KPANIC_1,         \
                  KPANIC_0)(__VA_ARGS__)

#endif  // DOOM_OS_KERNEL_DEBUG_KPANIC_HPP_
