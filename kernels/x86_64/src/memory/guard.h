#ifndef MEMORY_GUARD_H
#define MEMORY_GUARD_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Install the stack guard page (HIGH-005 fix)
 *
 * Splits the 2MiB huge page that contains @c stack_guard into 4KiB entries
 * and marks the guard page not-present.  Any stack overflow that reaches
 * the guard page triggers a #PF instead of silently corrupting the boot
 * page tables that live immediately below.
 *
 * Must be called after serial is initialised (uses serial_write_str).
 * Must be called before enabling interrupts (modifies live page tables).
 */
void stack_guard_init(void);

#ifdef __cplusplus
}
#endif

#endif /* MEMORY_GUARD_H */
