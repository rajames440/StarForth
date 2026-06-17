# docs/working/ — Living Documentation

Source material for the formal documentation tier. Design notes, DoE logs,
architecture decisions, experiment reports, draft specs — everything not yet
promoted to `docs/formal/`. See `docs/CLAUDE.md` for the two-tier model.

## Layout

| Directory | Contents |
|-----------|----------|
| `architecture/` | System design: physics engine, HAL, heartbeat, pipelining, word-ACL, adaptive systems, getting-started. (`03-architecture/`, `getting-started/`, `heartbeat_csv_export.md`) |
| `experiments/` | DoE designs and run data. (`02-experiments/`, `campaigns/` = former top-level `experiments/`) |
| `specifications/` | Protocol/format specs. *(to be populated as specs are extracted from architecture/scratch)* |
| `hardware/` | Board bring-up, platform/perf notes. *(to be populated from scratch/src/{performance-profiling,platform-integration})* |
| `legal/` | Patent-adjacent material — **HOLD**. See `legal/README.md`. Nothing moved here without Bob's instruction. |
| `papers/` | Academic / citable writing: claim tables, anti-claims, null hypothesis, reproducibility, ontology (markdown), SSRN companion, FINAL_REPORT, published SSRN PDF (`published/`). |
| `scratch/` | Uncategorized working material: governance/meta docs, the full `docs/src/` developer-reference tree, asset holding area. |
| `archive/` | Historical / superseded / obsolete records: quality audits, operations, research drafts, session logs, the former `docs/archive/` tree (`legacy/`), artifacts. |

## Phase 2 reorganization notes (2026-06-16)

- Moves used `git mv` (history preserved). Each moved `.md`/`.adoc`/`.tex` file
  carries a one-line provenance header noting its original path.
- Reorg used **directory-level** consolidation rather than splitting every
  mixed-tag directory file-by-file, to keep companion data (logs, CSVs,
  randomization matrices) with the reports they belong to. Per-file status tags
  remain authoritative in `docs/INDEX.md`.
- `docs/src/` landed under `scratch/src/` as a holding area — it is mixed
  developer reference that still needs semantic re-filing into
  `architecture/`, `hardware/`, and `specifications/`.
- Repo-root convention files (`README.md`, `ROADMAP.md`, `ONTOLOGY.md`,
  `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `NOTICE.md`) and the docs landing
  files (`docs/CLAUDE.md`, `docs/INDEX.md`, `docs/README.md`) were left in place.
- DELETE candidates: see `docs/DELETE-CANDIDATES.md` (nothing deleted).
