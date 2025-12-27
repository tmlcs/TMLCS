#include "cpu/idt.h"
#include "utils/log.h"

// Assembly function to load the IDT
extern "C" void lidt(IDT::IDT_ptr* idt_ptr);

IDT::IDT() {
    idt_ptr.limit = sizeof(IDTEntry) * 256 - 1;
    idt_ptr.base = (uint64_t)&idt_entries;

    // Initialize all entries to zero
    for (int i = 0; i < 256; ++i) {
        idt_entries[i] = {};
    }

    Log::info("IDT initialized.");
}

IDT::~IDT() {
    Log::info("IDT destroyed.");
}

void IDT::SetGate(uint8_t num, uint64_t base, uint16_t selector, uint8_t type_attr, uint8_t ist) {
    idt_entries[num].offset_low = base & 0xFFFF;
    idt_entries[num].offset_mid = (base >> 16) & 0xFFFF;
    idt_entries[num].offset_high = (base >> 32) & 0xFFFFFFFF;

    idt_entries[num].selector = selector;
    idt_entries[num].ist = ist;
    idt_entries[num].type_attr = type_attr;
    idt_entries[num].zero = 0;
}

void IDT::Load() {
    lidt(&idt_ptr);
    Log::info("IDT loaded.");
}
