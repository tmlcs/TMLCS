global start
extern long_mode_start

section .text
bits 32
start:
	mov esp, stack_top

	call check_multiboot
	call check_cpuid
	call check_long_mode

	call setup_page_tables
	call enable_paging

	lgdt [gdt64.pointer]
	mov ebx, gdt64.data_segment
	jmp gdt64.code_segment:long_mode_start

	hlt

check_multiboot:
	cmp eax, 0x36d76289
	jne .no_multiboot
	ret
.no_multiboot:
	mov esi, panic_no_multiboot
	jmp panic

check_cpuid:
	pushfd
	pop eax
	mov ecx, eax
	xor eax, 1 << 21
	push eax
	popfd
	pushfd
	pop eax
	push ecx
	popfd
	cmp eax, ecx
	je .no_cpuid
	ret
.no_cpuid:
	mov esi, panic_no_cpuid
	jmp panic

check_long_mode:
	mov eax, 0x80000000
	cpuid
	cmp eax, 0x80000001
	jb .no_long_mode

	mov eax, 0x80000001
	cpuid
	test edx, 1 << 29
	jz .no_long_mode
	
	ret
.no_long_mode:
	mov esi, panic_no_long_mode
	jmp panic

setup_page_tables:
	mov eax, page_table_l3
	or eax, 0b11 ; present, writable
	mov [page_table_l4], eax
	
	mov eax, page_table_l2
	or eax, 0b11 ; present, writable
	mov [page_table_l3], eax

	mov ecx, 0 ; counter
.loop:

	mov eax, 0x200000 ; 2MiB
	mul ecx
	or eax, 0b10000011 ; present, writable, huge page
	mov [page_table_l2 + ecx * 8], eax

	inc ecx ; increment counter
	cmp ecx, 512 ; checks if the whole table is mapped
	jne .loop ; if not, continue

	ret

enable_paging:
	; pass page table location to cpu
	mov eax, page_table_l4
	mov cr3, eax

	; enable PAE
	mov eax, cr4
	or eax, 1 << 5
	mov cr4, eax

	; enable long mode
	mov ecx, 0xC0000080
	rdmsr
	or eax, 1 << 8
	wrmsr

	; enable paging
	mov eax, cr0
	or eax, 1 << 31
	mov cr0, eax

	ret

panic:
    ; print the message from esi
    mov edi, 0xb8000
    mov ah, 0x4f
.loop:
    lodsb
    test al, al
    jz .done
    mov [edi], ax
    add edi, 2
    jmp .loop
.done:
    hlt

section .bss
align 4096
page_table_l4:
	resb 4096
page_table_l3:
	resb 4096
page_table_l2:
	resb 4096
stack_bottom:
	resb 4096 * 4
stack_top:

section .rodata
panic_no_multiboot:
    db "PANIC: No Multiboot!", 0
panic_no_cpuid:
    db "PANIC: CPUID not supported!", 0
panic_no_long_mode:
    db "PANIC: Long mode not supported!", 0

gdt64:
	dq 0 ; zero entry
.code_segment: equ $ - gdt64
	dq (1 << 41) | (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53) ; code segment
.data_segment: equ $ - gdt64
	dq (1 << 41) | (1 << 44) | (1 << 47) ; data segment
.pointer:
	dw $ - gdt64 - 1 ; length
	dq gdt64 ; address
