# GLOBEX_OS - Documentation Style Guide

## Language Policy

**All documentation and comments must be written in English.**

This ensures:
- Consistency across the codebase
- Accessibility for international contributors
- Compatibility with automated tools (clang-tidy, Doxygen, etc.)

## Documentation Standards

### 1. File Headers

Each source file should have a brief header comment:

```cpp
/**
 * @file filename.cpp
 * @brief Brief description of file purpose
 *
 * Longer description if needed.
 */
```

### 2. Function Documentation

Use Doxygen-style comments for all public API functions:

```cpp
/**
 * @brief Clear, concise function description
 * @param param_name Parameter description
 * @return Return value description
 *
 * Optional extended description with notes:
 * @note Important notes here
 * @warning Warnings if applicable
 * @example Usage examples
 */
ReturnType functionName(Type param_name);
```

### 3. Inline Comments

Use `//` for inline comments. Keep them concise and in English:

```cpp
// ✅ CORRECT
// Disable interrupts before acquiring lock
cli();

// ❌ WRONG
// Deshabilitar interrupciones antes de adquirir lock
cli();
```

### 4. Block Comments

Use `/* */` for multi-line block comments:

```cpp
/* ==========================================
 * Section Header
 * ==========================================
 * Detailed explanation of this section's
 * purpose and functionality.
 * ==========================================
 */
```

### 5. Security-Sensitive Comments

Mark security-critical code with explicit warnings:

```cpp
/* =============================================================================
 * SECURITY NOTE
 * =============================================================================
 * This function validates pointers to prevent null pointer dereference.
 * Always validate pointers before dereferencing in kernel space.
 * =============================================================================
 */
```

### 6. SMP/Concurrency Notes

Document thread-safety and interrupt considerations:

```cpp
/**
 * @brief Function description
 *
 * SMP Safety:
 *   - Safe/Unsafe for multi-processor systems
 *   - Acquires/releases specific locks
 *
 * Interrupt Safety:
 *   - Safe/Unsafe to call from interrupt handlers
 *   - Disables/restores interrupts
 */
```

## Common Phrases (English Only)

| Context | Correct (English) | Incorrect (Spanish) |
|---------|------------------|---------------------|
| Notes | "Note:", "Important:" | "Nota:", "Importante:" |
| Warning | "Warning:", "Caution:" | "Advertencia:", "Precaución:" |
| Example | "Example:", "e.g.," | "Ejemplo:" |
| Parameters | "Parameters:", "Args:" | "Parámetros:" |
| Returns | "Returns:", "Return:" | "Devuelve:", "Retorna:" |
| See also | "See:", "See also:" | "Ver:", "Véase también:" |
| Use | "Use", "Using" | "Usar", "Utilizar" |
| Ensure | "Ensure", "Make sure" | "Asegurar", "Garantizar" |
| Check | "Check", "Verify" | "Verificar", "Comprobar" |

## Code Comment Examples

### Good Examples

```cpp
// Initialize VGA buffer address
volatile uint16_t* vga = reinterpret_cast<volatile uint16_t*>(VGA_BUFFER_ADDRESS);

// Check if serial port is ready
if (!serial_is_initialized()) {
    return;  // Serial not available, skip output
}

// Atomic increment - safe for SMP
atomic_inc32(&counter);
```

### Bad Examples (Mixed Language)

```cpp
// Inicializar buffer VGA - ❌ Spanish
// Verificar si el puerto serial está listo - ❌ Spanish
// Incremento atómico - seguro para SMP - ❌ Mixed
```

## Documentation Files

All `.md` documentation files must be in English:

- `README.md` - Project overview (English)
- `docs/CICD_GUIDE.md` - CI/CD documentation (English)
- `docs/ARCHITECTURE.md` - Architecture documentation (English)
- `CODE_AUDIT_REPORT.md` - Audit report (English)

## Review Checklist

Before committing code, verify:

- [ ] All comments are in English
- [ ] Doxygen tags are properly formatted
- [ ] Security notes are clearly marked
- [ ] SMP/interrupt safety is documented
- [ ] No mixed-language comments

## Enforcement

The CI/CD pipeline may include checks for:
- Non-ASCII characters in comments (excluding string literals)
- Common Spanish technical terms in comments

To check locally:
```bash
# Search for Spanish comments
grep -rn "^[[:space:]]*//.*[áéíóúñ]" src/

# Search for Spanish technical terms
grep -rn "barrera\|memoria\|hilos\|asegurar" src/
```

## References

- [Doxygen Manual](https://www.doxygen.nl/manual/)
- [Linux Kernel Coding Style](https://www.kernel.org/doc/html/latest/process/coding-style.html)
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)

---

**Last updated:** March 15, 2026  
**Version:** 1.0
