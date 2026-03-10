#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================
 * Standard Serial Ports (UART 16550)
 * ==========================================
 * COM1: 0x3F8
 * COM2: 0x2F8
 * COM3: 0x3E8
 * COM4: 0x2E8
 */
#define SERIAL_COM1 0x3F8
#define SERIAL_COM2 0x2F8
#define SERIAL_COM3 0x3E8
#define SERIAL_COM4 0x2E8

/* ==========================================
 * UART 16550 Registers
 * ==========================================
 * Note: All registers are accessed via base port
 * Accessible register depends on DLAB (Divisor Latch Access Bit)
 */

/* When DLAB = 0 */
#define SERIAL_RBR 0  /* Receiver Buffer Register (read) */
#define SERIAL_THR  0  /* Transmitter Holding Register (write) */
#define SERIAL_IER  1  /* Interrupt Enable Register */

/* When DLAB = 1 */
#define SERIAL_DLL  0  /* Divisor Latch Low byte */
#define SERIAL_DLM  1  /* Divisor Latch High byte */

/* Common registers (DLAB-independent) */
#define SERIAL_IIR  2  /* Interrupt Identification Register (read) */
#define SERIAL_FCR  2  /* FIFO Control Register (write) */
#define SERIAL_LCR  3  /* Line Control Register */
#define SERIAL_MCR  4  /* Modem Control Register */
#define SERIAL_LSR  5  /* Line Status Register */
#define SERIAL_MSR  6  /* Modem Status Register */
#define SERIAL_SR   7  /* Scratch Register */

/* ==========================================
 * Line Status Register (LSR) Bits
 * ========================================== */
#define SERIAL_LSR_DR   0x01  /* Data Ready (data in RBR) */
#define SERIAL_LSR_OE   0x02  /* Overrun Error */
#define SERIAL_LSR_PE   0x04  /* Parity Error */
#define SERIAL_LSR_FE   0x08  /* Framing Error */
#define SERIAL_LSR_BI   0x10  /* Break Interrupt */
#define SERIAL_LSR_THRE 0x20  /* Transmitter Holding Register Empty */
#define SERIAL_LSR_TEMT 0x40  /* Transmitter Empty (both THR and shift register) */
#define SERIAL_LSR_EF   0x80  /* Error in FIFO (16550+) */

/* ==========================================
 * Line Control Register (LCR) Bits
 * ========================================== */
#define SERIAL_LCR_DLAB 0x80  /* Divisor Latch Access Bit */
#define SERIAL_LCR_8N1  0x03  /* 8 bits, no parity, 1 stop bit */

/* ==========================================
 * Modem Control Register (MCR) Bits
 * ========================================== */
#define SERIAL_MCR_DTR  0x01  /* Data Terminal Ready */
#define SERIAL_MCR_RTS  0x02  /* Request To Send */
#define SERIAL_MCR_OUT1 0x04  /* Auxiliary output 1 */
#define SERIAL_MCR_OUT2 0x08  /* Auxiliary output 2 (enable interrupts) */

/* ==========================================
 * Default Configuration
 * ========================================== */
#define SERIAL_DEFAULT_PORT SERIAL_COM1
#define SERIAL_DEFAULT_BAUD 115200

/* ==========================================
 * Serial Error Codes
 * ==========================================
 */
#define SERIAL_ERROR_NONE       0
#define SERIAL_ERROR_TIMEOUT    1
#define SERIAL_ERROR_INIT_FAIL  2
#define SERIAL_ERROR_NULL_PTR   3

/* ==========================================
 * Serial function API
 * ========================================== */

/**
 * @brief Initialize serial port
 * @param port Serial port (e.g., SERIAL_COM1)
 * @param baud Baud rate (e.g., 115200)
 * @return 1 if success, 0 if failure
 */
int serial_init(uint16_t port, uint32_t baud);

/**
 * @brief Initialize COM1 with default baud rate
 * @return 1 if success, 0 if failure
 */
int serial_init_default(void);

/**
 * @brief Check if serial port is initialized
 * @return 1 if initialized, 0 otherwise
 */
int serial_is_initialized(void);

/**
 * @brief Write a character to serial
 * @param data Character to write
 */
void serial_write_char(char data);

/**
 * @brief Write a null-terminated string to serial
 * @param str Null-terminated string
 */
void serial_write_str(const char* str);

/**
 * @brief Write an unsigned integer in hexadecimal to serial
 * @param value Value to write
 */
void serial_write_hex(uint32_t value);

/**
 * @brief Write a 64-bit unsigned integer in hexadecimal to serial
 * @param value 64-bit value to write
 */
void serial_write_hex64(uint64_t value);

/**
 * @brief Write an unsigned integer in decimal to serial
 * @param value Value to write
 */
void serial_write_dec(uint32_t value);

/**
 * @brief Write a 64-bit unsigned integer in decimal to serial
 * @param value 64-bit value to write
 */
void serial_write_dec64(uint64_t value);

/**
 * @brief Write a 32-bit signed integer in decimal to serial
 * @param value Signed value to write
 * @note Handles negative values with '-' prefix
 */
void serial_write_dec_signed(int32_t value);

/**
 * @brief Write a 64-bit signed integer in decimal to serial
 * @param value 64-bit signed value to write
 * @note Handles negative values with '-' prefix
 */
void serial_write_dec64_signed(int64_t value);

/**
 * @brief Read a character from serial (non-blocking)
 * @param data Pointer to store read data
 * @return 1 if data available, 0 otherwise
 */
int serial_read_char(char* data);

/**
 * @brief Wait until transmitter is empty
 * @note Uses internal timeout to prevent infinite hangs
 */
void serial_wait_transmit_empty(void);

/* ==========================================
 * Error Reporting API
 * ==========================================
 * These functions allow diagnosing serial port failures
 * instead of silent failures.
 * ========================================== */

/**
 * @brief Check if serial port has failed
 * @return 1 if failed, 0 if OK or not initialized
 *
 * A failed serial port may still have been initialized successfully,
 * but encountered a hardware error during operation (e.g., timeout).
 */
int serial_has_failed(void);

/**
 * @brief Get the last serial error code
 * @return Error code (0 = none, 1 = timeout, 2 = init fail, 3 = null ptr)
 *
 * Error codes:
 *   SERIAL_ERROR_NONE (0)      - No error
 *   SERIAL_ERROR_TIMEOUT (1)   - Hardware timeout (not responding)
 *   SERIAL_ERROR_INIT_FAIL (2) - Initialization failed
 *   SERIAL_ERROR_NULL_PTR (3)  - Null pointer passed to function
 */
uint32_t serial_get_error_code(void);

/**
 * @brief Get the count of timeout errors
 * @return Number of timeouts since initialization
 *
 * This counter increments each time a serial operation times out.
 * Useful for diagnosing intermittent hardware issues.
 */
uint32_t serial_get_timeout_count(void);

/**
 * @brief Clear the serial error state
 *
 * Resets the failed flag and error code.
 * Useful for recovery attempts or re-initialization.
 */
void serial_clear_error(void);

/**
 * @brief Re-initialize serial port after a timeout or failure
 * @param port Serial port (e.g., SERIAL_COM1)
 * @param baud Baud rate (e.g., 115200)
 * @return 1 if success, 0 if failure
 *
 * This function allows recovery from timeout errors by:
 *   1. Clearing the failed state
 *   2. Resetting error counters
 *   3. Re-initializing the hardware
 *
 * Use this when:
 *   - serial_has_failed() returns 1
 *   - serial_get_error_code() returns SERIAL_ERROR_TIMEOUT
 *   - You want to retry initialization without rebooting
 *
 * @note This is different from serial_init() - it properly clears
 *       the failed state before re-initializing.
 */
int serial_reinit(uint16_t port, uint32_t baud);

/**
 * @brief Re-initialize COM1 with default baud rate
 * @return 1 if success, 0 if failure
 *
 * Convenience wrapper for serial_reinit(SERIAL_DEFAULT_PORT, SERIAL_DEFAULT_BAUD).
 */
int serial_reinit_default(void);

/**
 * @brief Get a human-readable error message
 * @param error_code Error code to translate
 * @return Static string describing the error
 *
 * @note Returns English strings only.
 * @note String is static - do not free or modify.
 */
const char* serial_get_error_string(uint32_t error_code);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_H */
