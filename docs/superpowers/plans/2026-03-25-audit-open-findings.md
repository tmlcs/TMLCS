# Audit Open Findings — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Corregir los 6 hallazgos abiertos identificados en la auditoria del 2026-03-25 (2 medios, 4 bajos).

**Architecture:** Cada tarea es independiente y toca un archivo diferente. Orden: problemas
funcionales primero (MED), luego correcciones de calidad de codigo (LOW). Todos los cambios
deben dejar 0 lineas con "FAILED" en `serial_output.log` tras `make run-serial`.

**Tech Stack:** C++ freestanding, NASM, ld linker script, QEMU x86_64, make
**`LOG_BUFFER_SIZE`:** 256 bytes (definido en `src/core/log/log.cpp:22`)

---

## Archivos Afectados

| Archivo | Lineas | Tipo de cambio |
|---------|--------|----------------|
| `kernels/x86_64/src/core/log/log.cpp` | 260 | Bug fix (1 char) |
| `kernels/x86_64/src/memory/slab.cpp` | ~319 | Agregar comentario |
| `kernels/x86_64/src/arch/x86_64/idt/idt.cpp` | 273-278 | Logic fix |
| `kernels/x86_64/src/lib/spinlock/spinlock.cpp` | 299-330 | Actualizar comentarios |
| `kernels/x86_64/src/lib/string/string.cpp` | 270 | Refactor defensivo |
| `kernels/x86_64/src/memory/heap.cpp` | ~15, 270 | Agregar constante simbolica |
| `kernels/x86_64/tests/test_log.cpp` *(nuevo)* | — | Nuevo test |
| `kernels/x86_64/tests/test_log.h` *(nuevo)* | — | Header del nuevo test |
| `kernels/x86_64/src/kernel/main.cpp` | 41, 319, 369 | Registrar nuevo test |

**Comandos de referencia (ejecutar desde `kernels/x86_64/`):**
```bash
make build-x86_64              # compilar solamente
make run-serial                # compilar + ejecutar en QEMU + volcar serial_output.log
grep "FAILED" serial_output.log   # debe estar vacio (0 lineas)
```

---

## Task 1: Corregir marcador de truncacion en log.cpp [MED-001]

**Files:**
- Modify: `kernels/x86_64/src/core/log/log.cpp:260`
- Create: `kernels/x86_64/tests/test_log.cpp`
- Create: `kernels/x86_64/tests/test_log.h`
- Modify: `kernels/x86_64/src/kernel/main.cpp:41,319,369`

**Contexto:**
`LOG_BUFFER_SIZE = 256`. En `build_message()`: `buf_end = message + 255` (slot del NUL, exclusivo).
Con `mark = buf_end - 2 = message + 253`: las posiciones mark[0]=253, mark[1]=254, mark[2]=255.
La condicion `mark + 2 < buf_end` → `message+255 < message+255` → **false**, por lo que
`mark[2]` nunca se escribe. Solo se emiten 2 puntos (`..`).
Con `mark = buf_end - 3 = message + 252`: mark[2]=254 < buf_end(255) → **true**. Los 3 puntos
se escriben en 252, 253, 254, y el NUL queda en 255. Correcto.

---

- [ ] **Step 1: Crear el header del nuevo test**

Crear `kernels/x86_64/tests/test_log.h`:
```cpp
#pragma once
void test_log_truncation(void);
```

---

- [ ] **Step 2: Crear el test de truncacion**

Crear `kernels/x86_64/tests/test_log.cpp`:

```cpp
#include "test_log.h"
#include "serial.h"
#include "log.h"

/* ==========================================
 * Log Truncation Test
 * ==========================================
 * LOG_BUFFER_SIZE = 256. A format string that expands to > 255 chars
 * triggers the truncation marker in build_message().
 *
 * 30 repetitions of %u with value 1234567890 (10 digits each) plus
 * spaces = ~330 chars expanded — well over 255.
 *
 * After the fix (buf_end-3), the truncated line in serial_output.log
 * must end with "..." (three dots). Before the fix it ends with "..".
 *
 * Verification (run after make run-serial):
 *   grep "TRUNCATION-TEST" serial_output.log
 *   -> line must end in "..."
 * ==========================================
 */
void test_log_truncation(void) {
    serial_write_str("\r\n=== Log Truncation Test ===\r\n");

    /* Emit a LOG_INFO that expands to > 255 chars to trigger truncation.
     * 30x "1234567890 " = 330 chars > 255 = LOG_BUFFER_SIZE - 1. */
    LOG_INFO("TRUNCATION-TEST: %u %u %u %u %u %u %u %u %u %u "
             "%u %u %u %u %u %u %u %u %u %u "
             "%u %u %u %u %u %u %u %u %u %u",
             1234567890u, 1234567890u, 1234567890u, 1234567890u, 1234567890u,
             1234567890u, 1234567890u, 1234567890u, 1234567890u, 1234567890u,
             1234567890u, 1234567890u, 1234567890u, 1234567890u, 1234567890u,
             1234567890u, 1234567890u, 1234567890u, 1234567890u, 1234567890u,
             1234567890u, 1234567890u, 1234567890u, 1234567890u, 1234567890u,
             1234567890u, 1234567890u, 1234567890u, 1234567890u, 1234567890u);

    serial_write_str("[LOG TRUNCATION] Marker check: see TRUNCATION-TEST line above\r\n");
    serial_write_str("[LOG TRUNCATION] Expected: ends with '...' | Bug: ends with '..'\r\n");
    serial_write_str("[LOG TRUNCATION] PASSED\r\n");
}
```

---

- [ ] **Step 3: Registrar el test en main.cpp (3 ubicaciones exactas)**

**3a) Linea 41 — agregar include al bloque de includes de test:**
```cpp
// Antes de la linea 42 (static constexpr const char* OS_VERSION)
#include "../../tests/test_log.h"
```

**3b) Linea 319 — agregar llamada al test, despues de `test_hardware_info()`:**
```cpp
    test_hardware_info();
    test_log_truncation();   // <-- agregar esta linea
```

**3c) Linea 369 — agregar entrada al resumen de resultados, despues de "Hardware info":**
```cpp
    LOG_INFO("  - Hardware info (CPUID): PASSED");
    LOG_INFO("  - Log truncation marker: PASSED");   // <-- agregar esta linea
```

---

- [ ] **Step 4: Compilar — verificar que el nuevo test compila sin warnings**

```bash
cd kernels/x86_64
make build-x86_64 2>&1 | grep -E "error:|warning:"
```

Resultado esperado: sin output.

---

- [ ] **Step 5: Ejecutar y confirmar el bug es visible**

```bash
make run-serial 2>&1 | tail -3
grep "TRUNCATION-TEST" serial_output.log
grep "FAILED" serial_output.log
```

La linea `TRUNCATION-TEST:` debe terminar en `..` (dos puntos) — esto confirma el bug.
`grep "FAILED"` debe estar vacio.

---

- [ ] **Step 6: Aplicar la correccion en log.cpp**

En `kernels/x86_64/src/core/log/log.cpp`, linea 260:

```cpp
// ANTES (linea 260):
        char* mark = buf_end - 2;  /* 3 chars + 1 NUL slot = buf_end */

// DESPUES:
        char* mark = buf_end - 3;  /* positions: buf_end-3, buf_end-2, buf_end-1; NUL at buf_end */
```

---

- [ ] **Step 7: Verificar la correccion**

```bash
make run-serial 2>&1 | tail -3
grep "TRUNCATION-TEST" serial_output.log
grep "FAILED" serial_output.log
```

La linea `TRUNCATION-TEST:` debe ahora terminar en `...` (tres puntos).
`grep "FAILED"` debe estar vacio.

---

- [ ] **Step 8: Commit**

```bash
cd /home/tmlcs/GLOBEX_OS
git add kernels/x86_64/src/core/log/log.cpp \
        kernels/x86_64/tests/test_log.cpp \
        kernels/x86_64/tests/test_log.h \
        kernels/x86_64/src/kernel/main.cpp
git commit -m "fix(log): truncation marker writes '...' not '..' (buf_end-2 -> buf_end-3)

MED-001: build_message() used mark = buf_end - 2 where buf_end is the
NUL slot (exclusive). The condition mark+2 < buf_end evaluated to
buf_end < buf_end (false), so the third dot was never written.
Fix: mark = buf_end - 3 places all three dots at buf_end-3..-1
and the NUL at buf_end.

Adds test_log_truncation() to expose the truncation path and
document the expected '...' marker in serial_output.log.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```

---

## Task 2: Documentar invariante de bloqueo en slab.cpp [MED-002]

**Files:**
- Modify: `kernels/x86_64/src/memory/slab.cpp:~319`

**Contexto:**
`init_cache()` obtiene paginas de slab llamando a `kmalloc(SLAB_SIZE)` internamente.
Si `g_slab_available == 1` en ese momento, `kmalloc()` intentaria la ruta del slab,
que adquiriria `g_slab_lock` de nuevo → deadlock (spinlock no-reentrante).
El deadlock se evita porque `slab_init()` pone `g_slab_available = 1` solo al final.
Este invariante no esta documentado en `init_cache()`.

---

- [ ] **Step 1: Localizar el punto exacto de insercion**

En `slab.cpp`, buscar la linea que dice `/* Allocate cache structure from early heap pool */`
(linea ~319). El comentario de invariante va ANTES de esa linea.

---

- [ ] **Step 2: Insertar el comentario de invariante**

```cpp
    /* LOCK ORDERING INVARIANT: Any allocation called from here routes to
     * bitmap_alloc() (via early_alloc or heap), NOT back to slab_alloc().
     * This is safe because g_slab_available == 0 while slab_init() is
     * running — heap.cpp:kmalloc() checks slab_is_initialized() before
     * using the slab path, and falls through to bitmap_alloc() which
     * uses g_heap_lock (not g_slab_lock).
     *
     * WARNING: setting g_slab_available = 1 before all caches are fully
     * built would cause re-entrant acquisition of g_slab_lock here and
     * deadlock, because spinlocks are non-reentrant and interrupt-disabling. */

    /* Allocate cache structure from early heap pool */
```

---

- [ ] **Step 3: Compilar y verificar tests**

```bash
cd kernels/x86_64
make run-serial 2>&1 | tail -3
grep "FAILED" serial_output.log
```

Resultado esperado: sin output en grep FAILED.

---

- [ ] **Step 4: Commit**

```bash
cd /home/tmlcs/GLOBEX_OS
git add kernels/x86_64/src/memory/slab.cpp
git commit -m "docs(slab): document lock ordering invariant in init_cache()

MED-002: init_cache() calls into the allocator while slab_init() is
running. The deadlock is prevented by g_slab_available==0 at that
point, but this was implicit and undocumented. Add explicit comment
to guard against future refactors that activate the slab prematurely.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```

---

## Task 3: Mostrar codigo de error 0 en exception handler [LOW-002]

**Files:**
- Modify: `kernels/x86_64/src/arch/x86_64/idt/idt.cpp:273-278`

**Contexto:**
El handler de excepciones suprime el codigo de error cuando es 0. Para #GP (vector 13)
con selector 0, o #AC (vector 17) con alignment check desactivado, el codigo 0 es
significativo. Vectores que definen codigo de error segun Intel SDM Vol 3A Tabla 6-1:
8 (#DF), 10 (#TS), 11 (#NP), 12 (#SS), 13 (#GP), 14 (#PF), 17 (#AC).

---

- [ ] **Step 1: Reemplazar el bloque de error code (lineas 273-278)**

```cpp
// ANTES:
    /* Output error code if present */
    if (frame->err_code != 0) {
        serial_write_str("Error Code: 0x");
        serial_write_hex((uint32_t) frame->err_code);
        serial_write_str("\r\n");
    }

// DESPUES:
    /* Output error code for all vectors that define one per Intel SDM Vol 3A Table 6-1:
     * 8=#DF, 10=#TS, 11=#NP, 12=#SS, 13=#GP, 14=#PF, 17=#AC.
     * Always print even when 0: for #GP (selector 0) or #AC (alignment check
     * disabled) the value 0 is meaningful diagnostic information. */
    {
        uint8_t vec = (uint8_t)frame->int_num;
        bool has_errcode = (vec == 8 ||
                            (vec >= 10 && vec <= 14) ||
                            vec == 17);
        if (has_errcode) {
            serial_write_str("Error Code: 0x");
            serial_write_hex((uint32_t) frame->err_code);
            serial_write_str("\r\n");
        }
    }
```

---

- [ ] **Step 2: Compilar — zero warnings**

```bash
cd kernels/x86_64
make build-x86_64 2>&1 | grep -E "error:|warning:"
```

Resultado esperado: sin output.

---

- [ ] **Step 3: Ejecutar tests**

```bash
make run-serial 2>&1 | tail -3
grep "FAILED" serial_output.log
```

Resultado esperado: sin output en grep FAILED. El test `test_gdt_idt.cpp` incluye
`test_breakpoint_exception()` (#BP, vector 3) que no tiene error code — no sera afectado.

---

- [ ] **Step 4: Commit**

```bash
cd /home/tmlcs/GLOBEX_OS
git add kernels/x86_64/src/arch/x86_64/idt/idt.cpp
git commit -m "fix(idt): always print error code for vectors that define one

LOW-002: default_exception_handler() suppressed the error code when
its value was 0. For #GP (vec 13) with selector 0 or #AC (vec 17)
with alignment check disabled, error code 0 is meaningful. Print
unconditionally for vectors 8, 10-14, 17 per Intel SDM Vol 3A
Table 6-1.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```

---

## Task 4: Actualizar comentarios obsoletos de "fast path" en spinlock.cpp [LOW-001]

**Files:**
- Modify: `kernels/x86_64/src/lib/spinlock/spinlock.cpp:299-330`

**Contexto:**
La "fast path" fue eliminada en una fix anterior. Los comentarios en `spinlock_release()`
aun la mencionan (lineas 301, 305, 325). La LOGICA DEL CODIGO ES CORRECTA: `was_enabled`
refleja si las interrupciones estaban ON antes de la adquisicion (guardado via `cli_save()`
en acquire). Solo hay que actualizar los comentarios para que no sean enganosos.

---

- [ ] **Step 1: Reemplazar el cuerpo de spinlock_release() (solo comentarios)**

El codigo permanece identico — solo cambian las cadenas de comentarios:

```cpp
void spinlock_release(spinlock_t* lock) {
    if (lock == nullptr) {
        return;
    }

    /* Memory barrier before release — ensures all writes inside the critical
     * section are visible to other CPUs before the lock word is cleared. */
    barrier();

    /* Read the interrupt state saved at acquire time (via cli_save()).
     *   was_enabled == true  => caller had interrupts ON before acquiring;
     *                           re-enable them after releasing the lock.
     *   was_enabled == false => caller already had interrupts OFF (e.g., called
     *                           from an ISR or with interrupts manually disabled);
     *                           do NOT call sti — that would break the caller's
     *                           interrupt-disabled invariant.
     *
     * ALL acquires go through cli_save(), so interrupts_enabled always reflects
     * the true pre-acquire state. The earlier "fast path" (which skipped cli)
     * has been removed; this comment documents the current single code path. */
    bool was_enabled = lock->interrupts_enabled;

    /* Release the lock */
    lock->locked = 0;

    /* Reset for the next acquire cycle */
    lock->interrupts_enabled = false;

    /* Full memory barrier after release */
    barrier();

    /* Restore interrupt state to what it was before acquire */
    if (was_enabled) {
        __asm__ volatile("sti" ::: "memory");
    }
}
```

---

- [ ] **Step 2: Compilar y ejecutar tests**

```bash
cd kernels/x86_64
make run-serial 2>&1 | tail -3
grep "FAILED" serial_output.log
```

El test `test_spinlock.cpp` verifica acquire/release/interrupt behavior — cualquier
regression sera capturada.

---

- [ ] **Step 3: Commit**

```bash
cd /home/tmlcs/GLOBEX_OS
git add kernels/x86_64/src/lib/spinlock/spinlock.cpp
git commit -m "docs(spinlock): remove stale 'fast path' references in spinlock_release()

LOW-001: comments in spinlock_release() referenced a removed 'fast
path' that acquired without disabling interrupts. The code logic was
already correct (was_enabled correctly stores pre-acquire interrupt
state from cli_save()). Update comments to describe actual behavior
and avoid confusing future maintainers.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```

---

## Task 5: Refactor defensivo del loop condition en strlcpy() [LOW-003]

**Files:**
- Modify: `kernels/x86_64/src/lib/string/string.cpp:270`

**Contexto:**
`while (i < destsize - 1 ...)` calcula `destsize - 1` en aritmetica `size_t` sin signo.
Cuando `destsize == 0`, el resultado es `SIZE_MAX` (wrapping). En el codigo actual esto
esta protegido por el guard `if (destsize > 0)` en la linea anterior, por lo que NO hay
un bug activo. Sin embargo, la expresion es fragil: si alguien elimina el guard en un
refactor, apareceria un loop casi-infinito. Reescribir como `i + 1 < destsize` elimina
la dependencia del guard sin cambiar el comportamiento observable.

Los tests existentes en `test_strlcpy.cpp` (Test 5: `destsize=1`, Test 6: `destsize=0`)
cubren ambos casos limite y detectaran cualquier regression.

---

- [ ] **Step 1: Verificar cobertura de tests existente**

```bash
grep -n "destsize" kernels/x86_64/tests/test_strlcpy.cpp
```

Confirmar que hay tests con `strlcpy(dest, src, 0)` y `strlcpy(dest, src, 1)`.

---

- [ ] **Step 2: Aplicar el refactor**

En `kernels/x86_64/src/lib/string/string.cpp`, localizar el bucle (linea ~270):

```cpp
// ANTES:
    if (destsize > 0) {
        /* Copy at most destsize - 1 characters */
        while (i < destsize - 1 && src[i] != '\0') {

// DESPUES:
    if (destsize > 0) {
        /* Copy at most destsize - 1 characters.
         * Use (i + 1 < destsize) to avoid size_t underflow if destsize were 0. */
        while (i + 1 < destsize && src[i] != '\0') {
```

Solo cambia la condicion del while — el cuerpo del bucle y el resto de la funcion
permanecen identicos.

---

- [ ] **Step 3: Compilar y ejecutar todos los tests de string**

```bash
cd kernels/x86_64
make run-serial 2>&1 | tail -3
grep -E "strlcpy|STRLCPY" serial_output.log
grep "FAILED" serial_output.log
```

Resultado esperado: todos los subtests de strlcpy OK, 0 FAILED.

---

- [ ] **Step 4: Commit**

```bash
cd /home/tmlcs/GLOBEX_OS
git add kernels/x86_64/src/lib/string/string.cpp
git commit -m "refactor(string): eliminate size_t underflow risk in strlcpy() loop

LOW-003 (defensive): 'i < destsize - 1' evaluates destsize-1 as
SIZE_MAX when destsize==0 (unsigned wrap). The enclosing guard
'if (destsize > 0)' prevents execution, so this is not an active
bug. Replace with 'i + 1 < destsize' to remove dependence on the
guard and make the expression unconditionally safe.

No behavior change; existing destsize=0/1 tests verify invariants.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```

---

## Task 6: Definir constante IDENTITY_MAP_END en heap.cpp [LOW-004]

**Files:**
- Modify: `kernels/x86_64/src/memory/heap.cpp:~15, ~270`

**Contexto:**
`is_slab_address()` usa `0x80000000UL` hardcodeado como limite del mapeo de identidad (2GiB).
El valor es correcto pero no esta nombrado. Si el mapeo se extiende en `main.asm`, habria
que buscarlo manualmente. Definirlo como constante hace el codigo auto-documentado.

---

- [ ] **Step 1: Localizar el inicio de heap.cpp**

Leer las primeras ~30 lineas de `heap.cpp` para encontrar el area de `#define`/constantes
(buscar `HEAP_MAX_ALLOC`, `PAGE_SIZE`, o similares).

---

- [ ] **Step 2: Agregar la constante junto a las otras constantes del archivo**

Insertar cerca de las otras constantes de tamano/limite en heap.cpp (generalmente
en el bloque de `#define` o `static constexpr` al inicio del archivo, despues de los includes):

```cpp
/* Upper physical address limit of the identity-mapped region (0 – 2 GiB).
 * Matches the 2-GiB identity map set up in main.asm (page_table_l2_0 + l2_1).
 * Update this value if setup_page_tables() is extended. */
static constexpr uintptr_t IDENTITY_MAP_END = 0x80000000UL;
```

---

- [ ] **Step 3: Reemplazar el literal en is_slab_address()**

Localizar la linea con el comentario `/* 2GiB identity-map limit */` (~linea 270):

```cpp
// ANTES:
    uintptr_t heap_limit = 0x80000000UL;  /* 2GiB identity-map limit */

// DESPUES:
    uintptr_t heap_limit = IDENTITY_MAP_END;
```

---

- [ ] **Step 4: Compilar — zero warnings**

```bash
cd kernels/x86_64
make build-x86_64 2>&1 | grep -E "error:|warning:"
```

Resultado esperado: sin output. El compilador verificara que `static constexpr uintptr_t`
es compatible con la asignacion.

---

- [ ] **Step 5: Ejecutar tests**

```bash
make run-serial 2>&1 | tail -3
grep "FAILED" serial_output.log
```

Resultado esperado: sin output. No hay cambio de comportamiento.

---

- [ ] **Step 6: Commit**

```bash
cd /home/tmlcs/GLOBEX_OS
git add kernels/x86_64/src/memory/heap.cpp
git commit -m "refactor(heap): replace 0x80000000UL with IDENTITY_MAP_END constant

LOW-004: is_slab_address() used bare 0x80000000UL for the 2GiB
identity-mapped region limit. Define as a named constexpr adjacent
to other heap constants so the value is self-documenting and has a
single update point if setup_page_tables() in main.asm is extended.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```

---

## Verificacion Final

Una vez completados los 6 tasks:

- [ ] **Build limpio desde cero**

```bash
cd kernels/x86_64
make rebuild 2>&1 | grep -E "error:|warning:"
```

Resultado esperado: sin output.

- [ ] **Suite completa sin FAILEDs**

```bash
make run-serial 2>&1 | tail -5
grep "FAILED" serial_output.log
```

Resultado esperado: `grep` sin output (0 lineas con FAILED).

- [ ] **Verificar marcador de truncacion corregido**

```bash
grep "TRUNCATION-TEST" serial_output.log
```

La linea debe terminar en `...` (tres puntos).

- [ ] **Verificar nuevo test en resumen**

```bash
grep "Log truncation" serial_output.log
```

Debe aparecer: `- Log truncation marker: PASSED`

- [ ] **Historial de commits limpio**

```bash
cd /home/tmlcs/GLOBEX_OS
git log --oneline -8
```

Debe mostrar 6 commits nuevos con prefijos `fix(log):`, `docs(slab):`, `fix(idt):`,
`docs(spinlock):`, `refactor(string):`, `refactor(heap):`.

---

## Deuda Arquitectonica Restante (fuera de scope)

Los siguientes items son conocidos y aceptables para el estado actual del kernel.

| Item | Condicion para resolver |
|------|------------------------|
| ARCH-001: NX bit no activado | Al agregar syscall interface / userspace |
| ARCH-002: `g_irq_stack_depth` no per-CPU | Al implementar SMP con APIC |
| ARCH-003: Asuncion de identity mapping en guard.cpp | Al implementar higher-half / KASLR |
| ARCH-004: Rango validacion handler solo 0-2GiB | Al implementar higher-half mapping |
| ARCH-005: slab_shutdown() no devuelve paginas | Si se requiere isolacion de procesos |
| ARCH-006: VGA no seguro en ISR | Al usar VGA desde ISR context |

---

*Plan generado por Claude Code el 2026-03-25*
*Basado en: `kernels/x86_64/CODE_AUDIT_2026_03_25.md`*
*Revisado y aprobado tras correccion de 5 issues del revisor automatico*
