#!/bin/bash
# =============================================================================
# GLOBEX_OS - Generate compile_commands.json
# =============================================================================
# This script generates a compile_commands.json file for IDE/tool integration.
# This enables clang-tidy, clangd, and other tools to work correctly.
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Project root is parent of scripts directory
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_DIR"

OUTPUT_FILE="$PROJECT_DIR/compile_commands.json"

echo "============================================"
echo "GLOBEX_OS - Compile Commands Generator"
echo "============================================"
echo ""

# Compiler and flags from Makefile
CC="g++"
CFLAGS="-ffreestanding -fno-exceptions -fno-rtti -fno-stack-protector -nostdlib -nostartfiles -O2 -Wall -Wextra -Wpedantic -Werror -Wuninitialized -Wno-unused-command-line-argument"

# Include paths (relative to kernels/x86_64/)
# Note: Using absolute paths for better IDE compatibility
INCLUDES=(
    "-I$PROJECT_DIR/src/kernel"
    "-I$PROJECT_DIR/src/core"
    "-I$PROJECT_DIR/src/core/panic"
    "-I$PROJECT_DIR/src/core/debug"
    "-I$PROJECT_DIR/src/core/coverage"
    "-I$PROJECT_DIR/src/core/log"
    "-I$PROJECT_DIR/src/drivers/console"
    "-I$PROJECT_DIR/src/drivers/vga"
    "-I$PROJECT_DIR/src/drivers/serial"
    "-I$PROJECT_DIR/src/lib/string"
    "-I$PROJECT_DIR/src/lib/spinlock"
    "-I$PROJECT_DIR/src/lib/utils"
    "-I$PROJECT_DIR/src/arch/x86_64/include"
    "-I$PROJECT_DIR/src/arch/x86_64/gdt"
    "-I$PROJECT_DIR/src/arch/x86_64/idt"
    "-I$PROJECT_DIR/tests"
)

# Target for freestanding C++
TARGET="-target x86_64-elf"

# C++ standard
STD="-std=c++17"

# Start JSON array
echo "[" > "$OUTPUT_FILE"

FIRST=true

# Find all C++ source files (from project root perspective)
while IFS= read -r -d '' file; do
    # Skip if file doesn't exist
    if [[ ! -f "$file" ]]; then
        continue
    fi
    
    # Add comma before all entries except the first
    if [[ "$FIRST" == true ]]; then
        FIRST=false
    else
        echo "," >> "$OUTPUT_FILE"
    fi
    
    # Get absolute path to source file
    ABS_FILE="$(realpath "$file")"
    
    # Build command string
    CMD="$CC $TARGET $STD $CFLAGS ${INCLUDES[*]} -c $ABS_FILE"
    
    # Write JSON entry
    cat >> "$OUTPUT_FILE" << EOF
  {
    "directory": "$PROJECT_DIR",
    "file": "$ABS_FILE",
    "command": "$CMD"
  }
EOF

done < <(find "$PROJECT_DIR/src" "$PROJECT_DIR/tests" -name "*.cpp" -print0 2>/dev/null | sort -z)

# Close JSON array
echo "" >> "$OUTPUT_FILE"
echo "]" >> "$OUTPUT_FILE"

echo ""
echo "============================================"
echo "Generated: $OUTPUT_FILE"
echo "============================================"
echo ""

# Count entries
COUNT=$(grep -c '"file":' "$OUTPUT_FILE" || echo "0")
echo "Entries: $COUNT C++ source files"
echo ""

# Verify JSON is valid (if jq is available)
if command -v jq &> /dev/null; then
    if jq empty "$OUTPUT_FILE" 2>/dev/null; then
        echo "✓ JSON validation: PASSED"
    else
        echo "✗ JSON validation: FAILED"
        exit 1
    fi
else
    echo "ℹ Install 'jq' for JSON validation (optional)"
fi

echo ""
echo "Usage with clang-tidy:"
echo "  clang-tidy -p $PROJECT_DIR <source-file.cpp>"
echo ""
echo "Usage with clangd (VS Code, Vim, etc.):"
echo "  The editor will automatically detect compile_commands.json"
echo ""
