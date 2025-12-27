#include "cpu/gdt.h"

GDT::SegmentDescriptor::SegmentDescriptor(uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity) {
    this->base_low = base & 0xFFFF;
    this->base_middle = (base >> 16) & 0xFF;
    this->base_high = (base >> 24) & 0xFF;

    this->limit_low = limit & 0xFFFF;
    this->granularity = (granularity & 0xF0) | ((limit >> 16) & 0x0F);
    this->access = access;
}

uint32_t GDT::SegmentDescriptor::Base() {
    return (uint32_t)base_low | ((uint32_t)base_middle << 16) | ((uint32_t)base_high << 24);
}

uint32_t GDT::SegmentDescriptor::Limit() {
    return (uint32_t)limit_low | ((uint32_t)(granularity & 0x0F) << 16);
}

GDT::GDT() :
    null_descriptor(0, 0, 0, 0),
    unused_descriptor(0, 0, 0, 0),
    code_descriptor(0, 0xFFFFFFFF, 0x9A, 0xCF),
    data_descriptor(0, 0xFFFFFFFF, 0x92, 0xCF)
{
    code_segment = (uint64_t)&code_descriptor - (uint64_t)this;
    data_segment = (uint64_t)&data_descriptor - (uint64_t)this;

    gdt_ptr ptr;
    ptr.limit = sizeof(GDT) - 1;
    ptr.base = (uint64_t)this;

    asm volatile ("lgdt %0" : : "m" (ptr));
}

GDT::~GDT() {
    
}



