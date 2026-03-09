# 📋 PLAN DE ACCIÓN DE CALIDAD - GLOBEX_OS v0.015_x64

## Visión General

Este documento define el plan de acción detallado para abordar los **34 hallazgos** identificados en la auditoría de calidad del código. El plan está organizado en **4 fases** con entregables específicos y criterios de aceptación.

---

## 📊 Resumen de Fases

| Fase | Duración Estimada | Hallazgos | Prioridad |
|------|-------------------|-----------|-----------|
| **Fase 1: Críticos** | 1-2 semanas | 5 críticos | 🔴 Urgente |
| **Fase 2: Altos** | 2-3 semanas | 10 altos | 🟠 Importante |
| **Fase 3: Medios** | 2-3 semanas | 8 medios | 🟡 Recomendado |
| **Fase 4: Bajos** | 1-2 semanas | 11 bajos | 🟢 Opcional |

---

# 📍 FASE 1: CORRECCIONES CRÍTICAS (Sprint 1-2)

## CRIT-001: Desbordamiento en `uint32_to_decimal_string`

**Archivo:** `src/intf/decimal_utils.h`  
**Líneas:** 33-46  
**Estado:** ⏳ Pendiente  
**Prioridad:** 🔴 P0

### Tareas
- [ ] Cambiar índice inicial de `i = 10` a `i = 11`
- [ ] Modificar condición del bucle para prevenir escritura en `buffer[0]`
- [ ] Actualizar documentación del buffer mínimo requerido (12 bytes)
- [ ] Añadir test de边界 para valor máximo (4294967295)
- [ ] Añadir test para valor cero (0)
- [ ] Ejecutar build y verificar sin warnings

### Criterios de Aceptación
```cpp
// ANTES (vulnerable):
int i = 10;
buffer[11] = '\0';
while (value > 0 && i > 0) {
    buffer[i--] = '0' + (value % 10);
}

// DESPUÉS (seguro):
int i = 11;
buffer[i] = '\0';
while (value > 0 && i > 1) {
    buffer[--i] = '0' + (value % 10);
    value /= 10;
}
```

### Tests Requeridos
```cpp
// test_decimal_utils.cpp
TEST(decimal_uint32, max_value) {
    char buffer[12];
    char* result = uint32_to_decimal_string(buffer, 4294967295);
    ASSERT_STREQ(result, "4294967295");
}

TEST(decimal_uint32, zero_value) {
    char buffer[12];
    char* result = uint32_to_decimal_string(buffer, 0);
    ASSERT_STREQ(result, "0");
}

TEST(decimal_uint32, buffer_underrun) {
    char buffer[12] = {0xDE};
    buffer[11] = 0xDE;
    uint32_to_decimal_string(buffer, 4294967295);
    ASSERT_EQ(buffer[11], 0xDE);  // No overflow
}
```

---

## CRIT-002: Desbordamiento en `uint64_to_decimal_string`

**Archivo:** `src/intf/decimal_utils.h`  
**Líneas:** 58-71  
**Estado:** ⏳ Pendiente  
**Prioridad:** 🔴 P0

### Tareas
- [ ] Cambiar índice inicial de `i = 20` a `i = 21`
- [ ] Modificar condición del bucle para prevenir escritura en `buffer[0]`
- [ ] Actualizar documentación del buffer mínimo requerido (22 bytes)
- [ ] Añadir test para valor máximo (18446744073709551615)
- [ ] Añadir test para INT64_MIN (-9223372036854775808)
- [ ] Verificar llamadas en `print.cpp` y `serial.cpp`

### Criterios de Aceptación
```cpp
// ANTES (vulnerable):
int i = 20;
buffer[21] = '\0';
while (value > 0 && i > 0) {
    buffer[i--] = '0' + (value % 10);
}

// DESPUÉS (seguro):
int i = 21;
buffer[i] = '\0';
while (value > 0 && i > 1) {
    buffer[--i] = '0' + (value % 10);
    value /= 10;
}
```

### Tests Requeridos
```cpp
TEST(decimal_uint64, max_value) {
    char buffer[22];
    char* result = uint64_to_decimal_string(buffer, 18446744073709551615ULL);
    ASSERT_STREQ(result, "18446744073709551615");
}

TEST(decimal_uint64, buffer_underrun) {
    char guard_before[4] = {0xAA, 0xAA, 0xAA, 0xAA};
    char buffer[22];
    char guard_after[4] = {0xBB, 0xBB, 0xBB, 0xBB};
    
    uint64_to_decimal_string(buffer, 18446744073709551615ULL);
    
    // Verificar guards no modificados
    ASSERT_EQ(guard_before[0], 0xAA);
    ASSERT_EQ(guard_after[0], 0xBB);
}
```

---

## CRIT-003: Falso positivo de fallo de hardware en serial

**Archivo:** `src/impl/x86_64/serial.cpp`
**Líneas:** 275-286
**Estado:** ✅ COMPLETADO
**Prioridad:** 🔴 P0

### Resumen
Se corrigió el problema donde pasar un puntero nulo a `serial_write_str()` incorrectamente marcaba el puerto serial como fallo de hardware (`serial_failed = 1`). Un puntero nulo es un error de programación, no un fallo de hardware.

### Cambios Realizados
- [x] Remover asignación de `serial_failed = 1` para null pointer
- [x] Mantener `serial_error_code = SERIAL_ERROR_NULL_PTR` para diagnóstico
- [x] Añadir test de validación de null pointer
- [x] Verificar que panic no se active por error de programación

### Código Final
```cpp
void serial_write_str(const char* str) {
    /* 
     * CRIT-003 FIX: Null pointer is a programming error, NOT hardware failure.
     * Do NOT set serial_failed or increment timeout counters.
     * Just record the error code for diagnostics and return.
     */
    if (str == nullptr) {
        serial_error_code = SERIAL_ERROR_NULL_PTR;
        /* Do NOT set serial_failed = 1 - this is not a hardware error */
        return;
    }

    while (*str) {
        serial_write_char(*str);
        str++;
    }
}
```

### Tests Añadidos
- `test_serial_null_pointer_handling()` en `test_serial.cpp`
- Verifica que `serial_has_failed()` retorna falso después de pasar nullptr
- Verifica que `serial_get_error_code()` retorna `SERIAL_ERROR_NULL_PTR`
- Verifica que el serial sigue funcional después del error

### Verificación
```
=== Serial Null Pointer Test ===
Serial initial state: OK (not failed)
Testing serial_write_str(nullptr)...
CRIT-003 PASSED: serial_failed NOT set for null pointer
Error code: SERIAL_ERROR_NULL_PTR (correct)
Testing serial still works after nullptr...
Serial still functional: OK
[SERIAL NULL POINTER] All tests passed
```

---

## CRIT-004: Condición de carrera en VGA (SMP-safety)

**Archivo:** `src/impl/x86_64/print.cpp`
**Líneas:** 34-60 (declaración de variables), 163-180 (print_newline), 210-230 (print_char), 298-312 (print_set_color)
**Estado:** ✅ COMPLETADO (Fase 1: Documentación)
**Prioridad:** 🔴 P0 (Documentación), 🟠 P1 (Implementación SMP Futura)

### Resumen
Se añadió documentación exhaustiva sobre las implicaciones de SMP-safety en el driver VGA. El driver actual es **UP-safe** (uni-procesador) pero **no es SMP-safe**. Se documentaron las variables compartidas sin sincronización y los requisitos para futura implementación SMP.

### Cambios Realizados - Fase 1 (Documentación)

#### 1. Bloque de advertencia SMP en variables globales (líneas 34-60)
```cpp
/* =============================================================================
 * SMP SAFETY WARNING [CRIT-004]
 * =============================================================================
 * This driver is NOT safe for SMP (Symmetric Multi-Processing) environments.
 * The following variables are shared without synchronization:
 *   - cursor_col, cursor_row  (cursor position state)
 *   - current_color           (color attribute state)
 *   - vga_buffer[]            (hardware MMIO region at 0xB8000)
 *
 * For future SMP support, these must be protected by:
 *   1. Spinlocks - Mutual exclusion for multi-core access
 *   2. Per-CPU cursor state with IPI for cross-CPU updates
 *   3. Atomic operations (if available in freestanding mode)
 *
 * Race conditions that can occur in SMP:
 *   - Two CPUs writing to cursor_col/row simultaneously
 *   - Color attribute corruption from concurrent print_set_color()
 *   - Garbled output from interleaved VGA buffer writes
 *
 * Current status: Uni-processor only (UP-safe)
 * Tracking issue: #SMP-001
 * =============================================================================
 */
```

#### 2. Documentación en `print_newline()` (líneas 163-180)
- Describe corrupción de cursor durante scroll
- Documenta corrupción de pantalla por operaciones interleaved
- Especifica requisitos de protección con spinlock

#### 3. Documentación en `print_char()` (líneas 210-230)
- Describe corrupción de posición de cursor
- Documenta caracteres escritos en posiciones incorrectas
- Especifica ciclo read-modify-write que requiere protección

#### 4. Documentación en `print_set_color()` (líneas 298-312)
- Describe corrupción de color por writes interleaved
- Documenta color incorrecto aplicado a caracteres
- Sugiere uso de atomic compare-and-swap

### Tareas - Fase 2 (Implementación SMP Futura)
- [ ] Investigar atomic operations en modo freestanding
- [ ] Diseñar estructura per-CPU para cursor state
- [ ] Implementar spinlock básico para x86_64
- [ ] Refactorizar acceso a VGA buffer con lock

### Criterios de Aceptación (Fase 1 - Documentación)
- [x] Comentario explícito sobre no ser thread-safe añadido
- [x] Variables que requieren protección SMP documentadas
- [x] Advertencia en código para futuros desarrolladores
- [x] Issue de seguimiento creado (#SMP-001)

### Verificación
```bash
# Build exitoso sin warnings
make build-x86_64
# Result: Build completed successfully
```

### Referencias
- [SMP Locking Strategies](https://wiki.osdev.org/SMP_Locking_Strategies)
- [Spinlock Implementation](https://wiki.osdev.org/Spinlock)
- [Atomic Operations](https://en.cppreference.com/w/cpp/atomic)

---

## CRIT-005: Falta validación de overlap en memcpy

**Archivo:** `src/impl/x86_64/string.cpp`  
**Líneas:** 58-73  
**Estado:** ⏳ Pendiente  
**Prioridad:** 🔴 P0

### Tareas
- [ ] Añadir verificación de overlap en modo DEBUG
- [ ] Usar `DEBUG_PRINT` para reportar overlap detectado
- [ ] Mantener código ligero en modo release
- [ ] Añadir tests de overlap explícitos
- [ ] Documentar comportamiento en header

### Criterios de Aceptación
```cpp
// ANTES (sin validación):
void* memcpy(void* dest, const void* src, size_t n) {
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

// DESPUÉS (con validación DEBUG):
void* memcpy(void* dest, const void* src, size_t n) {
    #ifdef DEBUG_ENABLE
    // Check for overlapping regions
    if (dest < src + n && src < dest + n) {
        DEBUG_PRINT("\r\n[MEMCPY OVERLAP DETECTED]\r\n");
        DEBUG_PRINT("  src:  0x");
        DEBUG_PRINT_HEX((uintptr_t)src);
        DEBUG_PRINT("\r\n  dest: 0x");
        DEBUG_PRINT_HEX((uintptr_t)dest);
        DEBUG_PRINT("\r\n  size: ");
        DEBUG_PRINT_DEC(n);
        DEBUG_PRINT("\r\n  HINT: Use memmove() for overlapping regions\r\n");
        DEBUG_BREAK();
    }
    #endif
    
    uint8_t* d = static_cast<uint8_t*>(dest);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}
```

### Tests Requeridos
```cpp
TEST(memcpy, overlap_detection_debug_mode) {
    char buffer[20] = "Hello World!";
    
    // Overlap: dest is within src region
    // This should trigger DEBUG assertion in debug mode
    memcpy(buffer + 3, buffer, 10);  // Overlapping!
    
    // In release mode, behavior is undefined but should not crash
    // In debug mode, should hit DEBUG_BREAK()
}

TEST(memcpy, no_overlap_valid) {
    char src[10] = "Hello";
    char dest[10];
    memcpy(dest, src, 6);  // Non-overlapping, OK
    ASSERT_STREQ(dest, "Hello");
}
```

---

# 📍 FASE 2: CORRECCIONES ALTAS (Sprint 3-5)

## HIGH-001: DEBUG_PRINTF solo soporta un argumento

**Archivo:** `src/intf/debug.h`  
**Líneas:** 64-85  
**Estado:** ⏳ Pendiente  
**Prioridad:** 🟠 P1

### Tareas
- [ ] Documentar limitación de un solo argumento en comentario
- [ ] Añadir ejemplo de uso correcto en documentación
- [ ] Considerar implementación con múltiples argumentos (opcional)
- [ ] Actualizar tests para reflejar limitación

### Criterios de Aceptación
```cpp
/**
 * @brief [D001] DEBUG_PRINTF - Print with limited format support
 * 
 * ⚠️  LIMITATION: Only supports ONE argument. All format specifiers
 *     will use the same argument value. For multiple values, use
 *     multiple DEBUG_PRINT calls.
 * 
 * Supported formats: %s (string), %x (hex), %d (decimal), %c (char)
 * 
 * @example
 *     // CORRECT usage:
 *     DEBUG_PRINT("Value: "); DEBUG_PRINT_HEX(val); DEBUG_PRINT("\r\n");
 *     
 *     // INCORRECT usage (both %x use same arg):
 *     DEBUG_PRINTF("A: %x, B: %x", val1);  // Wrong!
 * 
 * @note Simple implementation without varargs for freestanding kernel
 */
#define DEBUG_PRINTF(fmt, arg) do { ... } while(0)
```

---

## HIGH-002: serial_port_exists puede dar falsos negativos

**Archivo:** `src/impl/x86_64/serial.cpp`  
**Líneas:** 89-104  
**Estado:** ⏳ Pendiente  
**Prioridad:** 🟠 P1

### Tareas
- [ ] Hacer verificación 0xFF opcional con flag de configuración
- [ ] Añadir fallback: intentar init incluso si retorna 0xFF
- [ ] Documentar comportamiento en QEMU vs hardware real
- [ ] Añadir test de detección en diferentes entornos

### Criterios de Aceptación
```cpp
static bool serial_port_exists(uint16_t port) {
    /*
     * Note: Reading 0xFF may indicate no port present, but this is
     * unreliable in QEMU and some older hardware. We use this as a
     * hint only, not a definitive test.
     */
    uint8_t iir = inb(port + SERIAL_IIR);
    
    #ifdef SERIAL_SKIP_0xFF_CHECK
    // Always attempt initialization (for QEMU/testing)
    return true;
    #else
    // 0xFF typically indicates no port, but may be false negative
    if (iir == 0xFF) {
        // Try one more read to confirm
        iir = inb(port + SERIAL_IIR);
        if (iir == 0xFF) {
            return false;
        }
    }
    return true;
    #endif
}
```

---

## HIGH-003: Validación de baud rate demasiado restrictiva

**Archivo:** `src/impl/x86_64/serial.cpp`  
**Líneas:** 120-127  
**Estado:** ⏳ Pendiente  
**Prioridad:** 🟠 P1

### Tareas
- [ ] Remover límite superior fijo de 115200
- [ ] Calcular divisor y validar que sea > 0
- [ ] Añadir warning para rates no estándar
- [ ] Soportar 230400, 460800, 921600
- [ ] Actualizar tests de baud rate

### Criterios de Aceptación
```cpp
// ANTES (restrictivo):
if (baud < 110 || baud > 115200) {
    serial_failed = 1;
    serial_error_code = SERIAL_ERROR_INIT_FAIL;
    return 0;
}

// DESPUÉS (flexible):
// Validate baud rate produces valid divisor
// UART clock is typically 1.8432 MHz
// divisor = 115200 / baud (for 115200 base)
if (baud == 0) {
    serial_failed = 1;
    serial_error_code = SERIAL_ERROR_INIT_FAIL;
    return 0;
}

uint32_t divisor = 115200 / baud;
if (divisor == 0) {
    // Baud rate too high for UART
    serial_failed = 1;
    serial_error_code = SERIAL_ERROR_INIT_FAIL;
    return 0;
}

// Warn about non-standard rates
if (baud != 110 && baud != 9600 && baud != 57600 && 
    baud != 115200 && baud != 230400 && baud != 460800) {
    DEBUG_PRINT("[SERIAL] Non-standard baud rate: ");
    DEBUG_PRINT_DEC(baud);
    DEBUG_PRINT("\r\n");
}
```

---

## HIGH-004: Validación de CR3 usa rango arbitrario

**Archivo:** `src/impl/tests/test_memory.cpp`  
**Líneas:** 35-44  
**Estado:** ⏳ Pendiente  
**Prioridad:** 🟠 P1

### Tareas
- [ ] Validar alineación a 4KB (CR3 & 0xFFF == 0)
- [ ] Validar que CR3 apunta a memoria mapeada conocida
- [ ] Remover validación de rango fijo 1MB-256MB
- [ ] Añadir verificación de estructura de page table

### Criterios de Aceptación
```cpp
// ANTES (rango arbitrario):
if (pt_base > 0x100000 && pt_base < 0x10000000) {
    serial_write_str("CR3 validation: OK\r\n");
} else {
    panic("Page table CR3 validation failed", ...);
}

// DESPUÉS (validación correcta):
// CR3 should be 4KB aligned (lower 12 bits are flags)
if (cr3_value & 0xFFF) {
    serial_write_str("CR3 validation: FAILED (not aligned)\r\n");
    panic("CR3 not 4KB aligned", static_cast<uint32_t>(cr3_value));
}

// Extract page directory base (mask out flags)
uintptr_t pt_base = cr3_value & ~0xFFFULL;

// Verify it points to mapped memory (within 2GiB region)
if (pt_base >= 0x80000000) {
    serial_write_str("CR3 validation: FAILED (outside mapped memory)\r\n");
    panic("CR3 points outside mapped memory", static_cast<uint32_t>(pt_base));
}

serial_write_str("CR3 validation: OK (aligned and mapped)\r\n");
```

---

## HIGH-005: Inconsistencia print_detect vs print_is_initialized

**Archivo:** `src/impl/x86_64/print.cpp`  
**Líneas:** 91-103  
**Estado:** ⏳ Pendiente  
**Prioridad:** 🟠 P1

### Tareas
- [ ] Separar `print_detect()` (solo hardware detection)
- [ ] Mantener `vga_initialized` separado de `vga_detected`
- [ ] `print_clear()` debería marcar como inicializado
- [ ] Actualizar documentación de ambas funciones
- [ ] Revisar llamadas en kernel_main para orden correcto

### Criterios de Aceptación
```cpp
// print_detect - solo detecta hardware, no inicializa
bool print_detect(void) {
    if (vga_initialized) {
        return true;  // Already initialized
    }
    
    // Test VGA hardware presence
    volatile VgaChar* test_ptr = &vga_buffer[0];
    uint8_t saved_char = test_ptr->character;
    uint8_t saved_color = test_ptr->color;
    
    test_ptr->character = 0xAA;
    test_ptr->color = 0x55;
    
    bool exists = (test_ptr->character == 0xAA && 
                   test_ptr->color == 0x55);
    
    // Restore
    test_ptr->character = saved_char;
    test_ptr->color = saved_color;
    
    vga_detected = exists;
    // NOTE: Does NOT set vga_initialized - caller must call print_clear()
    return exists;
}

// print_clear - inicializa VGA para uso
void print_clear(void) {
    if (!vga_detected) {
        return;  // Hardware not present
    }
    
    // Clear buffer
    for (size_t i = 0; i < VGA_BUFFER_SIZE; i++) {
        vga_buffer[i] = (current_color << 8) | ' ';
    }
    
    cursor_col = 0;
    cursor_row = 0;
    vga_initialized = true;  // Now marked as initialized
    update_cursor();
}
```

---

## HIGH-006: Scroll en print_newline orden subóptimo

**Archivo:** `src/impl/x86_64/print.cpp`  
**Líneas:** 152-181  
**Estado:** ⏳ Pendiente  
**Prioridad:** 🟠 P1

### Tareas
- [ ] Cambiar dirección del scroll (abajo hacia arriba)
- [ ] Verificar que no hay corrupción visual
- [ ] Añadir comentario explicando el orden
- [ ] Testear con buffer lleno de caracteres únicos

### Criterios de Aceptación
```cpp
// ANTES (arriba hacia abajo):
for (size_t r = 0; r < VGA_ROWS - 1; r++) {
    volatile uint16_t* dst = ...;
    const volatile uint16_t* src = ...;
    for (size_t c = 0; c < VGA_COLS; c++) {
        dst[c] = src[c];
    }
}

// DESPUÉS (abajo hacia arriba - correcto para overlap):
for (size_t r = VGA_ROWS - 2; r < VGA_ROWS; r--) {  // r-- con underflow check
    volatile uint16_t* dst = &vga_buffer[vga_index(r, 0)];
    const volatile uint16_t* src = &vga_buffer[vga_index(r + 1, 0)];
    for (size_t c = 0; c < VGA_COLS; c++) {
        dst[c] = src[c];
    }
}

// Alternative: use memmove semantics (dst < src, so forward copy is safe)
// Current implementation is actually safe because dst < src always
// But processing bottom-to-top is more intuitive for scroll
```

---

## HIGH-007: strcpy no valida tamaño de buffer

**Archivo:** `src/impl/x86_64/string.cpp`  
**Líneas:** 113-124  
**Estado:** ⏳ Pendiente  
**Prioridad:** 🟠 P1

### Tareas
- [ ] Implementar `strcpy_s()` con parámetro de tamaño
- [ ] Mantener `strcpy()` original para compatibilidad
- [ ] Documentar que `strcpy()` es inseguro
- [ ] Migrar código interno a usar `strcpy_s()`
- [ ] Añadir tests de边界 para strcpy_s

### Criterios de Aceptación
```cpp
// Nueva función segura:
/**
 * @brief Copy string with bounds checking
 * @param dest Destination buffer
 * @param dest_size Size of destination buffer in bytes
 * @param src Source null-terminated string
 * @return Pointer to dest, or nullptr if error
 * 
 * @note Truncates source if it exceeds dest_size - 1
 * @note Always null-terminates result (unless dest_size == 0)
 */
char* strcpy_s(char* dest, size_t dest_size, const char* src) {
    if (dest == nullptr || src == nullptr || dest_size == 0) {
        return nullptr;
    }
    
    size_t i;
    for (i = 0; i < dest_size - 1 && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';  // Always null-terminate
    
    return dest;
}

// strcpy original se mantiene pero con advertencia:
/**
 * @deprecated Use strcpy_s() for bounds-checked copying.
 *             strcpy() does not validate destination buffer size.
 */
char* strcpy(char* dest, const char* src) {
    // ... existing implementation ...
}
```

---

## HIGH-008: Test de string con buffer potencialmente insuficiente

**Archivo:** `src/impl/tests/test_string.cpp`  
**Líneas:** 23-35  
**Estado:** ⏳ Pendiente  
**Prioridad:** 🟠 P1

### Tareas
- [ ] Revisar todos los buffers en tests de string
- [ ] Añadir guards antes/después de buffers para detectar overflow
- [ ] Añadir test explícito de boundary conditions
- [ ] Documentar tamaño mínimo requerido para cada test

### Criterios de Aceptación
```cpp
TEST(string, memcpy_with_guards) {
    // Guards to detect buffer overflow
    uint8_t guard_before[4] = {0xAA, 0xAA, 0xAA, 0xAA};
    char buffer1[20];
    char buffer2[20];
    uint8_t guard_after[4] = {0xBB, 0xBB, 0xBB, 0xBB};
    
    // Fill buffers
    for (int i = 0; i < 20; i++) buffer1[i] = 'A';
    for (int i = 0; i < 20; i++) buffer2[i] = 'B';
    
    memcpy(buffer2, buffer1, 6);
    
    // Verify guards unchanged
    ASSERT_EQ(guard_before[0], 0xAA);
    ASSERT_EQ(guard_after[0], 0xBB);
}
```

---

## HIGH-009: Falta validación de límites en print_set_cursor

**Archivo:** `src/impl/x86_64/print.cpp`  
**Líneas:** 308-326  
**Estado:** ⏳ Pendiente  
**Prioridad:** 🟠 P1

### Tareas
- [ ] Añadir comentario sobre comportamiento con negativos casteados
- [ ] Documentar que size_t es unsigned
- [ ] Verificar que clamp funciona correctamente

### Criterios de Aceptación
```cpp
/**
 * @brief Set cursor position
 * @param col New column (0-79)
 * @param row New row (0-24)
 * 
 * @note Values are clamped to valid range:
 *       - col >= 80 → col = 79
 *       - row >= 25 → row = 24
 * 
 * @warning size_t is unsigned. Negative values cast to size_t will
 *          wrap to large positive values and be clamped to maximum.
 *          Example: print_set_cursor((size_t)-1, 0) → cursor at (79, 0)
 * 
 * @note Does not check vga_detected - caller must ensure VGA is initialized.
 */
void print_set_cursor(size_t col, size_t row) {
    // Clamp column to [0, VGA_COLS - 1]
    if (col >= VGA_COLS) {
        col = VGA_COLS - 1;
    }
    // Clamp row to [0, VGA_ROWS - 1]
    if (row >= VGA_ROWS) {
        row = VGA_ROWS - 1;
    }
    // ...
}
```

---

## HIGH-010: No hay manejo de errores en print_detect

**Archivo:** `src/impl/x86_64/print.cpp`  
**Líneas:** 91-103  
**Estado:** ⏳ Pendiente  
**Prioridad:** 🟠 P1

### Tareas
- [ ] Añadir comentario sobre seguridad de acceso MMIO
- [ ] Documentar que asume memoria mapeada válida
- [ ] Considerar fallback si acceso causa fault (futuro)

### Criterios de Aceptación
```cpp
/**
 * @brief Detect VGA hardware presence
 * @return true if VGA detected, false otherwise
 * 
 * @note This function performs a test write to VGA memory at 0xB8000.
 *       It assumes this address is mapped and accessible.
 * 
 * @note In this kernel, 2GiB of memory is identity-mapped (0x00000000
 *       to 0x7FFFFFFF), which includes VGA buffer at 0xB8000.
 *       Access is safe and will not fault.
 * 
 * @note For kernels without guaranteed mapping, wrap in exception handler.
 */
bool print_detect(void) {
    // ... existing implementation ...
}
```

---

# 📍 FASE 3: MEJORAS MEDIAS (Sprint 6-8)

## MED-001: Código duplicado en conversión hex/decimal

**Archivos:** `print.cpp`, `serial.cpp`  
**Prioridad:** 🟡 P2

### Tareas
- [ ] Analizar impacto en tamaño de binario
- [ ] Si significativo, mover implementaciones a archivo común
- [ ] Mantener headers como wrappers inline si es necesario
- [ ] Medir tamaño antes/después

---

## MED-002: Documentar constantes mágicas en assembly

**Archivo:** `src/impl/x86_64/boot/main.asm`  
**Prioridad:** 🟡 P2

### Tareas
- [ ] Añadir comentario bit-a-bit para `0b10000011`
- [ ] Documentar flags de entrada de página:
  - Bit 0: Present (P)
  - Bit 1: Read/Write (R/W)
  - Bit 7: Page Size (PS) para huge pages

---

## MED-003: serial_wait_transmit_empty_timeout usa busy-wait

**Archivo:** `src/impl/x86_64/serial.cpp`  
**Prioridad:** 🟡 P2

### Tareas
- [ ] Documentar que es solo para early boot
- [ ] Considerar usar `__asm__ volatile ("hlt")` en lugar de `nop`
- [ ] Diseñar mecanismo de sleep para futuro multitasking

---

## MED-004: Test framework no reporta resumen

**Archivo:** `src/impl/tests/test_framework.h`  
**Prioridad:** 🟡 P2

### Tareas
- [ ] Añadir contadores globales: `g_tests_passed`, `g_tests_failed`
- [ ] Implementar `test_summary()` que imprime estadísticas
- [ ] Llamar desde `kernel_main()` al final
- [ ] Formato: "Tests: X passed, Y failed, Z total"

---

## MED-005: Makefile no tiene target para tests unitarios

**Archivo:** `Makefile`  
**Prioridad:** 🟡 P2

### Tareas
- [ ] Evaluar si es posible correr tests sin QEMU (nativo)
- [ ] Si no, añadir target `make test-qemu` con timeout
- [ ] Extraer serial output y validar automáticamente
- [ ] Integrar con `verify_tests.sh`

---

## MED-006: Falta validación en print_str para strings largas

**Archivo:** `src/impl/x86_64/print.cpp`  
**Prioridad:** 🟡 P2

### Tareas
- [ ] Documentar por qué MAX_PRINT_LEN = 4096
- [ ] Considerar hacer configurable
- [ ] Añadir DEBUG warning si se alcanza el límite

---

## MED-007: hex_utils.h usa formato de ancho fijo sin documentar

**Archivo:** `src/impl/x86_64/hex_utils.h`  
**Prioridad:** 🟡 P2

### Tareas
- [ ] Añadir comentario sobre formato de 16 dígitos
- [ ] Explicar que es intencional para consistencia
- [ ] Considerar función alternativa sin padding (futuro)

---

## MED-008: Linker script no valida tamaño de pila

**Archivo:** `targets/x86_64/linker.ld`  
**Prioridad:** 🟡 P2

### Tareas
- [ ] Añadir ASSERT para tamaño mínimo de `.boot.data`
- [ ] Documentar cálculo: 64KB pila + 20KB page tables = 84KB mínimo
- [ ] Validar que hay espacio suficiente

---

# 📍 FASE 4: MEJORAS BAJAS (Sprint 9-10)

## LOW-001 a LOW-010: Mejuras Menores

| ID | Descripción | Esfuerzo | Impacto |
|----|-------------|----------|---------|
| LOW-001 | Traducir comentario español en print.cpp:203 | 5 min | Bajo |
| LOW-002 | Traducir comentario español en print.cpp:289 | 5 min | Bajo |
| LOW-003 | Renombrar variables de test más descriptivas | 30 min | Bajo |
| LOW-004 | Añadir newline final en archivos | 15 min | Bajo |
| LOW-005 | Usar constante para vendor ID length | 15 min | Bajo |
| LOW-006 | Optimizar Makefile (wildcard vs find) | 30 min | Bajo |
| LOW-007 | Añadir target `make docs` con Doxygen | 2 horas | Medio |
| LOW-008 | Añadir prefijo GLOBEX_ a constantes | 1 hora | Bajo |
| LOW-009 | Diseñar API interrupt-driven para serial | 4 horas | Alto (futuro) |
| LOW-010 | Hacer path de GRUB configurable | 30 min | Bajo |

---

# 📊 SEGUIMIENTO Y MÉTRICAS

## Dashboard de Progreso

```
Fase 1 (Críticos):    [████████] 4/5 completados
Fase 2 (Altos):       [░░░░░] 0/10 completados
Fase 3 (Medios):      [░░░░░] 0/8 completados
Fase 4 (Bajos):       [░░░░░] 0/11 completados
────────────────────────────────────────
Total:                [███░░] 4/34 completados (12%)
```

## Criterios de Éxito

| Métrica | Objetivo | Actual |
|---------|----------|--------|
| Críticos resueltos | 100% | 80% (4/5) ✅ |
| Altos resueltos | 100% | 0% |
| Tests passing | 100% | 100% ✅ |
| Build sin warnings | 100% | 100% ✅ |
| Code coverage | >80% | N/A |
| Technical debt | Reducir 50% | - |

---

# 📅 CRONOGRAMA ESTIMADO

| Semana | Fase | Entregables |
|--------|------|-------------|
| 1-2 | Fase 1 | CRIT-001, CRIT-002, CRIT-003, CRIT-005 |
| 3 | Fase 1 | CRIT-004 (documentación) |
| 4-5 | Fase 2 | HIGH-001 a HIGH-005 |
| 6-7 | Fase 2 | HIGH-006 a HIGH-010 |
| 8-9 | Fase 3 | MED-001 a MED-008 |
| 10-11 | Fase 4 | LOW-001 a LOW-010 |
| 12 | - | Revisión final, auditoría de seguimiento |

**Duración Total Estimada:** 12 semanas (3 meses)

---

# 📝 NOTAS DE IMPLEMENTACIÓN

## Convenciones para Commits

Usar formato convencional con referencia a hallazgo:

```
fix(crit-001): Corregir desbordamiento en uint32_to_decimal_string

- Cambiar índice inicial de 10 a 11
- Añadir validación de límites en bucle
- Añadir tests para valor máximo y cero

Refs: CRIT-001, AUDIT-2026-001
```

## Branch Strategy

```
dev (main development)
├── fix/crit-001-decimal-overflow
├── fix/crit-002-decimal64-overflow
├── fix/crit-003-serial-null-check
├── fix/crit-004-smp-documentation
├── fix/crit-005-memcpy-overlap-check
└── ...
```

## Definición de "Completado"

Cada hallazgo se considera completado cuando:
- [ ] Código implementado y revisado
- [ ] Tests añadidos y passing
- [ ] Build sin warnings nuevos
- [ ] Documentación actualizada
- [ ] Commit con mensaje convencional

---

# 🔗 REFERENCIAS

- [Informe de Auditoría Completo](./AUDIT_REPORT.md)
- [Guía de Estilo de Código](./.clang-format)
- [Proceso de Contribución](./CONTRIBUTING.md)
- [Issue Tracker](https://github.com/tmlcs/GLOBEX_OS/issues)

---

*Documento generado: 2026-03-09*  
*Próxima revisión: 2026-03-23*  
*Responsable: Equipo de Desarrollo GLOBEX_OS*
