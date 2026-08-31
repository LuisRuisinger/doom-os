// =================================================================================================
// Hello world, in Rust
//
// The same program as examples/helloworld, in a language this project's toolchain cannot compile.
// That is the point: doom_os_application() takes objects, so the image never learns what produced
// them, and the only conversation is symbol resolution.
//
// no_std, because std wants threads, TLS and mmap - things a unikernel answers differently or not
// at all. core needs nothing from the image. printf comes from newlib, reached through
// libos/posix.cpp exactly as the C version reaches it: no syscall, just a call the linker resolves.
//
// The finished object asks for puts rather than printf, which is not a mistake: LLVM rewrites a
// printf with no arguments and a trailing newline into puts. newlib supplies both, so it makes no
// difference here, but it is worth knowing before reading nm output and looking for a symbol that
// is not there.
// =================================================================================================

#![no_std]
#![no_main]

use core::panic::PanicInfo;

extern "C" {
    fn printf(format: *const u8, ...) -> i32;
}

// The kernel calls main once, from kernel_main64, after the boot graph has finished. It is an
// ordinary C-ABI symbol: defining it here overrides the weak one in libos/entry.cpp at link time,
// which is the whole of how an application attaches to the image.
#[no_mangle]
pub extern "C" fn main(_argc: i32, _argv: *mut *mut u8) -> i32 {
    unsafe {
        printf(b"hello from rust, such as it is\n\0".as_ptr());
    }

    0
}

// Required by core, and the analogue of __cxa_pure_virtual: the one language feature that needs a
// runtime answer. There is nothing to unwind to and nowhere to report, so it stops.
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {
        unsafe { core::arch::asm!("hlt") };
    }
}
