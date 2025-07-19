#!/bin/bash

# Setup script for AFL-style fuzzing of LPM implementation

set -e

echo "Setting up fuzzing environment for LPM..."

# Create fuzzing directories
mkdir -p fuzz_input
mkdir -p fuzz_output

# Create initial test cases
echo "Creating initial test cases..."

# Create binary test data using Python
python3 -c "
import struct

# Create a simple test case
data = bytearray()

# Header: 2 prefixes, 1 lookup
data.extend(struct.pack('<II', 2, 1))

# Prefix 1: 192.168.0.0/24
data.append(24)  # prefix length
data.extend(struct.pack('<I', 1))  # next hop
data.extend([192, 168, 0])  # prefix bytes

# Prefix 2: 10.0.0.0/8  
data.append(8)  # prefix length
data.extend(struct.pack('<I', 2))  # next hop
data.extend([10])  # prefix bytes

# Lookup: 192.168.0.1
data.extend([192, 168, 0, 1] + [0] * 12)  # 16-byte address
data.extend(struct.pack('<I', 1))  # expected result

with open('fuzz_input/test1.bin', 'wb') as f:
    f.write(data)
print('Created test1.bin')
"

echo "Creating build script for AFL..."

# Create AFL build script with fixes
cat > build_afl.sh << 'EOF'
#!/bin/bash

# Build with AFL instrumentation
export CC=afl-clang
export CXX=afl-clang++

# Clean and rebuild
rm -rf build_afl
mkdir build_afl
cd build_afl

# Configure with AFL and disable problematic optimizations
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-sanitize=undefined' \
      ..

# Build
make -j$(nproc)

echo "AFL build complete. Run with:"
echo "afl-fuzz -i ../tests/fuzz_input -o ../tests/fuzz_output ./tests/test_fuzz_advanced"
EOF

chmod +x build_afl.sh

echo "Creating fuzzing runner script..."

# Create fuzzing runner
cat > run_fuzz.sh << 'EOF'
#!/bin/bash

# Run AFL fuzzing
if [ ! -f "build_afl/tests/test_fuzz_advanced" ]; then
    echo "Building with AFL first..."
    ./build_afl.sh
fi

echo "Starting AFL fuzzing..."
echo "Input directory: tests/fuzz_input"
echo "Output directory: tests/fuzz_output"

# Set environment variable to ignore core pattern issues
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1

# Run AFL
afl-fuzz -i tests/fuzz_input -o tests/fuzz_output -x tests/dict.txt -- build_afl/tests/test_fuzz_advanced @@

echo "Fuzzing complete. Check tests/fuzz_output for results."
EOF

chmod +x run_fuzz.sh

# Create dictionary for AFL
cat > tests/dict.txt << 'EOF'
# AFL dictionary for LPM fuzzing
# Common IPv4 prefixes
192.168.0.0/16
10.0.0.0/8
172.16.0.0/12
127.0.0.0/8
0.0.0.0/0

# Common IPv6 prefixes
2001:db8::/32
::/0
::1/128
fe80::/10

# Edge cases
0.0.0.0/0
255.255.255.255/32
EOF

echo "Creating continuous fuzzing script..."

# Create continuous fuzzing script
cat > continuous_fuzz.sh << 'EOF'
#!/bin/bash

# Continuous fuzzing with crash detection
set -e

echo "Starting continuous fuzzing..."

# Build if needed
if [ ! -f "build_afl/tests/test_fuzz_advanced" ]; then
    echo "Building with AFL..."
    ./build_afl.sh
fi

# Create output directory
mkdir -p fuzz_results

# Set environment variable to ignore core pattern issues
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1

# Run fuzzing with timeout
timeout 3600 afl-fuzz -i tests/fuzz_input -o fuzz_results -x tests/dict.txt -- build_afl/tests/test_fuzz_advanced @@

echo "Fuzzing session complete."

# Check for crashes
if [ -d "fuzz_results/crashes" ] && [ "$(ls -A fuzz_results/crashes)" ]; then
    echo "CRASHES DETECTED!"
    ls -la fuzz_results/crashes/
    
    # Try to reproduce crashes
    echo "Attempting to reproduce crashes..."
    for crash in fuzz_results/crashes/id:*; do
        echo "Reproducing: $crash"
        build_afl/tests/test_fuzz_advanced "$crash"
    done
else
    echo "No crashes detected."
fi
EOF

chmod +x continuous_fuzz.sh

echo "Fuzzing setup complete!"
echo ""
echo "To start fuzzing:"
echo "1. Run: ./run_fuzz.sh"
echo "2. Or run continuous fuzzing: ./continuous_fuzz.sh"
echo ""
echo "The fuzzing tests will test:"
echo "- Memory safety with random inputs"
echo "- Edge cases in prefix handling"
echo "- Bitmap operations"
echo "- Custom allocators"
echo "- IPv4 and IPv6 functionality"
echo "- Batch operations"
echo "- Result management"
