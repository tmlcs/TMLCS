#!/bin/bash
# =============================================================================
# GLOBEX_OS - Static Analysis Script
# =============================================================================
# Runs clang-tidy and cppcheck on all C++ source files
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
KERNEL_DIR="$PROJECT_ROOT/kernels/x86_64"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters
ERRORS=0
WARNINGS=0

echo "============================================"
echo "GLOBEX_OS Static Analysis"
echo "============================================"
echo ""

# Check if tools are available
echo "Checking required tools..."
if ! command -v clang-tidy &> /dev/null; then
    echo -e "${RED}Error: clang-tidy not found${NC}"
    echo "Install with: sudo apt-get install clang-tidy"
    exit 1
fi

if ! command -v cppcheck &> /dev/null; then
    echo -e "${RED}Error: cppcheck not found${NC}"
    echo "Install with: sudo apt-get install cppcheck"
    exit 1
fi

echo -e "${GREEN}✓ clang-tidy: $(clang-tidy --version | head -n1)${NC}"
echo -e "${GREEN}✓ cppcheck: $(cppcheck --version | head -n1)${NC}"
echo ""

# Get include paths for clang-tidy (proper format with --extra-arg)
INCLUDE_ARGS="--extra-arg=-I$KERNEL_DIR/src/core \
--extra-arg=-I$KERNEL_DIR/src/core/panic \
--extra-arg=-I$KERNEL_DIR/src/core/debug \
--extra-arg=-I$KERNEL_DIR/src/drivers/console \
--extra-arg=-I$KERNEL_DIR/src/drivers/vga \
--extra-arg=-I$KERNEL_DIR/src/drivers/serial \
--extra-arg=-I$KERNEL_DIR/src/lib/string \
--extra-arg=-I$KERNEL_DIR/src/lib/spinlock \
--extra-arg=-I$KERNEL_DIR/src/lib/utils \
--extra-arg=-I$KERNEL_DIR/src/arch/x86_64/include"

# =============================================================================
# clang-tidy Configuration
# =============================================================================
# Using checks appropriate for freestanding kernel development
# Note: We analyze .cpp files only since headers need compilation context
# =============================================================================
CLANG_TIDY_CHECKS="-*,\
bugprone-*,\
cert-*,\
misc-*,\
performance-*,\
portability-*,\
readability-*,\
clang-analyzer-*,\
-clang-analyzer-cplusplus.NewDelete,\
-clang-analyzer-cplusplus.NewDeleteLeaks,\
-readability-magic-numbers,\
-readability-identifier-length,\
-bugprone-narrowing-conversions"

echo "============================================"
echo "Running clang-tidy"
echo "============================================"
echo ""

# Find C++ implementation files only (headers need compilation context)
CPP_FILES=$(find "$KERNEL_DIR/src" -name "*.cpp")
FILE_COUNT=$(echo "$CPP_FILES" | wc -l)

echo "Found $FILE_COUNT files to analyze"
echo ""

# Analyze each file
TIDY_ERRORS=0
TIDY_WARNINGS=0

for file in $CPP_FILES; do
    relative_path="${file#$PROJECT_ROOT/}"
    echo -e "${BLUE}Analyzing:${NC} $relative_path"
    
    # Run clang-tidy with C++ mode
    output=$(clang-tidy \
        -checks="$CLANG_TIDY_CHECKS" \
        -header-filter='.*' \
        --extra-arg="-xc++" \
        --extra-arg="-std=c++17" \
        --extra-arg="-ffreestanding" \
        --extra-arg="-fno-exceptions" \
        --extra-arg="-fno-rtti" \
        --extra-arg="-nostdlib" \
        --extra-arg="-O2" \
        $INCLUDE_ARGS \
        "$file" 2>&1) || true
    
    # Count issues
    file_errors=$(echo "$output" | grep -c "\[clang-diagnostic-error\]" || true)
    file_warnings=$(echo "$output" | grep -cE "\[(bugprone|cert|misc|performance|portability|readability|clang-analyzer)-" || true)
    
    if [ -n "$output" ] && [ "$output" != "" ]; then
        echo "$output" | while IFS= read -r line; do
            if [[ "$line" == *"[clang-diagnostic-error]"* ]]; then
                echo -e "${RED}$line${NC}"
            elif [[ "$line" == *"[cert"* ]] || [[ "$line" == *"[bugprone"* ]]; then
                echo -e "${YELLOW}$line${NC}"
            else
                echo "$line"
            fi
        done
        TIDY_ERRORS=$((TIDY_ERRORS + file_errors))
        TIDY_WARNINGS=$((TIDY_WARNINGS + file_warnings))
    else
        echo -e "${GREEN}  ✓ No issues found${NC}"
    fi
done

echo ""
echo -e "${BLUE}clang-tidy summary:${NC}"
echo -e "  Errors:   ${RED}$TIDY_ERRORS${NC}"
echo -e "  Warnings: ${YELLOW}$TIDY_WARNINGS${NC}"
echo ""

# =============================================================================
# cppcheck Configuration
# =============================================================================
# Using checks appropriate for kernel development
# =============================================================================
echo "============================================"
echo "Running cppcheck"
echo "============================================"
echo ""

CHECK_ERRORS=0
CHECK_WARNINGS=0

# Create cppcheck config file
CPPCHECK_CONFIG="--std=c++17 \
-I$KERNEL_DIR/src/core \
-I$KERNEL_DIR/src/core/panic \
-I$KERNEL_DIR/src/core/debug \
-I$KERNEL_DIR/src/drivers/console \
-I$KERNEL_DIR/src/drivers/vga \
-I$KERNEL_DIR/src/drivers/serial \
-I$KERNEL_DIR/src/lib/string \
-I$KERNEL_DIR/src/lib/spinlock \
-I$KERNEL_DIR/src/lib/utils \
-I$KERNEL_DIR/src/arch/x86_64/include"

# Run cppcheck on source directory
cppcheck_output=$(cppcheck \
    --enable=warning,performance,portability,style \
    --disable=missingInclude \
    --inconclusive \
    $CPPCHECK_CONFIG \
    --template="{severity}:{file}:{line}:{message}" \
    --suppress=missingIncludeSystem \
    --suppress=unusedFunction \
    --suppress=unmatchedSuppression \
    "$KERNEL_DIR/src" 2>&1) || true

# Process cppcheck output
if [ -n "$cppcheck_output" ] && [ "$cppcheck_output" != "" ]; then
    echo "$cppcheck_output" | while IFS= read -r line; do
        if [[ "$line" == *"error:"* ]]; then
            echo -e "${RED}$line${NC}"
            CHECK_ERRORS=$((CHECK_ERRORS + 1))
        elif [[ "$line" == *"warning:"* ]] || [[ "$line" == *"performance:"* ]]; then
            echo -e "${YELLOW}$line${NC}"
            CHECK_WARNINGS=$((CHECK_WARNINGS + 1))
        else
            echo "$line"
        fi
    done
else
    echo -e "${GREEN}✓ No issues found${NC}"
fi

echo ""
echo -e "${BLUE}cppcheck summary:${NC}"
echo -e "  Errors:   ${RED}$CHECK_ERRORS${NC}"
echo -e "  Warnings: ${YELLOW}$CHECK_WARNINGS${NC}"
echo ""

# =============================================================================
# Final Summary
# =============================================================================
TOTAL_ERRORS=$((TIDY_ERRORS + CHECK_ERRORS))
TOTAL_WARNINGS=$((TIDY_WARNINGS + CHECK_WARNINGS))

echo "============================================"
echo "Static Analysis Summary"
echo "============================================"
echo "Total Errors:   ${RED}$TOTAL_ERRORS${NC}"
echo "Total Warnings: ${YELLOW}$TOTAL_WARNINGS${NC}"
echo ""

if [ $TOTAL_ERRORS -gt 0 ]; then
    echo -e "${RED}❌ Static analysis FAILED${NC}"
    echo "Please fix the errors above."
    exit 1
elif [ $TOTAL_WARNINGS -gt 0 ]; then
    echo -e "${YELLOW}⚠️  Static analysis passed with warnings${NC}"
    echo "Consider fixing the warnings for better code quality."
    exit 0
else
    echo -e "${GREEN}✅ Static analysis PASSED${NC}"
    echo "No issues found!"
    exit 0
fi
