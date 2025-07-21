#!/bin/bash

set -e

echo "=== CI Debug Script for LPM Library ==="
echo "Working directory: $(pwd)"

# Get the directory where the script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "Script directory: $SCRIPT_DIR"
echo "Project root: $PROJECT_ROOT"

# Change to project root
cd "$PROJECT_ROOT"

echo "Build directory exists: $(test -d build && echo "YES" || echo "NO")"
echo "Test binary exists: $(test -f build/tests/test_basic && echo "YES" || echo "NO")"

cd build

echo ""
echo "=== Environment Information ==="
echo "OS: $(uname -a)"
echo "Compiler version:"
gcc --version | head -1
echo "gdb available: $(which gdb && echo "YES" || echo "NO")"
echo "valgrind available: $(which valgrind && echo "YES" || echo "NO")"

echo ""
echo "=== Library Files ==="
ls -la liblpm* || echo "No library files found"

echo ""
echo "=== Test Binary Information ==="
if [[ -f tests/test_basic ]]; then
    echo "File size: $(ls -lh tests/test_basic | awk '{print $5}')"
    echo "File type: $(file tests/test_basic)"
    echo "Dependencies:"
    ldd tests/test_basic || echo "ldd failed"
else
    echo "ERROR: test_basic binary not found!"
    exit 1
fi

echo ""
echo "=== Step 1: Basic test run ==="
echo "Running test directly..."
./tests/test_basic || {
    exit_code=$?
    echo "Test failed with exit code: $exit_code"
    
    if [[ $exit_code -eq 139 ]]; then
        echo "Exit code 139 indicates SEGFAULT"
    elif [[ $exit_code -eq 134 ]]; then
        echo "Exit code 134 indicates SIGABRT (assertion failure)"
    fi
}

echo ""
echo "=== Step 2: Memory debugging with Valgrind ==="
if command -v valgrind >/dev/null 2>&1; then
    echo "Running with Valgrind..."
    timeout 60s valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
        --error-exitcode=99 --suppressions=tests/valgrind.supp \
        ./tests/test_basic 2>&1 | head -100 || {
        echo "Valgrind run completed (may have timed out or failed)"
    }
else
    echo "Valgrind not available"
fi

echo ""
echo "=== Step 3: GDB stack trace ==="
if command -v gdb >/dev/null 2>&1; then
    echo "Getting stack trace with GDB..."
    cat > debug_script.gdb << 'EOF'
set confirm off
set print pretty on
run
bt
info registers
quit
EOF
    timeout 30s gdb -batch -x debug_script.gdb ./tests/test_basic || {
        echo "GDB run completed"
    }
    rm -f debug_script.gdb
else
    echo "GDB not available"
fi

echo ""
echo "=== Step 4: Strace system call trace ==="
if command -v strace >/dev/null 2>&1; then
    echo "Running with strace..."
    timeout 30s strace -e trace=write,mmap,munmap,brk,mprotect ./tests/test_basic 2>&1 | tail -50 || {
        echo "strace run completed"
    }
else
    echo "strace not available"
fi

echo ""
echo "=== Debug script completed ===" 