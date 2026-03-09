#!/bin/bash
# =============================================================================
# GLOBEX_OS - Test Verification Script
# =============================================================================
# Verifies test output from QEMU serial console
# =============================================================================

# Don't use set -e as ((var++)) returns 1 when var is 0

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

OUTPUT_FILE="${1:-test_output.log}"

echo "============================================"
echo "GLOBEX_OS Test Verification"
echo "============================================"
echo ""

if [ ! -f "$OUTPUT_FILE" ]; then
    echo -e "${RED}Error: Output file not found: $OUTPUT_FILE${NC}"
    echo "Run 'make test' first to generate test output"
    exit 1
fi

echo "Analyzing: $OUTPUT_FILE"
echo ""

# Track test results
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# =============================================================================
# Check for test markers
# =============================================================================
echo -e "${BLUE}Checking test markers...${NC}"

# BSS Test
if grep -q "\[BSS TEST\] PASSED" "$OUTPUT_FILE"; then
    echo -e "${GREEN}✓ [BSS TEST] PASSED${NC}"
    ((PASSED_TESTS++))
else
    echo -e "${RED}✗ [BSS TEST] FAILED${NC}"
    ((FAILED_TESTS++))
fi
((TOTAL_TESTS++))

# Memory Test
if grep -q "\[MEM TEST\] PASSED" "$OUTPUT_FILE"; then
    echo -e "${GREEN}✓ [MEM TEST] PASSED${NC}"
    ((PASSED_TESTS++))
else
    echo -e "${RED}✗ [MEM TEST] FAILED${NC}"
    ((FAILED_TESTS++))
fi
((TOTAL_TESTS++))

# Color Test
if grep -q "\[COLOR TEST\] PASSED" "$OUTPUT_FILE"; then
    echo -e "${GREEN}✓ [COLOR TEST] PASSED${NC}"
    ((PASSED_TESTS++))
else
    echo -e "${RED}✗ [COLOR TEST] FAILED${NC}"
    ((FAILED_TESTS++))
fi
((TOTAL_TESTS++))

# Debug Macros Test
if grep -q "\[DEBUG MACROS\] All tests passed" "$OUTPUT_FILE"; then
    echo -e "${GREEN}✓ [DEBUG MACROS] PASSED${NC}"
    ((PASSED_TESTS++))
else
    echo -e "${RED}✗ [DEBUG MACROS] FAILED${NC}"
    ((FAILED_TESTS++))
fi
((TOTAL_TESTS++))

# Print Functions Test
if grep -q "\[PRINT FUNCTIONS\] All tests passed" "$OUTPUT_FILE"; then
    echo -e "${GREEN}✓ [PRINT FUNCTIONS] PASSED${NC}"
    ((PASSED_TESTS++))
else
    echo -e "${RED}✗ [PRINT FUNCTIONS] FAILED${NC}"
    ((FAILED_TESTS++))
fi
((TOTAL_TESTS++))

# Query Functions Test
if grep -q "\[QUERY FUNCTIONS\] All tests passed" "$OUTPUT_FILE"; then
    echo -e "${GREEN}✓ [QUERY FUNCTIONS] PASSED${NC}"
    ((PASSED_TESTS++))
else
    echo -e "${RED}✗ [QUERY FUNCTIONS] FAILED${NC}"
    ((FAILED_TESTS++))
fi
((TOTAL_TESTS++))

# Serial Signed Numbers Test
if grep -q "\[SERIAL SIGNED\] All tests passed" "$OUTPUT_FILE"; then
    echo -e "${GREEN}✓ [SERIAL SIGNED] PASSED${NC}"
    ((PASSED_TESTS++))
else
    echo -e "${RED}✗ [SERIAL SIGNED] FAILED${NC}"
    ((FAILED_TESTS++))
fi
((TOTAL_TESTS++))

# Hardware Info Test
if grep -q "\[HARDWARE INFO\] CPU detection complete" "$OUTPUT_FILE"; then
    echo -e "${GREEN}✓ [HARDWARE INFO] PASSED${NC}"
    ((PASSED_TESTS++))
else
    echo -e "${RED}✗ [HARDWARE INFO] FAILED${NC}"
    ((FAILED_TESTS++))
fi
((TOTAL_TESTS++))

# Check for panic messages
echo ""
echo -e "${BLUE}Checking for errors...${NC}"

if grep -q "KERNEL PANIC" "$OUTPUT_FILE"; then
    echo -e "${RED}✗ KERNEL PANIC detected in output${NC}"
    ((FAILED_TESTS++))
else
    echo -e "${GREEN}✓ No kernel panics${NC}"
fi

if grep -q "ASSERT FAILED" "$OUTPUT_FILE"; then
    echo -e "${RED}✗ ASSERT FAILURE detected in output${NC}"
    ((FAILED_TESTS++))
else
    echo -e "${GREEN}✓ No assertion failures${NC}"
fi

# =============================================================================
# Summary
# =============================================================================
echo ""
echo "============================================"
echo "Test Summary"
echo "============================================"
echo "Total Tests:  $TOTAL_TESTS"
echo -e "Passed:       ${GREEN}$PASSED_TESTS${NC}"
echo -e "Failed:       ${RED}$FAILED_TESTS${NC}"
echo ""

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}All tests PASSED!${NC}"
    echo "============================================"
    exit 0
else
    echo -e "${RED}Some tests FAILED!${NC}"
    echo "============================================"
    exit 1
fi
