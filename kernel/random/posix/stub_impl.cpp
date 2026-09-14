// =================================================================================================
// POSIX ABI files
// =================================================================================================

#include "kernel/core/cast.hpp"
#include "posix/abi.hpp"

// =================================================================================================
// Architecture files
// =================================================================================================

#include "arch/x86_64/cpu/registers.hpp"

namespace {

using namespace libos::abi;  // NOLINT(google-build-using-namespace) - this file *is* the ABI

using kernel::core::u32;
using kernel::core::u64;
using kernel::core::u8;
using kernel::core::usize;

constexpr u32 CPUID_FEATURE_LEAF = 1;
constexpr u32 CPUID_ECX_RDRAND = 1u << 30;
constexpr int RDRAND_ATTEMPTS = 10;

bool rdrand_supported()
{
    namespace cpu = kernel::arch::x86_64::cpu;

    if (cpu::cpuid(0).eax < CPUID_FEATURE_LEAF) {
        return false;
    }

    return (cpu::cpuid(CPUID_FEATURE_LEAF).ecx & CPUID_ECX_RDRAND) != 0;
}

bool rdrand64(u64 &out)
{
    for (int attempt = 0; attempt < RDRAND_ATTEMPTS; ++attempt) {
        u64           value;
        unsigned char carry;

        asm volatile("rdrand %0; setc %1" : "=r"(value), "=qm"(carry)::"cc");

        if (carry) {
            out = value;
            return true;
        }
    }

    return false;
}

}  // namespace

// =================================================================================================
// Entropy
//
// RDRAND is used as the current minimal entropy source. This remains x86_64-specific until a
// kernel/random subsystem can sit above hardware or paravirtual entropy drivers.
// =================================================================================================

extern "C" ssize_t getrandom(void *buffer, size_t length, unsigned int flags)
{
    static_cast<void>(flags);

    if (buffer == nullptr) {
        errno = EFAULT;
        return -1;
    }

    if (!rdrand_supported()) {
        errno = ENOSYS;
        return -1;
    }

    u8   *out = buffer as(u8 *);
    usize done = 0;

    while (done < length) {
        u64 word;

        if (!rdrand64(word)) {
            errno = EIO;
            return -1;
        }

        const usize chunk = (length - done) < sizeof word ? (length - done) : sizeof word;

        for (usize i = 0; i < chunk; ++i) {
            out[done + i] = (word >> (i * 8)) as(u8);
        }

        done += chunk;
    }

    return done as(ssize_t);
}

// BSD's spelling of the same thing, and the one a few runtimes reach for first. All or nothing by
// contract, which getrandom already is here.
extern "C" int getentropy(void *buffer, size_t length)
{
    if (length > 256) {
        errno = EIO;
        return -1;
    }

    return getrandom(buffer, length, 0) < 0 ? -1 : 0;
}
