#!/usr/bin/env bash
#
# StarForth Documentation Builder (idempotent + fixed paths)
# - Uses repo_root/docs as the docs root
# - Wraps docs/build/generated/l4re-blkio-generated.xml ONCE (only if needed)
# - Validates with xmllint
# - Builds the book via docs/Makefile
#
# Usage:
#   ./scripts/build-docs.sh
#   DOCBOOK=db5  ./scripts/build-docs.sh   # force DocBook 5
#   DOCBOOK=db45 ./scripts/build-docs.sh   # force DocBook 4.5
#
set -euo pipefail

# ---- tiny helpers ------------------------------------------------------------
need() { command -v "$1" >/dev/null 2>&1 || { echo "✖ Missing tool: $1" >&2; exit 1; }; }
log()  { echo "• $*"; }
die()  { echo "✖ $*" >&2; exit 1; }

need xmllint
need awk
need make

# ---- layout ------------------------------------------------------------------
SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &>/dev/null && pwd )"
REPO_ROOT="$(realpath "${SCRIPT_DIR}/..")"
DOCS_DIR="${REPO_ROOT}/docs"
DOCS_MAKEFILE="${DOCS_DIR}/Makefile"
BUILD_DIR="${DOCS_DIR}/build"
GEN_DIR="${BUILD_DIR}/generated"

# Problematic fragment to normalize (adjust if you add more)
FRAG_PATH="${GEN_DIR}/l4re-blkio-generated.xml"
FRAG_ID="l4re-blkio"
FRAG_TITLE="L4Re Block I/O"

# ---- docbook flavor detection ------------------------------------------------
DOCBOOK="${DOCBOOK:-auto}"
detect_docbook() {
  if [[ "$DOCBOOK" == "db5" || "$DOCBOOK" == "db45" ]]; then
    echo "$DOCBOOK"; return
  fi
  local probe1="${DOCS_DIR}/starforth-book.xml"
  local probe2="${BUILD_DIR}/starforth-book.xml"
  if [[ -f "$probe1" ]] && grep -q 'xmlns="http://docbook.org/ns/docbook"' "$probe1"; then
    echo "db5"; return
  fi
  if [[ -f "$probe2" ]] && grep -q 'xmlns="http://docbook.org/ns/docbook"' "$probe2"; then
    echo "db5"; return
  fi
  echo "db45"
}
DOCBOOK_FLAVOR="$(detect_docbook)"
echo "→ DocBook flavor: ${DOCBOOK_FLAVOR}"

# ---- fragment wrapper (DocBook 4.5 or 5) -------------------------------------
wrap_docbook_fragment() {
  # $1=src $2=id $3=title $4=db5|db45
  local src="$1" id="$2" title="$3" flavor="$4"
  [[ -f "$src" ]] || die "wrap_docbook_fragment: missing file: $src"

  local tmp="${src}.wrapped.$$"
  case "$flavor" in
    db5)
      awk -v _ID="$id" -v _TITLE="$title" '
        BEGIN{
          print "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
          print "<section xmlns=\"http://docbook.org/ns/docbook\" version=\"5.0\" xml:id=\"" _ID "\">"
          print "  <title>" _TITLE "</title>"
        }
        !/^[[:space:]]*<\?xml/ && !/^[[:space:]]*<!DOCTYPE/ { print }
        END{ print "</section>" }' "$src" > "$tmp"
      ;;
    db45)
      awk -v _ID="$id" -v _TITLE="$title" '
        BEGIN{
          print "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
          print "<!DOCTYPE sect1 PUBLIC \"-//OASIS//DTD DocBook XML V4.5//EN\" \"http://www.oasis-open.org/docbook/xml/4.5/docbookx.dtd\">"
          print "<sect1 id=\"" _ID "\"><title>" _TITLE "</title>"
        }
        !/^[[:space:]]*<\?xml/ && !/^[[:space:]]*<!DOCTYPE/ { print }
        END{ print "</sect1>" }' "$src" > "$tmp"
      ;;
    *) die "Unknown DocBook flavor: $flavor" ;;
  esac

  xmllint --noout "$tmp" || { rm -f "$tmp"; die "Not well-formed after wrap: $src"; }
  mv -f "$tmp" "$src"
  log "Wrapped fragment → $src"
}

# ---- idempotent normalization ------------------------------------------------
normalize_fragment_if_needed() {
  local src="$1" id="$2" flavor="$3"
  [[ -f "$src" ]] || { log "⚠ fragment missing, skipping normalize: $src"; return 0; }

  # If not well-formed, wrap once.
  if ! xmllint --noout "$src" >/dev/null 2>&1; then
    log "Fragment not well-formed; applying wrapper"
    wrap_docbook_fragment "$src" "$id" "$FRAG_TITLE" "$flavor"
    return 0
  fi

  # If well-formed, ensure it has the right root + id. If yes, skip; else wrap.
  local root_name root_id
  root_name="$(xmllint --xpath 'name(/*)' "$src" 2>/dev/null || echo '')"
  root_id="$(xmllint --xpath 'string(/*/@id|/*/@xml:id)' "$src" 2>/dev/null || echo '')"

  local need_wrap=0
  if [[ "$flavor" == "db45" && "$root_name" != "sect1" ]]; then need_wrap=1; fi
  if [[ "$flavor" == "db5"  && "$root_name" != "section" ]]; then need_wrap=1; fi
  if [[ "$root_id" != "$id" ]]; then need_wrap=1; fi

  if [[ $need_wrap -eq 1 ]]; then
    log "Fragment well-formed but not normalized; wrapping once"
    wrap_docbook_fragment "$src" "$id" "$FRAG_TITLE" "$flavor"
  else
    log "Fragment already normalized; skipping wrap"
  fi
}

# ---- main --------------------------------------------------------------------
main() {
  mkdir -p "$GEN_DIR"

  # optional: let a docs-local HTML step run if present (no-op if target absent)
  if [[ -f "$DOCS_MAKEFILE" ]]; then
    log "Running docs HTML step via docs/Makefile…"
    make -C "$DOCS_DIR" -k html >/dev/null 2>&1 || true
  else
    log "⚠ No docs/Makefile found; you should have created it in docs/."; :
  fi

  # If the fragment isn't there yet, try a generator script if you have one
  if [[ ! -f "$FRAG_PATH" ]]; then
    if [[ -x "${REPO_ROOT}/scripts/generate_docs.sh" ]]; then
      log "Generating fragments via scripts/generate_docs.sh…"
      "${REPO_ROOT}/scripts/generate_docs.sh" || die "generator failed"
    fi
  fi

  # Normalize (idempotent)
  normalize_fragment_if_needed "$FRAG_PATH" "$FRAG_ID" "$DOCBOOK_FLAVOR"

  # Validate once more for good measure
  if [[ -f "$FRAG_PATH" ]]; then
    xmllint --noout "$FRAG_PATH" || die "Still malformed after normalize: $FRAG_PATH"
  fi

  # Build the book via docs/Makefile
  [[ -f "$DOCS_MAKEFILE" ]] || die "No Makefile in ${DOCS_DIR} to build the book."
  log "Building DocBook book via docs/Makefile…"
  make -C "$DOCS_DIR" book
  log "✓ Book build complete."
}

main "$@"
