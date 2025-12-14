#!/usr/bin/env python3
"""
Convert Doxygen XML output to individual AsciiDoc files (flat structure).

This script parses Doxygen's XML output and generates one AsciiDoc file per
source file, placing all output files in a flat directory structure.

Usage:
    python3 doxygen-xml-to-asciidoc.py <xml_input_dir> <asciidoc_output_dir>
"""

#   StarForth — Steady-State Virtual Machine Runtime
#
#   Copyright (c) 2023–2025 Robert A. James
#   All rights reserved.
#
#   This file is part of the StarForth project.
#
#   Licensed under the StarForth License, Version 1.0 (the "License");
#   you may not use this file except in compliance with the License.
#
#   You may obtain a copy of the License at:
#       https://github.com/star.4th@proton.me/StarForth/LICENSE.txt
#
#   This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
#   express or implied, including but not limited to the warranties of
#   merchantability, fitness for a particular purpose, and noninfringement.
#
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
#  StarForth — Steady-State Virtual Machine Runtime
#   Copyright (c) 2023–2025 Robert A. James
#   All rights reserved.
#
#   This file is part of the StarForth project.
#
#   Licensed under the StarForth License, Version 1.0 (the "License");
#   you may not use this file except in compliance with the License.
#
#   You may obtain a copy of the License at:
#        https://github.com/star.4th@proton.me/StarForth/LICENSE.txt
#
#   This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
#   express or implied, including but not limited to the warranties of
#   merchantability, fitness for a particular purpose, and noninfringement.
#
#   See the License for the specific language governing permissions and
#   limitations under the License.
#

#                                   ***   StarForth   ***
#
#   doxygen-xml-to-asciidoc.py- FORTH-79 Standard and ANSI C99 ONLY
#   Modified by - rajames
#   Last modified - 2025-10-26T16:05:12.167-04
#
#
#
#   See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
#
#   /home/rajames/CLionProjects/StarForth/scripts/doxygen-xml-to-asciidoc.py

#                                   ***   StarForth   ***
#
#   doxygen-xml-to-asciidoc.py- FORTH-79 Standard and ANSI C99 ONLY
#   Modified by - rajames
#   Last modified - 2025-10-26T15:51:32.242-04
#
#
#
#   See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
#
#   /home/rajames/CLionProjects/StarForth/scripts/doxygen-xml-to-asciidoc.py

import sys
import os
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Dict, List, Optional, Set


class DoxygenXMLParser:
    """Parse Doxygen XML output and extract documentation."""

    def __init__(self, xml_dir: str):
        """Initialize parser with Doxygen XML directory."""
        self.xml_dir = Path(xml_dir)
        self.index_xml = self.xml_dir / "index.xml"

        if not self.index_xml.exists():
            raise FileNotFoundError(f"Doxygen index.xml not found at {self.index_xml}")

        self.tree = ET.parse(str(self.index_xml))
        self.root = self.tree.getroot()

    def get_source_files(self) -> List[Dict]:
        """Extract all source files from Doxygen index."""
        files = []

        # Find all file compounds in the index
        for compound in self.root.findall(".//compound[@kind='file']"):
            file_info = {
                'refid': compound.get('refid'),
                'name': compound.findtext('name', ''),
                'path': compound.findtext('path', ''),
                'language': compound.get('language', 'c'),
            }

            # Only include C/H files
            if file_info['name'].endswith(('.c', '.h', '.asm')):
                files.append(file_info)

        return sorted(files, key=lambda x: x['name'])

    def extract_file_documentation(self, refid: str) -> Optional[str]:
        """Extract documentation for a specific file by refid."""
        file_xml = self.xml_dir / f"{refid}.xml"

        if not file_xml.exists():
            return None

        try:
            tree = ET.parse(str(file_xml))
            root = tree.getroot()

            # Find the compounddef element
            compounddef = root.find(".//compounddef")
            if compounddef is None:
                return None

            doc = []

            # Extract detailed description
            detailed = compounddef.find(".//detailedDescription")
            if detailed is not None:
                doc.append(self._extract_description(detailed))

            # Extract function/variable documentation
            doc.extend(self._extract_members(compounddef))

            return "\n".join(filter(None, doc))

        except Exception as e:
            print(f"Error parsing {file_xml}: {e}", file=sys.stderr)
            return None

    def _extract_description(self, desc_elem) -> str:
        """Extract text from a description element."""
        text_parts = []

        for para in desc_elem.findall(".//para"):
            para_text = self._extract_para_text(para)
            if para_text:
                text_parts.append(para_text)

        return "\n".join(text_parts)

    def _extract_para_text(self, para_elem) -> str:
        """Extract text content from a paragraph element."""
        text = []

        if para_elem.text:
            text.append(para_elem.text)

        for child in para_elem:
            if child.tag == 'ref':
                text.append(child.text or '')
                if child.tail:
                    text.append(child.tail)
            elif child.tag == 'emphasis':
                text.append(f"_{child.text or ''}_")
                if child.tail:
                    text.append(child.tail)
            elif child.tag == 'bold':
                text.append(f"*{child.text or ''}*")
                if child.tail:
                    text.append(child.tail)
            elif child.tag == 'computeroutput':
                text.append(f"`{child.text or ''}`")
                if child.tail:
                    text.append(child.tail)
            else:
                if child.text:
                    text.append(child.text)
                if child.tail:
                    text.append(child.tail)

        return "".join(text).strip()

    def _extract_members(self, compounddef) -> List[str]:
        """Extract member (function/variable) documentation."""
        members = []

        sectiondef_list = compounddef.findall(".//sectiondef")

        for sectiondef in sectiondef_list:
            kind = sectiondef.get('kind', '')

            # Create section header based on kind
            section_header = self._get_section_header(kind)
            if section_header:
                members.append(f"\n== {section_header}\n")

            for member in sectiondef.findall(".//memberdef"):
                member_text = self._extract_member(member)
                if member_text:
                    members.append(member_text)

        return members

    def _get_section_header(self, kind: str) -> Optional[str]:
        """Map Doxygen section kind to header text."""
        headers = {
            'public-type': 'Public Types',
            'public-func': 'Public Functions',
            'public-attrib': 'Public Attributes',
            'protected-type': 'Protected Types',
            'protected-func': 'Protected Functions',
            'protected-attrib': 'Protected Attributes',
            'private-type': 'Private Types',
            'private-func': 'Private Functions',
            'private-attrib': 'Private Attributes',
            'user-defined': 'User Defined',
        }
        return headers.get(kind)

    def _extract_member(self, memberdef) -> str:
        """Extract documentation for a single member."""
        parts = []

        name = memberdef.findtext('name', '')
        kind = memberdef.get('kind', '')

        if not name:
            return ""

        # Create member header
        parts.append(f"=== {name}")
        parts.append("")

        # Add member type/signature info
        if kind == 'function':
            returns = memberdef.findtext(".//type", '')
            params = memberdef.findall(".//param")
            param_list = []
            for param in params:
                param_type = param.findtext('type', '')
                param_name = param.findtext('declname', '')
                if param_type and param_name:
                    param_list.append(f"{param_type} {param_name}")

            if returns or param_list:
                sig = f"{returns}(" if returns else "("
                sig += ", ".join(param_list) + ")"
                parts.append(f"[source,c]\n----\n{sig}\n----")
                parts.append("")

        # Add documentation
        briefdesc = memberdef.find(".//briefDescription")
        if briefdesc is not None:
            brief_text = self._extract_description(briefdesc)
            if brief_text:
                parts.append(brief_text)
                parts.append("")

        detaileddesc = memberdef.find(".//detailedDescription")
        if detaileddesc is not None:
            detailed_text = self._extract_description(detaileddesc)
            if detailed_text:
                parts.append(detailed_text)
                parts.append("")

        return "\n".join(parts)


def generate_asciidoc_file(filename: str, title: str, content: str, all_files: Optional[List[str]] = None) -> str:
    """Generate AsciiDoc file content with header and content."""
    doc = []

    # Navigation breadcrumb
    doc.append("xref:index.adoc[← Back to API Documentation Index]")
    doc.append("")

    doc.append(f"= {title}")
    doc.append(":toc: left")
    doc.append(":toc-title: Contents")
    doc.append(":toclevels: 2")
    doc.append("")
    doc.append(f"*File:* `{title}`")
    doc.append("")

    # Find related files (header for source, source for header)
    related = []
    if filename.endswith('.c'):
        header_file = filename.replace('.c', '.h') + '.adoc'
        if all_files and header_file in all_files:
            related.append(f"xref:{header_file}[Header file ({filename.replace('.c', '.h')})]")
    elif filename.endswith('.h'):
        source_file = filename.replace('.h', '.c') + '.adoc'
        if all_files and source_file in all_files:
            related.append(f"xref:{source_file}[Implementation file ({filename.replace('.h', '.c')})]")

    if related:
        doc.append("[TIP]")
        doc.append("====")
        doc.append("*Related files:*")
        doc.append("")
        for rel in related:
            doc.append(f"* {rel}")
        doc.append("====")
        doc.append("")

    doc.append("[NOTE]")
    doc.append("====")
    doc.append("Auto-generated from Doxygen documentation.")
    doc.append("====")
    doc.append("")

    if content:
        doc.append(content)
    else:
        doc.append("_No documentation available for this file._")

    # Navigation footer
    doc.append("")
    doc.append("---")
    doc.append("")
    doc.append("xref:index.adoc[← Back to API Documentation Index]")

    return "\n".join(doc)


def main():
    """Main entry point."""
    if len(sys.argv) != 3:
        print("Usage: doxygen-xml-to-asciidoc.py <xml_input_dir> <asciidoc_output_dir>")
        sys.exit(1)

    xml_dir = sys.argv[1]
    output_dir = sys.argv[2]

    # Validate input directory
    if not os.path.isdir(xml_dir):
        print(f"Error: XML input directory not found: {xml_dir}", file=sys.stderr)
        sys.exit(1)

    # Create output directory
    Path(output_dir).mkdir(parents=True, exist_ok=True)

    try:
        # Parse Doxygen XML
        parser = DoxygenXMLParser(xml_dir)
        files = parser.get_source_files()

        if not files:
            print("Warning: No source files found in Doxygen XML output", file=sys.stderr)

        # Generate AsciiDoc files
        generated_count = 0
        index_entries = []
        total_files = len(files)

        # Build set of all output filenames for cross-referencing
        all_output_files = {f['name'] + '.adoc' for f in files}

        for idx, file_info in enumerate(files, 1):
            filename = file_info['name']
            refid = file_info['refid']

            # Extract documentation
            doc_content = parser.extract_file_documentation(refid)

            # Generate AsciiDoc file
            title = f"{filename}"
            asciidoc_content = generate_asciidoc_file(filename, title, doc_content or "", all_output_files)

            # Write output file (flat structure)
            # Replace extension while preserving the source file type (e.g., vm.c.adoc, vm.h.adoc)
            output_filename = filename + '.adoc'
            output_path = Path(output_dir) / output_filename

            with open(output_path, 'w') as f:
                f.write(asciidoc_content)

            generated_count += 1
            index_entries.append({
                'filename': output_filename,
                'title': filename,
            })

            # Progress indicator
            progress = (idx / total_files) * 100
            print(f"[{idx}/{total_files}] {progress:5.1f}% - {output_filename}", file=sys.stderr)

        # Generate index file
        if index_entries:
            index_content = generate_index(index_entries)
            index_path = Path(output_dir) / "index.adoc"
            with open(index_path, 'w') as f:
                f.write(index_content)
            print(f"Generated: index.adoc", file=sys.stderr)

        print(f"Successfully generated {generated_count} AsciiDoc files", file=sys.stderr)
        sys.exit(0)

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


def generate_index(entries: List[Dict]) -> str:
    """Generate an index AsciiDoc file."""
    index = []

    # Navigation breadcrumb
    index.append("xref:../README.adoc[← Back to Documentation Index]")
    index.append("")

    index.append("= API Documentation Index")
    index.append(":toc: left")
    index.append(":toc-title: Contents")
    index.append("")
    index.append("Auto-generated from Doxygen documentation.")
    index.append("")
    index.append("== Source Files")
    index.append("")

    for entry in entries:
        index.append(f"* xref:{entry['filename']}[{entry['title']}]")

    # Navigation footer
    index.append("")
    index.append("---")
    index.append("")
    index.append("xref:../README.adoc[← Back to Documentation Index]")

    return "\n".join(index)


if __name__ == "__main__":
    main()
