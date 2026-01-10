# Security Policy

## Supported Versions

The following versions are currently being supported with security updates:

| Version | Supported |
| ------- | --------- |
| 2.x.x   |     ✅    |
| < 2.0   |     ❌    |

## Reporting a Vulnerability

The liblpm team takes security vulnerabilities seriously. We appreciate your efforts to responsibly disclose your findings and will make every effort to acknowledge your contributions.

### How to Report a Security Vulnerability

**Please DO NOT report security vulnerabilities through public GitHub issues.**

Instead, please report them via email to:

**murilo.chianfa@outlook.com**

### What to Include in Your Report

To help us better understand the nature and scope of the security issue, please include as much of the following information as possible:

- **Type of vulnerability** (e.g., buffer overflow, memory corruption, use-after-free, etc.)
- **Full paths of source file(s)** related to the vulnerability
- **Location of the affected source code** (tag/branch/commit or direct URL)
- **Step-by-step instructions** to reproduce the issue
- **Proof-of-concept or exploit code** (if available)
- **Impact of the vulnerability**, including how an attacker might exploit it
- **Affected versions** of liblpm
- **Any special configuration** required to reproduce the issue

### Preferred Language

Please use **English** for all communications.

## Response Timeline

- **Initial Response**: We aim to acknowledge receipt of your vulnerability report within **48 hours**.
- **Status Updates**: We will send you regular updates about our progress, at least every **7 days**.
- **Disclosure Timeline**: We aim to patch critical vulnerabilities within **90 days** of the initial report.

## What to Expect

### After You Submit a Report

1. **Acknowledgment**: We will confirm receipt of your report within 48 hours
2. **Assessment**: We will assess the vulnerability and determine its severity
3. **Communication**: We will keep you informed of our progress
4. **Fix Development**: We will develop a patch for the vulnerability
5. **Testing**: We will test the fix thoroughly
6. **Disclosure**: We will coordinate with you on the disclosure timeline

### Severity Assessment

We use the following criteria to assess vulnerability severity:

- **Critical**: Remote code execution, privilege escalation, or data corruption affecting all users
- **High**: Memory corruption or security bypass affecting most users in default configurations
- **Medium**: Limited impact security issues affecting specific configurations
- **Low**: Minor issues with minimal security impact

## Security Update Process

When we release a security fix:

1. **Private Patch**: We first create a private patch
2. **Notification**: We notify you and request validation of the fix
3. **Release**: We release the patch in a new version
4. **Advisory**: We publish a security advisory with details
5. **Credit**: We credit you in the advisory (unless you prefer to remain anonymous)

## Public Disclosure

We follow a **coordinated disclosure** process:

- We will work with you to understand the issue and develop a fix
- We will not publicly disclose the issue until a patch is available
- We coordinate disclosure timing with you
- We credit security researchers who report vulnerabilities responsibly

### Disclosure Timeline

- **Day 0**: Initial report received
- **Day 1-7**: Assessment and confirmation
- **Day 7-90**: Fix development and testing
- **Day 90+**: Public disclosure (if no fix is available, we'll discuss options)

## Security Best Practices for Users

### When Using liblpm

1. **Keep Updated**: Always use the latest stable version
2. **Input Validation**: Validate all input data before passing to liblpm
3. **Memory Safety**: Use Valgrind or AddressSanitizer during development
4. **Monitoring**: Monitor for unusual behavior or crashes

### Runtime Considerations

- **Memory Limits**: Set appropriate memory limits for your application
- **Error Handling**: Always check return values from liblpm functions
- **Resource Cleanup**: Properly destroy LPM tries when done

## Known Security Features

### Memory Safety

- All array accesses are bounds-checked
- Extensive fuzzing with AFL++
- Valgrind testing for memory leaks
- AddressSanitizer testing in CI

### Thread Safety

- Core lookup functions are thread-safe (read-only operations)
- Modify operations (add/delete) require external synchronization
- No global mutable state

### Attack Surface

- **Input Validation**: All prefix lengths and addresses are validated
- **Integer Overflow**: Protected against integer overflow in size calculations
- **Resource Exhaustion**: Memory limits prevent unbounded allocation
- **Denial of Service**: Lookup operations have bounded time complexity

## Security Advisories

Published security advisories can be found at:

- GitHub Security Advisories: https://github.com/MuriloChianfa/liblpm/security/advisories
- Release Notes: https://github.com/MuriloChianfa/liblpm/releases

## Bug Bounty Program

We do not currently have a bug bounty program, but we deeply appreciate security research and will publicly acknowledge your contributions (with your permission).

## Hall of Fame

We recognize security researchers who have helped improve liblpm's security:

<!-- Security researchers will be listed here -->

_No security vulnerabilities have been reported yet._

## PGP Key

For encrypted communication, you can use the following PGP key:

```
Key ID: 12D0D82387FC53B0
Fingerprint: 3E1A 1F40 1A1C 47BC 77D1 7056 12D0 D823 87FC 53B0
```

Import the key:
```bash
gpg --keyserver keys.openpgp.org --recv-keys 12D0D82387FC53B0
```

## Additional Resources

- [CWE - Common Weakness Enumeration](https://cwe.mitre.org/)
- [CVE - Common Vulnerabilities and Exposures](https://cve.mitre.org/)

## Questions?

If you have questions about this security policy, please email us.

---

**Thank you for helping keep liblpm and its users safe!**
