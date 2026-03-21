# GLOBEX_OS - SMP Testing Guide

## Overview

This guide describes the Symmetric Multi-Processing (SMP) testing capabilities for GLOBEX_OS kernel. SMP testing validates kernel behavior under real multi-processor concurrency conditions.

## Quick Start

```bash
# Build and run SMP tests (4 CPUs)
make run-smp-test

# Interactive SMP testing
make run-smp-stdio

# View SMP test output
cat serial_output_smp.log
```

## SMP Test Architecture

### Test Files

| File | Purpose |
|------|---------|
| `tests/test_spinlock_smp.h` | SMP test function declarations |
| `tests/test_spinlock_smp.cpp` | SMP test implementations |
| `scripts/verify_smp_tests.sh` | Automated test verification |
| `scripts/run_smp_tests.sh` | Batch test runner |

### Test Suite Components

```
test_spinlock_smp()
├── test_smp_data_integrity()      # Counter atomicity test
├── test_smp_per_cpu_data()        # Per-CPU data isolation
├── test_smp_fairness()            # Lock fairness verification
├── test_smp_nested_locks()        # Deadlock detection
└── test_smp_memory_ordering()     # Memory barrier validation
```

## QEMU SMP Configuration

### Makefile Targets

| Target | QEMU Flags | Description |
|--------|------------|-------------|
| `run-smp` | `-smp 4 -cpu qemu64` | Standard SMP testing (4 CPUs) |
| `run-smp-stdio` | `-smp 4 -cpu qemu64 -serial stdio` | Interactive SMP console |
| `run-smp-test` | `-smp 4 -cpu qemu64` + verification | Full test suite |

### Custom SMP Configuration

Edit Makefile to change CPU count:

```makefile
# Change from 4 to 8 CPUs
QEMU_SMP_FLAGS := -cdrom $(ISO_FILE) -smp 8 -cpu qemu64 ...
```

## Test Descriptions

### 1. Data Integrity Test

**Purpose:** Verify no lost updates under concurrent access.

**Test Design:**
- Shared counter initialized to 0
- Each CPU increments counter 1000 times
- Final value should be exactly `CPUs × 1000`

**Success Criteria:**
```
Expected: 4000  (for 4 CPUs)
Actual:   4000
[PASS] DATA INTEGRITY
No lost updates detected!
```

**Failure Indicators:**
- `Lost updates detected` - Race condition in spinlock
- Counter value less than expected

### 2. Per-CPU Data Test

**Purpose:** Verify per-CPU data isolation with shared lock.

**Test Design:**
- Array of per-CPU counters
- Each CPU increments only its own counter
- Lock protects array access

**Success Criteria:**
```
CPU0 counter: 1000 (expected: 1000)
CPU1 counter: 1000 (expected: 1000)
CPU2 counter: 1000 (expected: 1000)
CPU3 counter: 1000 (expected: 1000)
[PASS] PER-CPU DATA
```

### 3. Fairness Test

**Purpose:** Verify no CPU starvation under contention.

**Test Design:**
- Track acquisitions per CPU
- Run many iterations
- Verify all CPUs get lock proportionally

**Success Criteria:**
```
CPU0: 2050 acquisitions
CPU1: 1980 acquisitions
CPU2: 2010 acquisitions
CPU3: 1960 acquisitions
Min: 1960, Max: 2050, Avg: ~2000
[PASS] FAIRNESS
```

**Warning Indicators:**
- `Possible starvation detected` - One CPU got < 50% of average

### 4. Nested Locks Test

**Purpose:** Detect potential deadlocks with multiple locks.

**Test Design:**
- Two locks: lock_a and lock_b
- Even CPUs acquire: A → B
- Odd CPUs acquire: B → A
- Verify all operations complete

**Success Criteria:**
```
Expected operations: 400
Actual:          400
[PASS] NESTED LOCKS (No Deadlock)
```

**Failure Indicators:**
- Test timeout - Deadlock occurred
- Operations count mismatch

### 5. Memory Ordering Test

**Purpose:** Validate memory barrier effectiveness.

**Test Design:**
- CPU 0: Write data, then set flag
- CPU 1: Wait for flag, read data
- Verify data visibility

**Success Criteria:**
```
[PASS] MEMORY ORDERING
```

## Running SMP Tests

### Standard Test Run

```bash
# Build and run full SMP test suite
make run-smp-test

# Expected output:
============================================
GLOBEX_OS SMP Test Suite (4 CPUs)
============================================
...
=== Verifying SMP Test Results ===
[PASS] DATA INTEGRITY
[PASS] PER-CPU DATA
[PASS] FAIRNESS
[PASS] NESTED LOCKS
[PASS] MEMORY ORDERING
All SMP tests PASSED!
```

### Interactive Debugging

```bash
# Run with serial on stdio for interactive debugging
make run-smp-stdio

# Output appears in terminal (Ctrl+A X to exit QEMU)
```

### Background Execution

```bash
# Run in background, capture output
make run-smp &

# View output in real-time
tail -f serial_output_smp.log
```

## Interpreting Results

### Single-CPU vs Multi-CPU

**Single-CPU Mode (Default QEMU):**
```
[NOTE] Single-CPU mode detected
[NOTE] For real SMP testing, run: make run-smp
[NOTE] Tests will verify logical correctness only
```

Tests pass but only validate **logical correctness**, not **concurrency safety**.

**Multi-CPU Mode (-smp 4):**
```
SMP Detected: 4 CPUs available
Current CPU: 0
[SMP] Real multi-processor testing enabled
```

Tests validate **actual concurrency** with real race conditions.

### Success Indicators

```
============================================
SMP Test Summary
============================================
Total Tests:  5
Passed:       5
Failed:       0

All SMP tests PASSED!
✓ Real SMP concurrency verified on 4 CPUs
```

### Failure Indicators

```
[FAIL] DATA INTEGRITY
ERROR: Lost updates detected!
Lost updates: 15

Some SMP tests FAILED!
Review the output file for details: serial_output_smp.log
```

## Troubleshooting

### Test Timeout

**Symptom:** QEMU hangs during SMP test

**Possible Causes:**
1. Deadlock in spinlock implementation
2. Interrupt handling issue
3. QEMU configuration problem

**Solutions:**
```bash
# Increase timeout
timeout 120 qemu-system-x86_64 -smp 4 ...

# Run with debug output
make run-debug

# Check QEMU logs
dmesg | grep qemu
```

### Single-CPU Only

**Symptom:** Tests always run in single-CPU mode

**Check QEMU flags:**
```bash
# Verify -smp flag is present
ps aux | grep qemu

# Should show: -smp 4 or similar
```

**Fix:**
```bash
# Rebuild to ensure Makefile changes applied
make clean && make build-x86_64
make run-smp-test
```

### Lost Updates Detected

**Symptom:** Data integrity test fails

**Investigation:**
```bash
# Check spinlock implementation
cat src/lib/spinlock/spinlock.cpp

# Verify LOCK prefix is used
grep -n "lock cmpxchg" src/lib/spinlock/spinlock.cpp
```

**Expected:**
```asm
lock cmpxchg %2, %1  # LOCK prefix required for SMP
```

## Performance Benchmarks

### Expected Test Duration

| CPUs | Data Integrity | Fairness | Total Suite |
|------|---------------|----------|-------------|
| 1 | ~0.5s | ~1.0s | ~3s |
| 2 | ~1.0s | ~2.0s | ~6s |
| 4 | ~2.0s | ~4.0s | ~12s |
| 8 | ~4.0s | ~8.0s | ~24s |

Times are approximate and depend on host CPU speed.

### Contention Metrics

High contention indicators:
- Longer test duration with more CPUs
- Higher variance in acquisition counts
- More PAUSE instructions executed

Low contention (good):
- Linear scaling with CPU count
- Even acquisition distribution
- Minimal spin time

## Advanced Configuration

### Custom Test Parameters

Edit `tests/test_spinlock_smp.cpp`:

```cpp
/* Number of iterations per CPU for stress tests */
#define SMP_TEST_ITERATIONS 1000  // Increase for more stress

/* Number of CPUs to test */
#define SMP_TEST_MAX_CPUS 4  // Match QEMU -smp flag

/* Minimum acquisitions per CPU for fairness test */
#define SMP_FAIRNESS_MIN_ACQUISITIONS 10  // Adjust threshold
```

### Adding New SMP Tests

1. Create test function in `test_spinlock_smp.cpp`:
```cpp
void test_smp_custom_scenario(void) {
    int cpu_id = smp_get_cpu_id();
    int cpu_count = smp_get_cpu_count();
    
    // Test implementation...
    
    smp_print_result("CUSTOM TEST", passed);
}
```

2. Add declaration to `test_spinlock_smp.h`:
```cpp
void test_smp_custom_scenario(void);
```

3. Call from `test_spinlock_smp()`:
```cpp
test_smp_custom_scenario();
```

## CI/CD Integration

### GitHub Actions Example

```yaml
smp-testing:
  name: SMP Tests
  runs-on: ubuntu-22.04
  steps:
    - uses: actions/checkout@v4
    - name: Install QEMU
      run: sudo apt-get install qemu-system-x86
    - name: Build kernel
      run: make build-x86_64
    - name: Run SMP tests
      run: make run-smp-test
    - name: Upload results
      uses: actions/upload-artifact@v4
      with:
        name: smp-test-output
        path: serial_output_smp.log
```

## References

### Documentation

- `QWEN.md` - Project context and architecture
- `CODE_QUALITY_AUDIT.md` - Quality audit findings
- `docs/CICD_GUIDE.md` - CI/CD pipeline configuration

### External Resources

- [QEMU System Emulation](https://www.qemu.org/docs/master/system/index.html)
- [Intel® 64 and IA-32 Architectures SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [Linux Kernel Memory Barriers](https://www.kernel.org/doc/Documentation/memory-barriers.txt)

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-03-19 | Initial SMP testing implementation |
| | | - 5 test scenarios |
| | | - Automated verification |
| | | - QEMU 4-CPU configuration |

---

**Next Steps:**
1. Run `make run-smp-test` to validate SMP safety
2. Review `serial_output_smp.log` for detailed results
3. Fix any failures before production deployment
