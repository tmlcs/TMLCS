#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================
 * VGA Text Mode Constants
 * ========================================== */

/**
 * @brief Base address of VGA text buffer
 * @note 0xB8000 is the standard address for 80x25 text mode
 */
#define VGA_BUFFER_ADDRESS  0xB8000

/**
 * @brief Number of columns in VGA text mode
 */
#define VGA_COLS  80

/**
 * @brief Number of rows in VGA text mode
 */
#define VGA_ROWS  25

/**
 * @brief Total size of VGA buffer in characters
 */
#define VGA_BUFFER_SIZE  (VGA_COLS * VGA_ROWS)

/**
 * @brief Size of each VGA cell in bytes (character + color)
 */
#define VGA_CELL_SIZE  2

/* ==========================================
 * Multiboot2 Constants
 * ========================================== */

/**
 * @brief Magic number for Multiboot2 header
 * @note Must be in kernel header
 */
#define MULTIBOOT2_MAGIC  0xE85250D6

/**
 * @brief Target architecture (0 = protected mode i386)
 */
#define MULTIBOOT2_ARCH  0

/**
 * @brief Multiboot2 header alignment
 */
#define MULTIBOOT2_HEADER_ALIGN  8

/**
 * @brief Minimum size of Multiboot2 header
 */
#define MULTIBOOT2_HEADER_MIN_SIZE  16

/* ==========================================
 * Memory Layout Constants
 * ========================================== */

/**
 * @brief Kernel start address (1MB)
 * @note Kernel is loaded at 0x100000 by bootloader
 */
#define KERNEL_LOAD_ADDRESS  0x100000

/**
 * @brief Standard page size (4KB)
 */
#define PAGE_SIZE  0x1000

/**
 * @brief Large page size (2MB)
 */
#define PAGE_SIZE_2M  0x200000

/* ==========================================
 * Stack Constants
 * ========================================== */

/**
 * @brief Default stack size in bytes (64KB)
 */
#define DEFAULT_STACK_SIZE  0x10000

#ifdef __cplusplus
}
#endif

#endif /* CONSTANTS_H */
