# 📋 GLOBEX_OS - Plan de Acción de Calidad

**Fecha de Creación:** 2026-03-07  
**Fecha de Actualización:** 2026-03-07  
**Versión Actual:** v0.012_x64  
**Versión Objetivo:** v0.015_x64  
**Horizonte Temporal:** 3 meses

---

## 🗓️ Roadmap General

```
Semana 1-2:   ████████████████████████████████████████  Fase 1 - Crítico ✅ COMPLETADA
Semana 3-4:   ████████████████████████████████████████  Fase 2 - Alto ✅ COMPLETADA
Semana 5-8:   ████████████████████░░░░░░░░░░░░░░░░░░░░  Fase 3 - Medio ⏳ EN PROGRESO
Semana 9-10:  ░░░░░░░░░░░░░░░░░░░░████████████████████  Fase 4 - Bajo ⏳ PENDIENTE
Semana 11-12: ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░████████  Fase 5 - Testing ⏳ PENDIENTE
```

---

## 📁 FASE 1: Correcciones Críticas (Semana 1-2) ✅ COMPLETADA

**Estado:** ✅ **COMPLETADA**  
**Versión:** v0.010_x64  
**Commit:** `86d4a59`

**Objetivo:** Eliminar riesgos de corrupción de datos y hangs del sistema

### 1.1 Timeout en Serial [SC001] ✅
- [x] Definir constante `SERIAL_MAX_WAIT`
- [x] Implementar `serial_wait_transmit_empty_timeout()`
- [x] Actualizar `serial_write_char()` con timeout
- [x] Manejo de error por timeout

### 1.2 Race Condition en Cursor VGA [PC001] ✅
- [x] Hacer `cursor_col` y `cursor_row` volatile
- [x] Agregar `memory_barrier()` macro
- [x] Documentar limitaciones SMP futuras

### 1.3 Variables de Estado Serial Volatile [SC002] ✅
- [x] Cambiar `serial_initialized` a `volatile int`
- [x] Cambiar `serial_port` a `volatile uint16_t`

### 1.4 Validación de Puertos Serial [SC003] ✅
- [x] Implementar `serial_port_exists()`
- [x] Verificar puerto en `serial_init()`

### 📦 Entregables Fase 1 ✅
- [x] Todas las funciones serial con timeout
- [x] Cursor VGA con volatile + barriers
- [x] Variables de estado serial volatile
- [x] Validación de existencia de puerto serial
- [x] Commit: `feat(phase1): Critical stability fixes for v0.010`

---

## 📁 FASE 2: Correcciones de Alta Prioridad (Semana 3-4) ✅ COMPLETADA

**Estado:** ✅ **COMPLETADA**  
**Versión:** v0.012_x64  
**Commits:** `951d52d`, `30e387a`, `b4b101b`, `c6ba0c3`

**Objetivo:** Mejorar robustez y capacidades del kernel

### 2.1 BSS Initialization ✅
- [x] Crear sección `.boot.data` para page tables
- [x] Separar `.boot.data` de `.bss` en linker.ld
- [x] Habilitar `zero_bss` en main64.asm
- [x] Testear con variable BSS sin inicializar

**Resultado:** `[BSS TEST] PASSED: BSS initialized to zero`

### 2.2 Memory Mapping (2GiB) ✅
- [x] Implementar mapeo de 2GiB con 2 tablas L2
- [x] Agregar símbolos de memoria en linker.ld
- [x] Testear acceso a memoria > 64MB

**Resultado:** `[MEM TEST] PASSED: Memory access at 80MB works`

### 2.3 Error Handling / Panic ✅
- [x] Crear `panic()` function (panic.h, panic.cpp)
- [x] Agregar `panic_simple()` y macro `PANIC_IF_FALSE()`
- [x] Error handling en kernel_main
- [x] Output VGA + serial en panic

### 2.4 Makefile Improvements ✅
- [x] Agregar variable QEMU
- [x] Target `clean` - Remove build artifacts
- [x] Target `distclean` - Remove all generated files
- [x] Target `rebuild` - Clean and rebuild
- [x] Target `verify-tools` - Check required tools
- [x] Target `help` - Show available targets

### 📦 Entregables Fase 2 ✅
- [x] BSS initialization habilitado
- [x] Memory mapping aumentado a 2GiB
- [x] Panic driver implementado
- [x] Makefile mejorado

---

## 📁 FASE 3: Mejoras de Prioridad Media (Semana 5-8) ⏳ EN PROGRESO

**Estado:** ⏳ **EN PROGRESO**  
**Versión Objetivo:** v0.013_x64

### 3.1 Validación de Colores [P001]
- [ ] Agregar validación de rango (0-15) en `print_set_color()`
- [ ] Función helper `is_valid_color()`

### 3.2 Optimizar `clear_row()` [PC003]
- [ ] Escribir ambos bytes juntos (character + color)
- [ ] Benchmark antes/después

### 3.3 Optimizar Scroll con memmove [PC004]
- [ ] Implementar `memmove()` para kernel
- [ ] Refactorizar `print_newline()` para usar memmove

### 3.4 Debug Macros Mejoradas [D001, D002, D003]
- [ ] Agregar `DEBUG_PRINTF()` con formato
- [ ] Agregar `DEBUG_PRINTLN()` con newline automático
- [ ] Agregar `DEBUG_ASSERT()`

### 3.5 Funciones de Impresión de Números
- [ ] `print_dec(uint32_t)` 
- [ ] `print_dec64(uint64_t)`
- [ ] `print_hex(uint32_t)` - ✅ YA IMPLEMENTADA
- [ ] `print_hex64(uint64_t)`

### 3.6 Funciones de Consulta en print
- [ ] `print_get_cursor(size_t* col, size_t* row)`
- [ ] `print_get_color(uint8_t* fg, uint8_t* bg)`
- [ ] `print_set_cursor(size_t col, size_t row)`

### 3.7 Serial Signed Numbers [S002]
- [ ] `serial_write_dec_signed(int32_t)`
- [ ] `serial_write_dec64_signed(int64_t)`

### 3.8 Información de Hardware en Boot [MA006]
- [ ] Guardar resultado de CPUID
- [ ] Guardar detección de long mode
- [ ] Imprimir información en kernel_main

### 📦 Entregables Fase 3
- [ ] Validación de colores
- [ ] clear_row optimizado
- [ ] Scroll con memmove
- [ ] Debug macros completas
- [ ] Funciones de impresión de números
- [ ] Funciones de consulta de cursor/color
- [ ] Serial con signed numbers
- [ ] Información de hardware en boot
- [ ] Commit: `feat(phase3): Feature enhancements for v0.013`

---

## 📁 FASE 4: Mejoras Menores y Enhancements (Semana 9-10) ⏳ PENDIENTE

**Estado:** ⏳ **PENDIENTE**  
**Versión Objetivo:** v0.014_x64

### 4.1 Constantes para Magic Numbers [PC005, H002]
- [ ] Definir `VGA_TEST_CHAR` y `VGA_TEST_COLOR`
- [ ] Definir constantes Multiboot2
- [ ] Crear archivo `constants.h`

### 4.2 GRUB.cfg Nombre Correcto [GRUB001]
- [ ] Cambiar "NW OS 0.001" a "GLOBEX_OS v0.012_x64"

### 4.3 Comentarios en Inglés [MNT001]
- [ ] Traducir comentarios de API pública al inglés
- [ ] Mantener comentarios internos en español (opcional)

### 4.4 Mejoras en Error Display [MA005]
- [ ] Mostrar más contexto en error handler
- [ ] Mostrar código de error en hexadecimal
- [ ] Considerar beep speaker

### 4.5 Documentación de Límites [DOC001]
- [ ] Documentar límite de memoria mapeada (2GiB)
- [ ] Documentar límite de stack (64KB)
- [ ] Documentar puertos serial soportados

### 📦 Entregables Fase 4
- [ ] Magic numbers reemplazados
- [ ] GRUB cfg actualizado
- [ ] Política de comentarios definida
- [ ] Error display mejorado
- [ ] Documentación de límites
- [ ] Commit: `chore: Polish and documentation for v0.014`

---

## 📁 FASE 5: Testing y CI/CD (Semana 11-12) ⏳ PENDIENTE

**Estado:** ⏳ **PENDIENTE**  
**Versión Objetivo:** v0.015_x64

### 5.1 Tests Unitarios para Serial [TEST001]
- [ ] Crear framework de tests simple
- [ ] Test `serial_init_default()`
- [ ] Test `serial_write_char()`
- [ ] Test `serial_write_hex()`
- [ ] Test timeout functionality

### 5.2 Tests Unitarios para Print [TEST002]
- [ ] Test `print_detect()`
- [ ] Test `print_char()` con caracteres especiales
- [ ] Test `print_str()` con NULL
- [ ] Test `print_set_color()` con valores límite
- [ ] Test scroll

### 5.3 Tests de Integración [TEST003]
- [ ] Test boot completo
- [ ] Test serial + VGA simultáneos
- [ ] Test idle loop por N segundos

### 5.4 CI/CD con GitHub Actions [CI001]
- [ ] Crear workflow de build
- [ ] Configurar Docker en CI
- [ ] Agregar test de compilación
- [ ] Agregar test de QEMU (headless)
- [ ] Configurar artifacts (ISO)

### 5.5 Script de Auto-Test [TEST004]
- [ ] Crear script `run_tests.sh`
- [ ] Capturar output serial
- [ ] Verificar mensajes esperados

### 📦 Entregables Fase 5
- [ ] Framework de tests implementado
- [ ] Tests unitarios para serial y print
- [ ] Tests de integración
- [ ] GitHub Actions workflow
- [ ] Script de auto-test
- [ ] Commit: `test: Add testing infrastructure for v0.015`

---

## 📊 CRONOGRAMA RESUMEN

| Fase | Semanas | Hitos Principales | Versión | Estado |
|------|---------|-------------------|---------|--------|
| **Fase 1** | 1-2 | Timeouts, Volatile, Validation | v0.010 | ✅ COMPLETADA |
| **Fase 2** | 3-4 | BSS Init, Memory Map, Error Handling | v0.012 | ✅ COMPLETADA |
| **Fase 3** | 5-8 | Optimizaciones, Features, Debug | v0.013 | ⏳ EN PROGRESO |
| **Fase 4** | 9-10 | Polish, Documentation | v0.014 | ⏳ PENDIENTE |
| **Fase 5** | 11-12 | Testing, CI/CD | v0.015 | ⏳ PENDIENTE |

---

## 🎯 KRITERIOS DE ÉXITO

### Al final de Fase 1 ✅
- [x] Kernel no se hanga por timeout serial
- [x] No hay race conditions obvias
- [x] Variables de hardware son volatile

### Al final de Fase 2 ✅
- [x] BSS se inicializa correctamente
- [x] Más de 1GiB mapeado (2GiB implementado)
- [x] Errores se manejan apropiadamente

### Al final de Fase 3
- [ ] Debug macros completas
- [ ] Todas las funciones de print/serial implementadas
- [ ] Información de hardware disponible

### Al final de Fase 4
- [ ] Código sin magic numbers
- [ ] Documentación completa

### Al final de Fase 5
- [ ] >80% code coverage
- [ ] CI/CD funcionando
- [ ] Tests automáticos en cada commit

---

## 📈 MÉTRICAS DE PROGRESO

| Métrica | Original | Fase 1 | Fase 2 | Fase 3 | Fase 4 | Fase 5 |
|---------|----------|--------|--------|--------|--------|--------|
| Issues Críticos | 2 | 0 | 0 | 0 | 0 | 0 |
| Issues Altos | 4 | 0 | 0 | 0 | 0 | 0 |
| Code Coverage | 0% | 0% | 0% | 20% | 40% | >80% |
| Tests Automatizados | 0 | 0 | 0 | 5 | 10 | 20+ |
| CI/CD | ❌ | ❌ | ❌ | ❌ | ⚠️ | ✅ |
| Versión | v0.009 | v0.010 | v0.012 | v0.013 | v0.014 | v0.015 |

---

## 🔗 RECURSOS NECESARIOS

### Herramientas
- Docker con build environment
- QEMU 6.x+
- NASM 2.15+
- GCC/G++ con soporte freestanding
- GRUB 2.x

### Conocimiento
- x86_64 assembly
- C++ freestanding
- UART 16550 programming
- VGA text mode
- Multiboot2 specification
- Linker scripts

---

## 📝 HISTORIAL DE CAMBIOS

| Fecha | Versión | Cambios |
|-------|---------|---------|
| 2026-03-07 | v0.012 | Fase 1 y Fase 2 completadas |
| 2026-03-07 | v0.009 | Plan inicial creado |

---

*Documento vivo - Actualizar después de cada fase completada*
