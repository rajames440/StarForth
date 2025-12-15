# StarForth Formula Reference

This directory contains reusable LaTeX formula files for the StarForth adaptive runtime system.

## Files

- **james-law.tex** - James Law of Computational Dynamics with wave modulation
- **steady-state.tex** - Steady-state equilibrium formulas and memristive heat dynamics
- **window-scaling.tex** - Window scaling, resonance detection, and Fibonacci selection
- **invariants.tex** - System invariants, conservation laws, and fundamental constants
- **notation.tex** - Mathematical notation and symbol definitions

## Usage

Each formula file is **both standalone AND reusable**:

### Option 1: Compile Standalone

Generate individual PDF reference sheets:

```bash
pdflatex james-law.tex       # → james-law.pdf
pdflatex steady-state.tex    # → steady-state.pdf
pdflatex window-scaling.tex  # → window-scaling.pdf
pdflatex invariants.tex      # → invariants.pdf
pdflatex notation.tex        # → notation.pdf
```

### Option 2: Include in Your Document

Include any formula file in your own LaTeX document:

```latex
\documentclass{article}
\usepackage{amsmath,amssymb,amsthm,booktabs}

% Define flag to indicate formulas are being included
\newcommand{\formulasincluded}{}

\begin{document}

\section{James Law}
\input{formulas/james-law.tex}

\section{Notation}
\input{formulas/notation.tex}

\end{document}
```

**Important:** Always define `\formulasincluded` before including formula files to prevent document structure conflicts.

### Option 3: Combined Reference

See **test-combined.tex** for an example that includes all formula files in one document.

```bash
pdflatex test-combined.tex   # → test-combined.pdf (16 pages)
```

## Requirements

All formula files require these LaTeX packages:
- `amsmath` - Advanced math typesetting
- `amssymb` - Mathematical symbols
- `amsthm` - Theorem environments
- `booktabs` - Professional tables (for tables in invariants.tex)

## Technical Details

Each formula file uses conditional compilation via `\ifdefined\formulasincluded`:
- When `\formulasincluded` is **not** defined: file acts as standalone document
- When `\formulasincluded` **is** defined: file skips preamble and acts as includable content

This pattern allows the same file to serve both purposes without duplication.

## Contents Summary

| File | Equations | Pages | Key Topics |
|------|-----------|-------|------------|
| notation.tex | 0 | 3 | Symbol definitions, operators, conventions |
| james-law.tex | 11 | 2 | Wave modulation, resonance, validation metrics |
| steady-state.tex | 15 | 3 | Heat dynamics, equilibrium, entropy |
| window-scaling.tex | 25 | 3 | Logarithmic scaling, FFT analysis, Fibonacci windows |
| invariants.tex | 17 | 4 | Maxwell analogs, fundamental constants, conservation laws |
| **Total** | **68** | **15** | Comprehensive formula reference |

## License

See [../../LICENSE](../../LICENSE)
