# GLOBEX_OS - Plan Maestro de Calidad del Código

**Versión:** 1.0  
**Fecha:** 2026-03-08  
**Horizonte:** 6 meses  
**Responsable:** Equipo de Desarrollo

---

## Tabla de Contenidos

1. [Resumen Ejecutivo](#1-resumen-ejecutivo)
2. [Objetivos de Calidad](#2-objetivos-de-calidad)
3. [Matriz de Priorización](#3-matriz-de-priorización)
4. [Roadmap de Implementación](#4-roadmap-de-implementación)
5. [Planes de Acción Detallados](#5-planes-de-acción-detallados)
6. [Infraestructura de Calidad](#6-infraestructura-de-calidad)
7. [Métricas y Seguimiento](#7-métricas-y-seguimiento)
8. [Gestión de Riesgos](#8-gestión-de-riesgos)
9. [Anexos](#9-anexos)

---

## 1. Resumen Ejecutivo

### 1.1 Estado Actual

| Dimensión | Estado | Puntuación |
|-----------|--------|------------|
| **Seguridad** | ⚠️ Moderado | 6.5/10 |
| **Mantenibilidad** | ✅ Bueno | 7.5/10 |
| **Confiabilidad** | ⚠️ Moderado | 6.0/10 |
| **Eficiencia** | ✅ Bueno | 8.0/10 |
| **Documentación** | ✅ Excelente | 8.5/10 |
| **Testing** | ❌ Ausente | 2.0/10 |
| **Puntuación Global** | **⚠️ Moderado** | **6.4/10** |

### 1.2 Inversión Requerida

| Fase | Duración | Esfuerzo Estimado |
|------|----------|-------------------|
| Fase 1: Críticos | 2 semanas | 40 horas |
| Fase 2: Altos | 4 semanas | 80 horas |
| Fase 3: Medios | 6 semanas | 60 horas |
| Fase 4: Infraestructura | 8 semanas | 100 horas |
| **Total** | **5 meses** | **280 horas** |

---

## 2. Objetivos de Calidad

### 2.1 Objetivos SMART

#### Corto Plazo (0-2 meses)

| ID | Objetivo | Métrica | Target |
|----|----------|---------|--------|
| O1 | Eliminar todos los hallazgos críticos | # Critical issues | 0 |
| O2 | Implementar validación de parámetros consistente | % Functions validated | 100% |
| O3 | Eliminar código duplicado | # Duplicate functions | 0 |
| O4 | Establecer configuración de formato automática | Files with .clang-format | 100% |

#### Mediano Plazo (2-4 meses)

| ID | Objetivo | Métrica | Target |
|----|----------|---------|--------|
| O5 | Implementar tests unitarios para funciones core | % Core functions tested | 80% |
| O6 | Documentar thread safety en todas las funciones públicas | % Functions documented | 100% |
| O7 | Alcanzar 0 warnings en compilación | # Warnings | 0 |
| O8 | Implementar CI básico | Build automation | 100% |

#### Largo Plazo (4-6 meses)

| ID | Objetivo | Métrica | Target |
|----|----------|---------|--------|
| O9 | Tests automatizados en QEMU | # Test cases | 50+ |
| O10 | Documentación API completa con Doxygen | % API documented | 100% |
| O11 | Implementar sistema de logging estructurado | Logging system | Complete |
| O12 | Análisis estático integrado en CI | Static analysis coverage | 100% |

---

## 3. Matriz de Priorización

### 3.1 Matriz Impacto/Esfuerzo

```
                    IMPACTO
              Alto │  Medium  │  Bajo
                   │          │
         ┌─────────┼──────────┼─────────┐
         │ [C-001] │ [H-005]  │ [L-004] │
    Alto │ [C-004] │ [H-006]  │ [L-013] │
         │ [C-005] │ [M-008]  │ [L-014] │
         ├─────────┼──────────┼─────────┤
ESFUERZO │ [H-001] │ [M-001]  │ [L-001] │
  Medium │ [H-002] │ [M-004]  │ [L-005] │
         │ [H-003] │ [M-006]  │ [L-007] │
         ├─────────┼──────────┼─────────┤
    Bajo │ [H-004] │ [M-010]  │ [L-002] │
         │         │ [M-012]  │ [L-003] │
         │         │ [M-015]  │ [L-006] │
         └─────────┴──────────┴─────────┘
```

### 3.2 Categorización de Hallazgos

#### Críticos (Resolver en 2 semanas)

| ID | Descripción | Impacto | Urgencia |
|----|-------------|---------|----------|
| C-001 | BSS Initialization Race Condition | Seguridad | Inmediata |
| C-004 | Serial Timeout Silent Failure | Debugging | Inmediata |
| C-005 | VGA Buffer Write Reordering | Display | Inmediata |

#### Altos (Resolver en 1-2 meses)

| ID | Descripción | Impacto | Urgencia |
|----|-------------|---------|----------|
| H-001 | print_set_cursor Boundary Validation | Seguridad | Alta |
| H-002 | memmove d==s Optimization | Eficiencia | Media |
| H-003 | memcpy Overlap Verification | Seguridad | Alta |
| H-004 | Serial Baud Rate Validation | Robustez | Alta |
| H-005 | Hex64 Function Deduplication | Mantenibilidad | Media |
| H-006 | Page Table Setup Verification | Seguridad | Alta |
| H-007 | panic() VGA Initialization Check | Robustez | Media |

#### Medios (Resolver en 2-4 meses)

| ID | Descripción | Impacto | Urgencia |
|----|-------------|---------|----------|
| M-001 | Null Validation Style Consistency | Mantenibilidad | Media |
| M-004 | Thread Safety Documentation | Documentación | Media |
| M-008 | memset Optimization | Eficiencia | Baja |
| M-010 | Multiboot2 Magic Number Verification | Seguridad | Alta |
| M-012 | Kernel Version Symbol | Mantenibilidad | Baja |
| M-015 | BSS Bounds Validation | Seguridad | Media |

#### Bajos (Resolver en 4-6 meses)

| ID | Descripción | Impacto | Urgencia |
|----|-------------|---------|----------|
| L-001 | Comment Language Standardization | Consistencia | Baja |
| L-002 | .gitattributes for Line Endings | Consistencia | Baja |
| L-003 | .clang-format Configuration | Consistencia | Baja |
| L-006 | Automated Test Target | Testing | Media |
| L-013 | .editorconfig | Consistencia | Baja |

---

## 4. Roadmap de Implementación

### 4.1 Timeline Visual

```
Mes 1          Mes 2          Mes 3          Mes 4          Mes 5          Mes 6
│──────────────│──────────────│──────────────│──────────────│──────────────│
│ FASE 1       │ FASE 2       │ FASE 3       │ FASE 4       │ FASE 4       │
│ Críticos     │ Altos        │ Medios       │ Infraestruct │ Consolidación│
│              │              │              │ ura          │              │
├──────────────┼──────────────┼──────────────┼──────────────┼──────────────┤
│ [C-001] BSS  │ [H-001] Print│ [M-001] Style│ [L-002] Git  │ [O9] QEMU    │
│ [C-004] Serial│ [H-003] Memcpy│ [M-004] Thread│ [L-003] Format│ Tests       │
│ [C-005] VGA  │ [H-004] Baud │ [M-008] Memset│ [L-006] Test │ [O10] Doxygen│
│ [O4] Format  │ [H-005] Dedup│ [M-010] Multi│ [O8] CI      │ [O11] Logging│
│ [O1] Zero    │ [H-006] Pages│ [M-015] BSS  │ [O5] Unit    │ [O12] Static │
│ Critical     │ [O2] Validate│ [O6] Doc     │ Tests        │ Analysis     │
```

### 4.2 Hitos Principales

| Hito | Fecha Target | Entregables |
|------|--------------|-------------|
| M1: Críticos Resueltos | 2026-03-22 | 0 critical issues |
| M2: Validación Completa | 2026-04-19 | 100% param validation |
| M3: Tests Unitarios | 2026-05-17 | 80% core coverage |
| M4: CI Implementado | 2026-06-14 | Automated builds |
| M5: Calidad Alcanzada | 2026-08-09 | 8.0/10 score |

---

## 5. Planes de Acción Detallados

### 5.1 Fase 1: Críticos (Semanas 1-2)

#### Tarea C-001: BSS Initialization Documentation

**Descripción:** Documentar y mitigar ventana de race condition en inicialización BSS

**Acciones:**
1. [ ] Agregar comentario explícito en `main.asm` prohibiendo acceso a BSS
2. [ ] Agregar `DEBUG_ASSERT` temprano en `kernel_main()` verificando BSS
3. [ ] Documentar en ARCHITECTURE.md el boot sequence

**Archivos:**
- `src/impl/x86_64/boot/main.asm`
- `src/impl/kernel/main.cpp`
- `docs/ARCHITECTURE.md` (nuevo)

**Criterios de Aceptación:**
- [ ] Comment added in assembly code
- [ ] ASSERT added in kernel_main (first 10 lines)
- [ ] Architecture document created

**Estimado:** 4 horas

---

#### Tarea C-004: Serial Timeout Improvement

**Descripción:** Separar flag de fallo de flag de inicialización

**Acciones:**
1. [ ] Agregar variable `serial_failed` separada
2. [ ] Modificar `serial_wait_transmit_empty_timeout` para setear `serial_failed`
3. [ ] Agregar función `serial_get_status()` que retorne estado detallado
4. [ ] Reportar fallo vía VGA cuando ocurra

**Archivos:**
- `src/impl/x86_64/serial.cpp`
- `src/intf/serial.h`

**Código Propuesto:**
```cpp
// serial.h
typedef enum SerialStatus {
    SERIAL_STATUS_UNINITIALIZED = 0,
    SERIAL_STATUS_OK = 1,
    SERIAL_STATUS_FAILED = 2,
} SerialStatus_t;

SerialStatus_t serial_get_status(void);

// serial.cpp
static volatile int serial_failed = 0;

SerialStatus_t serial_get_status(void) {
    if (serial_failed) return SERIAL_STATUS_FAILED;
    if (!serial_initialized) return SERIAL_STATUS_UNINITIALIZED;
    return SERIAL_STATUS_OK;
}
```

**Criterios de Aceptación:**
- [ ] New status enum defined
- [ ] serial_failed variable added
- [ ] serial_get_status() implemented
- [ ] VGA notification on failure

**Estimado:** 6 horas

---

#### Tarea C-005: VGA Atomic Write

**Descripción:** Consolidar writes a VGA buffer como acceso atómico de 16-bit

**Acciones:**
1. [ ] Modificar `print_char` para escribir como uint16_t
2. [ ] Verificar que `clear_row` ya usa este patrón (consistencia)
3. [ ] Agregar comentario explicando por qué es necesario

**Archivos:**
- `src/impl/x86_64/print.cpp`

**Código Propuesto:**
```cpp
void print_char(char character) {
    // ... existing switch for \n, \r, \t ...
    
    if (col >= VGA_COLS) {
        print_newline();
        memory_barrier();
        col = cursor_col;
        row = cursor_row;
    }

    if (is_valid_position(row, col)) {
        // Atomic 16-bit write to prevent character/color tearing
        volatile uint16_t* cell = reinterpret_cast<volatile uint16_t*>(
            &vga_buffer[vga_index(row, col)]);
        *cell = static_cast<uint16_t>(character) | 
                (static_cast<uint16_t>(current_color) << 8);
        cursor_col = col + 1;
        memory_barrier();
    }
}
```

**Criterios de Aceptación:**
- [ ] print_char uses 16-bit atomic write
- [ ] Code comment explains rationale
- [ ] Visual testing shows no flickering

**Estimado:** 4 horas

---

#### Tarea O1-1: Zero Critical Issues

**Descripción:** Verificar que todos los críticos estén resueltos

**Acciones:**
1. [ ] Re-run audit checklist
2. [ ] Update CODE_AUDIT_REPORT.md con estado
3. [ ] Commit con mensaje "fix: Resolve all critical issues"

**Criterios de Aceptación:**
- [ ] All C-xxx tasks completed
- [ ] Audit report updated
- [ ] Code review approved

**Estimado:** 2 horas

---

### 5.2 Fase 2: Altos (Semanas 3-6)

#### Tarea H-001: Print Cursor Validation

**Descripción:** Mejorar validación en `print_set_cursor`

**Acciones:**
1. [ ] Agregar validación para valores negativos (size_t es unsigned, pero igual validar)
2. [ ] Agregar logging en debug mode cuando se hace clamp
3. [ ] Documentar comportamiento de clamp

**Archivos:**
- `src/impl/x86_64/print.cpp`
- `src/intf/print.h`

**Criterios de Aceptación:**
- [ ] Boundary validation enhanced
- [ ] Debug logging added
- [ ] Documentation updated

**Estimado:** 3 horas

---

#### Tarea H-003: Memcpy Overlap Detection

**Descripción:** Agregar detección de overlap en debug mode

**Acciones:**
1. [ ] Agregar función `memory_regions_overlap`
2. [ ] Usar `DEBUG_ASSERT` en `memcpy` cuando `DEBUG_ENABLE=1`
3. [ ] Documentar que caller debe garantizar no-overlap

**Archivos:**
- `src/impl/x86_64/string.cpp`
- `src/intf/string.h`

**Código Propuesto:**
```cpp
// Helper function for debug builds
static inline bool memory_regions_overlap(
    const void* p1, size_t s1,
    const void* p2, size_t s2) 
{
    uintptr_t start1 = reinterpret_cast<uintptr_t>(p1);
    uintptr_t end1 = start1 + s1;
    uintptr_t start2 = reinterpret_cast<uintptr_t>(p2);
    uintptr_t end2 = start2 + s2;
    
    return (start1 < end2) && (end1 > start2);
}

void* memcpy(void* dest, const void* src, size_t n) {
    #if DEBUG_ENABLE
    DEBUG_ASSERT(!memory_regions_overlap(dest, n, src, n));
    #endif
    
    uint8_t* d = static_cast<uint8_t*>(dest);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    
    while (n--) {
        *d++ = *s++;
    }
    
    return dest;
}
```

**Criterios de Aceptación:**
- [ ] Overlap detection implemented
- [ ] Only active in DEBUG_ENABLE mode
- [ ] Documentation warns about overlap

**Estimado:** 4 horas

---

#### Tarea H-004: Serial Baud Rate Validation

**Descripción:** Validar rango de baud rates soportados

**Acciones:**
1. [ ] Definir constantes `SERIAL_MIN_BAUD` y `SERIAL_MAX_BAUD`
2. [ ] Agregar validación en `serial_init`
3. [ ] Documentar baud rates soportados

**Archivos:**
- `src/impl/x86_64/serial.cpp`
- `src/intf/serial.h`

**Código Propuesto:**
```cpp
// serial.h
#define SERIAL_MIN_BAUD  110
#define SERIAL_MAX_BAUD  115200

// serial.cpp
int serial_init(uint16_t port, uint32_t baud) {
    if (baud == 0) {
        return 0;
    }
    
    if (baud < SERIAL_MIN_BAUD || baud > SERIAL_MAX_BAUD) {
        return 0;  // Baud rate out of supported range
    }
    
    // ... rest of init
}
```

**Criterios de Aceptación:**
- [ ] Constants defined in header
- [ ] Validation in serial_init
- [ ] Documentation lists supported rates

**Estimado:** 3 horas

---

#### Tarea H-005: Hex64 Deduplication

**Descripción:** Eliminar duplicación de funciones hex64

**Acciones:**
1. [ ] Mover implementación a `string.cpp` como función interna
2. [ ] `print_hex64` y `serial_write_hex64` llaman a función común
3. [ ] Actualizar headers

**Archivos:**
- `src/impl/x86_64/string.cpp` (nueva función)
- `src/impl/x86_64/print.cpp`
- `src/impl/x86_64/serial.cpp`

**Código Propuesto:**
```cpp
// string.cpp - Internal utility
namespace internal {
    void format_hex64(uint64_t value, char* buffer, size_t buffer_size) {
        static const char hex_chars[] = "0123456789ABCDEF";
        
        if (buffer_size < 19) return;  // Safety check
        
        buffer[0] = '0';
        buffer[1] = 'x';
        
        for (int i = 0; i < 16; i++) {
            buffer[2 + i] = hex_chars[(value >> (60 - i * 4)) & 0xF];
        }
        buffer[18] = '\0';
    }
}

// print.cpp
void print_hex64(uint64_t value) {
    char buffer[19];
    internal::format_hex64(value, buffer, sizeof(buffer));
    print_str(buffer);
}

// serial.cpp
void serial_write_hex64(uint64_t value) {
    char buffer[19];
    internal::format_hex64(value, buffer, sizeof(buffer));
    serial_write_str(buffer);
}
```

**Criterios de Aceptación:**
- [ ] Common function in string.cpp
- [ ] Both print and serial use common function
- [ ] No code duplication

**Estimado:** 6 horas

---

#### Tarea H-006: Page Table Verification

**Descripción:** Agregar verificación post-setup de page tables

**Acciones:**
1. [ ] Agregar función `verify_page_tables` en assembly
2. [ ] Leer back entries después de escribir
3. [ ] Reportar error si verificación falla

**Archivos:**
- `src/impl/x86_64/boot/main.asm`

**Código Propuesto:**
```asm
setup_page_tables:
    ; ... existing setup code ...
    
    call verify_page_tables
    test al, al
    jz .page_table_error

; ... later ...

verify_page_tables:
    ; Verify L4[0] points to L3 with correct flags
    mov eax, [page_table_l4]
    and eax, 0xFFF
    cmp eax, page_table_l3
    jne .verify_fail
    
    ; Verify L3[0] and L3[1] point to L2 tables
    mov eax, [page_table_l3]
    and eax, 0xFFF
    cmp eax, page_table_l2_0
    jne .verify_fail
    
    mov eax, [page_table_l3 + 8]
    and eax, 0xFFF
    cmp eax, page_table_l2_1
    jne .verify_fail
    
    mov al, 1  ; Success
    ret
    
.verify_fail:
    xor al, al  ; Failure
    ret
    
.page_table_error:
    mov dword [0xb8000], 0x4f524f45  ; "ERR: PT"
    hlt
```

**Criterios de Aceptación:**
- [ ] verify_page_tables function added
- [ ] Called after setup_page_tables
- [ ] Error handling on failure

**Estimado:** 8 horas

---

#### Tarea H-007: Panic VGA Check

**Descripción:** Verificar estado de VGA en panic()

**Acciones:**
1. [ ] Agregar flag `vga_initialized`
2. [ ] Setear flag después de `print_clear()` exitoso
3. [ ] Verificar flag en `panic()`

**Archivos:**
- `src/impl/x86_64/print.cpp`
- `src/impl/x86_64/panic.cpp`

**Criterios de Aceptación:**
- [ ] vga_initialized flag added
- [ ] Flag set after print_clear
- [ ] panic checks flag before VGA operations

**Estimado:** 4 horas

---

#### Tarea O2-1: Complete Parameter Validation

**Descripción:** Verificar 100% de funciones públicas con validación

**Acciones:**
1. [ ] Audit todas las funciones públicas
2. [ ] Agregar validación donde falte
3. [ ] Documentar precondiciones

**Criterios de Aceptación:**
- [ ] All public functions audited
- [ ] Null checks where applicable
- [ ] Range checks where applicable
- [ ] Documentation updated

**Estimado:** 8 horas

---

### 5.3 Fase 3: Medios (Semanas 7-12)

#### Tarea M-001: Null Validation Style

**Descripción:** Estandarizar estilo de validación null

**Acciones:**
1. [ ] Definir estándar en CODING_STANDARDS.md
2. [ ] Refactorizar todas las validaciones al estándar
3. [ ] Configurar linter para verificar

**Estándar Propuesto:**
```cpp
// Preferred C++ style with braces
if (ptr == nullptr) {
    return;  // or return error code
}

// For functions returning error code
if (ptr == nullptr) {
    return ERROR_NULL_POINTER;
}
```

**Criterios de Aceptación:**
- [ ] Standard documented
- [ ] All files conform
- [ ] Linter configured

**Estimado:** 6 horas

---

#### Tarea M-004: Thread Safety Documentation

**Descripción:** Documentar thread safety en funciones públicas

**Acciones:**
1. [ ] Definir categorías de thread safety
2. [ ] Agregar tags en documentación
3. [ ] Crear guía de uso para SMP futuro

**Tags Propuestos:**
```cpp
/**
 * @threadsafe Yes - Safe for concurrent access
 * @threadsafe No - Requires external locking
 * @threadsafe Read-only - Safe for concurrent reads
 */
```

**Criterios de Aceptación:**
- [ ] All public functions tagged
- [ ] SMP guide created
- [ ] Non-safe functions identified

**Estimado:** 8 horas

---

#### Tarea M-008: Memset Optimization

**Descripción:** Optimizar memset con writes de 64-bit

**Acciones:**
1. [ ] Implementar versión optimizada
2. [ ] Mantener fallback byte por byte
3. [ ] Benchmark si es posible

**Código Propuesto:**
```cpp
void* memset(void* s, int c, size_t n) {
    uint8_t* p = static_cast<uint8_t*>(s);
    
    // Expand byte to 64-bit pattern
    uint64_t pattern = static_cast<uint64_t>(static_cast<uint8_t>(c));
    pattern = pattern * 0x0101010101010101ULL;
    
    // Align to 8-byte boundary
    while (n && (reinterpret_cast<uintptr_t>(p) & 7)) {
        *p++ = static_cast<uint8_t>(c);
        n--;
    }
    
    // 64-bit writes
    while (n >= 8) {
        *reinterpret_cast<uint64_t*>(p) = pattern;
        p += 8;
        n -= 8;
    }
    
    // Remaining bytes
    while (n--) {
        *p++ = static_cast<uint8_t>(c);
    }
    
    return s;
}
```

**Criterios de Aceptación:**
- [ ] Optimized implementation
- [ ] Alignment handling
- [ ] Fallback for remaining bytes

**Estimado:** 6 horas

---

#### Tarea M-010: Multiboot2 Magic Number

**Descripción:** Verificar y corregir magic number de Multiboot2

**Acciones:**
1. [ ] Verificar spec de Multiboot2
2. [ ] Corregir si hay inconsistencia
3. [ ] Agregar constante nombrada

**Código Propuesto:**
```asm
; constants equ
MULTIBOOT2_MAGIC equ 0xE85250D6

check_multiboot:
    cmp eax, MULTIBOOT2_MAGIC
    jne .no_multiboot
```

**Criterios de Aceptación:**
- [ ] Magic number verified against spec
- [ ] Named constant used
- [ ] Consistency between header and check

**Estimado:** 3 horas

---

#### Tarea M-015: BSS Bounds Validation

**Descripción:** Validar que `__bss_end > __bss_start`

**Acciones:**
1. [ ] Agregar validación en assembly
2. [ ] Agregar ASSERT en linker script
3. [ ] Documentar en linker.ld

**Código Propuesto:**
```asm
; main64.asm
zero_bss:
    lea rdi, [__bss_start]
    lea rcx, [__bss_end]
    
    ; Validate bounds
    cmp rcx, rdi
    jb .bss_error  ; Error if __bss_end < __bss_start
    je .bss_done   ; Empty BSS
    
    sub rcx, rdi
    shr rcx, 3
    xor rax, rax
    rep stosq
    ret
    
.bss_error:
    mov dword [0xb8000], 0x4f524f45  ; "ERR: BS"
    hlt
```

**Criterios de Aceptación:**
- [ ] Bounds validation added
- [ ] Error handling on invalid bounds
- [ ] Linker ASSERT added

**Estimado:** 4 horas

---

#### Tarea O6-1: Complete Thread Safety Docs

**Descripción:** Documentar 100% de funciones públicas

**Acciones:**
1. [ ] Audit all public headers
2. [ ] Add @threadsafe tags
3. [ ] Verify completeness

**Criterios de Aceptación:**
- [ ] All intf/*.h files tagged
- [ ] Consistent documentation style
- [ ] Review approved

**Estimado:** 8 horas

---

### 5.4 Fase 4: Infraestructura (Semanas 13-20)

#### Tarea L-002: .gitattributes

**Descripción:** Configurar normalización de line endings

**Acciones:**
1. [ ] Crear `.gitattributes`
2. [ ] Configurar para todos los tipos de archivo
3. [ ] Normalizar repositorio existente

**Archivo Propuesto:**
```gitattributes
# Source code
*.cpp text eol=lf
*.h text eol=lf
*.hpp text eol=lf

# Assembly
*.asm text eol=lf
*.s text eol=lf

# Linker scripts
*.ld text eol=lf

# Documentation
*.md text eol=lf
*.txt text eol=lf

# Build files
Makefile text eol=lf
*.mk text eol=lf

# Binary files (no normalization)
*.bin binary
*.iso binary
*.o binary
```

**Criterios de Aceptación:**
- [ ] File created
- [ ] All file types covered
- [ ] Repository normalized

**Estimado:** 2 horas

---

#### Tarea L-003: .clang-format

**Descripción:** Configurar formateo automático

**Acciones:**
1. [ ] Crear `.clang-format`
2. [ ] Configurar según estilo del proyecto
3. [ ] Agregar script de formateo

**Archivo Propuesto:**
```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 100
AlignAfterOpenBracket: Align
AllowShortFunctionsOnASingleLine: None
BreakBeforeBraces: Attach
Cpp11BracedListStyle: true
DerivePointerAlignment: false
PointerAlignment: Left
SpaceAfterCStyleCast: true
SpacesBeforeTrailingComments: 2
TabWidth: 4
UseTab: Never
```

**Criterios de Aceptación:**
- [ ] File created
- [ ] Style matches project conventions
- [ ] Format script added to Makefile

**Estimado:** 3 horas

---

#### Tarea L-006: QEMU Test Target

**Descripción:** Agregar target para tests automatizados

**Acciones:**
1. [ ] Crear script de test en QEMU
2. [ ] Capturar output serial
3. [ ] Verificar resultados esperados

**Makefile Target Propuesto:**
```makefile
.PHONY: test
test: build-x86_64
	@echo "Running automated tests..."
	@timeout 30 $(QEMU) $(QEMU_STDIO_FLAGS) > test_output.log 2>&1 || true
	@./scripts/verify_tests.sh test_output.log
	@echo "Tests completed"

.PHONY: test-verbose
test-verbose: build-x86_64
	@echo "Running tests with verbose output..."
	@timeout 30 $(QEMU) $(QEMU_STDIO_FLAGS) | tee test_output.log
	@./scripts/verify_tests.sh test_output.log
```

**Criterios de Aceptación:**
- [ ] Test target in Makefile
- [ ] Serial output captured
- [ ] Verification script works

**Estimado:** 8 horas

---

#### Tarea O5-1: Unit Tests Implementation

**Descripción:** Implementar tests unitarios para funciones core

**Acciones:**
1. [ ] Crear framework de test simple
2. [ ] Tests para string functions
3. [ ] Tests para print functions
4. [ ] Tests para serial functions

**Estructura Propuesta:**
```
tests/
├── test_string.cpp
├── test_print.cpp
├── test_serial.cpp
└── test_runner.cpp
```

**Criterios de Aceptación:**
- [ ] Test framework created
- [ ] 80% core functions tested
- [ ] Tests run in CI

**Estimado:** 24 horas

---

#### Tarea O8-1: CI Implementation

**Descripción:** Implementar integración continua

**Acciones:**
1. [ ] Configurar GitHub Actions
2. [ ] Build automation
3. [ ] Test automation
4. [ ] Artifact generation

**Workflow Propuesto:**
```yaml
name: GLOBEX_OS CI

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y g++ nasm grub-pc-bin xorriso qemu-system-x86
      
      - name: Verify tools
        run: make verify-tools
      
      - name: Build kernel
        run: make build-x86_64
      
      - name: Run tests
        run: make test
      
      - name: Upload ISO artifact
        uses: actions/upload-artifact@v4
        with:
          name: kernel-iso
          path: dist/x86_64/kernel.iso
```

**Criterios de Aceptación:**
- [ ] Workflow file created
- [ ] Build passes on CI
- [ ] Tests run on CI
- [ ] Artifacts uploaded

**Estimado:** 12 horas

---

#### Tarea O9-1: QEMU Automated Tests

**Descripción:** Implementar 50+ test cases en QEMU

**Acciones:**
1. [ ] Crear framework de test en QEMU
2. [ ] Tests para cada función pública
3. [ ] Tests de integración
4. [ ] Tests de regresión

**Criterios de Aceptación:**
- [ ] 50+ test cases
- [ ] Automated execution
- [ ] Pass/fail reporting

**Estimado:** 40 horas

---

#### Tarea O10-1: Doxygen Documentation

**Descripción:** Documentación API completa con Doxygen

**Acciones:**
1. [ ] Configurar Doxygen
2. [ ] Actualizar comentarios en código
3. [ ] Generar documentación HTML
4. [ ] Publicar en GitHub Pages

**Criterios de Aceptación:**
- [ ] Doxygen configured
- [ ] 100% API documented
- [ ] HTML docs generated
- [ ] Published online

**Estimado:** 20 horas

---

#### Tarea O11-1: Structured Logging

**Descripción:** Implementar sistema de logging estructurado

**Acciones:**
1. [ ] Diseñar formato de log
2. [ ] Implementar logging levels
3. [ ] Agregar timestamps
4. [ ] Soporte para múltiples outputs

**Criterios de Aceptación:**
- [ ] Logging levels (DEBUG, INFO, WARN, ERROR)
- [ ] Structured format
- [ ] Multiple outputs (serial, VGA, file)

**Estimado:** 24 horas

---

#### Tarea O12-1: Static Analysis

**Descripción:** Integrar análisis estático en CI

**Acciones:**
1. [ ] Configurar clang-tidy
2. [ ] Configurar cppcheck
3. [ ] Integrar en CI workflow
4. [ ] Fix existing issues

**Criterios de Aceptación:**
- [ ] Tools configured
- [ ] CI integration
- [ ] Zero warnings

**Estimado:** 16 horas

---

## 6. Infraestructura de Calidad

### 6.1 Herramientas Requeridas

| Herramienta | Propósito | Prioridad |
|-------------|-----------|-----------|
| clang-tidy | Análisis estático C++ | Alta |
| cppcheck | Análisis estático C++ | Alta |
| clang-format | Formateo automático | Alta |
| Doxygen | Generación docs | Media |
| gcov/lcov | Coverage reporting | Media |
| QEMU | Emulación para tests | Alta |

### 6.2 Configuración de Archivos

#### `.clang-format`
```yaml
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 100
AlignAfterOpenBracket: Align
AllowShortFunctionsOnASingleLine: None
BreakBeforeBraces: Attach
Cpp11BracedListStyle: true
DerivePointerAlignment: false
PointerAlignment: Left
SpaceAfterCStyleCast: true
SpacesBeforeTrailingComments: 2
TabWidth: 4
UseTab: Never
```

#### `.editorconfig`
```ini
root = true

[*]
indent_style = space
indent_size = 4
end_of_line = lf
charset = utf-8
trim_trailing_whitespace = true
insert_final_newline = true

[*.asm]
indent_style = tab
tab_width = 8

[*.md]
trim_trailing_whitespace = false

[Makefile]
indent_style = tab
```

#### `.gitattributes`
```gitattributes
*.cpp text eol=lf
*.h text eol=lf
*.asm text eol=lf
*.ld text eol=lf
*.md text eol=lf
*.bin binary
*.iso binary
```

### 6.3 Scripts de Calidad

#### `scripts/format.sh`
```bash
#!/bin/bash
# Format all C++ files according to .clang-format

find src/ -name "*.cpp" -o -name "*.h" | xargs clang-format -i
echo "Formatting complete"
```

#### `scripts/analyze.sh`
```bash
#!/bin/bash
# Run static analysis

echo "Running clang-tidy..."
clang-tidy src/**/*.cpp -- -I src/intf

echo "Running cppcheck..."
cppcheck --enable=all --std=c++17 src/

echo "Analysis complete"
```

#### `scripts/verify_tests.sh`
```bash
#!/bin/bash
# Verify test output

OUTPUT_FILE=$1

# Check for expected test markers
if grep -q "\[BSS TEST\] PASSED" "$OUTPUT_FILE" && \
   grep -q "\[PRINT FUNCTIONS\] All tests passed" "$OUTPUT_FILE" && \
   grep -q "\[SERIAL SIGNED\] All tests passed" "$OUTPUT_FILE"; then
    echo "All tests PASSED"
    exit 0
else
    echo "Some tests FAILED"
    exit 1
fi
```

---

## 7. Métricas y Seguimiento

### 7.1 Dashboard de Calidad

| Métrica | Actual | Target M1 | Target M3 | Target M6 |
|---------|--------|-----------|-----------|-----------|
| Critical Issues | 3 | 0 | 0 | 0 |
| High Issues | 7 | 7 | 0 | 0 |
| Medium Issues | 12 | 12 | 6 | 0 |
| Low Issues | 15 | 15 | 10 | 0 |
| Test Coverage | 0% | 0% | 40% | 80% |
| CI Pass Rate | N/A | 100% | 100% | 100% |
| Documentation | 25% | 30% | 60% | 100% |
| Quality Score | 6.4 | 7.0 | 8.0 | 9.0 |

### 7.2 Reportes Semanales

**Template de Reporte:**
```markdown
## Semana [N] - [Fecha]

### Completado
- [ ] Tarea 1
- [ ] Tarea 2

### En Progreso
- [ ] Tarea 3 (50%)

### Bloqueantes
- [ ] Issue description

### Métricas
- Critical: X → Y
- High: X → Y
- Test Coverage: X% → Y%
```

### 7.3 Revisiones de Calidad

| Tipo | Frecuencia | Participantes |
|------|------------|---------------|
| Daily Standup | Diario | Dev team |
| Sprint Review | Quincenal | Dev team + Stakeholders |
| Quality Review | Mensual | QA lead + Dev lead |
| Executive Review | Trimestral | Management |

---

## 8. Gestión de Riesgos

### 8.1 Matriz de Riesgos

| Riesgo | Probabilidad | Impacto | Mitigación |
|--------|--------------|---------|------------|
| R1: Scope creep | Media | Alto | Priorización estricta |
| R2: Dependencias externas | Baja | Medio | Documentar alternativas |
| R3: Cambios en requisitos | Media | Alto | Sprint reviews frecuentes |
| R4: Recursos insuficientes | Media | Alto | Ajustar scope si necesario |
| R5: Deuda técnica acumulada | Alta | Medio | Refactoring continuo |

### 8.2 Planes de Contingencia

#### R1: Scope Creep
- **Trigger:** Nuevas features solicitadas durante fase de calidad
- **Acción:** Mover a backlog, priorizar en siguiente sprint

#### R4: Recursos Insuficientes
- **Trigger:** Tareas toman >150% del estimado
- **Acción:** Reducir scope de fase 4, mantener fases 1-3

---

## 9. Anexos

### 9.1 Checklist de Code Review

```markdown
## Code Review Checklist

### Security
- [ ] Input validation present
- [ ] No buffer overflows
- [ ] No use-after-free
- [ ] Null checks where needed

### Correctness
- [ ] Logic matches requirements
- [ ] Edge cases handled
- [ ] Error handling present
- [ ] No undefined behavior

### Performance
- [ ] No unnecessary allocations
- [ ] Memory barriers where needed
- [ ] Volatile for MMIO

### Maintainability
- [ ] Code follows standards
- [ ] Comments explain why
- [ ] Function names descriptive
- [ ] No code duplication

### Testing
- [ ] Tests added/updated
- [ ] Tests pass locally
- [ ] Coverage maintained
```

### 9.2 Definition of Done

```markdown
## Definition of Done

A task is considered done when:
1. Code is implemented and compiles
2. All tests pass
3. Code is formatted (clang-format)
4. Static analysis passes (clang-tidy)
5. Documentation updated
6. Code review approved
7. Merged to main branch
```

### 9.3 Referencias

- [Multiboot2 Specification](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html)
- [OSDev Wiki](https://wiki.osdev.org/)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- [SEI CERT C++ Coding Standard](https://wiki.sei.cmu.edu/confluence/display/cplusplus)

---

**Documento aprobado por:** ___________________  
**Fecha de aprobación:** ___________________  
**Próxima revisión:** 2026-06-08
