@echo off
setlocal

REM Build script for ClassiCube targeting Windows 2000
REM Creates a clean output folder ready to copy to a Windows 2000 machine

set OUTPUT_DIR=dist\win2000

echo Building ClassiCube for Windows 2000...

REM Clean and build
make clean >nul 2>&1
make mingw RELEASE=1
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

REM Create output directory
if exist "%OUTPUT_DIR%" rmdir /s /q "%OUTPUT_DIR%"
mkdir "%OUTPUT_DIR%"

REM Copy executable
copy ClassiCube.exe "%OUTPUT_DIR%\"

echo.
echo ========================================
echo Build complete!
echo Output folder: %OUTPUT_DIR%
echo.
echo Contents:
dir /b "%OUTPUT_DIR%"
echo.
echo Copy the contents of %OUTPUT_DIR% to your Windows 2000 machine.
echo On first run, ClassiCube will download required assets automatically.
echo ========================================

endlocal
