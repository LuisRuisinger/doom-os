# =================================================================================================
# DoomOS application support
#
# An application enters the image as already-built objects, plus any libraries those objects need.
# The project does not know what language produced them: the contract is at link time. An app image
# must provide a C ABI main from either a direct object or a normally searched library archive, and
# it resolves OS services against libos plus whatever runtime libraries it brings. Everything a
# source-level path could have checked - kernel-private includes, freestanding flags - is
# unavailable once the answer is an object or archive, so the contract is stated rather than
# enforced. See doom_os_application() for it.
# =================================================================================================

function(doom_os_application application_name)
    set(_options)
    set(_one_value_args)
    set(_multi_value_args OBJECTS LIBRARIES DEPENDS)

    cmake_parse_arguments(APPLICATION
            "${_options}"
            "${_one_value_args}"
            "${_multi_value_args}"
            ${ARGN})

    if(APPLICATION_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
                "doom_os_application(${application_name}) got unexpected arguments: "
                "${APPLICATION_UNPARSED_ARGUMENTS}. Applications are linked as objects/libraries; "
                "there is no SOURCES form. Compile with whatever toolchain the application is written for, "
                "then pass objects in OBJECTS and archives/runtime libraries in LIBRARIES.")
    endif()

    if(NOT APPLICATION_OBJECTS AND NOT APPLICATION_LIBRARIES)
        message(FATAL_ERROR
                "doom_os_application(${application_name}) requires OBJECTS, LIBRARIES, or both")
    endif()

    get_property(_existing GLOBAL PROPERTY DOOM_OS_APPLICATION_TARGET)
    if(_existing)
        message(FATAL_ERROR
                "An application is already defined (${_existing}). A unikernel image holds one.")
    endif()

    # Archives are libraries. entry/start.cpp emits a strong unresolved main for app images, so a
    # library archive that defines main is extracted by the normal linker search. No C shim and no
    # --whole-archive are needed for the entry point.
    set(_application_objects)

    foreach(_object IN LISTS APPLICATION_OBJECTS)
        if(_object MATCHES "\\.a$")
            list(APPEND APPLICATION_LIBRARIES "${_object}")
        elseif(_object MATCHES "\\$<")
            list(APPEND _application_objects "${_object}")
        else()
            # A path from a custom command is an object to link, not a source to compile, and
            # CMake has to be told both that and that it does not exist yet.
            set_source_files_properties(${_object} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
            list(APPEND _application_objects "${_object}")
        endif()
    endforeach()

    set(APPLICATION_OBJECTS "${_application_objects}")

    set_property(GLOBAL PROPERTY DOOM_OS_APPLICATION_TARGET ${application_name})
    set_property(GLOBAL PROPERTY DOOM_OS_APPLICATION_OBJECTS "${APPLICATION_OBJECTS}")
    set_property(GLOBAL PROPERTY DOOM_OS_APPLICATION_LIBRARIES "${APPLICATION_LIBRARIES}")

    if(APPLICATION_DEPENDS)
        set_property(GLOBAL PROPERTY DOOM_OS_APPLICATION_DEPENDS "${APPLICATION_DEPENDS}")
    endif()
endfunction()
