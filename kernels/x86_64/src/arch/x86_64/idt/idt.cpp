/* =============================================================================
 * IDT - Interrupt Descriptor Table Implementation
 * =============================================================================
 *
 * This module implements the Interrupt Descriptor Table for x86_64.
 * It handles CPU exceptions (INT 0-31) and hardware IRQs (INT 32-47).
 *
 * =============================================================================
 */

#include "idt.h"
#include "stddef.h"
#include "print.h"
#include "serial.h"
#include "gdt.h" /* For GDT_SELECTOR_KERNEL_CODE */
#include "constants.h" /* For IDT_, PIC_, EXCEPTION_, IRQ_ constants */

/* =============================================================================
 * IDT Table - Static storage
 * =============================================================================
 * Aligned to 16 bytes for LIDT instruction efficiency.
 * =============================================================================
 */
static idt_entry_t idt_table[IDT_ENTRIES] __attribute__((aligned(16)));

/* =============================================================================
 * IDT Pointer - Static storage
 * =============================================================================
 */
static idt_pointer_t idt_pointer;

/* =============================================================================
 * External assembly functions
 * =============================================================================
 */
extern "C" {
void idt_load(idt_pointer_t* idtp);
}

/* =============================================================================
 * External interrupt handlers (defined in interrupts.asm)
 * =============================================================================
 * These are the actual ISR stubs that save state and call our C handlers.
 * Must use extern "C" to prevent C++ name mangling.
 * =============================================================================
 */

/* =============================================================================
 * External interrupt handlers (defined in interrupts.asm)
 * =============================================================================
 * These are the actual ISR stubs that save state and call our C handlers.
 * Must use extern "C" to prevent C++ name mangling.
 *
 * All vectors 0-31 now have handlers. Reserved vectors (9, 15, 20-31)
 * use stub handlers that trigger a panic with diagnostic info.
 * =============================================================================
 */

/* Exception handlers (no error code) - Vectors 0-7 */
extern "C" void isr0(void);  /* Divide Error (#DE) */
extern "C" void isr1(void);  /* Debug (#DB) */
extern "C" void isr2(void);  /* Non-Maskable Interrupt (NMI) */
extern "C" void isr3(void);  /* Breakpoint (#BP) */
extern "C" void isr4(void);  /* Overflow (#OF) */
extern "C" void isr5(void);  /* Bound Range Exceeded (#BR) */
extern "C" void isr6(void);  /* Invalid Opcode (#UD) */
extern "C" void isr7(void);  /* Device Not Available (#NM) */

/* Exception handlers (with error code) - Vectors 8-14 */
extern "C" void isr8(void);   /* Double Fault (#DF) */
extern "C" void isr9(void);   /* Reserved (Intel/AMD) - Stub handler */
extern "C" void isr10(void);  /* Invalid TSS (#TS) */
extern "C" void isr11(void);  /* Segment Not Present (#NP) */
extern "C" void isr12(void);  /* Stack Fault (#SS) */
extern "C" void isr13(void);  /* General Protection Fault (#GP) */
extern "C" void isr14(void);  /* Page Fault (#PF) */

/* Reserved and other exceptions - Vectors 15-31 */
extern "C" void isr15(void);  /* Reserved (Intel/AMD) - Stub handler */
extern "C" void isr16(void);  /* x87 FPU Error (#MF) */
extern "C" void isr17(void);  /* Alignment Check (#AC) */
extern "C" void isr18(void);  /* Machine Check (#MC) */
extern "C" void isr19(void);  /* SIMD FPU Exception (#XM) */

/* Reserved vectors 20-31 - Stub handlers for future CPU extensions */
extern "C" void isr20(void);  /* Reserved (future CPU extension) */
extern "C" void isr21(void);  /* Reserved (future CPU extension) */
extern "C" void isr22(void);  /* Reserved (future CPU extension) */
extern "C" void isr23(void);  /* Reserved (future CPU extension) */
extern "C" void isr24(void);  /* Reserved (future CPU extension) */
extern "C" void isr25(void);  /* Reserved (future CPU extension) */
extern "C" void isr26(void);  /* Reserved (future CPU extension) */
extern "C" void isr27(void);  /* Reserved (future CPU extension) */
extern "C" void isr28(void);  /* Reserved (future CPU extension) */
extern "C" void isr29(void);  /* Reserved (future CPU extension) */
extern "C" void isr30(void);  /* Reserved (future CPU extension) */
extern "C" void isr31(void);  /* Reserved (future CPU extension) */

/* Hardware IRQ handlers - Match interrupts.asm names */
extern "C" void irq0_stub(void);  /* PIT */
extern "C" void irq1_stub(void);  /* Keyboard */
extern "C" void irq2_stub(void);  /* Cascade */
extern "C" void irq3_stub(void);  /* COM2 */
extern "C" void irq4_stub(void);  /* COM1 */
extern "C" void irq5_stub(void);  /* LPT2 */
extern "C" void irq6_stub(void);  /* Floppy */
extern "C" void irq7_stub(void);  /* LPT1 */
extern "C" void irq8_stub(void);  /* RTC */
extern "C" void irq9_stub(void);  /* ACPI */
extern "C" void irq10_stub(void); /* Available */
extern "C" void irq11_stub(void); /* Free */
extern "C" void irq12_stub(void); /* Mouse */
extern "C" void irq13_stub(void); /* Coprocessor */
extern "C" void irq14_stub(void); /* Primary ATA */
extern "C" void irq15_stub(void); /* Secondary ATA */

/* =============================================================================
 * Exception Messages
 * =============================================================================
 * Messages for all exception vectors 0-31.
 * Reserved vectors indicate potential hardware bugs or future extensions.
 * =============================================================================
 */
static const char* exception_messages[] = {
    /* Vectors 0-7: Standard CPU exceptions */
    "Divide Error (#DE)",              /* 0 */
    "Debug (#DB)",                     /* 1 */
    "Non-Maskable Interrupt (NMI)",    /* 2 */
    "Breakpoint (#BP)",                /* 3 */
    "Overflow (#OF)",                  /* 4 */
    "Bound Range Exceeded (#BR)",      /* 5 */
    "Invalid Opcode (#UD)",            /* 6 */
    "Device Not Available (#NM)",      /* 7 */

    /* Vectors 8-14: Exceptions with error code */
    "Double Fault (#DF)",              /* 8 */
    "Reserved (Intel/AMD)",            /* 9 - Reserved */
    "Invalid TSS (#TS)",               /* 10 */
    "Segment Not Present (#NP)",       /* 11 */
    "Stack Fault (#SS)",               /* 12 */
    "General Protection Fault (#GP)",  /* 13 */
    "Page Fault (#PF)",                /* 14 */

    /* Vectors 15-19: Reserved and other exceptions */
    "Reserved (Intel/AMD)",            /* 15 - Reserved */
    "x87 FPU Error (#MF)",             /* 16 */
    "Alignment Check (#AC)",           /* 17 */
    "Machine Check (#MC)",             /* 18 */
    "SIMD FPU Exception (#XM)",        /* 19 */

    /* Vectors 20-31: Reserved for future CPU extensions */
    "Reserved (future CPU extension)", /* 20 */
    "Reserved (future CPU extension)", /* 21 */
    "Reserved (future CPU extension)", /* 22 */
    "Reserved (future CPU extension)", /* 23 */
    "Reserved (future CPU extension)", /* 24 */
    "Reserved (future CPU extension)", /* 25 */
    "Reserved (future CPU extension)", /* 26 */
    "Reserved (future CPU extension)", /* 27 */
    "Reserved (future CPU extension)", /* 28 */
    "Reserved (future CPU extension)", /* 29 */
    "Reserved (future CPU extension)", /* 30 */
    "Reserved (future CPU extension)", /* 31 */
};

/* =============================================================================
 * idt_set_gate - Register an interrupt handler
 * =============================================================================
 * Using type-safe wrappers prevents accidentally swapping handler/type_attr/dpl.
 */
void idt_set_gate(uint8_t vector, handler_addr_t handler, type_attr_t type_attr, dpl_t dpl) {
    idt_table[vector].offset_low = handler.value & IDT_OFFSET_LOW_MASK;
    idt_table[vector].selector = GDT_SELECTOR_KERNEL_CODE; /* Kernel code segment */
    idt_table[vector].ist = 0;         /* IST = 0 (use current stack) */
    idt_table[vector].type_attr = type_attr.value | (dpl.value << 5);
    idt_table[vector].offset_middle = (handler.value >> IDT_OFFSET_MIDDLE_SHIFT) & IDT_OFFSET_MIDDLE_MASK;
    idt_table[vector].offset_high = (handler.value >> IDT_OFFSET_HIGH_SHIFT) & IDT_OFFSET_HIGH_MASK;
    idt_table[vector].reserved = 0;
}

/* =============================================================================
 * io_wait - Small delay for I/O operations
 * =============================================================================
 */
static inline void io_wait(void) {
    /* Port 0x80 is used for checkpoints during POST */
    __asm__ volatile("outb %%al, $0x80" ::"a"(0));
}

/* =============================================================================
 * pic_remap - Remap PIC IRQs to vectors 0x20-0x2F
 * =============================================================================
 *
 * The PIC by default maps IRQs to INT 0x08-0x0F (master) and 0x70-0x77 (slave).
 * This conflicts with CPU exceptions (INT 0x00-0x1F).
 *
 * We remap to:
 *   - Master PIC: IRQ 0-7  -> INT 0x20-0x27
 *   - Slave PIC:  IRQ 8-15 -> INT 0x28-0x2F
 * =============================================================================
 */
void pic_remap(void) {
    /* Save current masks */
    uint8_t mask1 = pic_read_data(io_port(PIC1_DATA));
    uint8_t mask2 = pic_read_data(io_port(PIC2_DATA));

    /* ICW1: Start initialization */
    pic_send_command(io_port(PIC1_COMMAND), ICW1_INIT | ICW1_ICW4);
    io_wait();
    pic_send_command(io_port(PIC2_COMMAND), ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* ICW2: Set vector offsets */
    pic_send_data(io_port(PIC1_DATA), io_data(PIC1_OFFSET)); /* 0x20 */
    io_wait();
    pic_send_data(io_port(PIC2_DATA), io_data(PIC2_OFFSET)); /* 0x28 */
    io_wait();

    /* ICW3: Configure cascading */
    pic_send_data(io_port(PIC1_DATA), io_data(ICW3_MASTER_SLAVE_ON_IRQ2)); /* Tell master: slave on IRQ2 */
    io_wait();
    pic_send_data(io_port(PIC2_DATA), io_data(ICW3_SLAVE_CASCADE_IDENTITY)); /* Tell slave: cascade identity */
    io_wait();

    /* ICW4: Set 8086 mode */
    pic_send_data(io_port(PIC1_DATA), io_data(ICW4_8086));
    io_wait();
    pic_send_data(io_port(PIC2_DATA), io_data(ICW4_8086));
    io_wait();

    /* Restore masks */
    pic_send_data(io_port(PIC1_DATA), io_data(mask1));
    pic_send_data(io_port(PIC2_DATA), io_data(mask2));
}

/* =============================================================================
 * pic_send_eoi - Send End of Interrupt to PIC
 * =============================================================================
 */
void pic_send_eoi(uint8_t irq) {
    /* If IRQ >= 8, send EOI to both PICs */
    if (irq >= 8) {
        pic_send_command(io_port(PIC2_COMMAND), PIC_EOI);
    }
    /* Always send EOI to master */
    pic_send_command(io_port(PIC1_COMMAND), PIC_EOI);
}

/* =============================================================================
 * pic_disable - Disable PIC interrupts
 * =============================================================================
 */
void pic_disable(void) {
    pic_send_data(io_port(PIC1_DATA), io_data(PIC_MASK_ALL_IRQS)); /* Mask all IRQs on master */
    pic_send_data(io_port(PIC2_DATA), io_data(PIC_MASK_ALL_IRQS)); /* Mask all IRQs on slave */
}

/* =============================================================================
 * interrupts_enable - Enable interrupts (STI)
 * =============================================================================
 */
void interrupts_enable(void) {
    __asm__ volatile("sti");
}

/* =============================================================================
 * interrupts_disable - Disable interrupts (CLI)
 * =============================================================================
 */
void interrupts_disable(void) {
    __asm__ volatile("cli");
}

/* =============================================================================
 * interrupts_are_enabled - Check if interrupts are enabled
 * =============================================================================
 */
int interrupts_are_enabled(void) {
    uint64_t rflags;
    __asm__ volatile("pushfq; pop %0" : "=r"(rflags));
    return (rflags & RFLAGS_IF_BIT) != 0; /* Check IF bit */
}

/* =============================================================================
 * default_exception_handler - Handle CPU exceptions
 * =============================================================================
 */
void default_exception_handler(interrupt_frame_t* frame) {
    /* Disable interrupts to prevent further exceptions */
    interrupts_disable();

    /* Output to serial */
    serial_write_str("\r\n\r\n!!! EXCEPTION !!!\r\n");

    if (frame->int_num < EXCEPTION_MESSAGE_COUNT) {
        serial_write_str(exception_messages[frame->int_num]);
    } else {
        serial_write_str("Unknown Exception");
    }

    serial_write_str("\r\n");

    /* Output error code if present */
    if (frame->err_code != 0) {
        serial_write_str("Error Code: 0x");
        serial_write_hex((uint32_t) frame->err_code);
        serial_write_str("\r\n");
    }

    /* Output register state */
    serial_write_str("RIP: 0x");
    serial_write_hex64(frame->rip);
    serial_write_str("  CS: 0x");
    serial_write_hex((uint32_t) frame->cs);
    serial_write_str("  RFLAGS: 0x");
    serial_write_hex64(frame->rflags);
    serial_write_str("\r\n");

    serial_write_str("RSP: 0x");
    serial_write_hex64(frame->rsp);
    serial_write_str("  SS: 0x");
    serial_write_hex((uint32_t) frame->ss);
    serial_write_str("\r\n");

    serial_write_str("RAX: 0x");
    serial_write_hex64(frame->rax);
    serial_write_str("  RBX: 0x");
    serial_write_hex64(frame->rbx);
    serial_write_str("  RCX: 0x");
    serial_write_hex64(frame->rcx);
    serial_write_str("\r\n");

    serial_write_str("RDX: 0x");
    serial_write_hex64(frame->rdx);
    serial_write_str("  RSI: 0x");
    serial_write_hex64(frame->rsi);
    serial_write_str("  RDI: 0x");
    serial_write_hex64(frame->rdi);
    serial_write_str("\r\n");

    /* Output to VGA if available */
    if (print_is_initialized() != 0) {
        print_set_color(PRINT_COLOR_WHITE, PRINT_COLOR_RED);
        print_str("\r\n\r\n!!! EXCEPTION !!!\r\n");

        if (frame->int_num < EXCEPTION_MESSAGE_COUNT) {
            print_str(exception_messages[frame->int_num]);
        } else {
            print_str("Unknown Exception");
        }

        print_str("\r\nError Code: 0x");
        print_hex((uint32_t) frame->err_code);
        print_str("\r\nRIP: 0x");
        print_hex64(frame->rip);
        print_str("\r\n");
    }

    /* Halt */
    for (;;) {
        __asm__ volatile("hlt");
    }
}

/* =============================================================================
 * default_irq_handler - Default hardware IRQ handler
 * =============================================================================
 */
void default_irq_handler(interrupt_frame_t* frame) {
    /* Send EOI to PIC */
    uint8_t irq = (uint8_t) (frame->int_num - IRQ_PIT);
    pic_send_eoi(irq);
}

/* =============================================================================
 * idt_init - Initialize Interrupt Descriptor Table
 * =============================================================================
 */
void idt_init(void) {
    /* Clear IDT table */
    for (size_t i = 0; i < IDT_ENTRIES; i++) {
        idt_table[i].offset_low = 0;
        idt_table[i].selector = 0;
        idt_table[i].ist = 0;
        idt_table[i].type_attr = 0;
        idt_table[i].offset_middle = 0;
        idt_table[i].offset_high = 0;
        idt_table[i].reserved = 0;
    }

    /* Remap PIC */
    pic_remap();

    /* ==========================================================
     * Register CPU Exception Handlers (INT 0-31)
     * ==========================================================
     * All vectors 0-31 now have handlers. Reserved vectors use
     * stub handlers that will trigger a panic with diagnostic info.
     * ==========================================================
     */

    /* Exceptions without error code (vectors 0-7) */
    idt_set_gate(0, handler_addr((uint64_t) isr0), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(1, handler_addr((uint64_t) isr1), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(2, handler_addr((uint64_t) isr2), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(3, handler_addr((uint64_t) isr3), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(4, handler_addr((uint64_t) isr4), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(5, handler_addr((uint64_t) isr5), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(6, handler_addr((uint64_t) isr6), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(7, handler_addr((uint64_t) isr7), type_attr(IDT_INTERRUPT_GATE), dpl(0));

    /* Exceptions with error code (vectors 8-14) */
    idt_set_gate(8, handler_addr((uint64_t) isr8), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(9, handler_addr((uint64_t) isr9), type_attr(IDT_INTERRUPT_GATE), dpl(0));   /* Reserved */
    idt_set_gate(10, handler_addr((uint64_t) isr10), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(11, handler_addr((uint64_t) isr11), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(12, handler_addr((uint64_t) isr12), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(13, handler_addr((uint64_t) isr13), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(14, handler_addr((uint64_t) isr14), type_attr(IDT_INTERRUPT_GATE), dpl(0));

    /* Reserved and other exceptions (vectors 15-19) */
    idt_set_gate(15, handler_addr((uint64_t) isr15), type_attr(IDT_INTERRUPT_GATE), dpl(0));  /* Reserved */
    idt_set_gate(16, handler_addr((uint64_t) isr16), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(17, handler_addr((uint64_t) isr17), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(18, handler_addr((uint64_t) isr18), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(19, handler_addr((uint64_t) isr19), type_attr(IDT_INTERRUPT_GATE), dpl(0));

    /* Reserved vectors 20-31 (future CPU extensions) */
    idt_set_gate(20, handler_addr((uint64_t) isr20), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(21, handler_addr((uint64_t) isr21), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(22, handler_addr((uint64_t) isr22), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(23, handler_addr((uint64_t) isr23), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(24, handler_addr((uint64_t) isr24), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(25, handler_addr((uint64_t) isr25), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(26, handler_addr((uint64_t) isr26), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(27, handler_addr((uint64_t) isr27), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(28, handler_addr((uint64_t) isr28), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(29, handler_addr((uint64_t) isr29), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(30, handler_addr((uint64_t) isr30), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(31, handler_addr((uint64_t) isr31), type_attr(IDT_INTERRUPT_GATE), dpl(0));

    /* ==========================================================
     * Register Hardware IRQ Handlers (INT 0x20-0x2F)
     * Note: These are now registered by irq_init() in irq.cpp
     * This code is kept for reference but commented out
     * ==========================================================
     */
    /* IRQ handlers are now registered by irq_init() */
    /*
    idt_set_gate(0x20, handler_addr((uint64_t) irq0_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(0x21, handler_addr((uint64_t) irq1_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(0x22, handler_addr((uint64_t) irq2_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(0x23, handler_addr((uint64_t) irq3_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(0x24, handler_addr((uint64_t) irq4_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(0x25, handler_addr((uint64_t) irq5_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(0x26, handler_addr((uint64_t) irq6_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(0x27, handler_addr((uint64_t) irq7_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(0x28, handler_addr((uint64_t) irq8_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(0x29, handler_addr((uint64_t) irq9_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(0x2A, handler_addr((uint64_t) irq10_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(0x2B, handler_addr((uint64_t) irq11_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(0x2C, handler_addr((uint64_t) irq12_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(0x2D, handler_addr((uint64_t) irq13_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(0x2E, handler_addr((uint64_t) irq14_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(0x2F, handler_addr((uint64_t) irq15_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    */

    /* Set up IDT pointer */
    idt_pointer.limit = sizeof(idt_table) - 1;
    idt_pointer.base = (uint64_t) idt_table;

    /* Load IDT using LIDT instruction */
    idt_load(&idt_pointer);
}
