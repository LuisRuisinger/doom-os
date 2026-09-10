// =================================================================================================
// Hello world, in Rust std on musl
//
// This is deliberately ordinary Rust at the top: std is present, println! is present, and panics
// use Rust's panic runtime instead of a local #[panic_handler]. The exported C ABI main is the
// symbol entry/start.cpp calls after the kernel boot graph has completed.
// =================================================================================================

use core::ffi::{c_char, c_int};

fn app_main() {
    println!("hello from Rust std on musl");
    println!("std is linked by the app; DoomOS still owns the POSIX edge below it");
}

#[no_mangle]
pub extern "C" fn main(_argc: c_int, _argv: *mut *mut c_char) -> c_int {
    app_main();
    1
}
