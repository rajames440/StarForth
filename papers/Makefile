# Makefile for all papers
# Usage:
#   make arxiv        - Build arXiv src
#   make clean-arxiv  - Clean arXiv src
#   make all          - Build all papers
#   make clean        - Clean all papers

.PHONY: all clean arxiv clean-arxiv

# Default target: build all papers
all: arxiv

# Clean all papers
clean: clean-arxiv

# arXiv src targets
arxiv:
	@echo "Building arXiv paper..."
	@cd entities/arxiv && $(MAKE)

clean-arxiv:
	@echo "Cleaning arXiv paper..."
	@cd entities/arxiv && $(MAKE) clean
