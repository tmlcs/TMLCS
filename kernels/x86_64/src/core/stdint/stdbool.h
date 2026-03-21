#ifndef CORE_STDBOOL_H
#define CORE_STDBOOL_H

/* =============================================================================
 * GLOBEX_OS Standard Boolean Type
 * =============================================================================
 * 
 * Freestanding implementation for C99/C11 compatibility.
 * Compliant with C99/C11 stdbool.h specification.
 * 
 * @note For C++, bool is a built-in type, so we use conditional definition
 * @note No dependencies on system headers
 * =============================================================================
 */

#ifdef __cplusplus
/* In C++, bool is a built-in keyword, no definition needed */
#else
/* In C, define bool, true, false */

/* =============================================================================
 * bool - Boolean type
 * =============================================================================
 * Uses _Bool which is the built-in boolean type in C99+.
 * Stores 0 for false, 1 for true.
 * =============================================================================
 */
#define bool _Bool

/* =============================================================================
 * true - Boolean true value
 * =============================================================================
 * Expands to integer constant 1
 * =============================================================================
 */
#define true 1

/* =============================================================================
 * false - Boolean false value
 * =============================================================================
 * Expands to integer constant 0
 * =============================================================================
 */
#define false 0

#endif /* __cplusplus */

/* =============================================================================
 * __bool_true_false_are_defined - Feature test macro
 * =============================================================================
 * Indicates that true/false macros are defined.
 * Required by C99/C11 stdbool.h specification.
 * =============================================================================
 */
#define __bool_true_false_are_defined 1

#endif /* CORE_STDBOOL_H */
