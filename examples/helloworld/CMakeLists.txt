# =================================================================================================
# Hello world
#
# Built by pointing the kernel configure at this directory:
#
#   cmake --preset default -DDOOM_OS_APPLICATION_DIR=examples/helloworld
#   cmake --build build --target run
#
# One directory per example, because an image holds exactly one application. None of them is built
# by default, for the same reason DOOM_OS_DRIVER_DIR is not: which application an image carries is
# the integrator's choice rather than this repository's.
# =================================================================================================

doom_os_require_config(
        BOARD qemu_x86_64
        ARCHITECTURE x86_64
        MMU ON
        FPU ON)

doom_os_application(helloworld
        STDLIB NEWLIB
        SOURCES hello.c)
