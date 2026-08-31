# =================================================================================================
# DoomOS external driver support
# =================================================================================================

function(doom_os_reject_internal_driver_include_dir include_dir)
    get_filename_component(_driver_include_dir "${include_dir}" ABSOLUTE
                           BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(_kernel_dir "${DOOM_OS_ROOT}/kernel" ABSOLUTE)
    get_filename_component(_libos_dir "${DOOM_OS_ROOT}/libos" ABSOLUTE)

    string(FIND "${_driver_include_dir}/" "${_kernel_dir}/" _kernel_dir_pos)
    string(FIND "${_driver_include_dir}/" "${_libos_dir}/" _libos_dir_pos)

    if(_kernel_dir_pos EQUAL 0 OR _libos_dir_pos EQUAL 0)
        message(FATAL_ERROR
                "Driver include directory '${include_dir}' points at kernel internals. "
                "Use <kernel/driver/...>, which include/ provides.")
    endif()
endfunction()

function(doom_os_check_driver_source source_file)
    get_filename_component(_source_file "${source_file}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${_source_file}")
        message(FATAL_ERROR "Driver source '${source_file}' does not exist")
    endif()

    file(READ "${_source_file}" _driver_source)
    if(_driver_source MATCHES
       "#[ \t]*include[ \t]*[<\"]kernel/(arch|boot|core|debug|runtime|sync)/")
        message(FATAL_ERROR
                "Driver source '${source_file}' includes a kernel-private header. "
                "External drivers must include <kernel/driver/...> and use driver services.")
    endif()

    if(_driver_source MATCHES
       "#[ \t]*include[ \t]*[<\"]([^>\"]*/)?kernel/driver/lifecycle\\.hpp[>\"]")
        message(FATAL_ERROR
                "Driver source '${source_file}' includes kernel/driver/lifecycle.hpp, "
                "which is a kernel-private lifecycle hook.")
    endif()
endfunction()

function(doom_os_configure_freestanding_target target_name)
    cmake_parse_arguments(FREESTANDING "ALLOW_SSE" "" "" ${ARGN})

    target_compile_definitions(${target_name} PRIVATE REFLECT_NAMESPACE=reflect)
    if(NOT FREESTANDING_ALLOW_SSE)
        target_compile_options(${target_name} PRIVATE
                -mno-sse
                -mno-sse2
                -mno-mmx
                -mno-80387)
    endif()

    target_compile_options(${target_name} PRIVATE
            -ffile-prefix-map=${DOOM_OS_ROOT}/=
            -ffreestanding
            -mno-red-zone
            -nostdlib
            -mcmodel=kernel
            -fno-stack-protector
            -Wall
            -Wextra
            -Wpedantic
            $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
            $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
            $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>
            $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>)
endfunction()

function(doom_os_driver driver_name)
    set(_options ALLOW_SSE)
    set(_one_value_args STDLIB)
    set(_multi_value_args SOURCES INCLUDE_DIRECTORIES COMPILE_DEFINITIONS)

    cmake_parse_arguments(DRIVER "${_options}" "${_one_value_args}" "${_multi_value_args}"
                          ${ARGN})

    if(NOT DRIVER_SOURCES)
        message(FATAL_ERROR "doom_os_driver(${driver_name}) requires SOURCES")
    endif()

    if(DRIVER_STDLIB)
        doom_os_normalize_stdlib(_stdlib "${DRIVER_STDLIB}")
    else()
        set(_stdlib NONE)
    endif()

    if(NOT TARGET doom_os_driver_api)
        message(FATAL_ERROR "doom_os_driver_api is not available")
    endif()

    set(_target "doom_os_driver_${driver_name}")

    add_library(${_target} OBJECT ${DRIVER_SOURCES})
    target_link_libraries(${_target} PRIVATE doom_os_driver_api)
    target_compile_definitions(${_target} PRIVATE ${DRIVER_COMPILE_DEFINITIONS})
    target_compile_definitions(${_target} PRIVATE DOOM_OS_DRIVER_STDLIB_${_stdlib}=1)

    if(DRIVER_ALLOW_SSE)
        doom_os_configure_freestanding_target(${_target} ALLOW_SSE)
    else()
        doom_os_configure_freestanding_target(${_target})
    endif()

    foreach(_source IN LISTS DRIVER_SOURCES)
        doom_os_check_driver_source("${_source}")
    endforeach()

    foreach(_include_dir IN LISTS DRIVER_INCLUDE_DIRECTORIES)
        doom_os_reject_internal_driver_include_dir("${_include_dir}")
    endforeach()

    if(DRIVER_INCLUDE_DIRECTORIES)
        target_include_directories(${_target} PRIVATE ${DRIVER_INCLUDE_DIRECTORIES})
    endif()

    set_property(GLOBAL APPEND PROPERTY DOOM_OS_DRIVER_STDLIBS ${_stdlib})
    set_property(GLOBAL APPEND PROPERTY DOOM_OS_DRIVER_TARGETS ${_target})

    if(TARGET kernel.elf)
        target_sources(kernel.elf PRIVATE $<TARGET_OBJECTS:${_target}>)
    else()
        set_property(GLOBAL APPEND PROPERTY DOOM_OS_DRIVER_OBJECTS "$<TARGET_OBJECTS:${_target}>")
    endif()
endfunction()
