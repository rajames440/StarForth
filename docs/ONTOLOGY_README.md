# StarForth Ontology Documentation

This directory contains formal ontology, taxonomy, and lexicon materials for academic publications and documentation.

## Files

### 1. `ONTOLOGY.md` (Markdown)
**Purpose**: Master reference document with comprehensive definitions
**Use for**:
- Internal reference
- GitHub documentation
- Quick lookups

**Sections**:
- I. Ontology (Conceptual Framework)
- II. Taxonomy (Hierarchical Classification)
- III. Lexicon (17 Precise Definitions)
- IV. Mathematical Formalism
- V-VI. Diagrams & Commitments
- VII. Usage Guidelines

---

### 2. `ontology.tex` (LaTeX)
**Purpose**: Academic paper inclusion
**Use for**:
- Journal submissions
- Conference papers
- Thesis/dissertation appendices

**To compile**:
```bash
cd docs/
pdflatex ontology.tex
bibtex ontology
pdflatex ontology.tex
pdflatex ontology.tex
```

**Output**: `ontology.pdf` (self-contained reference)

**To include in your paper**:
```latex
% In your main paper
\input{ontology.tex}  % Include full ontology

% OR just the lexicon section
\section{Terminology}
\input{ontology_lexicon.tex}  % Extract section 2 only
```

---

### 3. `appendix_glossary.adoc` (AsciiDoc)
**Purpose**: Book/report appendix
**Location**: `FINAL_REPORT/appendix_glossary.adoc`
**Use for**:
- Book compilation
- Multi-chapter reports
- HTML documentation

**To include in book**:
```asciidoc
// In FINAL_REPORT/book.adoc
include::appendix_glossary.adoc[]
```

**To compile standalone**:
```bash
cd FINAL_REPORT/
asciidoctor appendix_glossary.adoc
```

**Output**: `appendix_glossary.html`

---

### 4. `ontology_diagrams.dot` (GraphViz)
**Purpose**: Visual ontology diagrams
**Use for**:
- Paper figures
- Presentations
- Posters

**Contains 6 diagrams**:
1. **Main Ontology** - Core concepts and relationships
2. **Taxonomy** - 5-layer component hierarchy
3. **Feedback Loops** - Loop interactions and classifications
4. **Phase Space** - Attractor basin visualization
5. **Metaphor Mapping** - Thermodynamics ↔ Execution
6. **Data Flow** - Execution event → Optimized lookup

**To generate all diagrams**:
```bash
cd docs/

# PNG format (for papers)
dot -Tpng ontology_diagrams.dot -o ontology_full.png

# SVG format (for web/scaling)
dot -Tsvg ontology_diagrams.dot -o ontology_full.svg

# PDF format (for LaTeX)
dot -Tpdf ontology_diagrams.dot -o ontology_full.pdf
```

**To generate individual diagrams**:
```bash
# Main ontology
dot -Tpng -Gname=ontology_main ontology_diagrams.dot -o fig_ontology_main.png

# Taxonomy
dot -Tpng -Gname=taxonomy ontology_diagrams.dot -o fig_taxonomy.png

# Feedback loops
dot -Tpng -Gname=feedback_loops ontology_diagrams.dot -o fig_feedback.png

# Phase space
dot -Tpng -Gname=phase_space ontology_diagrams.dot -o fig_phase_space.png

# Metaphor mapping
dot -Tpng -Gname=metaphor_mapping ontology_diagrams.dot -o fig_metaphor.png

# Data flow
dot -Tpng -Gname=data_flow ontology_diagrams.dot -o fig_data_flow.png
```

**To include in LaTeX**:
```latex
\begin{figure}[h]
\centering
\includegraphics[width=0.8\textwidth]{fig_ontology_main.png}
\caption{StarForth ontology: core concepts and relationships}
\label{fig:ontology}
\end{figure}
```

---

## Quick Reference: Terminology Usage

### ✅ **Preferred Terms** (Academic Writing)

| Context | Use |
|---------|-----|
| Counting executions | "Execution frequency" |
| Time-based reduction | "Exponential decay" |
| Stable metrics | "Steady-state equilibrium" |
| Convergence analysis | "Attractor-based characterization" |
| System timing | "Adaptive heartbeat" |
| Lookup optimization | "Frequency-based caching" |
| Zero variance | "Deterministic convergence" |

### ❌ **Avoid** (Without Qualification)

- "Physics-based" → Use "Thermodynamically-inspired metaphor"
- "Execution heat" → Use "Execution frequency with decay"
- "AI-driven" → Use "Statistically-inferred"
- "Learning" → Use "Adaptive inference"

### ⚠️ **Qualify When Using Metaphors**

When you must use metaphorical terms:
```
"...employs a thermodynamic metaphor..."
"...execution frequency, metaphorically termed 'heat'..."
"...decay analogous to thermal dissipation..."
```

---

## Citation

To cite this ontology in academic work:

**BibTeX**:
```bibtex
@techreport{starforth-ontology-2025,
  title        = {StarForth Ontology: Formal Conceptual Framework for
                  Thermodynamically-Inspired Adaptive Virtual Machine},
  author       = {{StarForth Project}},
  year         = {2025},
  institution  = {StarshipOS},
  type         = {Technical Report},
  number       = {v1.0},
  month        = dec,
  note         = {Available at: \url{https://github.com/starshipos/starforth}}
}
```

**In-text citation**:
> "Following the formal ontology established for StarForth [StarForth-Ontology-2025], we define execution frequency as..."

---

## Version History

**v1.0** (2025-12-13):
- Initial ontology, taxonomy, and lexicon
- LaTeX, AsciiDoc, and GraphViz versions
- 6 visual diagrams
- Mathematical formalism
- Usage guidelines

---

## Dependencies

### For LaTeX Compilation:
```bash
# Ubuntu/Debian
sudo apt-get install texlive-full

# macOS
brew install --cask mactex
```

### For AsciiDoc Compilation:
```bash
# Ruby gem
gem install asciidoctor

# Or via package manager
sudo apt-get install asciidoctor  # Ubuntu/Debian
brew install asciidoctor          # macOS
```

### For GraphViz Diagrams:
```bash
# Ubuntu/Debian
sudo apt-get install graphviz

# macOS
brew install graphviz

# Verify installation
dot -V
```

---

## Maintenance

**When to update this ontology**:
- Adding new concepts to the adaptive runtime
- Changing mathematical formulations
- Publishing papers (ensure terminology consistency)
- Before DARPA proposal submission

**How to update**:
1. Edit `ONTOLOGY.md` (master document)
2. Regenerate `ontology.tex` sections from markdown
3. Update `appendix_glossary.adoc` with new terms
4. Add new diagrams to `ontology_diagrams.dot` if needed
5. Bump version number in all files
6. Update this README's version history

---

## License

CC0 / Public Domain

This ontology is released into the public domain to maximize reproducibility
and enable citation in academic work.