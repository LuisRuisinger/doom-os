# =================================================================================================
# C++ x SIMD
#
# Built by pointing the kernel configure at this directory:
#
#   cmake --preset default -DDOOM_OS_APPLICATION_DIR=examples/simd
#   cmake --build build --target run
#
# One directory per example, because an image holds exactly one application. Configuring with a
# different DOOM_OS_APPLICATION_DIR is how the image swaps one for another.
# =================================================================================================

doom_os_require_config(
        BOARD qemu_x86_64
        ARCHITECTURE x86_64
        MMU ON
        FPU ON)

doom_os_application(simd
        STDLIB NEWLIB_CXX
        SOURCES simd.cpp)
