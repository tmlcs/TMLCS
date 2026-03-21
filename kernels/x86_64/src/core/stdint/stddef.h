#ifndef CORE_STDDEF_H
#define CORE_STDDEF_H

/* =============================================================================
 * GLOBEX_OS Standard Definitions
 * =============================================================================
 * 
 * Freestanding implementation for x86_64 architecture.
 * Compliant with C99/C11 stddef.h specification.
 * 
 * @note No dependencies on system headers
 * =============================================================================
 */

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * NULL Pointer Constant
 * =============================================================================
 */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* =============================================================================
 * size_t - Unsigned integral type for sizes
 * =============================================================================
 * Result of sizeof operator. On x86_64: 64-bit unsigned long.
 * Used for array indices, object sizes, memory sizes.
 * =============================================================================
 */
typedef unsigned long size_t;

/* =============================================================================
 * ptrdiff_t - Signed integral type for pointer differences
 * =============================================================================
 * Result of subtracting two pointers. On x86_64: 64-bit signed long.
 * Used for pointer arithmetic differences.
 * =============================================================================
 */
typedef long ptrdiff_t;

/* =============================================================================
 * wchar_t - Wide character type
 * =============================================================================
 * Used for wide character strings. On x86_64: 32-bit int.
 * Note: Not currently used in kernel, provided for completeness.
 * Note: In C++, wchar_t is a built-in type, so we don't redefine it.
 * =============================================================================
 */
#ifndef __cplusplus
typedef int wchar_t;
#endif

/* =============================================================================
 * max_align_t - Type with maximum alignment requirement
 * =============================================================================
 * Used for aligned memory allocation. On x86_64: long double.
 * Note: In C++, max_align_t may be a built-in type, so we guard the definition.
 * =============================================================================
 */
#ifndef __cplusplus
typedef long double max_align_t;
#endif

/* =============================================================================
 * offsetof - Macro to get offset of member within struct
 * =============================================================================
 * Returns the byte offset of a member from the beginning of its structure.
 * Implementation uses compiler builtin for correctness.
 * =============================================================================
 */
#define offsetof(type, member) __builtin_offsetof(type, member)

#ifdef __cplusplus
}
#endif

#endif /* CORE_STDDEF_H */
