# Design: Kernel Correctness Fixes — GLOBEX_OS x86_64

**Date:** 2026-03-26
**Branch:** `fix/kernel-correctness`
**Scope:** Critical and high-severity findings from full codebase audit
**Approach:** Sequential phases in dependency order; `make run-serial` between each phase

---

## Context

A full codebase audit identified 4 CRITICAL, 13 HIGH, and 25 MEDIUM findings. The root cause of
three of the four CRITICAL findings is a single architectural defect: the spinlock stores the
per-CPU interrupt flag state inside the shared lock struct, causing STI to fire inside exception
handlers and creating SMP data races on the saved IF state. All other phases build on the
corrected spinlock.

---

## Phase 1 — Spinlock: Token API

### Problem

`spinlock_t.interrupts_enabled` is a field in the shared lock struct. This causes:
- `spinlock_release()` calling `sti()` inside exception/IRQ handlers (re-enables interrupts
  mid-handler before `for(;;) hlt`)
- SMP race: releasing CPU can read a stale `interrupts_enabled` value after the new owner
  overwrites it, permanently disabling interrupts on the releasing CPU

### Design

**Struct change** — remove `interrupts_enabled`:
```c
typedef struct {
    volatile uint64_t locked;
    // interrupts_enabled removed: lives in caller's stack frame via token
} spinlock_t;
```

**New opaque token type:**
```c
typedef struct { uint64_t rflags; } spinlock_token_t;
```

Token holds the full RFLAGS register captured before `cli`. This is a zero-overhead abstraction:
one register at runtime, but typed to prevent accidental misuse (no hardcoded `true`/`false`).

**New API:**
```c
spinlock_token_t spinlock_acquire(spinlock_t* lock);
void             spinlock_release(spinlock_t* lock, spinlock_token_t tok);
```

**Acquire implementation:**
1. `pushfq; pop tok.rflags` — capture RFLAGS including IF
2. `cli` — disable interrupts
3. CAS spin loop on `lock->locked`
4. Compiler barrier
5. Return `tok`

**Release implementation:**
1. Compiler barrier
2. `lock->locked = 0`
3. Compiler barrier
4. If `tok.rflags & 0x200` (IF was set before acquire) → `sti`

**`SPINLOCK_INIT`:** `{0}` — remains valid; only `locked` field remains.

**Migration:** ~40 call sites updated cleanly, no compatibility wrappers. Each:
```c
// Before
spinlock_acquire(&lock);
spinlock_release(&lock);

// After
spinlock_token_t tok = spinlock_acquire(&lock);
spinlock_release(&lock, tok);
```

**Files affected:**
- `src/lib/spinlock/spinlock.h` — struct, typedef, declarations
- `src/lib/spinlock/spinlock.cpp` — implementation
- All callers: `vga.cpp`, `serial.cpp` (via header), `log.cpp`, `slab.cpp`, `heap.cpp`,
  `bitmap.cpp`, `irq.cpp`, `pit.cpp`, `coverage.cpp`

---

## Phase 2 — Fault Path: Defense-in-Depth Output

### Problem

`panic()` and `default_exception_handler()` call `serial_write_str()`, which acquires
`g_serial_lock`. Due to the spinlock design (now fixed in Phase 1), `spinlock_release` could
call `sti` mid-handler. Even after Phase 1, using normal locking in fault paths is dangerous:
if the fault occurred while `g_serial_lock` was held, `serial_write_str` spins forever
(deadlock with IF=0). Similarly, calling `print_str` while `g_vga_lock` is held deadlocks the
VGA output path.

### Design

**New function:** `serial_write_unsafe(const char* str)`

- Declared in `src/drivers/serial/serial.h`
- Implemented in `src/drivers/serial/serial.cpp`
- Writes directly to UART port 0x3F8 via polling (`inb(0x3F8+5)` waits for THRE bit)
- No spinlock, no shared buffer, no state mutation
- For use **only** in `panic()` and `default_exception_handler()`

```c
void serial_write_unsafe(const char* str) {
    while (*str) {
        while (!(inb(0x3F8 + 5) & 0x20)) {}   // wait for transmitter empty
        outb(0x3F8, (uint8_t)*str++);
    }
}
```

**New function:** `vga_force_unlock()` in `src/drivers/vga/vga.cpp`

- Analogous to existing `serial_force_unlock()`
- Resets `g_vga_lock` to unlocked state without restoring IF
- Called at the top of `default_exception_handler()` before any VGA output

**Changes to `panic()`** (`src/core/panic/panic.cpp`):
- Keep `serial_force_unlock()` at start
- Replace all `serial_write_str()` calls with `serial_write_unsafe()`
- VGA path uses `vga_put_string_early()` (already lockless — no change needed)

**Changes to `default_exception_handler()`** (`src/arch/x86_64/idt/irq.cpp`):
- Call `serial_force_unlock()` (already present — keep)
- Add `vga_force_unlock()` before VGA output section
- Replace all `serial_write_str()` calls with `serial_write_unsafe()`

---

## Phase 3 — Memory Subsystem

### 3a. Slab list corruption (HIGH-1)

**Problem:** `kmem_free` determines which list a slab is in by heuristic (`num_objects == 1`
→ assume `full`, else assume `partial`). This is wrong: a slab in `partial` with `num_free`
transitioning to `num_objects` via a series of frees would be removed from the wrong list,
setting `target_cache->full = NULL` and destroying all full slabs.

**Fix:** Add `list_state` field to `slab_t`:
```c
typedef enum : uint8_t {
    SLAB_LIST_FREE    = 0,
    SLAB_LIST_PARTIAL = 1,
    SLAB_LIST_FULL    = 2,
} slab_list_state_t;
```

Update `list_state` in every `add_slab_to_list` / `remove_slab_from_list` call site.
In `kmem_free`, use `slab->list_state` to dispatch to the correct list — no heuristic needed.

### 3b. krealloc use-after-free (HIGH-2)

**Problem:** `krealloc` reads `slab->object_size` without holding `g_slab_lock`. On SMP, a
concurrent `kmem_free` on the same pointer can recycle the slab page between `is_slab_address`
and `memcpy`.

**Fix:** Wrap the `is_slab_address` check and `old_size` read with `g_slab_lock`:
```c
spinlock_token_t tok = spinlock_acquire(&g_slab_lock);
bool is_slab = is_slab_address(ptr);
size_t old_size;
if (is_slab) {
    const slab_t* s = reinterpret_cast<const slab_t*>(
        (uintptr_t)ptr & ~(uintptr_t)(SLAB_SIZE - 1));
    old_size = s->object_size;
} else {
    old_size = kmalloc_size(ptr);
}
spinlock_release(&g_slab_lock, tok);
// kmalloc + memcpy + kfree outside lock
```
On the current uniprocessor kernel, spinlock disables interrupts, making the window between
release and `kfree` safe. The lock acquisition is still added for correctness and SMP-readiness.

### 3c. uint16_t page count — missing compile-time guard (HIGH-3)

Add to `src/memory/heap.cpp`:
```c
static_assert(HEAP_MAX_ALLOC / PAGE_SIZE <= 65535,
    "HEAP_MAX_ALLOC exceeds uint16_t capacity in g_page_alloc_count");
```

### 3d. Bitmap phantom pages (M-01)

`BITMAP_WORDS = (BITMAP_SIZE_BYTES + 7) / 8` produces one extra word when `TOTAL_PAGES` is
divisible by 64. Fix: compute `BITMAP_WORDS = (TOTAL_PAGES + 63) / 64` directly. In
`bitmap_count_free_pages`, mask the last word's unused bits before `popcount64`.

---

## Phase 4 — Interrupt Handling

### 4a. NMI permanently blocked after first NMI (HIGH-4)

**Problem:** `isr2` calls `default_exception_handler` which enters `for(;;) hlt`. On x86_64,
NMI delivery is re-armed only when the NMI handler executes `iretq`. The `hlt` loop never does
this, permanently blocking all future NMIs.

**Fix:** In `irq.cpp`, add a dedicated NMI handler alongside the existing exception dispatch
logic. The `irq_common_handler` (or equivalent C handler) currently routes all exceptions to
`default_exception_handler`. Add a specific branch for `int_num == 2` before the generic
fallthrough:
```c
// In the C exception dispatch function in irq.cpp:
if (frame->int_num == 2) {
    serial_write_unsafe("[NMI] Non-maskable interrupt — continuing\r\n");
    return;  // allows iretq in stub → re-arms NMI delivery
}
// ... existing default_exception_handler call for all other vectors
```
The `isr2` stub in `interrupts.asm` already ends with `iretq`; the only change needed is
ensuring the C handler returns instead of entering `for(;;) hlt`.

### 4b. Spurious IRQ EOI corruption (HIGH-5)

**Problem:** `irq_send_eoi` sends EOI unconditionally. For spurious IRQ15, sending EOI to the
slave PIC clears an unrelated pending interrupt.

**Fix:** Read the PIC In-Service Register before sending EOI:
```c
static bool irq_is_spurious(uint8_t irq) {
    if (irq == 7) {
        outb(PIC1_COMMAND, 0x0B);              // OCW3: read ISR
        return (inb(PIC1_COMMAND) & 0x80) == 0;
    }
    if (irq == 15) {
        outb(PIC2_COMMAND, 0x0B);
        if (inb(PIC2_COMMAND) & 0x80) return false;
        outb(PIC1_COMMAND, PIC_EOI);           // EOI only to master (IRQ2 cascade)
        return true;
    }
    return false;
}
```
In `irq_dispatch`: if `irq_is_spurious(irq)` returns `true`, skip calling the handler and skip
the normal `irq_send_eoi`.

### 4c. CS not reloaded after LGDT (HIGH-6)

**Problem:** `gdt_load` returns after `lgdt` without reloading CS. The cached CS selector from
GRUB coincidentally matches `GDT_SELECTOR_KERNEL_CODE = 0x08`, but this is fragile.

**Fix:** far return in `gdt.asm` to flush the CS descriptor cache:
```asm
gdt_load:
    lgdt [rdi]
    push qword 0x08           ; kernel code selector
    lea  rax, [rel .cs_flush]
    push rax
    retfq                     ; far return: reloads CS = 0x08
.cs_flush:
    ret
```

### 4d. `IDT_USER_INTERRUPT = 0x8` — P-bit zero (HIGH-7)

Remove the constant from `src/core/constants.h`. It is unused in the codebase and would cause
`#NP` (exception 11) if any gate were configured with it (P=0 means gate not present).

---

## Phase 5 — Drivers and Log

### 5a. PIT frequency clamping stores wrong value (M-03)

After clamping divisor, recompute and store the actual hardware frequency:
```c
if (divisor > 0xFFFF) divisor = 0xFFFF;
if (divisor == 0)     divisor = 1;
g_pit_state.frequency_hz = PIT_BASE_FREQUENCY / divisor;  // actual, not requested
g_pit_state.divisor       = divisor;
```

### 5b. `memcmp` null contract mismatch (M-04)

Header documents "returns 0 if either pointer is NULL". Implementation panics.
Fix: replace `PANIC_IF_FALSE` with a null guard that returns 0.

### 5c. Format specifier documentation correction (L-04)

Update `CLAUDE.md` format specifier table to reflect actual `log.cpp` implementation:

| Specifier | Status |
|-----------|--------|
| `%u`, `%d` | supported |
| `%llu`, `%lld` | supported (64-bit) |
| `%p` | supported |
| `%zu` | supported |
| `%s`, `%x`, `%X` | supported |
| `%lu` | NOT supported — produces incorrect output silently |

### 5d. `log_hex_dump` 32-bit pointer truncation (M-09)

Replace `(uint32_t)(i + (size_t)bytes)` with `(uint64_t)((uintptr_t)bytes + i)` and use
`append_hex64` for the offset label.

---

## Phase 6 — Tests

### 6a. Three test files compiled but never called (HIGH-9/10)

Add to `src/kernel/main.cpp`:
```cpp
#include "../../tests/test_spinlock_stress.h"
#include "../../tests/test_slab_debug.h"
#include "../../tests/test_string_boundaries.h"
```
And call `test_spinlock_stress()`, `test_slab_debug_all()`, `test_string_boundaries()`,
`test_buffer_overflow_prevention()` in the appropriate order after existing memory/spinlock tests.

Fix `tests/test_spinlock_stress.cpp`: add `#include "../../src/drivers/console/print.h"`.

### 6b. `g_test_failed` shared state — missing resets (HIGH-11)

Add `TEST_RESET_FAILURE()` at the top of each sub-function in `tests/test_spinlock.cpp` that
calls `TEST_ASSERT` but does not currently reset the global:
- `test_spinlock_acquire_release()`
- `test_spinlock_try_acquire()`
- `test_spinlock_null_pointer_safety()`
- `test_spinlock_interrupt_state()`

### 6c. `test_pit_init` vacuously true tick=0 assertion (HIGH-12)

Move the `pit_init()` reset call from `test_pit_all()` into `test_pit_init()` only (as its
local setup step). All other sub-tests (`test_pit_ticks`, `test_pit_irq`, etc.) operate on the
live counter without resetting it, reflecting actual kernel state.

### 6d. `test_log_truncation` unconditional PASSED (M-13)

Two-part fix:

1. **Compile-time guard:** wrap the test body with `#if LOG_MAX_LEVEL <= LOG_LEVEL_INFO` so
   that when `LOG_INFO` is compiled out, the test prints `[SKIPPED at this LOG_LEVEL]` instead
   of PASSED.

2. **Programmatic verification:** expose a thin internal helper in `log.h`:
   ```c
   // Format into caller-supplied buffer; returns number of chars written (excluding null).
   // Truncates with "..." if output exceeds cap-1 chars, same as log_output internal path.
   size_t log_format_to_buf(char* dst, size_t cap, const char* fmt, ...);
   ```
   The test calls `log_format_to_buf` with a buffer smaller than the format output, then
   asserts `dst[strlen(dst)-3..dst[strlen(dst)-1] == "..."` before printing PASSED. This
   tests the actual truncation logic without needing to intercept serial output.

---

## Git Strategy

- Branch: `fix/kernel-correctness` (from `main`)
- One commit per phase, message format: `fix(<subsystem>): <description>`
- `make run-serial` after each commit; serial_output.log must show all tests PASSED

## Testing Criteria

Each phase passes when:
1. `make build-x86_64` completes with zero warnings (all warnings are errors)
2. `make run-serial` produces `serial_output.log` with no `FAILED` lines
3. All previously passing tests continue to pass

## Non-goals

- SMP support (kernel is uniprocessor; SMP-readiness comments are noted but not implemented)
- Fixing LOW severity findings (deferred)
- Modifying the linker script `.boot.data` writability (architectural limitation, documented)
