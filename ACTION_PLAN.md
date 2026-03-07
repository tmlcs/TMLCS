# 📋 GLOBEX_OS - Plan de Acción de Calidad

**Fecha de Creación:** 2026-03-07  
**Versión Objetivo:** v0.010_x64 → v0.015_x64  
**Horizonte Temporal:** 3 meses

---

## 🗓️ Roadmap General

```
Semana 1-2:   ████████████████████░░░░░░░░░░░░░░░░░░░░  Fase 1 - Crítico
Semana 3-4:   ████████████████████████████████░░░░░░░░  Fase 2 - Alto
Semana 5-8:   ████████████████████████████████████████  Fase 3 - Medio
Semana 9-10:  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░████████████  Fase 4 - Bajo
Semana 11-12: ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░██████  Fase 5 - Testing
```

---

## 📁 FASE 1: Correcciones Críticas (Semana 1-2)

**Objetivo:** Eliminar riesgos de corrupción de datos y hangs del sistema

### 1.1 Timeout en Serial [SC001]
**Archivo:** `src/impl/x86_64/serial.cpp`  
**Esfuerzo:** 2 horas  
**Dependencias:** Ninguna

#### Tareas
- [ ] 1.1.1 Definir constante `SERIAL_TIMEOUT_MS` y `SERIAL_MAX_WAIT`
- [ ] 1.1.2 Modificar `serial_wait_transmit_empty()` para aceptar timeout
- [ ] 1.1.3 Crear `serial_wait_transmit_empty_timeout(uint32_t timeout_ms)`
- [ ] 1.1.4 Actualizar `serial_write_char()` para usar timeout
- [ ] 1.1.5 Agregar manejo de error cuando timeout ocurre
- [ ] 1.1.6 Testear con QEMU desconectando serial

#### Criterios de Aceptación
```cpp
// Debe compilar sin warnings
// Debe retornar false después de timeout
// Debe set serial_initialized = 0 en timeout
```

#### Código Esperado
```cpp
#define SERIAL_MAX_WAIT 100000

bool serial_wait_transmit_empty_timeout(uint32_t timeout) {
    while (timeout-- > 0) {
        if (inb(serial_port + SERIAL_LSR) & SERIAL_LSR_THRE) {
            return true;
        }
        for (volatile int i = 0; i < 100; i++) {
            __asm__ volatile ("nop");
        }
    }
    return false;  // Timeout
}
```

---

### 1.2 Race Condition en Cursor VGA [PC001]
**Archivo:** `src/impl/x86_64/print.cpp`  
**Esfuerzo:** 3 horas  
**Dependencias:** 1.1

#### Tareas
- [ ] 1.2.1 Investigar soporte de `std::atomic` en freestanding
- [ ] 1.2.2 Si no hay atomic, usar `volatile` + memory barriers
- [ ] 1.2.3 Hacer `cursor_col` y `cursor_row` volatile
- [ ] 1.2.4 Agregar memory barriers en lecturas/escrituras
- [ ] 1.2.5 Documentar limitaciones SMP futuras
- [ ] 1.2.6 Testear con múltiples prints consecutivos

#### Criterios de Aceptación
```cpp
// Variables deben ser volatile
// Memory barriers deben prevenir reordering
// Tests de estrés no deben mostrar corrupción
```

#### Código Esperado
```cpp
// Agregar al inicio de print.cpp
#define memory_barrier() __asm__ volatile ("" ::: "memory")

static volatile size_t cursor_col = 0;
static volatile size_t cursor_row = 0;

// En cada acceso:
void print_char(char character) {
    memory_barrier();
    size_t col = cursor_col;  // Lectura volatile
    size_t row = cursor_row;
    // ...
    cursor_col = col + 1;  // Escritura volatile
    memory_barrier();
}
```

---

### 1.3 Variables de Estado Serial Volatile [SC002]
**Archivo:** `src/impl/x86_64/serial.cpp`  
**Esfuerzo:** 1 hora  
**Dependencias:** Ninguna

#### Tareas
- [ ] 1.3.1 Cambiar `serial_initialized` a `volatile int`
- [ ] 1.3.2 Cambiar `serial_port` a `volatile uint16_t`
- [ ] 1.3.3 Verificar todos los accesos usan las variables volatile
- [ ] 1.3.4 Testear inicialización múltiple

#### Código Esperado
```cpp
static volatile int serial_initialized = 0;
static volatile uint16_t serial_port = 0;
```

---

### 1.4 Validación de Puertos Serial [SC003]
**Archivo:** `src/impl/x86_64/serial.cpp`  
**Esfuerzo:** 2 horas  
**Dependencias:** 1.3

#### Tareas
- [ ] 1.4.1 Implementar `serial_port_exists(uint16_t port)`
- [ ] 1.4.2 Leer registro IIR para verificar UART
- [ ] 1.4.3 Agregar check en `serial_init()` antes de configurar
- [ ] 1.4.4 Retornar error si puerto no existe
- [ ] 1.4.5 Testear con puertos inexistentes

#### Código Esperado
```cpp
static bool serial_port_exists(uint16_t port) {
    // UART 16550+ tiene bits 6-7 del IIR en 0xC0
    uint8_t iir = inb(port + SERIAL_IIR);
    return (iir & 0xC0) == 0xC0;
}

int serial_init(uint16_t port, uint32_t baud) {
    if (!serial_port_exists(port)) {
        return 0;  // Puerto no existe
    }
    // ... resto de init
}
```

---

### 📦 Entregables Fase 1
- [ ] Todas las funciones serial con timeout
- [ ] Cursor VGA con volatile + barriers
- [ ] Variables de estado serial volatile
- [ ] Validación de existencia de puertos
- [ ] Tests manuales documentados
- [ ] Commit: `fix: Critical stability fixes for v0.010`

---

## 📁 FASE 2: Correcciones de Alta Prioridad (Semana 3-4)

**Objetivo:** Mejorar robustez y capacidades del kernel

### 2.1 Memory Barriers en VGA [PC002]
**Archivo:** `src/impl/x86_64/print.cpp`  
**Esfuerzo:** 2 horas  
**Dependencias:** 1.2

#### Tareas
- [ ] 2.1.1 Agregar memory barrier después de cada write a VGA
- [ ] 2.1.2 Crear macro `VGA_WRITE(row, col, char, color)`
- [ ] 2.1.3 Refactorizar `print_char()` para usar macro
- [ ] 2.1.4 Refactorizar `clear_row()` para usar macro
- [ ] 2.1.5 Documentar por qué son necesarios

---

### 2.2 BSS Initialization [M6001]
**Archivos:** `main64.asm`, `linker.ld`, `main.asm`  
**Esfuerzo:** 4 horas  
**Dependencias:** 2.3

#### Tareas
- [ ] 2.2.1 Mover page tables a sección `.boot.data` en linker.ld
- [ ] 2.2.2 Mover stack a sección `.boot.stack` en linker.ld
- [ ] 2.2.3 Habilitar `zero_bss` en main64.asm
- [ ] 2.2.4 Verificar símbolos `__bss_start` y `__bss_end`
- [ ] 2.2.5 Testear con variables BSS sin inicializar
- [ ] 2.2.6 Verificar que page tables no se borran

#### Código Esperado linker.ld
```ld
.boot.data : ALIGN(4K)
{
    __boot_data_start = .;
    *(.boot.data)
    __boot_data_end = .;
} :data

.boot.stack : ALIGN(4K)
{
    __boot_stack_start = .;
    *(.boot.stack)
    __boot_stack_end = .;
} :data

.bss : ALIGN(4K)
{
    __bss_start = .;
    *(.bss)
    *(.bss.*)
    *(COMMON)
    __bss_end = .;
} :data
```

#### Código Esperado main.asm
```asm
section .boot.data nobits
align 4096
page_table_l4:
    resb 4096
page_table_l3:
    resb 4096
page_table_l2:
    resb 4096

section .boot.stack nobits
align 4096
stack_bottom:
    resb 4096 * 64
stack_top:
```

#### Código Esperado main64.asm
```asm
; Habilitar zero_bss
extern __bss_start, __bss_end

zero_bss:
    push rdi
    push rcx
    push rax
    
    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    jz .bss_done
    shr rcx, 3
    xor rax, rax
    rep stosq
.bss_done:
    
    pop rax
    pop rcx
    pop rdi
    ret

; En long_mode_start:
call zero_bss
```

---

### 2.3 Separar .bss.boot [L001]
**Archivo:** `targets/x86_64/linker.ld`  
**Esfuerzo:** 2 horas  
**Dependencias:** Ninguna

#### Tareas
- [ ] 2.3.1 Crear sección `.boot.data` para page tables
- [ ] 2.3.2 Crear sección `.boot.stack` para stack
- [ ] 2.3.3 Actualizar main.asm para usar nuevas secciones
- [ ] 2.3.4 Agregar ASSERTs para verificar separación
- [ ] 2.3.5 Actualizar símbolos de exportación

---

### 2.4 Aumentar Mapeo de Memoria [MA001]
**Archivo:** `src/impl/x86_64/boot/main.asm`  
**Esfuerzo:** 3 horas  
**Dependencias:** 2.2

#### Tareas
- [ ] 2.4.1 Evaluar opciones: más entries en L2 vs L3 mapping
- [ ] 2.4.2 Opción A: Aumentar a 1024 entries (2GiB)
- [ ] 2.4.3 Opción B: Usar L3 para mapear 512GiB
- [ ] 2.4.4 Implementar opción seleccionada
- [ ] 2.4.5 Testear acceso a memoria > 1GiB
- [ ] 2.4.6 Documentar límite actual

#### Opción Recomendada: L3 Mapping
```asm
; Mapear 512GiB usando L3 directamente
setup_page_tables:
    mov eax, page_table_l2
    or eax, 0b11
    mov [page_table_l3], eax
    
    ; Mapear cada entry de L2 para 2MiB pages
    ; 512 entries × 512 L2 entries × 2MiB = 512GiB
```

---

### 2.5 Manejo de Errores en kernel_main [M001]
**Archivo:** `src/impl/kernel/main.cpp`  
**Esfuerzo:** 2 horas  
**Dependencias:** 1.1, 1.4

#### Tareas
- [ ] 2.5.1 Capturar retorno de `serial_init_default()`
- [ ] 2.5.2 Agregar fallback a VGA si serial falla
- [ ] 2.5.3 Agregar mensaje de error apropiado
- [ ] 2.5.4 Considerar panic() function para errores fatales
- [ ] 2.5.5 Documentar comportamientos de error

#### Código Esperado
```cpp
extern "C" [[noreturn]] void kernel_main() {
    bool serial_ok = serial_init_default() != 0;
    bool vga_ok = print_detect();
    
    if (!serial_ok && !vga_ok) {
        // Error fatal - no hay output disponible
        // Intentar escribir directamente a VGA
        volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
        vga[0] = 0x4F20;  // ' ' on red
        vga[1] = 0x4F46;  // 'F'
        vga[2] = 0x4F41;  // 'A'
        vga[3] = 0x4F49;  // 'I'
        vga[4] = 0x4F4C;  // 'L'
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }
    
    if (serial_ok) {
        serial_write_str("Serial: OK\r\n");
    }
    if (vga_ok) {
        print_clear();
        // ... init VGA
    }
    
    // Idle loop
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
```

---

### 2.6 Agregar Targets al Makefile [MK001, MK002]
**Archivo:** `0_008_x64/Makefile`  
**Esfuerzo:** 1 hora  
**Dependencias:** Ninguna

#### Tareas
- [ ] 2.6.1 Agregar variable `QEMU`
- [ ] 2.6.2 Agregar target `clean`
- [ ] 2.6.3 Agregar target `distclean`
- [ ] 2.6.4 Agregar target `rebuild`
- [ ] 2.6.5 Agregar target `verify-tools`

#### Código Esperado
```makefile
# Variables configurables
QEMU := qemu-system-x86_64
QEMU_FLAGS := -cdrom $(ISO_FILE)

# Limpieza
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(KERNEL_BIN)
	rm -f serial_output.log

.PHONY: distclean
distclean: clean
	rm -rf $(ISO_DIR)

.PHONY: rebuild
rebuild: clean build-x86_64

.PHONY: verify-tools
verify-tools:
	@echo "Checking tools..."
	@command -v docker >/dev/null || (echo "ERROR: docker not found" && exit 1)
	@command -v $(QEMU) >/dev/null || (echo "ERROR: qemu not found" && exit 1)
	@command -v nasm >/dev/null || (echo "ERROR: nasm not found" && exit 1)
	@command -v $(LD) >/dev/null || (echo "ERROR: ld not found" && exit 1)
	@command -v $(GRUB) >/dev/null || (echo "ERROR: grub-mkrescue not found" && exit 1)
	@echo "All tools available"
```

---

### 📦 Entregables Fase 2
- [ ] Memory barriers en VGA
- [ ] BSS initialization habilitado
- [ ] Secciones .boot.* separadas
- [ ] Mapeo de memoria aumentado (≥2GiB)
- [ ] Manejo de errores en kernel_main
- [ ] Makefile con targets completos
- [ ] Commit: `feat: Robustness improvements for v0.011`

---

## 📁 FASE 3: Mejoras de Prioridad Media (Semana 5-8)

**Objetivo:** Mejorar funcionalidad y experiencia de desarrollo

### 3.1 Validación de Colores en print_set_color [P001]
**Archivo:** `src/impl/x86_64/print.cpp`  
**Esfuerzo:** 1 hora

#### Tareas
- [ ] 3.1.1 Agregar función helper `is_valid_color()`
- [ ] 3.1.2 Validar parámetros en `print_set_color()`
- [ ] 3.1.3 Documentar comportamiento con valores inválidos
- [ ] 3.1.4 Considerar clamp vs reject

---

### 3.2 Optimizar clear_row [PC003]
**Archivo:** `src/impl/x86_64/print.cpp`  
**Esfuerzo:** 2 horas

#### Tareas
- [ ] 3.2.1 Reescribir para escribir ambos bytes juntos
- [ ] 3.2.2 Usar struct assignment en lugar de campo por campo
- [ ] 3.2.3 Benchmark antes/después (contar ciclos)
- [ ] 3.2.4 Documentar optimización

#### Código Esperado
```cpp
void clear_row(size_t row) {
    if (!vga_detected || row >= VGA_ROWS) return;
    
    VgaChar blank = {' ', current_color};
    volatile VgaChar* row_ptr = &vga_buffer[row * VGA_COLS];
    
    for (size_t c = 0; c < VGA_COLS; c++) {
        row_ptr[c] = blank;  // Single write
    }
}
```

---

### 3.3 Optimizar Scroll con memmove [PC004]
**Archivo:** `src/impl/x86_64/print.cpp`  
**Esfuerzo:** 2 horas

#### Tareas
- [ ] 3.3.1 Implementar `memmove()` para kernel (si no existe)
- [ ] 3.3.2 Refactorizar `print_newline()` para usar memmove
- [ ] 3.3.3 Verificar que no hay overlap issues
- [ ] 3.3.4 Benchmark antes/después

---

### 3.4 Debug Macros Mejoradas [D001, D002, D003]
**Archivo:** `src/intf/debug.h`  
**Esfuerzo:** 2 horas

#### Tareas
- [ ] 3.4.1 Agregar `DEBUG_PRINTF()` con formato
- [ ] 3.4.2 Agregar `DEBUG_PRINTLN()` con newline automático
- [ ] 3.4.3 Agregar `DEBUG_ASSERT()`
- [ ] 3.4.4 Agregar `DEBUG_HEX64()` para 64-bit values
- [ ] 3.4.5 Documentar uso de cada macro

#### Código Esperado
```cpp
#define DEBUG_PRINTLN(msg) do { \
    serial_write_str(msg); \
    serial_write_str("\r\n"); \
} while(0)

#define DEBUG_ASSERT(cond) do { \
    if (!(cond)) { \
        serial_write_str("\r\n[ASSERT FAILED] "); \
        serial_write_str(__FILE__); \
        serial_write_str(":"); \
        serial_write_dec(__LINE__); \
        serial_write_str("\r\n"); \
        for(;;) __asm__ volatile ("hlt"); \
    } \
} while(0)
```

---

### 3.5 Funciones de Impresión de Números
**Archivos:** `src/intf/print.h`, `src/impl/x86_64/print.cpp`  
**Esfuerzo:** 3 horas

#### Tareas
- [ ] 3.5.1 Agregar `print_dec(uint32_t)` al header
- [ ] 3.5.2 Agregar `print_dec64(uint64_t)` al header
- [ ] 3.5.3 Agregar `print_hex(uint32_t)` al header
- [ ] 3.5.4 Agregar `print_hex64(uint64_t)` al header
- [ ] 3.5.5 Implementar en print.cpp
- [ ] 3.5.6 Testear con valores límite

---

### 3.6 Funciones de Consulta en print
**Archivos:** `src/intf/print.h`, `src/impl/x86_64/print.cpp`  
**Esfuerzo:** 2 horas

#### Tareas
- [ ] 3.6.1 Agregar `print_get_cursor(size_t* col, size_t* row)`
- [ ] 3.6.2 Agregar `print_get_color(uint8_t* fg, uint8_t* bg)`
- [ ] 3.6.3 Agregar `print_set_cursor(size_t col, size_t row)`
- [ ] 3.6.4 Implementar funciones
- [ ] 3.6.5 Documentar uso

---

### 3.7 Serial Signed Numbers [S002]
**Archivos:** `src/intf/serial.h`, `src/impl/x86_64/serial.cpp`  
**Esfuerzo:** 2 horas

#### Tareas
- [ ] 3.7.1 Agregar `serial_write_dec_signed(int32_t)` al header
- [ ] 3.7.2 Agregar `serial_write_dec64_signed(int64_t)` al header
- [ ] 3.7.3 Implementar manejo de signo
- [ ] 3.7.4 Testear con valores positivos, negativos y cero

---

### 3.8 Información de Hardware en Boot [MA006]
**Archivos:** `main.asm`, `main64.asm`, `main.cpp`  
**Esfuerzo:** 4 horas

#### Tareas
- [ ] 3.8.1 Guardar resultado de CPUID en variables
- [ ] 3.8.2 Guardar detección de long mode
- [ ] 3.8.3 Pasar información al kernel (struct o registers)
- [ ] 3.8.4 Imprimir información en kernel_main
- [ ] 3.8.5 Considerar memory map (INT 15h E820)

---

### 📦 Entregables Fase 3
- [ ] Validación de colores
- [ ] clear_row optimizado
- [ ] Scroll con memmove
- [ ] Debug macros completas
- [ ] Funciones de impresión de números
- [ ] Funciones de consulta de cursor/color
- [ ] Serial con signed numbers
- [ ] Información de hardware en boot
- [ ] Commit: `feat: Feature enhancements for v0.012`

---

## 📁 FASE 4: Mejoras Menores y Enhancements (Semana 9-10)

**Objetivo:** Pulir código y preparar para contribuciones

### 4.1 Constantes para Magic Numbers [PC005, H002]
**Archivos:** Varios  
**Esfuerzo:** 2 horas

#### Tareas
- [ ] 4.1.1 Definir `VGA_TEST_CHAR` y `VGA_TEST_COLOR`
- [ ] 4.1.2 Definir constantes Multiboot2
- [ ] 4.1.3 Reemplazar magic numbers en todo el código
- [ ] 4.1.4 Crear archivo `constants.h` si es necesario

---

### 4.2 GRUB.cfg Nombre Correcto [GRUB001]
**Archivo:** `targets/x86_64/iso/boot/grub/grub.cfg`  
**Esfuerzo:** 30 minutos

#### Tareas
- [ ] 4.2.1 Cambiar "NW OS 0.001" a "GLOBEX_OS v0.009_x64"
- [ ] 4.2.2 Usar variable de versión si es posible

---

### 4.3 Comentarios en Inglés [MNT001]
**Archivos:** Todos  
**Esfuerzo:** 4 horas

#### Tareas
- [ ] 4.3.1 Identificar comentarios críticos en español
- [ ] 4.3.2 Traducir comentarios de API pública
- [ ] 4.3.3 Mantener comentarios internos en español (opcional)
- [ ] 4.3.4 Actualizar documentación

**Nota:** Esta es una decisión de proyecto. Se recomienda:
- API pública: Inglés
- Comentarios internos: Español (equipo actual)

---

### 4.4 Función panic() [PANIC001]
**Archivos:** `src/intf/`, `src/impl/`  
**Esfuerzo:** 3 horas

#### Tareas
- [ ] 4.4.1 Crear `panic.h` con declaración
- [ ] 4.4.2 Crear `panic.cpp` con implementación
- [ ] 4.4.3 Imprimir mensaje de error
- [ ] 4.4.4 Imprimir stack trace (si es posible)
- [ ] 4.4.5 Hlt loop infinito
- [ ] 4.4.6 Usar en kernel_main para errores fatales

---

### 4.5 Mejoras en Error Display [MA005]
**Archivo:** `src/impl/x86_64/boot/main.asm`  
**Esfuerzo:** 2 horas

#### Tareas
- [ ] 4.5.1 Mostrar más contexto en error handler
- [ ] 4.5.2 Mostrar código de error en hexadecimal
- [ ] 4.5.3 Mostrar mensaje descriptivo
- [ ] 4.5.4 Considerar beep speaker para atención

---

### 4.6 Documentación de Límites [DOC001]
**Archivos:** README, QWEN.md  
**Esfuerzo:** 2 horas

#### Tareas
- [ ] 4.6.1 Documentar límite de memoria mapeada
- [ ] 4.6.2 Documentar límite de stack (64KB)
- [ ] 4.6.3 Documentar puertos serial soportados
- [ ] 4.6.4 Documentar requisitos de QEMU

---

### 📦 Entregables Fase 4
- [ ] Magic numbers reemplazados
- [ ] GRUB cfg actualizado
- [ ] Política de comentarios definida
- [ ] Función panic() implementada
- [ ] Error display mejorado
- [ ] Documentación de límites
- [ ] Commit: `chore: Polish and documentation for v0.013`

---

## 📁 FASE 5: Testing y CI/CD (Semana 11-12)

**Objetivo:** Establecer infraestructura de testing continuo

### 5.1 Tests Unitarios para Serial [TEST001]
**Archivos:** `tests/serial_test.cpp`  
**Esfuerzo:** 4 horas

#### Tareas
- [ ] 5.1.1 Crear framework de tests simple
- [ ] 5.1.2 Test `serial_init_default()`
- [ ] 5.1.3 Test `serial_write_char()`
- [ ] 5.1.4 Test `serial_write_str()`
- [ ] 5.1.5 Test `serial_write_hex()`
- [ ] 5.1.6 Test `serial_write_dec()`
- [ ] 5.1.7 Test timeout functionality

---

### 5.2 Tests Unitarios para Print [TEST002]
**Archivos:** `tests/print_test.cpp`  
**Esfuerzo:** 4 horas

#### Tareas
- [ ] 5.2.1 Test `print_detect()`
- [ ] 5.2.2 Test `print_char()` con todos los caracteres especiales
- [ ] 5.2.3 Test `print_str()` con NULL
- [ ] 5.2.4 Test `print_set_color()` con valores límite
- [ ] 5.2.5 Test scroll (llenar pantalla + 1 caracter)

---

### 5.3 Tests de Integración [TEST003]
**Archivos:** `tests/integration_test.cpp`  
**Esfuerzo:** 4 horas

#### Tareas
- [ ] 5.3.1 Test boot completo
- [ ] 5.3.2 Test serial + VGA simultáneos
- [ ] 5.3.3 Test idle loop por N segundos
- [ ] 5.3.4 Test reboot después de panic

---

### 5.4 CI/CD con GitHub Actions [CI001]
**Archivo:** `.github/workflows/build.yml`  
**Esfuerzo:** 4 horas

#### Tareas
- [ ] 5.4.1 Crear workflow de build
- [ ] 5.4.2 Configurar Docker en CI
- [ ] 5.4.3 Agregar test de compilación
- [ ] 5.4.4 Agregar test de QEMU (headless)
- [ ] 5.4.5 Configurar artifacts (ISO)
- [ ] 5.4.6 Agregar badge al README

#### Workflow Esperado
```yaml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    container:
      image: myos-buildenv
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Build Kernel
      run: make build-x86_64
    
    - name: Test Boot (QEMU)
      run: |
        timeout 10 qemu-system-x86_64 -cdrom dist/x86_64/kernel.iso \
          -serial file:serial.log -display none || true
        grep -q "GLOBEX_OS" serial.log
    
    - name: Upload ISO
      uses: actions/upload-artifact@v3
      with:
        name: kernel-iso
        path: dist/x86_64/kernel.iso
```

---

### 5.5 Script de Auto-Test [TEST004]
**Archivo:** `scripts/run_tests.sh`  
**Esfuerzo:** 2 horas

#### Tareas
- [ ] 5.5.1 Crear script que compile y ejecute en QEMU
- [ ] 5.5.2 Capturar output serial
- [ ] 5.5.3 Verificar mensajes esperados
- [ ] 5.5.4 Retornar código de exit apropiado

---

### 5.6 Code Coverage [TEST005]
**Esfuerzo:** 3 horas

#### Tareas
- [ ] 5.6.1 Investigar gcov para kernel freestanding
- [ ] 5.6.2 Configurar flags de coverage
- [ ] 5.6.3 Generar reporte después de tests
- [ ] 5.6.4 Integrar con CI

---

### 📦 Entregables Fase 5
- [ ] Framework de tests implementado
- [ ] Tests unitarios para serial
- [ ] Tests unitarios para print
- [ ] Tests de integración
- [ ] GitHub Actions workflow
- [ ] Script de auto-test
- [ ] Reporte de code coverage
- [ ] Commit: `test: Add testing infrastructure for v0.015`

---

## 📊 CRONOGRAMA RESUMEN

| Fase | Semanas | Hitos Principales | Versión |
|------|---------|-------------------|---------|
| **Fase 1** | 1-2 | Timeouts, Volatile, Validation | v0.010 |
| **Fase 2** | 3-4 | BSS Init, Memory Map, Error Handling | v0.011 |
| **Fase 3** | 5-8 | Optimizaciones, Features, Debug | v0.012 |
| **Fase 4** | 9-10 | Polish, Documentation, Panic | v0.013 |
| **Fase 5** | 11-12 | Testing, CI/CD | v0.015 |

---

## 🎯 KRITERIOS DE ÉXITO

### Al final de Fase 1
- [ ] Kernel no se hanga por timeout serial
- [ ] No hay race conditions obvias
- [ ] Variables de hardware son volatile

### Al final de Fase 2
- [ ] BSS se inicializa correctamente
- [ ] Más de 1GiB mapeado
- [ ] Errores se manejan apropiadamente

### Al final de Fase 3
- [ ] Debug macros completas
- [ ] Todas las funciones de print/serial implementadas
- [ ] Información de hardware disponible

### Al final de Fase 4
- [ ] Código sin magic numbers
- [ ] Documentación completa
- [ ] Función panic() funcional

### Al final de Fase 5
- [ ] >80% code coverage
- [ ] CI/CD funcionando
- [ ] Tests automáticos en cada commit

---

## 📈 MÉTRICAS DE PROGRESO

| Métrica | Actual | Fase 1 | Fase 2 | Fase 3 | Fase 4 | Fase 5 |
|---------|--------|--------|--------|--------|--------|--------|
| Issues Críticos | 2 | 0 | 0 | 0 | 0 | 0 |
| Issues Altos | 4 | 0 | 0 | 0 | 0 | 0 |
| Code Coverage | 0% | 0% | 0% | 20% | 40% | >80% |
| Tests Automatizados | 0 | 0 | 0 | 5 | 10 | 20+ |
| CI/CD | ❌ | ❌ | ❌ | ❌ | ⚠️ | ✅ |

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

*Documento vivo - Actualizar después de cada fase completada*
