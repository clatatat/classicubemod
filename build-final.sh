#!/bin/bash
# Final build script for Windows 2000 (32-bit) ClassiCube
# Run this from MSYS2 terminal

echo "=================================="
echo "ClassiCube Windows 2000 Build"
echo "=================================="

# Set temp directory to avoid Windows permission issues
export TMPDIR=/tmp/ccbuild
export TEMP=$TMPDIR
export TMP=$TMPDIR
mkdir -p $TMPDIR

# Add MinGW32 to PATH
export PATH="/c/msys64/mingw32/bin:$PATH"

cd /c/dev/Classicube

echo "Cleaning previous build..."
make clean PLAT=mingw

echo ""
echo "Building..."
make ClassiCube PLAT=mingw

if [ -f "ClassiCube.exe" ]; then
    echo ""
    echo "=========================================="
    echo "SUCCESS! Build completed!"
    echo "=========================================="
    echo ""
    echo "Executable details:"
    file ClassiCube.exe
    ls -lh ClassiCube.exe
    echo "Done! Executable is ready for Windows 2000."
else
    echo ""
    echo "=========================================="
    echo "Build failed - check errors above"
    echo "=========================================="
    exit 1
fi
