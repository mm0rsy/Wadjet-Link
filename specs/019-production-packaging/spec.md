# Feature Specification: Production Packaging & Distribution

**Feature Branch**: `milestone/019-production-packaging`  
**Created**: 2026-01-09  
**Status**: Planned (Not Yet Implemented)  
**Milestone**: M19 - Production Packaging & Distribution

## Overview

Create production-ready packaging and distribution infrastructure for Wadjet-Link. This includes native installers for Linux distributions, Docker images, cross-platform builds, CI/CD automation, and release management. The goal is to make Wadjet-Link easily installable and deployable across target platforms.

## User Scenarios & Testing

### User Story 1 - Native Linux Package Installation (Priority: P1)

As a Linux user, I need native package installation (DEB/RPM/Snap), so that I can install Wadjet-Link using standard package managers.

**Why this priority**: Native packages are expected by Linux users - enables simple `apt install` or `dnf install`.

**Independent Test**: Install Wadjet-Link via package manager and verify functionality.

**Acceptance Scenarios**:

1. **Given** Debian-based system (Ubuntu, Debian), **When** `sudo apt install wadjet-link.deb` executed, **Then** all binaries, libraries, and headers are installed
2. **Given** RPM-based system (Fedora, RHEL), **When** `sudo dnf install wadjet-link.rpm` executed, **Then** package installs successfully
3. **Given** Snap package, **When** `sudo snap install wadjet-link` executed, **Then** confined app runs with network permissions
4. **Given** package installation, **When** complete, **Then** `wadjet --version` returns correct version
5. **Given** package dependencies, **When** installed, **Then** all runtime dependencies are satisfied automatically

### User Story 2 - Docker Image Deployment (Priority: P1)

As a DevOps engineer, I need Docker images for Wadjet-Link, so that I can run tests in containerized CI/CD pipelines.

**Why this priority**: Docker is standard for CI/CD - enables reproducible test environments.

**Independent Test**: Pull Docker image and run Wadjet-Link commands inside container.

**Acceptance Scenarios**:

1. **Given** Docker Hub repository, **When** `docker pull wadjet/wadjet-link:latest` executed, **Then** image is downloaded
2. **Given** Docker container, **When** `docker run wadjet/wadjet-link wadjet --version` executed, **Then** version is displayed
3. **Given** PCAP volume mount, **When** container runs, **Then** host PCAP files are accessible
4. **Given** multi-stage Docker build, **When** image built, **Then** final image size ≤500MB
5. **Given** Docker Compose setup, **When** deployed, **Then** Wadjet-Link integrates with other services (e.g., Grafana)

### User Story 3 - Cross-Platform Build Support (Priority: P2)

As a developer, I need cross-platform build support (Linux, Windows, macOS), so that I can use Wadjet-Link on my preferred platform.

**Why this priority**: Multi-platform support expands user base - especially for Windows development workstations.

**Independent Test**: Build Wadjet-Link on Windows and macOS, verify core functionality.

**Acceptance Scenarios**:

1. **Given** Windows 10/11 with MSVC 2022, **When** built, **Then** all core libraries compile successfully
2. **Given** macOS with Xcode, **When** built, **Then** binaries run on macOS 13+ (Ventura)
3. **Given** Windows build with Npcap SDK, **When** packet capture attempted, **Then** Npcap backend works
4. **Given** macOS build with libpcap, **When** packet capture attempted, **Then** libpcap backend works
5. **Given** cross-platform CI, **When** triggered, **Then** builds succeed on Linux, Windows, macOS runners

### User Story 4 - Release Automation (Priority: P1)

As a maintainer, I need automated release builds and publishing, so that new versions are consistently packaged and distributed.

**Why this priority**: Manual releases are error-prone - automation ensures quality and consistency.

**Independent Test**: Trigger release workflow and verify all artifacts are generated.

**Acceptance Scenarios**:

1. **Given** Git tag `v1.0.0` pushed, **When** release workflow triggered, **Then** DEB, RPM, Snap, tarball, Docker image are built
2. **Given** release artifacts, **When** generated, **Then** all artifacts contain version 1.0.0
3. **Given** changelog, **When** release created, **Then** GitHub release notes include changelog
4. **Given** GPG signing, **When** enabled, **Then** all packages are signed with maintainer key
5. **Given** Docker image, **When** published, **Then** both `:latest` and `:1.0.0` tags are pushed

### User Story 5 - Documentation Packaging (Priority: P2)

As a user, I need bundled documentation with installation, so that I can access guides offline.

**Why this priority**: Offline docs improve user experience - especially in secure/air-gapped environments.

**Independent Test**: Install package and access bundled documentation.

**Acceptance Scenarios**:

1. **Given** installed package, **When** `/usr/share/doc/wadjet-link` accessed, **Then** quickstart.md, architecture.md, scenarios.md are present
2. **Given** Doxygen HTML docs, **When** generated, **Then** API reference is included in package
3. **Given** man pages, **When** `man wadjet` executed, **Then** man page is displayed
4. **Given** example files, **When** `/usr/share/wadjet-link/examples` accessed, **Then** sample YAML scenarios and C++ examples are present
5. **Given** Docker image, **When** docs accessed, **Then** documentation is available at `/usr/share/doc/wadjet-link`

## Edge Cases

- What happens when package conflicts with existing system libraries?
- How are older package versions uninstalled during upgrade?
- What if Docker build cache becomes stale (dependency version changes)?
- How does system handle partial package installations (interrupted mid-install)?
- What happens when GPG key is not trusted on target system?
- How are multi-architecture builds handled (x86_64, ARM64)?

## Requirements

### Functional Requirements

#### Debian/Ubuntu Packaging (DEB)

- **FR-001**: System MUST generate Debian package (.deb) for Ubuntu 20.04+, Debian 11+
- **FR-002**: DEB package MUST include binaries (`wadjet`, `wadjet-run`, `wadjet-arxml`, `wadjet-odx`, `wadjet-signals`)
- **FR-003**: DEB package MUST include shared libraries (`libwadjet.so`, `libwadjet_c.so`)
- **FR-004**: DEB package MUST include development headers (`/usr/include/wadjet/`)
- **FR-005**: DEB package MUST include pkg-config file (`/usr/lib/pkgconfig/wadjet.pc`)
- **FR-006**: DEB package MUST declare dependencies (libc6, libstdc++6, libpcap0.8)
- **FR-007**: DEB package MUST include post-install scripts for ldconfig

#### Fedora/RHEL Packaging (RPM)

- **FR-008**: System MUST generate RPM package for Fedora 38+, RHEL 9+
- **FR-009**: RPM package MUST include all binaries and libraries (same as DEB)
- **FR-010**: RPM package MUST declare dependencies (glibc, libstdc++, libpcap)
- **FR-011**: RPM spec file MUST define %files, %build, %install sections correctly

#### Snap Packaging

- **FR-012**: System MUST generate Snap package for Ubuntu Core and Linux distributions
- **FR-013**: Snap MUST request network plug for packet capture
- **FR-014**: Snap MUST include all dependencies (vendored if not in base snap)
- **FR-015**: Snap MUST work in strict confinement with appropriate interfaces

#### Docker Images

- **FR-016**: System MUST build Docker image based on Ubuntu 22.04 LTS
- **FR-017**: Docker image MUST include all Wadjet-Link binaries and libraries
- **FR-018**: Docker image MUST use multi-stage build (builder + runtime stages)
- **FR-019**: Final Docker image size MUST be ≤500MB compressed
- **FR-020**: Docker image MUST be published to Docker Hub (wadjet/wadjet-link)
- **FR-021**: Docker image MUST support version tags (:latest, :1.0.0, :1.0, :1)

#### Cross-Platform Builds

- **FR-022**: System MUST support Windows build with MSVC 2022 and CMake
- **FR-023**: Windows build MUST use Npcap SDK for packet capture
- **FR-024**: System MUST support macOS build with Xcode 14+ and CMake
- **FR-025**: macOS build MUST use libpcap for packet capture
- **FR-026**: CI/CD MUST run builds on Linux (Ubuntu), Windows (Server 2022), macOS (13 Ventura)

#### Release Automation

- **FR-027**: System MUST automate release builds triggered by Git tags (vX.Y.Z)
- **FR-028**: Release workflow MUST build DEB, RPM, Snap, tarball, Docker image
- **FR-029**: Release artifacts MUST be attached to GitHub release
- **FR-030**: Release MUST generate changelog from Git commits (conventional commits)
- **FR-031**: Release MUST support GPG signing of packages (optional)
- **FR-032**: Release MUST publish Docker image to Docker Hub with tags

#### Documentation Packaging

- **FR-033**: Packages MUST include markdown documentation (`/usr/share/doc/wadjet-link/`)
- **FR-034**: Packages MUST include Doxygen-generated HTML API docs
- **FR-035**: Packages MUST include man pages for CLI tools
- **FR-036**: Packages MUST include example files (`/usr/share/wadjet-link/examples/`)

#### Tarball Distribution

- **FR-037**: System MUST generate source tarball (wadjet-link-X.Y.Z.tar.gz)
- **FR-038**: System MUST generate binary tarball for Linux x86_64
- **FR-039**: Tarballs MUST include all necessary files for installation or build
- **FR-040**: Binary tarball MUST include install.sh script for /usr/local installation

### Key Entities

- **DebPackageBuilder**: Generates Debian package
- **RpmPackageBuilder**: Generates RPM package
- **SnapPackageBuilder**: Generates Snap package
- **DockerImageBuilder**: Builds Docker image
- **ReleaseWorkflow**: GitHub Actions workflow for releases
- **ChangelogGenerator**: Generates changelog from commits
- **PackageMetadata**: Version, description, maintainer, license info
- **CrossPlatformBuilder**: Manages builds across OS/architectures

## Success Criteria

### Measurable Outcomes

- **SC-001**: DEB package installs successfully on Ubuntu 20.04, 22.04, 24.04
- **SC-002**: RPM package installs successfully on Fedora 38, 39, 40 and RHEL 9
- **SC-003**: Snap package installs and runs on Ubuntu with network permission
- **SC-004**: Docker image size ≤500MB compressed, starts in ≤5 seconds
- **SC-005**: Windows build compiles successfully with MSVC 2022, Npcap backend works
- **SC-006**: macOS build compiles successfully with Xcode 14+, libpcap backend works
- **SC-007**: Release workflow generates all artifacts (DEB, RPM, Snap, Docker, tarballs) in ≤30 minutes
- **SC-008**: All packages include complete documentation (markdown, HTML, man pages, examples)
- **SC-009**: Package installation via `apt`, `dnf`, `snap`, `docker` verified on 3+ systems each
- **SC-010**: CI/CD pipeline runs on every commit with Linux/Windows/macOS builds

## Assumptions

- Target Linux distributions: Ubuntu 20.04+, Debian 11+, Fedora 38+, RHEL 9+
- Docker Hub account available for image hosting
- GitHub Actions runners available for Linux, Windows, macOS
- GPG key available for package signing (optional)
- Focus on x86_64 architecture (ARM64 is future enhancement)

## Dependencies

- **External**: CPack (CMake packaging), Docker, GitHub Actions, Snapcraft
- **Internal**: All previous milestones (M0-M18)

## Out of Scope

- ARM64 / aarch64 builds (future enhancement)
- Windows installer (.msi) - focus on portable distribution
- macOS installer (.dmg) - focus on Homebrew formula (future)
- Package repository hosting (PPA, Copr) - users download from GitHub Releases
- Automatic update mechanism
- Package signing infrastructure (CI-based signing only)

## Implementation Notes

### Recommended Approach

**Phase 1 - CMake Packaging Setup** (1 week)
- Configure CPack for DEB and RPM
- Define package metadata
- Install targets (binaries, libraries, headers, docs)
- Tests: 5+ for package generation

**Phase 2 - Debian/RPM Packaging** (1 week)
- Create DEB build scripts
- Create RPM spec file
- Test on Ubuntu, Debian, Fedora, RHEL
- Tests: 10+ for package installation

**Phase 3 - Snap Packaging** (0.5 weeks)
- Create snapcraft.yaml
- Configure confinement and interfaces
- Test on Ubuntu
- Tests: 5+ for Snap installation

**Phase 4 - Docker Images** (1 week)
- Create multi-stage Dockerfile
- Optimize image size
- Publish to Docker Hub
- Tests: 8+ for Docker functionality

**Phase 5 - Cross-Platform Builds** (1.5 weeks)
- Windows build with MSVC + Npcap
- macOS build with Xcode + libpcap
- Update CMake for platform detection
- Tests: 12+ for cross-platform compatibility

**Phase 6 - Release Automation** (1 week)
- GitHub Actions release workflow
- Changelog generation
- Artifact upload to GitHub Releases
- Docker Hub publish
- Tests: 5+ for release workflow

**Phase 7 - Documentation Packaging** (0.5 weeks)
- Package markdown docs
- Generate and package Doxygen HTML
- Create man pages
- Bundle examples
- Tests: 5+ for doc installation

**Total Duration**: ~6.5 weeks

### CPack Configuration Example

```cmake
# CMakeLists.txt
include(CPack)

set(CPACK_PACKAGE_NAME "wadjet-link")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Automotive Ethernet validation framework")
set(CPACK_PACKAGE_VENDOR "Wadjet Project")
set(CPACK_PACKAGE_CONTACT "maintainer@wadjet.dev")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")

# Debian
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.31), libstdc++6 (>= 10), libpcap0.8 (>= 1.9)")
set(CPACK_DEBIAN_PACKAGE_SECTION "devel")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# RPM
set(CPACK_RPM_PACKAGE_LICENSE "Polyform Noncommercial 1.0.0")
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
set(CPACK_RPM_PACKAGE_REQUIRES "glibc >= 2.31, libstdc++ >= 10, libpcap >= 1.9")
```

### Dockerfile Example

```dockerfile
# Stage 1: Builder
FROM ubuntu:22.04 AS builder
RUN apt-get update && apt-get install -y cmake g++ libpcap-dev python3-dev
COPY . /src
WORKDIR /src
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel

# Stage 2: Runtime
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y libpcap0.8 && rm -rf /var/lib/apt/lists/*
COPY --from=builder /src/build/bin/* /usr/local/bin/
COPY --from=builder /src/build/lib/* /usr/local/lib/
RUN ldconfig
ENTRYPOINT ["wadjet"]
CMD ["--help"]
```

### GitHub Actions Release Workflow

```yaml
name: Release

on:
  push:
    tags:
      - 'v*.*.*'

jobs:
  build-packages:
    strategy:
      matrix:
        os: [ubuntu-22.04, windows-2022, macos-13]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - name: Build DEB (Linux only)
        if: matrix.os == 'ubuntu-22.04'
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Release
          cd build && cpack -G DEB
      - name: Upload artifacts
        uses: actions/upload-artifact@v4
        with:
          name: packages-${{ matrix.os }}
          path: build/*.deb
  
  publish-docker:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build and push Docker image
        run: |
          docker build -t wadjet/wadjet-link:latest .
          docker tag wadjet/wadjet-link:latest wadjet/wadjet-link:${{ github.ref_name }}
          docker push wadjet/wadjet-link --all-tags
```

### Package Installation Verification

```bash
# Debian/Ubuntu
sudo apt install ./wadjet-link_1.0.0_amd64.deb
wadjet --version
wadjet-run --help

# Fedora/RHEL
sudo dnf install ./wadjet-link-1.0.0-1.x86_64.rpm

# Snap
sudo snap install wadjet-link_1.0.0_amd64.snap --dangerous
wadjet-link.wadjet --version

# Docker
docker run --rm wadjet/wadjet-link:1.0.0 --version
docker run --rm -v $(pwd):/pcaps wadjet/wadjet-link:1.0.0 /pcaps/capture.pcap
```

### Test Count Target

**50+ tests** covering:
- Package generation: 5 tests
- DEB installation: 10 tests
- RPM installation: 10 tests
- Snap installation: 5 tests
- Docker functionality: 8 tests
- Cross-platform builds: 12 tests
- Release workflow: 5 tests
- Documentation packaging: 5 tests
