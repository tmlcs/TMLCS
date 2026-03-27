global start
global stack_guard        ; Guard page address (exported for C++ guard setup)
global page_table_l2_0   ; First 1GiB L2 page table (exported for guard page setup)
extern long_mode_start

section .text
bits 32
start:
    ; ==========================================
    ; CRITICAL: Initialize stack with 16-byte alignment
    ; ==========================================
    ; System V ABI requires 16-byte stack alignment.
    ; Stack is 64KB (16 * 4096 bytes), so stack_top is already aligned.
    ; However, we explicitly ensure alignment for safety.
    ; ==========================================
    mov esp, stack_top
    and esp, ~0xF         ; Ensure 16-byte alignment in 32-bit mode

    call check_multiboot
	call check_cpuid
	call check_long_mode

	call setup_page_tables
	call enable_paging

    ; ==========================================
    ; TRANSITION TO LONG MODE
    ; ==========================================
    ; BSS initialization is intentionally deferred to main64.asm.
    ; See main64.asm for BSS zeroing implementation.
    ;
    ; SECURITY GUARANTEE:
    ;   - This code does NOT access any BSS variables
    ;   - The jump instruction doesn't touch BSS
    ;   - main64.asm zeros BSS before calling kernel_main
    ; ==========================================
	lgdt [gdt64.pointer]
	jmp gdt64.code_segment:long_mode_start

	hlt

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
; Page Table Flag Constants
; ==========================================
%define PTE_PRESENT     (1 << 0)    ; Present bit
%define PTE_WRITABLE    (1 << 1)    ; Writable bit
%define PTE_HUGE        (1 << 7)    ; Huge page (2MiB) bit
%define PTE_NX          (1 << 63)   ; No-Execute bit (upper 32 bits)
%define PTE_FLAGS_MASK  0xFFF       ; Flag bits mask

; ==========================================
; setup_page_tables - Configure page tables
; ==========================================
; @brief Configure identity mapping with huge pages (2MiB)
; @details
;   - L4[0] -> L3[0] -> L2[0] (512 entries = 1GiB)
;   - L4[0] -> L3[1] -> L2[1] (512 entries = 1GiB)
;   - Total: 1024 entries × 2MiB = 2GiB mapped
;   - Mapped range: 0x00000000 - 0x7FFFFFFF (0-2GiB)
;
;   SECURITY: NX (No-Execute) protection is applied at the L2 page entry level.
;   Pages at addresses >= 10MB (data region) have NX bit set.
;   Pages below 10MB (.text region) remain executable.
;   EFER.NXE is enabled in enable_paging() to activate NX protection.
;
;   To map more memory in the future:
;   - Add more L3 entries to point to more L2 tables
;   - 512 L3 entries × 512 L2 entries × 2MiB = 512GiB theoretical
; ==========================================
setup_page_tables:
    ; ==========================================
    ; Configure L4 -> L3 mapping
    ; ==========================================
    ; L4[0] -> L3 table (first L3 table)
    ; L4 is a code-adjacent page table, keep executable for simplicity
    mov eax, page_table_l3
    or eax, PTE_PRESENT | PTE_WRITABLE
    mov [page_table_l4], eax
    mov dword [page_table_l4 + 4], 0    ; Upper 32 bits: NX=0 (executable)

    ; ==========================================
    ; Configure L3 -> L2 mapping for 2GiB
    ; ==========================================
    ; L3[0] -> L2_0 (first 1GiB: 0-0x3FFFFFFF)
    ; Contains .text, .rodata at low addresses, then data pages
    mov eax, page_table_l2_0
    or eax, PTE_PRESENT | PTE_WRITABLE
    mov [page_table_l3 + 0 * 8], eax
    mov dword [page_table_l3 + 0 * 8 + 4], 0    ; Upper 32 bits: NX=0

    ; L3[1] -> L2_1 (second 1GiB: 0x40000000-0x7FFFFFFF)
    ; Pure data region - NX bit will be set at L2 page entry level
    ; L3 entries point to L2 tables, so NX here is irrelevant (non-leaf entry)
    mov eax, page_table_l2_1
    or eax, PTE_PRESENT | PTE_WRITABLE
    mov [page_table_l3 + 1 * 8], eax
    mov dword [page_table_l3 + 1 * 8 + 4], 0    ; Upper 32 bits: zero (NX irrelevant for non-leaf)

    ; ==========================================
    ; Map first 1GiB (L2_0)
    ; ==========================================
    ; First 10MB: Contains kernel code (.text) - executable
    ; After kernel: Data pages (.data, .bss, heap) - non-executable
    mov ecx, 0              ; counter
.map_loop_0:
    mov eax, 0x200000       ; 2MiB
    mul ecx                 ; eax = ecx * 2MiB (physical address); edx = 0 (always, addr < 2GiB)
    or eax, PTE_PRESENT | PTE_WRITABLE | PTE_HUGE
    
    ; CRIT-SEC-002 FIX: Mark pages above 10MB as non-executable
    ; Kernel is limited to 10MB by linker script assertion
    cmp eax, 0x00A00000     ; 10MB threshold
    jae .set_nx_0           ; If >= 10MB, set NX bit
    mov edx, 0              ; Upper 32 bits: NX=0 (executable for .text)
    jmp .store_entry_0
.set_nx_0:
    mov edx, 0x80000000     ; Upper 32 bits: NX=1 (non-executable for data)
.store_entry_0:
    mov [page_table_l2_0 + ecx * 8], eax
    mov [page_table_l2_0 + ecx * 8 + 4], edx

    inc ecx
    cmp ecx, 512            ; 512 entries × 2MiB = 1GiB
    jne .map_loop_0

    ; ==========================================
    ; Map second 1GiB (L2_1)
    ; ==========================================
    ; Entire second 1GiB is data region - all pages non-executable
    mov ecx, 0              ; counter
.map_loop_1:
    mov eax, 0x200000       ; 2MiB
    mul ecx                 ; eax = ecx * 2MiB; edx = 0 (addr < 2GiB)
    add eax, 0x40000000     ; + 1GiB offset (physical address base)
    or eax, PTE_PRESENT | PTE_WRITABLE | PTE_HUGE
    mov [page_table_l2_1 + ecx * 8], eax
    mov dword [page_table_l2_1 + ecx * 8 + 4], 0x80000000  ; Upper 32 bits: NX=1 (non-executable)

    inc ecx
    cmp ecx, 512            ; 512 entries × 2MiB = 1GiB
    jne .map_loop_1

    ; ==========================================
    ; VERIFICATION: Read back critical entries
    ; ==========================================
    ; Preserve ebx register (callee-saved per System V ABI)
    push ebx

    ; Verify L4[0] -> L3 mapping was written correctly
    mov eax, [page_table_l4]
    mov ebx, eax
    and ebx, PTE_FLAGS_MASK   ; Mask to get flags only
    cmp ebx, PTE_PRESENT | PTE_WRITABLE
    jne .page_table_error

    ; Verify L3[0] -> L2_0 mapping (executable for .text region)
    mov eax, [page_table_l3]
    mov ebx, eax
    and ebx, PTE_FLAGS_MASK
    cmp ebx, PTE_PRESENT | PTE_WRITABLE
    jne .page_table_error

    ; Verify L3[1] -> L2_1 mapping (L3 entries are non-leaf, NX is irrelevant)
    mov eax, [page_table_l3 + 8]  ; Entry 1 is at offset 8 bytes
    mov ebx, eax
    and ebx, PTE_FLAGS_MASK
    cmp ebx, PTE_PRESENT | PTE_WRITABLE
    jne .page_table_error
    ; Note: Upper 32 bits of L3 entries are zero (NX bit irrelevant for non-leaf)

    ; Verify first L2 entry (2MiB huge page, executable for .text)
    mov eax, [page_table_l2_0]
    mov ebx, eax
    and ebx, PTE_PRESENT | PTE_WRITABLE | PTE_HUGE
    cmp ebx, PTE_PRESENT | PTE_WRITABLE | PTE_HUGE
    jne .page_table_error

    ; Verify last L2_0 entry (entry 511 = 0x3FE00000) has NX set
    mov eax, [page_table_l2_0 + 511 * 8]
    mov ebx, eax
    and ebx, 0xFFFFF000     ; Mask to get address only
    cmp ebx, 0x3FE00000     ; Should map to 0x3FE00000
    jne .page_table_error
    ; Verify NX bit is set (data page)
    mov ebx, [page_table_l2_0 + 511 * 8 + 4]
    cmp ebx, 0x80000000     ; Should have NX bit set
    jne .page_table_error

    ; Restore ebx register before returning
    pop ebx

    ret

.page_table_error:
    ; Display "PTE" (Page Table Error) on VGA
    mov dword [0xb8000], 0x4f455450  ; "PTE" in red on white
    hlt

enable_paging:
	; pass page table location to cpu
	mov eax, page_table_l4
	mov cr3, eax

	; enable PAE (required for long mode and NX support)
	mov eax, cr4
	or eax, 1 << 5
	mov cr4, eax

	; CRIT-SEC-002 FIX: Enable NX (No-Execute) support
	; Set EFER.NXE (bit 11 of MSR 0xC0000080) to activate NX bit in page tables
	; This prevents code execution from data pages (stack, heap, .data, .bss)
	mov ecx, 0xC0000080     ; MSR_EFER
	rdmsr
	or eax, 1 << 11         ; Set NXE bit (bit 11)
	or eax, 1 << 8          ; Set LME bit (bit 8) for long mode
	wrmsr

	; enable long mode (already set LME above, now enable via CR4.PAE)
	; PAE was enabled earlier, long mode is enabled by setting LME in EFER

	; enable paging + write-protect (CR0.WP, bit 16)
	; LOW-NEW-006 FIX: Set WP (bit 16) alongside PG (bit 31) so that even
	; ring-0 (kernel) code cannot write to pages whose page table entry has
	; the read-only (R/W=0) bit clear.  Without WP, CR0.PG alone does NOT
	; enforce write protection at CPL 0 — a kernel bug can silently corrupt
	; any page in the identity-mapped region.
	mov eax, cr0
	or eax, (1 << 31) | (1 << 16)
	mov cr0, eax

	ret

; ==========================================
; JUMP TO LONG MODE - Critical Transition
; ==========================================
; SECURITY NOTE: BSS initialization is intentionally NOT done here.
; 
; Why BSS init is done in long mode (main64.asm), not here:
;   1. In 32-bit mode, we can only access 4GB address space
;   2. After enabling paging, we need 64-bit registers for proper addressing
;   3. The jump to long mode (below) doesn't access any BSS variables
;   4. main64.asm initializes BSS immediately upon entering long mode
;
; GUARANTEE: No BSS access occurs between this jump and zero_bss in main64.asm
; ==========================================
error:
	; print "ERR: X" where X is the error code
	mov dword [0xb8000], 0x4f524f45
	mov dword [0xb8004], 0x4f3a4f52
	mov dword [0xb8008], 0x4f204f20
	mov byte  [0xb800a], al
	hlt

; ==========================================
; Boot Data Section - Page tables and stack (NO initialize)
; ==========================================
; This section is NOBITS - does not occupy file space
; and is NOT zero-initialized (separate from .bss)
;
; Memory Mapping:
;   - 1 page table L4 (4KB)
;   - 1 page table L3 (4KB) - supports 512 entries
;   - 2 page tables L2 (8KB) - map 2GiB total
;   - Stack (64KB)
; ==========================================
section .boot.data nobits
align 4096
page_table_l4:
    resb 4096               ; L4: 512 entries (used: 1 entry)
page_table_l3:
    resb 4096               ; L3: 512 entries (used: 2 entries)
page_table_l2_0:
    resb 4096               ; L2_0: 512 entries × 2MiB = 1GiB (0-0x3FFFFFFF)
page_table_l2_1:
    resb 4096               ; L2_1: 512 entries × 2MiB = 1GiB (0x40000000-0x7FFFFFFF)
stack_guard:
    resb 4096               ; HIGH-005: guard page - marked not-present by stack_guard_init()
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
; .note.GNU-stack section to eliminate linker warning
; ==========================================
section .note.GNU-stack noexec
