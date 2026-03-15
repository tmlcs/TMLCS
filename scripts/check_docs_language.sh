#!/bin/bash
# =============================================================================
# GLOBEX_OS - Documentation Language Checker
# =============================================================================
# Verifies that all comments and documentation are in English
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
KERNEL_DIR="$PROJECT_ROOT/kernels/x86_64"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "============================================"
echo "GLOBEX_OS Documentation Language Checker"
echo "============================================"
echo ""

# Spanish technical terms commonly found in code comments
# Using word boundaries (\b) to avoid false positives with English words
SPANISH_TERMS=(
    "\\bbarrera\\b"
    "\\bmemoria\\b"
    "\\bhilos\\b"
    "\\bsubproceso\\b"
    "\\bbloquear\\b"
    "\\basegurar\\b"
    "\\bgarantizar\\b"
    "\\blectura\\b"
    "\\bescritura\\b"
    "\\bcompartido\\b"
    "\\breordenamiento\\b"
    "\\bimportante\\b"
    "\\bcrítico\\b"
    "\\bseguro\\b"
    "\\bpeligro\\b"
    "\\badvertencia\\b"
    "\\bparámetro\\b"
    "\\bretorno\\b"
    "\\bdevuelve\\b"
    "\\butiliza\\b"
    "\\busar\\b"
    "\\bpara\\s+la\\b"
    "\\bde\\s+la\\b"
    "\\bdel\\b"
    "\\bcon\\s+el\\b"
    "\\bsin\\b"
    "\\bsobre\\b"
    "\\bdesde\\b"
    "\\bhasta\\b"
    "\\bpero\\b"
    "\\bcomo\\b"
    "\\beste\\b"
    "\\besta\\b"
    "\\bestos\\b"
    "\\bestas\\b"
    "\\bque\\b"
    "\\bcual\\b"
    "\\bdonde\\b"
    "\\bcuando\\b"
)

# Build grep pattern
PATTERN=""
for term in "${SPANISH_TERMS[@]}"; do
    if [ -z "$PATTERN" ]; then
        PATTERN="$term"
    else
        PATTERN="$PATTERN|$term"
    fi
done

echo "Searching for Spanish terms in source files..."
echo ""

# Search in .h and .cpp files (excluding string literals)
FOUND_ISSUES=0

while IFS= read -r file; do
    relative_path="${file#$PROJECT_ROOT/}"
    
    # Search for Spanish terms in comments (// and /* */ and * lines)
    matches=$(grep -n "^[[:space:]]*//\|^[[:space:]]*\*" "$file" | grep -iE "$PATTERN" 2>/dev/null || true)
    
    if [ -n "$matches" ]; then
        echo -e "${YELLOW}File: ${relative_path}${NC}"
        echo "$matches" | while IFS= read -r line; do
            echo -e "  ${RED}→${NC} $line"
        done
        echo ""
        ((FOUND_ISSUES++)) || true
    fi
done < <(find "$KERNEL_DIR/src" -name "*.h" -o -name "*.cpp")

echo "============================================"
echo "Summary"
echo "============================================"

if [ $FOUND_ISSUES -eq 0 ]; then
    echo -e "${GREEN}✓ No Spanish terms found in comments${NC}"
    echo "All documentation appears to be in English."
    exit 0
else
    echo -e "${RED}✗ Found $FOUND_ISSUES file(s) with potential Spanish terms${NC}"
    echo ""
    echo "Please review and translate to English."
    echo "Reference: docs/DOCUMENTATION_STYLE_GUIDE.md"
    exit 1
fi
