# =================================================================================================
# DoomOS application support
#
# An application enters the image as object files and nothing else. The project does not compile
# it, does not know what language it was written in, and does not read it: the only contract is at
# link time, where the objects must define main with C linkage and resolve against what the image
# already provides. Everything a source-level path could have checked - kernel-private includes,
# freestanding flags - is unavailable once the answer is an object, so the contract is stated
# rather than enforced. See doom_os_application() for it.
# =================================================================================================

function(doom_os_application application_name)
    set(_options)
    set(_one_value_args)
    set(_multi_value_args OBJECTS DEPENDS)

    cmake_parse_arguments(APPLICATION
            "${_options}"
            "${_one_value_args}"
            "${_multi_value_args}"
            ${ARGN})

    if(APPLICATION_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
                "doom_os_application(${application_name}) got unexpected arguments: "
                "${APPLICATION_UNPARSED_ARGUMENTS}. Applications are linked as objects; there is "
                "no SOURCES form. Compile with whatever toolchain the application is written for "
                "and pass the result in OBJECTS.")
    endif()

    if(NOT APPLICATION_OBJECTS)
        message(FATAL_ERROR "doom_os_application(${application_name}) requires OBJECTS")
    endif()

    get_property(_existing GLOBAL PROPERTY DOOM_OS_APPLICATION_TARGET)
    if(_existing)
        message(FATAL_ERROR
                "An application is already defined (${_existing}). A unikernel image holds one.")
    endif()

    # A path handed in from a custom command is an object to link, not a source to compile, and
    # CMake has to be told both that and that it does not exist yet. Generator expressions are
    # left alone: $<TARGET_OBJECTS:...> already carries the same meaning.
    foreach(_object IN LISTS APPLICATION_OBJECTS)
        if(NOT _object MATCHES "\\$<")
            set_source_files_properties(${_object} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
        endif()
    endforeach()

    set_property(GLOBAL PROPERTY DOOM_OS_APPLICATION_TARGET ${application_name})
    set_property(GLOBAL PROPERTY DOOM_OS_APPLICATION_OBJECTS "${APPLICATION_OBJECTS}")

    if(APPLICATION_DEPENDS)
        set_property(GLOBAL PROPERTY DOOM_OS_APPLICATION_DEPENDS "${APPLICATION_DEPENDS}")
    endif()
endfunction()
