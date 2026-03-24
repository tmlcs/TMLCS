#include "irq.h"
#include "idt.h"
#include "serial.h"
#include "print.h"
#include "barriers.h"
#include "constants.h"
#include "atomic.h"
#include "io.h"

/* =============================================================================
 * IRQ System State
 * =============================================================================
 */
static int g_irq_initialized = 0;
static irq_handler_t g_irq_handlers[IRQ_COUNT];
static uint32_t g_irq_counts[IRQ_COUNT];
static uint16_t g_irq_mask = 0;  /* Bitmask of enabled IRQs */

/* CRIT-002 FIX: IRQ stack depth tracking to prevent stack overflow */
static volatile uint32_t g_irq_stack_depth = 0;
static constexpr uint32_t MAX_IRQ_DEPTH = 8;  /* Maximum nested IRQ depth */

/* =============================================================================
 * PIC Initialization
 * =============================================================================
 */

/**
 * @brief Remap PIC IRQs to IDT entries 32-47
 * 
 * The PIC by default uses IRQ0-15 mapped to IDT 0-15.
 * This conflicts with CPU exceptions (0-31).
 * We remap to IDT 32-47 (0x20-0x2F).
 */
static void pic_remap_impl(void) {
    /* Save current masks */
    uint8_t mask1 = inb(PIC1_DATA);
    io_delay();
    uint8_t mask2 = inb(PIC2_DATA);
    io_delay();

    /* ICW1: Start initialization */
    outb(PIC1_COMMAND, 0x11);
    io_delay();
    outb(PIC2_COMMAND, 0x11);
    io_delay();

    /* ICW2: Set vector offsets */
    outb(PIC1_DATA, IRQ_BASE_MASTER);  /* IRQ0-7 -> IDT 32-39 */
    io_delay();
    outb(PIC2_DATA, IRQ_BASE_SLAVE);   /* IRQ8-15 -> IDT 40-47 */
    io_delay();

    /* ICW3: Configure cascade */
    outb(PIC1_DATA, 0x04);  /* Tell master: slave is at IRQ2 */
    io_delay();
    outb(PIC2_DATA, 0x02);  /* Tell slave: cascade identity */
    io_delay();

    /* ICW4: Set mode */
    outb(PIC1_DATA, 0x01);  /* 8086 mode */
    io_delay();
    outb(PIC2_DATA, 0x01);  /* 8086 mode */
    io_delay();

    /* Restore masks */
    outb(PIC1_DATA, mask1);
    io_delay();
    outb(PIC2_DATA, mask2);
    io_delay();

    serial_write_str("[IRQ] PIC remapped to IDT 32-47\r\n");
}

/**
 * @brief Initialize IRQ masks (disable all initially)
 */
static void irq_masks_init(void) {
    /* Initially disable all IRQs */
    g_irq_mask = 0xFFFF;
    outb(PIC1_DATA, 0xFF);
    io_delay();
    outb(PIC2_DATA, 0xFF);
    io_delay();
}

/* =============================================================================
 * Public API - Initialization
 * =============================================================================
 */

int irq_init(void) {
    serial_write_str("[IRQ] Initializing IRQ system...\r\n");

    /* Initialize state */
    g_irq_initialized = 0;
    for (int i = 0; i < IRQ_COUNT; i++) {
        g_irq_handlers[i] = nullptr;
        g_irq_counts[i] = 0;
    }

    /* Remap PIC */
    pic_remap_impl();

    /* Initialize masks (all disabled) */
    irq_masks_init();

    /* Register IRQ stubs in IDT */
    /* Master PIC */
    idt_set_gate(32, handler_addr((uint64_t)irq0_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(33, handler_addr((uint64_t)irq1_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(34, handler_addr((uint64_t)irq2_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(35, handler_addr((uint64_t)irq3_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(36, handler_addr((uint64_t)irq4_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(37, handler_addr((uint64_t)irq5_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(38, handler_addr((uint64_t)irq6_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(39, handler_addr((uint64_t)irq7_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));

    /* Slave PIC */
    idt_set_gate(40, handler_addr((uint64_t)irq8_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(41, handler_addr((uint64_t)irq9_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(42, handler_addr((uint64_t)irq10_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(43, handler_addr((uint64_t)irq11_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(44, handler_addr((uint64_t)irq12_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(45, handler_addr((uint64_t)irq13_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(46, handler_addr((uint64_t)irq14_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));
    idt_set_gate(47, handler_addr((uint64_t)irq15_stub), type_attr(IDT_INTERRUPT_GATE), dpl(0));

    serial_write_str("[IRQ] IRQ stubs registered in IDT\r\n");

    /* Mark as initialized */
    wmb();
    g_irq_initialized = 1;
    mb();

    serial_write_str("[IRQ] IRQ system initialized\r\n");

    return 1;
}

int irq_is_initialized(void) {
    rmb();
    return g_irq_initialized;
}

void irq_shutdown(void) {
    /* Disable all IRQs */
    irq_disable_all();
    
    /* Clear handlers */
    for (int i = 0; i < IRQ_COUNT; i++) {
        g_irq_handlers[i] = nullptr;
    }

    wmb();
    g_irq_initialized = 0;
    wmb();
}

/* =============================================================================
 * Handler Registration
 * =============================================================================
 */

int irq_register_handler(uint8_t irq, irq_handler_t handler) {
    if (irq >= IRQ_COUNT) {
        return 0;
    }

    if (handler == nullptr) {
        return 0;
    }

    wmb();
    g_irq_handlers[irq] = handler;
    wmb();

    return 1;
}

int irq_unregister_handler(uint8_t irq) {
    if (irq >= IRQ_COUNT) {
        return 0;
    }

    wmb();
    g_irq_handlers[irq] = nullptr;
    wmb();

    return 1;
}

irq_handler_t irq_get_handler(uint8_t irq) {
    if (irq >= IRQ_COUNT) {
        return nullptr;
    }

    rmb();
    return g_irq_handlers[irq];
}

/* =============================================================================
 * Mask Control
 * =============================================================================
 */

void irq_enable(uint8_t irq) {
    if (irq >= IRQ_COUNT) {
        return;
    }

    /* Update mask */
    g_irq_mask &= ~(1 << irq);

    /* Update PIC */
    if (irq < 8) {
        uint8_t mask = inb(PIC1_DATA);
        io_delay();
        mask &= ~(1 << irq);
        outb(PIC1_DATA, mask);
        io_delay();
    } else {
        uint8_t irq2 = irq - 8;
        uint8_t mask = inb(PIC2_DATA);
        io_delay();
        mask &= ~(1 << irq2);
        outb(PIC2_DATA, mask);
        io_delay();
    }
}

void irq_disable(uint8_t irq) {
    if (irq >= IRQ_COUNT) {
        return;
    }

    /* Update mask */
    g_irq_mask |= (1 << irq);

    /* Update PIC */
    if (irq < 8) {
        uint8_t mask = inb(PIC1_DATA);
        io_delay();
        mask |= (1 << irq);
        outb(PIC1_DATA, mask);
        io_delay();
    } else {
        uint8_t irq2 = irq - 8;
        uint8_t mask = inb(PIC2_DATA);
        io_delay();
        mask |= (1 << irq2);
        outb(PIC2_DATA, mask);
        io_delay();
    }
}

int irq_is_enabled(uint8_t irq) {
    if (irq >= IRQ_COUNT) {
        return 0;
    }

    rmb();
    return (g_irq_mask & (1 << irq)) == 0;
}

void irq_enable_all(void) {
    for (int i = 0; i < IRQ_COUNT; i++) {
        irq_enable(i);
    }
}

void irq_disable_all(void) {
    for (int i = 0; i < IRQ_COUNT; i++) {
        irq_disable(i);
    }
}

/* =============================================================================
 * Interrupt Handling
 * =============================================================================
 */

void irq_dispatch(uint8_t irq) {
    /* CRIT-002 FIX: Check for excessive nesting to prevent stack overflow */
    uint32_t current_depth = atomic_inc32(&g_irq_stack_depth);
    
    if (current_depth > MAX_IRQ_DEPTH) {
        /* Stack depth exceeded - log error and skip handler */
        serial_write_str("[IRQ] CRIT-002: Stack depth exceeded (");
        serial_write_dec(current_depth);
        serial_write_str(" > ");
        serial_write_dec(MAX_IRQ_DEPTH);
        serial_write_str(") on IRQ ");
        serial_write_dec(irq);
        serial_write_str("\r\n");
        
        /* Still send EOI to prevent IRQ lockout */
        irq_send_eoi(irq);
        atomic_dec32(&g_irq_stack_depth);
        return;
    }
    
    /* Increment counter */
    g_irq_counts[irq]++;

    /* Call handler if registered */
    irq_handler_t handler = g_irq_handlers[irq];
    if (handler != nullptr) {
        handler();
    }

    /* Send EOI */
    irq_send_eoi(irq);
    
    /* Decrement stack depth */
    atomic_dec32(&g_irq_stack_depth);
}

void irq_send_eoi(uint8_t irq) {
    /* Send EOI to master PIC */
    outb(PIC1_COMMAND, PIC_EOI);
    io_delay();

    /* If IRQ came from slave, also send EOI to slave */
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
        io_delay();
    }
}

/* =============================================================================
 * Debug/Testing
 * =============================================================================
 */

void irq_print_status(void) {
    serial_write_str("\r\n=== IRQ Status ===\r\n");
    serial_write_str("Initialized: ");
    serial_write_dec(g_irq_initialized);
    serial_write_str("\r\n");

    serial_write_str("Enabled IRQs: ");
    for (int i = 0; i < IRQ_COUNT; i++) {
        if (irq_is_enabled(i)) {
            serial_write_dec(i);
            serial_write_str(" ");
        }
    }
    serial_write_str("\r\n");

    serial_write_str("IRQ Counts:\r\n");
    for (int i = 0; i < IRQ_COUNT; i++) {
        serial_write_str("  IRQ");
        if (i < 10) serial_write_str(" ");
        serial_write_dec(i);
        serial_write_str(": ");
        serial_write_dec(g_irq_counts[i]);
        serial_write_str("\r\n");
    }

    serial_write_str("==================\r\n");
}

uint32_t irq_get_count(uint8_t irq) {
    if (irq >= IRQ_COUNT) {
        return 0;
    }

    rmb();
    return g_irq_counts[irq];
}

void irq_reset_counts(void) {
    wmb();
    for (int i = 0; i < IRQ_COUNT; i++) {
        g_irq_counts[i] = 0;
    }
    wmb();
}
