## Feature Description

<!-- Provide a clear and concise description of the new feature -->

## Related Issue

<!-- Link to the feature request issue -->

Closes #

## Motivation

<!-- Why is this feature needed? What problem does it solve? -->

## Proposed Changes

<!-- Detailed description of the implementation -->

### API Changes

<!-- If this adds/modifies public API -->

```c
/* New/modified function signatures */
```

### Architecture Changes

<!-- If this changes internal architecture -->

## Implementation Details

<!-- Technical details about the implementation -->

### Core Changes

- 
- 

### Algorithm/Data Structure

<!-- Explain any new algorithms or data structures -->

### SIMD Optimizations

<!-- If applicable, describe SIMD implementations -->

- [ ] Scalar implementation
- [ ] SSE2 implementation
- [ ] AVX2 implementation
- [ ] AVX512 implementation
- [ ] Uses ifunc dispatch

## Design Decisions

<!-- Explain key design choices and trade-offs -->

### Alternatives Considered

<!-- What other approaches did you consider? -->

1. **Alternative 1**: 
   - Pros: 
   - Cons: 
   
2. **Alternative 2**: 
   - Pros: 
   - Cons: 

### Why This Approach?

<!-- Justify your design choice -->

## Usage Examples

### Basic Usage

```c
/* Example code showing how to use the new feature */
```

### Advanced Usage

```c
/* More complex usage example */
```

## Test Environment

- **OS**: <!-- e.g., Ubuntu 22.04 -->
- **Compiler**: <!-- e.g., GCC 11.4.0 -->
- **CPU**: <!-- e.g., Intel Core i7-12700K -->
- **SIMD Support**: <!-- e.g., AVX2 -->

## Testing

### Test Coverage

- [ ] Unit tests for all new functions
- [ ] Edge case tests
- [ ] Error handling tests
- [ ] Integration tests
- [ ] Fuzz tests (if security-relevant)
- [ ] IPv4 tests
- [ ] IPv6 tests

### Test Results

```
<!-- Test output -->
```

### Memory Safety

- [ ] Valgrind: No leaks
- [ ] AddressSanitizer: No issues
- [ ] Static analysis: No warnings

## Performance

### Benchmarks

<!-- Required for performance-related features -->

#### Before (baseline)

```
<!-- Benchmark results before this feature -->
```

#### After (with feature)

```
<!-- Benchmark results with this feature -->
```

#### Analysis

<!-- Interpret the benchmark results -->

- **Throughput impact**: 
- **Latency impact**: 
- **Memory impact**: 

### Performance Characteristics

- **Time complexity**: O(?)
- **Space complexity**: O(?)
- **Cache behavior**: 
- **Scalability**: 

## Documentation

- [ ] Code comments for public API
- [ ] Internal documentation for complex logic
- [ ] README.md updated
- [ ] API documentation updated
- [ ] Example programs added/updated
- [ ] Language bindings updated (C++/Go)
- [ ] CHANGELOG entry (if applicable)

## Backward Compatibility

- [ ] Fully backward compatible
- [ ] Deprecates old API (migration guide provided)
- [ ] Breaking change (justified and documented)

### Compatibility Details

<!-- Explain any compatibility considerations -->

## Language Bindings

### C++ Binding

- [ ] C++ wrapper updated
- [ ] C++ tests added
- [ ] C++ examples updated
- [ ] Not applicable

### Go Binding

- [ ] Go wrapper updated
- [ ] Go tests added
- [ ] Go examples updated
- [ ] Not applicable

## Configuration Options

<!-- If this adds new configuration or build options -->

- [ ] CMake options added/modified
- [ ] Default values documented
- [ ] Build tested with various configurations

## Dependencies

<!-- Does this feature introduce new dependencies? -->

- [ ] No new dependencies
- [ ] New dependencies added (justified below)

### New Dependencies

<!-- If applicable, justify each new dependency -->

## Security Considerations

<!-- Any security implications of this feature? -->

- [ ] No security implications
- [ ] Security review completed
- [ ] Input validation added
- [ ] Fuzzing performed

## Migration Guide

<!-- If this is a breaking change or deprecation -->

### For Users Migrating From

<!-- Provide step-by-step migration instructions -->

## Future Work

<!-- Related features or improvements for future PRs -->

- 
- 

## Checklist

- [ ] Feature is complete and tested
- [ ] Code follows project standards
- [ ] All tests pass
- [ ] Performance benchmarks included
- [ ] Documentation complete
- [ ] Examples provided
- [ ] Backward compatibility maintained (or justified)
- [ ] Language bindings updated
- [ ] CI checks pass
- [ ] Tested on multiple platforms/compilers
- [ ] Memory safety verified

## Screenshots/Visualizations

<!-- If applicable, add diagrams, performance charts, etc. -->

## Reviewer Notes

<!-- Areas you'd like reviewers to focus on -->

### Review Focus Areas

- 
- 

### Open Questions

<!-- Questions for reviewers -->

- 
- 
