#!/bin/bash
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

# IDE Integration Setup for StarshipOS
# Run this script to set up IDE features for the custom build system

set -e

echo "=== StarshipOS IDE Integration Setup ==="
echo

# Check if Bear is installed
if ! command -v bear &> /dev/null; then
    echo "Installing Bear (compilation database generator)..."
    sudo apt-get update
    sudo apt-get install -y bear
    echo "✓ Bear installed successfully"
else
    echo "✓ Bear is already installed"
fi

echo

# Create IDE file generation script
echo "Creating IDE file generation script..."
cat > scripts/generate-ide-files.sh << 'EOF'
#!/bin/bash
# Generate IDE files for StarshipOS
# This script creates compile_commands.json for IDE integration

echo "=== Generating IDE Files for StarshipOS ==="
echo

# Clean previous builds to ensure complete compilation database
echo "Cleaning previous builds..."
make clean-all

echo "Generating compilation database with Bear..."
echo "Building x86_64 with verbose output..."
bear -- make all V=1

echo
if [ -f compile_commands.json ]; then
    entries=$(jq length compile_commands.json 2>/dev/null || wc -l < compile_commands.json)
    echo "✓ compile_commands.json generated successfully with $entries entries"
    echo "✓ IDEs can now provide code completion, navigation, and error checking"
else
    echo "✗ Failed to generate compile_commands.json"
    exit 1
fi

echo
echo "=== Setup Complete ==="
echo "To use with your IDE:"
echo "  CLion: File → Open → select compile_commands.json"
echo "  VSCode: C/C++ extension will auto-detect the file"
echo "  Vim/Neovim: Configure clangd to use the compilation database"
echo
EOF

chmod +x scripts/generate-ide-files.sh
echo "✓ Created scripts/generate-ide-files.sh"

echo

# Add compile_commands.json to .gitignore if not already there
if ! grep -q "compile_commands.json" .gitignore 2>/dev/null; then
    echo "compile_commands.json" >> .gitignore
    echo "✓ Added compile_commands.json to .gitignore"
else
    echo "✓ compile_commands.json already in .gitignore"
fi

echo

# Create .clangd configuration for better IDE experience
echo "Creating .clangd configuration..."
cat > .clangd << 'EOF'
# Clangd configuration for StarshipOS
CompileFlags:
  Add:
    - -Wall
    - -Wextra
    - -D__l4__
  Remove:
    - -mno-red-zone
    - -fno-exceptions
    - -fno-rtti
Diagnostics:
  ClangTidy:
    Add: [performance-*, readability-*]
    Remove: [readability-magic-numbers, readability-isolate-declaration]
EOF
echo "✓ Created .clangd configuration"

echo

# Create VSCode workspace settings
mkdir -p .vscode
cat > .vscode/settings.json << 'EOF'
{
    "C_Cpp.default.compileCommands": "${workspaceFolder}/compile_commands.json",
    "C_Cpp.default.intelliSenseMode": "linux-gcc-x64",
    "C_Cpp.default.cStandard": "c99",
    "C_Cpp.default.cppStandard": "c++17",
    "files.associations": {
        "*.h": "c",
        "*.c": "c"
    },
    "makefile.extensionOutputFolder": "./.vscode",
    "makefile.launchConfigurations": [
        {
            "cwd": "${workspaceFolder}",
            "binaryPath": "${workspaceFolder}/l4/build-x86_64/bin/x86_64_K8-fiasco/starforth",
            "binaryArgs": []
        }
    ],
    "makefile.buildTarget": "all",
    "makefile.configurations": [
        {
            "name": "Default",
            "makeArgs": ["V=1"]
        }
    ]
}
EOF
echo "✓ Created .vscode/settings.json"

echo

echo "=== Next Steps ==="
echo "1. Wait for your current build to finish"
echo "2. Run: ./scripts/generate-ide-files.sh"
echo "3. Open compile_commands.json in CLion or restart your IDE"
echo "4. Enjoy full IDE features with your custom build system!"
echo
echo "The generated compile_commands.json will enable:"
echo "  ✓ Code completion and IntelliSense"
echo "  ✓ Go to definition/declaration"
echo "  ✓ Find references and usages"
echo "  ✓ Real-time error checking"
echo "  ✓ Refactoring tools"
echo "  ✓ Symbol navigation"
echo