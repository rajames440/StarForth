# docs/formal/Makefile — build all formal LaTeX publications

PDFLATEX := pdflatex -interaction=nonstopmode -halt-on-error
OUTDIR    := build

VOLS   := vol1-vm-physics vol2-kernel vol3-research
PUBS   := dev-guide user-guide cookbook

.PHONY: all vols pubs vol1 vol2 vol3 dev-guide user-guide cookbook clean

all: vols pubs

vols: vol1 vol2 vol3

pubs: dev-guide user-guide cookbook

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

clean:
	rm -rf $(OUTDIR)
