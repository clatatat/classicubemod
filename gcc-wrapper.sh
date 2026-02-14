#!/bin/bash
export TMPDIR="/c/dev/Classicube/build/win/tmp"
export TEMP="$TMPDIR"
export TMP="$TMPDIR"
mkdir -p "$TMPDIR"
exec /c/msys64/mingw32/bin/gcc.exe "$@"
