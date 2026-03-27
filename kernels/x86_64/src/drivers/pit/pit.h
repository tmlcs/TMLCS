#ifndef PIT_H
#define PIT_H

#include "stddef.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * PIT (Programmable Interval Timer) Driver
 * =============================================================================
 *
 * Hardware: Intel 8253/8254 Programmable Interval Timer
 *
 * The PIT is a legacy timer chip that generates periodic interrupts.
 * It has 3 independent channels:
 *   - Channel 0: System timer (IRQ0) - Used for scheduling
 *   - Channel 1: DRAM refresh (obsolete)
 *   - Channel 2: Speaker/PC speaker
 *
 * Input frequency: 1.193182 MHz (1193182 Hz)
 * Output frequency: Programmable via divisor
 *
 * @note Modern systems may use HPET or APIC timer instead
 * @note PIT is available on all x86 systems for compatibility
 * =============================================================================
 */

/* =============================================================================
 * PIT I/O Ports
 * =============================================================================
 */
#define PIT_CHANNEL_0 0x40 /* Channel 0 data port */
#define PIT_CHANNEL_1 0x41 /* Channel 1 data port */
#define PIT_CHANNEL_2 0x42 /* Channel 2 data port */
#define PIT_COMMAND 0x43   /* Command register port */

/* =============================================================================
 * PIT Command Register Bits
 * =============================================================================
 */
/* Channel selection (bits 6-7) */
#define PIT_CHANNEL_0_SEL 0x00 /* Select channel 0 */
#define PIT_CHANNEL_1_SEL 0x40 /* Select channel 1 */
#define PIT_CHANNEL_2_SEL 0x80 /* Select channel 2 */

/* Access mode (bits 4-5) */
#define PIT_ACCESS_LATCH 0x00 /* Latch command */
#define PIT_ACCESS_LO 0x10    /* Access low byte only */
#define PIT_ACCESS_HI 0x20    /* Access high byte only */
#define PIT_ACCESS_BOTH 0x30  /* Access low then high byte */

/* Operating mode (bits 1-3) */
#define PIT_MODE_0 0x00 /* Interrupt on terminal count */
#define PIT_MODE_1 0x02 /* Hardware retriggerable one-shot */
#define PIT_MODE_2 0x04 /* Rate generator */
#define PIT_MODE_3 0x06 /* Square wave generator */
#define PIT_MODE_4 0x08 /* Software triggered strobe */
#define PIT_MODE_5 0x0A /* Hardware triggered strobe */

/* BCD/Binary mode (bit 0) */
#define PIT_BINARY 0x00 /* 16-bit binary */
#define PIT_BCD 0x01    /* 4-digit BCD */

/* =============================================================================
 * PIT Constants
 * =============================================================================
 */
#define PIT_BASE_FREQUENCY 1193182 /* Base frequency in Hz (1.193182 MHz) */
#define PIT_DEFAULT_HZ 100         /* Default interrupt frequency (100 Hz) */
#define PIT_MIN_HZ 1               /* Minimum frequency */
#define PIT_MAX_HZ 1000            /* Maximum frequency */

/* =============================================================================
 * PIT Driver State
 * =============================================================================
 * FIX-PIT-001: All fields should be accessed as volatile in SMP environments.
 * State is updated in IRQ0 handler and read from other contexts.
 *
 * @note Use memory barriers (rmb()/mb()) when reading multiple fields
 * @note ticks and milliseconds are updated in interrupt context
 * =============================================================================
 */
typedef struct {
    volatile int initialized;       /* 1 if PIT initialized */
    volatile uint32_t frequency_hz; /* Current frequency in Hz */
    volatile uint32_t divisor;      /* Current divisor value */
    volatile uint64_t ticks;        /* Total ticks since init */
    volatile uint64_t milliseconds; /* Total milliseconds since init */
} pit_state_t;

/* =============================================================================
 * PIT API - Initialization
 * =============================================================================
 */

/**
 * @brief Initialize PIT with default frequency (100 Hz)
 * @return 1 on success, 0 on failure
 *
 * Configures channel 0 in square wave mode (mode 3)
 * with 100 Hz output frequency (10ms period).
 */
int pit_init(void);

/**
 * @brief Initialize PIT with custom frequency
 * @param frequency_hz Desired frequency in Hz (1-1000)
 * @return 1 on success, 0 on failure
 *
 * Calculates appropriate divisor for requested frequency.
 * Valid range: 1 Hz to 1000 Hz.
 */
int pit_init_frequency(uint32_t frequency_hz);

/**
 * @brief Check if PIT is initialized
 * @return 1 if initialized, 0 otherwise
 */
int pit_is_initialized(void);

/**
 * @brief Shutdown PIT (restore default state)
 */
void pit_shutdown(void);

/* =============================================================================
 * PIT API - Query Functions
 * =============================================================================
 */

/**
 * @brief Get current tick count
 * @return Total ticks since initialization
 *
 * @note Thread-safe (uses atomic read if available)
 */
uint64_t pit_get_ticks(void);

/**
 * @brief Get current frequency
 * @return Frequency in Hz
 */
uint32_t pit_get_frequency(void);

/**
 * @brief Get milliseconds since initialization
 * @return Total milliseconds
 */
uint64_t pit_get_milliseconds(void);

/**
 * @brief Get seconds since initialization
 * @return Total seconds (truncated)
 */
uint32_t pit_get_seconds(void);

/**
 * @brief Get tick divisor
 * @return Current divisor value
 */
uint32_t pit_get_divisor(void);

/* =============================================================================
 * PIT API - Control Functions
 * =============================================================================
 */

/**
 * @brief Change PIT frequency
 * @param frequency_hz New frequency in Hz
 * @return 1 on success, 0 on failure
 *
 * Recalculates divisor and reprograms PIT.
 * Valid range: 1 Hz to 1000 Hz.
 */
int pit_set_frequency(uint32_t frequency_hz);

/**
 * @brief Reset tick counter to zero
 */
void pit_reset_ticks(void);

/**
 * @brief Wait for specified milliseconds
 * @param ms Milliseconds to wait
 *
 * Busy-waits using PIT ticks.
 * Less accurate than sleep but works without scheduler.
 */
void pit_wait_ms(uint32_t ms);

/**
 * @brief Wait for specified microseconds
 * @param us Microseconds to wait
 *
 * Uses PIT with calibration for short delays.
 */
void pit_wait_us(uint32_t us);

/* =============================================================================
 * PIT API - Interrupt Handler
 * =============================================================================
 */

/**
 * @brief PIT timer interrupt handler (IRQ0)
 *
 * Called from assembly IRQ stub when timer fires.
 * Increments tick counter and updates time tracking.
 *
 * @note Must be fast - runs with interrupts disabled
 * @note Called at 100 Hz by default (every 10ms)
 */
void pit_irq_handler(void);

/**
 * @brief Register custom timer callback
 * @param callback Function to call on each tick
 *
 * Callback is called from interrupt context.
 * Keep it short and avoid blocking operations.
 */
typedef void (*pit_callback_t)(void);
void pit_register_callback(pit_callback_t callback);

/**
 * @brief Unregister custom timer callback
 */
void pit_unregister_callback(void);

/* =============================================================================
 * PIT API - Debug/Testing
 * =============================================================================
 */

/**
 * @brief Print PIT statistics
 *
 * Outputs frequency, tick count, uptime via serial.
 */
void pit_print_stats(void);

/**
 * @brief Get PIT driver state
 * @return Pointer to internal state structure (volatile)
 *
 * FIX-PIT-001: Returns volatile pointer for SMP safety.
 * State is updated in IRQ handler and read from other contexts.
 *
 * @note For testing/debugging only
 * @note Use memory barriers when reading multiple fields
 */
volatile pit_state_t* pit_get_state(void);

#ifdef __cplusplus
}
#endif

#endif /* PIT_H */
