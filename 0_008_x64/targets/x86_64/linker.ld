/* ==========================================
 * Linker Script para GLOBEX_OS x86_64
 * ==========================================
 * Kernel loaded at 1MB (0x100000)
 * Esta dirección evita conflictos con BIOS data areas
 */

ENTRY(start)

PHDRS
{
    /* 
     * Segmentos con permisos explícitos para eliminar warning RWX
     * Flags: 1 = X (execute), 2 = W (write), 4 = R (read)
     */
    text PT_LOAD FLAGS(5);  /* R + E = 5 */
    data PT_LOAD FLAGS(6);  /* R + W = 6 */
}

SECTIONS
{
    /* Base address: 1MB */
    . = 1M;

    /* ==========================================
     * Boot Section - Multiboot Header
     * ========================================== */
    .boot : ALIGN(4K)
    {
        KEEP(*(.multiboot_header))
    } :text

    /* ==========================================
     * Code Section - Executable instructions
     * ========================================== */
    .text : ALIGN(4K)
    {
        *(.text)
        *(.text.*)
    } :text

    /* ==========================================
     * Read-Only Data - Constants, strings
     * ========================================== */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    } :text

    /* ==========================================
     * Initialized Data - Global/static con inicializador
     * ========================================== */
    .data : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    } :data

    /* ==========================================
     * Uninitialized Data - BSS (zero-initialized)
     * ==========================================
     * NOTA: Las page tables y stack del boot code están en .bss.boot
     * para evitar que se inicialicen a cero. Solo las variables C++
     * sin inicializador explícito van a .bss y deben inicializarse.
     * ========================================== */
    .bss : ALIGN(4K)
    {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    } :data

    /* ==========================================
     * Boot BSS - Page tables y stack (NOBITS, NO inicializar)
     * ==========================================
     * Esta sección contiene las page tables y el stack definidos
     * en main.asm. Es NOBITS y no se inicializa a cero.
     * ========================================== */
    .bss.boot : ALIGN(4K)
    {
        __bss_boot_start = .;
        *(.bss.boot)
        __bss_boot_end = .;
    } :data

    /* ==========================================
     * Validations - ASSERTs para seguridad
     * ========================================== */
    ASSERT(. < 11M, "ERROR: Kernel excede 10MB de tamaño")
    ASSERT(ADDR(.text) % 4096 == 0, "ERROR: .text no está alineada a 4KB")
    ASSERT(ADDR(.rodata) % 4096 == 0, "ERROR: .rodata no está alineada a 4KB")
    ASSERT(ADDR(.data) % 4096 == 0, "ERROR: .data no está alineada a 4KB")
    ASSERT(ADDR(.bss) % 4096 == 0, "ERROR: .bss no está alineada a 4KB")

    /* Descartar secciones de debug */
    /DISCARD/ : { *(.stab) *(.stabstr) }
}

/* ==========================================
 * Símbolos globales útiles para el kernel
 * ========================================== */
__kernel_start = 1M;
__kernel_end = .;

/* ==========================================
 * Símbolos BSS para inicialización en boot
 * ========================================== */
/* Estos símbolos son usados por el código de boot para zero-initializar BSS */
