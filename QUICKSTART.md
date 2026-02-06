# Quick Start Guide - Building ClassiCube

## Prerequisites Installation

### Windows Users

**Option 1: MinGW-w64 (Recommended for Windows 2000 compatibility)**

1. Download MSYS2 from https://www.msys2.org/
2. Install to default location (`C:\msys64`)
3. Open "MSYS2 MinGW 64-bit" terminal
4. Update package database:
   ```bash
   pacman -Syu
   ```
5. Install development tools:
   ```bash
   pacman -S mingw-w64-x86_64-gcc make
   ```
6. Add to Windows PATH:
   - `C:\msys64\mingw64\bin`
   - `C:\msys64\usr\bin`

**Option 2: Visual Studio (Modern Windows)**

1. Download Visual Studio Community 2022 from:
   https://visualstudio.microsoft.com/downloads/
2. During installation, select:
   - "Desktop development with C++"
   - Windows 10 SDK
3. No additional setup needed

**Option 3: TDM-GCC (Standalone MinGW)**

1. Download from https://jmeubank.github.io/tdm-gcc/
2. Run installer and select "Add to PATH"
3. Verify: Open CMD and type `gcc --version`

### Linux Users

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install build-essential libx11-dev libxi-dev libgl1-mesa-dev libopenal-dev
```

**Fedora/RHEL/CentOS:**
```bash
sudo dnf install gcc make libX11-devel libXi-devel mesa-libGL-devel openal-soft-devel
```

**Arch Linux:**
```bash
sudo pacman -S base-devel libx11 libxi mesa openal
```

## Building ClassiCube

### Windows with MinGW

1. Open PowerShell or Command Prompt
2. Navigate to project directory:
   ```powershell
   cd C:\dev\Classicube
   ```
3. Build the game:
   ```bash
   make mingw
   ```
4. For optimized release build:
   ```bash
   make mingw RELEASE=1
   ```
5. Run the game:
   ```powershell
   .\ClassiCube.exe
   ```

### Windows with Visual Studio

**Using Visual Studio IDE:**
1. Open `src\ClassiCube.sln`
2. Select build configuration (Debug/Release) and platform (x64/Win32)
3. Press F5 to build and run, or Ctrl+Shift+B to build only

**Using Command Line:**
1. Open "Developer Command Prompt for VS"
2. Navigate to project:
   ```cmd
   cd C:\dev\Classicube
   ```
3. Build:
   ```cmd
   msbuild src\ClassiCube.sln /p:Configuration=Release /p:Platform=x64
   ```
4. Run:
   ```cmd
   ClassiCube.exe
   ```

### Linux

1. Open terminal
2. Navigate to project:
   ```bash
   cd /path/to/Classicube
   ```
3. Build:
   ```bash
   make linux
   ```
4. For release build:
   ```bash
   make linux RELEASE=1
   ```
5. Run:
   ```bash
   ./ClassiCube
   ```

## Build Output Locations

- **Windows MinGW:** `build/win/`
- **Linux:** `build/linux/`
- **Visual Studio:** `src/Debug/` or `src/Release/`

## Troubleshooting

### "gcc: command not found" (Windows)
- Ensure MinGW bin directory is in PATH
- Restart terminal after PATH changes
- Verify with: `echo $env:PATH` (PowerShell)

### "make: command not found" (Windows)
- Install make via MSYS2: `pacman -S make`
- Or use MinGW32-make instead: `mingw32-make mingw`

### Missing OpenGL libraries (Linux)
```bash
# Ubuntu/Debian
sudo apt-get install mesa-common-dev libgl1-mesa-dev

# Fedora
sudo dnf install mesa-libGL-devel
```

### Missing X11 libraries (Linux)
```bash
# Ubuntu/Debian
sudo apt-get install libx11-dev libxi-dev

# Fedora
sudo dnf install libX11-devel libXi-devel
```

### OpenAL errors (Linux)
```bash
# Ubuntu/Debian
sudo apt-get install libopenal-dev libopenal1

# Fedora
sudo dnf install openal-soft openal-soft-devel
```

### Visual Studio: "Platform Toolset not found"
- Open Visual Studio Installer
- Modify your installation
- Ensure "MSVC v142" or later is installed
- Ensure Windows 10 SDK is installed

## Cleaning Build Files

```bash
# Remove all build artifacts
make clean

# Or manually delete build directories
rm -rf build/
```

## Running the Game

On first run, ClassiCube will:
1. Create configuration files
2. Download required game assets from classicube.net
3. Download Minecraft Classic textures
4. Open the main menu

**Controls:**
- WASD - Movement
- Space - Jump
- E - Inventory (creative mode)
- Esc - Pause menu
- F11 - Fullscreen

## Next Steps After Building

1. **Verify the build works** - Run ClassiCube and test basic functionality
2. **Review the codebase** - Start with these files:
   - `src/Game.c` - Main game loop
   - `src/Block.c` - Block system
   - `src/Entity.c` - Entity system
   - `src/World.c` - World management
3. **Read the documentation** - Check `doc/` folder
4. **Follow the roadmap** - See `ROADMAP.md` for feature implementation plan
5. **Set up debugging**:
   - Visual Studio: Built-in debugger (F5)
   - GCC/GDB: `gdb ./ClassiCube`

## Development Tools (Optional but Recommended)

### Code Editor
- **Visual Studio Code** - Free, excellent C support
  - Install C/C++ extension
  - Install Makefile Tools extension
  
- **Visual Studio** - Full IDE with debugger
  
- **CLion** - JetBrains IDE (commercial)

### Debugging
- **GDB** (Linux/MinGW):
  ```bash
  gdb ./ClassiCube
  (gdb) run
  (gdb) backtrace  # After crash
  ```

- **Visual Studio Debugger** (Windows):
  - Set breakpoints (F9)
  - Step through code (F10/F11)
  - Watch variables
  - Memory inspector

### Version Control
Current setup assumes Git is already in use. Helpful commands:

```bash
# Create a development branch
git checkout -b feature/survival-mode

# Commit your changes
git add .
git commit -m "Added health system"

# View changes
git status
git diff
```

## Performance Testing

### Test on Low-End Hardware
To verify performance goals:
1. Run in Windows 2000/XP virtual machine
2. Limit VM RAM to 256MB or 512MB
3. Disable 3D acceleration in VM settings
4. Should maintain 30+ FPS

### Memory Profiling (Advanced)
- **Windows:** Use Visual Studio Performance Profiler
- **Linux:** Use Valgrind
  ```bash
  valgrind --leak-check=full ./ClassiCube
  ```

## Getting Help

- **ClassiCube Forums:** https://www.classicube.net/forum/
- **ClassiCube Discord:** https://classicube.net/discord
- **GitHub Issues:** https://github.com/ClassiCube/ClassiCube/issues
- **Documentation:** Check `doc/` folder in project

## Common Build Flags

```bash
# Debug build with symbols
make mingw

# Release build (optimized)
make mingw RELEASE=1

# Build with SDL2 instead of native window
make mingw BUILD_SDL2=1

# Custom optimization level
make mingw RELEASE=1 OPT_LEVEL=3

# Verbose output
make mingw VERBOSE=1
```

## File Structure Reference

```
ClassiCube/
├── src/               # All C source code
│   ├── Game.c        # Main game logic
│   ├── Block.c       # Block system
│   ├── Entity.c      # Entity system
│   ├── World.c       # World/chunk management
│   └── ...
├── doc/              # Documentation
├── misc/
│   └── windows/      # Windows resources (icons, etc.)
├── third_party/      # External libraries (BearSSL, etc.)
├── Makefile          # Build configuration
├── ROADMAP.md        # Feature development plan
└── CLEANUP_SUMMARY.md # Changes made to codebase
```

---

**You're now ready to build and start developing!** 🎮

Proceed to `ROADMAP.md` for the feature implementation plan.
