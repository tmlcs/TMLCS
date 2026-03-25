#ifndef ARCH_ATOMIC_H
#define ARCH_ATOMIC_H

/**
 * @file atomic.h
 * @brief Atomic operations for x86_64 architecture
 *
 * This header provides atomic operations for SMP-safe access to shared variables.
 * All operations use GCC built-in atomic functions which generate appropriate
 * LOCK-prefixed instructions on x86_64.
 *
 * @note All functions are inline for performance (no function call overhead)
 * @note Memory ordering: All operations use __ATOMIC_SEQ_CST (sequential consistency)
 *       unless otherwise specified. This is the strongest ordering guarantee.
 *
 * @see https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
 */

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * 32-bit Atomic Operations
 * =============================================================================
 */

/**
 * @brief Atomically increment a 32-bit value
 * @param ptr Pointer to value to increment
 * @return New value after increment
 *
 * @assembly
 *   Instruction: LOCK XADD or LOCK CMPXCHG (depending on GCC version)
 *   Memory Ordering: Sequential consistency (__ATOMIC_SEQ_CST)
 *   Cycles: ~10-20 (with LOCK prefix, cache line ownership)
 *
 * @note Thread-safe and SMP-safe
 * @note Generates LOCK-prefixed instruction for atomicity
 *
 * @example
 *     volatile uint32_t counter = 0;
 *     uint32_t new_val = atomic_inc32(&counter);
 *     // counter is now 1, new_val == 1
 */
static inline uint32_t atomic_inc32(volatile uint32_t* ptr) {
    return __atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST);
}

/**
 * @brief Atomically decrement a 32-bit value
 * @param ptr Pointer to value to decrement
 * @return New value after decrement
 *
 * @assembly
 *   Instruction: LOCK XADD or LOCK CMPXCHG
 *   Memory Ordering: Sequential consistency (__ATOMIC_SEQ_CST)
 *   Cycles: ~10-20 (with LOCK prefix)
 *
 * @note Thread-safe and SMP-safe
 *
 * @example
 *     volatile uint32_t counter = 10;
 *     uint32_t new_val = atomic_dec32(&counter);
 *     // counter is now 9, new_val == 9
 */
static inline uint32_t atomic_dec32(volatile uint32_t* ptr) {
    return __atomic_sub_fetch(ptr, 1, __ATOMIC_SEQ_CST);
}

/**
 * @brief Atomically add a value to a 32-bit variable
 * @param ptr Pointer to value to modify
 * @param value Value to add (can be negative for subtraction)
 * @return New value after addition
 *
 * @assembly
 *   Instruction: LOCK XADD
 *   Memory Ordering: Sequential consistency (__ATOMIC_SEQ_CST)
 *   Cycles: ~10-20 (with LOCK prefix)
 *
 * @note Thread-safe and SMP-safe
 *
 * @example
 *     volatile uint32_t counter = 100;
 *     uint32_t new_val = atomic_add32(&counter, 50);
 *     // counter is now 150, new_val == 150
 */
static inline uint32_t atomic_add32(volatile uint32_t* ptr, uint32_t value) {
    return __atomic_add_fetch(ptr, value, __ATOMIC_SEQ_CST);
}

/**
 * @brief Atomically subtract a value from a 32-bit variable
 * @param ptr Pointer to value to modify
 * @param value Value to subtract
 * @return New value after subtraction
 *
 * @assembly
 *   Instruction: LOCK XADD with negated value
 *   Memory Ordering: Sequential consistency (__ATOMIC_SEQ_CST)
 *   Cycles: ~10-20 (with LOCK prefix)
 *
 * @note Thread-safe and SMP-safe
 *
 * @example
 *     volatile uint32_t counter = 100;
 *     uint32_t new_val = atomic_sub32(&counter, 50);
 *     // counter is now 50, new_val == 50
 */
static inline uint32_t atomic_sub32(volatile uint32_t* ptr, uint32_t value) {
    return __atomic_sub_fetch(ptr, value, __ATOMIC_SEQ_CST);
}

/**
 * @brief Atomically load a 32-bit value
 * @param ptr Pointer to value to load
 * @return Loaded value
 *
 * @assembly
 *   Instruction: MOV with atomic guarantee
 *   Memory Ordering: Sequential consistency (__ATOMIC_SEQ_CST)
 *   Cycles: ~1-5 (depends on cache state)
 *
 * @note Thread-safe and SMP-safe
 * @note Ensures atomic read even for unaligned addresses
 *
 * @example
 *     volatile uint32_t value = 42;
 *     uint32_t loaded = atomic_load32(&value);
 *     // loaded == 42
 */
static inline uint32_t atomic_load32(volatile uint32_t* ptr) {
    return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
}

/**
 * @brief Atomically store a 32-bit value
 * @param ptr Pointer to value to store
 * @param value Value to store
 *
 * @assembly
 *   Instruction: MOV with atomic guarantee
 *   Memory Ordering: Sequential consistency (__ATOMIC_SEQ_CST)
 *   Cycles: ~1-5 (depends on cache state)
 *
 * @note Thread-safe and SMP-safe
 *
 * @example
 *     volatile uint32_t value = 0;
 *     atomic_store32(&value, 42);
 *     // value is now 42
 */
static inline void atomic_store32(volatile uint32_t* ptr, uint32_t value) {
    __atomic_store_n(ptr, value, __ATOMIC_SEQ_CST);
}

/* =============================================================================
 * 64-bit Atomic Operations
 * =============================================================================
 */

/**
 * @brief Atomically increment a 64-bit value
 * @param ptr Pointer to value to increment
 * @return New value after increment
 *
 * @assembly
 *   Instruction: LOCK XADD (64-bit)
 *   Memory Ordering: Sequential consistency (__ATOMIC_SEQ_CST)
 *   Cycles: ~10-20 (with LOCK prefix)
 *
 * @note Thread-safe and SMP-safe
 *
 * @example
 *     volatile uint64_t counter = 0;
 *     uint64_t new_val = atomic_inc64(&counter);
 *     // counter is now 1, new_val == 1
 */
static inline uint64_t atomic_inc64(volatile uint64_t* ptr) {
    return __atomic_add_fetch(ptr, 1, __ATOMIC_SEQ_CST);
}

/**
 * @brief Atomically decrement a 64-bit value
 * @param ptr Pointer to value to decrement
 * @return New value after decrement
 *
 * @assembly
 *   Instruction: LOCK XADD (64-bit)
 *   Memory Ordering: Sequential consistency (__ATOMIC_SEQ_CST)
 *   Cycles: ~10-20 (with LOCK prefix)
 *
 * @note Thread-safe and SMP-safe
 *
 * @example
 *     volatile uint64_t counter = 10;
 *     uint64_t new_val = atomic_dec64(&counter);
 *     // counter is now 9, new_val == 9
 */
static inline uint64_t atomic_dec64(volatile uint64_t* ptr) {
    return __atomic_sub_fetch(ptr, 1, __ATOMIC_SEQ_CST);
}

/**
 * @brief Atomically add a value to a 64-bit variable
 * @param ptr Pointer to value to modify
 * @param value Value to add (can be negative for subtraction)
 * @return New value after addition
 *
 * @assembly
 *   Instruction: LOCK XADD (64-bit)
 *   Memory Ordering: Sequential consistency (__ATOMIC_SEQ_CST)
 *   Cycles: ~10-20 (with LOCK prefix)
 *
 * @note Thread-safe and SMP-safe
 *
 * @example
 *     volatile uint64_t counter = 100;
 *     uint64_t new_val = atomic_add64(&counter, 50);
 *     // counter is now 150, new_val == 150
 */
static inline uint64_t atomic_add64(volatile uint64_t* ptr, uint64_t value) {
    return __atomic_add_fetch(ptr, value, __ATOMIC_SEQ_CST);
}

/**
 * @brief Atomically subtract a value from a 64-bit variable
 * @param ptr Pointer to value to modify
 * @param value Value to subtract
 * @return New value after subtraction
 *
 * @assembly
 *   Instruction: LOCK XADD (64-bit) with negated value
 *   Memory Ordering: Sequential consistency (__ATOMIC_SEQ_CST)
 *   Cycles: ~10-20 (with LOCK prefix)
 *
 * @note Thread-safe and SMP-safe
 *
 * @example
 *     volatile uint64_t counter = 100;
 *     uint64_t new_val = atomic_sub64(&counter, 50);
 *     // counter is now 50, new_val == 50
 */
static inline uint64_t atomic_sub64(volatile uint64_t* ptr, uint64_t value) {
    return __atomic_sub_fetch(ptr, value, __ATOMIC_SEQ_CST);
}

/**
 * @brief Atomically load a 64-bit value
 * @param ptr Pointer to value to load
 * @return Loaded value
 *
 * @assembly
 *   Instruction: MOV (64-bit) with atomic guarantee
 *   Memory Ordering: Sequential consistency (__ATOMIC_SEQ_CST)
 *   Cycles: ~1-5 (depends on cache state)
 *
 * @note Thread-safe and SMP-safe
 *
 * @example
 *     volatile uint64_t value = 42;
 *     uint64_t loaded = atomic_load64(&value);
 *     // loaded == 42
 */
static inline uint64_t atomic_load64(volatile uint64_t* ptr) {
    return __atomic_load_n(ptr, __ATOMIC_SEQ_CST);
}

/**
 * @brief Atomically store a 64-bit value
 * @param ptr Pointer to value to store
 * @param value Value to store
 *
 * @assembly
 *   Instruction: MOV (64-bit) with atomic guarantee
 *   Memory Ordering: Sequential consistency (__ATOMIC_SEQ_CST)
 *   Cycles: ~1-5 (depends on cache state)
 *
 * @note Thread-safe and SMP-safe
 *
 * @example
 *     volatile uint64_t value = 0;
 *     atomic_store64(&value, 42);
 *     // value is now 42
 */
static inline void atomic_store64(volatile uint64_t* ptr, uint64_t value) {
    __atomic_store_n(ptr, value, __ATOMIC_SEQ_CST);
}

/* =============================================================================
 * Compare-and-Swap (CAS) Operations
 * =============================================================================
 */

/**
 * @brief Atomically compare and swap 32-bit value
 * @param ptr Pointer to value to modify
 * @param expected Expected current value
 * @param desired New value to store if expected matches
 * @return true if swap was successful, false otherwise
 *
 * @assembly
 *   Instruction: LOCK CMPXCHG
 *   Memory Ordering: Sequential consistency (__ATOMIC_SEQ_CST)
 *   Cycles: ~10-20 (with LOCK prefix)
 *
 * Algorithm:
 *   if (*ptr == expected) {
 *       *ptr = desired;
 *       return true;
 *   } else {
 *       expected = *ptr;  // Update expected with actual value
 *       return false;
 *   }
 *
 * @note Thread-safe and SMP-safe
 * @note On failure, expected is updated with actual value
 *
 * @example
 *     volatile uint32_t value = 10;
 *     uint32_t expected = 10;
 *     bool success = atomic_compare_exchange32(&value, &expected, 20);
 *     // success == true, value is now 20
 *
 * @example
 *     volatile uint32_t value = 10;
 *     uint32_t expected = 15;  // Wrong expectation
 *     bool success = atomic_compare_exchange32(&value, &expected, 20);
 *     // success == false, value unchanged, expected is now 10
 */
static inline bool atomic_compare_exchange32(volatile uint32_t* ptr, uint32_t* expected,
                                             uint32_t desired) {
    return __atomic_compare_exchange_n(ptr, expected, desired, 0, /* weak = false (strong CAS) */
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

/**
 * @brief Atomically compare and swap 64-bit value
 * @param ptr Pointer to value to modify
 * @param expected Expected current value
 * @param desired New value to store if expected matches
 * @return true if swap was successful, false otherwise
 *
 * @assembly
 *   Instruction: LOCK CMPXCHG (64-bit)
 *   Memory Ordering: Sequential consistency (__ATOMIC_SEQ_CST)
 *   Cycles: ~10-20 (with LOCK prefix)
 *
 * @note Thread-safe and SMP-safe
 * @note On failure, expected is updated with actual value
 *
 * @example
 *     volatile uint64_t value = 10;
 *     uint64_t expected = 10;
 *     bool success = atomic_compare_exchange64(&value, &expected, 20);
 *     // success == true, value is now 20
 */
static inline bool atomic_compare_exchange64(volatile uint64_t* ptr, uint64_t* expected,
                                             uint64_t desired) {
    return __atomic_compare_exchange_n(ptr, expected, desired, 0, /* weak = false (strong CAS) */
                                       __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

/* =============================================================================
 * Relaxed Memory Ordering Variants (for counters and statistics)
 * =============================================================================
 * These variants use __ATOMIC_RELAXED which provides atomicity but
 * no ordering guarantees. Use for counters where exact ordering
 * doesn't matter (e.g., statistics, profiling).
 *
 * Performance: ~2-5x faster than SEQ_CST variants on some workloads
 * =============================================================================
 */

/**
 * @brief Atomically increment 32-bit value with relaxed ordering
 * @param ptr Pointer to value to increment
 * @return New value after increment
 *
 * @note Atomic but no ordering guarantees
 * @note Use for statistics counters where exact ordering doesn't matter
 * @note Faster than atomic_inc32() but less safe for synchronization
 */
static inline uint32_t atomic_inc32_relaxed(volatile uint32_t* ptr) {
    return __atomic_add_fetch(ptr, 1, __ATOMIC_RELAXED);
}

/**
 * @brief Atomically increment 64-bit value with relaxed ordering
 * @param ptr Pointer to value to increment
 * @return New value after increment
 *
 * @note Atomic but no ordering guarantees
 * @note Use for statistics counters where exact ordering doesn't matter
 */
static inline uint64_t atomic_inc64_relaxed(volatile uint64_t* ptr) {
    return __atomic_add_fetch(ptr, 1, __ATOMIC_RELAXED);
}

#ifdef __cplusplus
}
#endif

#endif /* ARCH_ATOMIC_H */
