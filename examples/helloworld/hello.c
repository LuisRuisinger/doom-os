/* =================================================================================================
 * Hello world
 *
 * An ordinary C program. It includes no kernel header and knows nothing about the machine it runs
 * on - it defines main and calls the C library, which is the whole point: the same source builds
 * hosted on Linux and here.
 *
 * What is worth following is where the calls end up. printf and malloc are newlib's; newlib
 * reaches the machine through _write and _sbrk; those live in libos/posix.cpp and
 * forward to the kernel's serial emitter and physical allocator.
 *
 * None of that is a syscall. Every arrow in that chain is a direct call resolved by the linker,
 * because the application, the C library, the shim and the kernel are one binary at one privilege
 * level. That is the whole of what makes this a unikernel.
 * ============================================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    /* printf -> newlib stdio -> _write(1, ...) -> kernel::debug::detail::emit_bytes -> serial. */
    printf("hello from userspace, such as it is\n");

    /* malloc -> newlib -> _sbrk -> pmm::alloc_page. Proves the heap path independently of stdio,
     * which matters because the two fail in very different places. */
    char *greeting = malloc(64);

    if (greeting == NULL) {
        printf("malloc failed: the program break never came up\n");
        return 1;
    }

    strcpy(greeting, "and this string lives in a PMM page");
    printf("%s\n", greeting);

    free(greeting);

    return 0;
}
