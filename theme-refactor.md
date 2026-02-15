# Centralize World Generation Theme System

## Context

Theme checks (`if (Gen_Theme == GEN_THEME_X)`) are scattered across 50+ locations in Generator.c, plus BlockPhysics.c. The same environment color blocks are copy-pasted identically across 4 generator Setup() functions. This makes themes hard to modify and hard to add new ones.

**Goal:** Define all theme properties in a central `GenThemeData` struct + lookup table. Most scattered if/else checks become simple field lookups.

---

## Architecture Overview

```
                    +------------------------------------------+
                    |        Gen_Themes[GEN_THEME_COUNT]       |
                    |          (central theme table)            |
                    |                                          |
                    |  [NORMAL] [HELL] [PARADISE] [WOODS] ... |
                    |   .surfaceBlock = GRASS                  |
                    |   .fillBlock    = DIRT                   |
                    |   .fluidBlock   = STILL_WATER            |
                    |   .skyCol       = 0 (default)            |
                    |   .treePatchMul = 1                      |
                    |   .dirtToGrass  = true                   |
                    |   ... (all theme properties)             |
                    +------------------+-----------------------+
                                       |
                          Gen_Theme selects index
                                       |
              +------------------------+------------------------+
              |                        |                        |
              v                        v                        v
   +-------------------+   +--------------------+   +-------------------+
   | GenTheme_Apply-   |   | Direct field       |   | Flag-gated code   |
   | Environment()     |   | lookups             |   | paths             |
   |                   |   |                    |   |                   |
   | Sets sky, fog,    |   | t->surfaceBlock    |   | if (t->plantsCacti)|
   | clouds, shadow,   |   | t->fluidBlock      |   | if (t->hasOases)  |
   | edge, sides       |   | t->heightScale     |   | if (t->hasSnow-   |
   | colors/blocks     |   | t->treePatchMul    |   |     Layer)        |
   +--------+----------+   | t->treePlantMsg    |   +--------+----------+
            |               +--------+-----------+            |
            |                        |                        |
   +--------v----------+   +--------v-----------+   +--------v----------+
   | Called by each     |   | Used throughout    |   | Gates inline      |
   | generator's        |   | Generator.c in     |   | generation code   |
   | Setup() function   |   | place of if/else   |   | (oases, cacti,    |
   |                    |   | chains             |   |  snow, shadow      |
   | - FlatgrassGen     |   |                    |   |  ceiling, etc.)   |
   | - NotchyGen        |   | Also used by:      |   |                   |
   | - FloatingGen      |   | BlockPhysics.c     |   | Complex per-theme |
   | - CavesGen         |   | (dirtToGrass)      |   | logic stays inline|
   +--------------------+   +--------------------+   +-------------------+

  Remaining inline theme checks (can't be data-driven):
  +------------------------------------------------------------------+
  | NotchyGen surface smoothing (672-711)                            |
  |   - 5 different code paths with unique noise thresholds          |
  |   - Paradise beach logic, Winter ICE checks                     |
  | NotchyGen ore-in-dirt (326-328)                                  |
  |   - Cross-checks Gen_Theme AND gen_active generator type         |
  +------------------------------------------------------------------+
```

### Data Flow

```
  User selects theme in menu (Menus.c)
          |
          v
  Gen_Theme = s->worldTheme    (int index 0-5)
          |
          v
  Gen_Start() kicks off generation
          |
          +---> Generator reads Gen_Themes[Gen_Theme].field
          |     instead of doing if (Gen_Theme == X) everywhere
          |
          +---> Generator's Setup() calls GenTheme_ApplyEnvironment()
          |     which reads colors/blocks from the theme table
          |
          +---> BlockPhysics.c reads Gen_Themes[Gen_Theme].dirtToGrass
                for runtime dirt-to-grass conversion
```

---

## Files to Modify

- `classicubemod/src/Generator.h` - Add struct definition, extern array, helper prototype
- `classicubemod/src/Generator.c` - Add themes array + helper, replace ~40 theme checks
- `classicubemod/src/BlockPhysics.c` - Replace 1 theme check (line 2289)

---

## Step 1: Add `GenThemeData` struct to Generator.h

Add `#include "PackedCol.h"` at top. Define struct after the `GEN_THEME_*` defines:

```c
struct GenThemeData {
    /* Terrain blocks */
    BlockRaw surfaceBlock;    /* grass / sand / dirt / snowy_grass */
    BlockRaw fillBlock;       /* dirt / sand */
    BlockRaw fluidBlock;      /* still_water / still_lava (internal flood fill) */
    BlockRaw edgeFluidBlock;  /* still_water / still_lava / ice (border flood) */

    /* Edge/sides overrides (0 = let generator decide its own default) */
    BlockRaw edgeBlock;       /* hell=lava, else 0 */
    BlockRaw sidesBlock;      /* hell=obsidian, else 0 */

    /* Cave-specific blocks */
    BlockRaw caveFillBlock;   /* stone / dirt */
    BlockRaw gardenSurface;   /* grass / sand */
    BlockRaw gardenFill;      /* dirt / sand */

    /* Environment colors (0 = keep engine defaults) */
    PackedCol skyCol;
    PackedCol fogCol;
    PackedCol cloudsCol;
    PackedCol shadowCol;

    /* Generation multipliers */
    float heightScale;        /* 1.0 normal, 0.5 paradise/desert */
    int   treePatchMul;       /* 1 normal, 8 woods */
    int   flowerPatchMul;     /* 1 normal, 3 paradise */

    /* Feature flags */
    cc_bool hasShadowCeiling;
    cc_bool hasSnowLayer;
    cc_bool dirtToGrass;       /* dirt physics converts to grass when lit */
    cc_bool hasCaveGardens;
    cc_bool plantsCacti;       /* plant cacti instead of trees */
    cc_bool generateFlowers;
    cc_bool hasExtraCaveOres;  /* cobble + mossy in hell caves */
    cc_bool treesOnDirt;       /* trees can grow on dirt (hell) */
    cc_bool raiseWaterLevel;   /* raise water by Height/8 (paradise) */
    cc_bool hasOases;          /* generate oasis patches (desert) */

    /* Status messages */
    const char* treePlantMsg;
    const char* edgeFloodMsg;
    const char* internalFloodMsg;
};

extern const struct GenThemeData Gen_Themes[GEN_THEME_COUNT];
void GenTheme_ApplyEnvironment(void);
```

---

## Step 2: Add themes array + helper to Generator.c

Near top of file after globals (~line 14):

```c
const struct GenThemeData Gen_Themes[GEN_THEME_COUNT] = {
    /* GEN_THEME_NORMAL (0) */
    {
        BLOCK_GRASS, BLOCK_DIRT,                           /* surfaceBlock, fillBlock */
        BLOCK_STILL_WATER, BLOCK_STILL_WATER,              /* fluidBlock, edgeFluidBlock */
        0, 0,                                              /* edgeBlock, sidesBlock (generator default) */
        BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT,              /* caveFillBlock, gardenSurface, gardenFill */
        0, 0, 0, 0,                                        /* sky, fog, clouds, shadow (defaults) */
        1.0f, 1, 1,                                        /* heightScale, treePatchMul, flowerPatchMul */
        false, false, true, true, false, true, false, false, false, false,
        "Planting trees", "Flooding edge water", "Flooding water"
    },
    /* GEN_THEME_HELL (1) */
    {
        BLOCK_DIRT, BLOCK_DIRT,
        BLOCK_STILL_LAVA, BLOCK_STILL_LAVA,
        BLOCK_STILL_LAVA, BLOCK_OBSIDIAN,
        BLOCK_DIRT, BLOCK_GRASS, BLOCK_DIRT,               /* gardenSurface/Fill unused since hasCaveGardens=false */
        PackedCol_Make(0x80, 0x10, 0x10, 0xFF),            /* skyCol - dark red */
        PackedCol_Make(0x18, 0x14, 0x14, 0xFF),            /* fogCol - very dark red */
        PackedCol_Make(0x30, 0x28, 0x28, 0xFF),            /* cloudsCol - dark brown-red */
        PackedCol_Make(0x42, 0x41, 0x41, 0xFF),            /* shadowCol - dark gray */
        1.0f, 1, 1,
        true, false, false, false, false, true, true, true, false, false,
        "Planting trees", "Flooding edge lava", "Flooding lava"
    },
    /* GEN_THEME_PARADISE (2) */
    {
        BLOCK_GRASS, BLOCK_DIRT,
        BLOCK_STILL_WATER, BLOCK_STILL_WATER,
        0, 0,
        BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT,
        0, 0, 0, 0,
        0.5f, 1, 3,                                        /* flat terrain, 3x flowers */
        false, false, true, true, false, true, false, false, true, false,
        "Planting trees", "Flooding edge water", "Flooding water"
    },
    /* GEN_THEME_WOODS (3) */
    {
        BLOCK_GRASS, BLOCK_DIRT,
        BLOCK_STILL_WATER, BLOCK_STILL_WATER,
        0, 0,
        BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT,
        0, 0, 0, 0,
        1.0f, 8, 1,                                        /* 8x trees */
        false, false, true, true, false, true, false, false, false, false,
        "Planting trees", "Flooding edge water", "Flooding water"
    },
    /* GEN_THEME_DESERT (4) */
    {
        BLOCK_SAND, BLOCK_SAND,
        BLOCK_STILL_WATER, BLOCK_STILL_WATER,
        0, 0,
        BLOCK_STONE, BLOCK_SAND, BLOCK_SAND,               /* caves: sand gardens */
        PackedCol_Make(0xD4, 0xB8, 0x70, 0xFF),            /* skyCol - golden tan */
        PackedCol_Make(0xD4, 0xA5, 0x50, 0xFF),            /* fogCol - sandstorm */
        PackedCol_Make(0xE0, 0xC8, 0x90, 0xFF),            /* cloudsCol - light golden */
        0,
        0.5f, 1, 1,                                        /* flat terrain */
        false, false, true, true, true, false, false, false, false, true,
        "Planting cacti", "Flooding edge water", "Flooding water"
    },
    /* GEN_THEME_WINTER (5) */
    {
        BLOCK_SNOWY_GRASS, BLOCK_DIRT,
        BLOCK_STILL_WATER, BLOCK_ICE,                      /* edge flooding uses ice */
        0, 0,
        BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT,
        PackedCol_Make(0xC0, 0xD8, 0xF0, 0xFF),            /* skyCol - light blue */
        PackedCol_Make(0xE0, 0xE8, 0xF0, 0xFF),            /* fogCol - very light blue */
        PackedCol_Make(0xF0, 0xF0, 0xF0, 0xFF),            /* cloudsCol - white */
        0,
        1.0f, 1, 1,
        false, true, true, true, false, true, false, false, false, false,
        "Planting trees", "Flooding edge water", "Flooding water"
    }
};

void GenTheme_ApplyEnvironment(void) {
    const struct GenThemeData* t = &Gen_Themes[Gen_Theme];
    if (t->skyCol)    Env_SetSkyCol(t->skyCol);
    if (t->fogCol)    Env_SetFogCol(t->fogCol);
    if (t->cloudsCol) Env_SetCloudsCol(t->cloudsCol);
    if (t->shadowCol) Env_SetShadowCol(t->shadowCol);
    if (t->edgeBlock)  Env_SetEdgeBlock(t->edgeBlock);
    if (t->sidesBlock) Env_SetSidesBlock(t->sidesBlock);
}
```

---

## Step 3: Replace 4 duplicated Setup() functions

**Before** (repeated identically in FlatgrassGen_Setup, NotchyGen_Setup, FloatingGen_Setup, CavesGen_Setup):
```c
static void FlatgrassGen_Setup(void) {
    if (Gen_Theme == GEN_THEME_HELL) {
        Env_SetSkyCol(PackedCol_Make(0x80, 0x10, 0x10, 0xFF));
        Env_SetFogCol(PackedCol_Make(0x18, 0x14, 0x14, 0xFF));
        Env_SetCloudsCol(PackedCol_Make(0x30, 0x28, 0x28, 0xFF));
        Env_SetShadowCol(PackedCol_Make(0x42, 0x41, 0x41, 0xFF));
        Env_SetEdgeBlock(BLOCK_STILL_LAVA);
        Env_SetSidesBlock(BLOCK_OBSIDIAN);
    } else if (Gen_Theme == GEN_THEME_DESERT) {
        Env_SetSkyCol(PackedCol_Make(0xD4, 0xB8, 0x70, 0xFF));
        Env_SetFogCol(PackedCol_Make(0xD4, 0xA5, 0x50, 0xFF));
        Env_SetCloudsCol(PackedCol_Make(0xE0, 0xC8, 0x90, 0xFF));
    } else if (Gen_Theme == GEN_THEME_WINTER) {
        Env_SetSkyCol(PackedCol_Make(0xC0, 0xD8, 0xF0, 0xFF));
        Env_SetFogCol(PackedCol_Make(0xE0, 0xE8, 0xF0, 0xFF));
        Env_SetCloudsCol(PackedCol_Make(0xF0, 0xF0, 0xF0, 0xFF));
    }
}
```

**After:**
```c
static void FlatgrassGen_Setup(void) {
    GenTheme_ApplyEnvironment();
}

static void NotchyGen_Setup(void) {
    GenTheme_ApplyEnvironment();
}

/* FloatingGen/CavesGen keep their generator-specific defaults, then apply theme */
static void FloatingGen_Setup(void) {
    Env_SetEdgeBlock(BLOCK_AIR);
    Env_SetSidesBlock(BLOCK_AIR);
    Env_SetCloudsHeight(-16);
    GenTheme_ApplyEnvironment();
}

static void CavesGen_Setup(void) {
    Env_SetEdgeBlock(BLOCK_BEDROCK);
    Env_SetSidesBlock(BLOCK_BEDROCK);
    Env_SetCloudsHeight(-16);
    GenTheme_ApplyEnvironment();
}
```

---

## Step 4: Replace block/multiplier lookups

**Before** (FlatgrassGen_Generate, lines 143-171):
```c
if (Gen_Theme == GEN_THEME_HELL) {
    surfaceBlock = BLOCK_DIRT;
    fillBlock    = BLOCK_DIRT;
} else if (Gen_Theme == GEN_THEME_DESERT) {
    surfaceBlock = BLOCK_SAND;
    fillBlock    = BLOCK_SAND;
} else if (Gen_Theme == GEN_THEME_WINTER) {
    surfaceBlock = BLOCK_SNOWY_GRASS;
    fillBlock    = BLOCK_DIRT;
} else {
    surfaceBlock = BLOCK_GRASS;
    fillBlock    = BLOCK_DIRT;
}
// ... later ...
if (Gen_Theme == GEN_THEME_WINTER) {
    FlatgrassGen_MapSet(World.Height / 2, World.Height / 2, BLOCK_SNOW);
}
if (Gen_Theme == GEN_THEME_HELL) Gen_PlaceShadowCeiling();
```

**After:**
```c
const struct GenThemeData* t = &Gen_Themes[Gen_Theme];
BlockRaw surfaceBlock = t->surfaceBlock;
BlockRaw fillBlock    = t->fillBlock;
// ... use surfaceBlock/fillBlock as before ...
if (t->hasSnowLayer)      FlatgrassGen_MapSet(World.Height / 2, World.Height / 2, BLOCK_SNOW);
if (t->hasShadowCeiling)  Gen_PlaceShadowCeiling();
```

**Before** (FloodFillWaterBorders, lines 583-585):
```c
BlockRaw fluidBlock = (Gen_Theme == GEN_THEME_HELL) ? BLOCK_STILL_LAVA :
                      (Gen_Theme == GEN_THEME_WINTER) ? BLOCK_ICE : BLOCK_STILL_WATER;
Gen_CurrentState = (Gen_Theme == GEN_THEME_HELL) ? "Flooding edge lava" : "Flooding edge water";
```

**After:**
```c
const struct GenThemeData* t = &Gen_Themes[Gen_Theme];
BlockRaw fluidBlock = t->edgeFluidBlock;
Gen_CurrentState = t->edgeFloodMsg;
```

**Before** (PlantTrees, lines 854-856):
```c
numPatches = World.Width * World.Length / 4000;
if (Gen_Theme == GEN_THEME_WOODS) numPatches *= 8;
Gen_CurrentState = (Gen_Theme == GEN_THEME_DESERT) ? "Planting cacti" : "Planting trees";
```

**After:**
```c
const struct GenThemeData* t = &Gen_Themes[Gen_Theme];
numPatches = World.Width * World.Length / 4000;
numPatches *= t->treePatchMul;
Gen_CurrentState = t->treePlantMsg;
```

**Before** (heightmap, line 422):
```c
if (Gen_Theme == GEN_THEME_PARADISE || Gen_Theme == GEN_THEME_DESERT)
    height *= 0.5f;
```

**After:**
```c
height *= Gen_Themes[Gen_Theme].heightScale;
```

---

## Step 5: Replace flag-gated code paths

**Before** (tree planting, line 876-890):
```c
if (Gen_Theme == GEN_THEME_DESERT) {
    if (under == BLOCK_SAND) { /* cactus code... */ }
} else {
    if ((under == BLOCK_GRASS || under == BLOCK_SNOWY_GRASS ||
         (Gen_Theme == GEN_THEME_HELL && under == BLOCK_DIRT)) && TreeGen_CanGrow(...))
```

**After:**
```c
if (t->plantsCacti) {
    if (under == BLOCK_SAND) { /* cactus code... */ }
} else {
    if ((under == BLOCK_GRASS || under == BLOCK_SNOWY_GRASS ||
         (t->treesOnDirt && under == BLOCK_DIRT)) && TreeGen_CanGrow(...))
```

**Before** (oases, line 903):
```c
if (Gen_Theme == GEN_THEME_DESERT) { /* 60 lines of oasis generation... */ }
```

**After:**
```c
if (t->hasOases) { /* 60 lines unchanged */ }
```

**Before** (cave gardens, lines 1509-1520):
```c
if (Gen_Theme == GEN_THEME_HELL) hasGrass = 0;
if (hasGrass) {
    if (Gen_Theme == GEN_THEME_DESERT) {
        gardenSurface = BLOCK_SAND; gardenFill = BLOCK_SAND;
    } else {
        gardenSurface = BLOCK_GRASS; gardenFill = BLOCK_DIRT;
    }
```

**After:**
```c
if (!t->hasCaveGardens) hasGrass = 0;
if (hasGrass) {
    BlockRaw gardenSurface = t->gardenSurface;
    BlockRaw gardenFill    = t->gardenFill;
```

---

## Step 6: Replace BlockPhysics.c check

**Before** (line 2289):
```c
if (Gen_Theme == GEN_THEME_HELL) return;
```

**After:**
```c
if (!Gen_Themes[Gen_Theme].dirtToGrass) return;
```

---

## Theme Checks That REMAIN (and why)

1. **Surface smoothing** (Generator.c:672-711) - 5 different code paths with unique noise thresholds per theme. Paradise has different beach logic than Normal. Winter checks ICE instead of WATER. Can't reduce to simple lookups.

2. **Ore-in-dirt exception** (Generator.c:326-328) - `if (!(Gen_Theme == GEN_THEME_HELL && gen_active == &CavesGen))` checks both theme AND generator type. Cross-cutting concern.

---

## All Replacement Locations (checklist)

- [ ] FlatgrassGen_Generate (143-171): surfaceBlock, fillBlock, hasSnowLayer, hasShadowCeiling
- [ ] FlatgrassGen_Setup (176-195): GenTheme_ApplyEnvironment()
- [ ] NotchyGen heightmap (422-423): heightScale
- [ ] NotchyGen FloodFillWaterBorders (583-585): edgeFluidBlock, edgeFloodMsg
- [ ] NotchyGen FloodFillWater (611-614): fluidBlock, internalFloodMsg
- [ ] NotchyGen PlantFlowers (~773, 1199-1202): flowerPatchMul, generateFlowers
- [ ] NotchyGen PlantTrees (854-890): treePatchMul, treePlantMsg, plantsCacti, treesOnDirt
- [ ] NotchyGen oases (903): hasOases
- [ ] NotchyGen Prepare (968-969): raiseWaterLevel
- [ ] NotchyGen Generate (1001): hasShadowCeiling
- [ ] NotchyGen Setup (1006-1025): GenTheme_ApplyEnvironment()
- [ ] CarveSurfaces (1189-1194): surfaceBlock
- [ ] CarveSurfaces flowers (1201-1202): generateFlowers, flowerPatchMul
- [ ] CarveSurfaces trees (1230-1262): treePatchMul, treePlantMsg, plantsCacti, treesOnDirt
- [ ] FloatingGen Prepare (~1280): raiseWaterLevel
- [ ] FloatingGen Generate (~1358): hasShadowCeiling
- [ ] FloatingGen Setup (1363-1386): GenTheme_ApplyEnvironment()
- [ ] CavesGen FillStone (1404): caveFillBlock
- [ ] CavesGen caverns garden (1509-1520): hasCaveGardens, gardenSurface, gardenFill
- [ ] CavesGen caverns cacti (1546): plantsCacti
- [ ] CavesGen mushroom floor (1614): caveFillBlock
- [ ] CavesGen extra ores (1727-1730): hasExtraCaveOres
- [ ] CavesGen Setup (1748-1764): GenTheme_ApplyEnvironment()
- [ ] BlockPhysics.c (2289): dirtToGrass

---

## Verification

1. Build - no compile errors
2. Generate a world with each of the 6 themes x each generator (Flat, Normal, Floating, Caves)
3. Verify: Hell shadow ceiling, Winter snow, Desert cacti/oases, Paradise raised water + beaches, Woods dense trees
4. Verify dirt-to-grass physics works normally and is blocked in Hell
