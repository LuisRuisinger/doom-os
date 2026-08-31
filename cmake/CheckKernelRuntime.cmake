# =================================================================================================
# Kernel C++ runtime check
#
# Run against an image built with no application, where every byte came from the kernel. The
# kernel is written in a C++ subset with no runtime: no virtual functions, no global operator new,
# and no object whose construction or destruction happens at runtime. That rule is what lets the
# C++ runtime live in libos and be linked, or not, on the application's say-so.
#
# Most of it enforces itself. A vtable needs __cxa_pure_virtual and a `new` needs operator new,
# neither of which exists in a STDLIB NONE image, so either one is an undefined symbol at link
# time. A global constructor is the exception: it emits an .init_array entry that simply never
# runs, with nothing undefined and no diagnostic. That is the case worth a check.
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
            "An image with no application has nothing that may construct at runtime: the kernel "
            "is written in a C++ subset with no runtime, and the .init_array walk lives in libos "
            "on the application's behalf. A namespace-scope object needing a constructor here "
            "would silently never be constructed. Make it constant-initialised - constinit will "
            "say so at compile time - or move it behind an explicit init() the boot graph calls.")
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
