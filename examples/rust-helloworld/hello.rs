// =================================================================================================
// Hello world, in Rust
//
// The same program as examples/helloworld, in a language this project's toolchain cannot compile.
// That is the point: doom_os_application() takes objects, so the image never learns what produced
// them, and the only conversation is symbol resolution.
//
// no_std and no libc. write is a POSIX name libos implements directly, reached here through the
// same C ABI a C program uses. A Rust application wanting std brings its own libc and its own
// std; DoomOS supplies the x86_64 Linux ABI beneath, which libos/glibc_abi.cpp is the beginning
// of.
// =================================================================================================

#![no_std]
#![no_main]

use core::panic::PanicInfo;

extern "C" {
    fn write(fd: i32, buffer: *const u8, count: usize) -> isize;
}

fn say(text: &str) {
    unsafe {
        write(1, text.as_ptr(), text.len());
    }
}

// The kernel calls main once, from kernel_main64, after the boot graph has finished. It is an
// ordinary C-ABI symbol: defining it here overrides the weak one in libos/entry.cpp at link time,
// which is the whole of how an application attaches to the image.
#[no_mangle]
pub extern "C" fn main(_argc: i32, _argv: *mut *mut u8) -> i32 {
    say("hello from rust, such as it is\n");
    say("no libc, no std: this object resolves against libos and nothing else\n");

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
