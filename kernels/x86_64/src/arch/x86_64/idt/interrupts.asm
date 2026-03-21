; =============================================================================
; IDT Assembly Functions and Interrupt Service Routines
; =============================================================================
;
; This file provides:
;   1. idt_load() - Load IDT using LIDT instruction
;   2. ISR stubs for CPU exceptions (INT 0-31)
;   3. IRQ stubs for hardware interrupts (INT 32-47)
;
; All ISRs follow the System V AMD64 ABI calling convention.
; =============================================================================

global idt_load

; Exception handlers (exported to C)
global isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
global isr8, isr10, isr11, isr12, isr13, isr14
global isr16, isr17, isr18, isr19

; IRQ handlers (exported to C) - Match irq.h names
global irq0_stub, irq1_stub, irq2_stub, irq3_stub, irq4_stub, irq5_stub, irq6_stub, irq7_stub
global irq8_stub, irq9_stub, irq10_stub, irq11_stub, irq12_stub, irq13_stub, irq14_stub, irq15_stub

; External C handlers
extern default_exception_handler
extern default_irq_handler
extern irq_dispatch

section .text
bits 64

; =============================================================================
; idt_load - Load IDT using LIDT instruction
; =============================================================================
; Parameters:
;   RDI: Pointer to idt_pointer_t structure
;
; C prototype:
;   void idt_load(idt_pointer_t* idtp);
; =============================================================================
idt_load:
    lidt [rdi]              ; Load IDTR from memory location pointed by RDI
    ret

; =============================================================================
; pushaq / popaq - Save/Restore all general-purpose registers
; =============================================================================
; These macros save/restore registers in the order expected by
; interrupt_frame_t structure in idt.h
;
; MUST be defined before ISR/IRQ macros that use them
; =============================================================================

%macro pushaq 0
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro popaq 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

; =============================================================================
; ISR Macro - Interrupt Service Routine stub
; =============================================================================
; Creates an ISR stub that:
;   1. Pushes error code (or 0 if none)
;   2. Pushing interrupt number
;   3. Saves all general-purpose registers
;   4. Calls C handler
;   5. Restores registers
;   6. Returns with IRETQ
;
; Parameters:
;   %1: ISR number
;   %2: 1 if pushes error code, 0 otherwise
; =============================================================================

%macro ISR_NOERR 1
isr%1:
    ; Push 0 for error code (not provided by CPU for this exception)
    push 0
    
    ; Push interrupt number
    push %1
    
    ; Save all registers
    pushaq
    
    ; Call C handler (RDI = pointer to interrupt_frame_t)
    mov rdi, rsp
    call default_exception_handler
    
    ; Restore all registers
    popaq
    
    ; Remove interrupt number and error code from stack
    add rsp, 16
    
    ; Return from interrupt
    iretq
%endmacro

%macro ISR_ERR 1
isr%1:
    ; Error code already pushed by CPU
    ; Push interrupt number
    push %1
    
    ; Save all registers
    pushaq
    
    ; Call C handler
    mov rdi, rsp
    call default_exception_handler
    
    ; Restore all registers
    popaq
    
    ; Remove interrupt number from stack (error code removed by IRETQ)
    add rsp, 8
    
    ; Return from interrupt
    iretq
%endmacro

; =============================================================================
; CPU Exception Handlers (INT 0-31)
; =============================================================================

; Exceptions without error code
ISR_NOERR 0     ; Divide Error
ISR_NOERR 1     ; Debug
ISR_NOERR 2     ; NMI
ISR_NOERR 3     ; Breakpoint
ISR_NOERR 4     ; Overflow
ISR_NOERR 5     ; Bound Range
ISR_NOERR 6     ; Invalid Opcode
ISR_NOERR 7     ; Device Not Available

; Exceptions with error code
ISR_ERR 8       ; Double Fault
; ISR 9 doesn't exist (reserved)
ISR_ERR 10      ; Invalid TSS
ISR_ERR 11      ; Segment Not Present
ISR_ERR 12      ; Stack Fault
ISR_ERR 13      ; General Protection Fault
ISR_ERR 14      ; Page Fault

; ISR 15 doesn't exist (reserved)

; Other exceptions (no error code)
ISR_NOERR 16    ; x87 FPU Error
ISR_ERR 17      ; Alignment Check
ISR_NOERR 18    ; Machine Check
ISR_NOERR 19    ; SIMD FPU Exception

; =============================================================================
; IRQ Macro - Hardware Interrupt Request stub
; =============================================================================
; Creates an IRQ stub that:
;   1. Pushes error code (0)
;   2. Pushes interrupt number
;   3. Saves all general-purpose registers
;   4. Calls C handler
;   5. Restores registers
;   6. Returns with IRETQ
;
; Parameters:
;   %1: IRQ number (0-15)
;   %2: Vector number (0x20-0x2F)
; =============================================================================

%macro IRQ 2
irq%1_stub:
    ; Push 0 for error code
    push 0

    ; Push interrupt number
    push %2

    ; Save all registers
    pushaq

    ; Call irq_dispatch with IRQ number
    mov dil, %1           ; First argument: IRQ number (8-bit)
    call irq_dispatch

    ; Restore all registers
    popaq

    ; Remove interrupt number and error code from stack
    add rsp, 16

    ; Return from interrupt
    iretq
%endmacro

; =============================================================================
; Hardware IRQ Handlers (INT 0x20-0x2F)
; =============================================================================

IRQ 0,  0x20  ; PIT
IRQ 1,  0x21  ; Keyboard
IRQ 2,  0x22  ; Cascade
IRQ 3,  0x23  ; COM2
IRQ 4,  0x24  ; COM1
IRQ 5,  0x25  ; LPT2
IRQ 6,  0x26  ; Floppy
IRQ 7,  0x27  ; LPT1
IRQ 8,  0x28  ; RTC
IRQ 9,  0x29  ; ACPI
IRQ 10, 0x2A  ; Available
IRQ 11, 0x2B  ; Free
IRQ 12, 0x2C  ; Mouse
IRQ 13, 0x2D  ; Coprocessor
IRQ 14, 0x2E  ; Primary ATA
IRQ 15, 0x2F  ; Secondary ATA

; =============================================================================
; .note.GNU-stack section to eliminate linker warning
; =============================================================================
section .note.GNU-stack noexec

section .rodata
bits 64
