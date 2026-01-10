# CodeQL Security Analysis

This repository uses CodeQL for automated security scanning of C/C++ and Go code.

## Overview

The CodeQL workflow (`codeql.yml`) performs static analysis security testing (SAST) on:

- **C/C++ Code**: Main library source code in `src/`, headers in `include/`, and C++ bindings in `bindings/cpp/`
- **Go Code**: Go bindings in `bindings/go/`

## Workflow Triggers

The CodeQL analysis runs automatically on:

1. **Push events** to `main` and `develop` branches
2. **Pull requests** targeting `main` and `develop` branches
3. **Scheduled runs** every Monday at 2:30 AM UTC
4. **Manual dispatch** via GitHub Actions UI

## What Gets Analyzed

### C/C++ Analysis
- Core library implementation (`src/*.c`)
- Public API headers (`include/*.h`)
- C++ bindings (`bindings/cpp/**`)
- Checks for:
  - Buffer overflows
  - Memory leaks
  - Use-after-free vulnerabilities
  - Integer overflows
  - Format string vulnerabilities
  - Race conditions
  - And more security issues

### Go Analysis
- Go bindings (`bindings/go/**`)
- Checks for:
  - Command injection
  - Path traversal
  - Unsafe use of CGo
  - Resource leaks
  - And more Go-specific security issues

## Query Suites

The workflow uses enhanced query suites:
- `security-extended`: Extended set of security queries
- `security-and-quality`: Security queries plus code quality checks

## Build Process

### C/C++ Build
The workflow builds the C/C++ code manually to ensure complete coverage:
1. Builds main library with CMake
2. Builds C++ bindings separately
3. Uses Release build type for optimization analysis

### Go Build
The Go code uses CodeQL's autobuild feature, which automatically detects and builds Go modules.

## Viewing Results

### Pull Requests
- CodeQL findings appear as annotations
- New issues are highlighted in PRs

## Configuration

The analysis is configured in:
- `.github/workflows/codeql.yml` - Main workflow
- `.github/codeql/codeql-config.yml` - Path configuration
- `.github/codeql/custom-queries/` - Custom security queries (optional)

### Excluding Paths

To exclude certain paths from analysis, edit `.github/codeql/codeql-config.yml`:

```yaml
paths-ignore:
  - tests
  - benchmarks
  - external
```

## Custom Queries

Custom security queries specific to liblpm are available in `.github/codeql/custom-queries/`:
- `BufferOverflow.ql` - Detects potential buffer overflows in LPM operations
- `UnsafeCgoPointer.ql` - Detects unsafe CGo pointer usage

## Best Practices

1. **Review findings promptly**: Address security issues quickly
2. **Don't dismiss without investigation**: Understand each finding
3. **Monitor scheduled scans**: Weekly scans catch new vulnerabilities

## Resources

- [CodeQL Documentation](https://codeql.github.com/docs/)
- [CodeQL for C/C++](https://codeql.github.com/docs/codeql-language-guides/codeql-for-cpp/)
- [CodeQL for Go](https://codeql.github.com/docs/codeql-language-guides/codeql-for-go/)
- [GitHub Code Scanning](https://docs.github.com/en/code-security/code-scanning)

## Support

For issues with CodeQL analysis:
1. Check the [Actions logs](../../actions)
2. Review [Security alerts](../../security/code-scanning)
3. Open an issue with the `security` label
4. Contact repository maintainers
