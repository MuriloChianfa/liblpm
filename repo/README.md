# Docker-based Package Building for liblpm

This directory contains Docker configurations for building DEB and RPM packages for liblpm across multiple Linux distributions.

## Quick Start

### Build All Packages

```bash
# From project root
cd repo
./scripts/docker-build-all-packages.sh
```

This will build packages for:
- Ubuntu 24.04 (DEB)
- Fedora 41 (RPM)

Packages will be output to `packages/` in the project root.

### Build Specific Distributions

Using docker-compose directly:

```bash
cd repo

# Build Ubuntu 24.04 DEB packages
docker compose -f docker-compose.packages.yml run --rm ubuntu-noble

# Build Fedora 41 RPM packages
docker compose -f docker-compose.packages.yml run --rm fedora-41

# Build for other distributions (see available services below)
docker compose -f docker-compose.packages.yml run --rm debian-bookworm
docker compose -f docker-compose.packages.yml run --rm rocky-9
```

## Available Build Environments

### DEB Packages (Debian/Ubuntu)

| Service Name | Distribution | Codename | GCC Version |
|--------------|--------------|----------|-------------|
| `ubuntu-noble` | Ubuntu 24.04 LTS | Noble | 13.x |
| `ubuntu-jammy` | Ubuntu 22.04 LTS | Jammy | 11.x |
| `debian-bookworm` | Debian 12 | Bookworm | 12.x |

### RPM Packages (RHEL/Fedora)

| Service Name | Distribution | Version | GCC Version | Notes |
|--------------|--------------|---------|-------------|-------|
| `fedora-41` | Fedora | 41 | 14.x | ✅ Recommended |
| `fedora-40` | Fedora | 40 | 14.x | ✅ Works |
| `rocky-9` | Rocky Linux | 9 | 11.x → 13.x | Uses GCC toolset 13 |
| `rocky-8` | Rocky Linux | 8 | 8.x → 13.x | Uses GCC toolset 13 |

**Note:** Rocky Linux and RHEL 8/9 require GCC 13+ for C23 support, so the build images automatically install and use `gcc-toolset-13`.

## Package Output

After building, packages are available in `packages/`:

```
packages/
├── liblpm_2.0.0_amd64.deb              # Ubuntu/Debian runtime library
├── liblpm-dev_2.0.0_amd64.deb          # Ubuntu/Debian development files
├── liblpm-2.0.0.x86_64.rpm             # Fedora/RHEL runtime library
└── liblpm-devel-2.0.0.x86_64.rpm       # Fedora/RHEL development files
```

### Package Contents

**Runtime Package (`liblpm` / `liblpm`):**
- Shared library: `liblpm.so.2.0.0`
- Symlinks: `liblpm.so.1`, `liblpm.so`

**Development Package (`liblpm-dev` / `liblpm-devel`):**
- Header files in `/usr/include/lpm/`
- Static library: `liblpm.a`
- pkg-config file: `liblpm.pc`
- CMake config files for `find_package(liblpm)`

## Building Individual Images

### Build DEB Builder Image

```bash
cd repo
docker compose -f docker-compose.packages.yml build ubuntu-noble
```

### Build RPM Builder Image

```bash
cd repo
docker compose -f docker-compose.packages.yml build fedora-41
```

## Manual Docker Commands

If you prefer not to use docker-compose:

### DEB Packages

```bash
# Build image
docker build -f docker/Dockerfile.deb \
    --build-arg DISTRO=ubuntu \
    --build-arg VERSION=24.04 \
    -t liblpm-builder-deb:ubuntu-24.04 .

# Run build
docker run --rm \
    -v $PWD:/workspace \
    -v $PWD/packages:/packages \
    liblpm-builder-deb:ubuntu-24.04
```

### RPM Packages

```bash
# Build image
docker build -f docker/Dockerfile.rpm \
    --build-arg DISTRO=fedora \
    --build-arg VERSION=41 \
    -t liblpm-builder-rpm:fedora-41 .

# Run build
docker run --rm \
    -v $PWD:/workspace \
    -v $PWD/packages:/packages \
    liblpm-builder-rpm:fedora-41
```

## Testing Packages

### Test DEB Package Installation

```bash
# On Ubuntu/Debian system or container
sudo apt install ./packages/liblpm_2.0.0_amd64.deb
sudo apt install ./packages/liblpm-dev_2.0.0_amd64.deb

# Verify installation
pkg-config --modversion liblpm
ldconfig -p | grep liblpm
```

### Test RPM Package Installation

```bash
# On Fedora/RHEL system or container
sudo dnf install ./packages/liblpm-2.0.0.x86_64.rpm
sudo dnf install ./packages/liblpm-devel-2.0.0.x86_64.rpm

# Verify installation
pkg-config --modversion liblpm
ldconfig -p | grep liblpm
```

## Troubleshooting

### Clean Build

If you encounter issues, try a clean build:

```bash
# Clean build directory
rm -rf build-pkg-docker

# Rebuild Docker images without cache
cd repo
docker compose -f docker-compose.packages.yml build --no-cache ubuntu-noble
docker compose -f docker-compose.packages.yml build --no-cache fedora-41
```

### GCC Version Issues (Rocky Linux 9)

Rocky Linux 9 ships with GCC 11 which doesn't support C23. The Docker image automatically installs `gcc-toolset-13` to provide GCC 13.x support. If you encounter compiler version errors, verify the toolset is enabled:

```bash
# Inside container
source /opt/rh/gcc-toolset-13/enable
gcc --version  # Should show 13.x
```

### Missing Libraries (-lm, -lc errors)

If you encounter linking errors for `-lm` or `-lc`, ensure `glibc-devel` is installed in the container. The Dockerfile should handle this automatically.

## CI/CD Integration

These Docker builds can be integrated into GitHub Actions or other CI systems. See `.github/workflows/packaging.yml` for an example workflow.

## File Structure

```
repo/
├── docker-compose.packages.yml     # Docker Compose configuration
├── scripts/
│   └── docker-build-all-packages.sh  # Convenience build script
└── README.md                       # This file

docker/
├── Dockerfile.deb                  # DEB package builder
├── Dockerfile.rpm                  # RPM package builder
└── scripts/
    ├── build-deb.sh               # Build script (embedded in image)
    └── build-rpm.sh               # Build script (embedded in image)
```
