#include "cpu/interrupts.h"
#include "utils/log.h"
#include "print.h"
#include "cpu/pic.h" // For PIC::send_eoi
#include "memory/pmm.h" // For PMM::allocate_page
#include "memory/vmm.h" // For VMM::map_page

// Define a type for interrupt handler functions
typedef void (*InterruptHandlerFunction)(InterruptStackFrame* stack_frame);

// Array of interrupt handler functions
static InterruptHandlerFunction interrupt_handlers[256];

// Default interrupt handler
void DefaultInterruptHandler(InterruptStackFrame* stack_frame) {
    Log::panic("UNHANDLED INTERRUPT! Interrupt Number: %d, Error Code: %x", stack_frame->interrupt_number, stack_frame->error_code);
}

// Divide-by-zero Exception Handler
void DivideByZeroHandler(InterruptStackFrame* stack_frame) {
    Log::panic("DIVIDE BY ZERO EXCEPTION! RIP: %x", stack_frame->rip);
}

// General Protection Fault Handler
void GeneralProtectionFaultHandler(InterruptStackFrame* stack_frame) {
    Log::panic("GENERAL PROTECTION FAULT! Error Code: %x, RIP: %x", stack_frame->error_code, stack_frame->rip);
}

// Page Fault Handler
void PageFaultHandler(InterruptStackFrame* stack_frame) {
    uint64_t cr2;
    asm volatile ("mov %%cr2, %0" : "=r" (cr2));

    // Check if the page fault was due to a non-present page
    if (!(stack_frame->error_code & PF_PRESENT)) {
        // Page not present, attempt to allocate and map a new page
        uint64_t physical_page = PMM::allocate_page();
        if (physical_page == 0) {
            Log::panic("PAGE FAULT: Out of physical memory! Virtual Address: %x, Error Code: %x, RIP: %x", cr2, stack_frame->error_code, stack_frame->rip);
        }

        // Determine appropriate flags for mapping
        uint64_t flags = VMM::PAGE_PRESENT;
        if (stack_frame->error_code & PF_WRITE) {
            flags |= VMM::PAGE_WRITE;
        }
        // If the fault occurred in user mode, set the USER flag
        if (stack_frame->error_code & PF_USER) {
            flags |= VMM::PAGE_USER;
        }

        VMM::map_page(cr2, physical_page, flags);
        Log::info("PAGE FAULT: Handled by mapping new page. Virtual Address: %x, Physical Address: %x, Error Code: %x, RIP: %x", cr2, physical_page, stack_frame->error_code, stack_frame->rip);
        return; // Page fault handled
    }

    // If we reach here, it's a different type of page fault or an unrecoverable one
    Log::panic("PAGE FAULT! Virtual Address: %x, Error Code: %x, RIP: %x", cr2, stack_frame->error_code, stack_frame->rip);
}

namespace Interrupts {
    void RegisterInterruptHandler(uint8_t interrupt_number, InterruptHandlerFunction handler) {
        interrupt_handlers[interrupt_number] = handler;
    }

    void init() {
        // Initialize all handlers to the default handler
        for (int i = 0; i < 256; ++i) {
            interrupt_handlers[i] = DefaultInterruptHandler;
        }
        
        // Register specific exception handlers
        RegisterInterruptHandler(0x0, DivideByZeroHandler); // Divide-by-zero
        RegisterInterruptHandler(0xD, GeneralProtectionFaultHandler); // General Protection Fault
        RegisterInterruptHandler(0xE, PageFaultHandler); // Page Fault

        Log::info("Interrupt handlers initialized.");
    }
}

extern "C" void InterruptHandler(InterruptStackFrame* stack_frame) {
    // Call the registered handler for this interrupt number
    if (interrupt_handlers[stack_frame->interrupt_number]) {
        interrupt_handlers[stack_frame->interrupt_number](stack_frame);
    } else {
        // Should not happen if all handlers are initialized to DefaultInterruptHandler
        DefaultInterruptHandler(stack_frame);
    }

    // If it's a hardware IRQ (0x20-0x2F), send EOI
    if (stack_frame->interrupt_number >= 0x20 && stack_frame->interrupt_number <= 0x2F) {
        PIC::send_eoi(stack_frame->interrupt_number);
    }
}
