#include "spinlock.h"
#include "barriers.h"
#include "constants.h"
#include "serial.h"

/* Use centralized barriers from barriers.h
 * Previous: Local #define barrier()
 * Now: #include "barriers.h" provides consistent barriers
 * @see src/arch/x86_64/include/barriers.h
 */

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
 *
 * @assembly
 *   Instruction 1: pushfq; pop %0
 *     - pushfq: Push RFLAGS register onto stack
 *     - pop %0: Pop into output operand (rflags)
 *     - Purpose: Read current interrupt state
 *     - Effects: Copies RFLAGS to C++ variable
 *     - Cycles: ~3-5
 *
 *   Instruction 2: cli
 *     - Purpose: Clear Interrupt Flag (disable interrupts)
 *     - Effects: IF bit in RFLAGS = 0
 *     - Cycles: ~3
 *     - Barriers: Implicit (privileged instruction)
 *
 * @note RFLAGS bit 9 (IF - Interrupt Flag) controls maskable interrupts
 * @note If IF=1, interrupts enabled; if IF=0, disabled
 * @note 0x200 = bit 9 = Interrupt Flag in RFLAGS
 *
 * @param state Struct to save interrupt state
 *
 * @see sti_restore() to restore state
 */
static inline void cli_save(interrupt_state_t* state) {
    /* Check if interrupts are enabled by reading RFLAGS */
    uint64_t rflags;
    __asm__ volatile("pushfq; pop %0" : "=r"(rflags));
    state->interrupts_enabled = (rflags & 0x200) != 0; /* Interrupt flag bit */

    /* Disable interrupts */
    __asm__ volatile("cli" ::: "memory");
}

/**
 * Restore interrupt state
 * Uses volatile read to ensure correct state restoration
 *
 * @assembly
 *   Instruction: sti
 *     - Purpose: Set Interrupt Flag (enable interrupts)
 *     - Effects: IF bit in RFLAGS = 1
 *     - Cycles: ~3
 *     - Barriers: Implicit (privileged instruction)
 *
 * @note Only executes sti if interrupts were enabled
 * @note Do nothing if they were disabled (correct state)
 *
 * @param state State saved by cli_save()
 *
 * @see cli_save() to save state
 */
static inline void sti_restore(const interrupt_state_t* state) {
    if (state->interrupts_enabled) {
        __asm__ volatile("sti" ::: "memory");
    }
}

/* barrier() now provided by barriers.h */

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
    barrier();
}

void spinlock_acquire(spinlock_t* lock) {
    if (lock == nullptr) {
        return;
    }

    /* =======================================================================
     * Always disable interrupts for SMP safety
     * =======================================================================
     * The previous fast path (try acquire without disabling interrupts)
     * could cause deadlock in nested interrupt context:
     *
     * DEADLOCK SCENARIO (fast path removed):
     *   1. CPU 0 acquires lock via fast path (interrupts still enabled)
     *   2. Interrupt occurs on CPU 0
     *   3. Interrupt handler tries to acquire same lock
     *   4. Interrupt handler takes slow path (lock is held)
     *   5. Interrupt handler spins forever (interrupts disabled in slow path)
     *   6. Original code never resumes to release lock -> DEADLOCK
     *
     * SOLUTION: Always disable interrupts to prevent this scenario.
     * This ensures:
     *   1. No interrupt can preempt while holding lock
     *   2. Interrupt handlers can safely acquire locks (no nested deadlock)
     *   3. Interrupt state is properly restored on release
     *
     * Performance note: The cli/sti overhead is negligible compared to
     * the cost of a cache line bounce in SMP systems.
     * =======================================================================
     */

    /* Disable interrupts and save state BEFORE attempting to acquire */
    interrupt_state_t int_state;
    cli_save(&int_state);

    /*
     * Spin until we can atomically set locked from 0 to 1
     * Using LOCK CMPXCHG ensures atomicity across all CPUs
     *
     * ALGORITHM:
     *   1. We wait for locked == 0 (unlocked)
     *   2. We try to change locked from 0 to 1 atomically
     *   3. If another CPU won, we wait with PAUSE
     *   4. We repeat until we get the lock
     */
    while (1) {
        uint64_t expected = 0; /* We expect unlocked state */
        uint64_t desired = 1;  /* We want to lock it */

        /*
         * LOCK CMPXCHG: Atomic compare-and-swap
         *
         * @assembly
         *   Instruction: lock cmpxchg %2, %1
         *   Operands:
         *     - %0 (output, "=a"): result - value returned in RAX
         *     - %1 (input/output, "+m"): lock->locked - memory to modify
         *     - %2 (input, "r"): desired - value to write (1)
         *     - %3 (input, "a"): expected - expected value (0)
         *
         *   Operation:
         *     IF [lock->locked] == expected (0):
         *       [lock->locked] ← desired (1)
         *       ZF ← 1 (zero flag set)
         *       result ← expected (0)
         *     ELSE:
         *       result ← [lock->locked] (current value)
         *       ZF ← 0 (zero flag clear)
         *
         *   LOCK prefix:
         *     - Locks the memory bus during the operation
         *     - Prevents other CPUs from accessing the same address
         *     - Acts as a full memory barrier (implicit mfence)
         *
         *   Effects:
         *     - Atomicity guaranteed across the entire system (all CPUs)
         *     - Memory ordering: all previous operations
         *       complete before, all subsequent ones wait
         *
         *   Cycles:
         *     - Without contention: ~20-50 cycles
         *     - With contention: variable (depends on wait)
         *
         *   Barriers: LOCK acts as a full barrier (load + store)
         *
         * @note RAX register must contain 'expected' before the instruction
         * @note If ZF=1 (zero flag), lock was acquired (result == 0)
         * @note If ZF=0, another CPU has the lock (result != 0)
         *
         * @see https://www.felixcloutier.com/x86/cmpxchg
         * @see https://www.felixcloutier.com/x86/lock
         */
        uint64_t result;
        __asm__ volatile("lock cmpxchg %2, %1"
                         : "=a"(result), "+m"(lock->locked)
                         : "r"(desired), "a"(expected)
                         : "memory");

        /* If result == 0, we successfully acquired the lock */
        if (result == 0) {
            /* Save interrupt state in lock for restore on release
             * interrupts_enabled is volatile, ensuring visibility across CPUs
             */
            lock->interrupts_enabled = int_state.interrupts_enabled;
            barrier();
            break;
        }

        /*
         * Spin with PAUSE instruction to reduce power consumption
         * and improve hyperthreading performance
         *
         * @assembly
         *   Instruction: pause (SSE2)
         *   Operands: None
         *   Effects:
         *     - Prevents erroneous detection of memory dependencies
         *     - Reduces power consumption during spin loops
         *     - Improves hyperthreading performance (prevents starvation of other thread)
         *   Cycles: ~10-20 (varies by implementation)
         *   Barriers: Soft - does not reorder but gives opportunity to other threads
         *
         * @note Without PAUSE, the spinlock loop can cause:
         *   - Pipeline stalls due to false dependency detection
         *   - Higher power consumption
         *   - Starvation of sibling hyperthread
         *
         * @see https://www.felixcloutier.com/x86/pause
         */
        __asm__ volatile("pause" ::: "memory");
    }

    barrier();
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

    /*
     * LOCK CMPXCHG - See full documentation in spinlock_acquire()
     *
     * @assembly
     *   Instruction: lock cmpxchg
     *   Difference with acquire(): No spin loop - tries once
     *   If fails, restores interrupts and returns false immediately
     */
    uint64_t result;
    __asm__ volatile("lock cmpxchg %2, %1"
                     : "=a"(result), "+m"(lock->locked)
                     : "r"(desired), "a"(expected)
                     : "memory");

    barrier();

    /* Return true if we acquired the lock (result == 0) */
    if (result == 0) {
        /* Save interrupt state in lock for restore on release
         * interrupts_enabled is volatile, ensuring visibility across CPUs
         */
        lock->interrupts_enabled = int_state.interrupts_enabled;
        barrier();
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
    barrier();

    /*
     * CRITICAL: Check if we need to restore interrupts
     * If interrupts was disabled during acquire (slow path), we MUST NOT enable them
     * because they were already disabled before we acquired the lock.
     *
     * interrupts_enabled == true  => We disabled interrupts, must restore
     * interrupts_enabled == false => Fast path, interrupts were never touched
     */
    bool was_enabled = lock->interrupts_enabled;

    /* Release the lock by setting locked to 0 */
    lock->locked = 0;

    /*
     * Restore interrupt state
     * Read interrupts_enabled BEFORE the memory barrier
     * interrupts_enabled is volatile - ensures we see the latest value
     * written by spinlock_acquire() on any CPU.
     */
    lock->interrupts_enabled = false; /* Reset for next acquire */

    /* Full memory barrier after release */
    barrier();

    /*
     * Only restore interrupts if we disabled them during acquire.
     * If was_enabled == false, we got the lock via fast path and
     * interrupts were never disabled.
     */
    if (was_enabled) {
        __asm__ volatile("sti" ::: "memory");
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

/* =============================================================================
 * Serial Spinlock Instance
 * =============================================================================
 * Global spinlock for protecting serial port operations in SMP environments.
 * This lock must be held when accessing:
 *   - UART registers (THR, RBR, LSR, etc.)
 *   - Serial driver state (g_serial_state)
 *
 * Benefits:
 *   - Prevents character interleaving from multiple CPUs
 *   - Ensures atomic multi-character output
 *   - SMP-safe with interrupt handling (same as VGA lock)
 *
 * Usage:
 *   - Internal: All serial_write_* functions acquire lock automatically
 *   - External: Use serial_lock()/serial_unlock() for multi-operation atomicity
 *
 * Tracking issue: #SMP-002 (Serial driver SMP safety)
 * =============================================================================
 */

/* Global serial lock - initialized to unlocked state */
spinlock_t g_serial_lock = SPINLOCK_INIT;

/* =============================================================================
 * Lock/Unlock helpers for serial driver
 * =============================================================================
 */

void serial_lock(void) {
    spinlock_acquire(&g_serial_lock);
}

void serial_unlock(void) {
    spinlock_release(&g_serial_lock);
}

bool serial_try_lock(void) {
    return spinlock_try_acquire(&g_serial_lock);
}

void serial_force_unlock(void) {
    /* HIGH-002 FIX: Reset the serial spinlock for use in exception handlers.
     *
     * If a CPU exception fires while g_serial_lock is held (e.g., a fault
     * inside serial_write_str()), the exception handler cannot call any
     * serial_write_* function — spinlock_acquire() would spin forever.
     *
     * PRECONDITIONS (both must be true before calling this):
     *   1. Interrupts MUST be disabled (cli) — prevents a second CPU or IRQ
     *      from acquiring the lock between the reset and the next acquire.
     *   2. The caller MUST NOT return — this path is for fatal halt sequences
     *      only. Forcing the lock open breaks the mutual exclusion guarantee,
     *      so the lock is permanently invalid after this call.
     *
     * LOW-NEW-001 FIX: Compiler barrier before the store prevents the
     * compiler from reordering prior writes past this unlock under -O2.
     */
    __asm__ volatile("" ::: "memory");
    g_serial_lock.locked = 0;
}
