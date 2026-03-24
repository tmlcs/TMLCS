/* =============================================================================
 * x86_64 Port I/O Primitives
 * =============================================================================
 *
 * LOW-001 FIX: Centralizes outb()/inb()/io_delay() which were copy-pasted
 * identically across serial.cpp, irq.cpp, and pit.cpp.
 *
 * All functions are static inline — no link-time symbol emitted.
 * =============================================================================
 */

#ifndef IO_H
#define IO_H

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Write a byte to an x86 I/O port
 *
 * Issues the x86 `outb` instruction.  The port address space is separate
 * from the memory address space; only privileged (ring-0) code may access it.
 *
 * @param port  16-bit I/O port address (e.g. 0x3F8 for COM1)
 * @param value Byte to write
 */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * @brief Read a byte from an x86 I/O port
 *
 * Issues the x86 `inb` instruction.
 *
 * @param port 16-bit I/O port address
 * @return Byte read from the port
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * @brief Short I/O delay via POST diagnostic port (0x80)
 *
 * Writing to port 0x80 wastes a small number of bus cycles without
 * side-effects.  Required between consecutive PIC ICW writes and
 * anywhere else back-to-back port accesses need a settling period.
 */
static inline void io_delay(void) {
    outb(0x80, 0);
}

#ifdef __cplusplus
}
#endif

#endif /* IO_H */
