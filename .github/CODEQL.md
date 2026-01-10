# CodeQL Security Analysis

This directory contains the CodeQL workflow configuration for automated security scanning of the liblpm repository.

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
  - SQL injection (if applicable)
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

### Excluding Paths

To exclude certain paths from analysis, modify the `paths` in the matrix:

```yaml
paths-ignore:
  - 'external/**'
  - 'tests/**'
  - '**/*.test.go'
```

### Adjusting Severity Threshold

Add configuration to fail on specific severity levels:

```yaml
- name: Check for high severity issues
  run: |
    # Check SARIF results for high/critical severity
    # Exit 1 if found
```

## Troubleshooting

### Build Failures

If the C/C++ build fails:
1. Check that all dependencies are installed in the workflow
2. Verify CMake configuration works in CI
3. Review build logs in Actions tab

### Missing Results

If no results appear:
1. Ensure `security-events: write` permission is granted
2. Check that the build completed successfully
3. Verify paths are correct for your language

### False Positives

To suppress false positives:
1. Navigate to the finding in Security tab
2. Click "Dismiss alert"
3. Select reason and add comment
4. Consider adding custom suppression queries

## Best Practices

1. **Review findings promptly**: Security issues should be addressed quickly
2. **Don't dismiss without investigation**: Understand each finding before dismissing
3. **Keep CodeQL updated**: GitHub automatically updates the action versions
4. **Monitor scheduled scans**: Weekly scans catch new vulnerabilities in unchanged code
5. **Integrate with branch protection**: Require CodeQL checks to pass before merging

## Resources

- [CodeQL Documentation](https://codeql.github.com/docs/)
- [CodeQL for C/C++](https://codeql.github.com/docs/codeql-language-guides/codeql-for-cpp/)
- [CodeQL for Go](https://codeql.github.com/docs/codeql-language-guides/codeql-for-go/)
- [GitHub Code Scanning](https://docs.github.com/en/code-security/code-scanning)
- [Writing Custom Queries](https://codeql.github.com/docs/writing-codeql-queries/)

## Support

For issues with CodeQL analysis:
1. Check the [Actions logs](../../actions)
2. Review [Security alerts](../../security/code-scanning)
3. Open an issue with the `security` label
4. Contact repository maintainers
