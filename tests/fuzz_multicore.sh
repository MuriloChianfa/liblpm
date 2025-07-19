#!/bin/bash

# Multi-core AFL fuzzing script for LPM implementation

set -e

echo "Setting up multi-core AFL fuzzing..."

# Configuration
NUM_CORES=4  # Adjust based on your system
FUZZ_TIME=3600  # 1 hour per session
INPUT_DIR="tests/fuzz_input"
OUTPUT_DIR="tests/fuzz_output_multicore"

# Create output directory
mkdir -p $OUTPUT_DIR

# Build if needed
if [ ! -f "build_afl/tests/test_fuzz_advanced" ]; then
    echo "Building with AFL first..."
    ./build_afl.sh
fi

# Set environment variables
export AFL_I_DONT_CARE_ABOUT_MISSING_CRASHES=1

echo "Starting multi-core AFL fuzzing with $NUM_CORES cores..."
echo "Input directory: $INPUT_DIR"
echo "Output directory: $OUTPUT_DIR"
echo "Fuzzing time: $FUZZ_TIME seconds per session"

# Function to run AFL instance
run_afl_instance() {
    local instance_id=$1
    local output_dir="$OUTPUT_DIR/instance_$instance_id"
    
    echo "Starting AFL instance $instance_id on core $instance_id..."
    
    # Run AFL with specific core binding
    timeout $FUZZ_TIME afl-fuzz \
        -i $INPUT_DIR \
        -o $output_dir \
        -x tests/dict.txt \
        -S "instance_$instance_id" \
        -- build_afl/tests/test_fuzz_advanced @@
    
    echo "AFL instance $instance_id completed."
}

# Function to monitor and report results
monitor_results() {
    echo "Monitoring fuzzing progress..."
    
    while true; do
        echo "=== Fuzzing Status ==="
        for i in $(seq 0 $((NUM_CORES-1))); do
            if [ -d "$OUTPUT_DIR/instance_$i" ]; then
                crashes=$(find "$OUTPUT_DIR/instance_$i/crashes" -name "id:*" 2>/dev/null | wc -l)
                hangs=$(find "$OUTPUT_DIR/instance_$i/hangs" -name "id:*" 2>/dev/null | wc -l)
                queue=$(find "$OUTPUT_DIR/instance_$i/queue" -name "id:*" 2>/dev/null | wc -l)
                echo "Instance $i: $crashes crashes, $hangs hangs, $queue queue items"
            else
                echo "Instance $i: Not started"
            fi
        done
        echo "====================="
        sleep 30
    done
}

# Function to merge results
merge_results() {
    echo "Merging results from all instances..."
    
    # Create merged output directory
    mkdir -p "$OUTPUT_DIR/merged"
    
    # Merge crashes
    for i in $(seq 0 $((NUM_CORES-1))); do
        if [ -d "$OUTPUT_DIR/instance_$i/crashes" ]; then
            cp -r "$OUTPUT_DIR/instance_$i/crashes"/* "$OUTPUT_DIR/merged/" 2>/dev/null || true
        fi
    done
    
    # Merge hangs
    for i in $(seq 0 $((NUM_CORES-1))); do
        if [ -d "$OUTPUT_DIR/instance_$i/hangs" ]; then
            cp -r "$OUTPUT_DIR/instance_$i/hangs"/* "$OUTPUT_DIR/merged/" 2>/dev/null || true
        fi
    done
    
    echo "Results merged to $OUTPUT_DIR/merged/"
}

# Function to analyze crashes
analyze_crashes() {
    echo "Analyzing crashes..."
    
    if [ -d "$OUTPUT_DIR/merged" ]; then
        crash_count=$(find "$OUTPUT_DIR/merged" -name "id:*" | wc -l)
        echo "Found $crash_count potential crashes"
        
        if [ $crash_count -gt 0 ]; then
            echo "Attempting to reproduce crashes..."
            for crash in "$OUTPUT_DIR/merged"/id:*; do
                if [ -f "$crash" ]; then
                    echo "Reproducing: $crash"
                    build_afl/tests/test_fuzz_advanced "$crash" || echo "Crash confirmed!"
                fi
            done
        fi
    fi
}

# Main execution
echo "Starting multi-core fuzzing..."

# Start monitoring in background
monitor_results &
MONITOR_PID=$!

# Start AFL instances
for i in $(seq 0 $((NUM_CORES-1))); do
    run_afl_instance $i &
    AFL_PIDS[$i]=$!
    
    # Bind to specific CPU core
    taskset -cp $i ${AFL_PIDS[$i]} 2>/dev/null || true
    
    # Small delay between starts
    sleep 2
done

echo "All AFL instances started. Monitoring..."

# Wait for all instances to complete
for i in $(seq 0 $((NUM_CORES-1))); do
    wait ${AFL_PIDS[$i]}
    echo "AFL instance $i finished"
done

# Stop monitoring
kill $MONITOR_PID 2>/dev/null || true

# Merge and analyze results
merge_results
analyze_crashes

echo "Multi-core fuzzing complete!"
echo "Check $OUTPUT_DIR/merged/ for results" 