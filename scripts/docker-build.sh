#!/bin/bash
#
# Docker Build Script for liblpm
# Builds all Docker images with proper tagging and caching
#

set -e

# Configuration
PROJECT_NAME="liblpm"
DOCKER_DIR="docker"
BUILD_ARGS=""
PLATFORM="${PLATFORM:-linux/amd64}"
CACHE_FROM="${CACHE_FROM:-}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Help message
show_help() {
    cat << EOF
Usage: $(basename "$0") [OPTIONS] [IMAGE...]

Build Docker images for liblpm project.

Available Images:
  base        - Base builder and runtime images
  dev         - Development environment
  test        - Testing environment
  fuzz        - AFL++ fuzzing environment
  cpp         - C++ bindings
  go          - Go bindings
  php         - PHP extension
  benchmark   - DPDK benchmark environment
  all         - Build all images (default)

Options:
  -h, --help              Show this help message
  -p, --platform PLATFORM Target platform (default: linux/amd64)
                          Examples: linux/amd64, linux/arm64, linux/amd64,linux/arm64
  -t, --tag TAG           Additional tag for images (can be specified multiple times)
  --no-cache              Build without cache
  --cache-from IMAGE      Use this image as cache source
  -v, --verbose           Verbose output

Examples:
  # Build all images
  $(basename "$0")

  # Build specific image
  $(basename "$0") dev

  # Build with custom tag
  $(basename "$0") --tag v1.2.0 all

  # Build for multiple architectures
  $(basename "$0") --platform linux/amd64,linux/arm64 all

  # Build without cache
  $(basename "$0") --no-cache test

EOF
}

# Parse arguments
IMAGES=()
TAGS=()
NO_CACHE=""
VERBOSE=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -p|--platform)
            PLATFORM="$2"
            shift 2
            ;;
        -t|--tag)
            TAGS+=("$2")
            shift 2
            ;;
        --no-cache)
            NO_CACHE="--no-cache"
            shift
            ;;
        --cache-from)
            CACHE_FROM="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE="--progress=plain"
            shift
            ;;
        base|dev|test|fuzz|cpp|go|php|benchmark|all)
            IMAGES+=("$1")
            shift
            ;;
        *)
            log_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Default to building all if no images specified
if [ ${#IMAGES[@]} -eq 0 ]; then
    IMAGES=("all")
fi

# Get version info
get_version() {
    if [ -f CMakeLists.txt ]; then
        grep "project(liblpm VERSION" CMakeLists.txt | sed -n 's/.*VERSION \([0-9.]*\).*/\1/p'
    else
        echo "unknown"
    fi
}

VERSION=$(get_version)
GIT_SHA=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
BUILD_DATE=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

log_info "liblpm Docker Build"
log_info "Version: ${VERSION}"
log_info "Git SHA: ${GIT_SHA}"
log_info "Platform: ${PLATFORM}"
log_info "Build Date: ${BUILD_DATE}"
echo ""

# Build function
build_image() {
    local name=$1
    local dockerfile=$2
    local tags_arg=""
    
    log_info "Building ${name}..."
    
    # Add default tags
    tags_arg="-t ${PROJECT_NAME}-${name}:latest"
    tags_arg="${tags_arg} -t ${PROJECT_NAME}-${name}:${VERSION}"
    tags_arg="${tags_arg} -t ${PROJECT_NAME}-${name}:${GIT_SHA}"
    
    # Add custom tags
    for tag in "${TAGS[@]}"; do
        tags_arg="${tags_arg} -t ${PROJECT_NAME}-${name}:${tag}"
    done
    
    # Build command
    local cache_arg=""
    if [ -n "$CACHE_FROM" ]; then
        cache_arg="--cache-from ${CACHE_FROM}"
    fi
    
    docker buildx build \
        --platform "${PLATFORM}" \
        ${tags_arg} \
        ${NO_CACHE} \
        ${cache_arg} \
        ${VERBOSE} \
        --build-arg VERSION="${VERSION}" \
        --build-arg GIT_SHA="${GIT_SHA}" \
        --build-arg BUILD_DATE="${BUILD_DATE}" \
        -f "${dockerfile}" \
        . || {
        log_error "Failed to build ${name}"
        return 1
    }
    
    log_success "Successfully built ${name}"
    echo ""
}

# Build images based on selection
build_images() {
    local target=$1
    
    case $target in
        base)
            build_image "base" "${DOCKER_DIR}/Dockerfile.base"
            ;;
        dev)
            build_image "dev" "${DOCKER_DIR}/Dockerfile.dev"
            ;;
        test)
            build_image "test" "${DOCKER_DIR}/Dockerfile.test"
            ;;
        fuzz)
            build_image "fuzz" "${DOCKER_DIR}/Dockerfile.fuzz"
            ;;
        cpp)
            build_image "cpp" "${DOCKER_DIR}/Dockerfile.cpp"
            ;;
        go)
            build_image "go" "${DOCKER_DIR}/Dockerfile.go"
            ;;
        php)
            build_image "php" "${DOCKER_DIR}/Dockerfile.php"
            ;;
        benchmark)
            build_image "benchmark" "${DOCKER_DIR}/Dockerfile.benchmark"
            ;;
        all)
            build_image "base" "${DOCKER_DIR}/Dockerfile.base"
            build_image "dev" "${DOCKER_DIR}/Dockerfile.dev"
            build_image "test" "${DOCKER_DIR}/Dockerfile.test"
            build_image "fuzz" "${DOCKER_DIR}/Dockerfile.fuzz"
            build_image "cpp" "${DOCKER_DIR}/Dockerfile.cpp"
            build_image "go" "${DOCKER_DIR}/Dockerfile.go"
            build_image "php" "${DOCKER_DIR}/Dockerfile.php"
            build_image "benchmark" "${DOCKER_DIR}/Dockerfile.benchmark"
            ;;
        *)
            log_error "Unknown image: $target"
            exit 1
            ;;
    esac
}

# Check if Docker is available
if ! command -v docker &> /dev/null; then
    log_error "Docker is not installed or not in PATH"
    exit 1
fi

# Check if buildx is available
if ! docker buildx version &> /dev/null; then
    log_warning "Docker buildx not found. Multi-platform builds may not work."
fi

# Change to project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}/.."

# Build requested images
for img in "${IMAGES[@]}"; do
    build_images "$img"
done

log_success "All builds completed successfully!"
echo ""
log_info "Built images:"
docker images | grep "^${PROJECT_NAME}-" | head -20

echo ""
log_info "To run a container:"
echo "  docker run -it --rm ${PROJECT_NAME}-dev"
echo "  docker run --rm ${PROJECT_NAME}-test"
echo "  docker run --rm ${PROJECT_NAME}-fuzz"
echo ""
log_info "For more information, see docs/DOCKER.md"
