# =================================================================================================
# C++ x SIMD
#
#   cmake --preset default -DDOOM_OS_APPLICATION_DIR=examples/simd
#   cmake --build build --target run
#
# Compiled here, then handed to doom_os_application() as objects. See examples/helloworld for why
# the flags are the application's own business, and which of them fails silently if dropped.
#
# The C++ specific ones: -fno-exceptions and -fno-rtti because the image carries no unwinder, and
# -fno-use-cxa-atexit and -fno-threadsafe-statics to match how the rest of the image is built.
# =================================================================================================

add_library(simd_objects OBJECT simd.cpp)

target_include_directories(simd_objects PRIVATE ${DOOM_OS_ROOT}/include)

target_compile_options(simd_objects PRIVATE
        -ffreestanding
        -nostdlib
        -mno-red-zone
        -mcmodel=kernel
        -fno-stack-protector
        -fno-exceptions
        -fno-rtti
        -fno-use-cxa-atexit
        -fno-threadsafe-statics
        -Wall
        -Wextra
        -Wpedantic)

doom_os_application(simd
        STDLIB NEWLIB_CXX
        OBJECTS $<TARGET_OBJECTS:simd_objects>)
