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
    add rcx, 7            ; Ceiling division: round up to next qword boundary
    shr rcx, 3            ; Convert bytes to qwords (rounded up)
    xor rax, rax          ; Zero
    rep stosq             ; Fill with zeros
.bss_done:

    ; ==========================================
    ; CRITICAL: Ensure 16-byte stack alignment
    ; ==========================================
    ; System V AMD64 ABI requires 16-byte stack alignment
    ; before CALL instructions. This ensures:
    ;   - SSE instructions (movaps, etc.) work correctly
    ;   - C++ runtime can assume proper alignment
    ;   - Compiler optimizations that assume alignment are safe
    ;
    ; Stack alignment algorithm:
    ;   RSP mod 16 should be 8 before CALL (return push makes it 0)
    ;   AND RSP, ~0xF aligns to 16 bytes (rounds down)
    ; ==========================================
    and rsp, ~0xF         ; Align stack to 16-byte boundary

    ; ==========================================
    ; SAFE: Now call kernel_main
    ; ==========================================
    ; BSS is guaranteed zeroed at this point.
    ; Stack is guaranteed 16-byte aligned.
    ; Any C++ global variable access in kernel_main() is safe.
    ; ==========================================
    call kernel_main
    hlt

; ==========================================
; .note.GNU-stack section to eliminate linker warning
; ==========================================
section .note.GNU-stack noexec
