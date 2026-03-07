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
     * Esta sección será inicializada a cero por el código de boot.
     * Solo variables C/C++ sin inicializador explícito van aquí.
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
     * Boot Data - Page tables y stack (NO inicializar)
     * ==========================================
     * Esta sección contiene las page tables y el stack del boot code.
     * Es NOBITS (no ocupa espacio en archivo) pero NO debe inicializarse
     * a cero porque contiene datos configurados en tiempo de boot.
     * 
     * Separada de .bss para permitir zero-initialization seguro del BSS.
     * ========================================== */
    .boot.data : ALIGN(4K)
    {
        __boot_data_start = .;
        *(.boot.data)
        *(.bss.boot)  /* Compatibilidad con código existente */
        __boot_data_end = .;
    } :data

    /* ==========================================
     * Validations - ASSERTs para seguridad
     * ========================================== */
    ASSERT(. < 11M, "ERROR: Kernel excede 10MB de tamaño")
    ASSERT(ADDR(.text) % 4096 == 0, "ERROR: .text no está alineada a 4KB")
    ASSERT(ADDR(.rodata) % 4096 == 0, "ERROR: .rodata no está alineada a 4KB")
    ASSERT(ADDR(.data) % 4096 == 0, "ERROR: .data no está alineada a 4KB")
    ASSERT(ADDR(.bss) % 4096 == 0, "ERROR: .bss no está alineada a 4KB")
    ASSERT(ADDR(.boot.data) % 4096 == 0, "ERROR: .boot.data no está alineada a 4KB")
    
    /* Verificar que las page tables caben en .boot.data */
    ASSERT((__boot_data_end - __boot_data_start) >= 0x10000, 
           "ERROR: .boot.data demasiado pequeña para page tables + stack")

    /* Descartar secciones de debug */
    /DISCARD/ : { *(.stab) *(.stabstr) }
}

/* ==========================================
 * Símbolos globales útiles para el kernel
 * ========================================== */
__kernel_start = 1M;
__kernel_end = .;

/* ==========================================
 * Memory Map Information
 * ========================================== */
__memory_mapped_start = 0x00000000;
__memory_mapped_end = 0x80000000;     /* 2GiB mapeados (0-0x7FFFFFFF) */
__memory_mapped_size = 0x80000000;    /* 2GiB en bytes */

/* ==========================================
 * Símbolos BSS para inicialización en boot
 * ========================================== */
/* Estos símbolos son usados por el código de boot para zero-initializar BSS */
