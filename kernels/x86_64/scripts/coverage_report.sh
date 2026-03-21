#!/bin/bash
# ==========================================
# GLOBEX_OS Code Coverage Report Generator
# ==========================================
# This script generates code coverage reports from serial output
# collected during QEMU kernel execution with COVERAGE_ENABLE=1.
#
# Usage: ./scripts/coverage_report.sh [serial_output_file] [output_dir]
#
# Output:
#   - coverage_output/coverage_report.txt (text summary)
#   - coverage_output/coverage_detailed.txt (per-function details)
# ==========================================

set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_ROOT="$(dirname "$KERNEL_DIR")"
SERIAL_FILE="${1:-${KERNEL_DIR}/serial_output.log}"
OUTPUT_DIR="${2:-${KERNEL_DIR}/coverage_output}"
COVERAGE_FILE="${OUTPUT_DIR}/coverage_report.txt"
DETAILED_FILE="${OUTPUT_DIR}/coverage_detailed.txt"

# Colors for terminal output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ==========================================
# Helper Functions
# ==========================================

print_header() {
    echo -e "${BLUE}============================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}============================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

# ==========================================
# Setup
# ==========================================

print_header "GLOBEX_OS Code Coverage Report Generator"

# Check for serial output file
if [ ! -f "$SERIAL_FILE" ]; then
    print_error "Serial output file not found: $SERIAL_FILE"
    echo ""
    echo "Please run the kernel with 'make run-coverage' or 'make run-serial' first."
    echo ""
    exit 1
fi

# Check for coverage data in serial output
if ! grep -q "\[COV\]" "$SERIAL_FILE" && ! grep -q "Code Coverage Report" "$SERIAL_FILE"; then
    print_error "No coverage data found in serial output!"
    echo ""
    echo "Please build with COVERAGE_ENABLE=1:"
    echo "  make build-coverage && make run-serial"
    echo ""
    exit 1
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

print_success "Found coverage data in $SERIAL_FILE"

# ==========================================
# Extract Coverage Data
# ==========================================

print_header "Extracting Coverage Data"

# Extract the coverage report section from serial output
{
    echo "=========================================="
    echo "GLOBEX_OS Code Coverage Report"
    echo "=========================================="
    echo ""
    echo "Source: $SERIAL_FILE"
    echo "Generated: $(date)"
    echo ""
    
    # Extract coverage section
    in_coverage=false
    while IFS= read -r line; do
        if [[ "$line" == *"GLOBEX_OS Code Coverage Report"* ]]; then
            in_coverage=true
        fi
        if [[ "$line" == *"=========================================="* ]] && $in_coverage; then
            echo "$line"
            if [[ "$line" == *"Summary"* ]] || [[ "$line" == *"Line Coverage:"* ]]; then
                # Continue until end of summary
                :
            elif [[ "$line" == *"=========================================="* ]] && [[ "$line" != *"GLOBEX_OS"* ]]; then
                # Check if this is end of report
                if grep -q "Line Coverage:" <<< "$(tail -20 "$SERIAL_FILE")"; then
                    echo "$line"
                    break
                fi
            fi
        fi
        if $in_coverage; then
            echo "$line"
        fi
    done < "$SERIAL_FILE"
    
} > "$COVERAGE_FILE"

print_success "Coverage report extracted: $COVERAGE_FILE"

# ==========================================
# Generate Detailed Report
# ==========================================

print_header "Generating Detailed Coverage Report"

{
    echo "=========================================="
    echo "GLOBEX_OS Detailed Coverage Analysis"
    echo "=========================================="
    echo ""
    echo "Source: $SERIAL_FILE"
    echo "Generated: $(date)"
    echo ""
    echo "=========================================="
    echo "Functions by Coverage Status"
    echo "=========================================="
    echo ""
    
    # Extract function data from coverage section
    echo "COVERED FUNCTIONS (executed at least once):"
    echo "--------------------------------------------"
    
    # Parse coverage table - functions with hits > 0
    in_table=false
    while IFS= read -r line; do
        if [[ "$line" == *"Function"* ]] && [[ "$line" == *"Line"* ]] && [[ "$line" == *"Hits"* ]]; then
            in_table=true
            continue
        fi
        if [[ "$line" == *"----------------------------------"* ]]; then
            continue
        fi
        if $in_table; then
            if [[ "$line" == *"========================================"* ]]; then
                break
            fi
            # Parse function line: "function_name | line | hits"
            if [[ "$line" =~ ^([a-zA-Z_][a-zA-Z0-9_]*)[[:space:]]*\|[[:space:]]*([0-9]+)[[:space:]]*\|[[:space:]]*([0-9]+) ]]; then
                fn="${BASH_REMATCH[1]}"
                line_num="${BASH_REMATCH[2]}"
                hits="${BASH_REMATCH[3]}"
                if [ "$hits" -gt 0 ]; then
                    printf "  %-30s line %s (%s hits)\n" "$fn" "$line_num" "$hits"
                fi
            fi
        fi
    done < "$SERIAL_FILE"
    
    echo ""
    echo "UNCOVERED FUNCTIONS (never executed):"
    echo "--------------------------------------"
    
    # List functions that exist but weren't hit
    # This requires comparing with source files
    echo "(Analysis requires source comparison - see uncovered lines below)"
    
    echo ""
    echo "=========================================="
    echo "Uncovered Lines by File"
    echo "=========================================="
    echo ""
    
    # Find source files and check coverage
    for src_file in $(find "$KERNEL_DIR/src" -name "*.cpp" -o -name "*.h" | sort); do
        filename=$(basename "$src_file")
        
        # Check if this file has any coverage data
        file_has_coverage=false
        while IFS= read -r line; do
            if [[ "$line" == *"$filename"* ]]; then
                file_has_coverage=true
                break
            fi
        done < "$COVERAGE_FILE"
        
        if ! $file_has_coverage; then
            echo "File: $filename - NO COVERAGE DATA"
            echo ""
        fi
    done
    
    echo ""
    echo "=========================================="
    echo "Coverage Statistics"
    echo "=========================================="
    echo ""
    
    # Extract statistics from coverage report
    total_fn=$(grep "Total Functions:" "$COVERAGE_FILE" 2>/dev/null | awk '{print $3}' || echo "0")
    hit_fn=$(grep "Hit Functions:" "$COVERAGE_FILE" 2>/dev/null | awk '{print $3}' || echo "0")
    total_lines=$(grep "Total Lines:" "$COVERAGE_FILE" 2>/dev/null | awk '{print $3}' || echo "0")
    hit_lines=$(grep "Hit Lines:" "$COVERAGE_FILE" 2>/dev/null | awk '{print $3}' || echo "0")
    line_pct=$(grep "Line Coverage:" "$COVERAGE_FILE" 2>/dev/null | awk '{print $3}' | tr -d '%' || echo "0")
    
    echo "Total Functions:   $total_fn"
    echo "Hit Functions:     $hit_fn"
    if [ "$total_fn" -gt 0 ] 2>/dev/null; then
        fn_pct=$((hit_fn * 100 / total_fn))
        echo "Function Coverage: $fn_pct%"
    else
        echo "Function Coverage: N/A"
    fi
    echo ""
    echo "Total Lines:       $total_lines"
    echo "Hit Lines:         $hit_lines"
    echo "Line Coverage:     ${line_pct}%"
    echo ""
    
    echo "=========================================="
    echo "Coverage Quality Assessment"
    echo "=========================================="
    echo ""
    
    # Assess coverage quality
    if [ "${line_pct:-0}" -ge 80 ] 2>/dev/null; then
        echo "Status: EXCELLENT - High coverage achieved"
    elif [ "${line_pct:-0}" -ge 60 ] 2>/dev/null; then
        echo "Status: GOOD - Moderate coverage achieved"
    elif [ "${line_pct:-0}" -ge 40 ] 2>/dev/null; then
        echo "Status: FAIR - Basic coverage achieved"
    else
        echo "Status: NEEDS IMPROVEMENT - Low coverage"
    fi
    
    echo ""
    echo "=========================================="
    echo "Recommendations"
    echo "=========================================="
    echo ""
    
    if [ "${line_pct:-0}" -lt 80 ] 2>/dev/null; then
        echo "1. Add more test cases to increase coverage"
        echo "2. Review uncovered functions for dead code"
        echo "3. Consider edge cases not currently tested"
        echo "4. Add COVERAGE_TRACK() macros to uncovered functions"
    else
        echo "Coverage is excellent! Consider:"
        echo "1. Adding boundary condition tests"
        echo "2. Testing error paths more thoroughly"
        echo "3. Adding integration tests"
    fi
    
    echo ""
    
} > "$DETAILED_FILE"

print_success "Detailed report generated: $DETAILED_FILE"

# ==========================================
# Display Summary
# ==========================================

print_header "Coverage Summary"

# Extract key metrics
total_fn=$(grep "Total Functions:" "$COVERAGE_FILE" 2>/dev/null | awk '{print $3}' || echo "0")
hit_fn=$(grep "Hit Functions:" "$COVERAGE_FILE" 2>/dev/null | awk '{print $3}' || echo "0")
total_lines=$(grep "Total Lines:" "$COVERAGE_FILE" 2>/dev/null | awk '{print $3}' || echo "0")
hit_lines=$(grep "Hit Lines:" "$COVERAGE_FILE" 2>/dev/null | awk '{print $3}' || echo "0")
line_pct=$(grep "Line Coverage:" "$COVERAGE_FILE" 2>/dev/null | awk '{print $3}' | tr -d '%' || echo "0")

echo ""
echo "  Functions: $hit_fn / $total_fn"
echo "  Lines:     $hit_lines / $total_lines (${line_pct}%)"
echo ""

if [ "${line_pct:-0}" -ge 80 ] 2>/dev/null; then
    print_success "Excellent coverage!"
elif [ "${line_pct:-0}" -ge 60 ] 2>/dev/null; then
    print_warning "Good coverage, room for improvement"
else
    print_warning "Coverage needs improvement"
fi

echo ""
echo "Reports available at:"
echo "  Summary:  $COVERAGE_FILE"
echo "  Detailed: $DETAILED_FILE"
echo ""

print_success "Coverage report generation complete!"
