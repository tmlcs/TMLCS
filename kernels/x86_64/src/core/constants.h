#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stddef.h> /* Explicit include for size_t */
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
#define VGA_BUFFER_ADDRESS 0xB8000

/**
 * @brief Number of columns in VGA text mode
 */
#define VGA_COLS 80

/**
 * @brief Number of rows in VGA text mode
 */
#define VGA_ROWS 25

/**
 * @brief Total size of VGA buffer in characters
 */
#define VGA_BUFFER_SIZE (VGA_COLS * VGA_ROWS)

/**
 * @brief Size of each VGA cell in bytes (character + color)
 */
#define VGA_CELL_SIZE 2

/* ==========================================
 * Multiboot2 Constants
 * ========================================== */

/**
 * @brief Magic number for Multiboot2 header
 * @note Must be in kernel header
 */
#define MULTIBOOT2_MAGIC 0xE85250D6

/**
 * @brief Target architecture (0 = protected mode i386)
 */
#define MULTIBOOT2_ARCH 0

/**
 * @brief Multiboot2 header alignment
 */
#define MULTIBOOT2_HEADER_ALIGN 8

/**
 * @brief Minimum size of Multiboot2 header
 */
#define MULTIBOOT2_HEADER_MIN_SIZE 16

/* ==========================================
 * Memory Layout Constants
 * ========================================== */

/**
 * @brief Kernel start address (1MB)
 * @note Kernel is loaded at 0x100000 by bootloader
 */
#define KERNEL_LOAD_ADDRESS 0x100000

/**
 * @brief Standard page size (4KB)
 */
#define PAGE_SIZE 0x1000

/**
 * @brief Large page size (2MB)
 */
#define PAGE_SIZE_2M 0x200000

/* ==========================================
 * Stack Constants
 * ========================================== */

/**
 * @brief Default stack size in bytes (64KB)
 */
#define DEFAULT_STACK_SIZE 0x10000

/* ==========================================
 * Serial Port (UART 16550) Constants
 * ========================================== */

/**
 * @brief IIR value when port does not exist
 * @note 0xFF indicates no device on port (bus floating)
 */
#define SERIAL_IIR_NO_DEVICE 0xFF

/**
 * @brief Maximum timeout for serial operations (iterations)
 * @note Adjust according to baud rate and hardware
 */
#define SERIAL_MAX_TIMEOUT 100000

/**
 * @brief Scratch register value for test
 * @note Used to detect UART presence
 */
#define SERIAL_SCRATCH_TEST 0x5A

/**
 * @brief Maximum number of iterations for initialization delay
 * @note ~0.5ms at 2GHz (1 cycle per iteration)
 */
#define SERIAL_INIT_DELAY_ITERATIONS 1000

/* ==========================================
 * VGA Color Constants
 * ========================================== */

/**
 * @brief Color attribute: white on red (error display)
 * @note Used in early_panic() to display errors
 */
#define VGA_COLOR_WHITE_ON_RED 0x4F

/**
 * @brief Color attribute: light green on black (success)
 * @note Used for successful boot messages
 */
#define VGA_COLOR_LIGHT_GREEN_ON_BLACK 0x0A

/**
 * @brief Color attribute: yellow on black (warning)
 * @note Used for warning messages
 */
#define VGA_COLOR_YELLOW_ON_BLACK 0x0E

#ifdef __cplusplus
}
#endif

#endif /* CONSTANTS_H */
