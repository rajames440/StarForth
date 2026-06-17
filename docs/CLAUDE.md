# CLAUDE.md — StarshipOS Documentation Root
# Instructions for Claude Code
# Location: docs/CLAUDE.md

## Mission

The documentation for StarshipOS/StarForth/LithosAnanke is scattered across
the repository. It exists in multiple forms, at multiple vintages, with no
consistent structure. The immediate goal is to triage, reorganize, and
establish a two-tier documentation system that can support patent review,
academic citation, commercial licensing, and hobbyist adoption.

Cash flow depends on this. Treat it accordingly.

---

## Two-Tier Documentation Model

```
docs/
├── CLAUDE.md          ← you are here
├── Makefile           ← builds both tiers
├── formal/            ← polished, versioned, citable (LaTeX → PDF)
│   └── CLAUDE.md      ← formal/ specific instructions
└── working/           ← living documents, design notes, active drafts
    └── CLAUDE.md      ← working/ specific instructions
```

### docs/formal/
Three-volume LaTeX set. Audience: patent counsel, SSRN reviewers, potential
licensees, paying customers, hobbyists with hardware in hand. Nothing goes
here until it is ready to be cited. See `formal/CLAUDE.md` for all authoring
conventions.

### docs/working/
Living documents. Design notes, DoE logs, architecture decision records,
experiment reports, draft specs, prose not yet promoted to formal. These are
NOT garbage — they are the source material that feeds formal/. The distinction
is audience and stability, not quality.

---

## Makefile Targets

```
make docs-formal     — build all three LaTeX volumes → PDFs in formal/build/
make docs-working    — render working/ markdown to HTML (optional, low priority)
make docs-index      — generate docs/INDEX.md inventory of all doc files
make docs-audit      — list files tagged OBSOLETE or SUPERSEDED in INDEX.md
make docs            — run docs-formal + docs-index
make docs-clean      — remove formal/build/ artifacts
```

---

## Phase 1: Triage (DO THIS FIRST)

Before writing, moving, or deleting anything, conduct a full inventory pass.

### Step 1 — Find everything

Locate every documentation file in the repository. Cast a wide net:

```bash
find . -type f \( \
  -name "*.md" \
  -o -name "*.txt" \
  -o -name "*.tex" \
  -o -name "*.rst" \
  -o -name "*.adoc" \
  -o -name "*.org" \
  -o -name "*.pdf" \
  -o -name "*.odt" \
  -o -name "*.docx" \
\) | sort > /tmp/doc-inventory-raw.txt
```

Also check for README files without extensions:
```bash
find . -name "README*" | sort >> /tmp/doc-inventory-raw.txt
```

### Step 2 — Read and classify each file

For every file found, read enough to assign it one of these status tags:

| Tag | Meaning |
|-----|---------|
| `CURRENT` | Accurate, up to date, reflects present architecture |
| `WORKING` | Active draft or living design document, still being edited |
| `HISTORICAL` | Accurate for its time, superseded but worth keeping as record |
| `SUPERSEDED` | Replaced by a newer document — note which one |
| `OBSOLETE` | Refers to architecture, naming, or design that no longer exists |
| `DELETE` | No value. Duplicates, temp files, auto-generated noise |

### Step 3 — Produce docs/INDEX.md

Create `docs/INDEX.md` with one entry per file:

```markdown
| Path | Topic | Status | Notes |
|------|-------|--------|-------|
| src/starforth/README.md | StarForth build instructions | CURRENT | Good candidate for Vol I Ch 3 |
| notes/old-ssm-design.md | Early SSM design | SUPERSEDED | Replaced by SSRN paper |
| scratch/ivmp-draft-v1.txt | IVMP protocol | HISTORICAL | Hermes spec v0.5.1 supersedes |
| tmp/foo.md | Unknown | DELETE | Empty file |
```

Do not delete anything during this pass. Tag only.

### Step 4 — Report to Bob

After producing INDEX.md, summarize:
- Total files found
- Count by status tag
- Top candidates for promotion into formal/ chapters
- Files recommended for deletion (DELETE tag) — list them explicitly and
  wait for confirmation before removing anything

**Never delete files without explicit confirmation from Bob.**

---

## Phase 2: Reorganize

Only begin Phase 2 after Bob has reviewed and approved the INDEX.md triage.

### Moving files into docs/working/

Files tagged `CURRENT` or `WORKING` that are not yet in `docs/working/`
should be moved there. Use this naming convention:

```
docs/working/
├── architecture/      — system design, capsule model, Uberkernel
├── experiments/       — DoE logs, run reports, raw data notes
├── specifications/    — IVMP, Jacquard, ACL, capsule specs
├── hardware/          — board bring-up notes, platform specifics
├── legal/             — patent-adjacent material (handle carefully)
├── papers/            — SSRN drafts, Math Companion, academic writing
└── scratch/           — genuinely informal notes, not yet categorized
```

When moving a file:
1. Move it with `git mv` to preserve history
2. Add a one-line header comment if the file format supports it:
   ```
   <!-- Moved to docs/working/architecture/ from [original path] — [date] -->
   ```
3. Update INDEX.md with the new path

### Files tagged HISTORICAL

Move to `docs/working/archive/` with a dated prefix:
```
docs/working/archive/2024-ssm-early-design.md
```

### Files tagged SUPERSEDED

Move to `docs/working/archive/` and add a header noting what supersedes it:
```
<!-- SUPERSEDED by docs/formal/common/starship.bib — citation james:2024:ssrn -->
```

### Files tagged DELETE

List them in a file called `docs/DELETE-CANDIDATES.md` and wait for Bob.

---

## Phase 3: Seed formal/

After working/ is organized, identify passages, sections, or entire documents
in working/ that are ready to be promoted into formal/ chapter stubs.

For each candidate:
1. Note the source file and section in a `%% SOURCE:` comment at the top of
   the chapter stub
2. Do not copy blindly — adapt to the formal/ voice conventions in
   `formal/CLAUDE.md`
3. Mark anything that needs Bob's dictation with:
   ```latex
   %% TODO(bob): [description of what's needed here]
   ```

The goal of Phase 3 is compilable stubs, not finished chapters. Every chapter
in all three volumes should exist as a .tex file that compiles without errors,
even if most of it is TODO comments.

---

## Ground Rules

- **Never delete without confirmation.** Tag as DELETE, list, wait.
- **Never rename Bob's technical terms.** James Law, SSM, Uberkernel,
  Hades, Jacquard Selector — these are proper nouns.
- **Never invent content.** If you don't have source material for a section,
  stub it and mark TODO.
- **git mv, not mv.** All file moves preserve history.
- **One commit per phase.** Triage in one commit, reorganize in another,
  seed formal/ in a third. Clean history matters.
- **INDEX.md is the source of truth** for what exists and where it lives.
  Keep it updated as files move.
- **Ask before touching legal/.** Anything patent-adjacent gets flagged
  for Bob's explicit instruction before moving or editing.
