#include "pit.h"
#include "serial.h"
#include "print.h"
#include "barriers.h"
#include "constants.h"
#include "io.h"

/* =============================================================================
 * PIT Driver State
 * =============================================================================
 * FIX-PIT-001: Added volatile to prevent compiler optimization in SMP.
 * State is updated in IRQ handler and read from other contexts.
 * =============================================================================
 */
static volatile pit_state_t g_pit_state;

/* Custom callback (optional) */
static pit_callback_t g_pit_callback = nullptr;

/* =============================================================================
 * PIT Hardware Access
 * =============================================================================
 */

/**
 * @brief Write divisor to PIT channel 0
 * @param divisor 16-bit divisor value
 * 
 * PIT calculates: output_frequency = PIT_BASE_FREQUENCY / divisor
 */
static void pit_write_divisor(uint16_t divisor) {
    /* Write low byte first, then high byte */
    outb(PIT_CHANNEL_0, divisor & 0xFF);
    io_delay();
    outb(PIT_CHANNEL_0, (divisor >> 8) & 0xFF);
    io_delay();
}

/**
 * @brief Send command to PIT
 * @param command Command byte
 */
static void pit_send_command(uint8_t command) {
    outb(PIT_COMMAND, command);
    io_delay();
}

/* =============================================================================
 * Initialization
 * =============================================================================
 */

int pit_init(void) {
    return pit_init_frequency(PIT_DEFAULT_HZ);
}

int pit_init_frequency(uint32_t frequency_hz) {
    /* Validate frequency */
    if (frequency_hz < PIT_MIN_HZ || frequency_hz > PIT_MAX_HZ) {
        serial_write_str("[PIT] Invalid frequency: ");
        serial_write_dec(frequency_hz);
        serial_write_str(" Hz (must be ");
        serial_write_dec(PIT_MIN_HZ);
        serial_write_str("-");
        serial_write_dec(PIT_MAX_HZ);
        serial_write_str(" Hz)\r\n");
        return 0;
    }

    /* Calculate divisor
     * Formula: divisor = PIT_BASE_FREQUENCY / frequency
     * For 100 Hz: divisor = 1193182 / 100 = 11931
     */
    uint32_t divisor = PIT_BASE_FREQUENCY / frequency_hz;
    
    /* Ensure divisor is in valid range (16-bit) */
    if (divisor > 0xFFFF) {
        divisor = 0xFFFF;
    }
    if (divisor == 0) {
        divisor = 1;
    }

    /* Initialize state */
    g_pit_state.initialized = 0;  /* Not yet ready */
    g_pit_state.ticks = 0;
    g_pit_state.milliseconds = 0;
    g_pit_state.frequency_hz = frequency_hz;
    g_pit_state.divisor = divisor;

    /* Send command to PIT
     * Channel 0, access both bytes, square wave mode, binary
     * Command: 0x36 = 00 (ch0) 11 (both) 011 (mode 3) 0 (binary)
     */
    pit_send_command(PIT_CHANNEL_0_SEL | PIT_ACCESS_BOTH | PIT_MODE_3 | PIT_BINARY);

    /* Write divisor */
    pit_write_divisor((uint16_t)divisor);

    /* Mark as initialized with memory barrier */
    wmb();
    g_pit_state.initialized = 1;
    mb();

    serial_write_str("[PIT] Initialized at ");
    serial_write_dec(frequency_hz);
    serial_write_str(" Hz\r\n");

    return 1;
}

int pit_is_initialized(void) {
    rmb();
    return g_pit_state.initialized;
}

void pit_shutdown(void) {
    /* Reset to default state (18.2 Hz - legacy DOS rate) */
    g_pit_state.initialized = 0;
    g_pit_callback = nullptr;
    
    /* Reprogram to 18.2 Hz */
    pit_init_frequency(18);
}

/* =============================================================================
 * Query Functions
 * =============================================================================
 */

uint64_t pit_get_ticks(void) {
    rmb();
    return g_pit_state.ticks;
}

uint32_t pit_get_frequency(void) {
    rmb();
    return g_pit_state.frequency_hz;
}

uint64_t pit_get_milliseconds(void) {
    rmb();
    return g_pit_state.milliseconds;
}

uint32_t pit_get_seconds(void) {
    rmb();
    return (uint32_t)(g_pit_state.milliseconds / 1000);
}

uint32_t pit_get_divisor(void) {
    rmb();
    return g_pit_state.divisor;
}

/* =============================================================================
 * Control Functions
 * =============================================================================
 */

int pit_set_frequency(uint32_t frequency_hz) {
    if (!g_pit_state.initialized) {
        return 0;
    }
    
    /* Reinitialize with new frequency */
    return pit_init_frequency(frequency_hz);
}

void pit_reset_ticks(void) {
    wmb();
    g_pit_state.ticks = 0;
    g_pit_state.milliseconds = 0;
    wmb();
}

/**
 * HIGH-003 FIX: Handle counter wraparound correctly
 * The millisecond counter will wrap after ~584 million years at 100Hz,
 * but we handle it correctly anyway for correctness.
 */
void pit_wait_ms(uint32_t ms) {
    if (!g_pit_state.initialized || ms == 0) {
        return;
    }

    uint64_t start = pit_get_milliseconds();
    uint64_t end = start + ms;
    
    /* Handle wraparound correctly */
    if (end < start) {
        /* Wraparound will occur - wait until counter wraps past start */
        while (pit_get_milliseconds() >= start) {
            __asm__ volatile("pause");
        }
        /* Now wait for remaining time */
        uint64_t remaining = end;  /* end wrapped to small value */
        while (pit_get_milliseconds() < remaining) {
            __asm__ volatile("pause");
        }
    } else {
        /* No wraparound - simple comparison */
        while (pit_get_milliseconds() < end) {
            __asm__ volatile("pause");
        }
    }
}

void pit_wait_us(uint32_t us) {
    if (!g_pit_state.initialized) {
        return;
    }

    /* For very short delays, use a simple loop
     * Approximate: 1 us ≈ 3 CPU cycles at 3 GHz
     * This is not precise but works for small delays
     */
    /* PIT-LOW-001 FIX: Cap us before multiplying to prevent uint32_t overflow.
     * 0xFFFFFFFF / 3 = 1431655765; above that, us * 3 wraps around. */
    if (us > 0xFFFFFFFFU / 3U) {
        us = 0xFFFFFFFFU / 3U;
    }
    uint32_t iterations = us * 3;  /* Rough calibration */
    
    for (uint32_t i = 0; i < iterations; i++) {
        __asm__ volatile("pause");
    }
}

/* =============================================================================
 * Interrupt Handler
 * =============================================================================
 */

void pit_irq_handler(void) {
    /* Increment tick counter */
    wmb();
    g_pit_state.ticks++;
    
    /* Update milliseconds (at 100 Hz, 100 ticks = 1000 ms) */
    g_pit_state.milliseconds += (1000 / g_pit_state.frequency_hz);
    wmb();

    /* Call custom callback if registered */
    if (g_pit_callback != nullptr) {
        g_pit_callback();
    }
}

void pit_register_callback(pit_callback_t callback) {
    wmb();
    g_pit_callback = callback;
    wmb();
}

void pit_unregister_callback(void) {
    wmb();
    g_pit_callback = nullptr;
    wmb();
}

/* =============================================================================
 * Debug/Testing
 * =============================================================================
 */

void pit_print_stats(void) {
    if (!g_pit_state.initialized) {
        serial_write_str("[PIT] Not initialized\r\n");
        return;
    }

    serial_write_str("\r\n=== PIT Statistics ===\r\n");
    serial_write_str("Frequency: ");
    serial_write_dec(g_pit_state.frequency_hz);
    serial_write_str(" Hz\r\n");
    
    serial_write_str("Divisor: ");
    serial_write_dec(g_pit_state.divisor);
    serial_write_str("\r\n");
    
    serial_write_str("Ticks: ");
    serial_write_dec64(g_pit_state.ticks);
    serial_write_str("\r\n");
    
    serial_write_str("Milliseconds: ");
    serial_write_dec64(g_pit_state.milliseconds);
    serial_write_str("\r\n");
    
    serial_write_str("Seconds: ");
    serial_write_dec(pit_get_seconds());
    serial_write_str("\r\n");

    serial_write_str("======================\r\n");
}

volatile pit_state_t* pit_get_state(void) {
    return &g_pit_state;
}
