# IFUNC and Python Bindings

## Known Issue

The `liblpm` library uses GNU IFUNC (Indirect Functions) for runtime CPU feature detection and SIMD dispatch. This works great for normal C/C++ programs, but causes issues when loaded via `dlopen()` by Python.

### Root Cause

When Python loads the `_liblpm` extension module via `dlopen()`, the dynamic linker tries to resolve IFUNC symbols. However, in some contexts (especially with `RTLD_LOCAL`), the IFUNC resolvers may execute before the library is fully initialized, leading to segmentation faults.

### Thread-Safe Resolvers

The library now includes thread-safe IFUNC resolvers (enabled via `LPM_TS_RESOLVERS` CMake option) that use atomic caching to avoid race conditions. While this improves safety, it does **not** fully eliminate the dlopen/IFUNC incompatibility. The fundamental issue remains that IFUNC resolution happens during dlopen(), which Python's module loading system can trigger at unsafe times.

## Workaround

Use `LD_PRELOAD` to force early relocation of the `liblpm` shared library:

```bash
export LD_PRELOAD=/usr/local/lib/liblpm.so.1
python your_script.py
```

Or set it inline:

```bash
LD_PRELOAD=/usr/local/lib/liblpm.so.1 python your_script.py
```

### Why This Works

- Forces `liblpm.so` to be loaded at process startup (before Python's `dlopen()`)
- IFUNC resolvers run during early startup when it's safe
- CPU feature detection happens once, correctly
- All subsequent references use the pre-resolved function pointers

## Permanent Solutions

### For System-Wide Installation

Add to your shell profile (`~/.bashrc`, `~/.zshrc`, etc.):

```bash
export LD_PRELOAD=/usr/local/lib/liblpm.so.1
```

### For Virtual Environments

Add to your venv activation script (`venv/bin/activate`):

```bash
export LD_PRELOAD=/usr/local/lib/liblpm.so.1
```

### For Docker

Set the environment variable in your Dockerfile:

```dockerfile
ENV LD_PRELOAD=/usr/local/lib/liblpm.so.1
```

### For systemd Services

Add to your service file:

```ini
[Service]
Environment="LD_PRELOAD=/usr/local/lib/liblpm.so.1"
```

## Technical Details

- **IFUNC**: GNU extension for runtime function dispatch
- **dlopen()**: POSIX function Python uses to load extension modules  
- **RTLD_LOCAL**: Default mode isolates symbol resolution
- **Early vs Late Binding**: IFUNC needs early binding but dlopen() often does late binding

## References

- [GNU IFUNC Documentation](https://sourceware.org/glibc/wiki/GNU_IFUNC)
- [dlopen Man Page](https://man7.org/linux/man-pages/man3/dlopen.3.html)
