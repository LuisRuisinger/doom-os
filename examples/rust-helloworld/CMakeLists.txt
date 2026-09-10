# =================================================================================================
# rustup target add x86_64-unknown-linux-musl
# cmake --preset default -DDOOM_OS_APPLICATION_DIR=examples/rust-helloworld
# cmake --build build --target run
# =================================================================================================

find_program(DOOM_OS_RUSTC rustc)
set(DOOM_OS_RUST_TARGET x86_64-unknown-linux-musl CACHE STRING "Rust target for the Rust example")

if(NOT DOOM_OS_RUSTC)
    message(FATAL_ERROR
            "This application is written in Rust and rustc was not found on PATH. "
            "Install a Rust toolchain and run 'rustup target add ${DOOM_OS_RUST_TARGET}'.")
endif()

execute_process(
        COMMAND ${DOOM_OS_RUSTC} --print target-libdir --target ${DOOM_OS_RUST_TARGET}
        OUTPUT_VARIABLE _rust_target_libdir
        RESULT_VARIABLE _rust_target_result
        OUTPUT_STRIP_TRAILING_WHITESPACE)

if(NOT _rust_target_result EQUAL 0)
    message(FATAL_ERROR
            "rustc could not resolve target '${DOOM_OS_RUST_TARGET}'. "
            "Run 'rustup target add ${DOOM_OS_RUST_TARGET}'.")
endif()

file(GLOB _rust_std "${_rust_target_libdir}/libstd-*.rlib")

if(NOT _rust_std)
    message(FATAL_ERROR
            "Rust std for '${DOOM_OS_RUST_TARGET}' was not found under "
            "${_rust_target_libdir}. Run 'rustup target add ${DOOM_OS_RUST_TARGET}'.")
endif()

set(_rust_self_contained_libdir "${_rust_target_libdir}/self-contained")
set(_rust_libunwind "${_rust_self_contained_libdir}/libunwind.a")
set(_rust_libc "${_rust_self_contained_libdir}/libc.a")

if(NOT EXISTS "${_rust_libunwind}" OR NOT EXISTS "${_rust_libc}")
    message(FATAL_ERROR
            "Rust target '${DOOM_OS_RUST_TARGET}' does not provide self-contained musl "
            "libunwind.a and libc.a under ${_rust_self_contained_libdir}.")
endif()

set(_rust_archive ${CMAKE_CURRENT_BINARY_DIR}/librust_helloworld.a)

add_custom_command(
        OUTPUT ${_rust_archive}
        COMMAND ${DOOM_OS_RUSTC}
        --target ${DOOM_OS_RUST_TARGET}
        --edition 2021
        --crate-type staticlib
        -C panic=abort
        -C code-model=kernel
        -C relocation-model=static
        -C no-redzone=yes
        -C opt-level=2
        -o ${_rust_archive}
        ${CMAKE_CURRENT_SOURCE_DIR}/hello.rs
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/hello.rs
        COMMENT "Compiling the Rust std application"
        VERBATIM)

add_custom_target(rust_helloworld_archive DEPENDS ${_rust_archive})

doom_os_application(rust_helloworld
        LIBRARIES ${_rust_archive} ${_rust_libunwind} ${_rust_libc}
        DEPENDS rust_helloworld_archive)
