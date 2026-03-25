#ifndef CORE_STDINT_H
#define CORE_STDINT_H

/* =============================================================================
 * GLOBEX_OS Standard Integer Types
 * =============================================================================
 * 
 * Freestanding implementation for x86_64 architecture.
 * Compliant with C99/C11 stdint.h specification.
 * 
 * @note All types are based on x86_64 data model (LP64)
 * @note No dependencies on system headers
 * =============================================================================
 */

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Exact-Width Integer Types
 * =============================================================================
 * Types with exactly the specified width in bits.
 * Available on all x86_64 implementations.
 * =============================================================================
 */

/* Signed exact-width integer types */
typedef signed char        int8_t;    /**< 8-bit signed integer */
typedef signed short       int16_t;   /**< 16-bit signed integer */
typedef signed int         int32_t;   /**< 32-bit signed integer */
typedef signed long        int64_t;   /**< 64-bit signed integer */

/* Unsigned exact-width integer types */
typedef unsigned char      uint8_t;   /**< 8-bit unsigned integer */
typedef unsigned short     uint16_t;  /**< 16-bit unsigned integer */
typedef unsigned int       uint32_t;  /**< 32-bit unsigned integer */
typedef unsigned long      uint64_t;  /**< 64-bit unsigned integer */

/* =============================================================================
 * Minimum-Width Integer Types
 * =============================================================================
 * Types with at least the specified width in bits.
 * Use when exact width is not required but minimum is.
 * =============================================================================
 */

/* Signed minimum-width integer types */
typedef int8_t             int_least8_t;   /**< At least 8-bit signed integer */
typedef int16_t            int_least16_t;  /**< At least 16-bit signed integer */
typedef int32_t            int_least32_t;  /**< At least 32-bit signed integer */
typedef int64_t            int_least64_t;  /**< At least 64-bit signed integer */

/* Unsigned minimum-width integer types */
typedef uint8_t            uint_least8_t;  /**< At least 8-bit unsigned integer */
typedef uint16_t           uint_least16_t; /**< At least 16-bit unsigned integer */
typedef uint32_t           uint_least32_t; /**< At least 32-bit unsigned integer */
typedef uint64_t           uint_least64_t; /**< At least 64-bit unsigned integer */

/* =============================================================================
 * Fastest Minimum-Width Integer Types
 * =============================================================================
 * Types with at least the specified width, optimized for speed.
 * On x86_64, int/uint are fastest for 32-bit, long/ulong for 64-bit.
 * =============================================================================
 */

/* Signed fastest minimum-width integer types */
typedef int8_t             int_fast8_t;    /**< Fastest at least 8-bit signed */
typedef long               int_fast16_t;   /**< Fastest at least 16-bit signed */
typedef long               int_fast32_t;   /**< Fastest at least 32-bit signed */
typedef int64_t            int_fast64_t;   /**< Fastest at least 64-bit signed */

/* Unsigned fastest minimum-width integer types */
typedef uint8_t            uint_fast8_t;   /**< Fastest at least 8-bit unsigned */
typedef unsigned long      uint_fast16_t;  /**< Fastest at least 16-bit unsigned */
typedef unsigned long      uint_fast32_t;  /**< Fastest at least 32-bit unsigned */
typedef uint64_t           uint_fast64_t;  /**< Fastest at least 64-bit unsigned */

/* =============================================================================
 * Pointer-Sized Integer Types
 * =============================================================================
 * Types capable of holding pointer values.
 * On x86_64: 64-bit (long/unsigned long)
 * =============================================================================
 */

/* Signed pointer-sized integer */
typedef long               intptr_t;      /**< Signed integer to hold a pointer */

/* Unsigned pointer-sized integer */
typedef unsigned long      uintptr_t;     /**< Unsigned integer to hold a pointer */

/* =============================================================================
 * Greatest-Width Integer Types
 * =============================================================================
 * Types with the maximum width supported on the platform.
 * On x86_64: 64-bit
 * =============================================================================
 */

/* Signed greatest-width integer */
typedef int64_t            intmax_t;      /**< Largest signed integer type */

/* Unsigned greatest-width integer */
typedef uint64_t           uintmax_t;     /**< Largest unsigned integer type */

/* =============================================================================
 * Limits of Exact-Width Integer Types
 * =============================================================================
 * Minimum and maximum values for exact-width types.
 * =============================================================================
 */

/* Limits of 8-bit integer types */
#define INT8_MIN   (-128)
#define INT8_MAX   (127)
#define UINT8_MAX  (255)

/* Limits of 16-bit integer types */
#define INT16_MIN  (-32767 - 1)
#define INT16_MAX  (32767)
#define UINT16_MAX (65535)

/* Limits of 32-bit integer types */
#define INT32_MIN  (-2147483647 - 1)
#define INT32_MAX  (2147483647)
#define UINT32_MAX (4294967295U)

/* Limits of 64-bit integer types */
#define INT64_MIN  (-9223372036854775807L - 1)
#define INT64_MAX  (9223372036854775807L)
#define UINT64_MAX (18446744073709551615UL)

/* =============================================================================
 * Limits of Minimum-Width Integer Types
 * =============================================================================
 * Minimum and maximum values for minimum-width types.
 * Same as exact-width on x86_64.
 * =============================================================================
 */

/* Limits of least8 types */
#define INT_LEAST8_MIN   INT8_MIN
#define INT_LEAST8_MAX   INT8_MAX
#define UINT_LEAST8_MAX  UINT8_MAX

/* Limits of least16 types */
#define INT_LEAST16_MIN  INT16_MIN
#define INT_LEAST16_MAX  INT16_MAX
#define UINT_LEAST16_MAX UINT16_MAX

/* Limits of least32 types */
#define INT_LEAST32_MIN  INT32_MIN
#define INT_LEAST32_MAX  INT32_MAX
#define UINT_LEAST32_MAX UINT32_MAX

/* Limits of least64 types */
#define INT_LEAST64_MIN  INT64_MIN
#define INT_LEAST64_MAX  INT64_MAX
#define UINT_LEAST64_MAX UINT64_MAX

/* =============================================================================
 * Limits of Fastest Minimum-Width Integer Types
 * =============================================================================
 */

/* Limits of fast8 types */
#define INT_FAST8_MIN   INT8_MIN
#define INT_FAST8_MAX   INT8_MAX
#define UINT_FAST8_MAX  UINT8_MAX

/* Limits of fast16 types — typedef'd to long (64-bit on x86_64) */
#define INT_FAST16_MIN  INT64_MIN
#define INT_FAST16_MAX  INT64_MAX
#define UINT_FAST16_MAX UINT64_MAX

/* Limits of fast32 types — typedef'd to long (64-bit on x86_64) */
#define INT_FAST32_MIN  INT64_MIN
#define INT_FAST32_MAX  INT64_MAX
#define UINT_FAST32_MAX UINT64_MAX

/* Limits of fast64 types */
#define INT_FAST64_MIN  INT64_MIN
#define INT_FAST64_MAX  INT64_MAX
#define UINT_FAST64_MAX UINT64_MAX

/* =============================================================================
 * Limits of Pointer-Sized Integer Types
 * =============================================================================
 */

/* Limits of intptr_t/uintptr_t (64-bit on x86_64) */
#define INTPTR_MIN   INT64_MIN
#define INTPTR_MAX   INT64_MAX
#define UINTPTR_MAX  UINT64_MAX

/* =============================================================================
 * Limits of Greatest-Width Integer Types
 * =============================================================================
 */

/* Limits of intmax_t/uintmax_t (64-bit on x86_64) */
#define INTMAX_MIN   INT64_MIN
#define INTMAX_MAX   INT64_MAX
#define UINTMAX_MAX  UINT64_MAX

/* =============================================================================
 * NULL Pointer Constant
 * =============================================================================
 * Defined here for convenience, also in stddef.h
 * =============================================================================
 */
#ifndef NULL
#define NULL ((void*)0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* CORE_STDINT_H */
