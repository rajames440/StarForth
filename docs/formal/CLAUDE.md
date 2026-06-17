# docs/formal/CLAUDE.md — Authoring Conventions for the Formal Documentation Tier

> **Draft** — generated 2026-06-16, starter for Bob's review.
> Override any rule below with explicit instruction.

---

## Purpose

`docs/formal/` is the three-volume LaTeX documentation set for StarshipOS /
StarForth / LithosAnanke. Audience: patent counsel, SSRN reviewers, potential
licensees, paying customers, hobbyist hackers with hardware in hand. Nothing
goes here until it is ready to be cited.

---

## Scraps System

`docs/formal/scraps/` contains press-ready LaTeX fragments converted from
every document in `docs/working/`. Each scrap is a self-contained `.tex`
fragment — no `\documentclass`, no `\begin{document}` — ready to `\input{}`
into any publication.

### Scrap file naming

Mirror the source path under `scraps/`:
```
docs/working/architecture/build-and-tooling/BUILD_OPTIONS.adoc
→ docs/formal/scraps/architecture/build-and-tooling/BUILD_OPTIONS.tex
```

### Scrap header (required on every file)

```latex
%% SCRAP: architecture/build-and-tooling/BUILD_OPTIONS
%% SOURCE: docs/working/architecture/build-and-tooling/BUILD_OPTIONS.adoc
%% STATUS: working-triage (CURRENT|WORKING|HISTORICAL|SUPERSEDED|OBSOLETE)
%% FITS: dev-guide/ch-build, cookbook/appendix-flags
%% EDITORIAL: lifted — prose rewritten to press voice
```

`FITS:` lists candidate `\input{}` locations across the three publications.
Use `none` if the scrap is archive/historical material kept for completeness.

### Scrap prose conventions (editorial lift)

- Rewrite passive and wordy constructions to active, tight prose.
- Strip filler phrases ("it is important to note that", "as mentioned above").
- Convert bullet lists to `\begin{itemize}` or prose where a list is lazy.
- Code blocks → `\begin{lstlisting}[language=bash]` or `[language=C]`.
- Tables → `\begin{tabular}` with `booktabs` rules.
- All claims checked against `ANTI_CLAIMS.md`; hedged language preserved.
- `%% TODO(bob):` for anything that requires dictation or verification.
- `%% PATENT:` flag on any section touching patent-adjacent claims.

### Three publications

| Directory | Title | Audience |
|-----------|-------|----------|
| `dev-guide/` | StarForth Developer Guide | Contributors, embedders, kernel hackers |
| `user-guide/` | StarForth User Guide | End users running StarForth interactively |
| `cookbook/` | The StarForth Cookbook | FORTH programmers wanting patterns and recipes |

Each publication has its own `main.tex` that `\input{}`s scraps in chapter order.



| Volume | Scope |
|--------|-------|
| **Vol I — StarForth VM and Physics Runtime** | FORTH-79 interpreter, physics-driven adaptive runtime (7 feedback loops), Isabelle/HOL formal verification, 90-run experimental results. |
| **Vol II — LithosAnanke Kernel and Capsule System** | Bare-metal UEFI kernel (M0–M7), capsule birth protocol, word-level ACL, platform support, L8 Jacquard Mode Selector. |
| **Vol III — Research Reference** | SSRN paper transcript, Mathematical Companion, formal claim tables, reproducibility protocol, roadmap. |

---

## Voice and Tone

- **Technical precision over enthusiasm.** Never write "cutting-edge," "revolutionary,"
  or "state-of-the-art." Let results speak.
- **Third person throughout.** Not "we did X" — "the system does X" or "the implementation
  uses X."
- **Calibrated claims only.** If a claim appears in `ANTI_CLAIMS.md` as out-of-bounds,
  it does not appear here. See `docs/working/papers/ANTI_CLAIMS.md`.
- **Thermodynamic metaphors are modeling tools, not physics claims.** Always introduce
  the domain of use: "using execution frequency as a proxy for thermal energy, …"
  See `ONTOLOGY.md` for canonical term definitions.
- **Proper nouns are proper.** Never rename: James Law, SSM (Steady-State Machine),
  Jacquard Selector, Uberkernel, Hades. These are registered terms.

---

## LaTeX Conventions

### Preamble

All volumes `\input{../common/preamble}` and `\bibliography{../common/starship}`.
Do not duplicate preamble content across volumes.

### Source and TODO markers

Every chapter stub must carry at the top:

```latex
%% SOURCE: <path to working/ source material>
%% TODO(bob): [description of what's needed — dictate or write]
```

Do not remove `%% SOURCE:` comments when a chapter is filled in — they serve as
provenance for later reviewers.

### Fonts and formatting

- Body text: Computer Modern (default), 11pt, `a4paper`, `twoside`.
- Code listings: `listings` package, `basicstyle=\ttfamily\small`.
- Math: `amsmath`, `amssymb`. Q48.16 fixed-point notation: `\mathbb{Q}_{48.16}`.
- Figures: `\includegraphics[width=\linewidth]{...}`. All figures in `common/figures/`
  or in the volume directory's `figures/` subfolder.
- Tables: `booktabs` package. `\toprule` / `\midrule` / `\bottomrule` — no vertical rules.

### Cross-volume references

Use `\label{vol:N:chap:slug}` convention, e.g. `\label{vol1:chap:physics}`.
Do not reference across volumes via `\ref` in production builds; link by title instead.

### Citations

All citations go through `common/starship.bib`. Use BibTeX keys in format:
- `james:2025:ssrn` — published paper
- `james:2025:patent` — patent application
- `james:2025:proof:loop1` — Isabelle/HOL theory file

---

## Promotion Criteria

A section of `docs/working/` is ready for formal inclusion when:

1. The underlying code or claim is confirmed in `.claude/CLAUDE.md` or the running codebase.
2. It has been reviewed against `ANTI_CLAIMS.md` and `ACADEMIC_WORDING_GUIDELINES.md`.
3. There is a `%% SOURCE:` pointer to the working/ document it came from.
4. It compiles without errors and warnings.

A stub with `%% TODO(bob):` markers **does satisfy #4** — compilable stubs are the
goal of Phase 3, not finished chapters.

---

## What NOT to do

- **Never invent results.** If a number isn't in a source doc or the published paper, stub it: `%% TODO(bob): cite figure here`.
- **Never invent proofs.** If a theorem isn't in `proof/*.thy`, mark it TODO.
- **Never draft patent-adjacent text** (independent/dependent claims, claim mapping) without Bob's explicit dictation. Leave those sections as `%% TODO(bob): PATENT — do not draft`.
- **Never copy-paste from `COMPUTATIONAL_PHYSICS_FRAMEWORK.md`.** That doc is HISTORICAL and contains out-of-scope claims that contradict `ANTI_CLAIMS.md`.
