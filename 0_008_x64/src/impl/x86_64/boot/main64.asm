global long_mode_start
extern kernel_main

section .text
bits 64
long_mode_start:
    ; load null into all data segment registers
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; ==========================================
    ; Zero BSS section - DESHABILITADO
    ; ==========================================
    ; NOTA: La inicialización BSS está deshabilitada porque:
    ; 1. Las page tables y stack (main.asm) están en .bss
    ; 2. Inicializar BSS borraría las page tables configuradas
    ;
    ; Las variables C++ en este kernel tienen inicializador
    ; explícito (=0), así que van a .data y están inicializadas.
    ;
    ; PARA HABITAR EN EL FUTURO:
    ; - Mover page tables/stack a sección separada en linker.ld
    ; - O inicializar BSS antes de setup_page_tables
    ;
    ; Código de inicialización (requiere linker.ld actualizado):
    ;   extern __bss_start, __bss_end
    ;   mov rdi, __bss_start
    ;   mov rcx, __bss_end
    ;   sub rcx, rdi
    ;   test rcx, rcx
    ;   jz .bss_done
    ;   shr rcx, 3
    ;   xor rax, rax
    ;   rep stosq
    ; .bss_done:
    ; ==========================================

    call kernel_main
    hlt

; ==========================================
; Sección .note.GNU-stack para eliminar warning del linker
; ==========================================
section .note.GNU-stack noexec
