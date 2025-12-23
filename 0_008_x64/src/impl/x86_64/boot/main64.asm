global long_mode_start
extern kernel_main
extern call_ctors

section .text
bits 64
long_mode_start:
    ; Multiboot magic number is in eax
    ; Multiboot info structure address is in ebx

    push rax ; Save eax (Multiboot magic number)
    push rbx ; Save ebx (Multiboot info pointer)

    ; load null into all data segment registers
    xor ax, ax
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call call_ctors

    pop rbx ; Restore ebx (Multiboot info pointer)
    pop rax ; Restore eax (Multiboot magic number)

    ; Pass Multiboot info to kernel_main
    ; rbx contains the address of the Multiboot information structure
    ; rax contains the Multiboot magic number
    mov rdi, rbx ; First argument (multiboot_structure)
    mov rsi, rax ; Second argument (magic_number)

	call kernel_main
    hlt
