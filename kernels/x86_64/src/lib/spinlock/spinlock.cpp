#include "spinlock.h"
#include "barriers.h"
#include "constants.h"
#include "serial.h"

/* =============================================================================
 * Internal helpers
 * =============================================================================
 */

static inline uint64_t read_rflags(void) {
    uint64_t rflags;
    __asm__ volatile("pushfq; pop %0" : "=r"(rflags));
    return rflags;
}

/* =============================================================================
 * Spinlock Implementation
 * =============================================================================
 */

void spinlock_init(spinlock_t* lock) {
    if (lock == nullptr) {
        return;
    }
    lock->locked = 0;
    barrier();
}

spinlock_token_t spinlock_acquire(spinlock_t* lock) {
    if (lock == nullptr) {
        spinlock_token_t empty = {0};
        return empty;
    }

    spinlock_token_t tok;
    tok.rflags = read_rflags();
    __asm__ volatile("cli" ::: "memory");

    while (1) {
        uint64_t expected = 0;
        uint64_t desired  = 1;
        uint64_t result;
        __asm__ volatile("lock cmpxchg %2, %1"
                         : "=a"(result), "+m"(lock->locked)
                         : "r"(desired), "a"(expected)
                         : "memory");
        if (result == 0) {
            barrier();
            return tok;
        }
        __asm__ volatile("pause" ::: "memory");
    }
}

bool spinlock_try_acquire(spinlock_t* lock, spinlock_token_t* out_tok) {
    if (lock == nullptr || out_tok == nullptr) {
        return false;
    }

    spinlock_token_t tok;
    tok.rflags = read_rflags();
    __asm__ volatile("cli" ::: "memory");

    uint64_t expected = 0;
    uint64_t desired  = 1;
    uint64_t result;
    __asm__ volatile("lock cmpxchg %2, %1"
                     : "=a"(result), "+m"(lock->locked)
                     : "r"(desired), "a"(expected)
                     : "memory");
    barrier();

    if (result == 0) {
        *out_tok = tok;
        return true;
    }

    /* Failed — restore IF if it was enabled before our cli */
    if (tok.rflags & 0x200) {
        __asm__ volatile("sti" ::: "memory");
    }
    return false;
}

void spinlock_release(spinlock_t* lock, spinlock_token_t tok) {
    if (lock == nullptr) {
        return;
    }
    barrier();
    lock->locked = 0;
    barrier();
    if (tok.rflags & 0x200) {
        __asm__ volatile("sti" ::: "memory");
    }
}

/* =============================================================================
 * VGA Spinlock Instance and Wrappers
 * =============================================================================
 */

spinlock_t g_vga_lock = SPINLOCK_INIT;

spinlock_token_t vga_lock(void) {
    return spinlock_acquire(&g_vga_lock);
}

void vga_unlock(spinlock_token_t tok) {
    spinlock_release(&g_vga_lock, tok);
}

bool vga_try_lock(spinlock_token_t* out_tok) {
    return spinlock_try_acquire(&g_vga_lock, out_tok);
}

/* =============================================================================
 * Serial Spinlock Instance and Wrappers
 * =============================================================================
 */

spinlock_t g_serial_lock = SPINLOCK_INIT;

spinlock_token_t serial_lock(void) {
    return spinlock_acquire(&g_serial_lock);
}

void serial_unlock(spinlock_token_t tok) {
    spinlock_release(&g_serial_lock, tok);
}

bool serial_try_lock(spinlock_token_t* out_tok) {
    return spinlock_try_acquire(&g_serial_lock, out_tok);
}

void serial_force_unlock(void) {
    /* Reset serial lock for use in exception handlers.
     * PRECONDITIONS: interrupts must be disabled; caller must not return.
     * Compiler barrier prevents reordering of prior writes past this store. */
    __asm__ volatile("" ::: "memory");
    g_serial_lock.locked = 0;
}

void vga_force_unlock(void) {
    /* Mirrors serial_force_unlock contract:
     * Interrupts MUST be disabled. Caller MUST NOT return (lock is invalid after this).
     * Compiler barrier prevents reordering of prior writes past this store. */
    __asm__ volatile("" ::: "memory");
    g_vga_lock.locked = 0;
}
