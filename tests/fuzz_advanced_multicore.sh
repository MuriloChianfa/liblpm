#!/bin/bash

# Advanced multi-core AFL fuzzing with different strategies

set -e

echo "Setting up advanced multi-core AFL fuzzing..."

# Configuration
TOTAL_CORES=$(nproc)
NUM_INSTANCES=$((TOTAL_CORES - 1))  # Leave one core free
FUZZ_TIME=7200  # 2 hours per session
INPUT_DIR="tests/fuzz_input"
OUTPUT_DIR="tests/fuzz_output_advanced"

# Create output directory
mkdir -p $OUTPUT_DIR

# Build if needed
if [ ! -f "build_afl/tests/test_fuzz_advanced" ]; then
    echo "Building with AFL first..."
    ./build_afl.sh
fi

# Set environment variables
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1

echo "System has $TOTAL_CORES cores, using $NUM_INSTANCES for fuzzing"
echo "Input directory: $INPUT_DIR"
echo "Output directory: $OUTPUT_DIR"
echo "Fuzzing time: $FUZZ_TIME seconds per session"

# Function to run AFL instance with different strategies
run_afl_instance() {
    local instance_id=$1
    local strategy=$2
    local output_dir="$OUTPUT_DIR/instance_${instance_id}_${strategy}"
    
    echo "Starting AFL instance $instance_id with strategy: $strategy"
    
    # Different AFL strategies based on instance
    case $strategy in
        "fast")
            # Fast exploration
            timeout $FUZZ_TIME afl-fuzz \
                -i $INPUT_DIR \
                -o $output_dir \
                -x tests/dict.txt \
                -S "fast_$instance_id" \
                -p fast \
                -- build_afl/tests/test_fuzz_advanced @@
            ;;
        "explore")
            # Exploration focused
            timeout $FUZZ_TIME afl-fuzz \
                -i $INPUT_DIR \
                -o $output_dir \
                -x tests/dict.txt \
                -S "explore_$instance_id" \
                -p explore \
                -- build_afl/tests/test_fuzz_advanced @@
            ;;
        "exploit")
            # Exploitation focused
            timeout $FUZZ_TIME afl-fuzz \
                -i $INPUT_DIR \
                -o $output_dir \
                -x tests/dict.txt \
                -S "exploit_$instance_id" \
                -p exploit \
                -- build_afl/tests/test_fuzz_advanced @@
            ;;
        "coverage")
            # Coverage focused
            timeout $FUZZ_TIME afl-fuzz \
                -i $INPUT_DIR \
                -o $output_dir \
                -x tests/dict.txt \
                -S "coverage_$instance_id" \
                -p coverage \
                -- build_afl/tests/test_fuzz_advanced @@
            ;;
        *)
            # Default strategy
            timeout $FUZZ_TIME afl-fuzz \
                -i $INPUT_DIR \
                -o $output_dir \
                -x tests/dict.txt \
                -S "default_$instance_id" \
                -- build_afl/tests/test_fuzz_advanced @@
            ;;
    esac
    
    echo "AFL instance $instance_id ($strategy) completed."
}

# Function to create different input seeds for each instance
create_diverse_seeds() {
    echo "Creating diverse seed inputs..."
    
    # Create different types of test cases
    python3 -c "
import struct
import random

# Create various test cases for different instances
test_cases = [
    # IPv4 focused
    {'name': 'ipv4_basic', 'prefixes': 5, 'lookups': 3, 'max_len': 24},
    # IPv6 focused  
    {'name': 'ipv6_basic', 'prefixes': 3, 'lookups': 2, 'max_len': 64},
    # Edge cases
    {'name': 'edge_cases', 'prefixes': 10, 'lookups': 5, 'max_len': 32},
    # Performance test
    {'name': 'performance', 'prefixes': 20, 'lookups': 10, 'max_len': 16},
]

for i, case in enumerate(test_cases):
    data = bytearray()
    
    # Header
    data.extend(struct.pack('<II', case['prefixes'], case['lookups']))
    
    # Add prefixes
    for j in range(case['prefixes']):
        prefix_len = random.randint(8, case['max_len'])
        data.append(prefix_len)
        data.extend(struct.pack('<I', j))
        
        if case['name'] == 'ipv6_basic':
            # IPv6 prefix
            prefix = [0x20, 0x01, 0x0d, 0xb8] + [j] * 12
        else:
            # IPv4 prefix
            prefix = [192, 168, j, 0]
        
        data.extend(prefix[:prefix_len//8])
    
    # Add lookups
    for j in range(case['lookups']):
        if case['name'] == 'ipv6_basic':
            addr = [0x20, 0x01, 0x0d, 0xb8] + [j] * 12
        else:
            addr = [192, 168, j, 1]
        
        data.extend(addr + [0] * (16 - len(addr)))
        data.extend(struct.pack('<I', j))
    
    with open(f'$INPUT_DIR/test_case_{i}.bin', 'wb') as f:
        f.write(data)
    
    print(f'Created test case {i}: {case[\"name\"]}')
"
}

# Function to monitor all instances
monitor_advanced() {
    echo "Starting advanced monitoring..."
    
    while true; do
        clear
        echo "=== Advanced Multi-Core Fuzzing Status ==="
        echo "Time: $(date)"
        echo "Active instances:"
        
        total_crashes=0
        total_hangs=0
        total_queue=0
        
        for instance_dir in $OUTPUT_DIR/instance_*; do
            if [ -d "$instance_dir" ]; then
                instance_name=$(basename "$instance_dir")
                crashes=$(find "$instance_dir/crashes" -name "id:*" 2>/dev/null | wc -l)
                hangs=$(find "$instance_dir/hangs" -name "id:*" 2>/dev/null | wc -l)
                queue=$(find "$instance_dir/queue" -name "id:*" 2>/dev/null | wc -l)
                
                echo "  $instance_name: $crashes crashes, $hangs hangs, $queue queue"
                
                total_crashes=$((total_crashes + crashes))
                total_hangs=$((total_hangs + hangs))
                total_queue=$((total_queue + queue))
            fi
        done
        
        echo ""
        echo "Totals: $total_crashes crashes, $total_hangs hangs, $total_queue queue items"
        echo "=========================================="
        sleep 60
    done
}

# Function to merge and analyze results
merge_advanced_results() {
    echo "Merging results from all instances..."
    
    mkdir -p "$OUTPUT_DIR/merged"
    mkdir -p "$OUTPUT_DIR/analysis"
    
    # Collect all crashes and hangs
    find $OUTPUT_DIR/instance_* -name "id:*" -exec cp {} "$OUTPUT_DIR/merged/" \;
    
    # Analyze unique crashes
    echo "Analyzing unique crashes..."
    crash_files=$(find "$OUTPUT_DIR/merged" -name "id:*" | head -10)
    
    for crash in $crash_files; do
        if [ -f "$crash" ]; then
            echo "Testing crash: $crash"
            build_afl/tests/test_fuzz_advanced "$crash" 2>&1 | tee "$OUTPUT_DIR/analysis/$(basename "$crash").log"
        fi
    done
    
    # Generate summary report
    cat > "$OUTPUT_DIR/analysis/summary.txt" << EOF
Multi-Core Fuzzing Summary
==========================

Date: $(date)
Duration: $FUZZ_TIME seconds
Instances: $NUM_INSTANCES
Total Crashes: $(find "$OUTPUT_DIR/merged" -name "id:*" | wc -l)
Total Hangs: $(find "$OUTPUT_DIR/merged" -name "id:*" | wc -l)

Instance Breakdown:
$(for instance_dir in $OUTPUT_DIR/instance_*; do
    if [ -d "$instance_dir" ]; then
        instance_name=$(basename "$instance_dir")
        crashes=$(find "$instance_dir/crashes" -name "id:*" 2>/dev/null | wc -l)
        hangs=$(find "$instance_dir/hangs" -name "id:*" 2>/dev/null | wc -l)
        echo "  $instance_name: $crashes crashes, $hangs hangs"
    fi
done)

EOF
}

# Main execution
echo "Creating diverse seed inputs..."
create_diverse_seeds

echo "Starting advanced multi-core fuzzing..."

# Start monitoring
monitor_advanced &
MONITOR_PID=$!

# Define strategies for different instances
strategies=("fast" "explore" "exploit" "coverage" "default")

# Start AFL instances with different strategies
for i in $(seq 0 $((NUM_INSTANCES-1))); do
    strategy=${strategies[$((i % ${#strategies[@]}))]}
    run_afl_instance $i $strategy &
    AFL_PIDS[$i]=$!
    
    # Bind to specific CPU core
    taskset -cp $i ${AFL_PIDS[$i]} 2>/dev/null || true
    
    sleep 3  # Stagger starts
done

echo "All $NUM_INSTANCES AFL instances started with different strategies."

# Wait for completion
for i in $(seq 0 $((NUM_INSTANCES-1))); do
    wait ${AFL_PIDS[$i]}
    echo "AFL instance $i finished"
done

# Stop monitoring
kill $MONITOR_PID 2>/dev/null || true

# Merge and analyze
merge_advanced_results

echo "Advanced multi-core fuzzing complete!"
echo "Check $OUTPUT_DIR/merged/ for crashes"
echo "Check $OUTPUT_DIR/analysis/ for detailed analysis"
