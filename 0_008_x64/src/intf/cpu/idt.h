#ifndef IDT_H
#define IDT_H

#include <stdint.h>
#include <types.h> // For fixed-width integer types

class IDT {
public:
    struct IDTEntry {
        uint16_t offset_low;    // Offset bits 0..15
        uint16_t selector;      // A code segment selector in GDT or LDT
        uint8_t ist;            // Interrupt Stack Table offset (0 for unused)
        uint8_t type_attr;      // Type and attributes
        uint16_t offset_mid;    // Offset bits 16..31
        uint32_t offset_high;   // Offset bits 32..63
        uint32_t zero;          // Reserved (must be zero)
    } __attribute__((packed));

    struct IDT_ptr {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed));

    IDTEntry idt_entries[256]; // 256 possible interrupt vectors
    IDT_ptr idt_ptr;

public:
    IDT();
    ~IDT();

    void SetGate(uint8_t num, uint64_t base, uint16_t selector, uint8_t type_attr, uint8_t ist = 0);
    void Load();
};

#endif // IDT_H
