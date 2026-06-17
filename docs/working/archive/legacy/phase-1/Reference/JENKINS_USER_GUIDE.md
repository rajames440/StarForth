<!-- Moved from docs/archive/phase-1/Reference/JENKINS_USER_GUIDE.md to docs/working/archive/legacy/phase-1/Reference/JENKINS_USER_GUIDE.md on 2026-06-16 (docs reorg Phase 2) -->
# Jenkins User Guide

## Nightly Pipeline Controls

- Entry point: `jenkinsfiles/nightly/Jenkinsfile`.
- Every folder under `jenkinsfiles/nightly/<arch>/<target>/` now has a controller `Jenkinsfile`; the root pipeline loads
  only the combos whose `RUN_*` boolean is selected.
- Defaults keep the “fastest” profiles for `amd64`, `arm64`, `raspi`, and `riscv64`; all other profiles start disabled
  but can be toggled on per build.
- To lock configurations (no UI changes): remove the `parameters(...)` block in the nightly Jenkinsfile and rerun once
  to persist Jenkins job properties.
- Each per-combo file returns a `stages` map. Flip those booleans to add smoke/tests/benchmarks for that profile without
  touching the root pipeline.
