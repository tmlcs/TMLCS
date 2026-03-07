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
    ; magic number (identifica al kernel como multiboot)
    dd 0xe85250d6

    ; arquitectura (protegida, en este caso)
    dd 0

    ; longitud del encabezado (header length)
    dd header_end - header_start ; calculada como la distancia entre el inicio y el final del encabezado

    ; checksum (calcular como la suma de todos los bytes menos el número mágico)
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start))

    ; etiqueta de fin (tag end: type=0, flags=0, size=8)
    dw 0
    dw 0
    dd 8

header_end:

; ==========================================
; Sección .note.GNU-stack para eliminar warning del linker
; ==========================================
section .note.GNU-stack noexec