// =================================================================================================
// Kernel files
// =================================================================================================

#include "kernel/runtime/memory.hpp"
#include "kernel/debug/kpanic.hpp"

// =================================================================================================
// libos files
// =================================================================================================

#include "libos/abi.hpp"

// =================================================================================================
// The C library subset the platform owns
//
// On Linux none of this would be here. The boundary there is the syscall: the kernel supplies brk
// and mmap, and malloc belongs to whichever libc the program linked. That split exists because a
// process and a kernel are separated by a privilege boundary worth defending.
//
// A unikernel has no such boundary, and every one of them resolves this the same way - OSv bundles
// musl, Unikraft offers a choice of libc as a library, HermitCore ships its own. With one binary
// there is nothing for a separate C library to protect, and something has to answer malloc before
// the first std function runs.
//
// So the line is drawn at the glibc ABI rather than at the syscall, which is where
// libos/glibc_abi.cpp already drew it: __errno_location and pthread_key_create are no more the
// kernel's than malloc is. This file is that decision applied consistently, not a new one.
//
// It is a subset, and being link-compatible is not the same as being compliant. What is here has
// glibc's signatures and glibc's behaviour for the cases it handles; what is not here refuses.
// =================================================================================================

// Weak, for the reason libos/glibc_abi.cpp is weak: an application that links its own C library
// has a real malloc, and the real one must win. What is here is the fallback for an application
// that brought none.
#define DOOM_OS_WEAK __attribute__((weak))

namespace {

using namespace libos::abi;  // NOLINT(google-build-using-namespace)

using kernel::core::u8;
using kernel::core::usize;

// =================================================================================================
// Allocator
//
// First fit over a free list, carved from sbrk. Deliberately simple: it reuses freed blocks but
// does not coalesce adjacent ones, so a workload that alternates sizes will fragment. That is a
// real limit rather than a hidden one, and the honest place to fix it is the VMM - sbrk cannot
// grow, so a smarter allocator over the same 2 MiB would only postpone the same wall.
//
// Every block carries its size because realloc needs the old one and free is given no length.
// =================================================================================================

constexpr usize ALIGNMENT = 16;

struct block {
    usize  size;   // payload bytes, not counting this header
    block *next;   // free list link; meaningless while the block is handed out
};

static_assert(sizeof(block) % ALIGNMENT == 0, "a header must not disturb payload alignment");

block *g_free_list = nullptr;

usize aligned_up(usize value)
{
    return (value + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

}  // namespace

extern "C" void *sbrk(ptrdiff_t increment);

extern "C" DOOM_OS_WEAK void *malloc(size_t size)
{
    if (size == 0) {
        // Distinct, freeable, and not null: callers test the result rather than the request.
        size = 1;
    }

    const usize payload = aligned_up(size);

    block *previous = nullptr;

    for (block *candidate = g_free_list; candidate != nullptr; candidate = candidate->next) {
        if (candidate->size >= payload) {
            if (previous == nullptr) {
                g_free_list = candidate->next;
            } else {
                previous->next = candidate->next;
            }

            return candidate + 1;
        }

        previous = candidate;
    }

    void *raw = sbrk(static_cast<ptrdiff_t>(payload + sizeof(block)));

    if (raw == reinterpret_cast<void *>(-1)) {
        errno = ENOMEM;
        return nullptr;
    }

    auto *header = static_cast<block *>(raw);
    header->size = payload;
    header->next = nullptr;

    return header + 1;
}

extern "C" DOOM_OS_WEAK void free(void *pointer)
{
    if (pointer == nullptr) {
        return;
    }

    auto *header = static_cast<block *>(pointer) - 1;
    header->next = g_free_list;
    g_free_list = header;
}

extern "C" DOOM_OS_WEAK void *calloc(size_t count, size_t size)
{
    // Overflow here is the classic way an allocator hands back a buffer smaller than the caller
    // computed, so it is checked rather than assumed.
    if (count != 0 && size > static_cast<size_t>(-1) / count) {
        errno = ENOMEM;
        return nullptr;
    }

    const size_t total = count * size;
    void        *memory = malloc(total);

    if (memory != nullptr) {
        memset(memory, 0, total);
    }

    return memory;
}

extern "C" DOOM_OS_WEAK void *realloc(void *pointer, size_t size)
{
    if (pointer == nullptr) {
        return malloc(size);
    }

    if (size == 0) {
        free(pointer);
        return nullptr;
    }

    const auto *header = static_cast<block *>(pointer) - 1;

    if (header->size >= aligned_up(size)) {
        return pointer;
    }

    void *moved = malloc(size);

    if (moved == nullptr) {
        return nullptr;
    }

    memcpy(moved, pointer, header->size < size ? header->size : size);
    free(pointer);

    return moved;
}

// =================================================================================================
// Strings
//
// The compiler emits calls to mem* regardless of what is linked, so those live in
// kernel/core/memory.cpp and are shared with the kernel. These three are asked for by name.
// =================================================================================================

extern "C" DOOM_OS_WEAK size_t strlen(const char *text)
{
    size_t length = 0;

    while (text[length] != '\0') {
        ++length;
    }

    return length;
}

extern "C" DOOM_OS_WEAK size_t strnlen(const char *text, size_t limit)
{
    size_t length = 0;

    while (length < limit && text[length] != '\0') {
        ++length;
    }

    return length;
}

extern "C" DOOM_OS_WEAK int bcmp(const void *lhs, const void *rhs, size_t count)
{
    return memcmp(lhs, rhs, count);
}

// =================================================================================================
// Environment
//
// libos/posix.cpp publishes an empty environ, so every lookup misses and every store has nowhere
// to go. Reporting failure from setenv is the truthful answer: a caller that checks learns it did
// not happen, and one that does not check would have been lied to by a silent success.
// =================================================================================================

extern "C" char **environ;

extern "C" DOOM_OS_WEAK char *getenv(const char *name)
{
    if (name == nullptr) {
        return nullptr;
    }

    for (char **entry = environ; *entry != nullptr; ++entry) {
        const char *candidate = *entry;
        size_t      index = 0;

        while (name[index] != '\0' && candidate[index] == name[index]) {
            ++index;
        }

        if (name[index] == '\0' && candidate[index] == '=') {
            return const_cast<char *>(candidate + index + 1);
        }
    }

    return nullptr;
}

extern "C" DOOM_OS_WEAK int setenv(const char *name, const char *value, int overwrite)
{
    static_cast<void>(name);
    static_cast<void>(value);
    static_cast<void>(overwrite);

    errno = ENOMEM;
    return -1;
}

extern "C" DOOM_OS_WEAK int unsetenv(const char *name)
{
    static_cast<void>(name);

    return 0;
}

// =================================================================================================
// Process
// =================================================================================================

extern "C" [[noreturn]] void _exit(int status);

extern "C" [[noreturn]] DOOM_OS_WEAK void exit(int status)
{
    _exit(status);
}

extern "C" [[noreturn]] DOOM_OS_WEAK void abort()
{
    KPANIC("abort() called");
}

// No filesystem to resolve against, so there is no path this could answer for.
extern "C" DOOM_OS_WEAK char *realpath(const char *path, char *resolved)
{
    static_cast<void>(path);
    static_cast<void>(resolved);

    errno = ENOSYS;
    return nullptr;
}

#undef DOOM_OS_WEAK
