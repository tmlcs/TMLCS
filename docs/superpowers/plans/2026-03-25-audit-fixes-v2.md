# Audit Fixes v2 — CRIT + Priority HIGH Findings

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix all 8 CRIT and priority HIGH findings from the 2026-03-25 full-scope audit.

**Architecture:** Each task is a targeted, independent fix to one subsystem. All changes must keep the kernel buildable and all tests passing after every commit. Verification command: `cd kernels/x86_64 && make run-serial 2>&1 | tail -5` — look for zero `FAILED` lines in `serial_output.log`.

**Tech Stack:** C++ freestanding kernel, NASM, `ld` linker script, QEMU x86_64. No stdlib. `-Wall -Wextra -Wpedantic -Werror`. Work in `.worktrees/audit-fixes-v2` (branch `feature/audit-fixes-v2`), all `make` commands from `kernels/x86_64/`.

---

## File Map

| File | Tasks |
|------|-------|
| `kernels/x86_64/targets/x86_64/linker.ld` | Task 1 |
| `.gitignore` | Task 2 |
| `kernels/x86_64/src/core/log/log.cpp` | Tasks 3, 8 |
| `kernels/x86_64/src/lib/utils/decimal_utils.h` | Task 4 |
| `kernels/x86_64/src/arch/x86_64/idt/idt.cpp` | Task 5 |
| `kernels/x86_64/src/memory/slab.cpp` | Task 6 |
| `kernels/x86_64/src/lib/spinlock/spinlock.cpp` | Task 7 |
| `kernels/x86_64/src/core/stdint/stdint.h` | Task 9 |
| `kernels/x86_64/src/drivers/serial/serial.cpp` | Task 10 |
| `kernels/x86_64/src/kernel/main.cpp` | Task 11 |
| `kernels/x86_64/tests/test_pit.cpp` | Task 12 |
| `kernels/x86_64/tests/test_slab.cpp` | Task 12 |

---

## Task 1: Fix linker script — `__kernel_end` placement + `.boot.data (NOLOAD)`

**Findings:** BUILD-CRIT-001 + BUILD-CRIT-002
**File:** `kernels/x86_64/targets/x86_64/linker.ld`

**Problem 1 (BUILD-CRIT-001):** `__kernel_end = .;` is at line 116, **outside** the `SECTIONS { }` block. Outside `SECTIONS`, `.` is implementation-defined and may not equal the true post-layout VMA. `bitmap_init()` reads `&__kernel_end` to find the first free physical page; a wrong symbol value causes the bitmap to overlap the kernel, silently corrupting memory.

**Problem 2 (BUILD-CRIT-002):** `.boot.data` on line 85 lacks `(NOLOAD)`. Without it, the linker assigns a `PT_LOAD` segment with file backing, and GRUB writes zeros over the page tables before the CPU enters long mode.

- [ ] **Step 1: Read the file**

  ```bash
  cat -n kernels/x86_64/targets/x86_64/linker.ld
  ```
  Confirm: `__kernel_end = .;` is after the closing `}` of `SECTIONS` (around line 116), and `.boot.data` starts without `(NOLOAD)` (around line 85).

- [ ] **Step 2: Apply fix**

  In `kernels/x86_64/targets/x86_64/linker.ld`:

  **Change 1** — add `(NOLOAD)` to `.boot.data` declaration (line 85):
  ```
  Before: .boot.data : ALIGN(4K)
  After:  .boot.data (NOLOAD) : ALIGN(4K)
  ```

  **Change 2** — add `__kernel_end = .;` inside `SECTIONS`, immediately after the `.boot.data` closing `} :data` and before the `ASSERT` block. The result should look like:
  ```
      } :data

      __kernel_end = .;   /* End of kernel image — must be inside SECTIONS */

      /* ==========================================
       * Validations - ASSERTs for safety
  ```

  **Change 3** — remove the now-duplicate `__kernel_end = .;` from outside `SECTIONS` (old line 116). Keep `__kernel_start = 1M;` and the memory map symbols outside, as they are constants.

- [ ] **Step 3: Build and verify**

  ```bash
  cd kernels/x86_64 && make build-x86_64 2>&1 | grep -E "error:|warning:"
  ```
  Expected: no output (zero errors, zero warnings).

- [ ] **Step 4: Run tests**

  ```bash
  make run-serial 2>&1 | tail -5
  grep "FAILED" serial_output.log
  ```
  Expected: `grep` returns empty (no FAILED lines).

- [ ] **Step 5: Commit**

  ```bash
  git add kernels/x86_64/targets/x86_64/linker.ld
  git commit -m "fix(linker): move __kernel_end inside SECTIONS + add (NOLOAD) to .boot.data

  BUILD-CRIT-001: __kernel_end outside SECTIONS gave wrong VMA for bitmap_init.
  BUILD-CRIT-002: .boot.data without (NOLOAD) assigned a PT_LOAD segment."
  ```

---

## Task 2: Fix gitignore — exclude `compile_commands.json` at repo root

**Finding:** BUILD-MED-003
**File:** `.gitignore`

**Problem:** `kernels/x86_64/compile_commands.json` contains absolute host paths and is tracked in git. The existing `.gitignore` entry only covers `kernels/x86_64/scripts/compile_commands.json`.

- [ ] **Step 1: Check current tracking**

  ```bash
  git ls-files kernels/x86_64/compile_commands.json
  ```
  If the file is printed, it is tracked and must be removed from the index.

- [ ] **Step 2: Add to `.gitignore`**

  Append this line to `.gitignore`:
  ```
  kernels/x86_64/compile_commands.json
  ```

- [ ] **Step 3: Remove from index if tracked**

  Only run this if Step 1 printed the file:
  ```bash
  git rm --cached kernels/x86_64/compile_commands.json
  ```

- [ ] **Step 4: Verify**

  ```bash
  git check-ignore kernels/x86_64/compile_commands.json
  ```
  Expected: prints the path (confirms it is now ignored).

- [ ] **Step 5: Commit**

  ```bash
  git add .gitignore
  git commit -m "chore(gitignore): exclude compile_commands.json from repo root

  BUILD-MED-003: absolute host paths should not be tracked in version control."
  ```

---

## Task 3: Fix `LOG_PANIC` halt without `cli`

**Finding:** CORE-CRIT-001
**File:** `kernels/x86_64/src/core/log/log.cpp`

**Problem:** The `log_output()` function halts in a `hlt` loop after releasing `g_log_lock`, but never disables interrupts. The PIT fires IRQ0 every 10 ms, waking the CPU from `hlt`, allowing re-entry into the log system. `panic()` correctly issues `cli` first; `log_output` must do the same.

- [ ] **Step 1: Locate the hlt loop**

  ```bash
  grep -n "hlt\|LOG_LEVEL_PANIC" kernels/x86_64/src/core/log/log.cpp
  ```
  Expected: finds `if (level == LOG_LEVEL_PANIC)` followed by a `for (;;) { __asm__ volatile("hlt"); }` block.

- [ ] **Step 2: Apply fix**

  In `log.cpp`, find the panic halt block (approximately):
  ```cpp
  /* Handle PANIC level - halt system */
  if (level == LOG_LEVEL_PANIC) {
      for (;;) {
          __asm__ volatile("hlt");
      }
  }
  ```

  Replace with:
  ```cpp
  /* Handle PANIC level - halt system */
  if (level == LOG_LEVEL_PANIC) {
      __asm__ volatile("cli" ::: "memory");   /* Disable interrupts before halt */
      for (;;) {
          __asm__ volatile("hlt");
      }
  }
  ```

- [ ] **Step 3: Build and verify**

  ```bash
  cd kernels/x86_64 && make build-x86_64 2>&1 | grep -E "error:|warning:"
  ```
  Expected: no output.

- [ ] **Step 4: Run tests**

  ```bash
  make run-serial 2>&1 | tail -5
  grep "FAILED" serial_output.log
  ```
  Expected: empty grep result.

- [ ] **Step 5: Commit**

  ```bash
  git add kernels/x86_64/src/core/log/log.cpp
  git commit -m "fix(log): disable interrupts before hlt loop in LOG_PANIC path

  CORE-CRIT-001: PIT IRQ0 fires every 10ms and re-enters log system.
  Add cli before hlt, matching the existing panic() implementation."
  ```

---

## Task 4: Fix `decimal_utils.h` — NULL check before buffer writes

**Findings:** LIB-CRIT-001 + LIB-CRIT-002
**File:** `kernels/x86_64/src/lib/utils/decimal_utils.h`

**Problem:** Both `uint32_to_decimal_string` and `uint64_to_decimal_string` write to `buffer[0]` / `buffer[1]` in the zero-value fast-path (and in the bounds-fail path) **before** the `if (buffer == nullptr)` check. Calling either function with `buffer=NULL` causes an immediate null-pointer write, producing a kernel triple fault.

- [ ] **Step 1: Locate both functions**

  ```bash
  grep -n "uint32_to_decimal_string\|uint64_to_decimal_string\|buffer == nullptr" \
      kernels/x86_64/src/lib/utils/decimal_utils.h
  ```
  Confirm the `buffer == nullptr` guard is after the first `buffer[0]` write in both functions.

- [ ] **Step 2: Fix `uint32_to_decimal_string`**

  Find the function opening (around line 62):
  ```cpp
  inline char* uint32_to_decimal_string(char* buffer, uint32_t value) {
      /* Handle zero case */
      if (value == 0) {
  ```

  Add the null guard as the very **first** statement in the function body, and **remove** the now-redundant null check that appears later:
  ```cpp
  inline char* uint32_to_decimal_string(char* buffer, uint32_t value) {
      if (buffer == nullptr) { return nullptr; }   /* Guard before any write */
      /* Handle zero case */
      if (value == 0) {
          buffer[0] = '0';
          buffer[1] = '\0';
          return buffer;
      }
      /* ... rest of function unchanged ... */
  ```

  Then **delete** the original null guard block that appeared after the bounds check (around line 92-95):
  ```cpp
  /* DELETE these lines: */
  /* Null pointer safety check */
  if (buffer == nullptr) {
      return nullptr;
  }
  ```

- [ ] **Step 3: Fix `uint64_to_decimal_string`** (same pattern)

  Find the function opening (around line 153):
  ```cpp
  inline char* uint64_to_decimal_string(char* buffer, uint64_t value) {
      /* Handle zero case */
  ```

  Add null guard as first statement, remove the later one (around line 183-186):
  ```cpp
  inline char* uint64_to_decimal_string(char* buffer, uint64_t value) {
      if (buffer == nullptr) { return nullptr; }   /* Guard before any write */
      /* Handle zero case */
      if (value == 0) {
          buffer[0] = '0';
          buffer[1] = '\0';
          return buffer;
      }
  ```

  Delete the late null guard at line ~184:
  ```cpp
  /* DELETE: */
  if (buffer == nullptr) {
      return nullptr;
  }
  ```

- [ ] **Step 4: Build**

  ```bash
  cd kernels/x86_64 && make build-x86_64 2>&1 | grep -E "error:|warning:"
  ```
  Expected: no output.

- [ ] **Step 5: Run tests**

  ```bash
  make run-serial 2>&1 | tail -5
  grep "FAILED" serial_output.log
  ```
  Expected: empty grep result.

- [ ] **Step 6: Commit**

  ```bash
  git add kernels/x86_64/src/lib/utils/decimal_utils.h
  git commit -m "fix(decimal_utils): move NULL guard before buffer writes in both conversion functions

  LIB-CRIT-001/002: NULL check was after buffer[0] write in zero fast-path
  and bounds-fail path — calling with NULL caused immediate triple fault."
  ```

---

## Task 5: Fix `idt_set_gate` — PANIC on invalid handler instead of silent return

**Finding:** ARCH-CRIT-001
**File:** `kernels/x86_64/src/arch/x86_64/idt/idt.cpp`

**Problem:** When `idt_set_gate` receives an out-of-range handler address, it prints a serial message and silently returns, leaving the IDT vector as a not-present all-zero entry. If that vector fires, the CPU triple-faults with no explanation. A PANIC is appropriate: this is a kernel programming error that should never happen at boot.

- [ ] **Step 1: Locate the validation block**

  ```bash
  grep -n "VALID_HANDLER\|Invalid handler" kernels/x86_64/src/arch/x86_64/idt/idt.cpp
  ```
  Expected: finds the `if (handler.value < VALID_HANDLER_MIN || ...)` block with a `return;` at the end.

- [ ] **Step 2: Apply fix**

  Find the validation block (approximately lines 182–187):
  ```cpp
  if (handler.value < VALID_HANDLER_MIN || handler.value >= VALID_HANDLER_MAX) {
      serial_write_str("[IDT] HIGH-004: Invalid handler address: 0x");
      serial_write_hex64(handler.value);
      serial_write_str("\r\n");
      return;  /* Don't register invalid handler */
  }
  ```

  Replace with:
  ```cpp
  if (handler.value < VALID_HANDLER_MIN || handler.value >= VALID_HANDLER_MAX) {
      PANIC("idt_set_gate: invalid handler for vector %u", (uint32_t)vector);
  }
  ```

  Confirm `panic.h` is already included in this file:
  ```bash
  grep -n "panic.h" kernels/x86_64/src/arch/x86_64/idt/idt.cpp
  ```
  If not included, add `#include "panic.h"` at the top of the file with the other includes.

- [ ] **Step 3: Build**

  ```bash
  cd kernels/x86_64 && make build-x86_64 2>&1 | grep -E "error:|warning:"
  ```
  Expected: no output.

- [ ] **Step 4: Run tests**

  ```bash
  make run-serial 2>&1 | tail -5
  grep "FAILED" serial_output.log
  ```
  Expected: empty grep result (all valid handlers still register correctly).

- [ ] **Step 5: Commit**

  ```bash
  git add kernels/x86_64/src/arch/x86_64/idt/idt.cpp
  git commit -m "fix(idt): PANIC on invalid handler address instead of silent return

  ARCH-CRIT-001: silent return left IDT vector as not-present zero entry;
  any fire of that vector would triple-fault with no diagnostic output."
  ```

---

## Task 6: Fix slab — 2048-byte exhausted slab goes to `full`, not `partial`

**Finding:** MEM-HIGH-001
**File:** `kernels/x86_64/src/memory/slab.cpp`

**Problem:** After allocating the first (and only) object from a newly-created 2048-byte slab, `slab->num_free` becomes 0, but the slab is unconditionally added to `cache->partial` (line 599). On the subsequent `kmem_free`, the code looks for the slab in `cache->full`, doesn't find it, corrupts the full-list pointer, and produces a use-after-free on the next `kmem_alloc(2048)`.

- [ ] **Step 1: Locate the placement**

  ```bash
  grep -n "Add slab to partial\|add_slab_to_list.*partial" kernels/x86_64/src/memory/slab.cpp
  ```
  Expected: one match around line 599 inside the "no partial slabs" branch.

- [ ] **Step 2: Apply fix**

  Find the unconditional line (approximately):
  ```cpp
  /* Add slab to partial list */
  add_slab_to_list(slab, &cache->partial);
  ```

  Replace with:
  ```cpp
  /* Add slab to appropriate list — full if no free objects remain */
  if (slab->num_free == 0) {
      add_slab_to_list(slab, &cache->full);
  } else {
      add_slab_to_list(slab, &cache->partial);
  }
  ```

- [ ] **Step 3: Build**

  ```bash
  cd kernels/x86_64 && make build-x86_64 2>&1 | grep -E "error:|warning:"
  ```
  Expected: no output.

- [ ] **Step 4: Run tests**

  ```bash
  make run-serial 2>&1 | tail -5
  grep "FAILED" serial_output.log
  ```
  Expected: empty grep result.

- [ ] **Step 5: Commit**

  ```bash
  git add kernels/x86_64/src/memory/slab.cpp
  git commit -m "fix(slab): place fully-exhausted slab in full list, not partial

  MEM-HIGH-001: when objects_per_slab==1 (2048B cache), the first allocation
  drains num_free to 0; adding to partial instead of full caused use-after-free
  on subsequent kmem_free -> list-pointer corruption -> UAF on next alloc."
  ```

---

## Task 7: Fix spinlock release — clear `interrupts_enabled` before dropping lock

**Finding:** LIB-HIGH-001
**File:** `kernels/x86_64/src/lib/spinlock/spinlock.cpp`

**Problem:** `spinlock_release()` reads `lock->interrupts_enabled` into `was_enabled`, then sets `lock->locked = 0`, then resets `lock->interrupts_enabled = false`. After `locked = 0`, another CPU can acquire the lock and write `lock->interrupts_enabled = true`. When the releasing CPU subsequently writes `false`, it destroys the new owner's saved interrupt state. When the new owner releases, it restores the wrong state (interrupts stay disabled).

**Fix:** Reset `lock->interrupts_enabled` to `false` **before** setting `lock->locked = 0`, while we still exclusively own the lock.

- [ ] **Step 1: Locate the release sequence**

  ```bash
  grep -n "interrupts_enabled\|lock->locked" kernels/x86_64/src/lib/spinlock/spinlock.cpp
  ```
  Confirm the order: read `was_enabled`, then `locked = 0`, then `interrupts_enabled = false`.

- [ ] **Step 2: Apply fix**

  Find the release sequence (approximately lines 313–322):
  ```cpp
  bool was_enabled = lock->interrupts_enabled;

  /* Release the lock by setting locked to 0 */
  lock->locked = 0;

  /*
   * Reset interrupt state flag for next acquire
   * interrupts_enabled is volatile - ensures visibility across CPUs
   */
  lock->interrupts_enabled = false; /* Reset for next acquire */
  ```

  Reorder to clear `interrupts_enabled` **before** releasing `locked`:
  ```cpp
  bool was_enabled = lock->interrupts_enabled;
  lock->interrupts_enabled = false; /* Reset before releasing — prevents race with new owner */

  /* Memory barrier: ensure interrupts_enabled reset is visible before lock release */
  barrier();

  /* Release the lock */
  lock->locked = 0;
  ```

  Remove the original `lock->interrupts_enabled = false;` line that was after `lock->locked = 0`. Keep the `barrier()` calls already present in the function; do not add duplicate barriers.

- [ ] **Step 3: Read the full function context**

  Before saving, read lines 285–340 of `spinlock.cpp` to verify the surrounding `barrier()` calls are still correctly placed and no barrier is duplicated.

- [ ] **Step 4: Build**

  ```bash
  cd kernels/x86_64 && make build-x86_64 2>&1 | grep -E "error:|warning:"
  ```

- [ ] **Step 5: Run tests**

  ```bash
  make run-serial 2>&1 | tail -5
  grep "FAILED" serial_output.log
  ```
  Expected: empty grep result (spinlock tests should still pass).

- [ ] **Step 6: Commit**

  ```bash
  git add kernels/x86_64/src/lib/spinlock/spinlock.cpp
  git commit -m "fix(spinlock): clear interrupts_enabled before releasing lock to prevent race

  LIB-HIGH-001: another CPU could acquire lock and set interrupts_enabled between
  locked=0 and the reset write, causing the new owner's interrupt state to be
  overwritten and interrupts to remain disabled after its release."
  ```

---

## Task 8: Fix `%d` formatter — safe `INT32_MIN` negation in `log.cpp`

**Finding:** CORE-HIGH-001
**File:** `kernels/x86_64/src/core/log/log.cpp`

**Problem:** The `'d'` case in `format_append_arg()` does `val = -val` when `val < 0`. When `val == INT32_MIN` (-2147483648), negation overflows a signed 32-bit integer — undefined behaviour. The 64-bit path in the same file already uses the safe idiom `(uint32_t)(-(val+1)) + 1`.

- [ ] **Step 1: Locate the `'d'` case**

  ```bash
  grep -n "case 'd'" kernels/x86_64/src/core/log/log.cpp
  ```
  Read 10 lines of context. Confirm `val = -val` is present.

- [ ] **Step 2: Apply fix**

  Find the `'d'` case (approximately lines 169–176):
  ```cpp
  case 'd': {
      int32_t val = va_arg(args, int32_t);
      if (val < 0) {
          append_char(buf, '-', buf_end);
          val = -val;
      }
      append_dec(buf, (uint32_t) val, buf_end);
      break;
  }
  ```

  Replace with:
  ```cpp
  case 'd': {
      int32_t val = va_arg(args, int32_t);
      if (val < 0) {
          append_char(buf, '-', buf_end);
          /* Safe negation: avoids INT32_MIN overflow UB */
          append_dec(buf, (uint32_t)(-(val + 1)) + 1, buf_end);
      } else {
          append_dec(buf, (uint32_t)val, buf_end);
      }
      break;
  }
  ```

- [ ] **Step 3: Build and test**

  ```bash
  cd kernels/x86_64 && make build-x86_64 2>&1 | grep -E "error:|warning:"
  make run-serial 2>&1 | tail -5
  grep "FAILED" serial_output.log
  ```

- [ ] **Step 4: Commit**

  ```bash
  git add kernels/x86_64/src/core/log/log.cpp
  git commit -m "fix(log): safe INT32_MIN negation in %d formatter to eliminate UB

  CORE-HIGH-001: val = -val when val==INT32_MIN is signed overflow (UB).
  Apply (uint32_t)(-(val+1))+1 idiom already used in the 64-bit path."
  ```

---

## Task 9: Fix `stdint.h` — correct `INT_FAST16/32` limit macros

**Finding:** CORE-HIGH-003
**File:** `kernels/x86_64/src/core/stdint/stdint.h`

**Problem:** `int_fast16_t` and `int_fast32_t` are typedef'd to `long` (64-bit on x86_64), but their limit macros are `INT16_MIN/MAX` and `INT32_MIN/MAX`. Any range check using `INT_FAST16_MAX` silently uses 32767 instead of the correct 9223372036854775807.

- [ ] **Step 1: Verify the typedef**

  ```bash
  grep -n "int_fast16_t\|int_fast32_t" kernels/x86_64/src/core/stdint/stdint.h | head -10
  ```
  Confirm both are `typedef long ...` (64-bit).

- [ ] **Step 2: Locate the limit macros**

  ```bash
  grep -n "INT_FAST16\|INT_FAST32\|UINT_FAST16\|UINT_FAST32" kernels/x86_64/src/core/stdint/stdint.h
  ```
  Expected: finds macros around lines 173–181 with `INT16_MIN/MAX`, `INT32_MIN/MAX` values.

- [ ] **Step 3: Apply fix**

  Find the fast16 and fast32 limit blocks:
  ```c
  /* Limits of fast16 types */
  #define INT_FAST16_MIN  INT16_MIN
  #define INT_FAST16_MAX  INT16_MAX
  #define UINT_FAST16_MAX UINT16_MAX

  /* Limits of fast32 types */
  #define INT_FAST32_MIN  INT32_MIN
  #define INT_FAST32_MAX  INT32_MAX
  #define UINT_FAST32_MAX UINT32_MAX
  ```

  Replace with:
  ```c
  /* Limits of fast16 types — typedef'd to long (64-bit on x86_64) */
  #define INT_FAST16_MIN  INT64_MIN
  #define INT_FAST16_MAX  INT64_MAX
  #define UINT_FAST16_MAX UINT64_MAX

  /* Limits of fast32 types — typedef'd to long (64-bit on x86_64) */
  #define INT_FAST32_MIN  INT64_MIN
  #define INT_FAST32_MAX  INT64_MAX
  #define UINT_FAST32_MAX UINT64_MAX
  ```

- [ ] **Step 4: Build and test**

  ```bash
  cd kernels/x86_64 && make build-x86_64 2>&1 | grep -E "error:|warning:"
  make run-serial 2>&1 | tail -5
  grep "FAILED" serial_output.log
  ```

- [ ] **Step 5: Commit**

  ```bash
  git add kernels/x86_64/src/core/stdint/stdint.h
  git commit -m "fix(stdint): correct INT_FAST16/32 limit macros to reflect 64-bit typedef

  CORE-HIGH-003: int_fast16_t and int_fast32_t are typedef long (64-bit)
  but INT_FAST16/32_MIN/MAX used 16/32-bit bounds causing silent wrong limits."
  ```

---

## Task 10: Fix serial — remove VGA call from transmit-timeout path

**Finding:** DRV-HIGH-001
**File:** `kernels/x86_64/src/drivers/serial/serial.cpp`

**Problem:** `serial_wait_transmit_empty_timeout()` is called while `g_serial_lock` is held (by both `serial_write_char` and `serial_write_str`). Inside the timeout path, it calls `print_set_color` and `print_str`, which acquire `g_vga_lock`. If any other code path holds `g_vga_lock` and calls serial (a realistic scenario during a panic), the two locks are acquired in opposite order → deadlock.

- [ ] **Step 1: Locate the VGA calls**

  ```bash
  grep -n "print_str\|print_set_color" kernels/x86_64/src/drivers/serial/serial.cpp
  ```
  Expected: finds the timeout block in `serial_wait_transmit_empty_timeout` (approximately lines 348–352).

- [ ] **Step 2: Read context**

  Read lines 305–360 of `serial.cpp` to understand the full timeout handling block.

- [ ] **Step 3: Apply fix**

  Find the VGA-calling block inside `serial_wait_transmit_empty_timeout` (at the end of the TIMEOUT section):
  ```cpp
  if (print_is_initialized()) {
      print_set_color(PRINT_COLOR_YELLOW, PRINT_COLOR_BLACK);
      print_str("[SERIAL TIMEOUT] Hardware not responding!\r\n");
      print_set_color(PRINT_COLOR_LIGHT_GREEN, PRINT_COLOR_BLACK);
  }
  ```

  Replace with a comment explaining the removal:
  ```cpp
  /* VGA output intentionally omitted here: this function is called while
   * g_serial_lock is held. Calling print_str would acquire g_vga_lock,
   * creating a lock-order inversion with callers that hold g_vga_lock
   * and call serial functions. Error state is already recorded above in
   * g_serial_state.error_code = SERIAL_ERROR_TIMEOUT. */
  ```

- [ ] **Step 4: Build and test**

  ```bash
  cd kernels/x86_64 && make build-x86_64 2>&1 | grep -E "error:|warning:"
  make run-serial 2>&1 | tail -5
  grep "FAILED" serial_output.log
  ```
  The serial baud rate and signed decimal tests must still pass.

- [ ] **Step 5: Commit**

  ```bash
  git add kernels/x86_64/src/drivers/serial/serial.cpp
  git commit -m "fix(serial): remove VGA call from timeout path to eliminate lock-order inversion

  DRV-HIGH-001: serial_wait_transmit_empty_timeout is called under g_serial_lock;
  calling print_str acquires g_vga_lock creating potential deadlock with callers
  that hold g_vga_lock and write serial output."
  ```

---

## Task 11: Fix test ordering and enable `test_memory_manager`

**Findings:** TEST-CRIT-001 + TEST-CRIT-002 + TEST-HIGH-007
**File:** `kernels/x86_64/src/kernel/main.cpp`

**Problems:**
- TEST-CRIT-001: `test_pit_all()` runs at line 292, before `early_alloc_init_default()` (line 295) and `heap_init()` (line 298). Any test added between 292–295 that calls kmalloc will crash.
- TEST-CRIT-002: `test_memory_manager()` (726-line suite covering kmalloc/krealloc/kcalloc/kmem_free_auto) compiles but is never called.
- TEST-HIGH-007: `test_spinlock_smp()` compiles but is never called.

- [ ] **Step 1: Read current test sequence**

  Read `main.cpp` lines 280–370 to see the exact call order.

- [ ] **Step 2: Move `test_pit_all()` after `heap_init()`**

  Remove `test_pit_all();` from its current position (line ~292, before early_alloc_init_default).

  Add `test_pit_all();` immediately after the `heap_init();` call, before the other post-init tests.

  The result should be:
  ```cpp
      early_alloc_init_default();
      heap_init();
      test_pit_all();                   // Moved here — heap available

      test_print_functions();
      ...
  ```

- [ ] **Step 3: Add missing test includes**

  In the `// Include test function declarations` block (lines 20–36), add:
  ```cpp
  #include "../../tests/test_memory_manager.h"
  #include "../../tests/test_spinlock_smp.h"
  ```

- [ ] **Step 4: Add `test_memory_manager()` and `test_spinlock_smp()` calls**

  After `test_kmem_free_auto();` and before `test_hardware_info();`, add:
  ```cpp
  test_memory_manager();          // Full kmalloc/krealloc/kcalloc/kmem_free_auto suite
  test_spinlock_smp();            // SMP spinlock test suite
  ```

- [ ] **Step 5: Update the summary LOG_INFO block**

  In the `LOG_INFO("Tests executed:")` block (lines 346–366), add two lines:
  ```cpp
  LOG_INFO("  - PIT timer (IRQ0, ticks, wait): PASSED");   // already exists — verify still present
  ...
  LOG_INFO("  - Memory manager (kmalloc/krealloc/kcalloc): PASSED");
  LOG_INFO("  - Spinlock SMP: PASSED");
  ```

- [ ] **Step 6: Build and test**

  ```bash
  cd kernels/x86_64 && make build-x86_64 2>&1 | grep -E "error:|warning:"
  make run-serial 2>&1 | tail -5
  grep "FAILED" serial_output.log
  ```
  Expected: empty grep result. The serial log should now show new test output sections for `test_memory_manager` and `test_spinlock_smp`.

  Also verify:
  ```bash
  grep "memory manager\|spinlock_smp\|Memory manager" serial_output.log
  ```
  Expected: lines appear, confirming the tests ran.

- [ ] **Step 7: Commit**

  ```bash
  git add kernels/x86_64/src/kernel/main.cpp
  git commit -m "fix(main): move test_pit_all after heap_init; add test_memory_manager + test_spinlock_smp

  TEST-CRIT-001: test_pit_all called before early_alloc/heap_init (ordering hazard).
  TEST-CRIT-002: test_memory_manager compiled but never called — 726 lines of tests
  covering kmalloc/krealloc/kcalloc/kmem_free_auto now execute at boot.
  TEST-HIGH-007: test_spinlock_smp compiled but never called — now runs."
  ```

---

## Task 12: Fix vacuous test assertions in PIT and slab test suites

**Findings:** TEST-HIGH-003 + TEST-HIGH-005 + TEST-HIGH-006
**Files:** `kernels/x86_64/tests/test_pit.cpp`, `kernels/x86_64/tests/test_slab.cpp`

**Problems:**
- TEST-HIGH-003 (`test_pit.cpp:216`): `current_count >= initial_count` passes even if zero IRQ0s fired (count unchanged satisfies `>=`). Should be `>`.
- TEST-HIGH-005 (`test_slab.cpp:342`): `test_slab_statistics()` calls `test_pass("Stats tracked")` with no actual assertion. Must check real counters.
- TEST-HIGH-006 (`test_slab.cpp:369`): `test_slab_direct_api()` is commented out with stale "only 32-byte supported" comment — all cache sizes work.

### Part A: `test_pit.cpp` — fix IRQ counter check

- [ ] **Step 1: Locate the check**

  ```bash
  grep -n "current_count >= initial_count\|current_count.*initial_count" \
      kernels/x86_64/tests/test_pit.cpp
  ```

- [ ] **Step 2: Apply fix**

  Find (approximately line 216):
  ```cpp
  if (current_count >= initial_count) {
      test_pass("IRQ0 count incrementing");
  ```

  Change to:
  ```cpp
  if (current_count > initial_count) {
      test_pass("IRQ0 count incrementing");
  ```

### Part B: `test_slab.cpp` — add real assertions in statistics test

- [ ] **Step 3: Read `slab_get_state` API**

  ```bash
  grep -n "slab_get_state\|total_allocations\|total_frees" \
      kernels/x86_64/src/memory/slab.cpp | head -10
  grep -n "slab_state_t\|total_allocations" \
      kernels/x86_64/src/memory/slab.h 2>/dev/null || \
  grep -rn "slab_state_t" kernels/x86_64/src/memory/
  ```

- [ ] **Step 4: Fix `test_slab_statistics()`**

  Find the end of `test_slab_statistics()` (around line 342):
  ```cpp
  test_pass("2 frees OK");
  test_pass("Stats tracked");
  ```

  Replace the vacuous `test_pass("Stats tracked")` with a real assertion:
  ```cpp
  test_pass("2 frees OK");

  slab_state_t* state = slab_get_state();
  if (state != nullptr && state->total_allocations > 0 && state->total_frees > 0) {
      test_pass("Stats tracked: allocs and frees non-zero");
  } else {
      test_fail("Stats not tracked: counters unexpectedly zero");
  }
  ```

  If `slab_get_state()` is not declared in the test's included headers, add the appropriate include. Check what headers `test_slab.cpp` already includes at the top.

### Part C: `test_slab.cpp` — re-enable `test_slab_direct_api`

- [ ] **Step 5: Locate the disabled call**

  ```bash
  grep -n "test_slab_direct_api" kernels/x86_64/tests/test_slab.cpp
  ```
  Expected: finds the commented-out call `/* test_slab_direct_api(); */  /* Disabled - only 32-byte supported */`.

- [ ] **Step 6: Re-enable**

  Remove the comment markers and stale comment:
  ```cpp
  /* Before: */
  /* test_slab_direct_api(); */  /* Disabled - only 32-byte supported */

  /* After: */
  test_slab_direct_api();
  ```

- [ ] **Step 7: Build and test**

  ```bash
  cd kernels/x86_64 && make build-x86_64 2>&1 | grep -E "error:|warning:"
  make run-serial 2>&1 | tail -5
  grep "FAILED" serial_output.log
  ```
  Expected: empty grep result.

- [ ] **Step 8: Commit**

  ```bash
  git add kernels/x86_64/tests/test_pit.cpp kernels/x86_64/tests/test_slab.cpp
  git commit -m "fix(tests): strengthen vacuous PIT, slab-stats, and slab-direct-api assertions

  TEST-HIGH-003: current_count >= initial_count passed with zero IRQs fired; use >.
  TEST-HIGH-005: test_slab_statistics() passed unconditionally; assert counters non-zero.
  TEST-HIGH-006: test_slab_direct_api disabled with stale comment; all caches work, re-enable."
  ```

---

## Final Verification

After all 12 tasks are committed:

```bash
cd kernels/x86_64 && make rebuild 2>&1 | grep -E "error:|warning:"
# Expected: no output

make run-serial 2>&1 | tail -10
# Expected: all tests PASSED, zero FAILED

grep "FAILED" serial_output.log
# Expected: no output

git log --oneline -12
# Expected: 12 new commits
```
