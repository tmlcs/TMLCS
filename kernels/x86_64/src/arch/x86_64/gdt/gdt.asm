; =============================================================================
; GDT Assembly Functions
; =============================================================================
;
; Provides low-level functions for loading GDT and TSS.
; These functions are called from gdt.cpp during kernel initialization.
;
; =============================================================================

global gdt_load
global tss_load

section .text
bits 64

; =============================================================================
; gdt_load - Load GDT using LGDT instruction
; =============================================================================
; Parameters:
;   RDI: Pointer to gdt_pointer_t structure
;
; C prototype:
;   void gdt_load(gdt_pointer_t* gdtp);
;
; In 64-bit long mode, segment registers (except FS/GS) are largely ignored.
; We only need to load the GDTR for:
;   - TSS descriptor (used for IST and RSP switching on interrupts)
;   - User/kernel mode separation (DPL checks)
;
; No need to reload CS/DS/ES/SS in long mode - they are ignored.
; =============================================================================
gdt_load:
    lgdt [rdi]              ; Load GDTR from memory location pointed by RDI
    
    ; In long mode, segment registers are cached and not reloaded from GDT
    ; unless explicitly needed. For our purposes (TSS loading), just LGDT
    ; is sufficient. No need for far return or segment reload.
    
    ret

; =============================================================================
; tss_load - Load TSS using LTR instruction
; =============================================================================
; Parameters:
;   DI: TSS segment selector
;
; C prototype:
;   void tss_load(uint16_t tss_selector);
;
; The LTR instruction loads the Task Register with:
;   - Segment selector pointing to TSS descriptor in GDT
;
; Note: In 64-bit mode, TSS is not used for task switching.
;       It's only used for:
;         - RSP0/RSP1/RSP2 for privilege level transitions
;         - IST (Interrupt Stack Table) for interrupt handling
; =============================================================================
tss_load:
    ltr di                  ; Load task register with selector
    ret

; =============================================================================
; .note.GNU-stack section to eliminate linker warning
; =============================================================================
section .note.GNU-stack noexec
