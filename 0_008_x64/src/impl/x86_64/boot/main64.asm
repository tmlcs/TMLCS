global long_mode_start
extern kernel_main
extern __bss_start
extern __bss_end

section .text
bits 64
long_mode_start:
    ; ==========================================
    ; CRITICAL: Initialize segment registers FIRST
    ; ==========================================
    ; Must be done before any memory access to ensure
    ; proper 64-bit segment descriptors are loaded.
    ; ==========================================
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; ==========================================
    ; CRITICAL: Zero BSS section BEFORE kernel_main
    ; ==========================================
    ; SECURITY GUARANTEE: No code between entering long_mode_start
    ; and this point accesses BSS variables. This is safe because:
    ;   1. 32-bit boot code (main.asm) doesn't access any BSS variables
    ;   2. We initialize BSS immediately after setting up segments
    ;   3. kernel_main() is called ONLY after BSS is zeroed
    ;
    ; This ensures C++ global variables without explicit initializers
    ; (e.g., static uint32_t x;) are guaranteed to be zero.
    ;
    ; .boot.data separation: Page tables and stack are in .boot.data
    ; which is NOBITS (not in file) but NOT zeroed by this code.
    ; Only .bss section (C++ globals) gets zeroed here.
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

    ; ==========================================
    ; SAFE: Now call kernel_main
    ; ==========================================
    ; BSS is guaranteed zeroed at this point.
    ; Any C++ global variable access in kernel_main() is safe.
    ; ==========================================
    call kernel_main
    hlt

; ==========================================
; .note.GNU-stack section to eliminate linker warning
; ==========================================
section .note.GNU-stack noexec
