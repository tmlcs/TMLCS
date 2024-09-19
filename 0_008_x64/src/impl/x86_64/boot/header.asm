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

    ; etiqueta de fin
    dw 0
    dw 0
    dd 8

header_end: