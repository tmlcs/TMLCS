# GLOBEX_OS - Plan de Acción Post-Auditoría

**Fecha:** March 21, 2026
**Versión:** v0.015_x64
**Estado:** Plan aprobado para implementación

---

## Visión General

Este documento establece el plan de acción para abordar los hallazgos de la auditoría de código. Las tareas están organizadas por prioridad y fase de implementación.

### Objetivos del Plan

1. **Eliminar hallazgos ALTOS** - Críticos para estabilidad del sistema
2. **Reducir hallazgos MEDIOS** - Mejorar robustez y mantenibilidad
3. **Optimizar código** - Eliminar código muerto y mejorar estilo
4. **Expandir tests** - Aumentar cobertura de pruebas

---

## Roadmap por Fases

```
┌─────────────────────────────────────────────────────────────────┐
│  FASE 1: Crítico (Sprint 1-2)                                   │
│  ├── Handlers de interrupción completos                         │
│  ├── Unificar API de memoria                                    │
│  └── Validación de punteros                                     │
├─────────────────────────────────────────────────────────────────┤
│  FASE 2: Estabilidad (Sprint 3-4)                               │
│  ├── Slab allocator completo                                    │
│  ├── CPU feature detection                                      │
│  └── Tests de memoria                                           │
├─────────────────────────────────────────────────────────────────┤
│  FASE 3: Calidad (Sprint 5-6)                                   │
│  ├── Limpieza de código muerto                                  │
│  ├── Mejorar error reporting                                    │
│  └── Fix warnings estáticos                                     │
├─────────────────────────────────────────────────────────────────┤
│  FASE 4: Optimización (Sprint 7+)                               │
│  ├── Expansión de baud rates                                    │
│  ├── Coverage overflow detection                                │
│  └── Mejoras de logging                                         │
└─────────────────────────────────────────────────────────────────┘
```

---

## FASE 1: Crítico (Sprint 1-2)

**Duración estimada:** 2-3 semanas
**Prioridad:** 🔴 ALTA

### Tarea 1.1: Handlers de Interrupción Completos

**ID:** `FIX-IDT-001`
**Severidad:** ALTA
**Archivos:** `src/arch/x86_64/idt/interrupts.asm`, `src/arch/x86_64/idt/idt.cpp`

#### Descripción
Agregar stub handlers para todos los vectores de interrupción reservados (0-31) que actualmente no tienen handler.

#### Problema Actual
```asm
; ISR 9 doesn't exist (reserved)  <- Vector sin handler
ISR_ERR 10      ; Invalid TSS
```

#### Solución
```asm
; Agregar stub handler para vectores reservados
ISR_NOERR 9     ; Reserved (stub handler)
ISR_NOERR 11    ; Reserved (stub handler)
ISR_NOERR 12    ; Reserved (stub handler)
; ... etc para todos los vectores 0-31
```

#### Criterios de Aceptación
- [ ] Todos los vectores 0-31 tienen handler definido
- [ ] Handlers reservados llaman a panic() con código de error apropiado
- [ ] Tests de IDT verifican todos los vectores
- [ ] Documentación actualizada con tabla completa de vectores

#### Estimación: 4-6 horas

---

### Tarea 1.2: Unificar API de Memoria Free

**ID:** `FIX-MEM-001`
**Severidad:** ALTA
**Archivos:** `src/memory/heap.h`, `src/memory/slab.h`, `src/memory/heap.cpp`

#### Descripción
Unificar `kfree()` y `slab_free()` en una sola API coherente que detecte automáticamente el tipo de allocación.

#### Problema Actual
```c
// Dos APIs separadas - confuso y propenso a errores
void kfree(void* ptr);           // Solo para grandes allocaciones
void slab_free(void* ptr);       // Solo para slab allocations
```

#### Solución Propuesta
```c
// API unificada con detección automática
void kmem_free(void* ptr);

// Implementación:
void kmem_free(void* ptr) {
    if (ptr == NULL) return;
    
    // Detectar si es slab allocation (dirección en rango de slabs)
    if (is_slab_address(ptr)) {
        slab_free(ptr);
    } else {
        kfree(ptr);
    }
}

// Marcar APIs antiguas como deprecated
[[deprecated("Use kmem_free() instead")]]
void kfree(void* ptr);
```

#### Criterios de Aceptación
- [ ] `kmem_free()` detecta automáticamente tipo de allocación
- [ ] `kfree()` y `slab_free()` marcados como deprecated
- [ ] Tests verifican que free funciona para ambos tipos
- [ ] Documentación actualizada con nueva API

#### Estimación: 6-8 horas

---

### Tarea 1.3: Validación de Punteros en krealloc

**ID:** `FIX-MEM-002`
**Severidad:** ALTA
**Archivos:** `src/memory/heap.cpp`

#### Descripción
Agregar validación de punteros en `krealloc()` antes de llamar a `kmalloc_size()`.

#### Problema Actual
```cpp
void* krealloc(void* ptr, size_t new_size) {
    size_t old_size = kmalloc_size(ptr);  // ¿Qué pasa si ptr es inválido?
    // ...
}
```

#### Solución
```cpp
void* krealloc(void* ptr, size_t new_size) {
    // Validar puntero NULL
    if (ptr == NULL) {
        return kmalloc(new_size);
    }
    
    // Validar que ptr está en rango válido
    if (!is_valid_heap_pointer(ptr)) {
        return NULL;  // O panic en debug mode
    }
    
    size_t old_size = kmalloc_size(ptr);
    // ... resto de la implementación
}
```

#### Criterios de Aceptación
- [ ] `krealloc()` maneja NULL correctamente (retorna nueva allocación)
- [ ] `krealloc()` valida punteros inválidos
- [ ] Tests agregados para casos borde
- [ ] Documentación actualizada

#### Estimación: 3-4 horas

---

### Tarea 1.4: Test de Validación de Punteros NULL

**ID:** `TEST-MEM-001`
**Severidad:** MEDIA
**Archivos:** `tests/test_memory_nullptr.cpp` (nuevo)

#### Descripción
Agregar tests específicos para validación de punteros NULL en todas las funciones de memoria.

#### Tests a Implementar
```cpp
test_kmalloc_null_handling();      // kmalloc(0) debe retornar NULL o válido
test_kfree_null_handling();        // kfree(NULL) debe ser safe
test_krealloc_null_handling();     // krealloc(NULL, n) == kmalloc(n)
test_kcalloc_overflow_handling();  // kcalloc con overflow debe fallar safe
```

#### Criterios de Aceptación
- [ ] Todos los tests pasan
- [ ] Coverage de memoria > 80%
- [ ] Tests integrados en kernel_main()

#### Estimación: 4-5 horas

---

## FASE 2: Estabilidad (Sprint 3-4)

**Duración estimada:** 3-4 semanas
**Prioridad:** 🟠 MEDIA-ALTA

### Tarea 2.1: Completar Slab Allocator

**ID:** `FEAT-MEM-001`
**Severidad:** ALTA
**Archivos:** `src/memory/slab.h`, `src/memory/slab.cpp`

#### Descripción
Implementar caches de slab completos para todos los tamaños declarados en el header.

#### Estado Actual
```cpp
// Solo 32 bytes implementado
void* slab_alloc_32(void) { /* implementado */ }
void* slab_alloc_64(void) { return nullptr; }  // ❌ No implementado
void* slab_alloc_128(void) { return nullptr; } // ❌ No implementado
// ... etc
```

#### Implementación Requerida
```cpp
// Implementar todos los caches
void* slab_alloc_32(void);   // ✅ Ya implementado
void* slab_alloc_64(void);   // ❌ Implementar
void* slab_alloc_128(void);  // ❌ Implementar
void* slab_alloc_256(void);  // ❌ Implementar
void* slab_alloc_512(void);  // ❌ Implementar
void* slab_alloc_1024(void); // ❌ Implementar
void* slab_alloc_2048(void); // ❌ Implementar

void* kmem_alloc(size_t size);  // ❌ Debe usar slab para size <= 2048
void  kmem_free(void* ptr);     // ❌ Debe detectar slab vs bitmap
```

#### Criterios de Aceptación
- [ ] Todos los caches de slab implementados
- [ ] `kmem_alloc()` usa slab para tamaños <= 2048 bytes
- [ ] `kmem_free()` detecta automáticamente tipo de allocación
- [ ] Tests de stress para cada cache
- [ ] Métricas de uso de slab disponibles

#### Estimación: 12-16 horas

---

### Tarea 2.2: CPU Feature Detection Completo

**ID:** `FEAT-BOOT-001`
**Severidad:** MEDIA
**Archivos:** `src/arch/x86_64/boot/main.asm`

#### Descripción
Expandir detección de features de CPU más allá de solo long mode.

#### Estado Actual
```asm
check_long_mode:
    mov eax, 0x80000000
    cpuid
    ; Solo verifica bit 29 (long mode)
```

#### Features a Detectar
```asm
; Features requeridos para x86_64
- Long Mode (bit 29, CPUID 80000001h/EDX) ✅ Implementado
- PAE (bit 6, CPUID 01h/EDX)               ❌ Faltante
- NX (bit 20, CPUID 80000001h/EDX)         ❌ Faltante
- LA57 (bit 16, CPUID 80000001h/EDX)       ❌ Faltante (5-level paging)
- SMEP (bit 7, CPUID 07h/EBX)              ❌ Faltante
- SMAP (bit 20, CPUID 07h/EBX)             ❌ Faltante

; Features opcionales para reporting
- SSE, SSE2, SSE3, SSSE3, SSE4.x
- AVX, AVX2, AVX-512
- AES-NI, SHA extensions
- TSC, TSC invariant
- APIC, x2APIC
```

#### Criterios de Aceptación
- [ ] PAE y NX verificados antes de habilitar paging
- [ ] Features opcionales reportados en boot log
- [ ] Panic informativo si faltan features requeridos
- [ ] Tests de CPU actualizados

#### Estimación: 6-8 horas

---

### Tarea 2.3: Tests de Memory Allocator

**ID:** `TEST-MEM-002`
**Severidad:** MEDIA
**Archivos:** `tests/test_bitmap.cpp`, `tests/test_slab.cpp`, `tests/test_heap.cpp`

#### Descripción
Agregar tests exhaustivos para todos los allocadores de memoria.

#### Tests de Bitmap
```cpp
test_bitmap_alloc_single();      // Allocación simple
test_bitmap_alloc_multiple();    // Múltiples allocaciones
test_bitmap_free();              // Free y reuso
test_bitmap_exhaustion();        // Comportamiento cuando se llena
test_bitmap_fragmentation();     // Fragmentación y coalescencia
test_bitmap_boundary();          // Límites de página
```

#### Tests de Slab
```cpp
test_slab_alloc_32();            // Cache de 32 bytes
test_slab_alloc_64();            // Cache de 64 bytes
// ... para cada cache size
test_slab_free();                // Free retorna al cache
test_slab_exhaustion();          // Slab lleno, crear nuevo
test_slab_corruption();          // Detectar corrupción de metadata
```

#### Tests de Heap
```cpp
test_kmalloc_basic();            // Allocación básica
test_kmalloc_zero();             // kmalloc(0) comportamiento
test_kmalloc_large();            // Grandes allocaciones
test_kfree();                    // Free básico
test_kcalloc();                  // calloc con zero-fill
test_krealloc_grow();            // Realloc que crece
test_krealloc_shrink();          // Realloc que encoge
test_krealloc_null();            // krealloc(NULL, n)
test_kmem_alloc_slab();          // kmem_alloc usa slab para pequeños
test_kmem_alloc_bitmap();        // kmem_alloc usa bitmap para grandes
test_kmem_free_auto();           // kmem_free detecta tipo automáticamente
```

#### Criterios de Aceptación
- [ ] Todos los tests pasan
- [ ] Coverage de memoria > 90%
- [ ] Tests de stress incluidos
- [ ] Tests de边界 (boundary) incluidos

#### Estimación: 10-12 horas

---

## FASE 3: Calidad (Sprint 5-6)

**Duración estimada:** 2-3 semanas
**Prioridad:** 🟡 MEDIA

### Tarea 3.1: Limpieza de Código Muerto

**ID:** `CLEAN-001`
**Severidad:** BAJA
**Archivos:** Múltiples

#### Funciones a Eliminar
| Función | Archivo | Razón |
|---------|---------|-------|
| `get_serial_state()` | `serial.cpp:84-87` | No usada |
| `print_clear_row()` | `print.cpp` | No usada |
| `pit_register_callback()` | `pit.cpp` | Feature no implementado |
| `serial_try_lock()` | `spinlock.cpp` | Declarada pero no implementada |
| `irq_register_handler()` | `irq.cpp` | Feature no implementado |

#### Criterios de Aceptación
- [ ] Funciones no usadas eliminadas
- [ ] Declaraciones en headers eliminadas
- [ ] Tests actualizados si es necesario
- [ ] Documentación actualizada

#### Estimación: 2-3 horas

---

### Tarea 3.2: Mejorar Error Reporting en Boot

**ID:** `IMPROVE-BOOT-001`
**Severidad:** MEDIA
**Archivos:** `src/arch/x86_64/boot/main.asm`

#### Descripción
Mejorar mensajes de error en page table verification para facilitar debugging.

#### Estado Actual
```asm
.page_table_error:
    mov dword [0xb8000], 0x4f455450  ; "PTE" en rojo sobre blanco
    hlt
```

#### Solución
```asm
.page_table_error:
    ; Mostrar código de error específico
    ; 0x01 = CR3 invalid
    ; 0x02 = L4 entry invalid
    ; 0x03 = L3 entry invalid
    ; 0x04 = Write test failed
    mov dword [0xb8000], 0x4f525245  ; "ERR"
    mov dword [0xb8002], 0x4f202020  ; "   "
    mov [0xb8004], al                ; Código de error en hex
    hlt
```

#### Criterios de Aceptación
- [ ] Cada error tiene código único
- [ ] Mensajes descriptivos en documentación
- [ ] Tests de boot verifican error reporting

#### Estimación: 3-4 horas

---

### Tarea 3.3: Fix Warnings de Análisis Estático

**ID:** `CLEAN-002`
**Severidad:** BAJA
**Archivos:** Múltiples

#### Warnings a Fix
1. **Narrowing conversions** (`coverage.cpp:45`, `log.cpp:85,104`)
```cpp
// Antes
tmp[i++] = '0' + (value % 10);

// Después (con cast explícito)
tmp[i++] = static_cast<char>('0' + (value % 10));
```

2. **Short variable names** (`coverage.cpp:82,84`, `log.cpp:61,83,101,103`)
```cpp
// Antes
int j;

// Después
int index;  // Nombre descriptivo
```

3. **Pointer to const** (`atomic.h`)
```cpp
// Antes
uint32_t atomic_inc32(volatile uint32_t* ptr);

// Después
uint32_t atomic_inc32(volatile const uint32_t* ptr);
```

#### Criterios de Aceptación
- [ ] clang-tidy warnings reducidos en 80%
- [ ] cppcheck warnings reducidos a 0
- [ ] Código más legible

#### Estimación: 4-5 horas

---

## FASE 4: Optimización (Sprint 7+)

**Duración estimada:** 2-3 semanas
**Prioridad:** 🟢 BAJA

### Tarea 4.1: Expansión de Baud Rates

**ID:** `IMPROVE-SERIAL-001`
**Severidad:** BAJA
**Archivos:** `src/drivers/serial/serial.cpp`

#### Descripción
Expandir rango de baud rates soportados con validación apropiada del divisor.

#### Estado Actual
```cpp
if (baud < 110 || baud > 115200) {
    return 0;  // Rechazado
}
```

#### Solución
```cpp
// Validar divisor en lugar de rango fijo
uint16_t divisor = SERIAL_BASE_BAUD / baud;
if (divisor == 0 || divisor > 0xFFFF) {
    return 0;  // Divisor inválido
}

// Soportar baud rates extendidos
// 230400, 460800, 500000, 576000, 921600, 1000000, etc.
```

#### Criterios de Aceptación
- [ ] Baud rates extendidos soportados
- [ ] Validación de divisor apropiada
- [ ] Tests para baud rates extendidos

#### Estimación: 3-4 horas

---

### Tarea 4.2: Coverage Table Overflow Detection

**ID:** `IMPROVE-COV-001`
**Severidad:** BAJA
**Archivos:** `src/core/coverage/coverage.h`, `src/core/coverage/coverage.cpp`

#### Descripción
Agregar detección y reporting de overflow en coverage table.

#### Estado Actual
```cpp
#define COVERAGE_MAX_ENTRIES 512

// Silently fails when full
if (g_coverage_count >= COVERAGE_MAX_ENTRIES) {
    return;  // ❌ Sin warning
}
```

#### Solución
```cpp
if (g_coverage_count >= COVERAGE_MAX_ENTRIES) {
    // Warning en serial y VGA
    serial_write_str("[COVERAGE] WARNING: Table full, some coverage data lost\r\n");
    // Opcional: panic en debug mode
    #if DEBUG_ENABLE
    panic("Coverage table overflow");
    #endif
    return;
}
```

#### Criterios de Aceptación
- [ ] Warning emitido cuando table se llena
- [ ] Opción de panic en debug mode
- [ ] Métricas de coverage table disponibles

#### Estimación: 2-3 horas

---

### Tarea 4.3: Log Buffer Truncation Detection

**ID:** `IMPROVE-LOG-001`
**Severidad:** BAJA
**Archivos:** `src/core/log/log.cpp`

#### Descripción
Agregar detección y reporting de truncamiento en log buffer.

#### Estado Actual
```cpp
#define LOG_BUFFER_SIZE 256

// Trunca sin warning
if (len >= LOG_BUFFER_SIZE) {
    len = LOG_BUFFER_SIZE - 1;
}
```

#### Solución
```cpp
if (len >= LOG_BUFFER_SIZE) {
    len = LOG_BUFFER_SIZE - 1;
    // Marcar flag de truncamiento
    g_log_truncated = true;
    g_log_truncate_count++;
    
    // Warning opcional
    #if DEBUG_ENABLE
    serial_write_str("[LOG] WARNING: Message truncated\r\n");
    #endif
}
```

#### Criterios de Aceptación
- [ ] Truncamiento detectado y contado
- [ ] Warning en debug mode
- [ ] Métricas de truncamiento disponibles

#### Estimación: 2-3 horas

---

## Resumen de Estimaciones

| Fase | Tareas | Horas Totales | Semanas |
|------|--------|---------------|---------|
| FASE 1: Crítico | 4 | 17-23 | 2-3 |
| FASE 2: Estabilidad | 3 | 28-36 | 3-4 |
| FASE 3: Calidad | 3 | 9-12 | 2-3 |
| FASE 4: Optimización | 3 | 7-10 | 2-3 |
| **TOTAL** | **13** | **61-81** | **9-13** |

---

## Sistema de Tracking

### IDs de Tarea

Cada tarea tiene un ID único para tracking:
- `FIX-XXX-NNN` - Fixes críticos
- `FEAT-XXX-NNN` - Nuevas features
- `TEST-XXX-NNN` - Tests
- `CLEAN-NNN` - Limpieza de código
- `IMPROVE-XXX-NNN` - Mejoras

### Convención de Commits

```bash
# Para fixes críticos
git commit -m "fix(idt): Add stub handlers for reserved vectors [FIX-IDT-001]"

# Para nuevas features
git commit -m "feat(memory): Implement slab allocator for 64-2048 bytes [FEAT-MEM-001]"

# Para tests
git commit -m "test(memory): Add comprehensive memory allocator tests [TEST-MEM-002]"

# Para limpieza
git commit -m "chore: Remove unused get_serial_state() function [CLEAN-001]"

# Para mejoras
git commit -m "improve(boot): Add detailed error codes for page table failures [IMPROVE-BOOT-001]"
```

---

## Criterios de Éxito

### Al finalizar FASE 1
- [ ] 0 hallazgos ALTOS pendientes
- [ ] Todos los vectores de interrupción manejados
- [ ] API de memoria unificada
- [ ] Validación de punteros implementada

### Al finalizar FASE 2
- [ ] Slab allocator 100% funcional
- [ ] CPU feature detection completo
- [ ] Tests de memoria con >90% coverage

### Al finalizar FASE 3
- [ ] Código muerto eliminado
- [ ] Error reporting mejorado
- [ ] Warnings estáticos reducidos en 80%

### Al finalizar FASE 4
- [ ] Baud rates extendidos soportados
- [ ] Coverage overflow detection implementado
- [ ] Log truncation detection implementado

---

## Riesgos y Mitigación

| Riesgo | Impacto | Mitigación |
|--------|---------|------------|
| Slab allocator introduce bugs | ALTO | Tests exhaustivos, revisión de código |
| Cambios en IDT rompen IRQs | ALTO | Testing en QEMU con debug flags |
| Unificar kfree() causa regresión | MEDIO | Mantener compatibilidad backward |
| CPU detection muy estricta | MEDIO | Solo requerir features esenciales |

---

## Próximos Pasos Inmediatos

1. **Crear branch de desarrollo**
   ```bash
   git checkout -b feature/audit-fixes-phase1
   ```

2. **Comenzar con Tarea 1.1** (Handlers de interrupción)
   - Es la más crítica
   - Tiene menor riesgo de regresión
   - Establece base para otras mejoras

3. **Setup de CI/CD para tracking**
   - Agregar verificación de handlers de interrupción
   - Agregar métricas de coverage de memoria

---

*Documento creado: March 21, 2026*
*Revisión próxima: Al completar cada fase*
