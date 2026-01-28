# TODO: Future Release Strategy

This document outlines planned enhancements for the release and distribution process of liblpm.

## Container Registry Strategy

### Goals
- Automate container image publishing on version releases
- Provide pre-built images for users
- Maintain security and trust through signed artifacts

### Tasks

#### 1. Container Registry Push Automation

**Status:** Planned

**Description:**
- Push Docker images to GitHub Container Registry (ghcr.io) on version tag releases
- Only push on semver tags (e.g., v1.2.0, v1.2.1)
- Do NOT push on every commit to avoid registry clutter

**Implementation Steps:**
- [ ] Update `.github/workflows/build.yml` with conditional push job
- [ ] Add GitHub Actions secrets for GHCR authentication
- [ ] Configure image tagging strategy:
  - `latest` - most recent release
  - `v1.2.0` - specific version tag
  - `v1.2` - minor version
  - `v1` - major version
- [ ] Test push workflow on release branch

**Example Workflow Addition:**
```yaml
push-images:
  if: startsWith(github.ref, 'refs/tags/v')
  needs: [build-images, test-containers]
  runs-on: ubuntu-latest
  steps:
    - name: Login to GHCR
      uses: docker/login-action@v3
      with:
        registry: ghcr.io
        username: ${{ github.actor }}
        password: ${{ secrets.GITHUB_TOKEN }}
    
    - name: Push images
      # ... push logic
```

#### 2. GPG Signing of Artifacts

**Status:** Planned

**Description:**
- Sign release tarballs, binaries, and container images with GPG
- Provide verification instructions for users
- Maintain chain of trust

**Implementation Steps:**
- [ ] Generate GPG key for releases
- [ ] Add public key to repository and documentation
- [ ] Configure GitHub Actions to sign artifacts
- [ ] Document verification process for users
- [ ] Sign Docker images using `docker trust` or cosign

**Tools to Consider:**
- GPG for tarballs and checksums
- [cosign](https://github.com/sigstore/cosign) for container images
- [Notary](https://github.com/notaryproject/notary) for Docker trust

#### 3. Release Artifact Generation

**Status:** Planned

**Description:**
- Automate creation of release artifacts (tarballs, binaries, checksums)
- Generate release notes from git history/commits
- Create platform-specific packages

**Artifacts to Generate:**
- [ ] Source tarball (with submodules)
- [ ] Binary tarball for x86_64 Linux
- [ ] Debian/Ubuntu .deb package
- [ ] RPM package for RHEL/Fedora
- [ ] SHA256 checksums file
- [ ] GPG signature files (.asc)

**Implementation:**
- Use GitHub Actions release workflow
- Leverage `goreleaser` or custom scripts
- Upload to GitHub Releases

#### 4. Multi-Architecture Container Builds

**Status:** Planned

**Description:**
- Build and push container images for multiple architectures
- Support amd64 (primary) and arm64

**Implementation Steps:**
- [ ] Configure Docker Buildx for multi-arch
- [ ] Update build scripts to support `--platform linux/amd64,linux/arm64`
- [ ] Test images on both architectures
- [ ] Push manifest lists to registry

**Considerations:**
- ARM64 builds may be slower in CI
- Some dependencies may need architecture-specific handling
- DPDK may have architecture-specific optimizations

#### 5. Security Scanning Integration

**Status:** Planned

**Description:**
- Scan container images for vulnerabilities before release
- Integrate with GitHub Security features
- Fail releases if critical vulnerabilities found

**Implementation Steps:**
- [ ] Add Trivy or Snyk scanning to GitHub Actions
- [ ] Configure vulnerability thresholds
- [ ] Set up automated security alerts
- [ ] Document security response process

**Tools:**
- [Trivy](https://github.com/aquasecurity/trivy)
- [Snyk](https://snyk.io/)
- GitHub Dependabot

## Release Process Documentation

### Tasks

- [ ] Document complete release process step-by-step
- [ ] Create release checklist for maintainers
- [ ] Automate changelog generation
- [ ] Set up semantic versioning guidelines

### Proposed Release Process

1. **Pre-Release Checks**
   - All tests passing in CI
   - Documentation updated
   - CHANGELOG.md updated
   - Version numbers bumped

2. **Tagging**
   - Create signed git tag: `git tag -s v1.2.0 -m "Release v1.2.0"`
   - Push tag: `git push origin v1.2.0`

3. **Automated Build**
   - GitHub Actions triggered on tag push
   - Builds all artifacts
   - Runs full test suite
   - Builds multi-arch containers

4. **Signing**
   - GPG signs all artifacts
   - Signs container images

5. **Publishing**
   - Pushes containers to ghcr.io
   - Creates GitHub Release
   - Uploads artifacts to release
   - Announces release (optional)

6. **Verification**
   - Test installation from published artifacts
   - Verify signatures
   - Pull and test container images

## Container Distribution

### GitHub Container Registry (ghcr.io)

**Advantages:**
- Integrated with GitHub
- Free for public repositories
- Automatic cleanup policies
- Good performance

**Configuration:**
- Registry: `ghcr.io/murilochianfa/liblpm`
- Images: `ghcr.io/murilochianfa/liblpm-dev:latest`

### Docker Hub (Optional)

**Advantages:**
- More discoverable
- Higher download limits for popular images
- Docker official image status (long-term goal)

**Configuration:**
- Requires separate account and setup
- May need Docker Hub access token

## Timeline

- **Phase 1 (Q1 2026):** Container registry push automation
- **Phase 2 (Q2 2026):** GPG signing and artifact generation
- **Phase 3 (Q3 2026):** Multi-architecture builds
- **Phase 4 (Q4 2026):** Security scanning and Docker Hub

## Dependencies

- GitHub Actions runner capabilities
- GPG key management solution
- Container registry access and quotas
- Build time considerations for multi-arch

## Questions to Resolve

1. **GPG Key Management:**
   - Where to store private key securely?
   - Use GitHub Secrets or external key management?
   - Key rotation policy?

2. **Release Frequency:**
   - Follow semantic versioning strictly?
   - Support LTS releases?
   - Backport security fixes?

3. **Container Tagging:**
   - How long to keep old image tags?
   - Cleanup policy for registry?
   - Support for RC/beta images?

4. **Distribution Channels:**
   - Just GitHub + GHCR, or also Linux distro repos?
   - Package managers (apt, yum, homebrew)?
   - Language-specific registries (Go modules, etc.)?

## Man Pages

**Status:** Implemented (January 2026)

**Description:**
Comprehensive man pages for the library API, accessible via `man liblpm`.

**Implemented:**
- [x] Main overview (`liblpm.3`)
- [x] Core API docs (`lpm_create.3`, `lpm_lookup.3`, `lpm_add.3`, `lpm_delete.3`, `lpm_destroy.3`)
- [x] Algorithm reference (`lpm_algorithms.3`)
- [x] Redirect aliases for all function variants
- [x] CMake installation to `${CMAKE_INSTALL_MANDIR}/man3/`
- [x] Included in devel packages (liblpm-dev, liblpm-devel)
- [x] Man page linting in CI (`mandoc -Tlint`)
- [x] Local linting script (`scripts/lint-manpages.sh`)
- [x] Localization infrastructure (CMake support for language directories)
- [x] Portuguese (pt_BR) translation of `liblpm.3` as proof of concept

**Location:** `docs/man/man3/` (English), `docs/man/<lang>/man3/` (translations)

## Related Documentation

- [docs/DOCKER.md](docs/DOCKER.md) - Container usage guide
- [docker/README.md](docker/README.md) - Image reference
- [scripts/docker-build.sh](scripts/docker-build.sh) - Build automation

## Notes

This is a living document. Update as decisions are made and implementation progresses.

Last updated: January 2026
