# Paper #1: arXiv Submission

**Title:** Steady-State Convergence in a Self-Adaptive Virtual Machine: An Empirical Study

**Status:** Draft v0.1 (single-column, references canonical data)

## Overview

Conservative, empirical paper documenting steady-state behavior in the StarForth adaptive VM.

## Directory Structure

```
arxiv/
├── main.tex              # Entry point (single-column)
├── abstract.tex          # 169 words
├── intro.tex - conclusion.tex  # 10 main sections
├── appendix.tex          # Consolidated appendices
├── references.bib        # 15 citations
└── figures/              # (empty - will copy from ../../LaTeX/figures/ at submission)
```

**References canonical data:**
- Tables: `\input{../../LaTeX/tables/*.tex}` (direct reference)
- Figures: Will reference `figures/*.pdf` (copy at submission time)
- Formulas: Embedded in appendix.tex

## Building (Development)

```bash
cd papers/entities/arxiv/
pdflatex main.tex
bibtex main
pdflatex main.tex
pdflatex main.tex
```

✓ **Builds successfully** - 314 KB, 25 pages, single-column

## Canonical Data Sources

```
papers/LaTeX/
├── figures/              # 100+ SVG figures (from R scripts)
├── tables/               # LaTeX tables (from xtable)
│   ├── doe300_main_effects.tex  ← referenced directly
│   ├── doe300_top_configs.tex   ← referenced directly
│   └── ...
├── formulas/             # Math formulas
└── scripts/              # R scripts to regenerate
    ├── regenerate_all_svgs.R
    └── regenerate_all_tables.R
```

## Workflow

### Development (current)
- Edit `.tex` files in `papers/entities/arxiv/`
- Tables referenced from `../../LaTeX/tables/` (no copies)
- Build with relative paths (works fine)

### arXiv Submission (when ready)
Run submission script to create self-contained archive:

```bash
cd papers/entities/arxiv/
./prepare_submission.sh  # Will create this script
```

Script will:
1. Copy canonical tables to local directory
2. Fix LaTeX escaping (`_` → `\_`, `%` → `\%`)
3. Convert figures: `../../LaTeX/figures/*.svg` → `figures/*.pdf`
4. Create `arxiv-submission.tar.gz`

## Known Issues

**Canonical tables need LaTeX escaping:**
- `OFF_mean` should be `OFF\_mean`
- `Effect %` should be `Effect \%`

These should be fixed in `papers/LaTeX/scripts/regenerate_all_tables.R` to output escaped characters. For now, the submission script will handle this.

## Paper Statistics

- **Format:** Single-column (more readable)
- **Pages:** 25
- **Size:** 314 KB
- **Sections:** 10 main + 1 appendix
- **Tables:** 2 (referenced from canonical source)
- **Figures:** 6 (to be added)
- **References:** 15 citations

## Next Steps

1. ✓ Single-column layout
2. ✓ Reference canonical tables (no redundant copies)
3. ⏳ Create `prepare_submission.sh` script
4. ⏳ Generate figures from canonical data
5. ⏳ Test full submission workflow
6. ⏳ Submit to arXiv

## License

StarForth License v1.0

## Contact

Robert A. James - star.4th@proton.me
