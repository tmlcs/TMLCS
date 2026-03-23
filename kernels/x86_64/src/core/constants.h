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

/* ==========================================
 * IDT Constants
 * ========================================== */

/**
 * @brief Number of IDT entries
 * @note 256 entries for all interrupt vectors
 */
#define IDT_ENTRIES 256

/**
 * @brief IDT gate types
 * @note Type field in IDT entry
 */
#define IDT_INTERRUPT_GATE 0xE  /**< 32-bit interrupt gate */
#define IDT_TRAP_GATE 0xF       /**< 32-bit trap gate */
#define IDT_USER_INTERRUPT 0x8  /**< User-defined interrupt gate */

/**
 * @brief IDT vector offsets
 * @note Vector ranges for different interrupt types
 */
#define IDT_VECTOR_EXCEPTIONS 0    /**< CPU exceptions (0-31) */
#define IDT_VECTOR_IRQS 32         /**< Hardware IRQs (32-47) */
#define IDT_VECTOR_SYSCALLS 48     /**< Software interrupts/syscalls (48+) */

/**
 * @brief Exception vector definitions
 * @note CPU exception vectors
 */
#define EXCEPTION_DE 0   /**< Divide Error */
#define EXCEPTION_DB 1   /**< Debug Exception */
#define EXCEPTION_NMI 2  /**< Non-Maskable Interrupt */
#define EXCEPTION_BP 3   /**< Breakpoint */
#define EXCEPTION_OF 4   /**< Overflow */
#define EXCEPTION_BR 5   /**< Bound Range Exceeded */
#define EXCEPTION_UD 6   /**< Invalid Opcode */
#define EXCEPTION_NM 7   /**< Device Not Available */
#define EXCEPTION_DF 8   /**< Double Fault */
#define EXCEPTION_CSF 9  /**< Coprocessor Segment Overrun (Reserved) */
#define EXCEPTION_TS 10  /**< Invalid TSS */
#define EXCEPTION_NP 11  /**< Segment Not Present */
#define EXCEPTION_SS 12  /**< Stack-Segment Fault */
#define EXCEPTION_GP 13  /**< General Protection Fault */
#define EXCEPTION_PF 14  /**< Page Fault */
#define EXCEPTION_RES15 15 /**< Reserved (Intel/AMD) */
#define EXCEPTION_MF 16  /**< x87 FPU Error */
#define EXCEPTION_AC 17  /**< Alignment Check */
#define EXCEPTION_MC 18  /**< Machine Check */
#define EXCEPTION_XM 19  /**< SIMD Floating-Point */
#define EXCEPTION_RES20 20 /**< Reserved (future CPU extension) */
#define EXCEPTION_RES21 21 /**< Reserved (future CPU extension) */
#define EXCEPTION_RES22 22 /**< Reserved (future CPU extension) */
#define EXCEPTION_RES23 23 /**< Reserved (future CPU extension) */
#define EXCEPTION_RES24 24 /**< Reserved (future CPU extension) */
#define EXCEPTION_RES25 25 /**< Reserved (future CPU extension) */
#define EXCEPTION_RES26 26 /**< Reserved (future CPU extension) */
#define EXCEPTION_RES27 27 /**< Reserved (future CPU extension) */
#define EXCEPTION_RES28 28 /**< Reserved (future CPU extension) */
#define EXCEPTION_RES29 29 /**< Reserved (future CPU extension) */
#define EXCEPTION_RES30 30 /**< Reserved (future CPU extension) */
#define EXCEPTION_RES31 31 /**< Reserved (future CPU extension) */

/**
 * @brief Total number of exception handlers (vectors 0-31)
 * @note All vectors 0-31 now have handlers. Reserved vectors use stub handlers.
 */
#define EXCEPTION_MESSAGE_COUNT 32

/* ==========================================
 * PIC Constants
 * ========================================== */

/**
 * @brief PIC1 (Master) I/O ports
 */
#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21

/**
 * @brief PIC2 (Slave) I/O ports
 */
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

/**
 * @brief PIC IRQ offsets (after remapping)
 * @note IRQs are remapped to vectors 0x20-0x2F
 */
#define PIC1_OFFSET 0x20  /**< Master PIC: IRQs 0-7 -> vectors 32-39 */
#define PIC2_OFFSET 0x28  /**< Slave PIC: IRQs 8-15 -> vectors 40-47 */

/**
 * @brief PIC EOI (End of Interrupt) command
 */
#define PIC_EOI 0x20

/**
 * @brief PIC ICW1 initialization flags
 */
#define ICW1_ICW4 0x01    /**< ICW4 present */
#define ICW1_SINGLE 0x02  /**< Single mode */
#define ICW1_INTERVAL4 0x04 /**< Call address interval 4 */
#define ICW1_LEVEL 0x08   /**< Level triggered */
#define ICW1_INIT 0x10    /**< Initialization required */

/**
 * @brief PIC ICW4 configuration flags
 */
#define ICW4_8086 0x01    /**< 8086 mode */
#define ICW4_AUTO 0x02    /**< Auto EOI */
#define ICW4_BUF_SLAVE 0x08 /**< Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C /**< Buffered mode/master */
#define ICW4_SFNM 0x10    /**< Special fully nested mode */

/* ==========================================
 * IRQ Vector Definitions
 * ========================================== */

/**
 * @brief Hardware IRQ vector definitions
 * @note After PIC remapping (0x20 + IRQ number)
 */
#define IRQ_PIT 0        /**< IRQ0: Programmable Interval Timer */
#define IRQ_KEYBOARD 1   /**< IRQ1: Keyboard */
#define IRQ_CASCADE 2    /**< IRQ2: Cascade (used for slave PIC) */
#define IRQ_SERIAL2 3    /**< IRQ3: Serial Port 2 */
#define IRQ_SERIAL1 4    /**< IRQ4: Serial Port 1 */
#define IRQ_PARALLEL2 5  /**< IRQ5: Parallel Port 2 / Sound Card */
#define IRQ_FLOPPY 6     /**< IRQ6: Floppy Disk */
#define IRQ_PARALLEL1 7  /**< IRQ7: Parallel Port 1 / Sound Card */
#define IRQ_RTC 8        /**< IRQ8: Real-Time Clock */
#define IRQ_ACPI 9       /**< IRQ9: ACPI */
#define IRQ_USB 11       /**< IRQ11: USB */
#define IRQ_PS2_MOUSE 12 /**< IRQ12: PS/2 Mouse */
#define IRQ_PS2_KBD 13   /**< IRQ13: PS/2 Keyboard */
#define IRQ_ATA1 14      /**< IRQ14: Primary ATA */
#define IRQ_ATA2 15      /**< IRQ15: Secondary ATA */

/* ==========================================
 * RFLAGS Register Constants
 * ========================================== */

/**
 * @brief RFLAGS interrupt flag bit
 * @note Set to enable interrupts, clear to disable
 */
#define RFLAGS_IF_BIT (1 << 9)

/* ==========================================
 * IDT Offset Masks and Shifts
 * ========================================== */

/**
 * @brief IDT offset bit masks
 * @note Used to split 64-bit handler address into IDT entry fields
 */
#define IDT_OFFSET_LOW_MASK 0xFFFF        /**< Mask for bits 0-15 */
#define IDT_OFFSET_MIDDLE_MASK 0xFFFF     /**< Mask for bits 16-31 */
#define IDT_OFFSET_HIGH_MASK 0xFFFFFFFF   /**< Mask for bits 32-63 */

/**
 * @brief IDT offset bit shifts
 * @note Used to shift handler address for IDT entry fields
 */
#define IDT_OFFSET_MIDDLE_SHIFT 16        /**< Shift for bits 16-31 */
#define IDT_OFFSET_HIGH_SHIFT 32          /**< Shift for bits 32-63 */

/* ==========================================
 * PIC ICW3 Constants
 * ========================================== */

/**
 * @brief PIC ICW3 master/slave configuration constants
 * @note Used during PIC initialization
 */
#define ICW3_MASTER_MASK 0xFF             /**< All IRQs masked initially */
#define ICW3_MASTER_SLAVE_ON_IRQ2 0x04    /**< Master: slave connected to IRQ2 */
#define ICW3_SLAVE_CASCADE_IDENTITY 0x02  /**< Slave: cascade identity */

/* ==========================================
 * PIC IRQ Mask Constants
 * ========================================== */

/**
 * @brief PIC mask all IRQs
 * @note Used to mask all interrupts
 */
#define PIC_MASK_ALL_IRQS 0xFF            /**< Mask all 8 IRQs */

#ifdef __cplusplus
}
#endif

#endif /* CONSTANTS_H */
