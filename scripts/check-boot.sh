#!/usr/bin/env sh
#
# Assert that a captured serial log shows a complete, correctly ordered boot.
#
# Two failure modes look identical from outside QEMU and this distinguishes them: a kernel
# that faults, and a kernel that was never loaded (a broken grub.cfg does exactly that, and
# produces no serial output whatsoever).

set -eu

log="${1:?usage: check-boot.sh <boot.log>}"

fail()
{
    echo "boot check FAILED: $1" >&2
    echo "--- serial log ---" >&2
    if [ -s "$log" ]; then
        cat "$log" >&2
    else
        echo "(empty)" >&2
    fi
    exit 1
}

[ -s "$log" ] || fail "no serial output at all - the kernel was never loaded"

# Order matters. The platform phase has to finish before anything reads bootloader-supplied
# data, because until the IDT is live a fault is a triple fault with no diagnostics.
prev=0
while IFS= read -r marker; do
    [ -n "$marker" ] || continue

    line=$(grep -n -F -m1 -- "$marker" "$log" | cut -d: -f1) || fail "missing: $marker"
    [ -n "$line" ] || fail "missing: $marker"
    [ "$line" -gt "$prev" ] || fail "out of order: $marker"

    prev=$line
done <<'MARKERS'
KERNEL BOOT
[init] CPU: OK
[init][core 0] STACKS: OK
[init][core 0] TSS: OK
[init][core 0] GDT: OK
[init][core 0] EXCEPTIONS: OK
[init][core 0] IDT: OK
[init] CORE_INIT: OK
[init] BOOT_INFO: OK
[init] PMM: OK
MARKERS

if grep -q "FAIL" "$log"; then
    fail "a component reported FAIL"
fi

# kprint renders these, so they double as a check on the formatter: a zero-padded 64-bit hex
# in alternate form ({:#018X}) and a plain decimal ({}).
grep -Eq 'rax: 0x[0-9A-F]{16}' "$log" || fail "register dump is not formatted as {:#018X}"
grep -Eq 'allocatable=[0-9]+ KiB' "$log" || fail "decimal formatting looks wrong"

# kernel_main64 currently ends in ud2 on purpose, so reaching the panic handler proves the IDT
# is genuinely live and dispatching. Drop this check when the kernel gets real work to do.
if ! grep -q "#UD invalid opcode" "$log"; then
    fail "expected the trailing ud2 to trap - is the IDT live?"
fi

echo "boot check OK: $(grep -c ': OK' "$log") components initialised, exceptions dispatching"
