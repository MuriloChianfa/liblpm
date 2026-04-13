# Development Guide

## Building from Source

```bash
git clone --recursive https://github.com/MuriloChianfa/liblpm.git
cd liblpm
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build . -j$(nproc)
ctest --output-on-failure
```

## Static Analysis with clang-tidy

liblpm ships a `.clang-tidy` configuration at the repository root. To run
clang-tidy during the build, enable the CMake option and use Clang as the
compiler:

```bash
mkdir build && cd build
cmake .. \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_BUILD_TYPE=Debug \
    -DLPM_ENABLE_CLANG_TIDY=ON \
    -DBUILD_TESTS=OFF
cmake --build . -j$(nproc)
```

All warnings are promoted to errors (`WarningsAsErrors: '*'`), so the build
will fail if any check fires. The vendored `libdynemit` submodule is
automatically excluded from analysis.

### Using a specific clang-tidy version

CMake searches for `clang-tidy-21`, `clang-tidy-20`, ..., `clang-tidy` in
that order. To force a particular version, set `CMAKE_C_CLANG_TIDY` directly:

```bash
cmake .. \
    -DCMAKE_C_COMPILER=clang-18 \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_CLANG_TIDY=clang-tidy-18
```

### Enabled check categories

| Category | Purpose |
|---|---|
| `bugprone-*` | Common bug patterns |
| `cert-*` | CERT C secure coding |
| `clang-analyzer-*` | Deep static analysis |
| `misc-*` | Miscellaneous useful checks |
| `performance-*` | Performance pitfalls |
| `readability-*` | Code clarity and consistency |

Some checks are intentionally disabled because they conflict with low-level
SIMD code or are too noisy for this codebase. See `.clang-tidy` for the full
exclusion list.

### IDE integration

`CMAKE_EXPORT_COMPILE_COMMANDS` is always enabled, so a `compile_commands.json`
is generated in the build directory. Point your editor or clangd to it for
real-time diagnostics:

```bash
ln -s build/compile_commands.json .
```
