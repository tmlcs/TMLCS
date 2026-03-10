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
 * 
 * @assembly
 *   Instrucción 1: pushfq; pop %0
 *     - pushfq: Push RFLAGS register onto stack
 *     - pop %0: Pop into output operand (rflags)
 *     - Propósito: Leer estado actual de interrupciones
 *     - Efectos: Copia RFLAGS a variable C++
 *     - Ciclos: ~3-5
 *   
 *   Instrucción 2: cli
 *     - Propósito: Clear Interrupt Flag (deshabilitar interrupciones)
 *     - Efectos: IF bit en RFLAGS = 0
 *     - Ciclos: ~3
 *     - Barreras: Implícita (instrucción privilegiada)
 * 
 * @note RFLAGS bit 9 (IF - Interrupt Flag) controla interrupciones maskables
 * @note Si IF=1, interrupciones habilitadas; si IF=0, deshabilitadas
 * @note 0x200 = bit 9 = Interrupt Flag en RFLAGS
 * 
 * @param state Struct para guardar estado de interrupciones
 * 
 * @see sti_restore() para restaurar estado
 */
static inline void cli_save(interrupt_state_t* state) {
    /* Check if interrupts are enabled by reading RFLAGS */
    uint64_t rflags;
    __asm__ volatile ("pushfq; pop %0" : "=r"(rflags));
    state->interrupts_enabled = (rflags & 0x200) != 0;  /* Interrupt flag bit */

    /* Disable interrupts */
    __asm__ volatile ("cli" ::: "memory");
}

/**
 * Restore interrupt state
 * Uses volatile read to ensure correct state restoration
 * 
 * @assembly
 *   Instrucción: sti
 *     - Propósito: Set Interrupt Flag (habilitar interrupciones)
 *     - Efectos: IF bit en RFLAGS = 1
 *     - Ciclos: ~3
 *     - Barreras: Implícita (instrucción privilegiada)
 * 
 * @note Solo ejecuta sti si las interrupciones estaban habilitadas
 * @note No hacer nada si estaban deshabilitadas (estado correcto)
 * 
 * @param state Estado guardado por cli_save()
 * 
 * @see cli_save() para guardar estado
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

    /* =======================================================================
     * FAST PATH: Try to acquire without disabling interrupts
     * =======================================================================
     * Most of the time, the lock is uncontended. We can acquire it atomically
     * without touching interrupt state. This is critical for:
     *   1. Single-CPU systems (QEMU default): prevents deadlock
     *   2. Performance: avoids unnecessary cli/sti overhead
     *   3. Nested locks: allows acquiring multiple locks safely
     */
    {
        uint64_t expected = 0;
        uint64_t desired = 1;
        uint64_t result;

        __asm__ volatile (
            "lock cmpxchg %2, %1"
            : "=a"(result), "+m"(lock->locked)
            : "r"(desired), "a"(expected)
            : "memory"
        );

        /* If result == 0, we acquired the lock without contention */
        if (result == 0) {
            /* Mark that we didn't disable interrupts (not needed for fast path) */
            lock->interrupts_enabled = false;
            memory_barrier();
            return;  /* Fast path success - return early */
        }
    }

    /* =======================================================================
     * SLOW PATH: Lock is contended, need to spin with interrupts disabled
     * =======================================================================
     * We failed the fast path, meaning another CPU holds the lock.
     * Now we must:
     *   1. Disable interrupts (prevent deadlock with ISR)
     *   2. Spin until we acquire the lock
     *   3. Remember interrupt state for restore on release
     */
    interrupt_state_t int_state;
    cli_save(&int_state);

    /*
     * Spin until we can atomically set locked from 0 to 1
     * Using LOCK CMPXCHG ensures atomicity across all CPUs
     *
     * ALGORITMO:
     *   1. Esperamos que locked == 0 (desbloqueado)
     *   2. Intentamos cambiar locked de 0 a 1 atómicamente
     *   3. Si otro CPU ganó, esperamos con PAUSE
     *   4. Repetimos hasta conseguir el lock
     */
    while (1) {
        uint64_t expected = 0;  /* We expect unlocked state */
        uint64_t desired = 1;   /* We want to lock it */

        /*
         * LOCK CMPXCHG: Atomic compare-and-swap
         *
         * @assembly
         *   Instrucción: lock cmpxchg %2, %1
         *   Operandos:
         *     - %0 (output, "=a"): result - valor devuelto en RAX
         *     - %1 (input/output, "+m"): lock->locked - memoria a modificar
         *     - %2 (input, "r"): desired - valor a escribir (1)
         *     - %3 (input, "a"): expected - valor esperado (0)
         *
         *   Funcionamiento:
         *     IF [lock->locked] == expected (0):
         *       [lock->locked] ← desired (1)
         *       ZF ← 1 (zero flag set)
         *       result ← expected (0)
         *     ELSE:
         *       result ← [lock->locked] (valor actual)
         *       ZF ← 0 (zero flag clear)
         *
         *   Prefijo LOCK:
         *     - Bloquea el bus de memoria durante la operación
         *     - Previene que otros CPUs accedan a la misma dirección
         *     - Actúa como barrera de memoria completa (mfence implícito)
         *
         *   Efectos:
         *     - Atomicidad garantizada en todo el sistema (todos los CPUs)
         *     - Ordenamiento de memoria: todas las operaciones anteriores
         *       completan antes, todas las posteriores esperan
         *
         *   Ciclos:
         *     - Sin contención: ~20-50 ciclos
         *     - Con contención: variable (depende de la espera)
         *
         *   Barreras: LOCK actúa como barrera completa (load + store)
         *
         * @note El registro RAX debe contener 'expected' antes de la instrucción
         * @note Si ZF=1 (zero flag), se adquirió el lock (result == 0)
         * @note Si ZF=0, otro CPU tiene el lock (result != 0)
         *
         * @see https://www.felixcloutier.com/x86/cmpxchg
         * @see https://www.felixcloutier.com/x86/lock
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
         *
         * @assembly
         *   Instrucción: pause (SSE2)
         *   Operandos: Ninguno
         *   Efectos:
         *     - Previene detección errónea de dependencias de memoria
         *     - Reduce consumo de energía durante spin loops
         *     - Mejora performance en hyperthreading (evita starvation del otro thread)
         *   Ciclos: ~10-20 (varía por implementación)
         *   Barreras: Suave - no reordena pero da oportunidad a otros threads
         *
         * @note Sin PAUSE, el bucle spinlock puede causar:
         *   - Pipeline stalls por detección falsa de dependencias
         *   - Mayor consumo de energía
         *   - Starvation del hyperthread hermano
         *
         * @see https://www.felixcloutier.com/x86/pause
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

    /*
     * LOCK CMPXCHG - Ver documentación completa en spinlock_acquire()
     * 
     * @assembly
     *   Instrucción: lock cmpxchg
     *   Diferencia con acquire(): No hay bucle spin - intenta una vez
     *   Si falla, restaura interrupciones y retorna false inmediatamente
     */
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
    lock->interrupts_enabled = false;  /* Reset for next acquire */

    /* Full memory barrier after release */
    memory_barrier();

    /*
     * Only restore interrupts if we disabled them during acquire.
     * If was_enabled == false, we got the lock via fast path and
     * interrupts were never disabled.
     */
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
