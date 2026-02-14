# ClassiCube Mod - Build Instructions for Windows 2000 (32-bit)

## Overview
This is a modified version of ClassiCube with custom features including:
- New terrain generation types and themes (Notchy, Floating, Caves, themed variants)
- Redstone system with full power distribution
- Mob system (6 types: Pig, Sheep, Creeper, Spider, Zombie, Skeleton)
- Arrow projectile system
- Damage system and mob AI
- Rotational block system (directional torches, doors, etc.)
- New blocks: Cactus, Ladder, Door, Redstone dust/torches, Diamond block, etc.
- Mob variations (different colors/sizes)
- Ultra-widescreen and very low resolution support
- D3D9 fog fix for very low-spec graphics cards

**Target Platform:** Windows 2000 and later (32-bit)

## Codebase Analysis Summary

### Architecture
- **Language:** Pure C (C99 features)
- **Build System:** Makefile-based
- **Total Source Files:** 109 core C files + third-party libraries
- **Lines of Code:** ~500,000+
- **License:** BSD-3 Clause

### Key Components

#### 1. Block System ([Block.c](Block.c), [BlockPhysics.c](BlockPhysics.c))
- 256 block types (extensible to 511 with `EXTENDED_BLOCKS`)
- Property arrays for collision, rendering, textures, lighting, speed modifiers
- Directional blocks with 8 variants (4 wall directions + ground/ceiling)
- Redstone power distribution system
- TNT explosions with configurable power

#### 2. Terrain Generation ([Generator.c](Generator.c))
- **Generators:** Flatgrass, Notchy (Minecraft Classic Perlin noise), Floating Islands, Caves, Empty
- **Themes:** Normal, Hell, Paradise, Woods, Desert
- Improved Perlin noise implementation
- Multi-threading support for non-blocking generation

#### 3. Mob System ([InputHandler.c](InputHandler.c), [Entity.c](Entity.c))
- **6 Mob Types:**
  - Passive: Pig, Sheep (10 HP)
  - Hostile: Creeper, Spider, Zombie, Skeleton (20 HP)
- **AI Features:**
  - Line-of-sight detection
  - Pathfinding toward player
  - Aggression distance-based (16 block aggro, 24 block deaggro)
  - Knockback from damage
  - Drowning in water
- **Arrow System:**
  - 32 max simultaneous arrows
  - Gravity and velocity physics
  - 3 damage to mobs, 4 to player
  - Stick-in-block collision detection

#### 4. Rendering ([MapRenderer.c](MapRenderer.c), Graphics_*.c)
- **Backends:** D3D9, D3D11, OpenGL 1.0/1.1/2.0+, Software renderers
- **D3D9 Fix:** Automatic vertex fog fallback for DX7-era cards
- Chunk-based rendering (16x16x16 blocks)
- Aspect ratio support for ultra-widescreen
- Resolution-agnostic (works from 320x240 to 4K+)

#### 5. Physics & Redstone ([BlockPhysics.c](BlockPhysics.c))
- Gravity for sand, gravel, TNT
- Liquid flow with priority queues
- Redstone power system with torch variants (ON/OFF, 8 directions)
- Door/button/lever mechanics

### Windows 2000 Compatibility
- **Compiler Flags:** `-D_WIN32_WINNT=0x0500 -DWINVER=0x0500`
- **Architecture:** `-march=i686` (Pentium Pro and later)
- **Subsystem Version:** 5.0 (Windows 2000)
- **Minimal Headers:** Uses custom minimal Windows headers in `/misc/windows/`

## Building

### Prerequisites
1. **MSYS2** with MinGW32 toolchain
   - Download from: https://www.msys2.org/
   - Install the i686 (32-bit) toolchain: `pacman -S mingw-w64-i686-gcc`

2. **Required packages:**
   ```bash
   pacman -S mingw-w64-i686-gcc make
   ```

### Build Methods

#### Method 1: MSYS2 MinGW 32-bit Terminal (Recommended)
1. Open **MSYS2 MinGW 32-bit** terminal from Start Menu
2. Navigate to project directory:
   ```bash
   cd /c/dev/Classicube
   ```
3. Run the build script:
   ```bash
   ./build-win2000.sh
   ```

#### Method 2: Manual Make Command
1. Open MSYS2 MinGW 32-bit terminal
2. Navigate to project:
   ```bash
   cd /c/dev/Classicube
   ```
3. Set environment variables:
   ```bash
   export TMPDIR="/c/dev/Classicube/build/tmp"
   export TEMP="$TMPDIR"
   export TMP="$TMPDIR"
   mkdir -p "$TMPDIR"
   ```
4. Build:
   ```bash
   make ClassiCube PLAT=mingw
   ```

#### Method 3: Windows Batch File
1. Open Windows Command Prompt
2. Navigate to project:
   ```cmd
   cd C:\dev\Classicube
   ```
3. Run:
   ```cmd
   build.bat
   ```

### Build Output
- **Executable:** `ClassiCube.exe` (in project root)
- **Distribution Copy:** `classicube executables/v1.2.0-pre0/ClassiCube.exe`
- **Build Artifacts:** `build/win/` directory

### Makefile Configuration
The Makefile is configured for Windows 2000 compatibility:
```makefile
ifeq ($(PLAT),mingw)
    CC      = C:/msys64/mingw32/bin/gcc.exe
    OEXT    = .exe
    CFLAGS  = -DUNICODE -D_WIN32_WINNT=0x0500 -DWINVER=0x0500 -march=i686 -pipe
    LDFLAGS = -g -Wl,--major-subsystem-version,5 -Wl,--minor-subsystem-version,0 -pipe
    LIBS    = -mwindows -lwinmm
    BUILD_DIR = build/win
endif
```

### Optimization Flags
- **Debug Build:** `make ClassiCube PLAT=mingw` (default, includes `-g`)
- **Release Build:** `make ClassiCube PLAT=mingw RELEASE=1` (adds `-O1` optimization)
- **Change Optimization Level:** Set `OPT_LEVEL=2` or `OPT_LEVEL=3` for more aggressive optimization

## Troubleshooting

### Issue: GCC Cannot Create Temporary Files
**Symptom:** `Cannot create temporary file in C:\Windows\: Permission denied`

**Solution:** Set TMPDIR to a writable location before building:
```bash
export TMPDIR="/c/dev/Classicube/build/tmp"
export TEMP="$TMPDIR"
export TMP="$TMPDIR"
mkdir -p "$TMPDIR"
```

### Issue: Missing DLL Files
**Symptom:** Executable doesn't run, complains about missing DLLs

**Solution:** Copy MinGW32 runtime DLLs to executable directory:
```bash
cp /c/msys64/mingw32/bin/libgcc_s_dw2-1.dll .
cp /c/msys64/mingw32/bin/libwinpthread-1.dll .
```

Or build with static linking (larger executable but no DLL dependencies):
```bash
make ClassiCube PLAT=mingw EXTRA_LDFLAGS="-static-libgcc"
```

### Issue: Compilation Errors in MSYS2 Bash
**Symptom:** GCC fails silently with no error output

**Solution:** Use the **MSYS2 MinGW 32-bit terminal** instead of MSYS2 MSYS terminal or regular bash. The MinGW32 environment is specifically configured for Windows-native compilation.

## Testing on Windows 2000
1. Copy `ClassiCube.exe` to a Windows 2000 machine
2. Ensure DirectX 9.0c or OpenGL drivers are installed
3. Run the executable
4. If it fails to start, check dependencies with Dependency Walker (depends.exe)

## Adding New Features

### Adding a New Block
1. Define block ID in [BlockID.h](BlockID.h)
2. Register block in [Block.c](Block.c) `Block_Init()`
3. Set properties (collision, draw type, textures, etc.)
4. Add physics behavior in [BlockPhysics.c](BlockPhysics.c) if needed

### Adding a New Mob Type
1. Add mob index in [InputHandler.c](InputHandler.c) (search for `#define MOB_IDX_`)
2. Create entity model in [Model.c](Model.c)
3. Add spawn logic in `InputHandler.c` mob spawning functions
4. Implement AI behavior in mob tick functions

### Modifying Terrain Generation
1. Edit generator functions in [Generator.c](Generator.c)
2. Modify theme application in generator `Setup()` functions
3. Adjust noise parameters for different terrain features

## Performance Considerations
- **Target Hardware:** Pentium III 500MHz, 128MB RAM, DirectX 7 GPU
- **Rendering:** Uses D3D9 vertex fog for compatibility with old GPUs
- **Optimization:** Chunk-based rendering reduces draw calls
- **Memory:** Low-memory builds available with `CC_BUILD_LOWMEM`

## Future Enhancements
Based on the codebase analysis, these features are architected but could be expanded:
- [ ] Survival mode (damage to player, hunger, health regeneration)
- [ ] Crafting system
- [ ] Inventory management improvements
- [ ] More mob types (Enderman, etc.)
- [ ] Biome-based mob spawning
- [ ] Structure generation (villages, dungeons)
- [ ] Block drops and collection
- [ ] Mod variants (baby mobs, aggressive vs passive states)
- [ ] Day/night cycle affecting mob spawning
- [ ] Sound effects for all mobs

## License
BSD-3 Clause (see original ClassiCube repository for details)

## Credits
- Based on ClassiCube by UnknownShadow200
- Terrain generation improvements
- Mob system implementation
- Redstone mechanics
- Custom block additions
