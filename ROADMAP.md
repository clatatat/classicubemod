# ClassiCube Minecraft Feature Enhancement Roadmap

## Project Goal
Create a lightweight, playable version of Minecraft that will run on extremely limited hardware (Windows 2000+) while incorporating survival features from Minecraft Indev and Alpha versions.

## Current Status
- ✅ Removed all console/mobile/embedded platform code
- ✅ Streamlined build system for Windows and Linux only
- ✅ Cleaned up platform-specific audio/graphics/window backends

## Compiler Setup Requirements

### Windows Build Environment
You'll need one of the following:
1. **MinGW-w64** (Recommended for Windows 2000 compatibility)
   - Download from: https://www.mingw-w64.org/downloads/
   - Or use MSYS2: https://www.msys2.org/
   - Install to PATH and verify with `gcc --version`
   
2. **Visual Studio 2017 or later** (for modern Windows)
   - Download Community Edition: https://visualstudio.microsoft.com/
   - Select "Desktop development with C++"
   - Project file included: `src/ClassiCube.sln`

3. **TDM-GCC** (Alternative MinGW distribution)
   - Download from: https://jmeubank.github.io/tdm-gcc/

### Linux Build Environment
```bash
# Debian/Ubuntu
sudo apt-get install gcc make libx11-dev libxi-dev libgl1-mesa-dev libopenal-dev

# Fedora/RHEL
sudo dnf install gcc make libX11-devel libXi-devel mesa-libGL-devel openal-soft-devel

# Arch Linux
sudo pacman -S gcc make libx11 libxi mesa openal
```

### Building the Game

**Windows (MinGW):**
```bash
cd c:\dev\Classicube
make mingw
# Or for release build:
make mingw RELEASE=1
```

**Windows (Visual Studio):**
```bash
# Open src/ClassiCube.sln in Visual Studio
# Press F5 to build and run
# Or use MSBuild from command line:
msbuild src\ClassiCube.sln /p:Configuration=Release /p:Platform=x64
```

**Linux:**
```bash
cd /path/to/Classicube
make linux
# Or for release build:
make linux RELEASE=1
```

## Phase 1: Foundation & Analysis (Current Phase)

### 1.1 Code Analysis
- [ ] Study ClassiCube block system (`Block.c`, `Block.h`)
- [ ] Understand entity system (`Entity.c`, `EntityComponents.c`)
- [ ] Review world generation (`Generator.c`)
- [ ] Analyze physics system (`BlockPhysics.c`)
- [ ] Study inventory system (currently menu-based)

### 1.2 Minecraft Source Analysis
From Indev 20100223 and Alpha 1.0.6 source code, identify:
- [ ] Block properties and behavior (hardness, tool requirements)
- [ ] Item system implementation
- [ ] Crafting system architecture
- [ ] Entity AI patterns (mobs)
- [ ] Damage and health systems
- [ ] Door mechanics and redstone basics

## Phase 2: Core Survival Mechanics

### 2.1 Health & Damage System
**Source Reference:** `EntityPlayer.java`, `Entity.java` (Alpha)
- [ ] Add health property to player entity
- [ ] Implement damage types (fall, mob, drowning, fire)
- [ ] Create health bar UI element
- [ ] Add regeneration mechanics
- [ ] Implement death and respawn

### 2.2 Item System
**Source Reference:** `Item.java`, `ItemStack.java` (Alpha)
- [ ] Design item data structure (ID, count, damage/durability)
- [ ] Create item registry similar to block registry
- [ ] Implement item dropping on block break
- [ ] Add item entities (dropped items in world)
- [ ] Create item pickup mechanics

### 2.3 Inventory System Overhaul
**Source Reference:** `InventoryPlayer.java` (Alpha)
- [ ] Add survival inventory, still retain Creative mode as something that can be enabled in the Options menu
- [ ] Implement hotbar (9 slots) at bottom of screen
- [ ] Create main inventory grid (27 slots)
- [ ] Add armor slots (4 slots)
- [ ] Implement inventory GUI
- [ ] Add drag-and-drop item movement
- [ ] Implement item stacking (up to 64)

### 2.4 Block Breaking & Tool System
**Source Reference:** `Block.java`, `ItemTool.java` (Alpha)
- [ ] Add block hardness values
- [ ] Implement proper tool types (pickaxe, axe, shovel)
- [ ] Add tool effectiveness matrix
- [ ] Create mining animation/progress bar
- [ ] Implement tool durability
- [ ] Add proper block drops (stone → cobblestone)

### 2.5 Difficulty System
- [ ] Might be added after enemies are added
- [ ] Creative mode, which is just vanilla classicube with the extra features like redstone and such
- [ ] Peaceful mode, survival mode with no hostile mobs
- [ ] Easy mode, mobs are not very difficult
- [ ] Normal mode, mobs are slightly harder
- [ ] Hard mode, mobs are much harder and have increased spawn rates
- [ ] Hell mode (don't know what to call it yet), kind of like the old Survival Test updates, mobs are harder than hard mode, and can spawn during the daytime, and don't burn. There is no respawning, and if you die, the world gets deleted. This is for people who REALLY want a challenge.

## Phase 3: Crafting System

### 3.1 Crafting Mechanics
**Source Reference:** `CraftingManager.java`, `RecipesTools.java` (Alpha)
- [ ] Implement 2x2 inventory crafting grid
- [ ] Create 3x3 crafting table interface
- [ ] Design recipe data structure
- [ ] Implement recipe matching algorithm
- [ ] Add shapeless recipes support
- [ ] Create recipe registry

### 3.2 Core Recipes
Priority recipes from Minecraft Alpha:
- [ ] Wooden planks (log → 4 planks)
- [ ] Sticks (2 planks → 4 sticks)
- [ ] Crafting table
- [ ] Basic tools (wooden, stone, gold, iron, diamond)
- [ ] Torches
- [ ] Chest
- [ ] Furnace
- [ ] Doors (wooden, iron)

## Phase 4: New Blocks

### 4.1 Functional Blocks
**Source Reference:** Various Block*.java files (Alpha)

**Crafting Table**
- [ ] Add block type and texture
- [ ] Implement 3x3 crafting GUI on right-click
- [ ] Block placement and breaking

**Furnace**
- [ ] Add block type and textures (inactive/active)
- [ ] Implement smelting GUI
- [ ] Add fuel system
- [ ] Create smelting recipes (ore → ingot, sand → glass)
- [ ] Implement progress bars

**Chest**
- [ ] Add block type and texture
- [ ] Implement 27-slot storage
- [ ] Create chest GUI
- [ ] Double chest (optional)

**Doors**
- [ ] Wooden door block (2 blocks tall)
- [ ] Iron door block (2 blocks tall)
- [ ] Implement door opening/closing
- [ ] Add door interaction sound

### 4.2 Ores & Resources
- [ ] Diamond ore
- [ ] Redstone ore (dont need glow effect)
- [ ] Implement ore block drops

## Phase 5: Redstone Basics

### 5.1 Core Redstone Components
**Source Reference:** `BlockRedstone*.java` (Alpha)
- [ ] Redstone dust (wire placement)
- [ ] Redstone torch (power source / inverter)
- [ ] Implement power propagation system
- [ ] Add visual state updates (wire color changes)

### 5.2 Redstone Inputs
- [ ] Lever (manual switch)
- [ ] Button (wooden, stone)
- [ ] Pressure plate (wooden, stone)

### 5.3 Redstone Outputs
- [ ] Iron door (redstone activated)
- [ ] TNT (redstone ignition)
- [ ] Note block (basic implementation)

### 5.4 Redstone Logic
- [ ] Implement power levels (0-15)
- [ ] Wire connection algorithm
- [ ] Power source detection

## Phase 6: Mobs & AI

### 6.1 Hostile Mobs (Priority)
**Source Reference:** `EntityZombie.java`, `EntitySkeleton.java`, etc. (Alpha)

**Zombie**
- [ ] Create entity type and import model
- [ ] Implement pathfinding to player
- [ ] Add melee attack (damage player)
- [ ] Implement spawning rules (darkness)
- [ ] Add drop (feather, as the only way to craft arrows)

**Skeleton**
- [ ] Create entity type and import model
- [ ] Implement bow attack
- [ ] Add arrow entity and physics
- [ ] Implement spawning rules
- [ ] Add drops (arrows, bones)

**Creeper**
- [ ] Create entity type and import model
- [ ] Implement approach and explosion behavior
- [ ] Add explosion damage and terrain destruction
- [ ] Implement spawning rules
- [ ] Add drops (gunpowder)

**Spider**
- [ ] Create entity type and import model
- [ ] Implement wall climbing
- [ ] Add jump attack
- [ ] Implement spawning rules
- [ ] Add drops (string)

### 6.2 Passive Mobs
**Pig**
- [ ] Create entity type and import model
- [ ] Implement wandering AI
- [ ] Add drops (raw pork)

**Sheep**
- [ ] Create entity type and import model
- [ ] Implement wandering AI
- [ ] Add drops (wool)

Note: do not bother adding cows, as they are a slightly newer feature than I want to add to this, and I find them slightly annoying
Same applies to chickens

### 6.3 AI System
- [ ] Pathfinding algorithm (A* or similar)
- [ ] Line-of-sight detection
- [ ] Target selection logic
- [ ] Mob spawning system
- [ ] Day/night cycle spawning rules
- [ ] Despawning logic

## Phase 7 is not here because it listed to add features Classicube already has.

## Phase 8: Advanced Features

### 8.1 Minecarts
- [ ] kind of self-explanatory, just minecarts. Include Minecart, Minecart Chest, Minecart furnace. Make the old Alpha minecart booster work if possible, else implement powered rails.

### 8.2 Multiplayer
- [ ] probably won't add this, but if I did, players would be able to host servers or play together over LAN. Any difficulty could be multiplayer, and potentially other modes could be made for multiplayer, like PvP.

## Technical Considerations

### Performance Optimization
Since the goal is to run on limited hardware:
- Use static memory allocation where possible
- Minimize dynamic allocations
- Optimize mob AI tick rate
- Implement entity culling
- Use efficient chunk updates for redstone

### Compatibility
- Target Windows 2000+ using Windows API
- Ensure OpenGL 1.1 compatibility (software rendering fallback)
- Try to keep memory footprint low (<128MB) (definitely >256mb if nothing else)
- Support older CPUs (no SSE requirements)

### Code Structure
- Keep C-style architecture consistent with ClassiCube
- Use ClassiCube's existing systems (events, entities, blocks)
- Maintain single-file compilation model
- Follow existing naming conventions

## Development Workflow

### Iteration Strategy
1. Implement feature in isolation
2. Test thoroughly
3. Integrate with existing systems
4. Optimize for performance
5. Test on target low-end hardware

### Testing Priorities
- My old laptop running Win2k from about 1999 (512mb RAM, 1.5ghz celeron, 16mb radeon gpu)
- OpenGL 1.1 software rendering
- Memory usage monitoring

## Success Metrics
- ✅ Playable survival mode with crafting
- ✅ Basic mobs with functional AI
- ✅ Redstone circuits work
- ✅ Runs smoothly on Windows 2000 with <512MB RAM

## Resources & References

### ClassiCube Documentation
- `doc/plugin-dev.md` - Understanding the plugin system
- `doc/modules.md` - Code architecture
- `doc/strings.md` - Text rendering system
- `doc/readme.md` - General documentation

### Minecraft Source Code
- `Minecraft Indev 20100223` - Early survival mechanics
- `Minecraft Alpha 1.0.6` - Mature survival systems, redstone

### External Resources
- Minecraft Wiki (archive versions for Alpha/Indev mechanics)
- ClassiCube Forums: https://www.classicube.net/forum/
- ClassiCube Discord for development help

## Next Steps
1. **Set up build environment** - Install MinGW or Visual Studio
2. **Test current build** - Ensure ClassiCube compiles and runs
3. **Study existing code** - Focus on Block.c, Entity.c, Generator.c
4. **Start with Phase 2.1** - Implement health system as first feature
5. **Create test maps** - Build testing environments for each feature

---

*This roadmap is a living document and should be updated as development progresses.*
