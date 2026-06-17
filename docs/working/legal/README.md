# docs/working/legal/ — Patent-Adjacent Material (HOLD)

**Status:** This directory is intentionally empty pending Bob's instruction.

Per the ground rule in `docs/CLAUDE.md` — *"Ask before touching legal/. Anything
patent-adjacent gets flagged for Bob's explicit instruction before moving or
editing."* — the following files were **NOT moved** during the Phase 2
reorganization. They remain in `docs/` exactly where they were.

## Flagged, awaiting instruction

| Path (unchanged) | What it is |
|------------------|------------|
| docs/patent/ (entire tree) | USPTO provisional patent: `main.tex` + `sections/`, compiled PDFs, USPTO forms (sb0015a/sb0016), `final_provisional/`, revision logs. |
| docs/DARPA_SSM_Proposal_James.docx | DARPA SSM grant proposal (Word). |
| docs/DARPA_SSM_Proposal_James.pdf | DARPA SSM grant proposal (PDF). |
| docs/SSM_DARPA_Complete_Proposal_James_FINAL.docx | DARPA SSM complete/final proposal (Word, "FINAL"). |
| docs/SSM_DARPA_Summary_Slide.pptx | DARPA SSM summary slide deck. |
| docs/SYSTEM_NARRATIVE.tex | Technical narrative written for patent counsel and reviewers. |
| docs/SYSTEM_NARRATIVE.pdf | Compiled system narrative. |

## Also deferred this pass (not legal, but left in place for safety)

These are LaTeX sources with coupled relative figure paths; moving them blindly
risks breaking `\includegraphics`. Deferred to a later, build-verified pass.

| Path (unchanged) | What it is |
|------------------|------------|
| docs/ontology.tex / docs/ontology.pdf | Formal ontology LaTeX source + compiled PDF (strong `formal/` candidate). |
| docs/ontology_diagrams.dot | Graphviz source for ontology diagrams. |
| docs/fig_*.png / docs/fig_*.svg | Shared figure assets referenced by the ontology and/or system-narrative LaTeX. |

When ready, tell Claude how to handle the patent/DARPA material (e.g. "move the
patent tree into docs/working/legal/" or "leave it at docs/patent/").
