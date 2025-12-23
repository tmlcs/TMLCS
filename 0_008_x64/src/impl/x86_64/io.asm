global outb
global inb
global lidt
global invlpg

section .text

; outb(port, data)
outb:
    mov dx, di  ; port
    mov al, sil ; data
    out dx, al
    ret

; inb(port)
inb:
    mov dx, di  ; port
    in al, dx
    movzx eax, al ; zero-extend al to eax
    ret

; lidt(idt_ptr)
lidt:
    lidt [rdi]
    ret

; invlpg(virtual_address)
invlpg:
    invlpg [rdi]
    ret
