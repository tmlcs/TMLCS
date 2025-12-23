#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>
#include <types.h>

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

extern "C" void InterruptHandler(InterruptStackFrame* stack_frame);

#endif // INTERRUPTS_H
