# Makefile for arXiv paper build
# Usage:
#   make        - Build main.pdf
#   make clean  - Remove all build artifacts

MAIN = main
SRC_DIR = src
LATEX = pdflatex
BIBTEX = bibtex
LATEX_FLAGS = -interaction=nonstopmode

.PHONY: all clean

all: $(MAIN).pdf

$(MAIN).pdf: $(SRC_DIR)/*.tex
	@echo "Building $(MAIN).pdf from $(SRC_DIR)/"
	cd $(SRC_DIR) && $(LATEX) $(LATEX_FLAGS) $(MAIN).tex
	cd $(SRC_DIR) && $(BIBTEX) $(MAIN)
	cd $(SRC_DIR) && $(LATEX) $(LATEX_FLAGS) $(MAIN).tex
	cd $(SRC_DIR) && $(LATEX) $(LATEX_FLAGS) $(MAIN).tex
	@cp $(SRC_DIR)/$(MAIN).pdf .
	@echo ""
	@echo "Build complete: $(MAIN).pdf"
	@ls -lh $(MAIN).pdf

clean:
	rm -f $(MAIN).pdf
	rm -f $(SRC_DIR)/$(MAIN).pdf
	rm -f $(SRC_DIR)/$(MAIN).aux $(SRC_DIR)/$(MAIN).log $(SRC_DIR)/$(MAIN).bbl $(SRC_DIR)/$(MAIN).blg
	rm -f $(SRC_DIR)/$(MAIN).out $(SRC_DIR)/$(MAIN).toc $(SRC_DIR)/$(MAIN).lof $(SRC_DIR)/$(MAIN).lot
	rm -f $(SRC_DIR)/$(MAIN).synctex.gz
	@echo "Clean complete"
