# GLOBEX OS — Auditoria de Codigo x86_64
**Fecha:** 2026-03-25
**Rama auditada:** `feature/audit-fixes-phase1-fix-idt-001`
**Metodologia:** 4 agentes especializados en paralelo + verificacion manual de hallazgos criticos
**Alcance:** Todo el codigo fuente bajo `kernels/x86_64/src/`, `tests/`, `Makefile`, `linker.ld`

---

## Resumen Ejecutivo

| Severidad  | Abiertos | Ya corregidos | Total hallazgos |
|-----------|----------|---------------|-----------------|
| CRITICO   | 0        | 3             | 3               |
| ALTO      | 0        | 8             | 8               |
| MEDIO     | 2        | 9             | 11              |
| BAJO      | 4        | 6             | 10              |
| INFO/ARCH | 6        | —             | 6               |

**Estado general:** El kernel esta en buen estado de seguridad y correccion. Todos los problemas
criticos y altos han sido corregidos en las fases de auditoria anteriores. Quedan 2 problemas
medios abiertos y 4 bajos, todos con bajo riesgo operacional.

---

## Hallazgos Abiertos

### MEDIO

---

#### [OPEN-MED-001] `src/core/log/log.cpp:260` — Marcador de truncacion escribe `..` en lugar de `...`

**Subsistema:** Core / Log
**Descripcion:**
`build_message()` define `buf_end = message + message_size - 1` (ranura del NUL, limite exclusivo).
Al escribir el marcador de truncacion:

```cpp
char* mark = buf_end - 2;          // posiciones: buf_end-2, buf_end-1, buf_end
if (mark + 1 < buf_end) mark[1] = '.';   // buf_end-1 < buf_end -> TRUE  -> se escribe
if (mark + 2 < buf_end) mark[2] = '.';   // buf_end   < buf_end -> FALSE -> NO se escribe
```

`mark + 2 < buf_end` se evalua como `buf_end < buf_end` → false, por lo que solo se escriben
2 puntos (`..`) en lugar de 3 (`...`).

**Riesgo:** Cosmético — los mensajes truncados aparecen con `..` en lugar de `...`, lo que puede
confundir al leer logs de depuracion. No hay desbordamiento de buffer.

**Correccion:**
```cpp
// En log.cpp:260
- char* mark = buf_end - 2;
+ char* mark = buf_end - 3;
```
Con `mark = buf_end - 3`: mark[0] en buf_end-3, mark[1] en buf_end-2, mark[2] en buf_end-1.
Todos los tres comprobantes `mark+N < buf_end` seran true, y el NUL en buf_end queda intacto.

---

#### [OPEN-MED-002] `src/memory/slab.cpp:548` — Invariante de bloqueo implicita en `kmem_alloc()`

**Subsistema:** Memoria / Slab
**Descripcion:**
`kmem_alloc()` se llama internamente desde `slab_init()` mientras se construyen las caches, pero
el lock `g_slab_lock` ya esta siendo adquirido por el contexto de inicializacion. El deadlock se
evita porque `g_slab_available == 0` durante `slab_init()`, lo que hace que `kmalloc()` evite la
ruta del slab. Sin embargo, este invariante no esta documentado en el codigo:

```cpp
// slab.cpp -- slab_init() adquiere g_slab_lock implicito durante construccion de cache
// Si g_slab_available cambiara a 1 prematuramente, kmalloc() desde init_cache()
// intentaria adquirir g_slab_lock de nuevo -> deadlock (spinlock no-reentrante).
```

**Riesgo:** Bajo en el diseno actual. Podria convertirse en un deadlock dificil de diagnosticar
si un futuro refactor cambia el orden de inicializacion o activa el slab antes de tiempo.

**Correccion:**
Agregar un comentario explicito en `init_cache()` y/o en `slab_init()`:

```cpp
/* INVARIANTE DE BLOQUEO: kmalloc() llamado aqui redirige a bitmap_alloc()
 * porque g_slab_available == 0 durante slab_init(). Si esto cambia,
 * la adquisicion de g_slab_lock aqui causaria deadlock. */
```

---

### BAJO

---

#### [OPEN-LOW-001] `src/lib/spinlock/spinlock.cpp:301-305,325-326` — Comentarios obsoletos de "fast path"

**Subsistema:** Lib / Spinlock
**Descripcion:**
La "fast path" (adquisicion sin deshabilitar interrupciones) fue eliminada, pero los comentarios
en `spinlock_release()` aun la mencionan:

```cpp
// Linea 305: "interrupts_enabled == false => Fast path, interrupts were never touched"
// Linea 325: "If was_enabled == false, we got the lock via fast path and
//             interrupts were never disabled."
```

La logica del codigo ES CORRECTA: `was_enabled` refleja correctamente si las interrupciones
estaban habilitadas antes de la adquisicion. Pero el comentario es enganoso y podria hacer que
un desarrollador crea que existe una ruta de codigo que ya no existe.

**Riesgo:** Solo confusion de mantenimiento. No hay bug funcional.

**Correccion:** Reemplazar los comentarios de "fast path" con la semantica real:
```
// was_enabled == false => Las interrupciones ya estaban deshabilitadas antes de adquirir;
//                         no llamar sti para preservar el estado del invocador.
// was_enabled == true  => Las interrupciones estaban habilitadas; restaurarlas al liberar.
```

---

#### [OPEN-LOW-002] `src/arch/x86_64/idt/idt.cpp:273-276` — Codigos de error cero suprimidos en el handler de excepciones

**Subsistema:** Arch / IDT
**Descripcion:**
`default_exception_handler()` solo imprime el codigo de error si `err_code != 0`:

```cpp
if (frame->error_code != 0) {
    serial_write_str("  Error code: 0x");
    serial_write_hex(frame->error_code);
}
```

Para excepciones que definen un codigo de error (#TS, #SS, #GP, #PF, #AC), un codigo de error
legitimo de 0 significa algo especifico (e.g., #GP con selector 0 indica acceso fuera de
segmento). Omitirlo puede dificultar el diagnostico.

**Riesgo:** Perdida de informacion de diagnostico. No es un bug de correccion.

**Correccion:** Imprimir el codigo de error siempre para vectores que lo definen (8, 10-14, 17):
```cpp
// Vectores con error code: 8, 10, 11, 12, 13, 14, 17
bool has_errcode = (vec == 8 || (vec >= 10 && vec <= 14) || vec == 17);
if (has_errcode) { /* imprimir siempre */ }
```

---

#### [OPEN-LOW-003] `src/lib/string/string.cpp:268` — Expresion `destsize - 1` con underflow potencial (protegida por guard)

**Subsistema:** Lib / String
**Descripcion:**
En `strlcpy()`, la condicion del bucle usa `i < destsize - 1`. Si `destsize == 0` (size_t),
`destsize - 1` se envuelve a `SIZE_MAX`, creando una condicion aparentemente siempre verdadera.
El guard `if (destsize > 0)` en la linea 267 previene la ejecucion del bucle, por lo que no hay
bug en el estado actual.

```cpp
if (destsize > 0) {                              // guard correcto
    while (i < destsize - 1 && ...) { ... }     // destsize-1 aun se evalua como SIZE_MAX
    dest[i] = '\0';
}
```

**Riesgo:** Minimo mientras el guard permanezca. Expresion fragil ante refactorizacion.

**Correccion:** Reescribir para eliminar la sustraccion potencialmente peligrosa:
```cpp
while (i + 1 < destsize && src[i] != '\0') {
    dest[i] = src[i];
    i++;
}
```

---

#### [OPEN-LOW-004] `src/memory/heap.cpp:265` — Limite magico `0x80000000UL` sin constante simbolica

**Subsistema:** Memoria / Heap
**Descripcion:**
`is_slab_address()` usa `0x80000000UL` como limite superior del espacio de direcciones mapeado:

```cpp
if ((uintptr_t)ptr < 0x100000UL || (uintptr_t)ptr >= 0x80000000UL)
    return false;
```

Este valor es correcto (mapeo de identidad de 0-2GiB), pero esta duplicado implicitamente
respecto a la arquitectura del mapa de memoria documentada en `CLAUDE.md`.

**Riesgo:** Sin impacto funcional. Si el mapeo cambia, hay que actualizar multiples lugares.

**Correccion:** Definir una constante:
```cpp
#define IDENTITY_MAP_END 0x80000000UL  // 2 GiB identity-mapped region limit
```

---

## Hallazgos Ya Corregidos (historial de referencia)

### CRITICOS — Corregidos

| ID | Archivo | Descripcion | Commit |
|----|---------|-------------|--------|
| CRIT-OLD-001 | `core/log/log.cpp` | Auto-deadlock en `log_hex_dump()` al llamar `LOG_DEBUG` dentro del lock | fase5 |
| CRIT-OLD-002 | `memory/bitmap.cpp` | TOCTOU race en `bitmap_alloc()` / `bitmap_free()` — falta atomicidad | fase anterior |
| CRIT-OLD-003 | `arch/x86_64/idt/irq.cpp` | Vectores IRQ 32-47 no registrados en IDT hasta llamar `irq_init()` | fase anterior |

### ALTOS — Corregidos

| ID | Archivo | Descripcion | Commit |
|----|---------|-------------|--------|
| HIGH-OLD-001 | `arch/x86_64/idt/irq.cpp:337` | EOI enviado a master antes que a slave — riesgo re-entrada espuria | fase6 |
| HIGH-OLD-002 | `arch/x86_64/idt/irq.cpp:235` | IRQ2 cascade no desenmascarado al habilitar IRQs esclavas | fase6 |
| HIGH-OLD-003 | `memory/heap.cpp:189` | `krealloc()` leia PAGE_SIZE para punteros slab en lugar de `object_size` | fase anterior |
| HIGH-OLD-004 | `memory/heap.cpp:124` | Metadata `g_page_alloc_count` escrita fuera del lock | fase7 |
| HIGH-OLD-005 | `drivers/serial/serial.cpp:712` | `serial_reinit()` no establecia `failed=1` si reinit falla | fase7 |
| HIGH-OLD-006 | `arch/x86_64/boot/main.asm:227` | `CR0.WP` no activado junto con `CR0.PG` — ring-0 podia escribir en paginas R/O | fase7 |
| HIGH-OLD-007 | `core/panic/panic.cpp:48` | Deadlock en panic si `g_serial_lock` estaba tomado al momento del fallo | fase5 |
| HIGH-OLD-008 | `arch/x86_64/gdt/gdt.cpp` | Descriptor TSS de 16 bytes mal codificado para x86_64 | fase anterior |

### MEDIOS — Corregidos

| ID | Archivo | Descripcion | Commit |
|----|---------|-------------|--------|
| MED-OLD-001 | `memory/slab.cpp:637` | Busqueda O(N*M) de slab propietario en `kmem_free()` — reemplazado por O(1) | fase7 |
| MED-OLD-002 | `memory/slab.cpp:648` | Ausencia de check de bounds `obj_area_start/end` — underflow de uintptr_t | fase5 |
| MED-OLD-003 | `arch/x86_64/idt/irq.cpp:391` | `irq_reset_counts()` usaba assignment simple en lugar de `atomic_store32` | fase6 |
| MED-OLD-004 | `arch/x86_64/idt/irq.cpp:177` | `wmb()` insuficiente para publicar handler (reemplazado por `mb()`) | fase6 |
| MED-OLD-005 | `core/log/log.cpp:125` | `append_hex64()` emitia digitos variables en lugar de siempre 16 | fase7 |
| MED-OLD-006 | `core/log/log.cpp:228` | Falta soporte `%p`, `%llu`, `%zu` en `build_message()` | fase7 |
| MED-OLD-007 | `lib/string/string.cpp:256` | `strlcpy()` recorria la cadena dos veces (copia + strlen) | fase7 |
| MED-OLD-008 | `memory/slab.cpp:307` | `init_cache()` sin comprobacion de agotamiento del pool | fase6 |
| MED-OLD-009 | `memory/guard.cpp:63` | Ausencia de comprobacion `l2_idx >= 512` antes de indexar tabla L2 | fase6 |

---

## Deuda Arquitectonica (documentada, aceptable)

Estos items son conocidos, documentados en el codigo, y aceptables para el estado actual
del kernel (ring-0, single address space, sin entrada no confiable).

| ID | Archivo | Descripcion | Condicion para resolver |
|----|---------|-------------|------------------------|
| ARCH-001 | `boot/main.asm:99` | Bit NX/XD no activado en ninguna entrada de tabla de paginas | Al agregar interfaz de usuario / syscalls |
| ARCH-002 | `idt/irq.cpp:23` | `g_irq_stack_depth` es un global unico, no per-CPU | Al implementar SMP con APIC |
| ARCH-003 | `memory/guard.cpp:52` | Asume mapeo de identidad (virt == phys) para paginas L1 | Al implementar higher-half o KASLR |
| ARCH-004 | `arch/x86_64/idt/idt.cpp:182` | Validacion de handler solo cubre rango 0-2GiB | Al implementar higher-half mapping |
| ARCH-005 | `memory/slab.cpp:444` | `slab_shutdown()` no devuelve paginas al bitmap | Si se requiere gestion de memoria por procesos |
| ARCH-006 | `drivers/vga/vga.cpp` | Operaciones VGA no son seguras en ISR (solo ring-0, no hay ISRs que usen VGA actualmente) | Al implementar VGA desde ISR |

---

## Evaluacion por Subsistema

### Boot (`src/arch/x86_64/boot/`)
**Nota: CORRECTO**
- BSS zeroing en 64-bit mode con `rep stosq` y division de techo: correcto
- Tablas de paginas con bits PS, P, R/W correctos; upper 32 bits zeroed explicitamente
- `CR0.WP | CR0.PG` activados juntos: correcto
- Verificacion de entrada multiboot: correcto
- Unico pendiente: NX bit (ARCH-001, deuda arquitectonica documentada)

### GDT (`src/arch/x86_64/gdt/`)
**Nota: CORRECTO**
- 7 entradas: null, code/data kernel, code/data usuario, TSS (2x8 bytes)
- Descriptor TSS de 64 bits correctamente codificado con base[63:32] en entrada de alta
- IST1/IST2/IST3 inicializados para #DF, NMI, #MC respectivamente
- Limpieza redundante del descriptor null: inofensiva (defensiva)

### IDT / IRQ (`src/arch/x86_64/idt/`)
**Nota: CORRECTO, 1 LOW abierto**
- Todos los vectores 0-31 con handler registrado (excepciones + reservados)
- Vectores 32-47 registrados via `irq_init()` antes de `sti`
- EOI: slave antes que master para IRQ >= 8 (corregido)
- IRQ2 cascade: desenmascarado al habilitar IRQs esclavas (corregido)
- Tipo de gate: `IDT_INTERRUPT_GATE (0x8E)` — correcto
- IST: #DF=IST1, NMI=IST2, #MC=IST3 — correcto
- LOW-002: codigos de error cero suprimidos en handler (ver OPEN-LOW-002)

### Memoria (`src/memory/`)
**Nota: EXCELENTE, 1 MEDIO documentacional**
- Bitmap: atomicidad completa via `__atomic_compare_exchange_n` y `__atomic_fetch_and`
- Slab: lookup O(1) correcto, bounds check obj_area, pool exhaustion check
- Heap: metadata dentro del lock, `krealloc()` lee `object_size` desde header del slab
- Guard: check `l2_idx >= 512`, comentario de asuncion de identidad
- MED-002: invariante de bloqueo implicita en `slab_init()` (ver OPEN-MED-002)

### Serial (`src/drivers/serial/`)
**Nota: CORRECTO**
- Tracking de escrituras parciales: correcto
- `serial_reinit()` establece `failed=1` si falla: correcto
- `serial_force_unlock()` con precondiciones documentadas: correcto
- Nota de layering: `serial_force_unlock()` definida en `spinlock.cpp` accede directamente
  a `g_serial_lock` — funcional pero acoplamiento de capas a considerar en refactorizacion

### VGA (`src/drivers/vga/`)
**Nota: CORRECTO**
- `vga_get_cursor_col/row()` intencionalmente sin lock (read atomico en x86_64): documentado
- Scroll con `volatile uint16_t*` para escrituras de 2 bytes: correcto
- Limite 80x25 respetado: correcto

### PIT (`src/drivers/pit/`)
**Nota: CORRECTO para single-CPU**
- `ms_remainder` como `static volatile uint32_t`: correcto para 8259 PIC (IRQ0
  entregado a un solo CPU; no hay concurrencia en el handler)
- Si se migra a APIC donde IRQ0 puede ir a multiples CPUs, se requiere `atomic_add32`
- `pit_shutdown()` side-effect documentado explicitamente: correcto

### Log (`src/core/log/`)
**Nota: BUENO, 1 MEDIO abierto**
- Auto-deadlock eliminado (LOG_DEBUG antes de adquirir lock)
- `%p`, `%llu`, `%zu`, `%lld` ahora soportados: correcto
- `append_hex64()` siempre emite 16 digitos: correcto
- **OPEN-MED-001**: marcador de truncacion escribe `..` en lugar de `...`

### Panic (`src/core/panic/`)
**Nota: CORRECTO**
- `serial_force_unlock()` llamado con `cli` activo antes de cualquier salida: correcto
- No intenta reinicializar serial en panic path: correcto (justificado en comentario)
- VGA panic output como fallback si serial no inicializado: correcto

### String (`src/lib/string/`)
**Nota: BUENO, 1 LOW abierto**
- `strlcpy()` single-pass: correcto, eficiente
- `memcmp()` NULL → `PANIC_IF_FALSE`: apropiado para kernel
- `memmove()` logica forward/backward: correcto
- `memcpy()` overlap detection solo en DEBUG: funcional, documentado (OPEN-LOW-003 adjacente)
- **OPEN-LOW-003**: expresion `destsize - 1` con underflow potencial (protegida por guard)

### Spinlock (`src/lib/spinlock/`)
**Nota: CORRECTO, 1 LOW de comentarios**
- Logica de `spinlock_release()` CORRECTA: `was_enabled` refleja estado pre-adquisicion
- CAS loop con LOCK CMPXCHG: correcto
- `serial_force_unlock()` con barrier compiler: correcto
- **OPEN-LOW-001**: comentarios de "fast path" obsoletos (logica correcta, comentarios enganosos)

### Kernel Main (`src/kernel/main.cpp`)
**Nota: CORRECTO**
- Orden de inicializacion correcto: serial → VGA → GDT → IDT/IRQ → PIT → heap → tests
- Todos los tests ejecutados después de la inicializacion de sus dependencias

### Build / Linker
**Nota: CORRECTO**
- `-Wall -Wextra -Wpedantic -Werror`: todo compila limpio
- Linker script: ASSERT para tamano (<10MB), alineacion (4KB), `.boot.data`
- `LOG_LEVEL` configurable en build time: correcto

---

## Cobertura de Tests

| Suite | Descripcion | Estado |
|-------|-------------|--------|
| test_bss | Inicializacion de BSS | PASSED |
| test_color | Validacion de colores VGA | PASSED |
| test_debug | Macros de debug | PASSED |
| test_gdt_idt | GDT init, IDT init, trap #BP | PASSED |
| test_hardware | CPUID info | PASSED |
| test_memory | Acceso a 80MB | PASSED |
| test_memory_manager | kmalloc/kfree/krealloc | PASSED |
| test_pit | IRQ0, ticks, pit_wait | PASSED |
| test_print | Funciones de impresion | PASSED |
| test_query | Funciones de consulta | PASSED |
| test_serial | Tasas de baud | PASSED |
| test_serial_signed | Numeros con signo | PASSED |
| test_slab | Slab allocator completo | PASSED |
| test_slab_debug | Slab debug checks | PASSED |
| test_spinlock | Acquire/release, interrupts | PASSED |
| test_spinlock_smp | SMP spinlock patterns | PASSED |
| test_spinlock_stress | Stress test spinlock | PASSED |
| test_strlcpy | Safe string copy | PASSED |
| test_string | Funciones de string | PASSED |
| test_string_boundaries | NULL safety, bounds | PASSED |

**20/20 tests PASSED en QEMU** (verificado en esta sesion)

**Brechas de cobertura:**
- No hay test para el marcador de truncacion de log (OPEN-MED-001)
- No hay test que verifique `irq_enable()` con IRQs esclavas y cascade
- No hay test de `serial_reinit()` failure path
- No hay test de `krealloc()` con puntero slab

---

## Matriz de Riesgo

```
Alta probabilidad de ocurrencia
        |
        |  [ARCH-001] NX bit
        |  [ARCH-002] IRQ depth SMP
   Baja |
proba-  |              [MED-001] log ..
bilidad |              [MED-002] slab lock invariant
        |                          [LOW-001] spinlock comments
        |                          [LOW-002] error code 0
        |                          [LOW-003] strlcpy underflow guard
        |                          [LOW-004] magic 0x80000000
        +----------------------------------------------->
              Bajo                    Alto
                    Impacto si ocurre
```

---

## Recomendaciones Priorizadas

### Inmediato (antes de merge a main)

1. **[OPEN-MED-001]** Corregir marcador de truncacion en `log.cpp:260`:
   `mark = buf_end - 2` → `mark = buf_end - 3`

2. **[OPEN-LOW-001]** Actualizar comentarios de "fast path" en `spinlock_release()`:
   Reflejar que ALL paths deshabilitan interrupciones; eliminar referencia a fast path.

### Corto plazo

3. **[OPEN-MED-002]** Documentar invariante de bloqueo en `slab.cpp::init_cache()`.

4. **[OPEN-LOW-002]** Imprimir codigo de error aun cuando sea 0 para excepciones que lo definen.

5. **[OPEN-LOW-003]** Reescribir condicion de bucle en `strlcpy()` para eliminar `destsize - 1`.

6. **[OPEN-LOW-004]** Definir `IDENTITY_MAP_END` como constante simbolica.

### Largo plazo (con SMP / userspace)

7. **[ARCH-001]** Activar NX bit en paginas que no son codigo al agregar syscall interface.
8. **[ARCH-002]** Reemplazar `g_irq_stack_depth` con variable per-CPU.
9. **[ARCH-003]** Agregar `virt_to_phys()` en guard.cpp al implementar higher-half mapping.

---

*Auditoria generada por Claude Code el 2026-03-25*
*Metodologia: 4 agentes especializados en paralelo + verificacion manual de hallazgos criticos*
