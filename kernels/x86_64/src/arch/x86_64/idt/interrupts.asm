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
global isr8, isr9, isr10, isr11, isr12, isr13, isr14
global isr15, isr16, isr17, isr18, isr19
global isr20, isr21, isr22, isr23, isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31

; IRQ handlers (exported to C) - Match irq.h names
global irq0_stub, irq1_stub, irq2_stub, irq3_stub, irq4_stub, irq5_stub, irq6_stub, irq7_stub
global irq8_stub, irq9_stub, irq10_stub, irq11_stub, irq12_stub, irq13_stub, irq14_stub, irq15_stub

; External C handlers
extern default_exception_handler
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
    
    ; Remove interrupt number and error code from stack
    add rsp, 16
    
    ; Return from interrupt
    iretq
%endmacro

; =============================================================================
; CPU Exception Handlers (INT 0-31)
; =============================================================================
;
; All vectors 0-31 now have handlers. Reserved vectors use stub handlers
; that trigger a panic with appropriate error message.
;
; Exception vectors:
;   0-19: CPU exceptions (some reserved)
;   20-31: Reserved for future CPU extensions
; =============================================================================

; -----------------------------------------------------------------------------
; CPU Exceptions (0-19) - Standard handlers
; -----------------------------------------------------------------------------

; Exceptions without error code
ISR_NOERR 0     ; Divide Error (#DE)
ISR_NOERR 1     ; Debug (#DB)
ISR_NOERR 2     ; Non-Maskable Interrupt (NMI)
ISR_NOERR 3     ; Breakpoint (#BP)
ISR_NOERR 4     ; Overflow (#OF)
ISR_NOERR 5     ; Bound Range Exceeded (#BR)
ISR_NOERR 6     ; Invalid Opcode (#UD)
ISR_NOERR 7     ; Device Not Available (#NM)

; Exceptions with error code
ISR_ERR 8       ; Double Fault (#DF)

; -----------------------------------------------------------------------------
; Reserved Vector Handlers (9, 15, 20-31)
; -----------------------------------------------------------------------------
; These vectors are reserved by Intel/AMD. If triggered, they indicate:
;   - Hardware bug
;   - Software corruption
;   - Future CPU extensions
; All reserved vectors use stub handlers that panic with diagnostic info.
; -----------------------------------------------------------------------------

ISR_NOERR 9     ; Reserved (Intel/AMD) - Stub handler

ISR_ERR 10      ; Invalid TSS (#TS)
ISR_ERR 11      ; Segment Not Present (#NP)
ISR_ERR 12      ; Stack Fault (#SS)
ISR_ERR 13      ; General Protection Fault (#GP)
ISR_ERR 14      ; Page Fault (#PF)

ISR_NOERR 15    ; Reserved (Intel/AMD) - Stub handler

; Other exceptions (no error code)
ISR_NOERR 16    ; x87 FPU Error (#MF)
ISR_ERR 17      ; Alignment Check (#AC)
ISR_NOERR 18    ; Machine Check (#MC)
ISR_NOERR 19    ; SIMD FPU Exception (#XM)

; -----------------------------------------------------------------------------
; Reserved Vectors 20-31 - Stub handlers for future CPU extensions
; -----------------------------------------------------------------------------
; Intel SDM Volume 3A, Section 6.9:
; "Vectors 20 through 31 are reserved for future expansion."
; These handlers prevent triple fault if a reserved vector is triggered.
; -----------------------------------------------------------------------------

ISR_NOERR 20    ; Reserved (future CPU extension)
ISR_NOERR 21    ; Reserved (future CPU extension)
ISR_NOERR 22    ; Reserved (future CPU extension)
ISR_NOERR 23    ; Reserved (future CPU extension)
ISR_NOERR 24    ; Reserved (future CPU extension)
ISR_NOERR 25    ; Reserved (future CPU extension)
ISR_NOERR 26    ; Reserved (future CPU extension)
ISR_NOERR 27    ; Reserved (future CPU extension)
ISR_NOERR 28    ; Reserved (future CPU extension)
ISR_NOERR 29    ; Reserved (future CPU extension)
ISR_NOERR 30    ; Reserved (future CPU extension)
ISR_NOERR 31    ; Reserved (future CPU extension)

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

    ; Call irq_dispatch with IRQ number.
    ; mov edi (32-bit) zero-extends into the full RDI register on x86_64,
    ; ensuring bits 8-63 are clean — correct per System V AMD64 ABI.
    mov edi, %1           ; First argument: IRQ number (zero-extended into RDI)
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
