# GLOBEX_OS - CI/CD and Static Analysis Guide

## Overview

This document describes the continuous integration and static analysis tools available for GLOBEX_OS kernel development.

## Tools

### Static Analysis

The project uses two static analysis tools:

1. **clang-tidy** - LLVM-based C++ linter
2. **cppcheck** - C/C++ static analysis tool

### CI/CD Pipeline

GitHub Actions runs automated checks on every push and pull request.

## Running Static Analysis Locally

### Prerequisites

Install required tools:

```bash
# Ubuntu/Debian
sudo apt-get install clang-tidy cppcheck

# macOS
brew install llvm cppcheck
```

### Run Analysis

```bash
# Make script executable (first time only)
chmod +x scripts/analyze.sh

# Run full analysis
./scripts/analyze.sh
```

### Expected Output

```
============================================
GLOBEX_OS Static Analysis
============================================

Checking required tools...
✓ clang-tidy: Debian LLVM version 14.0.6
✓ cppcheck: Cppcheck 2.10

============================================
Running clang-tidy
============================================

Found 7 files to analyze

Analyzing: kernels/x86_64/src/drivers/console/print.cpp
  ✓ No issues found

...

clang-tidy summary:
  Errors:   0
  Warnings: 5

============================================
Running cppcheck
============================================

...

cppcheck summary:
  Errors:   0
  Warnings: 1

============================================
Static Analysis Summary
============================================
Total Errors:   0
Total Warnings: 6

⚠️  Static analysis passed with warnings
```

## CI/CD Pipeline

The GitHub Actions workflow (`.github/workflows/ci-cd.yml`) runs on every push and pull request:

### Jobs

| Job | Description | Required |
|-----|-------------|----------|
| `build` | Compiles the kernel | ✅ Yes |
| `test` | Runs tests in QEMU | ✅ Yes |
| `static-analysis` | Runs clang-tidy and cppcheck | ⚠️ Advisory |
| `code-style` | Verifies code formatting | ✅ Yes |
| `summary` | Aggregates all results | - |

### Workflow Triggers

- **Push**: `dev`, `main`, `master` branches
- **Pull Request**: Any PR targeting above branches

## Configuration Files

### `.clang-tidy`

clang-tidy configuration for IDE integration:

```yaml
Checks: '-*,bugprone-*,cert-*,misc-*,performance-*,portability-*,readability-*,clang-analyzer-*'
FormatStyle: file
HeaderFilterRegex: '.*'
ExtraArgs:
  - -xc++
  - -std=c++17
  - -ffreestanding
  - -fno-exceptions
  - -fno-rtti
  - -nostdlib
```

### `scripts/analyze.sh`

Main analysis script that:
1. Verifies tool availability
2. Runs clang-tidy on all `.cpp` files
3. Runs cppcheck on `src/` directory
4. Generates summary report

### `.github/workflows/ci-cd.yml`

GitHub Actions workflow definition.

## Checks Performed

### clang-tidy Checks

| Category | Checks |
|----------|--------|
| Bugprone | `bugprone-*` (except `narrowing-conversions`) |
| Security | `cert-*` |
| Misc | `misc-*` |
| Performance | `performance-*` |
| Portability | `portability-*` |
| Readability | `readability-*` (except `magic-numbers`, `identifier-length`) |
| Analyzer | `clang-analyzer-*` (except `NewDelete`, `NewDeleteLeaks`) |

### cppcheck Checks

| Type | Enabled |
|------|---------|
| Warning | ✅ |
| Performance | ✅ |
| Portability | ✅ |
| Style | ✅ |

## Suppressing Warnings

Some warnings are expected in kernel development:

```cpp
// NOLINT(bugprone-narrowing-conversions) - Expected for low-level code
buffer[--i] = '0' + (value % 10);
```

Or in the script configuration:
```bash
-bugprone-narrowing-conversions  # Disabled for kernel code
```

## Fixing Common Issues

### Implicit Bool Conversion

**Warning:** `readability-implicit-bool-conversion`

**Fix:**
```cpp
// Before
while (n--) { }

// After (if you want to suppress)
while (n-- != 0) { }  // More explicit
```

### Parameter Name Mismatch

**Warning:** Function parameter names differ between declaration and definition.

**Fix:** Ensure consistency:
```cpp
// header
void print_str(const char* str);

// cpp
void print_str(const char* str) { }  // Match name
```

## Artifacts

The CI/CD pipeline generates these artifacts:

| Artifact | Path | Retention |
|----------|------|-----------|
| `kernel-bin` | `dist/x86_64/kernel.bin` | 7 days |
| `kernel-iso` | `dist/x86_64/kernel.iso` | 7 days |
| `test-output` | `serial_output.log` | 7 days |
| `analysis-report` | `*-report.txt` | 7 days |

## Badges

Add these badges to your README:

```markdown
[![CI/CD](https://github.com/tmlcs/GLOBEX_OS/actions/workflows/ci-cd.yml/badge.svg)](https://github.com/tmlcs/GLOBEX_OS/actions/workflows/ci-cd.yml)
```

## Troubleshooting

### "Could not auto-detect compilation database"

This is expected for freestanding kernels. The script uses `--extra-arg` flags instead.

### False positives from clang-tidy

Some checks don't apply to freestanding code. Report issues and consider adding suppressions to `.clang-tidy`.

### cppcheck "missingInclude" warnings

These are suppressed by default. The kernel uses custom headers.

## Future Enhancements

- [ ] Generate HTML reports with `clang-tidy-diff`
- [ ] Integrate with Codecov for coverage reporting
- [ ] Add fuzzing tests for string functions
- [ ] Automated performance regression detection

## References

- [clang-tidy Documentation](https://clang.llvm.org/extra/clang-tidy/)
- [cppcheck Documentation](http://cppcheck.sourceforge.net/)
- [GitHub Actions Documentation](https://docs.github.com/en/actions)
