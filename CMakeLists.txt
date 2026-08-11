cmake_minimum_required(VERSION 3.22)

# =================================================================================================
# DoomOS
#
# Bare-metal x86_64 kernel. Requires the cross toolchain file; configuring without it would try
# to build against the host compiler and produce something that cannot boot.
#
#   cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-x86_64-elf.cmake
#   cmake --build build
# =================================================================================================

if(NOT CMAKE_TOOLCHAIN_FILE AND NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
            "Configure with -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-x86_64-elf.cmake")
endif()

project(DoomOS
        VERSION 0.1.0
        DESCRIPTION "x86_64 kernel"
        LANGUAGES CXX ASM)

# Written out natively, so nothing has to intercept the build to produce it.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# No build type by default: the kernel is compiled without -O or -g unless asked. Pass
# -DCMAKE_BUILD_TYPE=Debug for symbols when working with the GDB stub.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =================================================================================================
# Sources
#
# Globbed rather than listed: components are added often, and CONFIGURE_DEPENDS makes the build
# re-glob rather than silently missing a new file.
# =================================================================================================

file(GLOB_RECURSE KERNEL_CXX_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/kernel/*.cpp)
file(GLOB_RECURSE KERNEL_ASM_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/kernel/*.S)
file(GLOB_RECURSE KERNEL_HEADERS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/kernel/*.hpp)

# =================================================================================================
# Dependencies
# =================================================================================================

add_subdirectory(third_party/result EXCLUDE_FROM_ALL)

# =================================================================================================
# Linker script
#
# Preprocessed so it and the early boot assembly can share the addresses in config/layout.h.
# =================================================================================================

set(DOOM_OS_LINKER_SCRIPT_IN ${CMAKE_SOURCE_DIR}/config/linker.ld.in)
set(DOOM_OS_LAYOUT_HEADER ${CMAKE_SOURCE_DIR}/config/layout.h)
set(DOOM_OS_LINKER_SCRIPT ${CMAKE_BINARY_DIR}/linker.ld)

add_custom_command(
        OUTPUT ${DOOM_OS_LINKER_SCRIPT}
        COMMAND ${CMAKE_CXX_COMPILER}
        -E -P -x c
        -I${CMAKE_SOURCE_DIR}
        ${DOOM_OS_LINKER_SCRIPT_IN}
        -o ${DOOM_OS_LINKER_SCRIPT}
        DEPENDS ${DOOM_OS_LINKER_SCRIPT_IN} ${DOOM_OS_LAYOUT_HEADER}
        COMMENT "Preprocessing linker script"
        VERBATIM)

add_custom_target(linker-script DEPENDS ${DOOM_OS_LINKER_SCRIPT})

# =================================================================================================
# Kernel
# =================================================================================================

add_executable(kernel.elf ${KERNEL_CXX_SOURCES} ${KERNEL_ASM_SOURCES})

add_dependencies(kernel.elf linker-script)

target_include_directories(kernel.elf PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(kernel.elf PRIVATE lsr::result)

target_compile_options(kernel.elf PRIVATE
        # CMake compiles with absolute paths, which would put the build directory into every
        # __FILE__ and so into panic output. Strip it back to a repo-relative path.
        -ffile-prefix-map=${CMAKE_SOURCE_DIR}/=
        -ffreestanding
        -mno-red-zone
        -nostdlib
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-stack-protector>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
        $<$<COMPILE_LANGUAGE:CXX>:-mcmodel=kernel>
        $<$<COMPILE_LANGUAGE:CXX>:-Wall>
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
        $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>)

target_link_options(kernel.elf PRIVATE
        -nostdlib
        -mcmodel=kernel
        -T ${DOOM_OS_LINKER_SCRIPT}
        -Wl,-z,max-page-size=0x1000)

# libgcc supplies the compiler helper routines (64-bit division and friends) that -nostdlib
# would otherwise leave undefined.
target_link_libraries(kernel.elf PRIVATE gcc)

# Without this, editing the linker script does not relink.
set_target_properties(kernel.elf PROPERTIES LINK_DEPENDS ${DOOM_OS_LINKER_SCRIPT})

# =================================================================================================
# Bootable image
# =================================================================================================

set(DOOM_OS_ISO_DIR ${CMAKE_BINARY_DIR}/iso)
set(DOOM_OS_ISO ${CMAKE_BINARY_DIR}/DoomOS.iso)

add_custom_command(
        OUTPUT ${DOOM_OS_ISO}
        # Also enforces that the multiboot2 header lands within the first 32 KiB of the image.
        COMMAND grub-file --is-x86-multiboot2 $<TARGET_FILE:kernel.elf>
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DOOM_OS_ISO_DIR}/boot/grub
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:kernel.elf> ${DOOM_OS_ISO_DIR}/boot/kernel.elf
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/config/grub.cfg
        ${DOOM_OS_ISO_DIR}/boot/grub/grub.cfg
        COMMAND grub-mkrescue -o ${DOOM_OS_ISO} ${DOOM_OS_ISO_DIR}
        DEPENDS kernel.elf ${CMAKE_SOURCE_DIR}/config/grub.cfg
        COMMENT "Building bootable ISO"
        VERBATIM)

add_custom_target(iso ALL DEPENDS ${DOOM_OS_ISO})

# =================================================================================================
# QEMU
# =================================================================================================

set(DOOM_OS_QEMU_MEMORY 8G CACHE STRING "Guest memory")
set(DOOM_OS_QEMU_SMP "8,sockets=1,cores=4,threads=2" CACHE STRING "Guest CPU topology")

set(DOOM_OS_QEMU_FLAGS
        -cdrom ${DOOM_OS_ISO}
        -m ${DOOM_OS_QEMU_MEMORY}
        -smp ${DOOM_OS_QEMU_SMP}
        -serial stdio
        -no-reboot
        -no-shutdown)

add_custom_target(run
        COMMAND qemu-system-x86_64 ${DOOM_OS_QEMU_FLAGS}
        USES_TERMINAL
        VERBATIM)
add_dependencies(run iso)

# Waits for a debugger on tcp::1234.
add_custom_target(debug
        COMMAND qemu-system-x86_64 ${DOOM_OS_QEMU_FLAGS} -s -S
        USES_TERMINAL
        VERBATIM)
add_dependencies(debug iso)

add_custom_target(qemu-log
        COMMAND qemu-system-x86_64 ${DOOM_OS_QEMU_FLAGS}
        -d int,cpu_reset,guest_errors
        -D ${CMAKE_BINARY_DIR}/qemu.log
        USES_TERMINAL
        VERBATIM)
add_dependencies(qemu-log iso)

# =================================================================================================
# Formatting
#
# .clang-format uses AlignFunctionDeclarations, which needs clang-format 20; older releases
# reject the file outright rather than degrading.
# =================================================================================================

find_program(DOOM_OS_CLANG_FORMAT clang-format)

if(DOOM_OS_CLANG_FORMAT)
    add_custom_target(format
            COMMAND ${DOOM_OS_CLANG_FORMAT} -i ${KERNEL_CXX_SOURCES} ${KERNEL_HEADERS}
            COMMENT "Formatting sources"
            VERBATIM)

    add_custom_target(format-check
            COMMAND ${DOOM_OS_CLANG_FORMAT} --dry-run --Werror
            ${KERNEL_CXX_SOURCES} ${KERNEL_HEADERS}
            COMMENT "Checking formatting"
            VERBATIM)
endif()
