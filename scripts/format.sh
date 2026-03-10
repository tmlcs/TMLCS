#!/bin/bash
# =============================================================================
# GLOBEX_OS - Code Formatting Script
# =============================================================================
# Formats all C++ source files according to .clang-format configuration
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "============================================"
echo "GLOBEX_OS Code Formatter"
echo "============================================"
echo ""

# Check if clang-format is available
if ! command -v clang-format &> /dev/null; then
    echo -e "${RED}Error: clang-format not found${NC}"
    echo "Please install clang-format:"
    echo "  Ubuntu/Debian: sudo apt-get install clang-format"
    echo "  macOS: brew install clang-format"
    exit 1
fi

echo -e "${GREEN}✓ clang-format found: $(clang-format --version | head -n1)${NC}"
echo ""

# Find all C++ files in kernels/x86_64
echo "Finding C++ source files..."
CPP_FILES=$(find "$PROJECT_ROOT/kernels/x86_64/src" -name "*.cpp" -o -name "*.h" -o -name "*.hpp" 2>/dev/null | wc -l)
echo "Found $CPP_FILES files to format"
echo ""

# Format files
echo "Formatting files..."
find "$PROJECT_ROOT/kernels/x86_64/src" -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | while read -r file; do
    clang-format -i "$file"
    echo -e "${GREEN}✓${NC} Formatted: ${file#$PROJECT_ROOT/}"
done

echo ""
echo "============================================"
echo -e "${GREEN}Formatting complete!${NC}"
echo "============================================"
