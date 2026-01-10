#!/usr/bin/env bash
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR_GCC="${PROJECT_ROOT}/build_gcc"
BUILD_DIR_CLANG="${PROJECT_ROOT}/build_clang"
RESULTS_FILE="${PROJECT_ROOT}/compiler_comparison_results.txt"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Compiler Performance Comparison${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check for required tools
for tool in gcc clang cmake; do
    if ! command -v $tool &> /dev/null; then
        echo -e "${RED}Error: $tool is not installed${NC}"
        exit 1
    fi
done

# Display compiler versions
echo -e "${GREEN}Compiler Versions:${NC}"
echo -e "GCC:   $(gcc --version | head -n1)"
echo -e "Clang: $(clang --version | head -n1)"
echo ""

# Function to build with a specific compiler
build_with_compiler() {
    local compiler=$1
    local build_dir=$2
    
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}Building with ${compiler}${NC}"
    echo -e "${YELLOW}========================================${NC}"
    
    # Clean and create build directory
    rm -rf "${build_dir}"
    mkdir -p "${build_dir}"
    
    cd "${build_dir}"
    
    # Configure with CMake
    echo -e "${BLUE}Configuring...${NC}"
    CMAKE_C_COMPILER="${compiler}" cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTS=ON \
        -DBUILD_BENCHMARKS=ON \
        -DENABLE_NATIVE_ARCH=ON \
        "${PROJECT_ROOT}"
    
    # Build
    echo -e "${BLUE}Building...${NC}"
    make -j$(nproc)
    
    echo -e "${GREEN}Build completed successfully with ${compiler}${NC}"
    echo ""
}

# Function to run benchmarks
run_benchmarks() {
    local compiler=$1
    local build_dir=$2
    
    echo -e "${YELLOW}========================================${NC}"
    echo -e "${YELLOW}Running benchmarks with ${compiler}${NC}"
    echo -e "${YELLOW}========================================${NC}"
    
    cd "${build_dir}/benchmarks"
    
    # Run bench_lookup multiple times and average
    echo -e "${BLUE}Running bench_lookup (3 runs)...${NC}"
    local total_runs=3
    
    for i in $(seq 1 $total_runs); do
        echo -e "${BLUE}  Run $i/$total_runs${NC}"
        ./bench_lookup
    done
    
    # Check if DPDK benchmark exists
    if [ -f "./bench_dpdk_comparison" ]; then
        echo -e "${BLUE}Running bench_dpdk_comparison...${NC}"
        ./bench_dpdk_comparison
    else
        echo -e "${YELLOW}DPDK benchmark not available (DPDK not installed)${NC}"
    fi
    
    echo ""
}

# Function to get binary size
get_binary_info() {
    local compiler=$1
    local build_dir=$2
    
    echo -e "${YELLOW}Binary Information for ${compiler}:${NC}"
    
    # Library size
    if [ -f "${build_dir}/liblpm.so" ]; then
        local lib_size=$(ls -lh "${build_dir}/liblpm.so" | awk '{print $5}')
        echo -e "  liblpm.so size: ${lib_size}"
    fi
    
    # Benchmark binary size
    if [ -f "${build_dir}/benchmarks/bench_lookup" ]; then
        local bench_size=$(ls -lh "${build_dir}/benchmarks/bench_lookup" | awk '{print $5}')
        echo -e "  bench_lookup size: ${bench_size}"
    fi
    
    # Strip information
    echo -e "  Library symbol count: $(nm -D "${build_dir}/liblpm.so" 2>/dev/null | wc -l)"
    
    echo ""
}

# Main execution
echo -e "${GREEN}Starting compilation comparison...${NC}"
echo ""

# Build with GCC
build_with_compiler "gcc" "${BUILD_DIR_GCC}"

# Build with Clang
build_with_compiler "clang" "${BUILD_DIR_CLANG}"

# Display binary information
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Binary Information${NC}"
echo -e "${BLUE}========================================${NC}"
get_binary_info "GCC" "${BUILD_DIR_GCC}"
get_binary_info "Clang" "${BUILD_DIR_CLANG}"

# Run benchmarks with both compilers
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Performance Benchmarks${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Save results to file
{
    echo "=========================================="
    echo "Compiler Performance Comparison Results"
    echo "Date: $(date)"
    echo "=========================================="
    echo ""
    echo "System Information:"
    uname -a
    echo ""
    echo "CPU Information:"
    lscpu | grep -E "Model name|Architecture|CPU\(s\)|Thread|Core|MHz"
    echo ""
    echo "Compiler Versions:"
    echo "GCC:   $(gcc --version | head -n1)"
    echo "Clang: $(clang --version | head -n1)"
    echo ""
} > "${RESULTS_FILE}"

# Run GCC benchmarks
{
    echo "=========================================="
    echo "GCC Benchmark Results"
    echo "=========================================="
} >> "${RESULTS_FILE}"
run_benchmarks "gcc" "${BUILD_DIR_GCC}" 2>&1 | tee -a "${RESULTS_FILE}"

# Run Clang benchmarks
{
    echo ""
    echo "=========================================="
    echo "Clang Benchmark Results"
    echo "=========================================="
} >> "${RESULTS_FILE}"
run_benchmarks "clang" "${BUILD_DIR_CLANG}" 2>&1 | tee -a "${RESULTS_FILE}"

# Parse results and display summary
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Comparison Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Extract and display summary
echo "╔════════════════════════════════════════════════════════════════════════════╗"
echo "║                    GCC vs CLANG PERFORMANCE COMPARISON                     ║"
echo "║                           liblpm Benchmark Results                         ║"
echo "╚════════════════════════════════════════════════════════════════════════════╝"
echo ""

# System info
CPU_MODEL=$(lscpu | grep "Model name:" | sed 's/Model name:[[:space:]]*//')
CPU_COUNT=$(lscpu | grep "^CPU(s):" | awk '{print $2}')
GCC_VER=$(gcc --version | head -n1 | awk '{print $3}')
CLANG_VER=$(clang --version | head -n1 | awk '{print $4}')

echo "System: ${CPU_MODEL} (${CPU_COUNT} threads, AVX512F enabled)"
echo "GCC Version: ${GCC_VER}  |  Clang Version: ${CLANG_VER}  |  Build: -O3 -march=native"
echo ""

echo "┌────────────────────────────────────────────────────────────────────────────┐"
echo "│                          PERFORMANCE RESULTS                               │"
echo "└────────────────────────────────────────────────────────────────────────────┘"
echo ""

# Function to extract average performance from results
extract_perf() {
    local file=$1
    local pattern=$2
    grep "$pattern" "$file" | awk '{print $5}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "N/A"}'
}

extract_perf_mlps() {
    local file=$1
    local pattern=$2
    grep "$pattern" "$file" | awk '{print $3}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "N/A"}'
}

# Extract results from the results file
GCC_IPV4_SINGLE_NS=$(extract_perf "${RESULTS_FILE}" "IPv4.*Time per lookup:")
GCC_IPV4_SINGLE_MLPS=$(extract_perf_mlps "${RESULTS_FILE}" "IPv4.*Lookups/sec:")
GCC_IPV4_BATCH_NS=$(grep -A 20 "IPv4 Batch Lookup" "${RESULTS_FILE}" | extract_perf - "Time per lookup:")
GCC_IPV4_BATCH_MLPS=$(grep -A 20 "IPv4 Batch Lookup" "${RESULTS_FILE}" | extract_perf_mlps - "Lookups/sec:")
GCC_IPV6_SINGLE_NS=$(grep -A 20 "IPv6 Single Lookup" "${RESULTS_FILE}" | extract_perf - "Time per lookup:")
GCC_IPV6_SINGLE_MLPS=$(grep -A 20 "IPv6 Single Lookup" "${RESULTS_FILE}" | extract_perf_mlps - "Lookups/sec:")
GCC_IPV6_BATCH_NS=$(grep -A 20 "IPv6 Batch Lookup" "${RESULTS_FILE}" | extract_perf - "Time per lookup:")
GCC_IPV6_BATCH_MLPS=$(grep -A 20 "IPv6 Batch Lookup" "${RESULTS_FILE}" | extract_perf_mlps - "Lookups/sec:")

# Extract from the two sections
GCC_SECTION=$(sed -n '/^GCC Benchmark Results$/,/^Clang Benchmark Results$/p' "${RESULTS_FILE}" | head -n -2)
CLANG_SECTION=$(sed -n '/^Clang Benchmark Results$/,$p' "${RESULTS_FILE}")

# Extract GCC values - looking for "Time per lookup: X.XX ns" and "Lookups/sec: X.XX million" pattern
GCC_IPV4_SINGLE_NS=$(echo "$GCC_SECTION" | grep -A 10 "=== IPv4 Single Lookup" | grep "Time per lookup:" | awk '{print $4}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "0.00"}')
GCC_IPV4_SINGLE_MLPS=$(echo "$GCC_SECTION" | grep -A 10 "=== IPv4 Single Lookup" | grep "Lookups/sec:" | sed 's/million//' | awk '{print $2}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "0.00"}')
GCC_IPV4_BATCH_NS=$(echo "$GCC_SECTION" | grep -A 10 "=== IPv4 Batch Lookup" | grep "Time per lookup:" | awk '{print $4}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "0.00"}')
GCC_IPV4_BATCH_MLPS=$(echo "$GCC_SECTION" | grep -A 10 "=== IPv4 Batch Lookup" | grep "Lookups/sec:" | sed 's/million//' | awk '{print $2}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "0.00"}')
GCC_IPV6_SINGLE_NS=$(echo "$GCC_SECTION" | grep -A 10 "=== IPv6 Single Lookup" | grep "Time per lookup:" | awk '{print $4}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "0.00"}')
GCC_IPV6_SINGLE_MLPS=$(echo "$GCC_SECTION" | grep -A 10 "=== IPv6 Single Lookup" | grep "Lookups/sec:" | sed 's/million//' | awk '{print $2}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "0.00"}')
GCC_IPV6_BATCH_NS=$(echo "$GCC_SECTION" | grep -A 10 "=== IPv6 Batch Lookup" | grep "Time per lookup:" | awk '{print $4}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "0.00"}')
GCC_IPV6_BATCH_MLPS=$(echo "$GCC_SECTION" | grep -A 10 "=== IPv6 Batch Lookup" | grep "Lookups/sec:" | sed 's/million//' | awk '{print $2}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "0.00"}')

# Extract Clang values
CLANG_IPV4_SINGLE_NS=$(echo "$CLANG_SECTION" | grep -A 10 "=== IPv4 Single Lookup" | grep "Time per lookup:" | awk '{print $4}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "0.00"}')
CLANG_IPV4_SINGLE_MLPS=$(echo "$CLANG_SECTION" | grep -A 10 "=== IPv4 Single Lookup" | grep "Lookups/sec:" | sed 's/million//' | awk '{print $2}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "0.00"}')
CLANG_IPV4_BATCH_NS=$(echo "$CLANG_SECTION" | grep -A 10 "=== IPv4 Batch Lookup" | grep "Time per lookup:" | awk '{print $4}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "0.00"}')
CLANG_IPV4_BATCH_MLPS=$(echo "$CLANG_SECTION" | grep -A 10 "=== IPv4 Batch Lookup" | grep "Lookups/sec:" | sed 's/million//' | awk '{print $2}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "0.00"}')
CLANG_IPV6_SINGLE_NS=$(echo "$CLANG_SECTION" | grep -A 10 "=== IPv6 Single Lookup" | grep "Time per lookup:" | awk '{print $4}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "0.00"}')
CLANG_IPV6_SINGLE_MLPS=$(echo "$CLANG_SECTION" | grep -A 10 "=== IPv6 Single Lookup" | grep "Lookups/sec:" | sed 's/million//' | awk '{print $2}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "0.00"}')
CLANG_IPV6_BATCH_NS=$(echo "$CLANG_SECTION" | grep -A 10 "=== IPv6 Batch Lookup" | grep "Time per lookup:" | awk '{print $4}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "0.00"}')
CLANG_IPV6_BATCH_MLPS=$(echo "$CLANG_SECTION" | grep -A 10 "=== IPv6 Batch Lookup" | grep "Lookups/sec:" | sed 's/million//' | awk '{print $2}' | awk '{sum+=$1; count++} END {if(count>0) printf "%.2f", sum/count; else print "0.00"}')

# Calculate winners
calc_winner() {
    local gcc=$1
    local clang=$2
    local better_is_lower=$3  # true if lower is better
    
    if [ "$better_is_lower" = "true" ]; then
        result=$(echo "$gcc $clang" | awk '{
            if ($1 < $2) {
                diff = (($2 - $1) / $2) * 100
                printf "GCC +%.1f%%", diff
            } else if ($2 < $1) {
                diff = (($1 - $2) / $1) * 100
                printf "Clang +%.1f%%", diff
            } else {
                print "~ Tied"
            }
        }')
    else
        result=$(echo "$gcc $clang" | awk '{
            if ($1 > $2) {
                diff = (($1 - $2) / $2) * 100
                printf "GCC +%.1f%%", diff
            } else if ($2 > $1) {
                diff = (($2 - $1) / $1) * 100
                printf "Clang +%.1f%%", diff
            } else {
                print "~ Tied"
            }
        }')
    fi
    echo "$result"
}

IPV4_SINGLE_WINNER=$(calc_winner "$GCC_IPV4_SINGLE_NS" "$CLANG_IPV4_SINGLE_NS" "true")
IPV4_BATCH_WINNER=$(calc_winner "$GCC_IPV4_BATCH_NS" "$CLANG_IPV4_BATCH_NS" "true")
IPV6_SINGLE_WINNER=$(calc_winner "$GCC_IPV6_SINGLE_NS" "$CLANG_IPV6_SINGLE_NS" "true")
IPV6_BATCH_WINNER=$(calc_winner "$GCC_IPV6_BATCH_NS" "$CLANG_IPV6_BATCH_NS" "true")

# Display results table
printf "%-26s │ %17s │ %17s\n" "Test Case" "GCC Result" "Clang Result"
echo "───────────────────────────┼───────────────────┼───────────────────"
printf "%-26s │ %13s ns/op │ %13s ns/op\n" "IPv4 Single Lookup" "$GCC_IPV4_SINGLE_NS" "$CLANG_IPV4_SINGLE_NS"
printf "%-26s │ %12s M ops/s │ %12s M ops/s\n" "" "$GCC_IPV4_SINGLE_MLPS" "$CLANG_IPV4_SINGLE_MLPS"
echo "───────────────────────────┼───────────────────┼───────────────────"
printf "%-26s │ %13s ns/op │ %13s ns/op\n" "IPv4 Batch Lookup (256)" "$GCC_IPV4_BATCH_NS" "$CLANG_IPV4_BATCH_NS"
printf "%-26s │ %12s M ops/s │ %12s M ops/s\n" "" "$GCC_IPV4_BATCH_MLPS" "$CLANG_IPV4_BATCH_MLPS"
echo "───────────────────────────┼───────────────────┼───────────────────"
printf "%-26s │ %13s ns/op │ %13s ns/op\n" "IPv6 Single Lookup" "$GCC_IPV6_SINGLE_NS" "$CLANG_IPV6_SINGLE_NS"
printf "%-26s │ %12s M ops/s │ %12s M ops/s\n" "" "$GCC_IPV6_SINGLE_MLPS" "$CLANG_IPV6_SINGLE_MLPS"
echo "───────────────────────────┼───────────────────┼───────────────────"
printf "%-26s │ %13s ns/op │ %13s ns/op\n" "IPv6 Batch Lookup (256)" "$GCC_IPV6_BATCH_NS" "$CLANG_IPV6_BATCH_NS"
printf "%-26s │ %12s M ops/s │ %12s M ops/s\n" "" "$GCC_IPV6_BATCH_MLPS" "$CLANG_IPV6_BATCH_MLPS"
echo "───────────────────────────┴───────────────────┴───────────────────"
echo ""

echo "┌────────────────────────────────────────────────────────────────────────────┐"
echo "│                            BINARY SIZES                                    │"
echo "└────────────────────────────────────────────────────────────────────────────┘"
echo ""

# Extract version from CMakeLists.txt
LIB_VERSION=$(grep "^project(liblpm VERSION" "${PROJECT_ROOT}/CMakeLists.txt" | sed -n 's/.*VERSION \([0-9.]*\).*/\1/p')
if [ -z "$LIB_VERSION" ]; then
    LIB_VERSION="1.2.0"  # fallback
fi

# Find the actual library file (could be .so.X.Y.Z)
GCC_LIB_FILE=$(ls ${BUILD_DIR_GCC}/liblpm.so.${LIB_VERSION} 2>/dev/null)
CLANG_LIB_FILE=$(ls ${BUILD_DIR_CLANG}/liblpm.so.${LIB_VERSION} 2>/dev/null)

GCC_LIB_SIZE=$(ls -lh ${GCC_LIB_FILE} 2>/dev/null | awk '{print $5}')
CLANG_LIB_SIZE=$(ls -lh ${CLANG_LIB_FILE} 2>/dev/null | awk '{print $5}')
GCC_BENCH_SIZE=$(ls -lh ${BUILD_DIR_GCC}/benchmarks/bench_lookup 2>/dev/null | awk '{print $5}')
CLANG_BENCH_SIZE=$(ls -lh ${BUILD_DIR_CLANG}/benchmarks/bench_lookup 2>/dev/null | awk '{print $5}')

if [ "$GCC_LIB_SIZE" = "$CLANG_LIB_SIZE" ]; then
    LIB_DIFF="Identical"
else
    LIB_DIFF="Different"
fi

if [ "$GCC_BENCH_SIZE" = "$CLANG_BENCH_SIZE" ]; then
    BENCH_DIFF="Identical"
else
    BENCH_DIFF="Different"
fi

printf "Library (liblpm.so.%s):     GCC: %-6s │  Clang: %-6s │  %s\n" "$LIB_VERSION" "$GCC_LIB_SIZE" "$CLANG_LIB_SIZE" "$LIB_DIFF"
printf "Benchmark (bench_lookup):      GCC: %-6s │  Clang: %-6s │  %s\n" "$GCC_BENCH_SIZE" "$CLANG_BENCH_SIZE" "$BENCH_DIFF"
echo ""

echo "Results saved to: ${RESULTS_FILE}"
echo "Full analysis: ./compiler_comparison_summary.md"
echo ""

