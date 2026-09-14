cmake_minimum_required(VERSION 3.22)

# ==================================================================================================
# DoomOS
#
# cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/x86_64-elf.cmake
# cmake --build build
# ==================================================================================================

if(NOT CMAKE_TOOLCHAIN_FILE AND NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
            "Configure with -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/x86_64-elf.cmake")
endif()

project(DoomOS
        VERSION 0.1.0
        DESCRIPTION "x86_64 kernel"
        LANGUAGES C CXX ASM)

set(DOOM_OS_ROOT ${CMAKE_CURRENT_SOURCE_DIR} CACHE INTERNAL "DoomOS source root")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/DoomOSConfig.cmake)

# Copying compilation database to project root
# Probably a bad fix given we please with that certain IDEs
if(PROJECT_IS_TOP_LEVEL AND NOT DOOM_OS_ROOT STREQUAL CMAKE_BINARY_DIR)
    file(CREATE_LINK
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${DOOM_OS_ROOT}/compile_commands.json
            SYMBOLIC RESULT DOOM_OS_COMPILE_COMMANDS_LINK)

    if(NOT DOOM_OS_COMPILE_COMMANDS_LINK EQUAL 0)
        message(WARNING
                "Could not link compile_commands.json into the project root: "
                "${DOOM_OS_COMPILE_COMMANDS_LINK}")
    endif()
endif()

# ==================================================================================================
# Sources
# ==================================================================================================

# arch/ and platform/ are globbed by the selected ids rather than wholesale. Those settings have to
# decide which port and machine wiring are compiled, or they decide nothing: a sweep of either tree
# would take every implementation present and hand the linker duplicate entry points.

file(GLOB_RECURSE KERNEL_CXX_SOURCES CONFIGURE_DEPENDS
        ${DOOM_OS_ROOT}/kernel/*.cpp)

file(GLOB_RECURSE KERNEL_ASM_SOURCES CONFIGURE_DEPENDS
        ${DOOM_OS_ROOT}/kernel/*.S)

file(GLOB_RECURSE KERNEL_ARCH_CXX_SOURCES CONFIGURE_DEPENDS
        ${DOOM_OS_ROOT}/arch/${DOOM_OS_ARCHITECTURE_ID}/*.cpp)

file(GLOB_RECURSE KERNEL_ARCH_ASM_SOURCES CONFIGURE_DEPENDS
        ${DOOM_OS_ROOT}/arch/${DOOM_OS_ARCHITECTURE_ID}/*.S)

if(NOT KERNEL_ARCH_CXX_SOURCES AND NOT KERNEL_ARCH_ASM_SOURCES)
    message(FATAL_ERROR
            "No sources under arch/${DOOM_OS_ARCHITECTURE_ID}. "
            "DOOM_OS_ARCHITECTURE names a port that is not in the tree.")
endif()

set(DOOM_OS_PLATFORM_DIR ${DOOM_OS_ROOT}/platform/${DOOM_OS_PLATFORM_ID})

if(NOT EXISTS "${DOOM_OS_PLATFORM_DIR}")
    message(FATAL_ERROR
            "No platform directory at platform/${DOOM_OS_PLATFORM_ID}. "
            "DOOM_OS_PLATFORM names machine wiring that is not in the tree.")
endif()

file(GLOB_RECURSE KERNEL_PLATFORM_CXX_SOURCES CONFIGURE_DEPENDS
        ${DOOM_OS_PLATFORM_DIR}/*.cpp)

file(GLOB_RECURSE KERNEL_PLATFORM_ASM_SOURCES CONFIGURE_DEPENDS
        ${DOOM_OS_PLATFORM_DIR}/*.S)

list(APPEND KERNEL_CXX_SOURCES
        ${KERNEL_ARCH_CXX_SOURCES}
        ${KERNEL_PLATFORM_CXX_SOURCES})

list(APPEND KERNEL_ASM_SOURCES
        ${KERNEL_ARCH_ASM_SOURCES}
        ${KERNEL_PLATFORM_ASM_SOURCES})

file(GLOB_RECURSE KERNEL_HEADERS CONFIGURE_DEPENDS
        ${DOOM_OS_ROOT}/kernel/*.hpp
        ${DOOM_OS_ROOT}/arch/*.hpp
        ${DOOM_OS_ROOT}/platform/*.hpp
        ${DOOM_OS_ROOT}/include/*.hpp)

file(GLOB_RECURSE LIBOS_SOURCES CONFIGURE_DEPENDS
        ${DOOM_OS_ROOT}/entry/*.cpp
        ${DOOM_OS_ROOT}/entry/*.c
        ${DOOM_OS_ROOT}/posix/*.cpp
        ${DOOM_OS_ROOT}/posix/*.c)

# ==================================================================================================
# Dependencies
# ==================================================================================================

include(FetchContent)
set(BUILD_TESTING OFF)

# Freestanding via __STDC_HOSTED__
FetchContent_Declare(result
        GIT_REPOSITORY https://github.com/LuisRuisinger/result.git
        GIT_TAG 57046555b6db0adcb1c9ec1232919cab79397865)
FetchContent_MakeAvailable(result)

# Freestanding via __STDC_HOSTED__
FetchContent_Declare(reflect
        GIT_REPOSITORY https://github.com/LuisRuisinger/cpp-reflect.git
        GIT_TAG experimental)
FetchContent_MakeAvailable(reflect)

# dlmalloc 2.8.6, the kernel heap's free lists. A single unversioned file rather than a repository,
# so the hash is the pin: an unofficial mirror would be a weaker guarantee than this, not a
# stronger one. It carries no CMakeLists, so MakeAvailable populates it and nothing more - the
# object library is built below, against kernel/mm/dlmalloc_config.h.
FetchContent_Declare(dlmalloc
        URL https://gee.cs.oswego.edu/pub/misc/malloc.c
        URL_HASH SHA256=be7be56b40b4b31417e74e77687f9b96127ea6cae534cf236aee40a4c739508d
        DOWNLOAD_NO_EXTRACT TRUE)
FetchContent_MakeAvailable(dlmalloc)

# =================================================================================================
# External directories
#
# DOOM_OS_DRIVER_DIR and DOOM_OS_APPLICATION_DIR are user-specified directories that are
# outside of the project. These directories build against kernel / posix shim APIs.
# =================================================================================================

function(doom_os_resolve_directory out_var directory)
    if(IS_ABSOLUTE "${directory}")
        set(${out_var} "${directory}" PARENT_SCOPE)
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/${directory}/CMakeLists.txt")
        set(${out_var} "${CMAKE_SOURCE_DIR}/${directory}" PARENT_SCOPE)
    else()
        set(${out_var} "${DOOM_OS_ROOT}/${directory}" PARENT_SCOPE)
    endif()
endfunction()

# =================================================================================================
# Public driver API
# =================================================================================================

include(cmake/DoomOSDrivers.cmake)

add_library(doom_os_driver_api INTERFACE)

target_include_directories(doom_os_driver_api INTERFACE
        ${DOOM_OS_ROOT}/include)

target_link_libraries(doom_os_driver_api INTERFACE doom_os_config lsr::result)

set(DOOM_OS_DRIVER_DIR "" CACHE STRING "External DoomOS driver directory")

if(DOOM_OS_DRIVER_DIR)
    doom_os_resolve_directory(DOOM_OS_DRIVER_PATH "${DOOM_OS_DRIVER_DIR}")

    if(NOT EXISTS "${DOOM_OS_DRIVER_PATH}/CMakeLists.txt")
        message(FATAL_ERROR
                "DOOM_OS_DRIVER_DIR must point at a directory with a CMakeLists.txt "
                "(resolved to ${DOOM_OS_DRIVER_PATH})")
    endif()

    add_subdirectory("${DOOM_OS_DRIVER_PATH}" "${CMAKE_BINARY_DIR}/drivers")
endif()

get_property(DOOM_OS_DRIVER_OBJECTS GLOBAL PROPERTY DOOM_OS_DRIVER_OBJECTS)

# =================================================================================================
# Standard libraries
#
# There are none. Which C or C++ library an image carries is the integrator's decision, exactly as
# the application and its drivers are: libos implements the x86_64 Linux ABI those libraries sit
# on, and the objects handed to doom_os_application() bring whatever they were built against.
#
# The kernel itself links no library but libgcc, whose helpers the compiler emits calls to.
# =================================================================================================

# =================================================================================================
# Application
#
# The application is whatever defines main to overwrite weak linkage.
# cmake --preset default -DDOOM_OS_APPLICATION_DIR=examples/helloworld
# =================================================================================================

include(cmake/DoomOSApplication.cmake)
set(DOOM_OS_APPLICATION_DIR "" CACHE STRING "DoomOS application directory")

if(DOOM_OS_APPLICATION_DIR)
    doom_os_resolve_directory(DOOM_OS_APPLICATION_PATH "${DOOM_OS_APPLICATION_DIR}")

    if(NOT EXISTS "${DOOM_OS_APPLICATION_PATH}/CMakeLists.txt")
        message(FATAL_ERROR
                "DOOM_OS_APPLICATION_DIR must point at a directory with a CMakeLists.txt "
                "(resolved to ${DOOM_OS_APPLICATION_PATH})")
    endif()

    add_subdirectory("${DOOM_OS_APPLICATION_PATH}" "${CMAKE_BINARY_DIR}/application")
endif()

get_property(DOOM_OS_APPLICATION_OBJECTS GLOBAL PROPERTY DOOM_OS_APPLICATION_OBJECTS)
get_property(DOOM_OS_APPLICATION_DEPENDS GLOBAL PROPERTY DOOM_OS_APPLICATION_DEPENDS)
get_property(DOOM_OS_APPLICATION_LIBRARIES GLOBAL PROPERTY DOOM_OS_APPLICATION_LIBRARIES)
get_property(DOOM_OS_APPLICATION_TARGET GLOBAL PROPERTY DOOM_OS_APPLICATION_TARGET)
get_property(DOOM_OS_DRIVER_DEPENDS GLOBAL PROPERTY DOOM_OS_DRIVER_DEPENDS)
# =================================================================================================
# Linker script
#
# Preprocessed via shared addresses in config/layout.h.
# =================================================================================================

set(DOOM_OS_LINKER_SCRIPT_IN ${DOOM_OS_ROOT}/config/linker.ld.in)
set(DOOM_OS_LAYOUT_HEADER ${DOOM_OS_ROOT}/config/layout.h)
set(DOOM_OS_LINKER_SCRIPT ${CMAKE_BINARY_DIR}/linker.ld)

add_custom_command(
        OUTPUT ${DOOM_OS_LINKER_SCRIPT}
        COMMAND ${CMAKE_CXX_COMPILER}
        -E -P -x c
        -I${DOOM_OS_ROOT}
        ${DOOM_OS_LINKER_SCRIPT_IN}
        -o ${DOOM_OS_LINKER_SCRIPT}
        DEPENDS ${DOOM_OS_LINKER_SCRIPT_IN} ${DOOM_OS_LAYOUT_HEADER}
        COMMENT "Preprocessing linker script"
        VERBATIM)

add_custom_target(linker-script DEPENDS ${DOOM_OS_LINKER_SCRIPT})

# =================================================================================================
# Kernel
# =================================================================================================

# =================================================================================================
# dlmalloc
#
# Its own translation unit, because the configuration is expressed as macros read before the file
# is parsed and there is nowhere to put those but ahead of it. NO_WARNINGS is for the source, not
# for the flags: the ABI options are the same ones the kernel is built with, and have to be.
#
# The image exports no malloc. ONLY_MSPACES in the config header is what guarantees that, and the
# post-build check on kernel.elf is what notices if it ever stops being true.
# =================================================================================================

add_library(doom_os_dlmalloc OBJECT ${dlmalloc_SOURCE_DIR}/malloc.c)

# For kernel/runtime/memory.h, which the config header reaches for. The C spelling exists so that
# this translation unit and memory.cpp declare memset and memcpy once between them.
target_include_directories(doom_os_dlmalloc PRIVATE ${DOOM_OS_ROOT})

target_compile_options(doom_os_dlmalloc PRIVATE
        -include ${DOOM_OS_ROOT}/kernel/mm/dlmalloc_config.h)

doom_os_configure_freestanding_target(doom_os_dlmalloc NO_WARNINGS)

# =================================================================================================
# libos
#
# Library layer to link application code against. The shape is deliberately small:
#   entry/ owns the app lifecycle,
#   posix/ owns ABI declarations, errno storage, weak POSIX fallbacks, and libc/runtime glue,
#   kernel/*/posix/ owns strong subsystem-specific POSIX adapters.
# =================================================================================================

add_library(doom_os_libos OBJECT ${LIBOS_SOURCES})

target_include_directories(doom_os_libos PRIVATE
        ${DOOM_OS_ROOT}
        ${DOOM_OS_ROOT}/include)
target_link_libraries(doom_os_libos PRIVATE
        doom_os_config
        lsr::result
        lsr::reflect)

doom_os_configure_freestanding_target(doom_os_libos)

if(DOOM_OS_APPLICATION_TARGET)
    target_compile_definitions(doom_os_libos PRIVATE DOOM_OS_HAS_APPLICATION=1)
endif()

add_executable(kernel.elf
        ${KERNEL_CXX_SOURCES}
        ${KERNEL_ASM_SOURCES}
        ${DOOM_OS_DRIVER_OBJECTS}
        ${DOOM_OS_APPLICATION_OBJECTS}
        $<TARGET_OBJECTS:doom_os_dlmalloc>
        $<TARGET_OBJECTS:doom_os_libos>)

add_dependencies(kernel.elf linker-script)

# Objects produced by a custom command rather than a CMake target carry no dependency of their own,
# so an application or driver that builds itself out-of-band names what has to run first.
if(DOOM_OS_APPLICATION_DEPENDS OR DOOM_OS_DRIVER_DEPENDS)
    add_dependencies(kernel.elf ${DOOM_OS_APPLICATION_DEPENDS} ${DOOM_OS_DRIVER_DEPENDS})
endif()

target_include_directories(kernel.elf PRIVATE
        ${DOOM_OS_ROOT}
        ${DOOM_OS_ROOT}/include)

# Hacky fix, given result and reflex default to the same namespace and their freestanding dependencies violate ODR
target_compile_definitions(kernel.elf PRIVATE REFLECT_NAMESPACE=reflect)

target_link_libraries(kernel.elf PRIVATE doom_os_config lsr::result lsr::reflect)

target_compile_options(kernel.elf PRIVATE
        -ffile-prefix-map=${DOOM_OS_ROOT}/=
        -ffreestanding
        -mno-red-zone

        # currently forcing the kernel to not omit SSE instructions
        -mno-sse
        -mno-sse2
        -mno-mmx
        -mno-80387

        # freestanding force
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

        # TODO
        # look into max-page-size
        -Wl,-z,max-page-size=0x1000)

if(DOOM_OS_APPLICATION_LIBRARIES)
    target_link_libraries(kernel.elf PRIVATE ${DOOM_OS_APPLICATION_LIBRARIES})
endif()

target_link_libraries(kernel.elf PRIVATE gcc)

# Relink linker script if changes are applied
set_target_properties(kernel.elf PROPERTIES LINK_DEPENDS ${DOOM_OS_LINKER_SCRIPT})

# Only meaningful with no application linked, where everything in the image is the kernel's.
if(NOT DOOM_OS_APPLICATION_DIR)
    add_custom_command(TARGET kernel.elf POST_BUILD
            COMMAND ${CMAKE_COMMAND}
            -DNM=${CMAKE_NM}
            -DKERNEL_ELF=$<TARGET_FILE:kernel.elf>
            -P ${DOOM_OS_ROOT}/cmake/CheckKernelRuntime.cmake
            COMMENT "Checking the kernel needs no C++ runtime"
            VERBATIM)
endif()

# =================================================================================================
# Bootable image
# =================================================================================================

set(DOOM_OS_ISO_DIR ${CMAKE_BINARY_DIR}/iso)
set(DOOM_OS_ISO ${CMAKE_BINARY_DIR}/DoomOS.iso)

add_custom_command(
        OUTPUT ${DOOM_OS_ISO}

        # Forces the multiboot2 header to land within the first 32KiB of the image
        COMMAND grub-file --is-x86-multiboot2 $<TARGET_FILE:kernel.elf>
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DOOM_OS_ISO_DIR}/boot/grub
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:kernel.elf> ${DOOM_OS_ISO_DIR}/boot/kernel.elf
        COMMAND ${CMAKE_COMMAND} -E copy ${DOOM_OS_ROOT}/config/grub.cfg
        ${DOOM_OS_ISO_DIR}/boot/grub/grub.cfg
        COMMAND grub-mkrescue -o ${DOOM_OS_ISO} ${DOOM_OS_ISO_DIR}
        DEPENDS kernel.elf ${DOOM_OS_ROOT}/config/grub.cfg
        COMMENT "Building bootable ISO"
        VERBATIM)

add_custom_target(iso ALL DEPENDS ${DOOM_OS_ISO})

# =================================================================================================
# Developer targets
# =================================================================================================

if(PROJECT_IS_TOP_LEVEL)
    set(DOOM_OS_QEMU_MEMORY 8G CACHE STRING "Guest memory")
    set(DOOM_OS_QEMU_SMP "8,sockets=1,cores=4,threads=2" CACHE STRING "Guest CPU topology")
    set(DOOM_OS_QEMU_DISPLAY "none" CACHE STRING "QEMU display backend (none, gtk, sdl)")
    set(DOOM_OS_QEMU_FLAGS
            -cdrom ${DOOM_OS_ISO}
            -m ${DOOM_OS_QEMU_MEMORY}
            -smp ${DOOM_OS_QEMU_SMP}
            -display ${DOOM_OS_QEMU_DISPLAY}
            -serial mon:stdio
            -no-reboot
            -no-shutdown)

    # QEMU emulation
    add_custom_target(run
            COMMAND qemu-system-x86_64 ${DOOM_OS_QEMU_FLAGS}
            USES_TERMINAL
            VERBATIM)
    add_dependencies(run iso)

    # GDB stub on tcp::1234
    add_custom_target(debug
            COMMAND qemu-system-x86_64 ${DOOM_OS_QEMU_FLAGS} -s -S
            USES_TERMINAL
            VERBATIM)
    add_dependencies(debug iso)
endif()
