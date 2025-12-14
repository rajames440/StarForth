#!/usr/bin/env bash
#
#  StarForth — Steady-State Virtual Machine Runtime
#
#  Copyright (c) 2023–2025 Robert A. James
#  All rights reserved.
#
#  This file is part of the StarForth project.
#
#  Licensed under the StarForth License, Version 1.0 (the "License");
#  you may not use this file except in compliance with the License.
#
#  You may obtain a copy of the License at:
#      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt
#
#  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
#  express or implied, including but not limited to the warranties of
#  merchantability, fitness for a particular purpose, and noninfringement.
#
# See the License for the specific language governing permissions and
# limitations under the License.
#
# StarForth — Steady-State Virtual Machine Runtime
#  Copyright (c) 2023–2025 Robert A. James
#  All rights reserved.
#
#  This file is part of the StarForth project.
#
#  Licensed under the StarForth License, Version 1.0 (the "License");
#  you may not use this file except in compliance with the License.
#
#  You may obtain a copy of the License at:
#       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt
#
#  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
#  express or implied, including but not limited to the warranties of
#  merchantability, fitness for a particular purpose, and noninfringement.
#
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
#

set -e

# ==========================================================
#  BUILD PATENT DOCUMENT SKELETON FOR STARFORTH / SSM PATENT
# ==========================================================

# Default target directory
TARGET_DIR="${1:-./patent}"

echo "🚀 Creating patent document structure in: ${TARGET_DIR}"
mkdir -p "${TARGET_DIR}"

# ------------------------------
# Main directory tree
# ------------------------------
mkdir -p "${TARGET_DIR}/sections"
mkdir -p "${TARGET_DIR}/figures"
mkdir -p "${TARGET_DIR}/tables"
mkdir -p "${TARGET_DIR}/macros"
mkdir -p "${TARGET_DIR}/bibliography"

touch "${TARGET_DIR}/figures/.gitkeep"
touch "${TARGET_DIR}/tables/.gitkeep"
touch "${TARGET_DIR}/bibliography/.gitkeep"

# ------------------------------
# Main TeX file
# ------------------------------
cat > "${TARGET_DIR}/main.tex" << 'EOF'
\documentclass[12pt]{article}

\usepackage{graphicx}
\usepackage{hyperref}
\usepackage{amsmath}
\usepackage{amssymb}
\usepackage{enumitem}
\usepackage{geometry}
\geometry{margin=1in}

\input{macros/commands.tex}

\begin{document}

\input{sections/01_title}
\input{sections/02_field}
\input{sections/03_background}
\input{sections/04_summary}
\input{sections/05_drawings}
\input{sections/06_detailed_description}
\input{sections/07_embodiments}
\input{sections/08_claims}
\input{sections/09_validation}
\input{sections/10_implementation}
\input{sections/11_appendix}

\end{document}
EOF

# ------------------------------
# Section placeholder files
# ------------------------------
declare -a sections=(
  "01_title"
  "02_field"
  "03_background"
  "04_summary"
  "05_drawings"
  "06_detailed_description"
  "07_embodiments"
  "08_claims"
  "09_validation"
  "10_implementation"
  "11_appendix"
)

for sec in "${sections[@]}"; do
    cat > "${TARGET_DIR}/sections/${sec}.tex" << EOF
% ===========================================
% ${sec}.tex
% Auto-generated placeholder
% ===========================================

\\section*{${sec//_/ }}

% TODO: Populate this section.
EOF
done

# ------------------------------
# Macros
# ------------------------------
cat > "${TARGET_DIR}/macros/commands.tex" << 'EOF'
% =====================================================
%  Custom LaTeX commands for the SSM / StarForth Patent
% =====================================================

\newcommand{\ssm}{\textbf{Steady State Machine}}
\newcommand{\lone}{L1\_HeatTracking}
\newcommand{\ltwo}{L2\_Window}
\newcommand{\lthree}{L3\_Decay}
\newcommand{\lfour}{L4\_Pipeline}
\newcommand{\lfive}{L5\_WindowInfluence}
\newcommand{\lsix}{L6\_DecayInfluence}
\newcommand{\lseven}{L7\_Heartbeat}
\newcommand{\leight}{L8\_AdaptiveSelector}
EOF

# ------------------------------
# Default Makefile
# ------------------------------
cat > "${TARGET_DIR}/Makefile" << 'EOF'
LATEX=pdflatex
MAIN=main.tex

all:
	$(LATEX) $(MAIN)
	$(LATEX) $(MAIN)

clean:
	rm -f *.aux *.log *.toc *.out *.pdf
EOF

echo "✅ Patent skeleton creation complete."
echo "Open ${TARGET_DIR}/main.tex in TeXstudio to begin writing."
