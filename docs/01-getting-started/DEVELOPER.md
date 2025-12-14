# StarForth Developer Guide

## Development Environment Setup

### Required Tools

#### Core Build Tools

```bash
sudo apt-get update
sudo apt-get install build-essential gcc make git
```

#### Optional Tools

- **Doxygen** - For API documentation generation
- **syft** - For SBOM generation
- **fpm** - For package building (DEB/RPM)

```bash
# Doxygen
sudo apt-get install doxygen

# syft
wget https://github.com/anchore/syft/releases/latest/download/syft_linux_amd64.tar.gz
tar -xzf syft_linux_amd64.tar.gz
sudo mv syft /usr/local/bin/

# fpm (for packaging)
sudo apt-get install ruby-dev build-essential
sudo gem install fpm
```

### Isabelle/HOL Installation

**Isabelle is required for formal verification documentation generation.**

#### Download & Install

1. **Download Isabelle2025** from https://isabelle.in.tum.de/

```bash
# Example installation to ~/bin/
cd ~/bin
wget https://isabelle.in.tum.de/dist/Isabelle2025_linux.tar.gz
tar -xzf Isabelle2025_linux.tar.gz
```

2. **Add Isabelle to your PATH** in `~/.bashrc`:

```bash
# Add Isabelle2025 binaries to PATH
export PATH="$PATH:$HOME/bin/Isabelle2025/bin"
```

3. **Reload your shell**:

```bash
source ~/.bashrc
```

4. **Verify installation**:

```bash
isabelle version
# Should output: Isabelle2025
```

#### Project Integration

The StarForth build system expects `isabelle` to be available on the `PATH`. The Makefile will automatically detect it:

```makefile
# From Makefile line 24:
ISABELLE ?= isabelle
```

**Do NOT commit Isabelle to the repository.** It's excluded via `.gitignore`:

```
/tools/Isabelle2025/
```

### Isabelle Make Targets

Once Isabelle is installed, you can use these targets:

```bash
# Verify all formal theories
make isabelle-build

# Quick syntax check (faster)
make isabelle-check

# Generate audit-ready documentation
make docs-isabelle
```

## CI/CD Considerations

### GitHub Actions / GitLab CI

For CI/CD pipelines, you'll need to install Isabelle as part of the build process:

#### GitHub Actions Example

```yaml
name: Build & Verify

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install build dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y build-essential gcc make

      - name: Install Isabelle
        run: |
          wget -q https://isabelle.in.tum.de/dist/Isabelle2025_linux.tar.gz
          tar -xzf Isabelle2025_linux.tar.gz
          echo "$GITHUB_WORKSPACE/Isabelle2025/bin" >> $GITHUB_PATH

      - name: Verify Isabelle installation
        run: isabelle version

      - name: Build StarForth
        run: make fastest

      - name: Run tests
        run: make test

      - name: Build formal verification docs
        run: make docs-isabelle

      - name: Upload documentation artifacts
        uses: actions/upload-artifact@v3
        with:
          name: isabelle-docs
          path: docs/src/isabelle/
```

#### GitLab CI Example

```yaml
variables:
  ISABELLE_VERSION: "2025"
  ISABELLE_URL: "https://isabelle.in.tum.de/dist/Isabelle2025_linux.tar.gz"

before_script:
  - apt-get update -qq
  - apt-get install -y -qq build-essential gcc make wget
  - wget -q $ISABELLE_URL
  - tar -xzf Isabelle2025_linux.tar.gz
  - export PATH="$PATH:$PWD/Isabelle2025/bin"
  - isabelle version

build:
  stage: build
  script:
    - make fastest
    - make test

docs:
  stage: deploy
  script:
    - make docs-isabelle
  artifacts:
    paths:
      - docs/src/isabelle/
```

### Docker Considerations

If building in Docker, Isabelle requires **Java** (JDK 17+):

```dockerfile
FROM ubuntu:22.04

# Install build tools
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    make \
    wget \
    openjdk-17-jdk \
    && rm -rf /var/lib/apt/lists/*

# Install Isabelle
RUN wget -q https://isabelle.in.tum.de/dist/Isabelle2025_linux.tar.gz && \
    tar -xzf Isabelle2025_linux.tar.gz && \
    rm Isabelle2025_linux.tar.gz

ENV PATH="/Isabelle2025/bin:${PATH}"

# Verify
RUN isabelle version
```

### CI/CD Performance Notes

- **Isabelle build takes 5-10 minutes** for all theory verification
- Consider **caching Isabelle installation** between builds
- Use `isabelle-check` for quick syntax checking in pre-commit hooks
- Only run full `isabelle-build` on main branch or release tags

#### GitHub Actions Caching Example

```yaml
- name: Cache Isabelle
  uses: actions/cache@v3
  with:
    path: Isabelle2025
    key: isabelle-2025-${{ runner.os }}

- name: Install Isabelle (if not cached)
  if: steps.cache-isabelle.outputs.cache-hit != 'true'
  run: |
    wget -q https://isabelle.in.tum.de/dist/Isabelle2025_linux.tar.gz
    tar -xzf Isabelle2025_linux.tar.gz
```

## Quick Start

```bash
# Clone the repository
git clone <repo-url>
cd StarForth

# Install dependencies (including Isabelle - see above)
# ...

# Build the fastest version
make fastest

# Run tests
make test

# Generate documentation (requires Isabelle)
make docs-isabelle

# View generated docs
ls -la docs/src/isabelle/
```

## Project Structure

```
StarForth/
├── src/                          # C source code
├── include/                      # Header files
├── docs/
│   ├── src/
│   │   ├── internal/formal/     # Isabelle theory files
│   │   └── isabelle/            # Generated Isabelle docs (gitignored)
│   └── DEVELOPER.md             # This file
├── scripts/
│   └── isabelle-to-adoc.sh      # Isabelle doc generator
├── build/                        # Build artifacts (gitignored)
├── tools/
│   └── Isabelle2025/            # Local Isabelle (gitignored)
└── Makefile                      # Build system
```

## Formal Verification

StarForth includes machine-checked correctness proofs using **Isabelle/HOL**:

- **Location**: `docs/src/internal/formal/*.thy`
- **Sessions**: `VM_Formal`, `Physics_Formal`
- **Generated docs**: `docs/src/isabelle/` (after `make docs-isabelle`)

### Theory Files

- `VM_Core.thy` - Core VM semantics
- `VM_Stacks.thy` - Stack operations
- `VM_StackRuntime.thy` - Runtime stack management
- `VM_Words.thy` - Word definitions
- `VM_DataStack_Words.thy` - Data stack operations
- `VM_ReturnStack_Words.thy` - Return stack operations
- `VM_Register.thy` - Register operations
- `Physics_StateMachine.thy` - Physics state machine
- `Physics_Observation.thy` - Observation model

All theories are **fully verified** with machine-checked proofs.

## Contributing

When contributing code that affects the VM semantics:

1. Update relevant Isabelle theories if needed
2. Run `make isabelle-build` to verify proofs
3. Run `make docs-isabelle` to regenerate documentation
4. Include Isabelle docs in your pull request

## Support

- **Issues**: GitHub Issues
- **Discussions**: GitHub Discussions
- **Isabelle Help**: https://isabelle.in.tum.de/
- **Formal Verification**: See `docs/src/isabelle/VERIFICATION_REPORT.adoc`

---

**Last Updated**: 2025-10-30
**Isabelle Version**: 2025
**Build System**: GNU Make