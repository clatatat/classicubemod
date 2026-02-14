@echo off
REM Optimized Release Build for Windows 2000 (32-bit)
REM Run this from Windows Command Prompt or double-click it

echo ==========================================
echo ClassiCube RELEASE Build (Optimized -O3)
echo ==========================================
echo.

REM Set temp directory to avoid Windows permission issues
set TMPDIR=C:\dev\Classicube\build\tmp
set TEMP=%TMPDIR%
set TMP=%TMPDIR%
if not exist "%TMPDIR%" mkdir "%TMPDIR%"

REM Add MinGW32 and MSYS2 tools to PATH
set PATH=C:\msys64\mingw32\bin;C:\msys64\usr\bin;%PATH%

REM Navigate to project directory
cd /d C:\dev\Classicube

echo Cleaning previous build...
make clean PLAT=mingw

echo.
echo Building with -O3 optimization...
echo This will take a few minutes...
echo.

make ClassiCube PLAT=mingw RELEASE=1 OPT_LEVEL=3

if exist ClassiCube.exe (
    echo.
    echo ==========================================
    echo SUCCESS! Optimized build completed!
    echo ==========================================
    echo.
    echo File info:
    dir ClassiCube.exe | find "ClassiCube.exe"
    echo.
    echo Compilation completed succesfully.
    echo Executable: %CD%\ClassiCube.exe
) else (
    echo.
    echo ==========================================
    echo BUILD FAILED!
    echo ==========================================
    echo Check the error messages above.
    pause
    exit /b 1
)

echo.
pause
