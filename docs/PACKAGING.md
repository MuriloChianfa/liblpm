# Package Building Guide

This guide explains how to build DEB and RPM packages for liblpm using Docker.

## Overview

liblpm provides Docker-based package building for multiple Linux distributions:

**DEB Packages:**
- Debian Bookworm (12)
- Debian Bullseye (11)
- Ubuntu 24.04 (Noble Numbat)
- Ubuntu 22.04 (Jammy Jellyfish)

**RPM Packages:**
- Rocky Linux 9
- Rocky Linux 8
- Fedora (latest)

## Quick Start

### Build All Packages

```bash
./scripts/docker-build-packages.sh all
```

This will build packages for all supported distributions and place them in the `packages/` directory.

### Build Specific Distribution

```bash
# Build only DEB packages
./scripts/docker-build-packages.sh deb

# Build only RPM packages
./scripts/docker-build-packages.sh rpm

# Build for Debian Bookworm
./scripts/docker-build-packages.sh debian-bookworm

# Build for Rocky Linux 9
./scripts/docker-build-packages.sh rocky-9
```

## Package Structure

Each build produces two packages:

### DEB Packages

1. **Runtime Package (`liblpm`)**
   - Shared library: `liblpm.so.2.0.0`
   - Symlinks: `liblpm.so.1` → `liblpm.so.2.0.0`
   - Location: `/usr/lib/x86_64-linux-gnu/`

2. **Development Package (`liblpm-dev`)**
   - Static library: `liblpm.a`
   - Header files: `/usr/include/lpm/*.h`
   - pkg-config file: `/usr/lib/x86_64-linux-gnu/pkgconfig/liblpm.pc`
   - CMake config files: `/usr/lib/x86_64-linux-gnu/cmake/liblpm/`

### RPM Packages

1. **Runtime Package (`liblpm`)**
   - Shared library: `liblpm.so.2.0.0`
   - Symlinks: `liblpm.so.1` → `liblpm.so.2.0.0`
   - Location: `/usr/lib64/`

2. **Development Package (`liblpm-devel`)**
   - Static library: `liblpm.a`
   - Header files: `/usr/include/lpm/*.h`
   - pkg-config file: `/usr/lib64/pkgconfig/liblpm.pc`
   - CMake config files: `/usr/lib64/cmake/liblpm/`

## Manual Building

### Build DEB Package Manually

```bash
# Build Docker image
docker build -f docker/Dockerfile.deb -t liblpm-deb .

# Build packages
docker run --rm \
    -v "$PWD:/workspace:ro" \
    -v "$PWD/packages:/packages" \
    liblpm-deb

# View packages
ls -lh packages/*.deb
```

### Build RPM Package Manually

```bash
# Build Docker image
docker build -f docker/Dockerfile.rpm -t liblpm-rpm .

# Build packages
docker run --rm \
    -v "$PWD:/workspace:ro" \
    -v "$PWD/packages:/packages" \
    liblpm-rpm

# View packages
ls -lh packages/*.rpm
```

### Build for Specific Distribution

#### Debian Bullseye

```bash
docker build -f docker/Dockerfile.deb \
    --build-arg DEBIAN_VERSION=bullseye \
    -t liblpm-deb:bullseye .

docker run --rm \
    -v "$PWD:/workspace:ro" \
    -v "$PWD/packages/debian-bullseye:/packages" \
    liblpm-deb:bullseye
```

#### Ubuntu 24.04

```bash
docker build -f docker/Dockerfile.deb \
    --build-arg DEBIAN_VERSION=ubuntu:24.04 \
    -t liblpm-deb:ubuntu24.04 .

docker run --rm \
    -v "$PWD:/workspace:ro" \
    -v "$PWD/packages/ubuntu-24.04:/packages" \
    liblpm-deb:ubuntu24.04
```

#### Rocky Linux 8

```bash
docker build -f docker/Dockerfile.rpm \
    --build-arg ROCKYLINUX_VERSION=8 \
    -t liblpm-rpm:rocky8 .

docker run --rm \
    -v "$PWD:/workspace:ro" \
    -v "$PWD/packages/rocky-8:/packages" \
    liblpm-rpm:rocky8
```

## Testing Packages

### Test DEB Package

```bash
# Install runtime package
sudo dpkg -i packages/debian-bookworm/liblpm_2.0.0_amd64.deb

# Check installation
dpkg -L liblpm
ldconfig -p | grep liblpm

# Install development package
sudo dpkg -i packages/debian-bookworm/liblpm-dev_2.0.0_amd64.deb

# Verify headers
ls -la /usr/include/lpm/

# Test with pkg-config
pkg-config --modversion liblpm
pkg-config --libs liblpm
pkg-config --cflags liblpm

# Uninstall
sudo dpkg -r liblpm-dev liblpm
```

### Test RPM Package

```bash
# Install runtime package
sudo rpm -ivh packages/rocky-9/liblpm-2.0.0.x86_64.rpm

# Check installation
rpm -ql liblpm
ldconfig -p | grep liblpm

# Install development package
sudo rpm -ivh packages/rocky-9/liblpm-devel-2.0.0.x86_64.rpm

# Verify headers
ls -la /usr/include/lpm/

# Test with pkg-config
pkg-config --modversion liblpm
pkg-config --libs liblpm
pkg-config --cflags liblpm

# Uninstall
sudo rpm -e liblpm-devel liblpm
```

## Package Contents

### Verify Package Contents

#### DEB Package

```bash
# List files
dpkg-deb -c packages/debian-bookworm/liblpm_2.0.0_amd64.deb
dpkg-deb -c packages/debian-bookworm/liblpm-dev_2.0.0_amd64.deb

# Show package info
dpkg-deb -I packages/debian-bookworm/liblpm_2.0.0_amd64.deb
```

#### RPM Package

```bash
# List files
rpm -qlp packages/rocky-9/liblpm-2.0.0.x86_64.rpm
rpm -qlp packages/rocky-9/liblpm-devel-2.0.0.x86_64.rpm

# Show package info
rpm -qip packages/rocky-9/liblpm-2.0.0.x86_64.rpm
```

## Advanced Usage

### Clean Build

```bash
# Remove all packages and rebuild
./scripts/docker-build-packages.sh --clean all
```

### No Cache Build

```bash
# Force rebuild of Docker images
./scripts/docker-build-packages.sh --no-cache all
```

### Verbose Output

```bash
# Show detailed build output
./scripts/docker-build-packages.sh --verbose all
```

### Build Multiple Specific Distributions

```bash
# Build for multiple distributions at once
./scripts/docker-build-packages.sh debian-bookworm rocky-9 ubuntu-24.04
```

## Package Repository Deployment

After building packages, you can deploy them to APT and YUM repositories. See the repository management scripts:

```bash
# Deploy packages to repositories
./repo/scripts/deploy-packages.sh

# Set up APT repository
./repo/scripts/setup-apt-repo.sh

# Set up YUM repositories
./repo/scripts/setup-rpm-repos.sh
```

For detailed repository setup instructions, see [REPOSITORY.md](REPOSITORY.md).

## Troubleshooting

### Build Failures

If package build fails, check:

1. **Git submodules**: Ensure `external/libdynemit` is initialized
   ```bash
   git submodule update --init --recursive
   ```

2. **CMake version**: Requires CMake 3.16 or higher
   ```bash
   cmake --version
   ```

3. **Build dependencies**: Check Docker image has all required packages
   ```bash
   docker run --rm liblpm-deb bash -c "dpkg -l | grep -E 'cmake|gcc|git'"
   ```

### Package Installation Failures

If package installation fails:

1. **Missing dependencies**: Install required runtime libraries
   ```bash
   # DEB
   sudo apt-get install -f
   
   # RPM
   sudo dnf install liblpm
   ```

2. **Architecture mismatch**: Ensure you're on x86_64
   ```bash
   uname -m  # Should show x86_64
   ```

3. **Library conflicts**: Check for existing liblpm installations
   ```bash
   ldconfig -p | grep lpm
   dpkg -l | grep lpm  # DEB
   rpm -qa | grep lpm  # RPM
   ```

### Docker Issues

1. **Insufficient disk space**
   ```bash
   docker system df
   docker system prune -a  # Clean up unused images
   ```

2. **Permission denied**
   ```bash
   # Add user to docker group
   sudo usermod -aG docker $USER
   newgrp docker
   ```

## CI/CD Integration

### GitHub Actions

The project includes automated package building in `.github/workflows/packaging.yml`:

```yaml
- name: Build DEB packages
  run: ./scripts/docker-build-packages.sh deb

- name: Build RPM packages
  run: ./scripts/docker-build-packages.sh rpm

- name: Upload packages
  uses: actions/upload-artifact@v3
  with:
    name: packages
    path: packages/
```

### GitLab CI

```yaml
build-packages:
  stage: build
  script:
    - ./scripts/docker-build-packages.sh all
  artifacts:
    paths:
      - packages/
    expire_in: 1 week
```

## Package Versioning

Package version is defined in `CMakeLists.txt`:

```cmake
project(liblpm VERSION 2.0.0 LANGUAGES C)
```

To change version:

1. Update `CMakeLists.txt`
2. Update `CITATION.cff`
3. Rebuild packages

## Build Scripts Reference

### docker-build-packages.sh

Main script for building packages across distributions.

**Options:**
- `-h, --help`: Show help
- `-c, --clean`: Clean packages directory
- `-n, --no-cache`: Build without cache
- `-v, --verbose`: Verbose output

**Targets:**
- `all`: All distributions
- `deb`: All DEB packages
- `rpm`: All RPM packages
- `debian-bookworm`: Debian 12
- `debian-bullseye`: Debian 11
- `ubuntu-24.04`: Ubuntu 24.04
- `ubuntu-22.04`: Ubuntu 22.04
- `rocky-9`: Rocky Linux 9
- `rocky-8`: Rocky Linux 8
- `fedora`: Fedora latest

### docker/scripts/build-deb.sh

Internal script executed inside DEB build container. Not called directly.

### docker/scripts/build-rpm.sh

Internal script executed inside RPM build container. Not called directly.

## File Locations

```
liblpm/
├── docker/
│   ├── Dockerfile.deb          # DEB builder Dockerfile
│   ├── Dockerfile.rpm          # RPM builder Dockerfile
│   └── scripts/
│       ├── build-deb.sh        # DEB build script (internal)
│       └── build-rpm.sh        # RPM build script (internal)
├── scripts/
│   └── docker-build-packages.sh  # Main package build script
├── packages/                    # Output directory (created)
│   ├── debian-bookworm/
│   ├── debian-bullseye/
│   ├── ubuntu-24.04/
│   ├── ubuntu-22.04/
│   ├── rocky-9/
│   └── rocky-8/
└── CMakeLists.txt              # Package configuration
```

## Support

For issues or questions:

- **GitHub Issues**: https://github.com/MuriloChianfa/liblpm/issues
- **Documentation**: https://github.com/MuriloChianfa/liblpm/docs
- **Email**: murilo.chianfa@outlook.com
