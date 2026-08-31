# =================================================================================================
# Hello world, in Rust
#
#   rustup target add x86_64-unknown-none
#   cmake --preset default -DDOOM_OS_APPLICATION_DIR=examples/rust-helloworld
#   cmake --build build --target run
#
# rustc is not in the toolchain image and is not going to be: the image builds the kernel, and
# nothing in this repository is written in Rust. It is probed here and named in the error if it is
# missing, the same way the top-level configure probes for newlib.
#
# The flags are the contract an object has to meet, and are worth reading rather than copying:
#
#   --target x86_64-unknown-none   bare metal, no std, no libc assumptions
#   -C code-model=kernel           the image links at 0xFFFFFFFF80000000
#   -C relocation-model=static     no PIC; the image is not relocated after load
#   -C no-redzone=yes              see below
#   -C panic=abort                 there is no unwinder in the image to unwind to
#   --emit obj                     one object, which is all doom_os_application() wants
#
# no-redzone is the one that fails silently, and the reason is the same for Rust as for C. There is
# one privilege level here, so an interrupt does not switch stacks: the CPU pushes its frame at the
# current RSP, straight over the 128 bytes below it that a leaf function is entitled to use without
# adjusting the stack. It is spelled out rather than left to the target default because a corrupted
# red zone produces no diagnostic, only a value that changed when an interrupt happened to land.
# =================================================================================================

find_program(DOOM_OS_RUSTC rustc)

if(NOT DOOM_OS_RUSTC)
    message(FATAL_ERROR
            "This application is written in Rust and rustc was not found on PATH. "
            "Install a Rust toolchain and run 'rustup target add x86_64-unknown-none'.")
endif()

set(_rust_object ${CMAKE_CURRENT_BINARY_DIR}/rust_helloworld.o)

add_custom_command(
        OUTPUT ${_rust_object}
        COMMAND ${DOOM_OS_RUSTC}
        --target x86_64-unknown-none
        --edition 2021
        --crate-type lib
        --emit obj
        -C panic=abort
        -C code-model=kernel
        -C relocation-model=static
        -C no-redzone=yes
        -C opt-level=2
        -o ${_rust_object}
        ${CMAKE_CURRENT_SOURCE_DIR}/hello.rs
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hello.rs
        COMMENT "Compiling the Rust application"
        VERBATIM)

add_custom_target(rust_helloworld_object DEPENDS ${_rust_object})

doom_os_application(rust_helloworld

        OBJECTS ${_rust_object}
        DEPENDS rust_helloworld_object)
