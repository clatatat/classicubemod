#!/bin/bash
# Build script for Windows 2000 (32-bit) ClassiCube
# Run this from MSYS2 MinGW32 terminal: Start Menu -> MSYS2 MinGW 32-bit

echo "Building ClassiCube for Windows 2000 (32-bit)..."
echo "==========================================="

# Set up environment
export TMPDIR="/c/dev/Classicube/build/tmp"
export TEMP="$TMPDIR"
export TMP="$TMPDIR"
mkdir -p "$TMPDIR"

# Clean previous build
echo "Cleaning previous build..."
rm -rf build/win
rm -f ClassiCube.exe

# Build
echo "Compiling..."
make ClassiCube PLAT=mingw -j4

# Check result
if [ -f "ClassiCube.exe" ]; then
    echo ""
    echo "==========================================="
    echo "BUILD SUCCESSFUL!"
    echo "==========================================="
    echo "Executable: $(pwd)/ClassiCube.exe"
    echo "File info:"
    file ClassiCube.exe
    ls -lh ClassiCube.exe

    # Copy to distribution folder
    mkdir -p "classicube executables/v1.2.0-pre0/"
    cp ClassiCube.exe "classicube executables/v1.2.0-pre0/ClassiCube.exe"
    echo "Copied to: classicube executables/v1.2.0-pre0/ClassiCube.exe"
else
    echo ""
    echo "==========================================="
    echo "BUILD FAILED!"
    echo "==========================================="
    echo "Check the error messages above"
    exit 1
fi
