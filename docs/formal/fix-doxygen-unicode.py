#!/usr/bin/env python3
"""
fix-doxygen-unicode.py — replace problematic Unicode characters in
Doxygen-generated LaTeX files with pdflatex-safe equivalents.

Run this on the doxygen/latex/ directory before calling pdflatex.
"""
import os, sys, re

REPLACEMENTS = [
    # Math symbols
    ('≈', r'\ensuremath{\approx}'),
    ('≤', r'\ensuremath{\leq}'),
    ('≥', r'\ensuremath{\geq}'),
    ('≠', r'\ensuremath{\neq}'),
    ('±', r'\ensuremath{\pm}'),
    ('×', r'\ensuremath{\times}'),
    ('÷', r'\ensuremath{\div}'),
    ('∞', r'\ensuremath{\infty}'),
    ('∑', r'\ensuremath{\sum}'),
    ('∏', r'\ensuremath{\prod}'),
    ('∫', r'\ensuremath{\int}'),
    ('√', r'\ensuremath{\sqrt{\cdot}}'),
    ('∂', r'\ensuremath{\partial}'),
    ('∆', r'\ensuremath{\Delta}'),
    ('∇', r'\ensuremath{\nabla}'),
    ('∈', r'\ensuremath{\in}'),
    ('∉', r'\ensuremath{\notin}'),
    ('∩', r'\ensuremath{\cap}'),
    ('∪', r'\ensuremath{\cup}'),
    ('⊂', r'\ensuremath{\subset}'),
    ('⊃', r'\ensuremath{\supset}'),
    ('∀', r'\ensuremath{\forall}'),
    ('∃', r'\ensuremath{\exists}'),
    ('∧', r'\ensuremath{\wedge}'),
    ('∨', r'\ensuremath{\vee}'),
    ('¬', r'\ensuremath{\neg}'),
    # Arrows
    ('←', r'\ensuremath{\leftarrow}'),
    ('→', r'\ensuremath{\rightarrow}'),
    ('↑', r'\ensuremath{\uparrow}'),
    ('↓', r'\ensuremath{\downarrow}'),
    ('↔', r'\ensuremath{\leftrightarrow}'),
    ('⇒', r'\ensuremath{\Rightarrow}'),
    ('⇐', r'\ensuremath{\Leftarrow}'),
    ('⇔', r'\ensuremath{\Leftrightarrow}'),
    # Greek letters
    ('α', r'\ensuremath{\alpha}'),
    ('β', r'\ensuremath{\beta}'),
    ('γ', r'\ensuremath{\gamma}'),
    ('δ', r'\ensuremath{\delta}'),
    ('ε', r'\ensuremath{\varepsilon}'),
    ('θ', r'\ensuremath{\theta}'),
    ('λ', r'\ensuremath{\lambda}'),
    ('μ', r'\ensuremath{\mu}'),
    ('π', r'\ensuremath{\pi}'),
    ('σ', r'\ensuremath{\sigma}'),
    ('τ', r'\ensuremath{\tau}'),
    ('φ', r'\ensuremath{\phi}'),
    ('ω', r'\ensuremath{\omega}'),
    ('Δ', r'\ensuremath{\Delta}'),
    ('Σ', r'\ensuremath{\Sigma}'),
    ('Ω', r'\ensuremath{\Omega}'),
    # Superscripts/subscripts
    ('⁻', r'\ensuremath{{}^{-}}'),
    ('⁺', r'\ensuremath{{}^{+}}'),
    ('²', r'\ensuremath{{}^{2}}'),
    ('³', r'\ensuremath{{}^{3}}'),
    ('⁰', r'\ensuremath{{}^{0}}'),
    ('¹', r'\ensuremath{{}^{1}}'),
    # Box-drawing (from ASCII art in comments)
    ('├', r'\textbar{}'),
    ('│', r'\textbar{}'),
    ('└', r'`'),
    ('┌', r'.'),
    ('┐', r'.'),
    ('┘', r"'"),
    ('─', r'-'),
    ('═', r'='),
    ('║', r'\textbar{}'),
    ('╔', r'.'),
    ('╗', r'.'),
    ('╚', r'.'),
    ('╝', r'.'),
    ('╠', r'\textbar{}'),
    ('╣', r'\textbar{}'),
    ('╦', r'-'),
    ('╩', r'-'),
    ('╬', r'+'),
    # Misc
    ('·', r'\ensuremath{\cdot}'),
    ('°', r'\ensuremath{{}^{\circ}}'),
    ('™', r'\texttrademark{}'),
    ('®', r'\textregistered{}'),
    ('©', r'\textcopyright{}'),
    ('…', r'\ldots{}'),
    ('–', r'--'),
    ('—', r'---'),
    ('‘', r'`'),   # left single quote
    ('’', r"'"),   # right single quote
    ('“', r'``'),  # left double quote
    ('”', r"''"),  # right double quote
]

def fix_line(line):
    # Protect \doxynewunicodechar{CHAR}{...} and \newunicodechar{CHAR}{...}
    # first arguments — these must stay as literal Unicode chars.
    # Strategy: split on these patterns, fix only the non-protected parts.
    import re
    parts = re.split(r'(\\(?:doxy)?newunicodechar\{[^}]*\})', line)
    result = []
    for i, part in enumerate(parts):
        if i % 2 == 1:  # protected token — preserve as-is
            result.append(part)
        else:
            for char, replacement in REPLACEMENTS:
                part = part.replace(char, replacement)
            result.append(part)
    return ''.join(result)

def fix_file(path):
    with open(path, encoding='utf-8', errors='replace') as f:
        lines = f.readlines()
    original = ''.join(lines)
    fixed_lines = [fix_line(l) for l in lines]
    content = ''.join(fixed_lines)
    # Replace any remaining non-ASCII with ? placeholder
    content = content.encode('ascii', errors='replace').decode('ascii')
    if content != original:
        with open(path, 'w', encoding='ascii') as f:
            f.write(content)
        return True
    return False

def main():
    latex_dir = sys.argv[1] if len(sys.argv) > 1 else '.'
    fixed = 0
    for root, _, files in os.walk(latex_dir):
        for fname in files:
            if fname.endswith('.tex') or fname.endswith('.sty'):
                path = os.path.join(root, fname)
                if fix_file(path):
                    fixed += 1
    print(f"Fixed {fixed} files in {latex_dir}")

if __name__ == '__main__':
    main()
