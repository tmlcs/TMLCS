#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * SPINLOCK TOKEN API
 * =============================================================================
 *
 * Non-reentrant interrupt-disabling CAS spinlock for x86_64.
 *
 * Acquire captures the full RFLAGS register (including IF) into an opaque
 * spinlock_token_t on the caller's stack frame BEFORE cli. Release checks
 * bit 9 (IF) of the saved RFLAGS and conditionally restores sti. This keeps
 * the interrupt-state decision per-CPU and per-call, eliminating:
 *   - STI inside exception/IRQ handlers (CRIT-1)
 *   - SMP race on the saved flag field (CRIT-2/3)
 *
 * Usage:
 *   spinlock_t my_lock = SPINLOCK_INIT;
 *
 *   spinlock_token_t tok = spinlock_acquire(&my_lock);
 *   // ... critical section ...
 *   spinlock_release(&my_lock, tok);
 *
 * Always initialise global/static locks with = SPINLOCK_INIT.
 * Never acquire the same lock recursively — spins forever with IF=0.
 * =============================================================================
 */

/** Opaque token returned by spinlock_acquire. Holds the RFLAGS snapshot
 *  captured before cli. Pass it unmodified to spinlock_release. */
typedef struct {
    uint64_t rflags;
} spinlock_token_t;

/** Spinlock structure. Only field is the CAS word. */
typedef struct {
    volatile uint64_t locked; /* 0 = unlocked, 1 = locked */
} spinlock_t;

/** Static initializer — compatible with C and C++ aggregate init. */
#define SPINLOCK_INIT {0}

/* =============================================================================
 * Core API
 * =============================================================================
 */

void             spinlock_init(spinlock_t* lock);
spinlock_token_t spinlock_acquire(spinlock_t* lock);
bool             spinlock_try_acquire(spinlock_t* lock, spinlock_token_t* out_tok);
void             spinlock_release(spinlock_t* lock, spinlock_token_t tok);

/* =============================================================================
 * VGA Driver Lock (global instance lives in spinlock.cpp)
 * =============================================================================
 */

spinlock_token_t vga_lock(void);
void             vga_unlock(spinlock_token_t tok);
bool             vga_try_lock(spinlock_token_t* out_tok);

/* =============================================================================
 * Serial Driver Lock (global instance lives in spinlock.cpp)
 * =============================================================================
 */

spinlock_token_t serial_lock(void);
void             serial_unlock(spinlock_token_t tok);
bool             serial_try_lock(spinlock_token_t* out_tok);
void             serial_force_unlock(void);

/** Reset g_vga_lock to unlocked state without restoring IF.
 *  For use ONLY at the top of default_exception_handler(). */
void vga_force_unlock(void);

#ifdef __cplusplus
}
#endif

#endif /* SPINLOCK_H */
