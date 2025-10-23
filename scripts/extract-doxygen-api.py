#!/usr/bin/env python3
"""
extract-doxygen-api.py - Extract complete API documentation from Doxygen XML

Generates DocBook 4.5 XML with ALL functions including parameters, return values,
and detailed descriptions.

Parses Doxygen's native XML format (not DocBook).
"""

#                                   ***   StarForth   ***
#
#   extract-doxygen-api.py- FORTH-79 Standard and ANSI C99 ONLY
#   Modified by - rajames
#   Last modified - 2025-10-23T10:55:25.243-04
#
#   Copyright (c) 2025 (rajames) Robert A. James - StarshipOS Forth Project.
#
#   This work is released into the public domain under the Creative Commons Zero v1.0 Universal license.
#   To the extent possible under law, the author(s) have dedicated all copyright and related
#   and neighboring rights to this software to the public domain worldwide.
#   This software is distributed without any warranty.
#
#   See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
#
#   /home/rajames/CLionProjects/StarForth/scripts/extract-doxygen-api.py

#                                   ***   StarForth   ***
#
#    extract-doxygen-api.py- FORTH-79 Standard and ANSI C99 ONLY
#    Modified by - rajames
#   Last modified - 2025-10-23T10:52:05.473-04
#
#
#
#    See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
#
#    /home/rajames/CLionProjects/StarForth/scripts/extract-doxygen-api.py

#                                   ***   StarForth   ***
#
#    extract-doxygen-api.py- FORTH-79 Standard and ANSI C99 ONLY
#    Modified by - rajames
#   Last modified - Thursday 23 October 2025 10:19
#
#
#
#    See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
#
#    /home/rajames/CLionProjects/StarForth/scripts/extract-doxygen-api.py

#                                   ***   StarForth   ***
#
#    extract-doxygen-api.py- FORTH-79 Standard and ANSI C99 ONLY
#    Modified by - rajames
#   Last modified - 10/18/25, 11:45 AM
#
#
#
#    See <http://creativecommons.org/publicdomain/zero/1.0/> for more information.
#
#    /home/rajames/CLionProjects/StarForth/scripts/extract-doxygen-api.py

import sys
import os
import xml.etree.ElementTree as ET
from pathlib import Path


def escape_xml(text):
    """Escape special XML characters"""
    if text is None:
        return ""
    return (text.replace('&', '&amp;')
            .replace('<', '&lt;')
            .replace('>', '&gt;')
            .replace('"', '&quot;')
            .replace("'", '&apos;'))


def get_text_content(elem):
    """Extract all text content from an element"""
    if elem is None:
        return ""
    text = elem.text or ""
    for child in elem:
        text += get_text_content(child)
        if child.tail:
            text += child.tail
    return text.strip()


def process_para(para_elem):
    """Process a para element and return DocBook text"""
    if para_elem is None:
        return ""

    text = para_elem.text or ""
    for child in para_elem:
        if child.tag.endswith('}para') or child.tag == 'para':
            text += process_para(child)
        elif child.tag.endswith('}computeroutput') or child.tag == 'computeroutput':
            text += f"<literal>{get_text_content(child)}</literal>"
        elif child.tag.endswith('}link') or child.tag == 'link':
            text += get_text_content(child)
        else:
            text += get_text_content(child)

        if child.tail:
            text += child.tail

    return text.strip()


def extract_function_doc(memberdef):
    """Extract complete documentation for a function from Doxygen memberdef element"""
    func_doc = {}

    # Get function name
    name = memberdef.find('name')
    if name is not None:
        func_doc['name'] = name.text

    # Build signature from type + definition + argsstring
    type_elem = memberdef.find('type')
    definition = memberdef.find('definition')
    argsstring = memberdef.find('argsstring')

    if definition is not None and argsstring is not None:
        func_doc['signature'] = f"{get_text_content(definition)}{argsstring.text or ''}"

    # Get brief description
    brief = memberdef.find('briefdescription/para')
    if brief is not None:
        func_doc['description'] = get_text_content(brief)

    # Get parameters from detailed description
    params = []
    paramlist = memberdef.find('.//parameterlist[@kind="param"]')
    if paramlist is not None:
        for paramitem in paramlist.findall('parameteritem'):
            param_name_elem = paramitem.find('.//parametername')
            param_desc_elem = paramitem.find('.//parameterdescription/para')
            if param_name_elem is not None and param_desc_elem is not None:
                param_name = param_name_elem.text or ""
                param_desc = get_text_content(param_desc_elem)
                if param_name and param_desc:
                    params.append((param_name, param_desc))

    if params:
        func_doc['parameters'] = params

    # Get return value
    simplesect_return = memberdef.find('.//simplesect[@kind="return"]')
    if simplesect_return is not None:
        return_para = simplesect_return.find('para')
        if return_para is not None:
            func_doc['returns'] = get_text_content(return_para)

    return func_doc


def process_group_file(xml_file):
    """Process a Doxygen group XML file in native Doxygen XML format"""
    try:
        tree = ET.parse(xml_file)
        root = tree.getroot()

        # Get group title from compounddef
        compounddef = root.find('compounddef')
        if compounddef is None:
            return None, []

        title_elem = compounddef.find('title')
        group_title = title_elem.text if title_elem is not None else "Functions"

        # Find all function memberdefs
        functions = []
        for sectiondef in compounddef.findall('sectiondef'):
            # Only process function sections
            if sectiondef.get('kind') == 'func':
                for memberdef in sectiondef.findall('memberdef'):
                    # Only process functions
                    if memberdef.get('kind') == 'function':
                        func_doc = extract_function_doc(memberdef)
                        if func_doc.get('name'):
                            functions.append(func_doc)

        return group_title, functions

    except Exception as e:
        print(f"Warning: Error processing {xml_file}: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return None, []


def generate_docbook(groups, output_file):
    """Generate DocBook 4.5 XML with all function documentation"""
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        f.write('<section>\n')
        f.write('  <title>Complete Function Reference</title>\n')
        f.write('  <para>\n')
        f.write('    This section contains complete API documentation for every function in StarForth,\n')
        f.write('    automatically extracted from source code Doxygen comments.\n')
        f.write('  </para>\n\n')

        for group_title, functions in groups:
            if not functions:
                continue

            f.write(f'  <section>\n')
            f.write(f'    <title>{escape_xml(group_title)}</title>\n')
            f.write(f'    <para>Functions in the {escape_xml(group_title)} module.</para>\n\n')

            for func in functions:
                f.write(f'    <section>\n')
                f.write(f'      <title>{escape_xml(func.get("name", "Unknown"))}</title>\n')

                # Signature
                if 'signature' in func:
                    f.write(f'      <para>\n')
                    f.write(f'        <programlisting>{escape_xml(func["signature"])}</programlisting>\n')
                    f.write(f'      </para>\n')

                # Description
                if 'description' in func:
                    f.write(f'      <para>{func["description"]}</para>\n')

                # Parameters
                if 'parameters' in func:
                    f.write(f'      <variablelist>\n')
                    f.write(f'        <title>Parameters:</title>\n')
                    for param_name, param_desc in func['parameters']:
                        f.write(f'        <varlistentry>\n')
                        f.write(f'          <term>{escape_xml(param_name)}</term>\n')
                        f.write(f'          <listitem><para>{param_desc}</para></listitem>\n')
                        f.write(f'        </varlistentry>\n')
                    f.write(f'      </variablelist>\n')

                # Returns
                if 'returns' in func:
                    f.write(f'      <para>\n')
                    f.write(f'        <emphasis role="bold">Returns:</emphasis> {func["returns"]}\n')
                    f.write(f'      </para>\n')

                f.write(f'    </section>\n\n')

            f.write(f'  </section>\n\n')

        f.write('</section>\n')


def main():
    if len(sys.argv) < 3:
        print("Usage: extract-doxygen-api.py <doxygen-xml-dir> <output-file>")
        sys.exit(1)

    doxy_dir = Path(sys.argv[1])
    output_file = Path(sys.argv[2])

    if not doxy_dir.is_dir():
        print(f"Error: Doxygen XML directory not found: {doxy_dir}")
        sys.exit(1)

    print("Extracting complete function documentation from Doxygen XML...")

    # Process all group files
    groups = []
    for xml_file in sorted(doxy_dir.glob('group__*.xml')):
        print(f"  Processing: {xml_file.name}")
        group_title, functions = process_group_file(xml_file)
        if group_title and functions:
            groups.append((group_title, functions))
            print(f"    Found {len(functions)} functions")

    # Generate DocBook output
    print(f"\nGenerating DocBook output: {output_file}")
    generate_docbook(groups, output_file)
    total_funcs = sum([len(g[1]) for g in groups])
    print(f"✓ Extraction complete: {total_funcs} functions documented")


if __name__ == '__main__':
    main()
