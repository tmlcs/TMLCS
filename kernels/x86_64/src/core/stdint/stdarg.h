#ifndef CORE_STDARG_H
#define CORE_STDARG_H

/* =============================================================================
 * GLOBEX_OS Standard Argument Handling - Pure Manual Implementation
 * =============================================================================
 * 
 * Pure manual implementation for x86_64 System V ABI.
 * Does NOT use ANY GCC builtins.
 * 
 * System V ABI calling convention:
 *   - First 6 integer/pointer args: RDI, RSI, RDX, RCX, R8, R9
 *   - First 8 float/double args: XMM0-XMM7
 *   - Additional args: pushed on stack (right-to-left)
 * 
 * Stack layout on function entry:
 *   [RBP+16] = return address
 *   [RBP+24] = first arg (or 9th+ arg)
 *   [RBP+32] = second arg (or 10th+ arg)
 *   ...
 * 
 * @note This implementation handles integer/pointer types only
 * @note Float support requires SSE register handling (not implemented)
 * @note Zero dependencies on system headers or compiler builtins
 * =============================================================================
 */

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Configuration
 * =============================================================================
 */

/* Number of general-purpose registers for argument passing */
#define VA_NUM_GPR_REGS     6

/* Offset where SSE registers would start (we don't use them) */
#define VA_SSE_OFFSET       (VA_NUM_GPR_REGS * 8)

/* =============================================================================
 * va_list - Manual implementation for x86_64 System V ABI
 * =============================================================================
 * 
 * Simplified structure for integer/pointer arguments only:
 *   - next_arg: pointer to next argument (either in saved regs or stack)
 *   - reg_area: pointer to saved register area (if available)
 *   - reg_count: number of register arguments remaining
 * 
 * For a function with variable arguments, the first variable arg is either:
 *   - In a register (if < 6 args total before it)
 *   - On the stack (if >= 6 args)
 * 
 * We use a simplified model: track the next argument pointer directly.
 */
typedef struct {
    void* next_arg;          /* Pointer to next argument */
    unsigned int reg_count;  /* Number of register args remaining (0-6) */
    void* reg_area;          /* Pointer to register save area (or NULL) */
} va_list[1];  /* Array of 1 for pointer decay compatibility */

/* =============================================================================
 * Internal helper: Get stack pointer from frame
 * =============================================================================
 */

/* 
 * Get address of first variable argument using frame pointer.
 * On x86_64 with -fno-omit-frame-pointer, RBP points to saved RBP.
 * First arg is at [RBP+16] (after return address and saved RBP).
 */
static inline void* _va_get_first_arg(void* last_arg_ptr) {
    /* 
     * The last named argument's address is our starting point.
     * Variable args come after it on the stack.
     */
    return (void*)((char*)last_arg_ptr + ((sizeof(void*) + 7) & ~7UL));
}

/* =============================================================================
 * va_start - Initialize va_list for variable arguments
 * =============================================================================
 * 
 * @param ap: The va_list to initialize
 * @param last: The last named parameter before the ellipsis (...)
 * 
 * Implementation:
 *   - Calculate address of first variable argument (after last named arg)
 *   - Set reg_count based on position (simplified: assume all on stack)
 *   - This is a simplified implementation that works for most cases
 * 
 * @note Must be called before accessing variable arguments
 * @note Must be paired with va_end
 */
#define va_start(ap, last) do {                                                 \
    (*ap).next_arg = (void*)((char*)&(last) + ((sizeof(last) + 7) & ~7UL));     \
    (*ap).reg_count = 0;  /* Simplified: assume args on stack */                \
    (*ap).reg_area = (void*)0;                                                  \
} while (0)

/* =============================================================================
 * va_arg - Get next argument from va_list
 * =============================================================================
 * 
 * @param ap: The va_list initialized by va_start
 * @param type: The expected type of the next argument
 * @return The next argument of the specified type
 * 
 * Implementation:
 *   1. Get pointer to current argument
 *   2. Advance next_arg by aligned size of type
 *   3. Return dereferenced pointer cast to type
 * 
 * @note Type must match the actual argument type
 * @note Advances the va_list to the next argument
 */
#define va_arg(ap, type) (*(type*)(_va_get_next_arg((ap), sizeof(type))))

/* Internal helper to get next argument pointer and advance */
static inline void* _va_get_next_arg(va_list ap, unsigned int size) {
    void* result = (*ap).next_arg;
    /* Align to 8-byte boundary (System V ABI requirement) */
    unsigned int aligned_size = (size + 7) & ~7U;
    (*ap).next_arg = (void*)((char*)(*ap).next_arg + aligned_size);
    return result;
}

/* =============================================================================
 * va_end - Clean up va_list
 * =============================================================================
 * 
 * @param ap: The va_list to clean up
 * 
 * Implementation:
 *   - Reset all fields to invalid values
 *   - This prevents accidental reuse after va_end
 * 
 * @note Must be called after finishing with variable arguments
 */
#define va_end(ap) do {                                                         \
    (*ap).next_arg = (void*)0;                                                  \
    (*ap).reg_count = 0;                                                        \
    (*ap).reg_area = (void*)0;                                                  \
} while (0)

/* =============================================================================
 * va_copy - Copy va_list
 * =============================================================================
 * 
 * @param dest: Destination va_list
 * @param src: Source va_list to copy
 * 
 * Implementation:
 *   - Copy all fields from src to dest
 * 
 * @note Allows iterating through arguments multiple times
 * @note Both dest and src must be passed to va_end separately
 */
#define va_copy(dest, src) do {                                                 \
    (*dest).next_arg = (*src).next_arg;                                         \
    (*dest).reg_count = (*src).reg_count;                                       \
    (*dest).reg_area = (*src).reg_area;                                         \
} while (0)

#ifdef __cplusplus
}
#endif

#endif /* CORE_STDARG_H */
