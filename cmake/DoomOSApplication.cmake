# =================================================================================================
# DoomOS application support
# =================================================================================================

function(doom_os_reject_internal_application_include_dir include_dir)
    get_filename_component(_application_include_dir "${include_dir}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(_kernel_dir "${DOOM_OS_ROOT}/kernel" ABSOLUTE)
    get_filename_component(_libos_dir "${DOOM_OS_ROOT}/libos" ABSOLUTE)

    string(FIND "${_application_include_dir}/" "${_kernel_dir}/" _kernel_dir_pos)
    string(FIND "${_application_include_dir}/" "${_libos_dir}/" _libos_dir_pos)

    if(_kernel_dir_pos EQUAL 0 OR _libos_dir_pos EQUAL 0)
        message(FATAL_ERROR
                "Application include directory '${include_dir}' points at kernel internals. "
                "An application reaches the kernel through the C library, not directly.")
    endif()
endfunction()

function(doom_os_check_application_source source_file)
    get_filename_component(_source_file "${source_file}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${_source_file}")
        message(FATAL_ERROR "Application source '${source_file}' does not exist")
    endif()

    file(READ "${_source_file}" _application_source)
    if(_application_source MATCHES
       "#[ \t]*include[ \t]*[<\"](kernel/(arch|boot|core|debug|runtime|sync)|libos)/")
        message(FATAL_ERROR
                "Application source '${source_file}' includes a kernel-private header. "
                "An application reaches the kernel through the C library, not directly.")
    endif()
endfunction()

function(doom_os_application application_name)
    set(_options)
    set(_one_value_args STDLIB)
    set(_multi_value_args SOURCES INCLUDE_DIRECTORIES COMPILE_DEFINITIONS)

    cmake_parse_arguments(APPLICATION
            "${_options}"
            "${_one_value_args}"
            "${_multi_value_args}"
            ${ARGN})

    if(NOT APPLICATION_SOURCES)
        message(FATAL_ERROR "doom_os_application(${application_name}) requires SOURCES")
    endif()

    if(NOT APPLICATION_STDLIB)
        message(FATAL_ERROR
                "doom_os_application(${application_name}) requires STDLIB. "
                "Use STDLIB NONE, NEWLIB or NEWLIB_CXX.")
    endif()

    doom_os_normalize_stdlib(_stdlib "${APPLICATION_STDLIB}")

    get_property(_existing GLOBAL PROPERTY DOOM_OS_APPLICATION_TARGET)
    if(_existing)
        message(FATAL_ERROR
                "An application is already defined (${_existing}). A unikernel image holds one.")
    endif()

    set(_target "doom_os_application_${application_name}")

    add_library(${_target} OBJECT ${APPLICATION_SOURCES})
    target_compile_definitions(${_target} PRIVATE ${APPLICATION_COMPILE_DEFINITIONS})
    target_compile_definitions(${_target} PRIVATE DOOM_OS_APPLICATION_STDLIB_${_stdlib}=1)
    target_include_directories(${_target} PRIVATE ${DOOM_OS_ROOT}/include)
    target_link_libraries(${_target} PRIVATE doom_os_config)

    doom_os_configure_freestanding_target(${_target} ALLOW_SSE)

    foreach(_source IN LISTS APPLICATION_SOURCES)
        doom_os_check_application_source("${_source}")
    endforeach()

    foreach(_include_dir IN LISTS APPLICATION_INCLUDE_DIRECTORIES)
        doom_os_reject_internal_application_include_dir("${_include_dir}")
    endforeach()

    if(APPLICATION_INCLUDE_DIRECTORIES)
        target_include_directories(${_target} PRIVATE ${APPLICATION_INCLUDE_DIRECTORIES})
    endif()

    set_property(GLOBAL PROPERTY DOOM_OS_APPLICATION_TARGET ${_target})
    set_property(GLOBAL PROPERTY DOOM_OS_APPLICATION_STDLIB ${_stdlib})

    if(TARGET kernel.elf)
        target_sources(kernel.elf PRIVATE $<TARGET_OBJECTS:${_target}>)
    else()
        set_property(GLOBAL PROPERTY DOOM_OS_APPLICATION_OBJECTS "$<TARGET_OBJECTS:${_target}>")
    endif()
endfunction()
