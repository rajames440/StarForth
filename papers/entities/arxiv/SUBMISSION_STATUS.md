# arXiv Submission Status

## ✅ Complete

- Paper structure reorganized to arXiv standards
- All sections written (10 main + 1 appendix)
- Bibliography with 15 citations
- Tables copied from canonical source (papers/LaTeX/tables/)
- README with complete build instructions
- **PDF builds successfully** (315 KB, 19 pages)

## 📊 Canonical Data Source

All figures, tables, and formulas come from:
```
papers/LaTeX/
├── figures/    ← 100+ SVG figures (already generated)
├── tables/     ← LaTeX tables (already generated)
├── formulas/   ← Math formulas (can be included)
└── scripts/    ← R scripts to regenerate
```

## ⏳ Before arXiv Submission

### 1. Check Available Figures

```bash
cd papers/src/figures/
ls -1 *.svg | wc -l    # Count available figures
```

Current status: **100+ SVG figures available**

### 2. Convert Required Figures to PDF

The paper references 6 specific figures. Check if they exist in canonical source:

```bash
cd papers/src/figures/
# Look for figures matching src requirements:
ls -1 | grep -E "(cv|window|histogram|trajectory|james|residual)"
```

### 3. Convert SVG → PDF for arXiv

```bash
cd papers/src/figures/
for f in *.svg; do
  inkscape "$f" --export-pdf="${f%.svg}.pdf" 2>/dev/null
done
```

### 4. Copy to arXiv Submission Directory

```bash
cd papers/entities/arxiv/figures/
cp ../../../src/figures/*.pdf .
```

### 5. Final Build Test

```bash
cd papers/entities/arxiv/
latexmk -C                 # Clean
pdflatex main.tex          # Build
bibtex main               # Bibliography
pdflatex main.tex         # Resolve references
pdflatex main.tex         # Final pass
```

### 6. Create Submission Archive

```bash
cd papers/entities/
tar czf arxiv-submission.tar.gz arxiv/
```

## 📝 Paper Contents

**Structure:** main.tex (arXiv-compliant) ✓
- abstract.tex (169 words)
- intro.tex
- related.tex  
- architecture.tex
- formalism.tex
- methods.tex
- results.tex
- discussion.tex
- implications.tex
- threats.tex
- conclusion.tex
- appendix.tex (consolidated)
- references.bib

**Assets:**
- Tables: 2 copied from papers/LaTeX/tables/ ✓
- Figures: 0/6 (need to copy from papers/LaTeX/figures/)
- Formulas: Embedded in appendix.tex ✓

## 🎯 Ready to Submit?

- [ ] Generate/identify required figures from canonical source
- [ ] Convert SVG → PDF
- [ ] Copy figures to arxiv/figures/
- [ ] Test build with all assets
- [ ] Proofread
- [ ] Create tarball
- [ ] Upload to arXiv

