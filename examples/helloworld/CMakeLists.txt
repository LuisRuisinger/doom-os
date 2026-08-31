# =================================================================================================
# Hello world
#
#   cmake --preset default -DDOOM_OS_APPLICATION_DIR=examples/helloworld
#   cmake --build build --target run
#
# The application is compiled here rather than by doom_os_application(), which takes objects and
# nothing else. That is the point of the split: the image resolves symbols, it does not build
# programs, so an application in Rust or Zig arrives the same way this one does.
#
# Which means the flags below are the contract, not boilerplate. -mno-red-zone is the one that
# matters and the one that fails silently: an interrupt at this privilege level does not switch
# stacks, so it lands on top of the red zone a leaf function was using. -mcmodel=kernel matches
# the higher-half link. SSE is permitted because cpu::fpu_component enables it before main.
# =================================================================================================

add_library(helloworld_objects OBJECT hello.c)

target_include_directories(helloworld_objects PRIVATE ${DOOM_OS_ROOT}/include)

target_compile_options(helloworld_objects PRIVATE
        -ffreestanding
        -nostdlib
        -mno-red-zone
        -mcmodel=kernel
        -fno-stack-protector
        -Wall
        -Wextra
        -Wpedantic)

doom_os_application(helloworld

        OBJECTS $<TARGET_OBJECTS:helloworld_objects>)
