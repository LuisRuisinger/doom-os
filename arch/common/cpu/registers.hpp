#ifndef DOOM_OS_ARCH_COMMON_CPU_REGISTERS_HPP_
#define DOOM_OS_ARCH_COMMON_CPU_REGISTERS_HPP_

#define DOOM_OS_REGISTER_READ(type__, name__, asm__) \
    [[nodiscard]] inline type__ read_##name__()      \
    {                                                \
        type__ value{};                              \
        asm volatile(asm__ : "=r"(value));           \
        return value;                                \
    }

#define DOOM_OS_REGISTER_WRITE(type__, name__, asm__)  \
    inline void write_##name__(type__ value)           \
    {                                                  \
        asm volatile(asm__ : : "r"(value) : "memory"); \
    }

#endif  // DOOM_OS_ARCH_COMMON_CPU_REGISTERS_HPP_
