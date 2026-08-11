# =================================================================================================
# Cross toolchain for the bare-metal x86_64-elf target.
#
# Configure with:
#   cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-x86_64-elf.cmake
# =================================================================================================

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(DOOM_OS_TOOLCHAIN_PREFIX x86_64-elf- CACHE STRING "Cross toolchain program prefix")

set(CMAKE_C_COMPILER ${DOOM_OS_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${DOOM_OS_TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${DOOM_OS_TOOLCHAIN_PREFIX}gcc)

set(CMAKE_OBJCOPY ${DOOM_OS_TOOLCHAIN_PREFIX}objcopy CACHE FILEPATH "objcopy")
set(CMAKE_OBJDUMP ${DOOM_OS_TOOLCHAIN_PREFIX}objdump CACHE FILEPATH "objdump")
set(CMAKE_NM ${DOOM_OS_TOOLCHAIN_PREFIX}nm CACHE FILEPATH "nm")

# CMake checks a compiler by linking a hosted executable, which cannot work against a
# freestanding target with no libc and no start files. Building a static library instead is
# what makes the configure step succeed at all.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Look for programs on the host, but never pick up host headers or libraries.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
