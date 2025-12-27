#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>
#include <types.h>

// Page Fault Error Code Flags
#define PF_PRESENT      (1 << 0) // Page not present
#define PF_WRITE        (1 << 1) // Write access
#define PF_USER         (1 << 2) // User mode access
#define PF_RESERVED     (1 << 3) // Overwritten CPU-reserved bit
#define PF_INSTR_FETCH  (1 << 4) // Instruction fetch

// Structure representing the stack frame when an interrupt occurs
struct InterruptStackFrame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8; // Pushed by common stub
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax; // Pushed by common stub
    uint64_t interrupt_number; // Pushed by ISR macro
    uint64_t error_code;       // Pushed by ISR macro (or dummy 0)
    uint64_t rip;              // Pushed by CPU
    uint64_t cs;               // Pushed by CPU
    uint64_t rflags;           // Pushed by CPU
    uint64_t rsp;              // Pushed by CPU
    uint64_t ss;               // Pushed by CPU
};

// Define a type for interrupt handler functions
typedef void (*InterruptHandlerFunction)(InterruptStackFrame* stack_frame);

namespace Interrupts {
    void RegisterInterruptHandler(uint8_t interrupt_number, InterruptHandlerFunction handler);
    void init();
}

extern "C" void InterruptHandler(InterruptStackFrame* stack_frame);

#endif // INTERRUPTS_H
