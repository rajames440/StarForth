# docs/formal/Makefile — build all three formal LaTeX volumes

VOLS    := vol1-vm-physics vol2-kernel vol3-research
PDFLATEX := pdflatex -interaction=nonstopmode -halt-on-error
OUTDIR  := build

.PHONY: all vol1 vol2 vol3 clean

all: vol1 vol2 vol3

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

clean:
	rm -rf $(OUTDIR)
