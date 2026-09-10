# =================================================================================================
# DoomOS build configuration
# =================================================================================================

include_guard(GLOBAL)

function(doom_os_normalize_architecture out_var architecture)
    string(TOLOWER "${architecture}" _architecture_id)

    if(_architecture_id STREQUAL "x86-64")
        set(_architecture_id "x86_64")
    endif()

    set(${out_var} "${_architecture_id}" PARENT_SCOPE)
endfunction()

function(doom_os_normalize_board out_var board)
    string(TOLOWER "${board}" _board_id)
    string(REPLACE "-" "_" _board_id "${_board_id}")
    set(${out_var} "${_board_id}" PARENT_SCOPE)
endfunction()

function(doom_os_normalize_platform out_var platform)
    string(TOLOWER "${platform}" _platform_id)
    string(REPLACE "-" "_" _platform_id "${_platform_id}")
    set(${out_var} "${_platform_id}" PARENT_SCOPE)
endfunction()

set(DOOM_OS_BOARD "qemu_x86_64" CACHE STRING "Target board/platform")
set_property(CACHE DOOM_OS_BOARD PROPERTY STRINGS qemu_x86_64 qemu-x86_64)

set(DOOM_OS_PLATFORM "pc_multiboot2" CACHE STRING "Target platform wiring")
set_property(CACHE DOOM_OS_PLATFORM PROPERTY STRINGS pc_multiboot2 pc-multiboot2)

set(DOOM_OS_ARCHITECTURE "x86_64" CACHE STRING "Target architecture")
set_property(CACHE DOOM_OS_ARCHITECTURE PROPERTY STRINGS x86_64 x86-64)

option(DOOM_OS_HAS_MMU "Target provides a hardware MMU" ON)
option(DOOM_OS_HAS_FPU "Target provides an FPU/SIMD state" ON)

doom_os_normalize_board(DOOM_OS_BOARD_ID "${DOOM_OS_BOARD}")
doom_os_normalize_platform(DOOM_OS_PLATFORM_ID "${DOOM_OS_PLATFORM}")
doom_os_normalize_architecture(DOOM_OS_ARCHITECTURE_ID "${DOOM_OS_ARCHITECTURE}")

if(NOT DOOM_OS_BOARD_ID STREQUAL "qemu_x86_64")
    message(FATAL_ERROR
            "Unsupported DOOM_OS_BOARD='${DOOM_OS_BOARD}'. "
            "The only implemented board is qemu_x86_64.")
endif()

if(NOT DOOM_OS_PLATFORM_ID STREQUAL "pc_multiboot2")
    message(FATAL_ERROR
            "Unsupported DOOM_OS_PLATFORM='${DOOM_OS_PLATFORM}'. "
            "The only implemented platform is pc_multiboot2.")
endif()

# No whitelist of implemented architectures. arch/<id> is globbed by this id, so the tree is the
# list: naming a port that is not there fails with "No sources under arch/<id>", which needs no
# editing when one is added. The toolchain check below still applies, and is the one that catches
# the common mistake of a setting that disagrees with the compiler.

if(CMAKE_SYSTEM_PROCESSOR)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" DOOM_OS_TOOLCHAIN_PROCESSOR_ID)

    if(DOOM_OS_TOOLCHAIN_PROCESSOR_ID STREQUAL "x86-64")
        set(DOOM_OS_TOOLCHAIN_PROCESSOR_ID "x86_64")
    endif()

    if(NOT DOOM_OS_TOOLCHAIN_PROCESSOR_ID STREQUAL DOOM_OS_ARCHITECTURE_ID)
        message(FATAL_ERROR
                "DOOM_OS_ARCHITECTURE='${DOOM_OS_ARCHITECTURE}' does not match "
                "the toolchain processor '${CMAKE_SYSTEM_PROCESSOR}'.")
    endif()
endif()

if(NOT DOOM_OS_HAS_MMU)
    message(FATAL_ERROR
            "Unsupported DOOM_OS_HAS_MMU=OFF. The current kernel requires an x86_64 MMU.")
endif()

if(NOT DOOM_OS_HAS_FPU)
    message(FATAL_ERROR
            "Unsupported DOOM_OS_HAS_FPU=OFF. The current x86_64 core wiring enables the FPU.")
endif()

# No compile definitions. The board, architecture, MMU and FPU settings decided which sources are
# compiled and which components the platform wiring lists; nothing in the tree branches on them at
# preprocessing time, and a macro no source reads is a claim the build cannot check.

add_library(doom_os_config INTERFACE)

message(STATUS
        "DoomOS config: board=${DOOM_OS_BOARD_ID}, platform=${DOOM_OS_PLATFORM_ID}, "
        "architecture=${DOOM_OS_ARCHITECTURE_ID}, mmu=${DOOM_OS_HAS_MMU}, "
        "fpu=${DOOM_OS_HAS_FPU}")
