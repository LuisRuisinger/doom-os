cmake_minimum_required(VERSION 3.22)

# ==================================================================================================
# DoomOS
#
# cmake -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-x86_64-elf.cmake
# cmake --build build
# ==================================================================================================

if(NOT CMAKE_TOOLCHAIN_FILE AND NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR
            "Configure with -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-x86_64-elf.cmake")
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

# kernel/arch is globbed per architecture rather than wholesale. The architecture setting has to
# decide which port is compiled or it decides nothing: a sweep of kernel/ would take every arch
# directory present and hand the linker two definitions of the same entry point.

file(GLOB_RECURSE KERNEL_CXX_SOURCES CONFIGURE_DEPENDS
        ${DOOM_OS_ROOT}/kernel/*.cpp)

file(GLOB_RECURSE KERNEL_ASM_SOURCES CONFIGURE_DEPENDS
        ${DOOM_OS_ROOT}/kernel/*.S)

file(GLOB_RECURSE KERNEL_FOREIGN_ARCH_SOURCES CONFIGURE_DEPENDS
        ${DOOM_OS_ROOT}/kernel/arch/*.cpp
        ${DOOM_OS_ROOT}/kernel/arch/*.S)

file(GLOB_RECURSE KERNEL_ARCH_SOURCES CONFIGURE_DEPENDS
        ${DOOM_OS_ROOT}/kernel/arch/${DOOM_OS_ARCHITECTURE_ID}/*.cpp
        ${DOOM_OS_ROOT}/kernel/arch/${DOOM_OS_ARCHITECTURE_ID}/*.S)

if(NOT KERNEL_ARCH_SOURCES)
    message(FATAL_ERROR
            "No sources under kernel/arch/${DOOM_OS_ARCHITECTURE_ID}. "
            "DOOM_OS_ARCHITECTURE names a port that is not in the tree.")
endif()

foreach(_foreign IN LISTS KERNEL_FOREIGN_ARCH_SOURCES)
    if(NOT _foreign IN_LIST KERNEL_ARCH_SOURCES)
        list(REMOVE_ITEM KERNEL_CXX_SOURCES ${_foreign})
        list(REMOVE_ITEM KERNEL_ASM_SOURCES ${_foreign})
    endif()
endforeach()

file(GLOB_RECURSE KERNEL_HEADERS CONFIGURE_DEPENDS
        ${DOOM_OS_ROOT}/kernel/*.hpp
        ${DOOM_OS_ROOT}/include/*.hpp)

file(GLOB_RECURSE LIBOS_SOURCES CONFIGURE_DEPENDS
        ${DOOM_OS_ROOT}/libos/*.cpp
        ${DOOM_OS_ROOT}/libos/*.c)

# ==================================================================================================
# Dependencies
# ==================================================================================================

include(FetchContent)
set(BUILD_TESTING OFF)

# Freestanding via __STDC_HOSTED__
FetchContent_Declare(result
        GIT_REPOSITORY https://github.com/LuisRuisinger/result.git
        GIT_TAG c5f66e342e3b40a8b86733a5d5cc689c663645f7)
FetchContent_MakeAvailable(result)

# Freestanding via __STDC_HOSTED__
FetchContent_Declare(reflect
        GIT_REPOSITORY https://github.com/LuisRuisinger/cpp-reflect.git
        GIT_TAG experimental)
FetchContent_MakeAvailable(reflect)

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
# Kernel code stays freestanding. Applications must declare their standard-library layer, and
# drivers may declare one when they deliberately depend on it. The final image links only the
# layers selected by those target declarations.
#
# Asking the compiler beats probing for a path: -print-file-name answers with the absolute path
# when it can find the archive and echoes the bare name back when it cannot.
# =================================================================================================

execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libc.a
        OUTPUT_VARIABLE DOOM_OS_LIBC_ARCHIVE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)

if(IS_ABSOLUTE "${DOOM_OS_LIBC_ARCHIVE}" AND EXISTS "${DOOM_OS_LIBC_ARCHIVE}")
    set(DOOM_OS_HAVE_LIBC TRUE)
    message(STATUS "C library: ${DOOM_OS_LIBC_ARCHIVE}")
else()
    set(DOOM_OS_HAVE_LIBC FALSE)
    message(STATUS "C library: none")
endif()

execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libstdc++.a
        OUTPUT_VARIABLE DOOM_OS_LIBSTDCXX_ARCHIVE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)

if(IS_ABSOLUTE "${DOOM_OS_LIBSTDCXX_ARCHIVE}" AND EXISTS "${DOOM_OS_LIBSTDCXX_ARCHIVE}")
    set(DOOM_OS_HAVE_LIBSTDCXX TRUE)
    message(STATUS "C++ library: ${DOOM_OS_LIBSTDCXX_ARCHIVE}")
else()
    set(DOOM_OS_HAVE_LIBSTDCXX FALSE)
    message(STATUS "C++ library: none")
endif()

execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libsupc++.a
        OUTPUT_VARIABLE DOOM_OS_LIBSUPCXX_ARCHIVE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)

if(IS_ABSOLUTE "${DOOM_OS_LIBSUPCXX_ARCHIVE}" AND EXISTS "${DOOM_OS_LIBSUPCXX_ARCHIVE}")
    set(DOOM_OS_HAVE_LIBSUPCXX TRUE)
    message(STATUS "C++ support library: ${DOOM_OS_LIBSUPCXX_ARCHIVE}")
else()
    set(DOOM_OS_HAVE_LIBSUPCXX FALSE)
    message(STATUS "C++ support library: none")
endif()

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
get_property(DOOM_OS_DRIVER_DEPENDS GLOBAL PROPERTY DOOM_OS_DRIVER_DEPENDS)
get_property(DOOM_OS_APPLICATION_STDLIB GLOBAL PROPERTY DOOM_OS_APPLICATION_STDLIB)
get_property(DOOM_OS_DRIVER_STDLIBS GLOBAL PROPERTY DOOM_OS_DRIVER_STDLIBS)

set(DOOM_OS_SELECTED_STDLIBS ${DOOM_OS_APPLICATION_STDLIB} ${DOOM_OS_DRIVER_STDLIBS})

set(DOOM_OS_LINK_NEWLIB FALSE)
set(DOOM_OS_LINK_LIBSTDCXX FALSE)

foreach(_stdlib IN LISTS DOOM_OS_SELECTED_STDLIBS)
    if(NOT _stdlib OR _stdlib STREQUAL "NONE")
    elseif(_stdlib STREQUAL "NEWLIB")
        set(DOOM_OS_LINK_NEWLIB TRUE)
    elseif(_stdlib STREQUAL "NEWLIB_CXX")
        set(DOOM_OS_LINK_NEWLIB TRUE)
        set(DOOM_OS_LINK_LIBSTDCXX TRUE)
    else()
        message(FATAL_ERROR
                "Unsupported stdlib '${_stdlib}'. Expected NONE, NEWLIB or NEWLIB_CXX.")
    endif()
endforeach()

if(NOT DOOM_OS_SELECTED_STDLIBS)
    set(DOOM_OS_SELECTED_STDLIBS NONE)
endif()

list(REMOVE_DUPLICATES DOOM_OS_SELECTED_STDLIBS)
message(STATUS "Selected stdlibs: ${DOOM_OS_SELECTED_STDLIBS}")

if(DOOM_OS_LINK_NEWLIB AND NOT DOOM_OS_HAVE_LIBC)
    message(FATAL_ERROR
            "A selected target requires newlib, but the cross toolchain has no C library.\n"
            "newlib is built by the Dockerfile's newlib stage - rebuild the container image, "
            "then configure again.")
endif()

if(DOOM_OS_LINK_LIBSTDCXX AND NOT DOOM_OS_HAVE_LIBSTDCXX)
    message(FATAL_ERROR
            "A selected target requires libstdc++, but the cross toolchain has no "
            "C++ standard library.")
endif()

if(DOOM_OS_LINK_LIBSTDCXX AND NOT DOOM_OS_HAVE_LIBSUPCXX)
    message(FATAL_ERROR
            "A selected target requires libstdc++, but the cross toolchain has no "
            "C++ support library.")
endif()

if(NOT DOOM_OS_LINK_NEWLIB)
    # The POSIX shim exists to be called by newlib and includes newlib headers. Without newlib it is
    # not merely unused, it cannot compile against a --without-headers toolchain.
    list(REMOVE_ITEM LIBOS_SOURCES ${DOOM_OS_ROOT}/libos/posix.cpp)
endif()

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
# libos
#
# Library layer to link application code against
# =================================================================================================

add_library(doom_os_libos OBJECT ${LIBOS_SOURCES})

target_include_directories(doom_os_libos PRIVATE ${DOOM_OS_ROOT})
target_link_libraries(doom_os_libos PRIVATE
        doom_os_config
        lsr::result
        lsr::reflect)

doom_os_configure_freestanding_target(doom_os_libos)

add_executable(kernel.elf
        ${KERNEL_CXX_SOURCES}
        ${KERNEL_ASM_SOURCES}
        ${DOOM_OS_DRIVER_OBJECTS}
        ${DOOM_OS_APPLICATION_OBJECTS}
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

if(DOOM_OS_LINK_LIBSTDCXX)
    target_link_libraries(kernel.elf PRIVATE stdc++)
    target_link_libraries(kernel.elf PRIVATE supc++)
endif()

if(DOOM_OS_LINK_NEWLIB)
    target_link_libraries(kernel.elf PRIVATE c)
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
