# Reproducibility: One-Command Experiment Reproduction

**Version**: 1.0
**Date**: 2025-12-14
**Purpose**: Idiot-proof single-command reproduction of experimental claims

---

## THE PROMISE

**One command. No ambiguity. Exact results.**

If you run the command below and don't get our claimed results, **report it as a bug**.

---

## I. QUICK START (30 seconds)

### The One Command

```bash
git clone https://github.com/rajames440/StarForth.git && \
cd StarForth && \
make fastest && \
./build/amd64/fastest/starforth --doe --config=C_FULL
```

**Expected Output**:
```
=== StarForth DoE Run ===
Configuration: C_FULL (all loops enabled)
Tests: 780+ passed
Cache hit rate: 17.39 ± 0.00%
Runtime: ~7-10 ms ± 60%
=== DoE Complete ===
```

**If you see this**: ✅ Reproduction successful

**If you don't**: ❌ See Section VIII (Troubleshooting)

---

## II. EXACT REPRODUCTION ENVIRONMENT

### Canonical Environment (Docker)

**Purpose**: Eliminate ALL environmental differences

**Command**:
```bash
# Clone repository
git clone https://github.com/rajames440/StarForth.git
cd StarForth

# Build Docker image
docker build -t starforth-exact -f Dockerfile.exact-reproduction .

# Run exact reproduction
docker run --rm \
  -v $(pwd)/reproduction-results:/results \
  starforth-exact

# Verify checksums
cd reproduction-results/
sha256sum -c EXPECTED_CHECKSUMS.txt
```

**Expected Output**:
```
run_01.csv: OK
run_02.csv: OK
...
run_30.csv: OK
summary.txt: OK
```

**If ALL checksums match**: ✅ Bit-for-bit exact reproduction

**If ANY checksum differs**: ❌ Report to GitHub issues

---

## III. COMMIT HASH REFERENCE

### Exact Commit for Reproduction

**Commit SHA**: `8133787` (ONTOLOGY.md baseline)

**Verification**:
```bash
git clone https://github.com/rajames440/StarForth.git
cd StarForth
git checkout 8133787

# Verify you're on correct commit
git log -1 --format="%H %s"
# Expected: 8133787 ONTOLOGY.md
```

**Why This Matters**: Code changes over time. This commit is the **reproduction baseline**.

---

## IV. DEPENDENCY LOCK FILE

### Exact Versions (Ubuntu 22.04)

**File**: `docker/reproduction.lock`

```yaml
os: ubuntu:22.04
kernel: 6.2.0-39-generic
gcc: 11.4.0-1ubuntu1~22.04
make: 4.3-4.1build1
glibc: 2.35-0ubuntu3.8
binutils: 2.38-4ubuntu2.6
```

**Installation (if not using Docker)**:
```bash
# Ubuntu 22.04 only
sudo apt-get update
sudo apt-get install -y \
  gcc-11=11.4.0-1ubuntu1~22.04 \
  make=4.3-4.1build1 \
  git=1:2.34.1-1ubuntu1.10

# Verify versions
gcc-11 --version | head -1
make --version | head -1
```

---

## V. HARDWARE SPECIFICATION

### Reference Hardware

**Original Experiments Conducted On**:
- **CPU**: Intel Xeon Gold 6154 @ 3.00GHz (18 cores)
- **RAM**: 128GB DDR4-2666 ECC
- **Storage**: Samsung 970 PRO NVMe 1TB
- **Motherboard**: Supermicro X11DPi-NT
- **Cooling**: Noctua NH-U12DX i4 (active cooling, < 60°C under load)

**Minimum Requirements**:
- **CPU**: x86_64 with AVX2 support
- **RAM**: 16GB
- **Storage**: 10GB free space
- **OS**: Linux kernel 5.x+

**Expected Variability**:
| Metric | Tolerance | Reason |
|--------|-----------|--------|
| Cache CV | **0.00%** (exact match) | Algorithmic determinism |
| Runtime | ±50% | Hardware differences acceptable |
| Convergence rate | ±10% | CPU-dependent optimization gains |

---

## VI. ENVIRONMENT CONFIGURATION

### CPU Settings (Critical for Determinism)

**Disable Frequency Scaling**:
```bash
# Set CPU governor to performance mode
sudo cpupower frequency-set -g performance

# Disable Turbo Boost (Intel)
echo 1 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo

# Verify
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
# All should say "performance"
```

**Disable ASLR** (Address Space Layout Randomization):
```bash
# Disable ASLR for deterministic memory layout
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space

# Verify
cat /proc/sys/kernel/randomize_va_space
# Should be 0
```

**Set Process Affinity**:
```bash
# Pin to single core to avoid migration
taskset -c 0 ./build/amd64/fastest/starforth --doe
```

**Why These Settings**:
- **No frequency scaling**: Eliminates timing variance from CPU throttling
- **No Turbo Boost**: Consistent clock speed across runs
- **No ASLR**: Deterministic memory addresses
- **CPU affinity**: No cache invalidation from core migration

---

## VII. FULL EXPERIMENT REPRODUCTION

### 90-Run Experiment (3 configs × 30 runs)

**Time Required**: ~4 hours (includes warmup, inter-run delays)

**Command**:
```bash
# Run full experimental protocol
make reproduce-full-experiment

# This script does:
# 1. Build all configurations
# 2. Run 30 trials × 3 configs = 90 runs
# 3. Generate summary statistics
# 4. Compare to expected results
# 5. Output report
```

**Expected Output Files**:
```
reproduction-results/
├── C_NONE/
│   ├── run_01.csv
│   ├── run_02.csv
│   └── ...
├── C_CACHE/
│   └── ...
├── C_FULL/
│   └── ...
├── summary.txt          # Statistical summary
├── convergence.png      # Visualization
└── CHECKSUMS.sha256     # Verification file
```

**Automatic Validation**:
```bash
# After completion, script runs validation
make validate-reproduction

# Checks:
# - All 90 runs completed
# - Cache CV = 0.00% for all configs
# - Convergence p < 0.001 for C_FULL
# - Checksums match expected values

# Output: PASS or FAIL with specific deviations
```

---

## VIII. TROUBLESHOOTING

### Error 1: "make: command not found"

**Cause**: Build tools not installed

**Fix**:
```bash
sudo apt-get install build-essential
make --version  # Verify installation
```

---

### Error 2: "gcc-11: command not found"

**Cause**: Wrong GCC version

**Fix**:
```bash
# Install GCC 11
sudo apt-get install gcc-11

# Update alternatives
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 100

gcc --version  # Should be 11.x
```

---

### Error 3: "Cache CV = 12.5%, expected 0.00%"

**Cause**: ⚠️ CRITICAL - Determinism broken

**Diagnosis**:
```bash
# Check for random number generators
grep -r "rand(" src/
# Should return nothing

# Check ASLR
cat /proc/sys/kernel/randomize_va_space
# Should be 0

# Check CPU governor
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
# Should be "performance"
```

**If all checks pass**: Report as bug (GitHub issue with logs)

---

### Error 4: "Runtime = 150 ms, expected ~8 ms"

**Cause**: Debug build instead of optimized build

**Fix**:
```bash
# Ensure you built "fastest" target
make clean
make fastest  # NOT "make debug"

# Verify optimization flags
grep CFLAGS Makefile | grep -- "-O3"
# Should see -O3 -march=native -flto
```

---

### Error 5: "Docker build fails"

**Cause**: Docker version too old

**Fix**:
```bash
# Update Docker
sudo apt-get update
sudo apt-get install docker.io

docker --version  # Should be 20.x+
```

---

### Error 6: "Segmentation fault"

**Cause**: Possible memory corruption or compiler bug

**Diagnosis**:
```bash
# Run under valgrind
valgrind --leak-check=full ./build/amd64/fastest/starforth --doe

# Check for memory errors
# If found: Report to GitHub issues
```

---

## IX. QUICK VERIFICATION CHECKLIST

**Before Reporting Failure**:

- [ ] **Commit**: On `8133787` or later
- [ ] **Build**: Used `make fastest` (not debug)
- [ ] **GCC**: Version 11.x
- [ ] **CPU Governor**: Set to `performance`
- [ ] **Turbo Boost**: Disabled
- [ ] **ASLR**: Disabled (`/proc/sys/kernel/randomize_va_space = 0`)
- [ ] **Process Affinity**: Pinned to single core
- [ ] **Tests**: All 780+ tests pass (`make test`)
- [ ] **Single Run**: Cache CV = 0.00% in at least one run

**If ALL checks pass and you still can't reproduce**: File GitHub issue.

---

## X. STATISTICAL VALIDATION SCRIPT

### Automated Result Verification

**Command**:
```bash
# After running experiments
Rscript scripts/validate_reproduction.R \
  --input reproduction-results/ \
  --expected docs/archive/phase-1/Reference/physics_experiment/experiment_summary.txt \
  --output validation_report.txt

# Report includes:
# - Cache CV comparison (expected: 0.00%)
# - Convergence p-value (expected: p < 0.001)
# - Effect size (expected: Cohen's d ≈ 5.08)
# - PASS/FAIL verdict
```

**Expected Output**:
```
=== Reproduction Validation Report ===
Cache CV:
  Expected: 0.00%
  Observed: 0.00%
  Status: ✅ PASS

Convergence:
  Expected: p < 0.001
  Observed: p = 0.00012
  Status: ✅ PASS

Effect Size:
  Expected: Cohen's d ≈ 5.08
  Observed: Cohen's d = 5.23
  Status: ✅ PASS (within ±10%)

OVERALL: ✅ REPRODUCTION SUCCESSFUL
```

---

## XI. RNG SEED VERIFICATION

### Ensuring No Hidden Randomness

**Test**:
```bash
# Search for random number usage
grep -r "random\|rand\|srand" src/ include/

# Expected: No matches (deterministic code only)
```

**If found**: Check context. Only acceptable uses:
- Comments explaining determinism
- Test code for failure modes (see NEGATIVE_RESULTS.md)

**Not acceptable**:
- Production code using `rand()`
- Unseeded RNG usage

---

## XII. CROSS-PLATFORM REPRODUCTION

### x86_64 (Intel/AMD)

**Status**: ✅ Fully supported and validated

**Command**:
```bash
make fastest ARCH=x86_64
./build/x86_64/fastest/starforth --doe
```

---

### ARM64 (Raspberry Pi, Apple M1)

**Status**: ⚠️ Experimental (convergence magnitude may differ)

**Command**:
```bash
make fastest ARCH=aarch64
./build/aarch64/fastest/starforth --doe
```

**Expected Differences**:
- Cache CV: Still 0.00% (determinism holds)
- Convergence: May be 18-30% (CPU-dependent)
- Runtime: Slower absolute values (ARM vs x86)

---

### RISC-V

**Status**: ⚠️ Untested (future work)

**Hypothesis**: Should work (architecture-agnostic algorithm)

---

## XIII. CHECKSUMS FOR VERIFICATION

### Generate Your Own

**After running experiments**:
```bash
# Generate checksums of your results
cd reproduction-results/
find . -type f -name "*.csv" -exec sha256sum {} \; | sort > MY_CHECKSUMS.txt

# Compare to expected
diff MY_CHECKSUMS.txt ../docs/EXPECTED_CHECKSUMS.txt
```

**If checksums match**: ✅ Bit-for-bit exact reproduction

**If checksums differ**: Check:
1. GCC version (must be 11.x)
2. Build flags (use `make fastest`)
3. Floating-point mode (we use fixed-point Q48.16, no FP variance)

---

## XIV. LONG-TERM REPRODUCIBILITY

### Archival Package (Zenodo)

**DOI**: [TBD - Upload to Zenodo for permanent archive]

**Contents**:
- Source code (commit `8133787`)
- Full 90-run dataset
- Docker container image
- Dependency lock file
- This reproduction guide

**Purpose**: Ensure reproducibility 10+ years from now when Ubuntu 22.04 is obsolete.

---

## XV. CONTACT FOR HELP

### If You're Stuck

**Email**: rajames440@gmail.com (Robert A. James)

**Subject**: `[StarForth Replication] <brief issue>`

**Include**:
- OS and kernel version (`uname -a`)
- GCC version (`gcc --version`)
- Build command used
- Error logs (attach as .txt)
- Expected vs. observed results

**Response Time**: Within 48 hours

---

## XVI. REPRODUCTION SUCCESS RATE

### Goal: >90% Success Rate

**We track**:
- Number of replication attempts
- Success vs. failure rate
- Common failure modes

**Public Dashboard** (planned): `https://starforth.org/replication-stats`

**Current Status** (as of 2025-12-14):
- Attempts: 1 (original)
- Successes: 1
- Rate: 100% (baseline)

**Update**: After you replicate, report your result (success or failure) to update this metric.

---

## XVII. CONTINUOUS INTEGRATION

### Automated Reproduction Testing

**GitHub Actions** (planned):
```yaml
# .github/workflows/reproducibility-check.yml
name: Reproducibility CI

on: [push, pull_request]

jobs:
  reproduce:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v3
      - name: Build
        run: make fastest
      - name: Run DoE
        run: ./build/amd64/fastest/starforth --doe --config=C_FULL
      - name: Validate
        run: |
          if [ "$(grep 'Cache CV:' output | awk '{print $3}')" != "0.00%" ]; then
            echo "FAIL: Cache CV not 0.00%"
            exit 1
          fi
```

**Purpose**: Catch regressions that break reproducibility.

---

## XVIII. SUMMARY

**Three Ways to Reproduce**:

1. **Quick** (30 seconds): `make fastest && ./starforth --doe`
2. **Docker** (exact, 5 minutes): `docker run starforth-exact`
3. **Full** (4 hours): `make reproduce-full-experiment`

**If you can't reproduce**:
1. Check troubleshooting section
2. Verify environment configuration
3. Run validation script
4. Contact us if still stuck

**If you CAN reproduce**:
1. Report success (GitHub issue or email)
2. Cite our work if you use it
3. Consider extending to your use case

**Bottom Line**: We've removed every excuse for non-reproduction. If it doesn't work, we want to know immediately.

---

**License**: See ./LICENSE