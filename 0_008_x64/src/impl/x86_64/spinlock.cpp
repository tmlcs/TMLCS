#include "spinlock.h"
#include "constants.h"

/* =============================================================================
 * x86_64 Spinlock Implementation
 * =============================================================================
 *
 * Uses LOCK CMPXCHG for atomic compare-and-swap operation.
 * The LOCK prefix ensures the operation is atomic across all CPUs.
 *
 * Memory ordering:
 *   - Acquire: Uses LOCK which acts as a full memory barrier
 *   - Release: Uses volatile write with memory barrier
 *
 * Interrupt handling:
 *   - Disables interrupts during spin to prevent deadlock
 *     (if interrupt handler tried to acquire same lock)
 *   - Re-enables interrupts after release
 *
 *   - interrupts_enabled field is now volatile in spinlock_t
 *   - Ensures compiler doesn't cache the value across CPUs
 *   - Critical for correct interrupt state restoration in SMP
 * =============================================================================
 */

/* Flag to track interrupt state */
typedef struct {
    bool interrupts_enabled;
} interrupt_state_t;

/* Save and disable interrupts
 * Uses volatile read to ensure fresh value from spinlock_t
 */
static inline void cli_save(interrupt_state_t* state) {
    /* Check if interrupts are enabled by reading RFLAGS */
    uint64_t rflags;
    __asm__ volatile ("pushfq; pop %0" : "=r"(rflags));
    state->interrupts_enabled = (rflags & 0x200) != 0;  /* Interrupt flag bit */

    /* Disable interrupts */
    __asm__ volatile ("cli" ::: "memory");
}

/* Restore interrupt state
 * Uses volatile read to ensure correct state restoration
 */
static inline void sti_restore(const interrupt_state_t* state) {
    if (state->interrupts_enabled) {
        __asm__ volatile ("sti" ::: "memory");
    }
}

/* Memory barrier macro */
#define memory_barrier() __asm__ volatile ("" ::: "memory")

/* =============================================================================
 * Spinlock Implementation
 * =============================================================================
 */

void spinlock_init(spinlock_t* lock) {
    if (lock == nullptr) {
        return;
    }
    lock->locked = 0;
    lock->interrupts_enabled = false;
    memory_barrier();
}

void spinlock_acquire(spinlock_t* lock) {
    if (lock == nullptr) {
        return;
    }

    /* Disable interrupts to prevent deadlock and save state */
    interrupt_state_t int_state;
    cli_save(&int_state);

    /*
     * Spin until we can atomically set locked from 0 to 1
     * Using LOCK CMPXCHG ensures atomicity across all CPUs
     */
    while (1) {
        uint64_t expected = 0;  /* We expect unlocked state */
        uint64_t desired = 1;   /* We want to lock it */

        /*
         * LOCK CMPXCHG: Atomic compare-and-swap
         * If [lock->locked] == expected: set to desired, return expected
         * If [lock->locked] != expected: return current value, no change
         *
         * The LOCK prefix ensures this is atomic across all CPUs.
         * It also acts as a full memory barrier.
         */
        uint64_t result;
        __asm__ volatile (
            "lock cmpxchg %2, %1"
            : "=a"(result), "+m"(lock->locked)
            : "r"(desired), "a"(expected)
            : "memory"
        );

        /* If result == 0, we successfully acquired the lock */
        if (result == 0) {
            /* Save interrupt state in lock for restore on release
             * interrupts_enabled is volatile, ensuring visibility across CPUs
             */
            lock->interrupts_enabled = int_state.interrupts_enabled;
            memory_barrier();
            break;
        }

        /*
         * Spin with PAUSE instruction to reduce power consumption
         * and improve hyperthreading performance
         */
        __asm__ volatile ("pause" ::: "memory");
    }

    memory_barrier();
}

bool spinlock_try_acquire(spinlock_t* lock) {
    if (lock == nullptr) {
        return false;
    }

    /* Disable interrupts and save state */
    interrupt_state_t int_state;
    cli_save(&int_state);

    uint64_t expected = 0;
    uint64_t desired = 1;

    uint64_t result;
    __asm__ volatile (
        "lock cmpxchg %2, %1"
        : "=a"(result), "+m"(lock->locked)
        : "r"(desired), "a"(expected)
        : "memory"
    );

    memory_barrier();

    /* Return true if we acquired the lock (result == 0) */
    if (result == 0) {
        /* Save interrupt state in lock for restore on release
         * interrupts_enabled is volatile, ensuring visibility across CPUs
         */
        lock->interrupts_enabled = int_state.interrupts_enabled;
        memory_barrier();
        return true;
    }

    /* Failed to acquire - restore interrupt state */
    sti_restore(&int_state);
    return false;
}

void spinlock_release(spinlock_t* lock) {
    if (lock == nullptr) {
        return;
    }

    /* Memory barrier before release */
    memory_barrier();

    /* Release the lock by setting locked to 0 */
    lock->locked = 0;

    /*
     * Restore interrupt state
     * Read interrupts_enabled BEFORE the memory barrier
     * interrupts_enabled is volatile - ensures we see the latest value
     * written by spinlock_acquire() on any CPU.
     */
    bool was_enabled = lock->interrupts_enabled;
    lock->interrupts_enabled = false;  /* Reset for next acquire */

    /* Full memory barrier after release */
    memory_barrier();

    /* Restore interrupt state if it was enabled before acquire */
    if (was_enabled) {
        __asm__ volatile ("sti" ::: "memory");
    }
}

/* =============================================================================
 * VGA Spinlock Instance
 * =============================================================================
 * Global spinlock for protecting VGA text mode operations.
 * This lock must be held when accessing:
 *   - cursor_col, cursor_row
 *   - current_color
 *   - vga_buffer[]
 * =============================================================================
 */

/* Global VGA lock - initialized to unlocked state */
spinlock_t g_vga_lock = SPINLOCK_INIT;

/* =============================================================================
 * Lock/Unlock helpers for VGA driver
 * =============================================================================
 */

void vga_lock(void) {
    spinlock_acquire(&g_vga_lock);
}

void vga_unlock(void) {
    spinlock_release(&g_vga_lock);
}

bool vga_try_lock(void) {
    return spinlock_try_acquire(&g_vga_lock);
}
