# ======================================================================
# StarForth Docs Makefile (DocBook 4.5) — HTML + EPUB + PDF + Kindle
# Outputs:
#   build/starforth-manual.html
#   build/StarForth-Manual.epub
#   build/StarForth-Manual.pdf
#   build/StarForth-Manual.azw3   (Kindle)
#   build/StarForth-Manual.mobi   (optional; enable KINDLE_MOBI=1)
# ======================================================================

SHELL := /bin/bash
.ONESHELL:

# ---- Config ------------------------------------------------------------------
DOCBOOK_FLAVOR ?= db45
DOCS_DIR        := $(abspath .)
BUILD_DIR       := $(DOCS_DIR)/build
GEN_DIR         := $(BUILD_DIR)/generated

# Where our custom XSL lives
CUSTOM_XSL      := $(DOCS_DIR)/xsl/custom-html.xsl

# Some of your book includes are "generated/..." — we mirror from build/generated/
SYNC_GENERATED  ?= 1

# Primary source DocBook with xi:include entries
SRC_BOOK        := $(DOCS_DIR)/starforth-book.xml
MERGED_BOOK     := $(BUILD_DIR)/starforth-book.merged.xml

# Outputs
HTML_OUT        := $(BUILD_DIR)/starforth-manual.html
EPUB_OUT        := $(BUILD_DIR)/StarForth-Manual.epub
PDF_OUT         := $(BUILD_DIR)/StarForth-Manual.pdf
AZW3_OUT        := $(BUILD_DIR)/StarForth-Manual.azw3
MOBI_OUT        := $(BUILD_DIR)/StarForth-Manual.mobi

# PDF engine preference: pandoc | dblatex | chromium | wkhtmltopdf | auto
PDF_ENGINE      ?= pandoc

# Tools
XMLLINT         ?= xmllint
XSLTPROC        ?= xsltproc
PANDOC          ?= pandoc
DBLATEX         ?= dblatex
WKHTMLTOPDF     ?= wkhtmltopdf
EBOOK_CONVERT   ?= ebook-convert

# Stock DocBook 4.5 HTML stylesheet (only used inside custom XSL; here for checks)
XSL_HTML        ?= /usr/share/xml/docbook/stylesheet/docbook-xsl/html/docbook.xsl

# Kindle MOBI toggle (off by default; AZW3 is preferred by modern Kindle)
KINDLE_MOBI     ?= 0

# ----- helpers ---------------------------------------------------------------
define NEED_TOOL
	@if ! command -v $(1) >/dev/null 2>&1; then \
	  echo "✖ Missing tool: $(1). Install it (e.g., sudo apt-get install $(2))" >&2; \
	  exit 1; \
	fi
endef

.PHONY: all book html epub pdf kindle azw3 mobi clean distclean tools check sync-gen

all: book

book: html epub pdf kindle
	@echo "✓ Book build complete →"
	@echo "   HTML: $(HTML_OUT)"
	@echo "   EPUB: $(EPUB_OUT)"
	@echo "   PDF : $(PDF_OUT)"
	@echo "   AZW3: $(AZW3_OUT)$(if $(filter 1,$(KINDLE_MOBI)),\n   MOBI: $(MOBI_OUT),)"

html: tools $(HTML_OUT)
epub: tools $(EPUB_OUT)
pdf:  tools $(PDF_OUT)
kindle: azw3 $(if $(filter 1,$(KINDLE_MOBI)),mobi,)

tools:
	@$(call NEED_TOOL,$(XMLLINT),libxml2-utils)
	@$(call NEED_TOOL,$(XSLTPROC),xsltproc)
	@$(call NEED_TOOL,$(PANDOC),pandoc)
	@if [ ! -f "$(XSL_HTML)" ]; then \
	  echo "✖ DocBook XSL not found: $(XSL_HTML)"; \
	  echo "  Install: sudo apt-get install docbook-xsl"; exit 1; \
	fi
	@if [ ! -f "$(CUSTOM_XSL)" ]; then \
	  echo "✖ Missing custom XSL: $(CUSTOM_XSL)"; \
	  echo "  Create it (see docs/xsl/custom-html.xsl)"; exit 1; \
	fi
	@mkdir -p "$(BUILD_DIR)" "$(GEN_DIR)" "$(DOCS_DIR)/css" "$(DOCS_DIR)/js" "$(DOCS_DIR)/xsl"

check:
	@if [ ! -f "$(SRC_BOOK)" ]; then \
	  echo "✖ Missing source book XML: $(SRC_BOOK)"; \
	  echo "  Create it or adjust SRC_BOOK in docs/Makefile."; \
	  exit 1; \
	fi

# Mirror build/generated → docs/generated if your book hrefs expect that path
sync-gen: | tools
	@if [ "$(SYNC_GENERATED)" = "1" ]; then \
	  mkdir -p "$(DOCS_DIR)/generated"; \
	  rsync -a "$(GEN_DIR)/" "$(DOCS_DIR)/generated/" 2>/dev/null || true; \
	fi

# Merge with XInclude and validate
$(MERGED_BOOK): check sync-gen | tools
	@echo "• Merging XIncludes → $(MERGED_BOOK)"
	@$(XMLLINT) --xinclude --noout "$(SRC_BOOK)" || { echo "✖ XInclude failed"; exit 1; }
	@$(XMLLINT) --xinclude --format "$(SRC_BOOK)" > "$(MERGED_BOOK)" || { echo "✖ Could not write merged book"; exit 1; }
	@$(XMLLINT) --noout "$(MERGED_BOOK)" || { echo "✖ Merged book invalid"; exit 1; }

# HTML via our custom XSL (handles CSS/JS + body attributes)
CUSTOM_XSL := $(DOCS_DIR)/xsl/custom-html.xsl
XSL_HTML   ?= /usr/share/xml/docbook/stylesheet/docbook-xsl/html/docbook.xsl

$(HTML_OUT): $(MERGED_BOOK) $(CUSTOM_XSL)
	@echo "• Generating HTML → $(HTML_OUT)"
	@$(XSLTPROC) --xinclude \
	  --stringparam html.stylesheet "../css/dark.css" \
	  -o "$(HTML_OUT)" "$(CUSTOM_XSL)" "$(MERGED_BOOK)"
	@iconv -f UTF-8 -t UTF-8 "$(HTML_OUT)" -o "$(HTML_OUT).utf8" && mv "$(HTML_OUT).utf8" "$(HTML_OUT)" || true

# EPUB via pandoc from HTML
$(EPUB_OUT): $(HTML_OUT)
	@echo "• Generating EPUB → $(EPUB_OUT)"
	@$(PANDOC) "$(HTML_OUT)" \
	  --metadata=title:"StarForth Manual" \
	  --embed-resources \
	  --standalone \
	  -o "$(EPUB_OUT)"

# PDF — respect PDF_ENGINE (default pandoc). Fallbacks if PDF_ENGINE=auto.
$(PDF_OUT): $(MERGED_BOOK) | tools
	@echo "• Generating PDF → $(PDF_OUT)"
	@engine="$(PDF_ENGINE)"; \
	if [ "$$engine" = "auto" ]; then \
	  if command -v $(PANDOC) >/dev/null 2>&1; then \
	    engine="pandoc"; \
	  elif command -v $(DBLATEX) >/dev/null 2>&1; then \
	    engine="dblatex"; \
	  elif command -v $(WKHTMLTOPDF) >/dev/null 2>&1; then \
	    engine="wkhtmltopdf"; \
	  else \
	    echo "✖ No PDF engine found. Install one: pandoc OR dblatex OR wkhtmltopdf"; exit 1; \
	  fi; \
	fi; \
	case "$$engine" in \
	  pandoc) \
	    $(MAKE) -s $(HTML_OUT); \
	    $(PANDOC) "$(HTML_OUT)" --pdf-engine=lualatex -o "$(PDF_OUT)" \
	      || $(PANDOC) "$(HTML_OUT)" --pdf-engine=xelatex -o "$(PDF_OUT)";; \
	  dblatex) \
	    $(DBLATEX) -o "$(PDF_OUT)" "$(MERGED_BOOK)";; \
	  wkhtmltopdf) \
	    $(MAKE) -s $(HTML_OUT); \
	    $(WKHTMLTOPDF) "$(HTML_OUT)" "$(PDF_OUT)";; \
	  chromium) \
	    $(MAKE) -s $(HTML_OUT); \
	    CHROME="$$(command -v chromium || command -v chromium-browser || command -v google-chrome)"; \
	    [ -n "$$CHROME" ] || { echo "✖ Chromium/Chrome not found"; exit 1; }; \
	    "$$CHROME" --headless --disable-gpu \
	      --print-to-pdf="$(PDF_OUT)" \
	      "file://$(abspath $(HTML_OUT))";; \
	  *) echo "✖ Unknown PDF_ENGINE=$$engine"; exit 1;; \
	esac

# Kindle (AZW3) from EPUB using Calibre
azw3: $(AZW3_OUT)
$(AZW3_OUT): $(EPUB_OUT)
	@echo "• Generating AZW3 (Kindle) → $(AZW3_OUT)"
	@if ! command -v $(EBOOK_CONVERT) >/dev/null 2>&1; then \
	  echo "✖ Missing 'ebook-convert'. Install Calibre to build AZW3/MOBI."; exit 1; \
	fi
	@$(EBOOK_CONVERT) "$(EPUB_OUT)" "$(AZW3_OUT)"

# Optional MOBI
mobi: $(MOBI_OUT)
$(MOBI_OUT): $(EPUB_OUT)
	@echo "• Generating MOBI (legacy) → $(MOBI_OUT)"
	@if ! command -v $(EBOOK_CONVERT) >/dev/null 2>&1; then \
	  echo "✖ Missing 'ebook-convert'. Install Calibre."; exit 1; \
	fi
	@$(EBOOK_CONVERT) "$(EPUB_OUT)" "$(MOBI_OUT)"

clean:
	@echo "🧹 Cleaning DocBook outputs..."
	@rm -f "$(HTML_OUT)" "$(EPUB_OUT)" "$(PDF_OUT)" "$(AZW3_OUT)" "$(MOBI_OUT)" "$(MERGED_BOOK)"

distclean: clean
	@echo "🧹 Deep clean generated docs dir..."
	@rm -rf "$(BUILD_DIR)"
