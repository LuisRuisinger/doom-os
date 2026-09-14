#include "arch/x86_64/serial/serial.hpp"

#include "arch/x86_64/serial/io.hpp"
#include "kernel/core/bits.hpp"
#include "kernel/core/cast.hpp"
#include "kernel/core/types.hpp"
#include "kernel/debug/console.hpp"

namespace kernel::arch::x86_64::serial {

using kernel::core::u16;
using kernel::core::u32;
using kernel::core::u8;
using kernel::core::utils::bit;

static constexpr u16 COM1 = 0x3F8;

static constexpr u16 DATA_PORT = COM1 + 0;
static constexpr u16 INTERRUPT_ENABLE = COM1 + 1;
static constexpr u16 FIFO_CONTROL = COM1 + 2;
static constexpr u16 LINE_CONTROL = COM1 + 3;
static constexpr u16 MODEM_CONTROL = COM1 + 4;
static constexpr u16 LINE_STATUS = COM1 + 5;

static constexpr u32 UART_BASE_CLOCK_HZ = 115200;
static constexpr u32 BAUD_RATE = 38400;
static constexpr u16 BAUD_DIVISOR = (UART_BASE_CLOCK_HZ / BAUD_RATE) as(u16);

static constexpr u8 LINE_CONTROL_DLAB = bit<u8>(7);
static constexpr u8 LINE_CONTROL_8N1 = 0x03;
static constexpr u8 FIFO_ENABLE_CLEAR_14 = 0xC7;
static constexpr u8 MODEM_DTR_RTS_OUT2 = 0x0B;
static constexpr u8 LINE_STATUS_TX_EMPTY = bit<u8>(5);

void init()
{
    outb(INTERRUPT_ENABLE, 0x00);
    outb(LINE_CONTROL, LINE_CONTROL_DLAB);

    outb(DATA_PORT, (BAUD_DIVISOR & 0xFF) as(u8));
    outb(INTERRUPT_ENABLE, ((BAUD_DIVISOR >> 8) & 0xFF) as(u8));

    outb(LINE_CONTROL, LINE_CONTROL_8N1);
    outb(FIFO_CONTROL, FIFO_ENABLE_CLEAR_14);
    outb(MODEM_CONTROL, MODEM_DTR_RTS_OUT2);
}

bool can_write()
{
    return (inb(LINE_STATUS) & LINE_STATUS_TX_EMPTY) != 0;
}

void write_char(char c)
{
    while (!can_write())
        asm volatile("pause");

    outb(DATA_PORT, c as(u8));
}

void write(const char *s)
{
    if (s == nullptr) {
        write("<null>");
        return;
    }

    while (*s != '\0') {
        if (*s == '\n')
            write_char('\r');

        write_char(*s);
        ++s;
    }
}

static void console_sink(const char *bytes, kernel::core::usize length)
{
    for (kernel::core::usize index = 0; index < length; ++index) {
        if (bytes[index] == '\n')
            write_char('\r');

        write_char(bytes[index]);
    }
}

void attach_console()
{
    kernel::debug::set_console_sink(&console_sink);
}

}  // namespace kernel::arch::x86_64::serial