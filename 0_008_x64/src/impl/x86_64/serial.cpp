#include "serial.h"
#include <stdint.h>

// Port addresses for COM1
#define COM1_PORT 0x3F8

// I/O port functions (assembly)
extern "C" void outb(uint16_t port, uint8_t data);
extern "C" uint8_t inb(uint16_t port);

namespace Serial {

    void init() {
        outb(COM1_PORT + 1, 0x00);    // Disable all interrupts
        outb(COM1_PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
        outb(COM1_PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
        outb(COM1_PORT + 1, 0x00);    //                  (hi byte)
        outb(COM1_PORT + 3, 0x03);    // 8 bits, no parity, 1 stop bit, Disable DLAB
        outb(COM1_PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
        outb(COM1_PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    }

    bool is_transmit_empty() {
        return inb(COM1_PORT + 5) & 0x20;
    }

    void write_char(char c) {
        while (!is_transmit_empty());
        outb(COM1_PORT, c);
    }

    void write_string(const char* str) {
        while (*str != '\0') {
            write_char(*str++);
        }
    }
}
