#ifndef CORE_STDARG_H
#define CORE_STDARG_H

/* =============================================================================
 * GLOBEX_OS Standard Argument Handling
 * =============================================================================
 *
 * Uses GCC built-in varargs, which correctly handle the System V AMD64 ABI
 * register-passing convention (first 6 integer/pointer args in RDI, RSI,
 * RDX, RCX, R8, R9). The compiler generates the required register save area
 * and advances the va_list through registers before falling back to the stack.
 *
 * The previous manual implementation assumed all arguments were on the stack,
 * which is incorrect when the variadic function has fewer than 6 named
 * parameters — the first variadic arg may arrive in a register, not on the
 * stack, causing va_arg() to read garbage (CRIT-001 fix).
 *
 * GCC __builtin_va_* work in -ffreestanding mode with no runtime library.
 * =============================================================================
 */

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)
#define va_copy(dest, src) __builtin_va_copy(dest, src)

#endif /* CORE_STDARG_H */
