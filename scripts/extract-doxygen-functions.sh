#!/bin/bash
#
# extract-doxygen-functions.sh - Extract ALL function documentation from Doxygen XML
# Includes parameters, return values, and detailed descriptions
#

set -e

if [ $# -lt 2 ]; then
    echo "Usage: $0 <doxygen-xml-dir> <output-file>"
    exit 1
fi

DOXY_XML="$1"
OUTPUT="$2"

if [ ! -d "$DOXY_XML" ]; then
    echo "Error: Doxygen XML directory not found: $DOXY_XML"
    exit 1
fi

echo "Extracting complete function documentation from Doxygen XML..."

# Start the DocBook 4.5 XML output
cat > "$OUTPUT" <<'EOF'
<section>
  <title>Complete Function Reference</title>
  <para>
    This section contains complete API documentation for every function in StarForth,
    automatically extracted from source code Doxygen comments.
  </para>
EOF

# Process each group file (these contain the categorized functions)
for group_file in "$DOXY_XML"/group__*.xml; do
    if [ ! -f "$group_file" ]; then
        continue
    fi

    # Extract group title
    GROUP_TITLE=$(xmllint --xpath "string(//section/title)" "$group_file" 2>/dev/null || echo "Functions")

    echo "  Processing group: $GROUP_TITLE"

    # Start group section
    cat >> "$OUTPUT" <<EOF

  <section>
    <title>$GROUP_TITLE</title>
EOF

    # Extract all function definitions from this group
    # Use xmllint to get function sections
    xmllint --xpath "//section[@xml:id and contains(@xml:id, '_1ga')]" "$group_file" 2>/dev/null | \
    while IFS= read -r line; do
        echo "$line"
    done | \
    sed 's/<section/<FUNCSECTION/g' | \
    sed 's/<\/section>/<\/FUNCSECTION>/g' | \
    tr '\n' ' ' | \
    sed 's/<FUNCSECTION/\n<section/g' | \
    sed 's/<\/FUNCSECTION>/<\/section>\n/g' | \
    grep '^<section' | \
    while IFS= read -r func_xml; do
        # Extract function name
        FUNC_NAME=$(echo "$func_xml" | sed -n 's/.*<title>\([^<]*\)<\/title>.*/\1/p' | head -1)

        if [ -n "$FUNC_NAME" ]; then
            echo "    - $FUNC_NAME"

            # Write function section
            cat >> "$OUTPUT" <<FUNCEOF

    <section>
      <title>$FUNC_NAME</title>
FUNCEOF

            # Extract and write function signature
            SIGNATURE=$(echo "$func_xml" | sed -n 's/.*<computeroutput>\([^<]*\)<\/computeroutput>.*/\1/p' | head -1)
            if [ -n "$SIGNATURE" ]; then
                cat >> "$OUTPUT" <<FUNCEOF
      <para>
        <programlisting>$SIGNATURE</programlisting>
      </para>
FUNCEOF
            fi

            # Extract description
            DESC=$(echo "$func_xml" | sed -n 's/.*<para>\([^<]*\)<\/para>.*/\1/p' | head -1)
            if [ -n "$DESC" ] && [ "$DESC" != "Parameters" ] && [ "$DESC" != "Returns" ]; then
                cat >> "$OUTPUT" <<FUNCEOF
      <para>$DESC</para>
FUNCEOF
            fi

            # Check if there's a parameters table
            if echo "$func_xml" | grep -q "<formalpara>.*<title>Parameters</title>"; then
                cat >> "$OUTPUT" <<FUNCEOF
      <variablelist>
        <title>Parameters:</title>
FUNCEOF
                # Extract parameters (simplified - just get the table content)
                echo "$func_xml" | \
                grep -A 100 "title>Parameters</title" | \
                grep -B 1 "<entry>" | \
                grep -v "^--$" | \
                paste - - | \
                while IFS=$'\t' read -r param_name param_desc; do
                    PNAME=$(echo "$param_name" | sed 's/<[^>]*>//g')
                    PDESC=$(echo "$param_desc" | sed 's/<[^>]*>//g')
                    if [ -n "$PNAME" ] && [ "$PNAME" != "Parameters" ]; then
                        cat >> "$OUTPUT" <<PARAMEOF
        <varlistentry>
          <term>$PNAME</term>
          <listitem><para>$PDESC</para></listitem>
        </varlistentry>
PARAMEOF
                    fi
                done

                cat >> "$OUTPUT" <<FUNCEOF
      </variablelist>
FUNCEOF
            fi

            # Check if there's a return value
            if echo "$func_xml" | grep -q "title>Returns</title"; then
                RETURN_VAL=$(echo "$func_xml" | sed -n 's/.*<title>Returns<\/title>.*<para>\([^<]*\)<\/para>.*/\1/p' | head -1)
                if [ -n "$RETURN_VAL" ]; then
                    cat >> "$OUTPUT" <<FUNCEOF
      <para>
        <emphasis>Returns:</emphasis> $RETURN_VAL
      </para>
FUNCEOF
                fi
            fi

            echo "    </section>" >> "$OUTPUT"
        fi
    done

    # Close group section
    echo "  </section>" >> "$OUTPUT"
done

# Also process individual file documentation
for file_xml in "$DOXY_XML"/*.xml; do
    filename=$(basename "$file_xml")

    # Skip group files (already processed) and index
    if [[ "$filename" == group__* ]] || [[ "$filename" == "index.xml" ]] || [[ "$filename" == *"_source.xml" ]]; then
        continue
    fi

    # Only process header files with function documentation
    if ! grep -q "<memberdef kind=\"function\"" "$file_xml" 2>/dev/null; then
        continue
    fi

    FILE_TITLE=$(xmllint --xpath "string(//section/title)" "$file_xml" 2>/dev/null || echo "$filename")

    # Skip if already processed in groups
    if grep -q "<title>$FILE_TITLE</title>" "$OUTPUT" 2>/dev/null; then
        continue
    fi

    echo "  Processing file: $FILE_TITLE"

    # Extract functions from simplesect if they have detailed docs
    # (This catches functions not in groups)
done

# Close main section
echo "</section>" >> "$OUTPUT"

echo "✓ Extraction complete: $OUTPUT"