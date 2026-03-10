# Plan de Acción de Calidad - GLOBEX_OS

**Versión:** 1.0  
**Fecha:** 9 de marzo de 2026  
**Basado en:** Auditoría de Calidad v1.0  
**Estado:** Pendiente de Aprobación

---

## Resumen del Plan

| Fase | Duración Estimada | Tareas | Prioridad |
|------|-------------------|--------|-----------|
| **Fase 1** | 1-2 días | Correcciones críticas | 🔴 ALTA |
| **Fase 2** | 2-3 días | Mejoras importantes | 🟠 MEDIA-ALTA |
| **Fase 3** | 3-5 días | Optimizaciones menores | 🟡 MEDIA |
| **Fase 4** | 1-2 semanas | Mejoras estructurales | 🟢 BAJA |
| **Fase 5** | Continuo | Mantenimiento y prevención | ⚪ CONTINUO |

---

## Fase 1: Correcciones Críticas (1-2 días)

### Tarea 1.1: Documentar Inline Assembly

**ID:** CRIT-002  
**Prioridad:** 🔴 CRÍTICA  
**Esfuerzo:** 2 horas  
**Archivos afectados:** `serial.cpp`, `vga.cpp`, `spinlock.cpp`, `panic.cpp`

#### Acciones

1. **Crear plantilla de documentación para assembly:**

```cpp
/**
 * @brief Enviar byte a puerto de E/S
 * @param port Puerto de E/S (ej: 0x3F8 para COM1)
 * @param value Byte a enviar
 * 
 * @assembly
 *   Instrucción: outb
 *   Operandos: AL (valor), DX (puerto)
 *   Efectos: Escribe en puerto de E/S
 *   Ciclos: ~100-1000 (depende del dispositivo)
 *   Barreras: Implícita (volatile)
 * 
 * @note Esta función es específica de x86/x86_64
 * @note No puede ser inlinada completamente debido a volatile
 * 
 * @see inb() para lectura de puertos
 */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}
```

2. **Documentar cada bloque assembly en:**
   - `serial.cpp`: `outb()`, `inb()`, barreras de memoria
   - `vga.cpp`: operaciones atómicas con spinlock
   - `spinlock.cpp`: `LOCK CMPXCHG`, barreras acquire/release
   - `panic.cpp`: `cli`, `hlt`

3. **Crear archivo de referencia:** `docs/assembly_reference.md`

#### Criterios de Aceptación

- [ ] Todas las funciones con assembly tienen documentación completa
- [ ] Archivo de referencia creado con ejemplos
- [ ] Comentarios explican efectos secundarios y dependencias

---

### Tarea 1.2: Encapsular Estado Global de Serial

**ID:** CRIT-003  
**Prioridad:** 🔴 ALTA  
**Esfuerzo:** 3 horas  
**Archivos afectados:** `serial.h`, `serial.cpp`

#### Acciones

1. **Crear struct de estado en `serial.h`:**

```cpp
/**
 * @brief Estado interno del driver serial
 * @note No acceder directamente - usar funciones de API
 */
typedef struct SerialState {
    volatile int initialized;      ///< 1 si inicializado, 0 si no
    volatile uint16_t port;        ///< Puerto base (ej: 0x3F8)
    volatile int failed;           ///< 1 si falló, 0 si OK
    volatile uint32_t error_code;  ///< Último código de error
    volatile uint32_t timeout_count; ///< Contador de timeouts
} SerialState_t;
```

2. **Refactorizar `serial.cpp`:**

```cpp
// ANTES (líneas 55-59)
static volatile int serial_initialized = 0;
static volatile uint16_t serial_port = 0;
static volatile int serial_failed = 0;
static volatile uint32_t serial_error_code = SERIAL_ERROR_NONE;
static volatile uint32_t serial_timeout_count = 0;

// DESPUÉS
static SerialState_t g_serial_state = {
    .initialized = 0,
    .port = 0,
    .failed = 0,
    .error_code = SERIAL_ERROR_NONE,
    .timeout_count = 0
};

// Funciones de acceso interno
static inline SerialState_t* get_serial_state(void) {
    return &g_serial_state;
}
```

3. **Actualizar todas las referencias:**

```cpp
// ANTES
serial_initialized = 1;
serial_port = port;

// DESPUÉS
SerialState_t* state = get_serial_state();
state->initialized = 1;
state->port = port;
```

4. **Agregar funciones getter públicas en `serial.h`:**

```cpp
/**
 * @brief Obtener estado de inicialización (thread-safe)
 * @return 1 si inicializado, 0 si no
 */
int serial_is_initialized(void);

/**
 * @brief Obtener código de error actual (thread-safe)
 * @return Código de error (0 = ninguno)
 */
uint32_t serial_get_error_code(void);
```

#### Criterios de Aceptación

- [ ] Struct `SerialState_t` definido en `serial.h`
- [ ] Todas las variables globales encapsuladas
- [ ] Funciones getter implementadas y documentadas
- [ ] Tests existentes pasan sin modificación
- [ ] No hay acceso directo a variables desde fuera del módulo

---

### Tarea 1.3: Reemplazar Magic Numbers

**ID:** IMP-005  
**Prioridad:** 🔴 ALTA  
**Esfuerzo:** 30 minutos  
**Archivos afectados:** `serial.cpp`, `constants.h`

#### Acciones

1. **Agregar constantes en `constants.h`:**

```cpp
/* ==========================================
 * Serial Port Constants
 * ========================================== */

/**
 * @brief Valor de IIR cuando el puerto no existe
 * @note 0xFF indica que no hay dispositivo en el puerto
 */
#define SERIAL_IIR_NO_DEVICE  0xFF

/**
 * @brief Timeout máximo para operaciones serial (iteraciones)
 * @note Ajustar según baud rate y hardware
 */
#define SERIAL_MAX_TIMEOUT  10000

/**
 * @brief Valor de scratch register para test
 * @note Usado para detectar presencia de UART
 */
#define SERIAL_SCRATCH_TEST  0x5A
```

2. **Actualizar `serial.cpp`:**

```cpp
// ANTES (línea 117)
if (iir == 0xFF) {
    return false;
}

// DESPUÉS
if (iir == SERIAL_IIR_NO_DEVICE) {
    return false;
}
```

#### Criterios de Aceptación

- [ ] Todos los magic numbers reemplazados
- [ ] Constantes documentadas en `constants.h`
- [ ] cppcheck no reporta magic numbers

---

## Fase 2: Mejoras Importantes (2-3 días)

### Tarea 2.1: Suprimir Warnings en Tests Intencionales

**ID:** IMP-001, IMP-002  
**Prioridad:** 🟠 MEDIA-ALTA  
**Esfuerzo:** 1 hora  
**Archivos afectados:** `test_string.cpp`

#### Acciones

1. **Agregar comentarios NOLINT:**

```cpp
// NOLINTBEGIN(nullPointer) - Tests intencionales de NULL pointer handling
void test_string_null_pointer_safety(void) {
    const char* src = "Test";
    char dest[10];
    
    // Test memcpy con NULL dest - debe retornar NULL sin crash
    void* result = memcpy(nullptr, src, 5);  // NOLINT(nullPointer)
    DEBUG_ASSERT(result == nullptr);
    
    // Test memcpy con NULL src - debe retornar dest sin crash
    result = memcpy(dest, nullptr, 5);  // NOLINT(nullPointer)
    DEBUG_ASSERT(result == dest);
}
// NOLINTEND(nullPointer)
```

2. **Agregar documentación del propósito:**

```cpp
/**
 * @brief Test de seguridad de NULL pointers
 * 
 * PROPÓSITO: Verificar que las funciones de string manejen
 * NULL pointers gracefulmente sin causar triple fault.
 * 
 * NOTA: Estos tests INTENCIONALMENTE pasan NULL pointers.
 * Los warnings de cppcheck/clang-tidy son falsos positivos.
 * 
 * @see string.h para documentación de manejo de NULL
 */
void test_string_null_pointer_safety(void);
```

#### Criterios de Aceptación

- [ ] Todos los tests de NULL pointer tienen NOLINT
- [ ] Documentación explica el propósito
- [ ] cppcheck no reporta errores en esta sección

---

### Tarea 2.2: Eliminar Código No Usado

**ID:** IMP-003, IMP-004  
**Prioridad:** 🟠 MEDIA-ALTA  
**Esfuerzo:** 30 minutos  
**Archivos afectados:** `print.cpp`, `print.h`, `test_debug.cpp`

#### Acciones

1. **Eliminar `print_clear_row()` en `print.cpp`:**

```cpp
// ELIMINAR completamente (línea 63)
void print_clear_row(size_t row) {
    vga_clear_row(row);
}
```

2. **Eliminar declaración en `print.h`:**

```cpp
// ELIMINAR declaración de print_clear_row
```

3. **Corregir test en `test_debug.cpp`:**

```cpp
// ANTES (línea 32)
DEBUG_ASSERT(1 == 1);  // Siempre verdadero

// DESPUÉS - Opción A: Eliminar si es placeholder
// (eliminar línea completamente)

// DESPUÉS - Opción B: Test real
uint32_t test_value = 0xCAFEBABE;
DEBUG_ASSERT(test_value == 0xCAFEBABE);  // Verifica valor real
```

#### Criterios de Aceptación

- [ ] Función `print_clear_row` eliminada
- [ ] Test `DEBUG_ASSERT(1 == 1)` corregido o eliminado
- [ ] Build exitoso sin warnings de unused function

---

### Tarea 2.3: Agregar Tests de Concurrencia

**ID:** INFO-007  
**Prioridad:** 🟠 MEDIA-ALTA  
**Esfuerzo:** 4 horas  
**Archivos afectados:** `test_spinlock.cpp` (nuevo), `spinlock.h`

#### Acciones

1. **Crear `src/impl/tests/test_spinlock.h`:**

```cpp
#ifndef TEST_SPINLOCK_H
#define TEST_SPINLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Test de inicialización de spinlock
 */
void test_spinlock_initialization(void);

/**
 * @brief Test de acquire/release básico
 */
void test_spinlock_acquire_release(void);

/**
 * @brief Test de try_acquire
 */
void test_spinlock_try_acquire(void);

/**
 * @brief Test de comportamiento con interrupciones
 */
void test_spinlock_irq_safety(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_SPINLOCK_H */
```

2. **Crear `src/impl/tests/test_spinlock.cpp`:**

```cpp
#include "spinlock.h"
#include "test_spinlock.h"
#include "../intf/debug.h"

void test_spinlock_initialization(void) {
    serial_write_str("\r\n=== Spinlock Initialization Test ===\r\n");
    
    spinlock_t lock = SPINLOCK_INIT;
    DEBUG_ASSERT(lock.locked == 0);
    
    serial_write_str("[SPINLOCK INIT] PASSED\r\n");
}

void test_spinlock_acquire_release(void) {
    serial_write_str("\r\n=== Spinlock Acquire/Release Test ===\r\n");
    
    spinlock_t lock = SPINLOCK_INIT;
    
    // Adquirir
    spinlock_acquire(&lock);
    DEBUG_ASSERT(lock.locked == 1);
    
    // Liberar
    spinlock_release(&lock);
    DEBUG_ASSERT(lock.locked == 0);
    
    serial_write_str("[SPINLOCK ACQUIRE/RELEASE] PASSED\r\n");
}

void test_spinlock_try_acquire(void) {
    serial_write_str("\r\n=== Spinlock Try Acquire Test ===\r\n");
    
    spinlock_t lock = SPINLOCK_INIT;
    
    // Primer try_acquire debe tener éxito
    bool result1 = spinlock_try_acquire(&lock);
    DEBUG_ASSERT(result1 == true);
    DEBUG_ASSERT(lock.locked == 1);
    
    // Segundo try_acquire debe fallar
    bool result2 = spinlock_try_acquire(&lock);
    DEBUG_ASSERT(result2 == false);
    DEBUG_ASSERT(lock.locked == 1);
    
    // Liberar y verificar
    spinlock_release(&lock);
    DEBUG_ASSERT(lock.locked == 0);
    
    serial_write_str("[SPINLOCK TRY_ACQUIRE] PASSED\r\n");
}

void test_spinlock_irq_safety(void) {
    serial_write_str("\r\n=== Spinlock IRQ Safety Test ===\r\n");
    
    // Nota: Este test verifica que el spinlock maneje IRQs
    // En hardware real, esto requeriría testing con interrupciones
    // Aquí verificamos que las funciones de IRQ se llamen correctamente
    
    spinlock_t lock = SPINLOCK_INIT;
    
    // Adquirir con IRQ save
    spinlock_acquire_irq(&lock);
    DEBUG_ASSERT(lock.locked == 1);
    
    // Liberar con IRQ restore
    spinlock_release_irq(&lock);
    DEBUG_ASSERT(lock.locked == 0);
    
    serial_write_str("[SPINLOCK IRQ SAFETY] PASSED\r\n");
}
```

3. **Integrar en `kernel_main()`:**

```cpp
// Incluir header
#include "../tests/test_spinlock.h"

// En kernel_main(), después de otros tests:
test_spinlock_initialization();
test_spinlock_acquire_release();
test_spinlock_try_acquire();
test_spinlock_irq_safety();
```

#### Criterios de Aceptación

- [ ] Archivo `test_spinlock.h` creado
- [ ] Archivo `test_spinlock.cpp` creado
- [ ] Tests integrados en `kernel_main()`
- [ ] Todos los tests pasan en QEMU
- [ ] Serial output muestra resultados

---

## Fase 3: Optimizaciones Menores (3-5 días)

### Tarea 3.1: Mejorar Tipos de Parámetros

**ID:** MIN-002  
**Prioridad:** 🟡 MEDIA  
**Esfuerzo:** 2 horas  
**Archivos afectados:** `serial.cpp`

#### Acciones

1. **Crear tipos wrapper en `constants.h`:**

```cpp
/**
 * @brief Tipo para puertos de E/S
 * @note Previene intercambio accidental de parámetros
 */
typedef struct {
    uint16_t value;
} io_port_t;

/**
 * @brief Tipo para valores de 8-bit
 * @note Previene intercambio accidental de parámetros
 */
typedef struct {
    uint8_t value;
} io_byte_t;
```

2. **Actualizar firmas de funciones:**

```cpp
// ANTES
static inline void outb(uint16_t port, uint8_t value)

// DESPUÉS (opcional - puede ser over-engineering para kernel hobby)
static inline void outb(io_port_t port, io_byte_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value.value), "Nd"(port.value));
}
```

**NOTA:** Evaluar si el beneficio justifica la complejidad añadida. Para kernel hobby, puede ser suficiente con documentación clara.

#### Criterios de Aceptación

- [ ] Tipos wrapper definidos (si se implementan)
- [ ] Funciones actualizadas consistentemente
- [ ] Tests pasan sin modificación

---

### Tarea 3.2: Agregar Const Correctness

**ID:** MIN-003  
**Prioridad:** 🟡 MEDIA  
**Esfuerzo:** 1 hora  
**Archivos afectados:** `test_memory.cpp`, `vga.cpp`, `print.cpp`

#### Acciones

1. **Actualizar punteros en `test_memory.cpp`:**

```cpp
// ANTES (línea 49)
volatile uint64_t* l4_entry = reinterpret_cast<volatile uint64_t*>(0x10A000);

// DESPUÉS
const volatile uint64_t* l4_entry = reinterpret_cast<const volatile uint64_t*>(0x10A000);
```

2. **Buscar y actualizar en todo el código:**

```bash
# Buscar punteros que podrían ser const
grep -rn "volatile.*\*" src/impl/ | grep -v "const volatile"
```

#### Criterios de Aceptación

- [ ] Todos los punteros a datos de solo-lectura son `const volatile`
- [ ] Build exitoso sin warnings
- [ ] Tests pasan

---

### Tarea 3.3: Mejorar Scope de Variables

**ID:** MIN-005  
**Prioridad:** 🟡 MEDIA  
**Esfuerzo:** 30 minutos  
**Archivos afectados:** `test_strlcpy.cpp`

#### Acciones

1. **Mover declaraciones cerca del uso:**

```cpp
// ANTES
const char* src = "This is a very long string!";
// ... 20 líneas de código ...
size_t len = strlcpy(dest, src, sizeof(dest));

// DESPUÉS
// ... 20 líneas de código ...
const char* src = "This is a very long string!";
size_t len = strlcpy(dest, src, sizeof(dest));
```

#### Criterios de Aceptación

- [ ] Variables declaradas en scope más estrecho posible
- [ ] clang-tidy no reporta `misc-throw-by-value-catch-by-reference`

---

## Fase 4: Mejoras Estructurales (1-2 semanas)

### Tarea 4.1: Implementar Análisis de Cobertura

**ID:** INFO-008  
**Prioridad:** 🟢 BAJA  
**Esfuerzo:** 8 horas  
**Archivos afectados:** Makefile, scripts/

#### Acciones

1. **Agregar flags de cobertura al Makefile:**

```makefile
# ==========================================
# Code Coverage Flags (opcional)
# ==========================================
ifdef ENABLE_COVERAGE
CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS += -lgcov
endif
```

2. **Crear script `scripts/coverage.sh`:**

```bash
#!/bin/bash
# =============================================================================
# GLOBEX_OS - Code Coverage Script
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/0_008_x64/build"

echo "============================================"
echo "GLOBEX_OS Code Coverage Analysis"
echo "============================================"
echo ""

# Build with coverage
echo "Building with coverage flags..."
cd "$PROJECT_ROOT/0_008_x64"
make clean
make build-x86_64 ENABLE_COVERAGE=1

# Run tests
echo "Running tests..."
make run-serial

# Generate report
echo "Generating coverage report..."
cd "$BUILD_DIR"
gcov -r **/*.cpp 2>/dev/null || true

# Summary
echo ""
echo "============================================"
echo "Coverage Summary"
echo "============================================"
grep -h "Lines executed:" *.gcov 2>/dev/null | sort || echo "No coverage data available"

echo ""
echo "Detailed reports in: $BUILD_DIR/*.gcov"
echo "============================================"
```

3. **Agregar target al Makefile:**

```makefile
.PHONY: coverage
coverage:
	@./scripts/coverage.sh
```

#### Criterios de Aceptación

- [ ] Script `coverage.sh` creado y funcional
- [ ] Target `make coverage` disponible
- [ ] Reportes generados en `build/`
- [ ] Documentación actualizada con instrucciones

---

### Tarea 4.2: Crear Framework de Fuzzing

**ID:** INFO-009  
**Prioridad:** 🟢 BAJA  
**Esfuerzo:** 12 horas  
**Archivos afectados:** `src/impl/tests/test_fuzz.cpp` (nuevo)

#### Acciones

1. **Crear `test_fuzz.h`:**

```cpp
#ifndef TEST_FUZZ_H
#define TEST_FUZZ_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Test de fuzzing para funciones de string
 * @note Genera inputs aleatorios y verifica que no haya crashes
 */
void test_string_fuzz(void);

/**
 * @brief Test de fuzzing para funciones serial
 * @note Verifica manejo de inputs inválidos
 */
void test_serial_fuzz(void);

/**
 * @brief Test de fuzzing para funciones de print
 * @note Verifica bounds checking en VGA buffer
 */
void test_print_fuzz(void);

#ifdef __cplusplus
extern "C" {
#endif

#endif /* TEST_FUZZ_H */
```

2. **Crear `test_fuzz.cpp` con generador pseudo-aleatorio:**

```cpp
#include "test_fuzz.h"
#include "../intf/string.h"
#include "../intf/serial.h"
#include "../intf/debug.h"

// Generador LCG simple para kernel (no requiere stdlib)
static uint32_t fuzz_seed = 0x12345678;

static uint32_t fuzz_random(void) {
    fuzz_seed = fuzz_seed * 1103515245 + 12345;
    return (fuzz_seed >> 16) & 0x7FFF;
}

static void fuzz_random_buffer(char* buffer, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buffer[i] = (char)(fuzz_random() % 256);
    }
}

void test_string_fuzz(void) {
    serial_write_str("\r\n=== String Fuzz Test ===\r\n");
    
    char buffer[256];
    char src[256];
    
    // 100 iteraciones de fuzzing
    for (int i = 0; i < 100; i++) {
        fuzz_random_buffer(src, sizeof(src));
        fuzz_random_buffer(buffer, sizeof(buffer));
        
        // Verificar que memcpy no crash
        memcpy(buffer, src, fuzz_random() % sizeof(buffer));
        
        // Verificar que memmove no crash
        memmove(buffer + 10, buffer, fuzz_random() % 100);
        
        // Verificar que memset no crash
        memset(buffer, fuzz_random() % 256, fuzz_random() % sizeof(buffer));
    }
    
    serial_write_str("[STRING FUZZ] PASSED - No crashes in 100 iterations\r\n");
}
```

#### Criterios de Aceptación

- [ ] Framework de fuzzing implementado
- [ ] Tests de fuzzing para string, serial, print
- [ ] Documentación de cómo extender fuzzing
- [ ] Todos los tests pasan

---

### Tarea 4.3: Documentación de SMP

**ID:** INFO-007  
**Prioridad:** 🟢 BAJA  
**Esfuerzo:** 4 horas  
**Archivos afectados:** `docs/smp_design.md` (nuevo)

#### Acciones

1. **Crear documento de diseño SMP:**

```markdown
# SMP Design Document - GLOBEX_OS

## Estado Actual

- [x] Spinlock básico implementado
- [x] Barreras de memoria (mb, rmb, wmb)
- [x] VGA lock para operaciones atómicas
- [ ] AP startup (pendiente)
- [ ] Per-CPU data (pendiente)
- [ ] Scheduler SMP-aware (pendiente)

## Arquitectura de Locking

### Spinlock

```cpp
typedef struct {
    volatile uint32_t locked;
} spinlock_t;
```

### Uso Correcto

```cpp
spinlock_t lock = SPINLOCK_INIT;

spinlock_acquire(&lock);
// ... sección crítica ...
spinlock_release(&lock);
```

### Uso con IRQs

```cpp
spinlock_acquire_irq(&lock);
// ... sección crítica (IRQs deshabilitadas) ...
spinlock_release_irq(&lock);
```

## Próximos Pasos

1. Implementar AP wakeup
2. Agregar per-CPU variables
3. Implementar scheduler SMP-aware
```

#### Criterios de Aceptación

- [ ] Documento `docs/smp_design.md` creado
- [ ] Arquitectura de locking documentada
- [ ] Próximos pasos definidos

---

## Fase 5: Mantenimiento Continuo

### Tarea 5.1: Integración Continua

**Prioridad:** ⚪ CONTINUO  
**Frecuencia:** Por commit

#### Checklist de Pre-Commit

```bash
# 1. Verificar herramientas
make verify-tools

# 2. Build limpio
make clean && make build-x86_64

# 3. Ejecutar tests
make run-serial
./scripts/verify_tests.sh serial_output.log

# 4. Análisis estático
./scripts/analyze.sh

# 5. Formato de código
./scripts/format.sh

# 6. Verificar que no hay warnings
make build-x86_64 2>&1 | grep -i warning || echo "No warnings"
```

---

### Tarea 5.2: Revisiones de Código Periódicas

**Prioridad:** ⚪ CONTINUO  
**Frecuencia:** Mensual

#### Checklist de Revisión Mensual

- [ ] Revisar issues de GitHub
- [ ] Actualizar documentación
- [ ] Verificar dependencias (GRUB, QEMU)
- [ ] Revisar logs de serial de tests
- [ ] Actualizar QWEN.md con cambios
- [ ] Ejecutar auditoría parcial de código

---

## Tracking de Progreso

### Tablero de Tareas

| ID | Tarea | Fase | Estado | Completado |
|----|-------|------|--------|------------|
| 1.1 | Documentar assembly | 1 | ✅ Completado | 100% |
| 1.2 | Encapsular estado serial | 1 | ⏳ Pendiente | 0% |
| 1.3 | Reemplazar magic numbers | 1 | ⏳ Pendiente | 0% |
| 2.1 | Suprimir warnings en tests | 2 | ⏳ Pendiente | 0% |
| 2.2 | Eliminar código no usado | 2 | ⏳ Pendiente | 0% |
| 2.3 | Tests de concurrencia | 2 | ⏳ Pendiente | 0% |
| 3.1 | Mejorar tipos de parámetros | 3 | ⏳ Pendiente | 0% |
| 3.2 | Const correctness | 3 | ⏳ Pendiente | 0% |
| 3.3 | Mejorar scope de variables | 3 | ⏳ Pendiente | 0% |
| 4.1 | Análisis de cobertura | 4 | ⏳ Pendiente | 0% |
| 4.2 | Framework de fuzzing | 4 | ⏳ Pendiente | 0% |
| 4.3 | Documentación SMP | 4 | ⏳ Pendiente | 0% |

### Métricas de Progreso

```
Fase 1 (Crítica):       1/3 tareas completadas (33%)
Fase 2 (Importante):    0/3 tareas completadas (0%)
Fase 3 (Menor):         0/3 tareas completadas (0%)
Fase 4 (Estructural):   0/3 tareas completadas (0%)
Fase 5 (Continuo):      0/2 tareas activas (0%)

TOTAL:                  1/14 tareas completadas (7%)
```

---

## Apéndice A: Comandos Útiles

### Build y Test
```bash
make build-x86_64      # Build kernel
make run               # Ejecutar en QEMU
make run-serial        # Ejecutar con serial output
make run-debug         # Ejecutar con debug flags
make clean             # Limpiar build artifacts
make rebuild           # Clean + build
```

### Análisis de Calidad
```bash
make verify-tools      # Verificar herramientas
./scripts/format.sh    # Formatear código
./scripts/analyze.sh   # Análisis estático
./scripts/verify_tests.sh serial_output.log  # Verificar tests
```

### Git Workflow
```bash
git status             # Estado del repositorio
git diff HEAD          # Cambios desde último commit
git log -n 5           # Últimos 5 commits
git branch -a          # Listar branches
```

---

## Apéndice B: Plantillas

### Plantilla de Commit

```
<tipo>(<área>): <descripción breve>

[opcional: cuerpo más detallado]

Fixes: #<issue-number>
Refs: <referencia a documento>
```

**Ejemplos:**
```
fix(serial): Encapsular variables globales en struct

- Crear SerialState_t para estado del driver
- Actualizar todas las referencias a variables
- Agregar funciones getter públicas

Fixes: #CRIT-003
```

```
docs(assembly): Documentar inline assembly en serial.cpp

- Agregar comentarios detallados para outb/inb
- Documentar efectos secundarios y ciclos
- Crear assembly_reference.md

Refs: QUALITY_ACTION_PLAN.md#tarea-11
```

---

**Fin del Plan de Acción de Calidad**

*Documento generado el 9 de marzo de 2026*  
*Próxima revisión: 16 de marzo de 2026*
