# GLOBEX_OS Code Quality Audit — 2026-03-25 (v2)

**Scope:** Full codebase — arch, memory, drivers, core, lib, kernel/main, tests, build system  
**Method:** 7 parallel specialist subagents, each auditing one subsystem  
**Build state at audit:** all tests PASSED, 0 FAILED (commit `87fed38`)

---

## Summary Table

| Subsystem | CRIT | HIGH | MED | LOW | Total |
|-----------|------|------|-----|-----|-------|
| Build system | 2 | 6 | 6 | 10 | 24 |
| Memory | 0 | 2 | 6 | 7 | 15 |
| Lib (string/spinlock/utils) | 2 | 1 | 3 | 5 | 11 |
| Core (log/panic/stdint) | 1 | 4 | 5 | 8 | 18 |
| Drivers (VGA/serial/PIT) | 0 | 3 | 5 | 8 | 16 |
| Arch (boot/GDT/IDT/IRQ) | 1 | 5 | 6 | 10 | 22 |
| Kernel main + Tests | 2 | 7 | 7 | 10 | 26 |
| **TOTAL** | **8** | **28** | **38** | **58** | **132** |

---

## CRITICAL Findings (8)

### BUILD-CRIT-001 — `linker.ld:116` — `__kernel_end` assigned outside `SECTIONS`
`__kernel_end = .;` is placed after the closing brace of `SECTIONS` and after the ASSERT block. Outside `SECTIONS`, `.` is implementation-defined and may not equal the true post-layout VMA. `bitmap_init()` reads `&__kernel_end` to determine the first free physical page; an incorrect symbol value causes bitmap allocation to overlap kernel .bss or .boot.data, producing silent memory corruption.  
**Fix:** Move `__kernel_end = .;` inside `SECTIONS`, immediately after the last output section.

### BUILD-CRIT-002 — `linker.ld:85-91` — `.boot.data` lacks `(NOLOAD)` directive
`.boot.data` holds the boot page tables and kernel stack set up by assembly. Without `(NOLOAD)` the linker assigns it to a `PT_LOAD` segment with file backing. GRUB overwrites those pages with file content (zeros) before the CPU enters long mode, destroying the page tables. The boot assembly re-initialises everything from scratch, so this is survivable today, but the section is semantically wrong and any content written before GRUB loading would be lost.  
**Fix:** Add `(NOLOAD)` to the `.boot.data` section declaration.

### CORE-CRIT-001 — `log.cpp:460-463` — `LOG_PANIC` halt path does not disable interrupts
`log_output()` releases `g_log_lock` then enters the `hlt` loop with no `cli`. Any IRQ that fires between the unlock and `hlt` can call `LOG_*`, re-entering the log system. The CPU is not truly halted — the PIT keeps waking it every 10 ms. `panic()` correctly issues `cli` first; `LOG_PANIC` must too.  
**Fix:** Add `__asm__ volatile("cli")` before the `hlt` loop in `log_output`.

### LIB-CRIT-001 — `decimal_utils.h:62-93` — NULL check after buffer write in `uint32_to_decimal_string`
`buffer[0]` and `buffer[1]` are written at line 65 (zero fast-path) and line 88 (bounds-fail path) before the NULL guard at line 93. Passing `buffer=NULL` causes an immediate null-pointer write, producing a kernel triple fault or silent address-0 corruption.  
**Fix:** Move the NULL check to the very top of the function, before any write.

### LIB-CRIT-002 — `decimal_utils.h:153-184` — Same NULL-after-write bug in `uint64_to_decimal_string`
Identical issue at lines 156 and 179 vs guard at line 184.  
**Fix:** Same — move NULL check to function top.

### ARCH-CRIT-001 — `idt.cpp:179-196` — Silent gate drop on invalid handler address
`idt_set_gate` validates the handler address and returns silently if it fails, leaving the IDT vector as a not-present all-zero entry. If that vector fires, the CPU triple-faults. There is no error log or panic from `idt_set_gate` to alert that a vector is unprotected.  
**Fix:** Replace `return;` with `PANIC("idt_set_gate: invalid handler %lu for vector %u", addr, vector)`.

### TEST-CRIT-001 — `main.cpp:294-298` — PIT tests run before `early_alloc_init_default()`
`test_pit_all()` is called at line 292 but `early_alloc_init_default()` is at line 295 and `heap_init()` at line 298. Any future test added between lines 292 and 295 that calls `kmem_alloc` will crash. Additionally, `test_pit_all()` resets the tick counter via `pit_init()`, discarding ticks accumulated during boot init.  
**Fix:** Move `test_pit_all()` to after `heap_init()`.

### TEST-CRIT-002 — `test_memory_manager.cpp:682` — `test_memory_manager()` never called from `kernel_main()`
The most comprehensive memory test suite (726 lines) is compiled but never executed. Any regression in `kmalloc`/`krealloc`/`kcalloc`/`kmem_free_auto` goes undetected at boot.  
**Fix:** Add `test_memory_manager();` to the test sequence in `kernel_main()`.

---

## HIGH Findings (28)

### Memory

**MEM-HIGH-001** `slab.cpp:598-599` — Fully-exhausted 2048-byte slab placed in `partial` instead of `full`  
When a newly-created slab for the 2048-byte cache is immediately exhausted (`objects_per_slab == 1`), the slab is added to `cache->partial`. On the subsequent free, `kmem_free` looks for the slab in `cache->full`, corrupts the full-list pointer, and leaves a dangling `partial` entry pointing at freed memory. Next `kmem_alloc(2048)` dereferences freed memory.  
**Fix:** After the allocation loop, check `slab->num_free == 0` and add to `full` instead of `partial`.

**MEM-HIGH-002** `slab.cpp:559` — `kmalloc()` called while holding `g_slab_lock`  
`kmem_alloc` holds `g_slab_lock` when it calls `kmalloc(SLAB_SIZE)` to expand a cache. Currently safe because `g_slab_available` prevents re-entry, but one edit (setting `g_slab_available = 1` earlier) causes a non-reentrant spinlock deadlock.  
**Fix:** Release `g_slab_lock` before calling `kmalloc`, re-acquire after, then re-check slab state.

### Drivers

**DRV-HIGH-001** `serial.cpp:348-351` — Lock-order inversion: serial lock → VGA lock  
`serial_wait_transmit_empty_timeout` calls `print_str` (VGA) while `g_serial_lock` is held. If any other code path holds `g_vga_lock` and calls serial, the two locks are acquired in opposite orders — potential deadlock.  
**Fix:** Remove VGA calls from `serial_wait_transmit_empty_timeout`; record the error in `g_serial_state` only.

**DRV-HIGH-002** `serial.cpp:542-553` — Non-atomic signed decimal write  
`serial_write_dec_signed` writes `-` then the digits under two separate lock acquisitions. Another CPU can interleave output between the two, splitting `-` from its digits.  
**Fix:** Hold `g_serial_lock` across both writes.

**DRV-HIGH-003** `serial.cpp:223` — FCR written as `0x07`; comment claims 14-byte threshold  
`0x07` selects a 1-byte FIFO trigger level, not 14-byte. No named FCR bit constants exist.  
**Fix:** Define `SERIAL_FCR_*` constants and set the correct trigger level.

### Core

**CORE-HIGH-001** `log.cpp:171-175` — Signed overflow UB on `INT32_MIN` negation in `%d` handler  
`val = -val` when `val == INT32_MIN` is undefined behaviour. The 64-bit path uses the safe `-(val+1)+1` idiom; the 32-bit `%d` path does not.  
**Fix:** Apply the same `(uint32_t)(-(val + 1)) + 1` idiom.

**CORE-HIGH-002** `log.cpp:534` — Pointer truncated to `uint32_t` in `log_hex_dump`  
`(uint32_t)(i + (size_t)bytes)` discards the upper 32 bits of a 64-bit kernel address. All hex dump row offsets are displayed incorrectly.  
**Fix:** Use `append_hex64` with a `uint64_t` cast.

**CORE-HIGH-003** `stdint.h:174-181` — `INT_FAST16/32` and `UINT_FAST16/32` limits wrong  
`int_fast16_t` and `int_fast32_t` are typedef'd to `long` (64-bit) but their limit macros are `INT16_MIN/MAX` and `INT32_MIN/MAX`. Range checks using these macros silently use wrong bounds.  
**Fix:** Define `INT_FAST16_MIN/MAX` and `INT_FAST32_MIN/MAX` equal to `INT64_MIN/MAX`; `UINT_FAST16/32_MAX` equal to `UINT64_MAX`.

**CORE-HIGH-004** `panic.h:4-5` — Angle-bracket `#include <stddef.h>` / `<stdint.h>`  
Uses system headers in a freestanding kernel. Should use `"stddef.h"` / `"stdint.h"` (kernel's own). Same issue in `constants.h:4` and `barriers.h:23-24`.  
**Fix:** Replace with quoted includes throughout.

### Lib

**LIB-HIGH-001** `spinlock.cpp:313-322` — Race on `lock->interrupts_enabled` during release  
After `lock->locked = 0` (line 316), another CPU can acquire the lock and write `lock->interrupts_enabled`. The releasing CPU then overwrites that field with `false` at line 322, destroying the new owner's saved interrupt state. When the new owner releases, it restores a wrong interrupt state.  
**Fix:** Clear `lock->interrupts_enabled` before writing `lock->locked = 0`, so the field is reset before the lock is visible to other CPUs.

### Arch

**ARCH-HIGH-001** `gdt.h:216` — Data segment with `G=0` gives 16 MB limit  
`GDT_GRANULARITY_DATA = 0x00` (G=0) with limit `0xFFFFFFFF` produces an effective 16 MB limit, not 4 GB. Inconsequential in pure 64-bit mode (segment limits not enforced for data), but wrong for any 32-bit compatibility code.  
**Fix:** Set `G=1` in the data segment granularity byte.

**ARCH-HIGH-002** `irq.cpp:303` — `g_irq_stack_depth` is a single global; SMP-unsafe  
On SMP, concurrent IRQ dispatch on different CPUs races on the global counter, potentially swallowing IRQs (false overflow) or missing the depth guard entirely.  
**Fix:** Make the counter per-CPU, or document as UP-only with a `static_assert`.

**ARCH-HIGH-003** `idt.cpp:364-365` — `#BP` (vector 3) and `#DB` (vector 1) registered as interrupt gates  
Intel SDM recommends trap gates (type `0x8F`) for debug exceptions so RFLAGS.IF is preserved. With interrupt gates, all IRQs are silently held off during breakpoint handling.  
**Fix:** Register vectors 1 and 3 with `IDT_TRAP_GATE = 0x8F`.

**ARCH-HIGH-004** `irq.cpp:325-329` — Asymmetric barriers in `irq_dispatch`  
`irq_register_handler` uses `mb()` on both sides of the store. `irq_dispatch` loads `g_irq_handlers[irq]` with no barrier, creating an asymmetric acquire/release pattern.  
**Fix:** Add an `mb()` (or acquire barrier) before reading `g_irq_handlers[irq]` in `irq_dispatch`.

**ARCH-HIGH-005** `barriers.h:23-24` — Angle-bracket includes in freestanding header  
`#include <stdbool.h>` and `#include <stdint.h>` in a freestanding kernel should use quoted kernel headers.  
**Fix:** Replace with `#include "stdbool.h"` / `#include "stdint.h"`.

### Tests

**TEST-HIGH-001** `test_gdt_idt.cpp:205` — `test_breakpoint_exception()` is a vacuous smoke test  
Unconditionally prints PASSED with no assertion on exception frame contents (int_num, CS, RIP advancement).  
**Fix:** Assert `int_num == 3` and `rip_after == rip_before + 1` in the `#BP` handler.

**TEST-HIGH-002** `test_spinlock.cpp:193-233` — `test_spinlock_interrupt_state()` never checks IF  
Claims to verify interrupt state preservation but makes no assertion that interrupts are actually disabled during acquisition.  
**Fix:** Read RFLAGS inside the critical section and assert IF=0.

**TEST-HIGH-003** `test_pit.cpp:209-225` — IRQ counter check uses `>=` not `>`  
Passes even if zero IRQ0 interrupts fired during the 50 ms wait.  
**Fix:** Change to `current_count > initial_count`.

**TEST-HIGH-004** `test_pit.cpp:131-176` — Tick wait timeout is uncalibrated  
Loop counter 1,000,000 is not tied to real time; can falsely fail on fast hardware.  
**Fix:** Use `pit_wait_ms(20)` followed by a real tick-count assertion.

**TEST-HIGH-005** `test_slab.cpp:324-343` — `test_slab_statistics()` unconditionally passes  
Calls `test_pass("Stats tracked")` without verifying any statistic.  
**Fix:** Assert `slab_get_state()->total_allocs > 0` and `total_frees > 0`.

**TEST-HIGH-006** `test_slab.cpp:368-369` — `test_slab_direct_api()` disabled with stale comment  
Comment says "only 32-byte supported" but 64-byte and all other caches work.  
**Fix:** Re-enable the test; remove the stale comment.

**TEST-HIGH-007** `test_spinlock_smp.cpp` — Entire `test_spinlock_smp()` not called from `kernel_main()`  
SMP spinlock tests compile but never run.  
**Fix:** Add `test_spinlock_smp();` to `kernel_main()`.

---

## MEDIUM Findings (selected — 38 total)

| ID | File | Description |
|----|------|-------------|
| BUILD-MED-001 | Makefile | No `-MMD -MP` dependency tracking; header changes don't trigger recompile |
| BUILD-MED-002 | Makefile | `-I src/core/coverage` missing from CFLAGS include paths |
| BUILD-MED-003 | .gitignore | `compile_commands.json` at repo root not gitignored; absolute host paths committed |
| BUILD-MED-005 | ci-cd.yml | CI only triggers on `dev`/`main`/`master`; `feature/*` branches get no CI |
| BUILD-MED-006 | Makefile | `clean` does not remove ISO; stale ISO survives `make clean` |
| MEM-MED-001 | bitmap.cpp:496 | `bitmap_dump_region`: underflow when `count==0` |
| MEM-MED-005 | slab.cpp:582 | Free-list loop wraps if `objects_per_slab==0` (no runtime guard) |
| MEM-MED-006 | guard.cpp:94 | Only guard page TLB entry flushed after splitting 2 MiB huge page |
| CORE-MED-001 | log.cpp:167 | `va_list` passed by value — accidentally correct on x86_64 GCC, UB per standard |
| CORE-MED-004 | log.h:67 | Default `LOG_MAX_LEVEL` is DEBUG (0), not INFO (1) as documented |
| CORE-MED-005 | constants.h:161 | `IDT_USER_INTERRUPT 0x8` is not a valid 64-bit gate attribute (should be `0xEE`) |
| LIB-MED-001 | spinlock.h:114 | `SPINLOCK_GUARD` macro unsafe in `if`/`for` without braces |
| LIB-MED-002 | string.h:56 vs string.cpp:180 | `memcmp` contract mismatch: header says returns 0 on NULL, impl panics |
| DRV-MED-002 | pit.cpp:252 | `pit_irq_handler` reads `frequency_hz` without disabling IRQs — race window |
| DRV-MED-003 | pit.cpp:120 | `pit_shutdown()` leaves PIT initialized (`initialized=1`) |
| DRV-MED-005 | serial.cpp:435 | `serial_lock()` + `serial_write_str()` deadlocks (non-reentrant spinlock) |
| ARCH-MED-003 | irq.cpp:86 | PIC mask save/restore in `pic_remap_impl` is vestigial and misleading |
| ARCH-MED-004 | main.asm:285 | `.boot.data` uninitialized on real hardware (GRUB may not zero it) |
| TEST-MED-001 | test_log.cpp | `test_log_truncation()` always prints PASSED; cannot signal FAILED |
| TEST-MED-006 | main.cpp:343 | Final summary unconditionally prints PASSED for all tests regardless of failures |
| TEST-MED-007 | test_pit.cpp:299 | `test_pit_all()` calls `pit_init()` a second time, resetting uptime ticks |

---

## LOW Findings (58 total — key items)

| ID | File | Description |
|----|------|-------------|
| BUILD-LOW-001 | Makefile:163 | `grub-mkrescue` hardcodes Debian path `/usr/lib/grub/i386-pc` |
| BUILD-LOW-003 | Makefile:29 | `QEMU_SERIAL_FLAGS` defined but never used |
| BUILD-LOW-005 | linker.ld:109 | NASM `-g` emits DWARF; not discarded by linker — inflates kernel ELF |
| BUILD-LOW-007 | .clang-tidy | `WarningsAsErrors: ''` — tidy findings never fatal |
| CORE-LOW-002 | panic.h | `panic()` lacks `[[noreturn]]` / `__attribute__((noreturn))` |
| CORE-LOW-003 | constants.h:321 | `ICW3_MASTER_MASK` misnamed (ICW3 is cascade config, not mask) |
| LIB-LOW-003 | hex/decimal_utils.h | `inline` inside `extern "C"` block — misleading, `extern "C"` is inert for inline functions |
| LIB-LOW-005 | decimal_utils.h | Both decimal functions return interior pointer `&buffer[i]`, not `buffer` — asymmetric API |
| ARCH-LOW-001 | main64.asm:43 | BSS zeroing rounds up to qword; can zero up to 7 bytes past `__bss_end` |
| ARCH-LOW-002 | idt.h:439 | `default_irq_handler` declared but never defined or called |
| ARCH-LOW-003 | irq.h:50 | PIC constants duplicated from constants.h (maintenance hazard) |
| DRV-LOW-004 | vga.cpp | VGA hardware cursor registers (0x3D4/0x3D5) never programmed |
| DRV-LOW-006 | print.cpp:63 | `print_clear_row()` implemented but not declared in `print.h` |
| MEM-LOW-002 | heap.cpp:136 | `(uint16_t)pages` truncation unchecked; no `static_assert` for `HEAP_MAX_ALLOC` bound |
| TEST-LOW-006 | test_slab.cpp:13 | Module `tests_failed` counter not connected to `g_test_failed`; slab failures don't abort |
| TEST-LOW-010 | multiple | Test functions print "All tests passed" unconditionally regardless of inline failures |

---

## Status vs Previous Audit (2026-03-25 v1)

The v1 audit closed all CRIT and HIGH findings from phases 1–7. The v2 full-scope audit discovers new findings not previously covered:

| Previously Fixed | Still Open |
|-----------------|------------|
| 3 CRIT (log truncation, slab deadlock, spinlock fast-path) | 8 CRIT (build linker, core LOG_PANIC, lib decimal NULL, arch idt silent-drop, tests ordering) |
| 8 HIGH (audit phases 1–7) | 28 HIGH across all subsystems |
| All 11 Fase-7 LOW | 58 LOW (new scope) |

**No regression from the phase-1 fixes was found.** All previously fixed issues remain corrected.

---

## Priority Recommendations

### Fix immediately (build correctness):
1. **BUILD-CRIT-001**: `__kernel_end` placement — bitmap corruption risk
2. **BUILD-CRIT-002**: `.boot.data NOLOAD` — conceptually wrong section type
3. **BUILD-MED-003**: `compile_commands.json` with absolute paths tracked in git

### Fix before next feature (correctness bugs):
4. **MEM-HIGH-001**: Slab 2048-byte use-after-free
5. **LIB-CRIT-001/002**: NULL-before-write in decimal utils
6. **CORE-HIGH-003**: Wrong `INT_FAST` limits
7. **CORE-HIGH-001**: `INT32_MIN` UB in `%d` formatter
8. **DRV-HIGH-001**: Serial→VGA lock inversion

### Fix for test reliability:
9. **TEST-CRIT-002**: Enable `test_memory_manager()`
10. **TEST-HIGH-003/005/006**: Fix vacuous PIT IRQ test, slab stats test, re-enable direct API test
11. **TEST-MED-006**: Summary block should reflect actual pass/fail state

