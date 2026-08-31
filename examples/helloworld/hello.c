/* =================================================================================================
 * Hello world
 *
 * An ordinary C program that includes no kernel header and links no C library. It calls write,
 * which is a POSIX name libos implements directly - so this is the whole boundary, with nothing
 * in between.
 *
 * A program wanting printf, malloc or strlen brings a libc that provides them; DoomOS supplies
 * the x86_64 Linux ABI underneath, not the library on top of it. That is the same decision as
 * the application being an object rather than a source file: the image resolves symbols, it does
 * not choose your toolchain.
 *
 * None of this is a syscall. Every arrow is a direct call the linker resolved, because the
 * application, libos and the kernel are one binary at one privilege level.
 * ============================================================================================== */

typedef unsigned long size_t;
typedef long          ssize_t;

ssize_t write(int fd, const void *buffer, size_t count);

static size_t length_of(const char *text)
{
    size_t length = 0;

    while (text[length] != '\0') {
        ++length;
    }

    return length;
}

static void say(const char *text)
{
    write(1, text, length_of(text));
}

int main(void)
{
    say("hello from userspace, such as it is\n");
    say("no libc in this image: write() is libos, and libos is the kernel\n");

    return 0;
}
