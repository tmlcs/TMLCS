; interrupt_stubs.asm
; Assembly stubs for CPU exceptions

%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    ; Push dummy error code if not provided by CPU
    push byte 0
    ; Push interrupt number
    push byte %1
    ; Jump to common handler
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    ; Error code is already on stack
    ; Push interrupt number
    push byte %1
    ; Jump to common handler
    jmp isr_common_stub
%endmacro

; Exceptions with no error code
ISR_NOERRCODE 0  ; Divide by Zero Exception
ISR_NOERRCODE 1  ; Debug Exception
ISR_NOERRCODE 2  ; Non Maskable Interrupt Exception
ISR_NOERRCODE 3  ; Breakpoint Exception
ISR_NOERRCODE 4  ; Overflow Exception
ISR_NOERRCODE 5  ; Bound Range Exceeded Exception
ISR_NOERRCODE 6  ; Invalid Opcode Exception
ISR_NOERRCODE 7  ; Device Not Available Exception
ISR_NOERRCODE 8  ; Double Fault Exception (has error code, but we push dummy for consistency)
ISR_NOERRCODE 9  ; Coprocessor Segment Overrun (reserved)
ISR_NOERRCODE 10 ; Invalid TSS Exception (has error code)
ISR_NOERRCODE 11 ; Segment Not Present Exception (has error code)
ISR_NOERRCODE 12 ; Stack-Fault Exception (has error code)
ISR_NOERRCODE 13 ; General Protection Fault Exception (has error code)
ISR_NOERRCODE 14 ; Page Fault Exception (has error code)
ISR_NOERRCODE 15 ; Reserved
ISR_NOERRCODE 16 ; x87 FPU Floating-Point Error
ISR_NOERRCODE 17 ; Alignment Check Exception
ISR_NOERRCODE 18 ; Machine Check Exception
ISR_NOERRCODE 19 ; SIMD Floating-Point Exception

; Hardware Interrupts (IRQs) - remapped to 0x20-0x2F
ISR_NOERRCODE 0x20 ; IRQ0 - Timer
ISR_NOERRCODE 0x21 ; IRQ1 - Keyboard
ISR_NOERRCODE 0x22 ; IRQ2 - Cascade
ISR_NOERRCODE 0x23 ; IRQ3 - COM2
ISR_NOERRCODE 0x24 ; IRQ4 - COM1
ISR_NOERRCODE 0x25 ; IRQ5 - LPT2
ISR_NOERRCODE 0x26 ; IRQ6 - Floppy
ISR_NOERRCODE 0x27 ; IRQ7 - LPT1
ISR_NOERRCODE 0x28 ; IRQ8 - RTC
ISR_NOERRCODE 0x29 ; IRQ9 - Redirected IRQ2
ISR_NOERRCODE 0x2A ; IRQ10 - Reserved
ISR_NOERRCODE 0x2B ; IRQ11 - Reserved
ISR_NOERRCODE 0x2C ; IRQ12 - PS/2 Mouse
ISR_NOERRCODE 0x2D ; IRQ13 - FPU
ISR_NOERRCODE 0x2E ; IRQ14 - Primary ATA
ISR_NOERRCODE 0x2F ; IRQ15 - Secondary ATA

; Common stub for all ISRs
global isr_common_stub
isr_common_stub:
    ; Save registers
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

    ; Call C++ interrupt handler
    ; The stack now contains:
    ;   R15
    ;   ...
    ;   RAX
    ;   Interrupt Number
    ;   Error Code (or dummy 0)
    ;   EIP
    ;   CS
    ;   EFLAGS
    ;   ESP
    ;   SS
    ; We need to pass the stack pointer to the C++ handler
    mov rdi, rsp ; Pass pointer to stack frame as first argument

    ; extern "C" void InterruptHandler(InterruptStackFrame* stack_frame);
    extern InterruptHandler
    call InterruptHandler

    ; Restore registers
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

    ; Pop interrupt number and error code
    add rsp, 16 ; Pop interrupt number (8 bytes) and error code (8 bytes)

    ; Return from interrupt
    iretq
