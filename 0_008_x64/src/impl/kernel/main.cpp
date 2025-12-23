#include "print.h"
#include "serial.h"
#include "log.h"
#include "idt.h"
#include "interrupts.h" // Include interrupts.h
#include "pic.h" // Include pic.h
#include "multiboot2.h" // Include multiboot2.h
#include "pmm.h" // Include pmm.h
#include "vmm.h" // Include vmm.h
#include "heap.h" // Include heap.h

typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;

extern "C" void call_ctors() {
    for (constructor* i = &start_ctors; i != &end_ctors; i++) {
        (*i)();
    }
}

IDT idt; // Global instance of the IDT

// Declare external assembly interrupt stubs
extern "C" void isr0();
extern "C" void isr1();
extern "C" void isr2();
extern "C" void isr3();
extern "C" void isr4();
extern "C" void isr5();
extern "C" void isr6();
extern "C" void isr7();
extern "C" void isr8();
extern "C" void isr9();
extern "C" void isr10();
extern "C" void isr11();
extern "C" void isr12();
extern "C" void isr13();
extern "C" void isr14();
extern "C" void isr15();
extern "C" void isr16();
extern "C" void isr17();
extern "C" void isr18();
extern "C" void isr19();

// Hardware IRQ stubs
extern "C" void isr0x20(); // IRQ0 - Timer
extern "C" void isr0x21(); // IRQ1 - Keyboard
extern "C" void isr0x22(); // IRQ2 - Cascade
extern "C" void isr0x23(); // IRQ3 - COM2
extern "C" void isr0x24(); // IRQ4 - COM1
extern "C" void isr0x25(); // IRQ5 - LPT2
extern "C" void isr0x26(); // IRQ6 - Floppy
extern "C" void isr0x27(); // IRQ7 - LPT1
extern "C" void isr0x28(); // IRQ8 - RTC
extern "C" void isr0x29(); // IRQ9 - Redirected IRQ2
extern "C" void isr0x2A(); // IRQ10 - Reserved
extern "C" void isr0x2B(); // IRQ11 - Reserved
extern "C" void isr0x2C(); // IRQ12 - PS/2 Mouse
extern "C" void isr0x2D(); // IRQ13 - FPU
extern "C" void isr0x2E(); // IRQ14 - Primary ATA
extern "C" void isr0x2F(); // IRQ15 - Secondary ATA


extern "C" void kernel_main(const void* multiboot_structure_ptr, unsigned int magic_number) {
    (void)multiboot_structure_ptr; // Suppress unused parameter warning
    Serial::init(); // Initialize serial early for logging
    Log::info("Serial port initialized.");

    Log::info("kernel_main entered.");
    Log::info("Magic Number: ");
    Log::info(Log::uint64_to_string(magic_number));
    Log::info("Multiboot Structure Ptr: ");
    Log::info(Log::uint64_to_string((uint64_t)multiboot_structure_ptr));

    print_clear();
    print_set_color(PRINT_COLOR_LIGHT_RED, PRINT_COLOR_BLACK);
    Log::info("Welcome to GLOBEX by TMLCS (NW OS V:0.0002a) 64-bit kernel");
    
    // Check Multiboot magic number
    if (magic_number != MULTIBOOT2_BOOTLOADER_MAGIC) {
        Log::critical("Invalid Multiboot magic number!");
        asm volatile ("hlt");
    }

    // Parse Multiboot structure
    multiboot_info* mbi = (multiboot_info*)multiboot_structure_ptr;
    uint64_t total_memory_bytes = 0;
    uint64_t highest_address = 0;

    for (multiboot_tag* tag = mbi->tags; tag->type != MULTIBOOT_TAG_TYPE_END; tag = (multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7))) {
        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_MMAP: {
                multiboot_tag_mmap* mmap_tag = (multiboot_tag_mmap*)tag;
                for (multiboot_mmap_entry* entry = mmap_tag->entries; (uint8_t*)entry < (uint8_t*)tag + tag->size; entry = (multiboot_mmap_entry*)((uint8_t*)entry + mmap_tag->entry_size)) {
                    if (entry->type == 1) { // Available memory
                        if (entry->addr + entry->len > highest_address) {
                            highest_address = entry->addr + entry->len;
                        }
                    }
                }
                total_memory_bytes = highest_address; // Simple approximation for total memory
                break;
            }
        }
    }

    if (total_memory_bytes == 0) {
        Log::critical("Could not determine total memory from Multiboot!");
        asm volatile ("hlt");
    }

    Log::info("Total memory detected: ");
    Log::info(Log::uint64_to_string(total_memory_bytes));
    Log::info(" bytes.");

    // Initialize PMM
    // Place bitmap at a known address, e.g., 16MB (0x1000000)
    // Ensure this address is not used by the kernel or other modules
    uint64_t pmm_bitmap_address = 0x1000000; 
    PMM::init(total_memory_bytes, pmm_bitmap_address);

    // Mark all available memory regions as free
    for (multiboot_tag* tag = mbi->tags; tag->type != MULTIBOOT_TAG_TYPE_END; tag = (multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7))) {
        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_MMAP: {
                multiboot_tag_mmap* mmap_tag = (multiboot_tag_mmap*)tag;
                for (multiboot_mmap_entry* entry = mmap_tag->entries; (uint8_t*)entry < (uint8_t*)tag + tag->size; entry = (multiboot_mmap_entry*)((uint8_t*)entry + mmap_tag->entry_size)) {
                    if (entry->type == 1) { // Available memory
                        PMM::mark_region_free(entry->addr, entry->len);
                    }
                }
                break;
            }
        }
    }

    // Mark kernel sections as used
    for (multiboot_tag* tag = mbi->tags; tag->type != MULTIBOOT_TAG_TYPE_END; tag = (multiboot_tag*)((uint8_t*)tag + ((tag->size + 7) & ~7))) {
        if (tag->type == MULTIBOOT_TAG_TYPE_ELF_SECTIONS) {
            multiboot_tag_elf_sections* elf_sections_tag = (multiboot_tag_elf_sections*)tag;
            for (unsigned int i = 0; i < elf_sections_tag->num; ++i) {
                multiboot_elf_section_header* shdr = (multiboot_elf_section_header*)((uint8_t*)elf_sections_tag->sections + (i * elf_sections_tag->entsize));
                
                // Mark only allocatable sections as used
                if ((shdr->flags & MB_ELF_SHF_ALLOC) && shdr->addr != 0) {
                    PMM::mark_region_used(shdr->addr, shdr->size);
                    Log::info("Marked kernel section as used: addr=");
                    Log::info(Log::uint64_to_string(shdr->addr));
                    Log::info(", size=");
                    Log::info(Log::uint64_to_string(shdr->size));
                    Log::info(" bytes.");
                }
            }
            break; // Found ELF sections, no need to continue
        }
    }

    // Mark the PMM bitmap itself as used
    uint64_t pmm_bitmap_size = (total_memory_bytes / PMM::PAGE_SIZE / 8) + PMM::PAGE_SIZE;
    PMM::mark_region_used(pmm_bitmap_address, pmm_bitmap_size);

    Log::info("PMM: Free memory: ");
    Log::info(Log::uint64_to_string(PMM::get_free_memory()));
    Log::info(" bytes.");
    Log::info("PMM: Used memory: ");
    Log::info(Log::uint64_to_string(PMM::get_used_memory()));
    Log::info(" bytes.");

    // Initialize Heap
    uint64_t heap_start = pmm_bitmap_address + pmm_bitmap_size;
    uint64_t heap_end = heap_start + (10 * PMM::PAGE_SIZE); // 10 pages for heap (40KB)
    Heap::init(heap_start, heap_end);

    // Test Heap
    Log::info("Heap: Testing malloc/free...");
    void* test_ptr1 = Heap::malloc(100);
    if (test_ptr1) {
        Log::info("Heap: Allocated 100 bytes.");
        Heap::free(test_ptr1);
        Log::info("Heap: Freed 100 bytes.");
    } else {
        Log::error("Heap: Failed to allocate 100 bytes.");
    }


    // Set up IDT gates for CPU exceptions
    idt.SetGate(0, (uint64_t)isr0, 0x08, 0x8E);
    idt.SetGate(1, (uint64_t)isr1, 0x08, 0x8E);
    idt.SetGate(2, (uint64_t)isr2, 0x08, 0x8E);
    idt.SetGate(3, (uint64_t)isr3, 0x08, 0x8E);
    idt.SetGate(4, (uint64_t)isr4, 0x08, 0x8E);
    idt.SetGate(5, (uint64_t)isr5, 0x08, 0x8E);
    idt.SetGate(6, (uint64_t)isr6, 0x08, 0x8E);
    idt.SetGate(7, (uint64_t)isr7, 0x08, 0x8E);
    idt.SetGate(8, (uint64_t)isr8, 0x08, 0x8E);
    idt.SetGate(9, (uint64_t)isr9, 0x08, 0x8E);
    idt.SetGate(10, (uint64_t)isr10, 0x08, 0x8E);
    idt.SetGate(11, (uint64_t)isr11, 0x08, 0x8E);
    idt.SetGate(12, (uint64_t)isr12, 0x08, 0x8E);
    idt.SetGate(13, (uint64_t)isr13, 0x08, 0x8E);
    idt.SetGate(14, (uint64_t)isr14, 0x08, 0x8E);
    idt.SetGate(15, (uint64_t)isr15, 0x08, 0x8E);
    idt.SetGate(16, (uint64_t)isr16, 0x08, 0x8E);
    idt.SetGate(17, (uint64_t)isr17, 0x08, 0x8E);
    idt.SetGate(18, (uint64_t)isr18, 0x08, 0x8E);
    idt.SetGate(19, (uint64_t)isr19, 0x08, 0x8E);

    // Set up IDT gates for hardware IRQs
    idt.SetGate(0x20, (uint64_t)isr0x20, 0x08, 0x8E);
    idt.SetGate(0x21, (uint64_t)isr0x21, 0x08, 0x8E);
    idt.SetGate(0x22, (uint64_t)isr0x22, 0x08, 0x8E);
    idt.SetGate(0x23, (uint64_t)isr0x23, 0x08, 0x8E);
    idt.SetGate(0x24, (uint64_t)isr0x24, 0x08, 0x8E);
    idt.SetGate(0x25, (uint64_t)isr0x25, 0x08, 0x8E);
    idt.SetGate(0x26, (uint64_t)isr0x26, 0x08, 0x8E);
    idt.SetGate(0x27, (uint64_t)isr0x27, 0x08, 0x8E);
    idt.SetGate(0x28, (uint64_t)isr0x28, 0x08, 0x8E);
    idt.SetGate(0x29, (uint64_t)isr0x29, 0x08, 0x8E);
    idt.SetGate(0x2A, (uint64_t)isr0x2A, 0x08, 0x8E);
    idt.SetGate(0x2B, (uint64_t)isr0x2B, 0x08, 0x8E);
    idt.SetGate(0x2C, (uint64_t)isr0x2C, 0x08, 0x8E);
    idt.SetGate(0x2D, (uint64_t)isr0x2D, 0x08, 0x8E);
    idt.SetGate(0x2E, (uint64_t)isr0x2E, 0x08, 0x8E);
    idt.SetGate(0x2F, (uint64_t)isr0x2F, 0x08, 0x8E);

    PIC::remap(0x20, 0x28); // Remap PIC IRQs to start at 0x20 and 0x28
    PIC::disable();         // Disable PIC for now (will enable later when needed)

    idt.Load(); // Load the IDT
    asm volatile ("sti"); // Enable interrupts

    asm volatile ("hlt");
}
