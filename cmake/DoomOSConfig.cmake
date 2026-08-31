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

function(doom_os_bool_to_on_off out_var value)
    string(TOUPPER "${value}" _value_id)

    if(_value_id MATCHES "^(1|ON|TRUE|YES)$")
        set(${out_var} ON PARENT_SCOPE)
    elseif(_value_id MATCHES "^(0|OFF|FALSE|NO)$")
        set(${out_var} OFF PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Expected a boolean value, got '${value}'")
    endif()
endfunction()

function(doom_os_normalize_stdlib out_var stdlib)
    string(TOUPPER "${stdlib}" _stdlib_id)
    string(REPLACE "-" "_" _stdlib_id "${_stdlib_id}")

    if(NOT _stdlib_id STREQUAL "NONE" AND
       NOT _stdlib_id STREQUAL "NEWLIB" AND
       NOT _stdlib_id STREQUAL "NEWLIB_CXX")
        message(FATAL_ERROR
                "Unsupported STDLIB '${stdlib}'. Supported values are NONE, NEWLIB and "
                "NEWLIB_CXX.")
    endif()

    set(${out_var} "${_stdlib_id}" PARENT_SCOPE)
endfunction()

set(DOOM_OS_BOARD "qemu_x86_64" CACHE STRING "Target board/platform")
set_property(CACHE DOOM_OS_BOARD PROPERTY STRINGS qemu_x86_64 qemu-x86_64)

set(DOOM_OS_ARCHITECTURE "x86_64" CACHE STRING "Target architecture")
set_property(CACHE DOOM_OS_ARCHITECTURE PROPERTY STRINGS x86_64 x86-64)

option(DOOM_OS_HAS_MMU "Target provides a hardware MMU" ON)
option(DOOM_OS_HAS_FPU "Target provides an FPU/SIMD state" ON)

doom_os_normalize_board(DOOM_OS_BOARD_ID "${DOOM_OS_BOARD}")
doom_os_normalize_architecture(DOOM_OS_ARCHITECTURE_ID "${DOOM_OS_ARCHITECTURE}")

if(NOT DOOM_OS_BOARD_ID STREQUAL "qemu_x86_64")
    message(FATAL_ERROR
            "Unsupported DOOM_OS_BOARD='${DOOM_OS_BOARD}'. "
            "The only implemented board is qemu_x86_64.")
endif()

if(NOT DOOM_OS_ARCHITECTURE_ID STREQUAL "x86_64")
    message(FATAL_ERROR
            "Unsupported DOOM_OS_ARCHITECTURE='${DOOM_OS_ARCHITECTURE}'. "
            "The only implemented architecture is x86_64.")
endif()

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

add_library(doom_os_config INTERFACE)

target_compile_definitions(doom_os_config INTERFACE
        DOOM_OS_BOARD_QEMU_X86_64=1
        DOOM_OS_ARCH_X86_64=1
        DOOM_OS_HAS_MMU=1
        DOOM_OS_HAS_FPU=1)

function(doom_os_require_config)
    set(_options)
    set(_one_value_args BOARD ARCHITECTURE MMU FPU)
    set(_multi_value_args)

    cmake_parse_arguments(REQUIRE "${_options}" "${_one_value_args}" "${_multi_value_args}"
                          ${ARGN})

    if(REQUIRE_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
                "doom_os_require_config() got unexpected arguments: "
                "${REQUIRE_UNPARSED_ARGUMENTS}")
    endif()

    if(DEFINED REQUIRE_BOARD)
        doom_os_normalize_board(_required_board "${REQUIRE_BOARD}")

        if(NOT _required_board STREQUAL DOOM_OS_BOARD_ID)
            message(FATAL_ERROR
                    "Project '${CMAKE_CURRENT_SOURCE_DIR}' requires "
                    "BOARD=${_required_board}, but DoomOS is configured with "
                    "BOARD=${DOOM_OS_BOARD_ID}.")
        endif()
    endif()

    if(DEFINED REQUIRE_ARCHITECTURE)
        doom_os_normalize_architecture(_required_architecture "${REQUIRE_ARCHITECTURE}")

        if(NOT _required_architecture STREQUAL DOOM_OS_ARCHITECTURE_ID)
            message(FATAL_ERROR
                    "Project '${CMAKE_CURRENT_SOURCE_DIR}' requires "
                    "ARCHITECTURE=${_required_architecture}, but DoomOS is configured with "
                    "ARCHITECTURE=${DOOM_OS_ARCHITECTURE_ID}.")
        endif()
    endif()

    if(DEFINED REQUIRE_MMU)
        doom_os_bool_to_on_off(_required_mmu "${REQUIRE_MMU}")
        doom_os_bool_to_on_off(_configured_mmu "${DOOM_OS_HAS_MMU}")

        if(NOT _required_mmu STREQUAL _configured_mmu)
            message(FATAL_ERROR
                    "Project '${CMAKE_CURRENT_SOURCE_DIR}' requires MMU=${_required_mmu}, "
                    "but DoomOS is configured with MMU=${_configured_mmu}.")
        endif()
    endif()

    if(DEFINED REQUIRE_FPU)
        doom_os_bool_to_on_off(_required_fpu "${REQUIRE_FPU}")
        doom_os_bool_to_on_off(_configured_fpu "${DOOM_OS_HAS_FPU}")

        if(NOT _required_fpu STREQUAL _configured_fpu)
            message(FATAL_ERROR
                    "Project '${CMAKE_CURRENT_SOURCE_DIR}' requires FPU=${_required_fpu}, "
                    "but DoomOS is configured with FPU=${_configured_fpu}.")
        endif()
    endif()
endfunction()

message(STATUS
        "DoomOS config: board=${DOOM_OS_BOARD_ID}, architecture=${DOOM_OS_ARCHITECTURE_ID}, "
        "mmu=${DOOM_OS_HAS_MMU}, fpu=${DOOM_OS_HAS_FPU}")
