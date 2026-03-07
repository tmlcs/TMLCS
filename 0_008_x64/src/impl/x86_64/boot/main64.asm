global long_mode_start
extern kernel_main
extern __bss_start
extern __bss_end

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
    ; Zero BSS section - Habilitado
    ; ==========================================
    ; Ahora que .boot.data está separada, podemos inicializar
    ; el BSS a cero sin borrar las page tables o el stack.
    ; Esto es esencial para variables C++ globales sin inicializador.
    ; ==========================================
    lea rdi, [__bss_start]
    lea rcx, [__bss_end]
    sub rcx, rdi
    test rcx, rcx
    jz .bss_done          ; Skip if BSS is empty
    shr rcx, 3            ; Convert bytes to qwords
    xor rax, rax          ; Zero
    rep stosq             ; Fill with zeros
.bss_done:

    call kernel_main
    hlt

; ==========================================
; Sección .note.GNU-stack para eliminar warning del linker
; ==========================================
section .note.GNU-stack noexec
