#!/bin/bash
# =============================================================================
# GLOBEX_OS - Static Analysis Script
# =============================================================================
# Runs static analysis tools on the codebase
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SRC_DIR="$PROJECT_ROOT/0_008_x64/src"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "============================================"
echo "GLOBEX_OS Static Analysis"
echo "============================================"
echo ""

# Track overall status
OVERALL_STATUS=0

# =============================================================================
# clang-tidy
# =============================================================================
echo -e "${BLUE}Running clang-tidy...${NC}"
echo "-------------------------------------------"

if command -v clang-tidy &> /dev/null; then
    echo -e "${GREEN}✓ clang-tidy found${NC}"
    
    # Find all C++ files
    find "$SRC_DIR" -name "*.cpp" | while read -r file; do
        echo "Analyzing: ${file#$PROJECT_ROOT/}"
        clang-tidy "$file" -- -I "$SRC_DIR/intf" -std=c++17 || true
    done
    
    echo -e "${GREEN}✓ clang-tidy complete${NC}"
else
    echo -e "${YELLOW}⚠ clang-tidy not found, skipping...${NC}"
    echo "Install with: sudo apt-get install clang-tidy"
fi

echo ""

# =============================================================================
# cppcheck
# =============================================================================
echo -e "${BLUE}Running cppcheck...${NC}"
echo "-------------------------------------------"

if command -v cppcheck &> /dev/null; then
    echo -e "${GREEN}✓ cppcheck found${NC}"
    
    cppcheck \
        --enable=all \
        --std=c++17 \
        --inconclusive \
        --inline-suppr \
        -I "$SRC_DIR/intf" \
        --suppress=missingIncludeSystem \
        "$SRC_DIR" 2>&1 || true
    
    echo -e "${GREEN}✓ cppcheck complete${NC}"
else
    echo -e "${YELLOW}⚠ cppcheck not found, skipping...${NC}"
    echo "Install with: sudo apt-get install cppcheck"
fi

echo ""

# =============================================================================
# Include what you use (IWYU)
# =============================================================================
echo -e "${BLUE}Checking include usage...${NC}"
echo "-------------------------------------------"

if command -v iwyu &> /dev/null; then
    echo -e "${GREEN}✓ iwyu found${NC}"
    
    find "$SRC_DIR" -name "*.cpp" | head -5 | while read -r file; do
        echo "Checking: ${file#$PROJECT_ROOT/}"
        iwyu "$file" -I "$SRC_DIR/intf" -std=c++17 || true
    done
    
    echo -e "${GREEN}✓ IWYU check complete${NC}"
else
    echo -e "${YELLOW}⚠ IWYU not found, skipping...${NC}"
    echo "Install with: sudo apt-get install include-what-you-use"
fi

echo ""

# =============================================================================
# Summary
# =============================================================================
echo "============================================"
if [ $OVERALL_STATUS -eq 0 ]; then
    echo -e "${GREEN}Analysis complete!${NC}"
else
    echo -e "${RED}Analysis completed with issues${NC}"
fi
echo "============================================"

exit $OVERALL_STATUS
