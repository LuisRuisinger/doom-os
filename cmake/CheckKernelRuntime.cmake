# =================================================================================================
# Kernel C++ runtime check
#
# Run against an image built with no application, where every byte came from the kernel.
#
# Virtual functions and function-local statics are fine: kernel/runtime/cxx_abi.cpp supplies what
# they need, so they work whatever the application linked. Global operator new is still absent, and
# a `new` in kernel code is an undefined symbol at link time, which is the intended answer.
#
# A namespace-scope object needing runtime construction is the one case that fails silently, and it
# fails for an ordering reason rather than a missing symbol. There is one .init_array and one walk
# of it, and that walk is the last node of the boot graph so that an application constructor finds
# a heap and legal SSE behind it. A kernel global would therefore be constructed after every
# component that could use it - the wrong end of boot, with no diagnostic. Constant-initialise it
# instead (constinit says so at compile time), or put it behind an init() the graph calls.
# =================================================================================================

execute_process(
        COMMAND ${NM} ${KERNEL_ELF}
        OUTPUT_VARIABLE _symbols
        RESULT_VARIABLE _nm_result
        ERROR_QUIET)

if(NOT _nm_result EQUAL 0)
    message(FATAL_ERROR "Could not read symbols from ${KERNEL_ELF}")
endif()

# Format independent, and the one that actually catches things: GCC names a translation unit's
# dynamic initialiser _GLOBAL__sub_I_<something> whether it lists it in .init_array or in .ctors.
# Checking the array bounds alone would not - a toolchain emitting .ctors leaves .init_array empty
# no matter how many constructors the image has.
if(_symbols MATCHES "_GLOBAL__sub_I")
    message(FATAL_ERROR
            "The kernel contains a global constructor.\n"
            "The .init_array walk is the last node of the boot graph, so that an application "
            "constructor finds a live heap and legal SSE behind it. A kernel global would be "
            "constructed there too - after every component that could use it. Make it "
            "constant-initialised (constinit will say so at compile time), or move it behind an "
            "explicit init() the boot graph calls.")
endif()

foreach(_bound init_array_start init_array_end fini_array_start fini_array_end)
    if(NOT _symbols MATCHES "([0-9a-fA-F]+) [^ ]+ __${_bound}\n")
        message(FATAL_ERROR "${KERNEL_ELF} defines no __${_bound}")
    endif()

    set(_${_bound} "${CMAKE_MATCH_1}")
endforeach()

if(NOT _init_array_start STREQUAL _init_array_end)
    message(FATAL_ERROR "The kernel contributed entries to .init_array.")
endif()

if(NOT _fini_array_start STREQUAL _fini_array_end)
    message(FATAL_ERROR
            "The kernel contributed entries to .fini_array. Nothing calls the destructor walk on "
            "an image with no application, so these would silently never run.")
endif()
