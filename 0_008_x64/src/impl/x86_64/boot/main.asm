global start
extern long_mode_start
extern __bss_start
extern __bss_end

section .text
bits 32
start:
	mov esp, stack_top

	call check_multiboot
	call check_cpuid
	call check_long_mode

	call setup_page_tables
	call enable_paging

    ; NOTA: BSS initialization se hace en long mode (main64.asm)
    ; call zero_bss

	lgdt [gdt64.pointer]
	jmp gdt64.code_segment:long_mode_start

	hlt

; ==========================================
; zero_bss - Inicializar sección BSS a cero
; ==========================================
; @brief Ceros la sección BSS antes del salto a long mode
; @note Esencial para variables globales sin inicializador explícito
; ==========================================
zero_bss:
	push eax
	push ecx
	push edi
	
	mov edi, __bss_start
	mov ecx, __bss_end
	sub ecx, edi
	shr ecx, 2          ; Convertir bytes a dwords
	xor eax, eax
	rep stosd           ; Llenar con ceros
	
	pop edi
	pop ecx
	pop eax
	ret

check_multiboot:
	cmp eax, 0x36d76289
	jne .no_multiboot
	ret
.no_multiboot:
	mov al, "M"
	jmp error

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
	mov al, "C"
	jmp error

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
	mov al, "L"
	jmp error

; ==========================================
; setup_page_tables - Configurar tablas de páginas
; ==========================================
; @brief Configura identity mapping con huge pages (2MiB)
; @details 
;   - L4[0] -> L3[0] -> L2[0] (512 entries = 1GiB)
;   - L4[0] -> L3[1] -> L2[1] (512 entries = 1GiB)
;   - Total: 1024 entries × 2MiB = 2GiB mapeado
;   - Rango mapeado: 0x00000000 - 0x7FFFFFFF (0-2GiB)
;   
;   Para mapear más memoria en el futuro:
;   - Agregar más entries L3 para apuntar a más L2 tables
;   - 512 L3 entries × 512 L2 entries × 2MiB = 512GiB teórico
; ==========================================
setup_page_tables:
    ; ==========================================
    ; Configurar L4 -> L3 mapping
    ; ==========================================
    ; L4[0] -> L3 table (primera tabla L3)
    mov eax, page_table_l3
    or eax, 0b11            ; present (bit 0) + writable (bit 1)
    mov [page_table_l4], eax

    ; ==========================================
    ; Configurar L3 -> L2 mapping para 2GiB
    ; ==========================================
    ; L3[0] -> L2_0 (primer 1GiB: 0-0x3FFFFFFF)
    mov eax, page_table_l2_0
    or eax, 0b11            ; present + writable
    mov [page_table_l3 + 0 * 8], eax
    
    ; L3[1] -> L2_1 (segundo 1GiB: 0x40000000-0x7FFFFFFF)
    mov eax, page_table_l2_1
    or eax, 0b11            ; present + writable
    mov [page_table_l3 + 1 * 8], eax

    ; ==========================================
    ; Mapear primer 1GiB (L2_0)
    ; ==========================================
    mov ecx, 0              ; counter
.map_loop_0:
    mov eax, 0x200000       ; 2MiB
    mul ecx                 ; eax = ecx * 2MiB (dirección física)
    or eax, 0b10000011      ; present + writable + huge page (bit 7)
    mov [page_table_l2_0 + ecx * 8], eax

    inc ecx
    cmp ecx, 512            ; 512 entries × 2MiB = 1GiB
    jne .map_loop_0

    ; ==========================================
    ; Mapear segundo 1GiB (L2_1)
    ; ==========================================
    mov ecx, 0              ; counter
.map_loop_1:
    mov eax, 0x200000       ; 2MiB
    mul ecx                 ; eax = ecx * 2MiB
    add eax, 0x40000000     ; + 1GiB offset (dirección física base)
    or eax, 0b10000011      ; present + writable + huge page
    mov [page_table_l2_1 + ecx * 8], eax

    inc ecx
    cmp ecx, 512            ; 512 entries × 2MiB = 1GiB
    jne .map_loop_1

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

error:
	; print "ERR: X" where X is the error code
	mov dword [0xb8000], 0x4f524f45
	mov dword [0xb8004], 0x4f3a4f52
	mov dword [0xb8008], 0x4f204f20
	mov byte  [0xb800a], al
	hlt

; ==========================================
; Boot Data Section - Page tables y stack (NO inicializar)
; ==========================================
; Esta sección es NOBITS - no ocupa espacio en el archivo
; y NO se inicializa a cero (separada de .bss)
; 
; Memory Mapping:
;   - 1 page table L4 (4KB)
;   - 1 page table L3 (4KB) - soporta 512 entries
;   - 2 page tables L2 (8KB) - mapean 2GiB total
;   - Stack (64KB)
; ==========================================
section .boot.data nobits
align 4096
page_table_l4:
    resb 4096               ; L4: 512 entries (usada: 1 entry)
page_table_l3:
    resb 4096               ; L3: 512 entries (usada: 2 entries)
page_table_l2_0:
    resb 4096               ; L2_0: 512 entries × 2MiB = 1GiB (0-0x3FFFFFFF)
page_table_l2_1:
    resb 4096               ; L2_1: 512 entries × 2MiB = 1GiB (0x40000000-0x7FFFFFFF)
stack_bottom:
    resb 4096 * 16          ; 64KB stack
stack_top:

section .rodata
gdt64:
	dq 0 ; zero entry
.code_segment: equ $ - gdt64
	dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53) ; code segment
.pointer:
	dw $ - gdt64 - 1 ; length
	dq gdt64 ; address

; ==========================================
; Sección .note.GNU-stack para eliminar warning del linker
; ==========================================
section .note.GNU-stack noexec
