#!/bin/bash
#
# DPDK Comparison Benchmark Runner
#
# Runs liblpm vs DPDK benchmark by uploading pre-built Docker image
#
# Usage:
#   ./scripts/run_dpdk_benchmark.sh              # Run locally
#   ./scripts/run_dpdk_benchmark.sh root@192.168.0.13  # Run remotely
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

IMAGE_NAME="liblpm:dpdk"
CONTAINER_NAME="liblpm_dpdk_bench"

# Parse arguments
if [ $# -eq 0 ]; then
    MODE="local"
    REMOTE_HOST=""
else
    MODE="remote"
    REMOTE_HOST="$1"
fi

if [ "$MODE" == "local" ]; then
    # ==========================================
    # LOCAL EXECUTION
    # ==========================================
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  DPDK Benchmark (Local)${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""

    # Check if image exists, build if needed
    if ! docker image inspect "$IMAGE_NAME" > /dev/null 2>&1; then
        echo -e "${YELLOW}Building Docker image with DPDK...${NC}"
        echo "This will take ~1 minute (using pre-built DPDK packages)"
        echo ""
        cd "$PROJECT_ROOT"
        docker build -f docker/Dockerfile.dpdk -t "$IMAGE_NAME" .
        echo ""
    fi

    echo -e "${YELLOW}Running benchmark...${NC}"
    echo ""
    
    docker run --rm --privileged --name "$CONTAINER_NAME" "$IMAGE_NAME"

    echo ""
    echo -e "${GREEN}Local benchmark complete!${NC}"
    
else
    # ==========================================
    # REMOTE EXECUTION - Upload pre-built image
    # ==========================================
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  DPDK Benchmark (Remote)${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    echo "Target: $REMOTE_HOST"
    echo ""

    # Check Docker on remote
    echo -e "${YELLOW}Step 1: Checking Docker on remote host...${NC}"
    if ! ssh "$REMOTE_HOST" "command -v docker > /dev/null 2>&1"; then
        echo -e "${RED}Error: Docker not found on $REMOTE_HOST${NC}"
        exit 1
    fi

    # Build locally if not exists
    if ! docker image inspect "$IMAGE_NAME" > /dev/null 2>&1; then
        echo -e "${YELLOW}Step 2: Building Docker image locally...${NC}"
        cd "$PROJECT_ROOT"
        docker build -f docker/Dockerfile.dpdk -t "$IMAGE_NAME" .
        echo ""
    fi

    # Save and upload image
    echo -e "${YELLOW}Step 2: Uploading Docker image to remote...${NC}"
    echo "Saving image to tarball..."
    docker save "$IMAGE_NAME" | gzip > /tmp/liblpm_dpdk.tar.gz
    
    echo "Uploading (~100MB, may take a minute)..."
    scp -q /tmp/liblpm_dpdk.tar.gz "$REMOTE_HOST:/tmp/"
    rm /tmp/liblpm_dpdk.tar.gz
    
    echo "Loading image on remote..."
    ssh "$REMOTE_HOST" "docker load < /tmp/liblpm_dpdk.tar.gz && rm /tmp/liblpm_dpdk.tar.gz"

    # Run on remote
    echo ""
    echo -e "${YELLOW}Step 3: Running benchmark on remote...${NC}"
    echo ""
    
    ssh "$REMOTE_HOST" "docker run --rm --privileged --name $CONTAINER_NAME $IMAGE_NAME"

    echo ""
    echo -e "${GREEN}Remote benchmark complete!${NC}"
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo ""

