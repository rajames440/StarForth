# StarForth Documentation System

Welcome to the StarForth documentation system! This document explains how to generate, view, and contribute to the
project documentation.

## Quick Start

### Generate All Documentation

```bash
make docs
```

This generates:

- **HTML** - Interactive web docs (docs/api/html/)
- **PDF** - Complete reference manual (docs/api/StarForth-API-Reference.pdf)
- **AsciiDoc** - Lightweight markup (docs/api/asciidoc/)
- **Markdown** - GitHub-friendly (docs/api/markdown/)
- **Man Pages** - Unix man pages (docs/api/man/)

### View Documentation

```bash
make docs-open      # Generate and open HTML in browser
```

Or manually:

```bash
xdg-open docs/api/html/index.html      # Linux
open docs/api/html/index.html          # macOS
start docs/api/html/index.html         # Windows
```

### Quick HTML-Only Generation

```bash
make docs-html      # Fast, no PDF/AsciiDoc/Markdown
```

## Installation Requirements

### Required (Core Functionality)

```bash
# Ubuntu/Debian
sudo apt-get install doxygen graphviz

# macOS
brew install doxygen graphviz

# Fedora/RHEL
sudo dnf install doxygen graphviz
```

### Optional (Additional Formats)

#### For PDF Generation

```bash
# Ubuntu/Debian
sudo apt-get install texlive-latex-base texlive-latex-extra

# macOS
brew install --cask mactex    # Full TeX Live (~4GB)
# Or for minimal install:
brew install --cask basictex  # Basic TeX (~100MB)

# Fedora/RHEL
sudo dnf install texlive-scheme-basic
```

#### For AsciiDoc and Markdown

```bash
# All platforms
sudo apt-get install pandoc        # Debian/Ubuntu
brew install pandoc                # macOS
sudo dnf install pandoc            # Fedora/RHEL

# AsciiDoc HTML rendering (optional)
gem install asciidoctor
```

### Verify Installation

```bash
doxygen --version    # Should be >= 1.9.0
dot -V               # Graphviz, any recent version
pandoc --version     # Optional, for AsciiDoc/Markdown
pdflatex --version   # Optional, for PDF
```

## Documentation Targets

| Target            | Description                       | Requirements                   |
|-------------------|-----------------------------------|--------------------------------|
| `make docs`       | Generate all formats              | doxygen, graphviz (+ optional) |
| `make docs-html`  | HTML only (fast)                  | doxygen, graphviz              |
| `make docs-pdf`   | PDF only                          | doxygen, graphviz, pdflatex    |
| `make docs-open`  | Generate HTML and open in browser | doxygen, graphviz              |
| `make docs-clean` | Remove generated docs             | none                           |

## Documentation Structure

```
docs/
├── api/                           # Generated API docs (created by make docs)
│   ├── html/                      # HTML documentation
│   │   └── index.html             # Main page
│   ├── latex/                     # LaTeX source for PDF
│   ├── xml/                       # XML intermediate format
│   ├── docbook/                   # DocBook format
│   ├── man/                       # Unix man pages
│   ├── asciidoc/                  # AsciiDoc format
│   │   └── starforth-api.adoc     # Main AsciiDoc file
│   ├── markdown/                  # GitHub Markdown
│   │   └── starforth-api.md       # Main Markdown file
│   ├── StarForth-API-Reference.pdf # PDF manual
│   ├── doxygen_warnings.log       # Documentation warnings
│   ├── starforth.tag              # Tag file for external projects
│   └── README.md                  # API docs README
│
├── examples/
│   └── doxygen_example.h          # Example documented header
│
├── DOXYGEN_STYLE_GUIDE.md         # How to write Doxygen comments
├── DOCUMENTATION_README.md         # This file
├── GAP_ANALYSIS.md                # Code quality analysis
├── ARCHITECTURE.md                # System architecture (TODO)
├── ARM64_OPTIMIZATIONS.md         # ARM64 performance guide
├── ASM_OPTIMIZATIONS.md           # x86_64 performance guide
├── L4RE_INTEGRATION.md            # L4Re microkernel guide
└── RASPBERRY_PI_BUILD.md          # RPi4 build guide
```

## Adding Documentation to Code

### Step 1: Read the Style Guide

See [DOXYGEN_STYLE_GUIDE.md](DOXYGEN_STYLE_GUIDE.md) for complete documentation standards.

### Step 2: Use the Template

Use [examples/doxygen_example.h](examples/doxygen_example.h) as a template showing all documentation styles.

### Step 3: Document Your Code

Example for a function:

```c
/**
 * @brief Push value onto data stack
 *
 * @details
 * Adds a value to the top of the data stack. Stack overflow
 * is checked and vm->error is set if stack is full.
 *
 * @param vm Pointer to VM instance
 * @param value Value to push
 *
 * @pre vm must be initialized
 * @pre vm->dsp < STACK_SIZE-1
 * @post vm->dsp incremented by 1
 * @post On error: vm->error is set
 *
 * @note This is a hot-path function - optimized for speed
 * @warning Always check vm->error after calling
 *
 * @see vm_pop()
 * @see vm_dup()
 *
 * @par Example:
 * @code
 * vm_push(&vm, 42);
 * if (vm.error) {
 *     fprintf(stderr, "Stack overflow\n");
 * }
 * @endcode
 */
void vm_push(VM *vm, cell_t value);
```

### Step 4: Check Documentation

```bash
make docs-html
cat docs/api/doxygen_warnings.log
```

Fix any warnings about undocumented functions or parameters.

## Documentation Coverage

### Current Status

Run `make docs` and check the summary for documentation coverage:

- Target: 100% of public API headers
- Current: TBD (run `make docs` to check)

### Priority Files to Document

1. **High Priority** (User-facing API):
    - `include/vm.h` - Core VM API
    - `include/word_registry.h` - Word registration
    - `include/log.h` - Logging API
    - `include/io.h` - I/O operations

2. **Medium Priority** (Developer API):
    - `src/word_source/include/*.h` - Word implementations
    - `include/profiler.h` - Performance profiling
    - `include/vm_debug.h` - Debugging support

3. **Low Priority** (Internal):
    - Private implementation files (*.c)
    - Test infrastructure headers

## Integration with External Projects

### Using StarForth Documentation in Your Project

If your project uses StarForth, you can link to our documentation:

1. Generate our docs: `make docs`
2. Reference our tag file: `docs/api/starforth.tag`
3. Add to your Doxyfile:
   ```
   TAGFILES = path/to/starforth/docs/api/starforth.tag=https://your-docs-url/
   ```

## Continuous Integration

### GitHub Actions Example

```yaml
name: Documentation

on: [push, pull_request]

jobs:
  docs:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y doxygen graphviz

      - name: Generate documentation
        run: make docs-html

      - name: Check for warnings
        run: |
          if [ -s docs/api/doxygen_warnings.log ]; then
            echo "Documentation warnings found:"
            cat docs/api/doxygen_warnings.log
            exit 1
          fi

      - name: Deploy to GitHub Pages
        if: github.ref == 'refs/heads/master'
        uses: peaceiris/actions-gh-pages@v3
        with:
          github_token: ${{ secrets.GITHUB_TOKEN }}
          publish_dir: ./docs/api/html
```

## Troubleshooting

### Problem: "doxygen: command not found"

**Solution:** Install doxygen (see Installation Requirements above)

### Problem: "dot: command not found"

**Solution:** Install graphviz package

### Problem: "pdflatex: command not found"

**Solution:** PDF generation is optional. Either:

- Install TeX Live (see Installation Requirements)
- Or use `make docs-html` for HTML only

### Problem: No graphs/diagrams in documentation

**Solution:** Install graphviz (`sudo apt-get install graphviz`)

### Problem: Warnings about undocumented functions

**Solution:** Add Doxygen comments to those functions (see Style Guide)

### Problem: "No such file or directory" when opening docs

**Solution:** Generate docs first: `make docs-html`

### Problem: Pandoc errors for AsciiDoc/Markdown

**Solution:** AsciiDoc/Markdown generation is optional. Either:

- Install pandoc
- Or ignore (HTML/PDF are sufficient)

## Man Pages

View generated man pages:

```bash
# List all man pages
ls docs/api/man/

# View a specific page
man -l docs/api/man/vm_init.3

# Install to system (optional)
sudo cp docs/api/man/*.3 /usr/local/man/man3/
sudo mandb
man vm_init  # Now works without -l
```

## Contributing Documentation

### Documentation Pull Requests

When contributing code:

1. **Document all public functions** - Use Doxygen comments
2. **Include examples** - Show how to use complex functions
3. **Check for warnings** - Run `make docs-html` and fix warnings
4. **Update existing docs** - If changing behavior, update comments

### Documentation-Only Changes

Documentation improvements are welcome! To contribute:

1. Follow the [DOXYGEN_STYLE_GUIDE.md](DOXYGEN_STYLE_GUIDE.md)
2. Test with `make docs`
3. Check for warnings in `docs/api/doxygen_warnings.log`
4. Submit PR with clear description of changes

## Advanced Configuration

### Customizing Documentation

Edit `Doxyfile` to customize:

- Project name/version: `PROJECT_NAME`, `PROJECT_NUMBER`
- Input files: `INPUT`, `FILE_PATTERNS`
- Output formats: `GENERATE_HTML`, `GENERATE_LATEX`, etc.
- Diagram options: `CALL_GRAPH`, `CALLER_GRAPH`

### Adding Custom Pages

Create Markdown files in `docs/` and add to `Doxyfile`:

```
INPUT = ... docs/MY_CUSTOM_PAGE.md
```

### Theming HTML Output

Customize appearance:

```
HTML_EXTRA_STYLESHEET = docs/custom.css
HTML_COLORSTYLE = AUTO_LIGHT
```

## Documentation Formats

### HTML

- **Best for:** Interactive browsing, searching, navigation
- **Location:** docs/api/html/index.html
- **Features:** Hyperlinks, search, call graphs, file dependencies

### PDF

- **Best for:** Printing, offline reading, comprehensive reference
- **Location:** docs/api/StarForth-API-Reference.pdf
- **Features:** Table of contents, index, bookmarks

### AsciiDoc

- **Best for:** Technical writing, conversion to other formats
- **Location:** docs/api/asciidoc/starforth-api.adoc
- **Features:** Lightweight markup, easy to edit

### Markdown

- **Best for:** GitHub viewing, copy-paste into wikis
- **Location:** docs/api/markdown/starforth-api.md
- **Features:** GFM (GitHub-Flavored Markdown)

### Man Pages

- **Best for:** Unix command-line reference
- **Location:** docs/api/man/*.3
- **Features:** Traditional man page format, system integration

## Questions?

- **Doxygen Manual:** https://www.doxygen.nl/manual/
- **Style Guide:** [DOXYGEN_STYLE_GUIDE.md](DOXYGEN_STYLE_GUIDE.md)
- **Example Code:** [examples/doxygen_example.h](examples/doxygen_example.h)
- **Issues:** https://github.com/rajames440/StarForth/issues

---

**Generated by the StarForth Documentation System**
**Sniff-tested by Santino 🐕**
