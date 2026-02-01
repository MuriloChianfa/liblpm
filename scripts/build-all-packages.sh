#!/bin/bash
# Build all packages and bindings for liblpm using Docker
# This ensures consistent production builds regardless of host system

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PACKAGES_DIR="${PROJECT_ROOT}/packages"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color

# Print colored message
print_message() {
    local color=$1
    shift
    echo -e "${color}$*${NC}"
}

# Print section header
print_header() {
    echo ""
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}$*${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo ""
}

# Error handling
error_exit() {
    print_message "$RED" "Error: $*"
    exit 1
}

# Check if Docker is available
check_docker() {
    if ! command -v docker >/dev/null 2>&1; then
        error_exit "Docker is not installed or not in PATH. Please install Docker first."
    fi
    
    if ! docker ps >/dev/null 2>&1; then
        error_exit "Cannot connect to Docker daemon. Is Docker running?"
    fi
}

# Print usage
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Build all packages using Docker containers with production flags.

OPTIONS:
    -h, --help              Show this help message
    -c, --clean             Clean packages directory before building
    --skip-native           Skip native DEB/RPM packages
    --skip-java             Skip Java JAR package
    --skip-python           Skip Python wheel package
    --skip-csharp           Skip C# NuGet package
    --skip-go               Skip Go package
    --skip-php              Skip PHP extension
    --skip-perl             Skip Perl module
    --skip-lua              Skip Lua module
    --only-native           Build only native packages (DEB/RPM)
    --only-java             Build only Java JAR
    --only-python           Build only Python wheel
    --only-csharp           Build only C# NuGet
    --only-go               Build only Go package
    --only-php              Build only PHP extension
    --only-perl             Build only Perl module
    --only-lua              Build only Lua module
    --no-cache              Build Docker images without cache

EXAMPLES:
    # Build all packages
    $0

    # Clean and rebuild everything
    $0 --clean

    # Build only Java JAR
    $0 --only-java

    # Build everything except Python and Go
    $0 --skip-python --skip-go

    # Force rebuild all Docker images
    $0 --clean --no-cache

EOF
}

# Parse command line arguments
CLEAN=false
BUILD_NATIVE=true
BUILD_JAVA=true
BUILD_PYTHON=true
BUILD_CSHARP=true
BUILD_GO=true
BUILD_PHP=true
BUILD_PERL=true
BUILD_LUA=true
NO_CACHE=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        --no-cache)
            NO_CACHE="--no-cache"
            shift
            ;;
        --skip-native)
            BUILD_NATIVE=false
            shift
            ;;
        --skip-java)
            BUILD_JAVA=false
            shift
            ;;
        --skip-python)
            BUILD_PYTHON=false
            shift
            ;;
        --skip-csharp)
            BUILD_CSHARP=false
            shift
            ;;
        --skip-go)
            BUILD_GO=false
            shift
            ;;
        --skip-php)
            BUILD_PHP=false
            shift
            ;;
        --skip-perl)
            BUILD_PERL=false
            shift
            ;;
        --skip-lua)
            BUILD_LUA=false
            shift
            ;;
        --only-native)
            BUILD_NATIVE=true
            BUILD_JAVA=false
            BUILD_PYTHON=false
            BUILD_CSHARP=false
            BUILD_GO=false
            BUILD_PHP=false
            BUILD_PERL=false
            BUILD_LUA=false
            shift
            ;;
        --only-java)
            BUILD_NATIVE=false
            BUILD_JAVA=true
            BUILD_PYTHON=false
            BUILD_CSHARP=false
            BUILD_GO=false
            BUILD_PHP=false
            BUILD_PERL=false
            BUILD_LUA=false
            shift
            ;;
        --only-python)
            BUILD_NATIVE=false
            BUILD_JAVA=false
            BUILD_PYTHON=true
            BUILD_CSHARP=false
            BUILD_GO=false
            BUILD_PHP=false
            BUILD_PERL=false
            BUILD_LUA=false
            shift
            ;;
        --only-csharp)
            BUILD_NATIVE=false
            BUILD_JAVA=false
            BUILD_PYTHON=false
            BUILD_CSHARP=true
            BUILD_GO=false
            BUILD_PHP=false
            BUILD_PERL=false
            BUILD_LUA=false
            shift
            ;;
        --only-go)
            BUILD_NATIVE=false
            BUILD_JAVA=false
            BUILD_PYTHON=false
            BUILD_CSHARP=false
            BUILD_GO=true
            BUILD_PHP=false
            BUILD_PERL=false
            BUILD_LUA=false
            shift
            ;;
        --only-php)
            BUILD_NATIVE=false
            BUILD_JAVA=false
            BUILD_PYTHON=false
            BUILD_CSHARP=false
            BUILD_GO=false
            BUILD_PHP=true
            BUILD_PERL=false
            BUILD_LUA=false
            shift
            ;;
        --only-perl)
            BUILD_NATIVE=false
            BUILD_JAVA=false
            BUILD_PYTHON=false
            BUILD_CSHARP=false
            BUILD_GO=false
            BUILD_PHP=false
            BUILD_PERL=true
            BUILD_LUA=false
            shift
            ;;
        --only-lua)
            BUILD_NATIVE=false
            BUILD_JAVA=false
            BUILD_PYTHON=false
            BUILD_CSHARP=false
            BUILD_GO=false
            BUILD_PHP=false
            BUILD_PERL=false
            BUILD_LUA=true
            shift
            ;;
        *)
            print_message "$RED" "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

print_header "liblpm Docker Package Builder"

# Check Docker availability
check_docker

print_message "$BLUE" "Project root: $PROJECT_ROOT"
print_message "$BLUE" "Packages dir: $PACKAGES_DIR"
echo ""
print_message "$BLUE" "Build Configuration:"
print_message "$BLUE" "  Native (DEB/RPM): $([ "$BUILD_NATIVE" = true ] && echo "YES" || echo "NO")"
print_message "$BLUE" "  Java (JAR):       $([ "$BUILD_JAVA" = true ] && echo "YES" || echo "NO")"
print_message "$BLUE" "  Python (wheel):   $([ "$BUILD_PYTHON" = true ] && echo "YES" || echo "NO")"
print_message "$BLUE" "  C# (NuGet):       $([ "$BUILD_CSHARP" = true ] && echo "YES" || echo "NO")"
print_message "$BLUE" "  Go (source):      $([ "$BUILD_GO" = true ] && echo "YES" || echo "NO")"
print_message "$BLUE" "  PHP (extension):  $([ "$BUILD_PHP" = true ] && echo "YES" || echo "NO")"
print_message "$BLUE" "  Perl (module):    $([ "$BUILD_PERL" = true ] && echo "YES" || echo "NO")"
print_message "$BLUE" "  Lua (module):     $([ "$BUILD_LUA" = true ] && echo "YES" || echo "NO")"

# Clean if requested
if [[ "$CLEAN" == "true" ]]; then
    print_message "$YELLOW" "Cleaning packages directory..."
    rm -rf "${PACKAGES_DIR}"
fi

# Create packages directory structure
mkdir -p "${PACKAGES_DIR}"/{native,java,python,csharp,go,php,perl,lua}

# Build counter for summary
BUILT_PACKAGES=()
FAILED_PACKAGES=()

# ============================================================================
# Build Native Packages (DEB and RPM)
# ============================================================================

if [[ "$BUILD_NATIVE" == "true" ]]; then
    print_header "Building Native Packages (DEB and RPM)"
    
    # Build DEB packages
    print_message "$YELLOW" "Building DEB packages (Ubuntu 24.04)..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.deb" \
        --build-arg DEBIAN_VERSION="24.04" \
        -t liblpm-deb:ubuntu-24.04 \
        "${PROJECT_ROOT}" \
        --build-arg DEBIAN_BASE="ubuntu"; then
        
        if docker run --rm \
            -v "${PROJECT_ROOT}:/workspace" \
            -v "${PACKAGES_DIR}/native:/packages" \
            liblpm-deb:ubuntu-24.04; then
            BUILT_PACKAGES+=("DEB packages")
            print_message "$GREEN" "✓ DEB packages built successfully"
        else
            print_message "$RED" "✗ Failed to generate DEB packages"
            FAILED_PACKAGES+=("DEB packages")
        fi
    else
        print_message "$RED" "✗ Failed to build DEB Docker image"
        FAILED_PACKAGES+=("DEB Docker image")
    fi
    
    # Build RPM packages
    print_message "$YELLOW" "Building RPM packages (Rocky Linux 9)..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.rpm" \
        --build-arg ROCKYLINUX_VERSION="9" \
        -t liblpm-rpm:rocky-9 \
        "${PROJECT_ROOT}"; then
        
        if docker run --rm \
            -v "${PROJECT_ROOT}:/workspace" \
            -v "${PACKAGES_DIR}/native:/packages" \
            liblpm-rpm:rocky-9; then
            BUILT_PACKAGES+=("RPM packages")
            print_message "$GREEN" "✓ RPM packages built successfully"
        else
            print_message "$RED" "✗ Failed to generate RPM packages"
            FAILED_PACKAGES+=("RPM packages")
        fi
    else
        print_message "$RED" "✗ Failed to build RPM Docker image"
        FAILED_PACKAGES+=("RPM Docker image")
    fi
fi

# ============================================================================
# Build Java JAR Package
# ============================================================================

if [[ "$BUILD_JAVA" == "true" ]]; then
    print_header "Building Java JAR Package"
    
    print_message "$YELLOW" "Building Java bindings with Docker..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.java" \
        -t liblpm-java \
        "${PROJECT_ROOT}"; then
        
        # Extract JAR from container
        if docker run --rm -v "${PACKAGES_DIR}/java:/artifacts" \
            --entrypoint sh \
            liblpm-java \
            -c 'find /app/build/libs -name "*.jar" ! -name "*-sources.jar" ! -name "*-javadoc.jar" -exec cp {} /artifacts/ \; 2>/dev/null || find /java/build/libs -name "*.jar" ! -name "*-sources.jar" ! -name "*-javadoc.jar" -exec cp {} /artifacts/ \;'; then
            BUILT_PACKAGES+=("Java JAR")
            print_message "$GREEN" "✓ Java JAR built successfully"
        else
            print_message "$RED" "✗ Failed to extract Java JAR"
            FAILED_PACKAGES+=("Java JAR extraction")
        fi
    else
        print_message "$RED" "✗ Failed to build Java Docker image"
        FAILED_PACKAGES+=("Java Docker image")
    fi
fi

# ============================================================================
# Build Python Wheel Package
# ============================================================================

if [[ "$BUILD_PYTHON" == "true" ]]; then
    print_header "Building Python Wheel Package"
    
    print_message "$YELLOW" "Building Python wheel with Docker..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.python" \
        -t liblpm-python \
        "${PROJECT_ROOT}"; then
        
        # Extract wheel from container
        if docker run --rm -v "${PACKAGES_DIR}/python:/artifacts" \
            --entrypoint bash \
            liblpm-python \
            -c '. /opt/venv/bin/activate && cd /build/bindings/python && pip install -q build && python3 -m build --wheel --outdir /artifacts 2>&1'; then
            BUILT_PACKAGES+=("Python wheel")
            print_message "$GREEN" "✓ Python wheel built successfully"
        else
            print_message "$RED" "✗ Failed to build Python wheel"
            FAILED_PACKAGES+=("Python wheel")
        fi
    else
        print_message "$RED" "✗ Failed to build Python Docker image"
        FAILED_PACKAGES+=("Python Docker image")
    fi
fi

# ============================================================================
# Build C# NuGet Package
# ============================================================================

if [[ "$BUILD_CSHARP" == "true" ]]; then
    print_header "Building C# NuGet Package"
    
    print_message "$YELLOW" "Building C# NuGet package with Docker..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.csharp" \
        -t liblpm-csharp \
        "${PROJECT_ROOT}"; then
        
        # Extract NuGet package from container
        if docker run --rm -v "${PACKAGES_DIR}/csharp:/artifacts" \
            --entrypoint bash \
            liblpm-csharp \
            -c 'cd /build/bindings/csharp/LibLpm && mkdir -p build && echo "<Project />" > build/LibLpm.targets && cd .. && dotnet pack LibLpm/LibLpm.csproj --configuration Release -o /artifacts 2>&1'; then
            BUILT_PACKAGES+=("C# NuGet")
            print_message "$GREEN" "✓ C# NuGet package built successfully"
        else
            print_message "$RED" "✗ Failed to build C# NuGet package"
            FAILED_PACKAGES+=("C# NuGet")
        fi
    else
        print_message "$RED" "✗ Failed to build C# Docker image"
        FAILED_PACKAGES+=("C# Docker image")
    fi
fi

# ============================================================================
# Build Go Package
# ============================================================================

if [[ "$BUILD_GO" == "true" ]]; then
    print_header "Building Go Package"
    
    print_message "$YELLOW" "Building Go source package with Docker..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.go" \
        -t liblpm-go \
        "${PROJECT_ROOT}"; then
        
        # Extract Go source tarball from container
        if docker run --rm -v "${PACKAGES_DIR}/go:/artifacts" \
            --entrypoint bash \
            liblpm-go \
            -c 'cd /app && tar -czf /artifacts/liblpm-go-$(date +%Y%m%d).tar.gz --exclude=".git" --exclude="*.test" . 2>&1'; then
            BUILT_PACKAGES+=("Go package")
            print_message "$GREEN" "✓ Go package built successfully"
        else
            print_message "$RED" "✗ Failed to create Go package"
            FAILED_PACKAGES+=("Go package")
        fi
    else
        print_message "$RED" "✗ Failed to build Go Docker image"
        FAILED_PACKAGES+=("Go Docker image")
    fi
fi

# ============================================================================
# Build PHP Extension (Original .so)
# ============================================================================

if [[ "$BUILD_PHP" == "true" ]]; then
    print_header "Building PHP Extension"
    
    print_message "$YELLOW" "Building PHP extension with Docker..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.php" \
        -t liblpm-php \
        "${PROJECT_ROOT}"; then
        
        # Extract PHP extension from container
        if docker run --rm -v "${PACKAGES_DIR}/php:/artifacts" \
            --entrypoint sh \
            liblpm-php \
            -c 'find /ext/modules -name "*.so" -exec cp {} /artifacts/ \;'; then
            BUILT_PACKAGES+=("PHP extension (.so)")
            print_message "$GREEN" "✓ PHP extension built successfully"
        else
            print_message "$RED" "✗ Failed to extract PHP extension"
            FAILED_PACKAGES+=("PHP extension")
        fi
    else
        print_message "$RED" "✗ Failed to build PHP Docker image"
        FAILED_PACKAGES+=("PHP Docker image")
    fi
    
    # ========================================================================
    # Build PHP PECL Package
    # ========================================================================
    
    print_header "Building PHP PECL Package"
    
    print_message "$YELLOW" "Building PECL tarball with Docker..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.php.pecl" \
        -t liblpm-php-pecl \
        "${PROJECT_ROOT}"; then
        
        if docker run --rm -v "${PACKAGES_DIR}/php:/artifacts" \
            liblpm-php-pecl; then
            BUILT_PACKAGES+=("PHP PECL package")
            print_message "$GREEN" "✓ PHP PECL package built successfully"
        else
            print_message "$RED" "✗ Failed to create PECL package"
            FAILED_PACKAGES+=("PHP PECL package")
        fi
    else
        print_message "$RED" "✗ Failed to build PHP PECL Docker image"
        FAILED_PACKAGES+=("PHP PECL Docker image")
    fi
    
    # ========================================================================
    # Build PHP DEB Packages (Multiple PHP versions/distros)
    # ========================================================================
    
    print_header "Building PHP DEB Packages"
    
    # Ubuntu 22.04 (PHP 8.1)
    print_message "$YELLOW" "Building PHP 8.1 DEB (Ubuntu 22.04)..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.php.deb" \
        --build-arg PHP_VERSION=8.1 \
        --build-arg TARGET_DISTRO=ubuntu \
        --build-arg TARGET_VERSION=22.04 \
        -t liblpm-php-deb:u22.04 \
        "${PROJECT_ROOT}"; then
        
        if docker run --rm \
            -v "${PROJECT_ROOT}:/workspace" \
            -v "${PACKAGES_DIR}/php:/packages" \
            liblpm-php-deb:u22.04; then
            BUILT_PACKAGES+=("PHP 8.1 DEB (Ubuntu 22.04)")
            print_message "$GREEN" "✓ PHP 8.1 DEB built successfully"
        else
            print_message "$RED" "✗ Failed to generate PHP 8.1 DEB"
            FAILED_PACKAGES+=("PHP 8.1 DEB")
        fi
    else
        print_message "$RED" "✗ Failed to build PHP 8.1 DEB Docker image"
        FAILED_PACKAGES+=("PHP 8.1 DEB Docker image")
    fi
    
    # Ubuntu 24.04 (PHP 8.3)
    print_message "$YELLOW" "Building PHP 8.3 DEB (Ubuntu 24.04)..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.php.deb" \
        --build-arg PHP_VERSION=8.3 \
        --build-arg TARGET_DISTRO=ubuntu \
        --build-arg TARGET_VERSION=24.04 \
        -t liblpm-php-deb:u24.04 \
        "${PROJECT_ROOT}"; then
        
        if docker run --rm \
            -v "${PROJECT_ROOT}:/workspace" \
            -v "${PACKAGES_DIR}/php:/packages" \
            liblpm-php-deb:u24.04; then
            BUILT_PACKAGES+=("PHP 8.3 DEB (Ubuntu 24.04)")
            print_message "$GREEN" "✓ PHP 8.3 DEB built successfully"
        else
            print_message "$RED" "✗ Failed to generate PHP 8.3 DEB"
            FAILED_PACKAGES+=("PHP 8.3 DEB")
        fi
    else
        print_message "$RED" "✗ Failed to build PHP 8.3 DEB Docker image"
        FAILED_PACKAGES+=("PHP 8.3 DEB Docker image")
    fi
    
    # Debian 12 (PHP 8.2)
    print_message "$YELLOW" "Building PHP 8.2 DEB (Debian 12)..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.php.deb" \
        --build-arg PHP_VERSION=8.2 \
        --build-arg TARGET_DISTRO=debian \
        --build-arg TARGET_VERSION=12 \
        -t liblpm-php-deb:d12 \
        "${PROJECT_ROOT}"; then
        
        if docker run --rm \
            -v "${PROJECT_ROOT}:/workspace" \
            -v "${PACKAGES_DIR}/php:/packages" \
            liblpm-php-deb:d12; then
            BUILT_PACKAGES+=("PHP 8.2 DEB (Debian 12)")
            print_message "$GREEN" "✓ PHP 8.2 DEB built successfully"
        else
            print_message "$RED" "✗ Failed to generate PHP 8.2 DEB"
            FAILED_PACKAGES+=("PHP 8.2 DEB")
        fi
    else
        print_message "$RED" "✗ Failed to build PHP 8.2 DEB Docker image"
        FAILED_PACKAGES+=("PHP 8.2 DEB Docker image")
    fi
    
    # ========================================================================
    # Build PHP RPM Packages (Multiple PHP versions/distros)
    # ========================================================================
    
    print_header "Building PHP RPM Packages"
    
    # Rocky Linux 9 (PHP 8.1, 8.2, 8.3 via Remi)
    print_message "$YELLOW" "Building PHP RPMs (Rocky Linux 9)..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.php.rpm" \
        --build-arg DISTRO=rockylinux \
        --build-arg VERSION=9 \
        -t liblpm-php-rpm:rocky9 \
        "${PROJECT_ROOT}"; then
        
        if docker run --rm \
            -v "${PROJECT_ROOT}:/workspace" \
            -v "${PACKAGES_DIR}/php:/packages" \
            liblpm-php-rpm:rocky9; then
            BUILT_PACKAGES+=("PHP RPMs (Rocky Linux 9)")
            print_message "$GREEN" "✓ PHP RPMs built successfully"
        else
            print_message "$RED" "✗ Failed to generate PHP RPMs"
            FAILED_PACKAGES+=("PHP RPMs")
        fi
    else
        print_message "$RED" "✗ Failed to build PHP RPM Docker image"
        FAILED_PACKAGES+=("PHP RPM Docker image")
    fi
    
    # Fedora 41 (PHP 8.3)
    print_message "$YELLOW" "Building PHP RPM (Fedora 41)..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.php.rpm" \
        --build-arg DISTRO=fedora \
        --build-arg VERSION=41 \
        -t liblpm-php-rpm:f41 \
        "${PROJECT_ROOT}"; then
        
        if docker run --rm \
            -v "${PROJECT_ROOT}:/workspace" \
            -v "${PACKAGES_DIR}/php:/packages" \
            liblpm-php-rpm:f41; then
            BUILT_PACKAGES+=("PHP RPM (Fedora 41)")
            print_message "$GREEN" "✓ PHP RPM built successfully"
        else
            print_message "$RED" "✗ Failed to generate PHP RPM"
            FAILED_PACKAGES+=("PHP Fedora RPM")
        fi
    else
        print_message "$RED" "✗ Failed to build PHP Fedora RPM Docker image"
        FAILED_PACKAGES+=("PHP Fedora RPM Docker image")
    fi
fi

# ============================================================================
# Build Perl Module
# ============================================================================

if [[ "$BUILD_PERL" == "true" ]]; then
    print_header "Building Perl Module"
    
    # ========================================================================
    # Build Perl DEB Packages (Multiple distributions)
    # ========================================================================
    
    print_header "Building Perl DEB Packages"
    
    # Ubuntu 22.04
    print_message "$YELLOW" "Building Perl DEB (Ubuntu 22.04)..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.perl.deb" \
        --build-arg TARGET_DISTRO=ubuntu \
        --build-arg TARGET_VERSION=22.04 \
        -t liblpm-perl-deb:u22.04 \
        "${PROJECT_ROOT}"; then
        
        if docker run --rm \
            -v "${PROJECT_ROOT}:/workspace" \
            -v "${PACKAGES_DIR}/perl:/packages" \
            liblpm-perl-deb:u22.04; then
            BUILT_PACKAGES+=("Perl DEB (Ubuntu 22.04)")
            print_message "$GREEN" "✓ Perl DEB (Ubuntu 22.04) built successfully"
        else
            print_message "$RED" "✗ Failed to generate Perl DEB (Ubuntu 22.04)"
            FAILED_PACKAGES+=("Perl DEB (Ubuntu 22.04)")
        fi
    else
        print_message "$RED" "✗ Failed to build Perl DEB Docker image (Ubuntu 22.04)"
        FAILED_PACKAGES+=("Perl DEB Docker image (Ubuntu 22.04)")
    fi
    
    # Ubuntu 24.04
    print_message "$YELLOW" "Building Perl DEB (Ubuntu 24.04)..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.perl.deb" \
        --build-arg TARGET_DISTRO=ubuntu \
        --build-arg TARGET_VERSION=24.04 \
        -t liblpm-perl-deb:u24.04 \
        "${PROJECT_ROOT}"; then
        
        if docker run --rm \
            -v "${PROJECT_ROOT}:/workspace" \
            -v "${PACKAGES_DIR}/perl:/packages" \
            liblpm-perl-deb:u24.04; then
            BUILT_PACKAGES+=("Perl DEB (Ubuntu 24.04)")
            print_message "$GREEN" "✓ Perl DEB (Ubuntu 24.04) built successfully"
        else
            print_message "$RED" "✗ Failed to generate Perl DEB (Ubuntu 24.04)"
            FAILED_PACKAGES+=("Perl DEB (Ubuntu 24.04)")
        fi
    else
        print_message "$RED" "✗ Failed to build Perl DEB Docker image (Ubuntu 24.04)"
        FAILED_PACKAGES+=("Perl DEB Docker image (Ubuntu 24.04)")
    fi
    
    # Debian 12
    print_message "$YELLOW" "Building Perl DEB (Debian 12)..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.perl.deb" \
        --build-arg TARGET_DISTRO=debian \
        --build-arg TARGET_VERSION=12 \
        -t liblpm-perl-deb:d12 \
        "${PROJECT_ROOT}"; then
        
        if docker run --rm \
            -v "${PROJECT_ROOT}:/workspace" \
            -v "${PACKAGES_DIR}/perl:/packages" \
            liblpm-perl-deb:d12; then
            BUILT_PACKAGES+=("Perl DEB (Debian 12)")
            print_message "$GREEN" "✓ Perl DEB (Debian 12) built successfully"
        else
            print_message "$RED" "✗ Failed to generate Perl DEB (Debian 12)"
            FAILED_PACKAGES+=("Perl DEB (Debian 12)")
        fi
    else
        print_message "$RED" "✗ Failed to build Perl DEB Docker image (Debian 12)"
        FAILED_PACKAGES+=("Perl DEB Docker image (Debian 12)")
    fi
    
    # ========================================================================
    # Build Perl RPM Packages (Multiple distributions)
    # ========================================================================
    
    print_header "Building Perl RPM Packages"
    
    # Rocky Linux 9
    print_message "$YELLOW" "Building Perl RPM (Rocky Linux 9)..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.perl.rpm" \
        --build-arg DISTRO=rockylinux \
        --build-arg VERSION=9 \
        -t liblpm-perl-rpm:rocky9 \
        "${PROJECT_ROOT}"; then
        
        if docker run --rm \
            -v "${PROJECT_ROOT}:/workspace" \
            -v "${PACKAGES_DIR}/perl:/packages" \
            liblpm-perl-rpm:rocky9; then
            BUILT_PACKAGES+=("Perl RPM (Rocky Linux 9)")
            print_message "$GREEN" "✓ Perl RPM (Rocky Linux 9) built successfully"
        else
            print_message "$RED" "✗ Failed to generate Perl RPM (Rocky Linux 9)"
            FAILED_PACKAGES+=("Perl RPM (Rocky Linux 9)")
        fi
    else
        print_message "$RED" "✗ Failed to build Perl RPM Docker image (Rocky Linux 9)"
        FAILED_PACKAGES+=("Perl RPM Docker image (Rocky Linux 9)")
    fi
    
    # Fedora 41
    print_message "$YELLOW" "Building Perl RPM (Fedora 41)..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.perl.rpm" \
        --build-arg DISTRO=fedora \
        --build-arg VERSION=41 \
        -t liblpm-perl-rpm:f41 \
        "${PROJECT_ROOT}"; then
        
        if docker run --rm \
            -v "${PROJECT_ROOT}:/workspace" \
            -v "${PACKAGES_DIR}/perl:/packages" \
            liblpm-perl-rpm:f41; then
            BUILT_PACKAGES+=("Perl RPM (Fedora 41)")
            print_message "$GREEN" "✓ Perl RPM (Fedora 41) built successfully"
        else
            print_message "$RED" "✗ Failed to generate Perl RPM (Fedora 41)"
            FAILED_PACKAGES+=("Perl RPM (Fedora 41)")
        fi
    else
        print_message "$RED" "✗ Failed to build Perl Fedora RPM Docker image"
        FAILED_PACKAGES+=("Perl Fedora RPM Docker image")
    fi
fi

# ============================================================================
# Build Python Native Distribution Packages (DEB and RPM)
# ============================================================================
# Note: This is separate from PyPI wheels built above
# These are native distro packages for apt/dnf installation

if [[ "$BUILD_PYTHON" == "true" ]]; then
    # ========================================================================
    # Build Python DEB Packages (Multiple distributions)
    # ========================================================================
    
    print_header "Building Python DEB Packages"
    
    # Ubuntu 22.04
    print_message "$YELLOW" "Building Python DEB (Ubuntu 22.04)..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.python.deb" \
        --build-arg TARGET_DISTRO=ubuntu \
        --build-arg TARGET_VERSION=22.04 \
        -t liblpm-python-deb:u22.04 \
        "${PROJECT_ROOT}"; then
        
        if docker run --rm \
            -v "${PROJECT_ROOT}:/workspace" \
            -v "${PACKAGES_DIR}/python:/packages" \
            liblpm-python-deb:u22.04; then
            BUILT_PACKAGES+=("Python DEB (Ubuntu 22.04)")
            print_message "$GREEN" "✓ Python DEB (Ubuntu 22.04) built successfully"
        else
            print_message "$RED" "✗ Failed to generate Python DEB (Ubuntu 22.04)"
            FAILED_PACKAGES+=("Python DEB (Ubuntu 22.04)")
        fi
    else
        print_message "$RED" "✗ Failed to build Python DEB Docker image (Ubuntu 22.04)"
        FAILED_PACKAGES+=("Python DEB Docker image (Ubuntu 22.04)")
    fi
    
    # Ubuntu 24.04
    print_message "$YELLOW" "Building Python DEB (Ubuntu 24.04)..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.python.deb" \
        --build-arg TARGET_DISTRO=ubuntu \
        --build-arg TARGET_VERSION=24.04 \
        -t liblpm-python-deb:u24.04 \
        "${PROJECT_ROOT}"; then
        
        if docker run --rm \
            -v "${PROJECT_ROOT}:/workspace" \
            -v "${PACKAGES_DIR}/python:/packages" \
            liblpm-python-deb:u24.04; then
            BUILT_PACKAGES+=("Python DEB (Ubuntu 24.04)")
            print_message "$GREEN" "✓ Python DEB (Ubuntu 24.04) built successfully"
        else
            print_message "$RED" "✗ Failed to generate Python DEB (Ubuntu 24.04)"
            FAILED_PACKAGES+=("Python DEB (Ubuntu 24.04)")
        fi
    else
        print_message "$RED" "✗ Failed to build Python DEB Docker image (Ubuntu 24.04)"
        FAILED_PACKAGES+=("Python DEB Docker image (Ubuntu 24.04)")
    fi
    
    # Debian 12
    print_message "$YELLOW" "Building Python DEB (Debian 12)..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.python.deb" \
        --build-arg TARGET_DISTRO=debian \
        --build-arg TARGET_VERSION=12 \
        -t liblpm-python-deb:d12 \
        "${PROJECT_ROOT}"; then
        
        if docker run --rm \
            -v "${PROJECT_ROOT}:/workspace" \
            -v "${PACKAGES_DIR}/python:/packages" \
            liblpm-python-deb:d12; then
            BUILT_PACKAGES+=("Python DEB (Debian 12)")
            print_message "$GREEN" "✓ Python DEB (Debian 12) built successfully"
        else
            print_message "$RED" "✗ Failed to generate Python DEB (Debian 12)"
            FAILED_PACKAGES+=("Python DEB (Debian 12)")
        fi
    else
        print_message "$RED" "✗ Failed to build Python DEB Docker image (Debian 12)"
        FAILED_PACKAGES+=("Python DEB Docker image (Debian 12)")
    fi
    
    # ========================================================================
    # Build Python RPM Packages (Multiple distributions)
    # ========================================================================
    
    print_header "Building Python RPM Packages"
    
    # Note: Rocky Linux 9 has Python 3.9, but liblpm Python bindings require 3.10+
    # Skipping Rocky Linux 9 for Python packages
    print_message "$YELLOW" "Skipping Python RPM (Rocky Linux 9) - requires Python 3.10+, Rocky 9 has 3.9"
    
    # Fedora 41
    print_message "$YELLOW" "Building Python RPM (Fedora 41)..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.python.rpm" \
        --build-arg DISTRO=fedora \
        --build-arg VERSION=41 \
        -t liblpm-python-rpm:f41 \
        "${PROJECT_ROOT}"; then
        
        if docker run --rm \
            -v "${PROJECT_ROOT}:/workspace" \
            -v "${PACKAGES_DIR}/python:/packages" \
            liblpm-python-rpm:f41; then
            BUILT_PACKAGES+=("Python RPM (Fedora 41)")
            print_message "$GREEN" "✓ Python RPM (Fedora 41) built successfully"
        else
            print_message "$RED" "✗ Failed to generate Python RPM (Fedora 41)"
            FAILED_PACKAGES+=("Python RPM (Fedora 41)")
        fi
    else
        print_message "$RED" "✗ Failed to build Python Fedora RPM Docker image"
        FAILED_PACKAGES+=("Python Fedora RPM Docker image")
    fi
fi

# ============================================================================
# Build Lua Module
# ============================================================================

if [[ "$BUILD_LUA" == "true" ]]; then
    print_header "Building Lua Module"
    
    print_message "$YELLOW" "Building Lua module with Docker..."
    if docker build $NO_CACHE -f "${PROJECT_ROOT}/docker/Dockerfile.lua" \
        -t liblpm-lua \
        "${PROJECT_ROOT}"; then
        
        # Extract Lua module from container
        if docker run --rm -v "${PACKAGES_DIR}/lua:/artifacts" \
            --entrypoint sh \
            liblpm-lua \
            -c 'find /usr/local/lib/lua -name "liblpm.so" -exec cp {} /artifacts/ \;'; then
            BUILT_PACKAGES+=("Lua module")
            print_message "$GREEN" "✓ Lua module built successfully"
        else
            print_message "$RED" "✗ Failed to extract Lua module"
            FAILED_PACKAGES+=("Lua module")
        fi
    else
        print_message "$RED" "✗ Failed to build Lua Docker image"
        FAILED_PACKAGES+=("Lua Docker image")
    fi
fi

# ============================================================================
# Summary
# ============================================================================

print_header "Build Summary"

if [[ ${#BUILT_PACKAGES[@]} -gt 0 ]]; then
    print_message "$GREEN" "Successfully built packages:"
    for pkg in "${BUILT_PACKAGES[@]}"; do
        print_message "$GREEN" "  ✓ $pkg"
    done
fi

if [[ ${#FAILED_PACKAGES[@]} -gt 0 ]]; then
    echo ""
    print_message "$RED" "Failed to build:"
    for pkg in "${FAILED_PACKAGES[@]}"; do
        print_message "$RED" "  ✗ $pkg"
    done
fi

echo ""
print_message "$CYAN" "Package Summary:"
echo ""

# Count packages in each directory
for dir in native java python csharp go php perl lua; do
    count=$(find "${PACKAGES_DIR}/${dir}" -type f 2>/dev/null | wc -l)
    if [[ $count -gt 0 ]]; then
        size=$(du -sh "${PACKAGES_DIR}/${dir}" 2>/dev/null | cut -f1)
        print_message "$BLUE" "  ${dir}: ${count} file(s) [${size}]"
        find "${PACKAGES_DIR}/${dir}" -type f -exec ls -lh {} \; | awk '{print "    - " $9 " (" $5 ")"}'
    fi
done

echo ""
print_message "$BLUE" "All packages are available in: ${PACKAGES_DIR}"

echo ""
if [[ ${#FAILED_PACKAGES[@]} -eq 0 ]]; then
    print_message "$GREEN" "✓ All requested packages built successfully!"
    exit 0
else
    print_message "$YELLOW" "⚠ Some packages failed to build. See details above."
    exit 1
fi
