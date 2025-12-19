# StarKernel Isabelle/HOL Formal Verification Makefile

# Isabelle command (assumes isabelle is in PATH)
ISABELLE = isabelle

# Session name
SESSION = StarKernel

# Theory files
THEORIES = VMCore.thy Capabilities.thy ACL.thy Scheduler.thy

# Document output directory
DOC_DIR = output/document

.PHONY: all check clean check-vmcore check-capabilities check-acl check-scheduler \
        document jedit help

all: check

# Check all theories (build the session)
check:
	@echo "==> Verifying all StarKernel theories..."
	$(ISABELLE) build -v -d . -b $(SESSION)
	@echo "==> Verification complete!"

# Check individual theories
check-vmcore:
	@echo "==> Verifying VMCore.thy..."
	$(ISABELLE) build -v -d . -b $(SESSION) -e 'theory VMCore imports Main begin end'

check-capabilities:
	@echo "==> Verifying Capabilities.thy..."
	$(ISABELLE) build -v -d . -b $(SESSION) -e 'theory Capabilities imports VMCore begin end'

check-acl:
	@echo "==> Verifying ACL.thy..."
	$(ISABELLE) build -v -d . -b $(SESSION) -e 'theory ACL imports Capabilities begin end'

check-scheduler:
	@echo "==> Verifying Scheduler.thy..."
	$(ISABELLE) build -v -d . -b $(SESSION) -e 'theory Scheduler imports VMCore begin end'

# Generate PDF document from theories
document:
	@echo "==> Generating PDF documentation..."
	$(ISABELLE) build -v -d . -D document $(SESSION)
	@echo "==> PDF generated in $(DOC_DIR)/"

# Open interactive theorem prover
jedit:
	$(ISABELLE) jedit -d . -l $(SESSION) &

# Open specific theory in jEdit
jedit-vmcore:
	$(ISABELLE) jedit -d . VMCore.thy &

jedit-capabilities:
	$(ISABELLE) jedit -d . Capabilities.thy &

jedit-acl:
	$(ISABELLE) jedit -d . ACL.thy &

jedit-scheduler:
	$(ISABELLE) jedit -d . Scheduler.thy &

# Clean build artifacts
clean:
	@echo "==> Cleaning build artifacts..."
	rm -rf output/
	rm -f ./*.aux ./*.log ./*.pdf
	@echo "==> Clean complete!"

# Quick status check (shows theory dependencies)
status:
	@echo "==> StarKernel Theory Status:"
	@echo ""
	@echo "VMCore.thy          - Foundation (VM model, states, execution)"
	@echo "├── Capabilities.thy - Capability-based security"
	@echo "│   └── ACL.thy      - Fine-grained ACLs with TTL caching"
	@echo "└── Scheduler.thy   - VM arbiter scheduling algorithm"
	@echo ""
	@echo "Run 'make check' to verify all theories"
	@echo "Run 'make jedit' to open interactive prover"

# Show verification statistics
stats:
	@echo "==> StarKernel Verification Statistics:"
	@echo ""
	@echo "Theory         | Axioms | Theorems | Sorries | Status"
	@echo "---------------|--------|----------|---------|----------"
	@echo "VMCore         |   4    |    5     |    0    | Complete"
	@echo "Capabilities   |   1    |    9     |    0    | Complete"
	@echo "ACL            |   0    |   12     |    0    | Complete"
	@echo "Scheduler      |   0    |   17     |    3    | Partial"
	@echo ""
	@echo "Total:            5 axioms, 43 theorems, 3 sorries"
	@echo ""
	@echo "Next steps: Complete scheduler fairness proofs"

# Help
help:
	@echo "StarKernel Isabelle/HOL Verification Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all              - Check all theories (default)"
	@echo "  check            - Verify all theories"
	@echo "  check-vmcore     - Verify VMCore.thy only"
	@echo "  check-capabilities - Verify Capabilities.thy only"
	@echo "  check-acl        - Verify ACL.thy only"
	@echo "  check-scheduler  - Verify Scheduler.thy only"
	@echo "  document         - Generate PDF documentation"
	@echo "  jedit            - Open interactive theorem prover"
	@echo "  jedit-vmcore     - Open VMCore.thy in jEdit"
	@echo "  jedit-capabilities - Open Capabilities.thy in jEdit"
	@echo "  jedit-acl        - Open ACL.thy in jEdit"
	@echo "  jedit-scheduler  - Open Scheduler.thy in jEdit"
	@echo "  status           - Show theory dependency tree"
	@echo "  stats            - Show verification statistics"
	@echo "  clean            - Remove build artifacts"
	@echo "  help             - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make check           # Verify all theories"
	@echo "  make jedit           # Open interactive prover"
	@echo "  make document        # Generate PDF"
	@echo "  make stats           # Show statistics"

# Default target info
.DEFAULT_GOAL := help
