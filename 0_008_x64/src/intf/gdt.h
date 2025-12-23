#ifndef GDT_H
#define GDT_H

#include <types.h>

class GDT {
public:
    struct gdt_ptr {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed));

    class SegmentDescriptor {
    private:
        uint16_t limit_low;
        uint16_t base_low;
        uint8_t base_middle;
        uint8_t access;
        uint8_t granularity;
        uint8_t base_high;
    public:
        SegmentDescriptor(uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity);
        uint32_t Base();
        uint32_t Limit();
    } __attribute__((packed));

    SegmentDescriptor null_descriptor;
    SegmentDescriptor unused_descriptor;
    SegmentDescriptor code_descriptor;
    SegmentDescriptor data_descriptor;
    
public:
    GDT();
    ~GDT();

    uint64_t code_segment;
    uint64_t data_segment;
    
};

#endif