; ==========================================
; Multiboot2 Header
; ==========================================
; Constants:
;   MULTIBOOT2_MAGIC = 0xE85250D6
;   MULTIBOOT2_ARCH  = 0 (protected mode i386)
;   MULTIBOOT2_HEADER_ALIGN = 8
; ==========================================

section .multiboot_header
header_start:
    ; magic number (identifies kernel as multiboot)
    dd 0xe85250d6

    ; architecture (protected mode in this case)
    dd 0

    ; header length
    dd header_end - header_start ; calculated as distance between header start and end

    ; checksum (calculated as sum of all bytes minus magic number)
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start))

    ; end tag (tag end: type=0, flags=0, size=8)
    dw 0
    dw 0
    dd 8

header_end:

; ==========================================
; .note.GNU-stack section to eliminate linker warning
; ==========================================
section .note.GNU-stack noexec