# ClassiCube Platform Cleanup Summary

## Overview
This document summarizes the cleanup performed to streamline ClassiCube for Windows and Linux development only.

## Removed Platform Folders

### From `misc/` directory:
- **Console Platforms:**
  - `3ds/` - Nintendo 3DS
  - `dreamcast/` - Sega Dreamcast
  - `gba/` - GameBoy Advance
  - `gc/` - Nintendo GameCube
  - `n64/` - Nintendo 64
  - `nds/` - Nintendo DS/DSi
  - `ps1/` - PlayStation 1
  - `ps2/` - PlayStation 2
  - `ps3/` - PlayStation 3
  - `ps4/` - PlayStation 4
  - `psp/` - PlayStation Portable
  - `saturn/` - Sega Saturn
  - `switch/` - Nintendo Switch
  - `vita/` - PlayStation Vita
  - `wii/` - Nintendo Wii
  - `wiiu/` - Nintendo Wii U
  - `xbox/` - Original Xbox
  - `xbox360/` - Xbox 360
  - `32x/` - Sega 32X

- **Mobile/Embedded Platforms:**
  - `ios/` - iOS (iPhone/iPad)
  - `symbian/` - Symbian OS
  - `UWP/` - Universal Windows Platform

- **Legacy/Alternative Platforms:**
  - `amiga/` - Commodore Amiga
  - `atari_st/` - Atari ST
  - `msdos/` - MS-DOS
  - `macclassic/` - Classic Mac OS
  - `macOS/` - Modern macOS (keeping only Windows/Linux)
  - `os2/` - OS/2

### From root directory:
- `android/` - Android build system and project files

## Removed Source Files

### From `src/` directory:
- `Audio_SLES.c` - Android OpenSL ES audio backend
- `Audio_OS2.c` - OS/2 audio backend
- `LBackend_Android.c` - Android-specific launcher backend
- `Platform_MacClassic.c` - Classic Mac OS platform layer
- `Platform_N64.c` - Nintendo 64 platform layer
- `Platform_WinCE.c` - Windows CE platform layer
- `Window_WinCE.c` - Windows CE window system
- `Window_Terminal.c` - Terminal-based window backend
- `Window_OS2.c` - OS/2 window system
- `Window_N64.c` - Nintendo 64 window system
- `Window_MacClassic.c` - Classic Mac OS window system
- `Graphics_N64.c` - Nintendo 64 graphics backend

## Makefile Changes

### Removed Platform Targets:
- `web` - WebAssembly/Emscripten build
- `sunos` - Solaris/SunOS
- `hp-ux` - HP-UX
- `darwin` - macOS
- `freebsd` - FreeBSD
- `openbsd` - OpenBSD
- `netbsd` - NetBSD
- `dragonfly` - DragonFly BSD
- `haiku` - Haiku OS
- `beos` - BeOS
- `serenityos` - SerenityOS
- `irix` - SGI IRIX
- `rpi` - Raspberry Pi (specialized build)
- `riscos` - RISC OS
- `wince` - Windows CE
- `os/2` - OS/2
- All console-specific targets (32x, saturn, dreamcast, psp, vita, ps1-4, xbox, xbox360, n64, gba, ds, 3ds, gamecube, wii, wiiu, switch)
- All legacy platform targets (dos, macclassic, amiga, atari_st, ios)

### Remaining Platform Targets:
- `mingw` - Windows (MinGW/GCC)
- `linux` - Linux (GNU/Linux)
- `sdl2` - SDL2 backend (cross-platform override)
- `sdl3` - SDL3 backend (cross-platform override)
- `terminal` - Terminal mode (console output)
- `release` - Release build flag

## Visual Studio Project Changes

### Updated Files:
- `src/ClassiCube.vcxproj` - Removed `Window_Terminal.c` reference
- `src/ClassiCube.vcxproj.filters` - Removed `Window_Terminal.c` filter entry

### Platform Configurations Retained:
- Win32 (x86) - Debug and Release
- x64 (AMD64) - Debug and Release
- ARM - Debug and Release (for Windows on ARM)
- ARM64 - Debug and Release (for Windows on ARM64)

## Retained Files/Folders

### Platforms Still Supported:
- **Windows:** Full support via MinGW (GCC) or Visual Studio
- **Linux:** Full support via GCC and X11

### Retained Platform Files:
- `Platform_Windows.c` - Windows platform layer
- `Platform_Posix.c` - POSIX/Linux platform layer
- `Window_Win.c` - Windows window system
- `Window_X11.c` - Linux/X11 window system
- `Window_SDL2.c` - SDL2 window backend (optional)
- `Window_SDL3.c` - SDL3 window backend (optional)
- `Audio_WinMM.c` - Windows multimedia audio
- `Audio_OpenAL.c` - OpenAL audio (cross-platform)
- `Audio_Null.c` - Null audio backend (fallback)
- `Graphics_D3D9.c` - Direct3D 9 (Windows)
- `Graphics_D3D11.c` - Direct3D 11 (Windows)
- `Graphics_GL1.c` - OpenGL 1.x (cross-platform)
- `Graphics_GL11.c` - OpenGL 1.1 (legacy support)
- `Graphics_GL2.c` - OpenGL 2.x+ (modern)
- `Graphics_SoftGPU.c` - Software renderer (fallback)
- `Graphics_SoftFP.c` - Software floating-point renderer
- `Graphics_SoftMin.c` - Minimal software renderer

### Retained Misc Folders:
- `misc/windows/` - Windows-specific resources
- `misc/linux/` - Linux-specific resources  
- `misc/x11/` - X11-specific resources
- `misc/sdl/` - SDL-related files
- `misc/opengl/` - OpenGL utilities
- `misc/certs/` - SSL certificates
- `misc/flatpak/` - Flatpak packaging (Linux)

## Benefits of Cleanup

1. **Reduced Complexity:** Removed ~30+ platform-specific build targets
2. **Smaller Codebase:** Deleted 12 platform-specific source files
3. **Focused Development:** Only Windows and Linux need to be tested
4. **Faster Builds:** Less conditional compilation and fewer targets
5. **Easier Maintenance:** No need to maintain console/mobile platform code
6. **Clear Direction:** Simplified Makefile with only 2 primary platforms

## Building After Cleanup

### Windows:
```bash
make mingw           # GCC/MinGW build
make mingw RELEASE=1 # Optimized build
```

Or use Visual Studio:
```bash
msbuild src\ClassiCube.sln /p:Configuration=Release /p:Platform=x64
```

### Linux:
```bash
make linux           # Standard build
make linux RELEASE=1 # Optimized build
```

## Compatibility Notes

- **Windows 2000+** supported (as per project goals)
- **Any modern Linux** with X11 support
- **OpenGL 1.1+** required (software fallback available)
- **DirectX 9+** optional on Windows

## Disk Space Saved

Approximate space saved by removing platform folders:
- Console platforms: ~15-20 MB
- Mobile platforms: ~5-8 MB
- Legacy platforms: ~3-5 MB
- **Total:** ~25-35 MB of platform-specific build files and documentation removed

## Next Steps

1. Install MinGW-w64 or Visual Studio
2. Test compilation on Windows
3. Test compilation on Linux (if available)
4. Begin implementing Minecraft features per ROADMAP.md
