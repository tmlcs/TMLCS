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
 * @brief Dirección base del buffer de texto VGA
 * @note 0xB8000 es la dirección estándar para modo texto 80x25
 */
#define VGA_BUFFER_ADDRESS  0xB8000

/**
 * @brief Número de columnas en modo texto VGA
 */
#define VGA_COLS  80

/**
 * @brief Número de filas en modo texto VGA
 */
#define VGA_ROWS  25

/**
 * @brief Tamaño total del buffer VGA en caracteres
 */
#define VGA_BUFFER_SIZE  (VGA_COLS * VGA_ROWS)

/**
 * @brief Tamaño de cada celda VGA en bytes (character + color)
 */
#define VGA_CELL_SIZE  2

/* ==========================================
 * Multiboot2 Constants
 * ========================================== */

/**
 * @brief Magic number para Multiboot2 header
 * @note Debe estar en el header del kernel
 */
#define MULTIBOOT2_MAGIC  0xE85250D6

/**
 * @brief Arquitectura objetivo (0 = protected mode i386)
 */
#define MULTIBOOT2_ARCH  0

/**
 * @brief Alineación del header Multiboot2
 */
#define MULTIBOOT2_HEADER_ALIGN  8

/**
 * @brief Tamaño mínimo del header Multiboot2
 */
#define MULTIBOOT2_HEADER_MIN_SIZE  16

/* ==========================================
 * Memory Layout Constants
 * ========================================== */

/**
 * @brief Dirección de inicio del kernel (1MB)
 * @note El kernel se carga en 0x100000 por el bootloader
 */
#define KERNEL_LOAD_ADDRESS  0x100000

/**
 * @brief Tamaño de página estándar (4KB)
 */
#define PAGE_SIZE  0x1000

/**
 * @brief Tamaño de página grande (2MB)
 */
#define PAGE_SIZE_2M  0x200000

/* ==========================================
 * Stack Constants
 * ========================================== */

/**
 * @brief Tamaño default del stack en bytes (64KB)
 */
#define DEFAULT_STACK_SIZE  0x10000

#ifdef __cplusplus
}
#endif

#endif /* CONSTANTS_H */
