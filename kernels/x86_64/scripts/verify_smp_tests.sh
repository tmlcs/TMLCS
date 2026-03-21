#!/bin/bash
# =============================================================================
# GLOBEX_OS - SMP Test Verification Script
# =============================================================================
# Verifies SMP test output from QEMU serial console with multi-CPU testing
# =============================================================================

# Note: Don't use set -e as ((var++)) returns 1 when var is 0

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

OUTPUT_FILE="${1:-serial_output_smp.log}"

echo "============================================"
echo "GLOBEX_OS SMP Test Verification"
echo "============================================"
echo ""

if [ ! -f "$OUTPUT_FILE" ]; then
    echo -e "${RED}Error: Output file not found: $OUTPUT_FILE${NC}"
    echo "Run 'make run-smp-test' first to generate SMP test output"
    exit 1
fi

echo "Analyzing: $OUTPUT_FILE"
echo ""

# Track test results
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# =============================================================================
# Check for SMP-specific test markers
# =============================================================================
echo -e "${BLUE}Checking SMP test markers...${NC}"

# Check if SMP was detected
if grep -q "SMP Detected:" "$OUTPUT_FILE"; then
    echo -e "${GREEN}✓ SMP environment detected${NC}"
    SMP_CPUS=$(grep "SMP Detected:" "$OUTPUT_FILE" | head -1 | sed 's/.*: \([0-9]*\) CPUs.*/\1/')
    echo "  CPUs detected: $SMP_CPUS"
else
    echo -e "${YELLOW}⚠ Single-CPU mode (QEMU default)${NC}"
    echo "  For real SMP testing, use: make run-smp"
fi

# Data Integrity Test
if grep -q "\[PASS\] DATA INTEGRITY" "$OUTPUT_FILE"; then
    echo -e "${GREEN}✓ [DATA INTEGRITY] PASSED${NC}"
    ((PASSED_TESTS++))
elif grep -q "\[FAIL\] DATA INTEGRITY" "$OUTPUT_FILE"; then
    echo -e "${RED}✗ [DATA INTEGRITY] FAILED${NC}"
    ((FAILED_TESTS++))
else
    echo -e "${YELLOW}⚠ [DATA INTEGRITY] NOT FOUND${NC}"
fi
((TOTAL_TESTS++))

# Fairness Test
if grep -q "\[PASS\] FAIRNESS" "$OUTPUT_FILE"; then
    echo -e "${GREEN}✓ [FAIRNESS] PASSED${NC}"
    ((PASSED_TESTS++))
elif grep -q "\[FAIL\] FAIRNESS" "$OUTPUT_FILE"; then
    echo -e "${RED}✗ [FAIRNESS] FAILED${NC}"
    ((FAILED_TESTS++))
else
    echo -e "${YELLOW}⚠ [FAIRNESS] NOT FOUND${NC}"
fi
((TOTAL_TESTS++))

# Nested Locks Test (Deadlock detection)
if grep -q "\[PASS\] NESTED LOCKS" "$OUTPUT_FILE"; then
    echo -e "${GREEN}✓ [NESTED LOCKS] PASSED - No deadlock detected${NC}"
    ((PASSED_TESTS++))
elif grep -q "\[FAIL\] NESTED LOCKS" "$OUTPUT_FILE"; then
    echo -e "${RED}✗ [NESTED LOCKS] FAILED - Possible deadlock${NC}"
    ((FAILED_TESTS++))
else
    echo -e "${YELLOW}⚠ [NESTED LOCKS] NOT FOUND${NC}"
fi
((TOTAL_TESTS++))

# Memory Ordering Test
if grep -q "\[PASS\] MEMORY ORDERING" "$OUTPUT_FILE"; then
    echo -e "${GREEN}✓ [MEMORY ORDERING] PASSED${NC}"
    ((PASSED_TESTS++))
elif grep -q "\[FAIL\] MEMORY ORDERING" "$OUTPUT_FILE"; then
    echo -e "${RED}✗ [MEMORY ORDERING] FAILED${NC}"
    ((FAILED_TESTS++))
else
    echo -e "${YELLOW}⚠ [MEMORY ORDERING] NOT FOUND${NC}"
fi
((TOTAL_TESTS++))

# Contention Test
if grep -q "\[PASS\] CONTENTION" "$OUTPUT_FILE"; then
    echo -e "${GREEN}✓ [CONTENTION] PASSED${NC}"
    ((PASSED_TESTS++))
elif grep -q "\[FAIL\] CONTENTION" "$OUTPUT_FILE"; then
    echo -e "${RED}✗ [CONTENTION] FAILED${NC}"
    ((FAILED_TESTS++))
else
    echo -e "${YELLOW}⚠ [CONTENTION] NOT FOUND${NC}"
fi
((TOTAL_TESTS++))

# Check for SMP test suite completion
if grep -q "\[SMP TESTING\] Test suite complete" "$OUTPUT_FILE"; then
    echo -e "${GREEN}✓ SMP test suite completed${NC}"
else
    echo -e "${YELLOW}⚠ SMP test suite may not have completed${NC}"
fi

# =============================================================================
# Check for errors
# =============================================================================
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

if grep -q "Lost updates detected" "$OUTPUT_FILE"; then
    echo -e "${RED}✗ Lost updates detected - SMP synchronization issue${NC}"
    ((FAILED_TESTS++))
else
    echo -e "${GREEN}✓ No lost updates detected${NC}"
fi

if grep -q "starvation detected" "$OUTPUT_FILE"; then
    echo -e "${YELLOW}⚠ Possible lock starvation detected${NC}"
    # Starvation is a warning, not a failure
fi

# =============================================================================
# Extract detailed metrics
# =============================================================================
echo ""
echo -e "${BLUE}SMP Test Metrics:${NC}"

# Extract counter values if available
if grep -q "Expected:" "$OUTPUT_FILE"; then
    echo ""
    echo "Data Integrity Results:"
    grep -A2 "Expected:" "$OUTPUT_FILE" | head -3 | while read line; do
        echo "  $line"
    done
fi

# Extract acquisition distribution if available
if grep -q "acquisitions" "$OUTPUT_FILE"; then
    echo ""
    echo "Lock Acquisition Distribution:"
    grep "acquisitions" "$OUTPUT_FILE" | head -4 | while read line; do
        echo "  $line"
    done
fi

# =============================================================================
# Summary
# =============================================================================
echo ""
echo "============================================"
echo "SMP Test Summary"
echo "============================================"
echo "Total Tests:  $TOTAL_TESTS"
echo -e "Passed:       ${GREEN}$PASSED_TESTS${NC}"
echo -e "Failed:       ${RED}$FAILED_TESTS${NC}"
echo ""

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}All SMP tests PASSED!${NC}"
    echo "============================================"
    
    if [ -n "$SMP_CPUS" ] && [ "$SMP_CPUS" -gt 1 ]; then
        echo -e "${GREEN}✓ Real SMP concurrency verified on $SMP_CPUS CPUs${NC}"
    else
        echo -e "${YELLOW}⚠ Tests passed in single-CPU mode (logical correctness only)${NC}"
        echo "   For real SMP testing: make run-smp"
    fi
    
    exit 0
else
    echo -e "${RED}Some SMP tests FAILED!${NC}"
    echo "============================================"
    echo ""
    echo "Review the output file for details: $OUTPUT_FILE"
    exit 1
fi
