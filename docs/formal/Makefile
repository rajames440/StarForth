# docs/formal/Makefile — build all formal LaTeX publications

PDFLATEX := pdflatex -interaction=nonstopmode -halt-on-error
OUTDIR    := build

# Three research volumes
VOLS := vol1-vm-physics vol2-kernel vol3-research

# Three practitioner books
BOOKS := dev-guide user-guide cookbook

# Four standalone publications
STANDALONE := experiments proofs ssrn patent

.PHONY: all vols books standalone doxygen \
        vol1 vol2 vol3 \
        dev-guide user-guide cookbook \
        experiments proofs ssrn patent \
        clean

all: vols books standalone doxygen

vols: vol1 vol2 vol3

books: dev-guide user-guide cookbook

standalone: experiments proofs ssrn patent

vol1:
	@mkdir -p $(OUTDIR)/vol1
	cd vol1-vm-physics && $(PDFLATEX) -output-directory=../$(OUTDIR)/vol1 main.tex
	cd vol1-vm-physics && $(PDFLATEX) -output-directory=../$(OUTDIR)/vol1 main.tex

vol2:
	@mkdir -p $(OUTDIR)/vol2
	cd vol2-kernel && $(PDFLATEX) -output-directory=../$(OUTDIR)/vol2 main.tex
	cd vol2-kernel && $(PDFLATEX) -output-directory=../$(OUTDIR)/vol2 main.tex

vol3:
	@mkdir -p $(OUTDIR)/vol3
	cd vol3-research && $(PDFLATEX) -output-directory=../$(OUTDIR)/vol3 main.tex
	cd vol3-research && $(PDFLATEX) -output-directory=../$(OUTDIR)/vol3 main.tex

dev-guide:
	@mkdir -p $(OUTDIR)/dev-guide
	cd dev-guide && $(PDFLATEX) -output-directory=../$(OUTDIR)/dev-guide main.tex
	cd dev-guide && $(PDFLATEX) -output-directory=../$(OUTDIR)/dev-guide main.tex

user-guide:
	@mkdir -p $(OUTDIR)/user-guide
	cd user-guide && $(PDFLATEX) -output-directory=../$(OUTDIR)/user-guide main.tex
	cd user-guide && $(PDFLATEX) -output-directory=../$(OUTDIR)/user-guide main.tex

cookbook:
	@mkdir -p $(OUTDIR)/cookbook
	cd cookbook && $(PDFLATEX) -output-directory=../$(OUTDIR)/cookbook main.tex
	cd cookbook && $(PDFLATEX) -output-directory=../$(OUTDIR)/cookbook main.tex

experiments:
	@mkdir -p $(OUTDIR)/experiments
	cd experiments && $(PDFLATEX) -output-directory=../$(OUTDIR)/experiments main.tex
	cd experiments && $(PDFLATEX) -output-directory=../$(OUTDIR)/experiments main.tex

proofs:
	@mkdir -p $(OUTDIR)/proofs
	cd proofs && $(PDFLATEX) -output-directory=../$(OUTDIR)/proofs main.tex
	cd proofs && $(PDFLATEX) -output-directory=../$(OUTDIR)/proofs main.tex

ssrn:
	@mkdir -p $(OUTDIR)/ssrn
	cd ssrn && $(PDFLATEX) -output-directory=../$(OUTDIR)/ssrn main.tex
	cd ssrn && $(PDFLATEX) -output-directory=../$(OUTDIR)/ssrn main.tex

patent:
	@mkdir -p $(OUTDIR)/patent
	cd patent && $(PDFLATEX) -output-directory=../$(OUTDIR)/patent main.tex
	cd patent && $(PDFLATEX) -output-directory=../$(OUTDIR)/patent main.tex

doxygen:
	cd ../.. && doxygen Doxyfile
	@# Replace all Unicode characters in generated LaTeX with pdflatex-safe equivalents
	python3 fix-doxygen-unicode.py doxygen/latex/
	$(MAKE) -C doxygen/latex pdf
	@mkdir -p $(OUTDIR)/doxygen
	cp doxygen/latex/refman.pdf $(OUTDIR)/doxygen/StarForth-API-Reference.pdf
	@echo "Doxygen PDF: $(OUTDIR)/doxygen/StarForth-API-Reference.pdf"

clean:
	rm -rf $(OUTDIR)
	rm -rf doxygen/
