#!/usr/bin/env python3
"""Convert all SVGs in charts/ to PDF for LaTeX \includegraphics."""
import subprocess
import sys
from pathlib import Path

CHARTS = Path(__file__).parent / "charts"
PDFS   = Path(__file__).parent / "report" / "figures"
PDFS.mkdir(parents=True, exist_ok=True)

svgs = sorted(CHARTS.glob("*.svg"))
if not svgs:
    print("No SVGs found in", CHARTS)
    sys.exit(1)

ok = fail = 0
for svg in svgs:
    pdf = PDFS / svg.with_suffix(".pdf").name
    result = subprocess.run(
        ["rsvg-convert", "--format=pdf", "--output", str(pdf), str(svg)],
        capture_output=True, text=True
    )
    if result.returncode == 0:
        print(f"  OK  {svg.name} -> {pdf.name}")
        ok += 1
    else:
        print(f"  ERR {svg.name}: {result.stderr.strip()}")
        fail += 1

print(f"\n{ok} converted, {fail} failed.")
sys.exit(0 if fail == 0 else 1)
