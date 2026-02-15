@echo off
setlocal

REM Build script for ClassiCube targeting Windows 2000
REM Creates a clean output folder ready to copy to a Windows 2000 machine

set OUTPUT_DIR=dist\win2000
set NEED_BUILD=0

echo Building ClassiCube for Windows 2000...

REM Extract source assets if not present
if not exist "src\audio" (
    echo Extracting audio assets...
    mkdir src\audio 2>nul
    powershell -NoProfile -Command "Expand-Archive -Path 'audio\default.zip' -DestinationPath 'src\audio' -Force"
)
if not exist "src\texpacks" (
    echo Extracting texpack assets...
    mkdir src\texpacks 2>nul
    powershell -NoProfile -Command "Expand-Archive -Path 'texpacks\Minecraft.zip' -DestinationPath 'src\texpacks' -Force"
)

REM Check if we need to rebuild (exe missing or source files newer)
if not exist "ClassiCube.exe" (
    set NEED_BUILD=1
) else (
    REM Check if any .c or .h files in src are newer than the exe
    for /f %%i in ('powershell -NoProfile -Command "$exe = (Get-Item 'ClassiCube.exe').LastWriteTime; $newer = Get-ChildItem 'src\*' -Include '*.c','*.h' | Where-Object { $_.LastWriteTime -gt $exe }; if ($newer) { 'yes' } else { 'no' }"') do (
        if "%%i"=="yes" set NEED_BUILD=1
    )
)

REM Create output directory
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
if not exist "%OUTPUT_DIR%\audio" mkdir "%OUTPUT_DIR%\audio"
if not exist "%OUTPUT_DIR%\texpacks" mkdir "%OUTPUT_DIR%\texpacks"

REM Start zip operations in background (parallel with build), track with lock files
del "%OUTPUT_DIR%\audio\.done" 2>nul
del "%OUTPUT_DIR%\texpacks\.done" 2>nul
start /b cmd /c powershell -NoProfile -Command "Compress-Archive -Path src\audio\* -DestinationPath %OUTPUT_DIR%\audio\default.zip -Force" ^& copy audio\*.ogg %OUTPUT_DIR%\audio\ ^>nul ^& echo Audio zip complete. ^& echo.^>%OUTPUT_DIR%\audio\.done
start /b cmd /c powershell -NoProfile -Command "Compress-Archive -Path src\texpacks\* -DestinationPath %OUTPUT_DIR%\texpacks\Minecraft.zip -Force" ^& echo Texpacks zip complete. ^& echo.^>%OUTPUT_DIR%\texpacks\.done

REM Clean and build only if source changed
if "%NEED_BUILD%"=="1" (
    echo Source changed, rebuilding...
    make clean >nul 2>&1
    make mingw RELEASE=1 OPT_LEVEL=3
    if errorlevel 1 (
        echo Build failed!
        exit /b 1
    )
    copy ClassiCube.exe "%OUTPUT_DIR%\"
) else (
    echo Source unchanged, skipping build.
    if not exist "%OUTPUT_DIR%\ClassiCube.exe" copy ClassiCube.exe "%OUTPUT_DIR%\"
)

REM Copy optimized options.txt for low-end hardware (only if not already present)
if not exist "%OUTPUT_DIR%\options.txt" (
    copy options.txt "%OUTPUT_DIR%\" >nul
    echo Copied default options.txt
) else (
    echo Keeping existing options.txt
)

REM Wait for zip operations to complete
:wait_zips
if not exist "%OUTPUT_DIR%\audio\.done" goto wait_zips
if not exist "%OUTPUT_DIR%\texpacks\.done" goto wait_zips
del "%OUTPUT_DIR%\audio\.done" 2>nul
del "%OUTPUT_DIR%\texpacks\.done" 2>nul

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
echo.
echo Starting ClassiCube...
echo.

cd "%OUTPUT_DIR%"
start ClassiCube.exe

endlocal
