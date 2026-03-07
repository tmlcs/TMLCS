#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================
 * Puertos Serial estándar (UART 16550)
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
 * Registros UART 16550
 * ==========================================
 * Nota: Todos los registros se acceden vía el puerto base
 * El registro accesible depende del DLAB (Divisor Latch Access Bit)
 */

/* Cuando DLAB = 0 */
#define SERIAL_RBR 0  /* Receiver Buffer Register (read) */
#define SERIAL_THR  0  /* Transmitter Holding Register (write) */
#define SERIAL_IER  1  /* Interrupt Enable Register */

/* Cuando DLAB = 1 */
#define SERIAL_DLL  0  /* Divisor Latch Low byte */
#define SERIAL_DLM  1  /* Divisor Latch High byte */

/* Registros comunes (independientes de DLAB) */
#define SERIAL_IIR  2  /* Interrupt Identification Register (read) */
#define SERIAL_FCR  2  /* FIFO Control Register (write) */
#define SERIAL_LCR  3  /* Line Control Register */
#define SERIAL_MCR  4  /* Modem Control Register */
#define SERIAL_LSR  5  /* Line Status Register */
#define SERIAL_MSR  6  /* Modem Status Register */
#define SERIAL_SR   7  /* Scratch Register */

/* ==========================================
 * Bits del Line Status Register (LSR)
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
 * Bits del Line Control Register (LCR)
 * ========================================== */
#define SERIAL_LCR_DLAB 0x80  /* Divisor Latch Access Bit */
#define SERIAL_LCR_8N1  0x03  /* 8 bits, no parity, 1 stop bit */

/* ==========================================
 * Bits del Modem Control Register (MCR)
 * ========================================== */
#define SERIAL_MCR_DTR  0x01  /* Data Terminal Ready */
#define SERIAL_MCR_RTS  0x02  /* Request To Send */
#define SERIAL_MCR_OUT1 0x04  /* Auxiliary output 1 */
#define SERIAL_MCR_OUT2 0x08  /* Auxiliary output 2 (enable interrupts) */

/* ==========================================
 * Configuración por defecto
 * ========================================== */
#define SERIAL_DEFAULT_PORT SERIAL_COM1
#define SERIAL_DEFAULT_BAUD 115200

/* ==========================================
 * API de funciones serial
 * ========================================== */

/**
 * @brief Inicializar el puerto serial
 * @param port Puerto serial (ej: SERIAL_COM1)
 * @param baud Baud rate (ej: 115200)
 * @return 1 si éxito, 0 si falla
 */
int serial_init(uint16_t port, uint32_t baud);

/**
 * @brief Inicializar COM1 con baud rate por defecto
 * @return 1 si éxito, 0 si falla
 */
int serial_init_default(void);

/**
 * @brief Verificar si el puerto serial está inicializado
 * @return 1 si inicializado, 0 si no
 */
int serial_is_initialized(void);

/**
 * @brief Escribir un caracter por serial
 * @param data Caracter a escribir
 */
void serial_write_char(char data);

/**
 * @brief Escribir un string por serial
 * @param str String null-terminated
 */
void serial_write_str(const char* str);

/**
 * @brief Escribir un entero en hexadecimal por serial
 * @param value Valor a escribir
 */
void serial_write_hex(uint32_t value);

/**
 * @brief Escribir un entero de 64-bit en hexadecimal por serial
 * @param value Valor de 64-bit a escribir
 */
void serial_write_hex64(uint64_t value);

/**
 * @brief Escribir un entero en decimal por serial
 * @param value Valor a escribir
 */
void serial_write_dec(uint32_t value);

/**
 * @brief Escribir un entero de 64-bit en decimal por serial
 * @param value Valor de 64-bit a escribir
 */
void serial_write_dec64(uint64_t value);

/**
 * @brief Leer un caracter de serial (non-blocking)
 * @param data Puntero para almacenar el dato leído
 * @return 1 si hay dato disponible, 0 si no
 */
int serial_read_char(char* data);

/**
 * @brief Esperar hasta que se pueda escribir
 * @note Usa timeout interno para prevenir hangs infinitos
 */
void serial_wait_transmit_empty(void);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_H */
