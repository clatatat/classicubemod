#!/bin/bash
# OPTIMIZED Release build for Windows 2000 (32-bit)

echo "=================================="
echo "ClassiCube RELEASE Build (Optimized)"
echo "=================================="

export TMPDIR=/tmp/ccbuild
export TEMP=$TMPDIR
export TMP=$TMPDIR
mkdir -p $TMPDIR

export PATH="/c/msys64/mingw32/bin:$PATH"

cd /c/dev/Classicube

echo "Cleaning..."
make clean PLAT=mingw

echo ""
echo "Building with -O3 optimization..."
make ClassiCube PLAT=mingw RELEASE=1 OPT_LEVEL=3

if [ -f "ClassiCube.exe" ]; then
    echo ""
    echo "=========================================="
    echo "SUCCESS! Optimized build completed!"
    echo "=========================================="
    echo ""
    file ClassiCube.exe
    ls -lh ClassiCube.exe
    echo ""
    echo "DONE! Build complete."
else
    echo "Build failed!"
    exit 1
fi
