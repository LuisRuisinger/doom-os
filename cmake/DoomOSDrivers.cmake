# =================================================================================================
# DoomOS external driver support
#
# Drivers enter the image the same way an application does: as object files. They resolve against
# <uk/...>, which include/ provides in both a C++ and a C spelling, so a driver can be
# written in anything with a C FFI. The project neither compiles them nor reads them.
# =================================================================================================

# NO_WARNINGS exists for third-party sources, which will not survive -Wpedantic and are not ours to
# fix. It suppresses the warning flags and nothing else: the ABI flags below decide how the object
# is callable - red zone, SSE, code model - and an object that disagrees with the kernel on any of
# them links silently and corrupts at run time. Keeping both in one function is what stops the two
# from drifting apart.
function(doom_os_configure_freestanding_target target_name)
    cmake_parse_arguments(FREESTANDING "ALLOW_SSE;NO_WARNINGS" "" "" ${ARGN})

    target_compile_definitions(${target_name} PRIVATE REFLECT_NAMESPACE=reflect)
    if(NOT FREESTANDING_ALLOW_SSE)
        target_compile_options(${target_name} PRIVATE
                -mno-sse
                -mno-sse2
                -mno-mmx
                -mno-80387)
    endif()

    if(NOT FREESTANDING_NO_WARNINGS)
        target_compile_options(${target_name} PRIVATE
                -Wall
                -Wextra
                -Wpedantic)
    endif()

    target_compile_options(${target_name} PRIVATE
            -ffile-prefix-map=${DOOM_OS_ROOT}/=
            -ffreestanding
            -mno-red-zone
            -nostdlib
            -mcmodel=kernel
            -fno-stack-protector
            $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
            $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
            $<$<COMPILE_LANGUAGE:CXX>:-fno-use-cxa-atexit>
            $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>)
endfunction()

function(doom_os_driver driver_name)
    set(_options)
    set(_one_value_args)
    set(_multi_value_args OBJECTS DEPENDS)

    cmake_parse_arguments(DRIVER "${_options}" "${_one_value_args}" "${_multi_value_args}"
                          ${ARGN})

    if(DRIVER_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
                "doom_os_driver(${driver_name}) got unexpected arguments: "
                "${DRIVER_UNPARSED_ARGUMENTS}. Drivers are linked as objects; there is no SOURCES "
                "form. Compile against <uk/...> and pass the result in OBJECTS.")
    endif()

    if(NOT DRIVER_OBJECTS)
        message(FATAL_ERROR "doom_os_driver(${driver_name}) requires OBJECTS")
    endif()

    set_property(GLOBAL APPEND PROPERTY DOOM_OS_DRIVER_OBJECTS "${DRIVER_OBJECTS}")

    if(DRIVER_DEPENDS)
        set_property(GLOBAL APPEND PROPERTY DOOM_OS_DRIVER_DEPENDS "${DRIVER_DEPENDS}")
    endif()
endfunction()
