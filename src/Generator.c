#include "Generator.h"
#include "BlockID.h"
#include "ExtMath.h"
#include "Funcs.h"
#include "Platform.h"
#include "World.h"
#include "Utils.h"
#include "Game.h"
#include "Screens.h"
#include "Window.h"
#include "Options.h"
#include "String_.h"
#include "InputHandler.h"

const struct MapGenerator* gen_active;
BlockRaw* Gen_Blocks;
int Gen_Theme;
cc_uint8 Gen_ActiveTimeMode;

const struct GenThemeData Gen_Themes[GEN_THEME_COUNT - 1] = {
	/* GEN_THEME_NORMAL (0) */
	{
		BLOCK_GRASS, BLOCK_DIRT,                           /* surfaceBlock, fillBlock */
		BLOCK_STILL_WATER, BLOCK_STILL_WATER,              /* fluidBlock, edgeFluidBlock */
		BLOCK_STILL_WATER, BLOCK_BEDROCK,                  /* edgeBlock, sidesBlock */
		0,                                                 /* edgeHeightOffset (offset from height/2) */
		BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT,              /* caveFillBlock, gardenSurface, gardenFill */
		0, 0, 0, 0,                                        /* sky, fog, clouds, shadow (defaults) */
		BLOCK_STONE, BLOCK_GRAVEL,                         /* stoneBlock, underwaterBlock */
		1.0f, 1.0f, 1, 1, 1,                              /* heightScale, caveFreqScale, treePatchMul, flowerPatchMul, mushroomPatchMul */
		GEN_TIME_CYCLE, false, true, true, false, true, false, false, false, false, false,
		0, 0,                                              /* nightSkyCol, nightFogCol (defaults) */
		"Planting trees", "Flooding edge water", "Flooding water"
	},
	/* GEN_THEME_HELL (1) */
	{
		BLOCK_DIRT, BLOCK_DIRT,
		BLOCK_STILL_LAVA, BLOCK_STILL_LAVA,
		BLOCK_STILL_LAVA, BLOCK_OBSIDIAN,
		0,
		BLOCK_DIRT, BLOCK_GRASS, BLOCK_DIRT,
		PackedCol_Make(0x80, 0x10, 0x10, 0xFF),            /* skyCol - dark red */
		PackedCol_Make(0x18, 0x14, 0x14, 0xFF),            /* fogCol - very dark red */
		PackedCol_Make(0x30, 0x28, 0x28, 0xFF),            /* cloudsCol - dark brown-red */
		0,
		BLOCK_STONE, BLOCK_GRAVEL,
		1.0f, 1.0f, 1, 1, 1,
		GEN_TIME_NIGHT, false, false, false, false, true, true, true, false, false, false,
		0, 0,                                              /* nightSkyCol, nightFogCol (defaults) */
		"Planting trees", "Flooding edge lava", "Flooding lava"
	},
	/* GEN_THEME_PARADISE (2) */
	{
		BLOCK_GRASS, BLOCK_DIRT,
		BLOCK_STILL_WATER, BLOCK_STILL_WATER,
		BLOCK_STILL_WATER, BLOCK_BEDROCK,
		0,
		BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT,
		0, 0, 0, 0,
		BLOCK_STONE, BLOCK_GRAVEL,
		0.5f, 1.0f, 1, 3, 1,                              /* flat terrain, 3x flowers */
		GEN_TIME_CYCLE, false, true, true, false, true, false, false, true, false, false,
		0, 0,                                              /* nightSkyCol, nightFogCol (defaults) */
		"Planting trees", "Flooding edge water", "Flooding water"
	},
	/* GEN_THEME_WOODS (3) */
	{
		BLOCK_GRASS, BLOCK_DIRT,
		BLOCK_STILL_WATER, BLOCK_STILL_WATER,
		BLOCK_STILL_WATER, BLOCK_BEDROCK,
		0,
		BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT,
		0, 0, 0, 0,
		BLOCK_STONE, BLOCK_GRAVEL,
		1.0f, 1.0f, 8, 1, 1,                              /* 8x trees */
		GEN_TIME_CYCLE, false, true, true, false, true, false, false, false, false, false,
		0, 0,                                              /* nightSkyCol, nightFogCol (defaults) */
		"Planting trees", "Flooding edge water", "Flooding water"
	},
	/* GEN_THEME_DESERT (4) */
	{
		BLOCK_SAND, BLOCK_SAND,
		BLOCK_STILL_WATER, BLOCK_SAND,                    /* fluidBlock, edgeFluidBlock (sand border) */
		0, BLOCK_SAND,                                    /* edgeBlock, sidesBlock (sand border) */
		0,
		BLOCK_STONE, BLOCK_SAND, BLOCK_SAND,               /* caves: sand gardens */
		PackedCol_Make(0xD4, 0xB8, 0x70, 0xFF),            /* skyCol - golden tan */
		PackedCol_Make(0xD4, 0xA5, 0x50, 0xFF),            /* fogCol - sandstorm */
		PackedCol_Make(0xE0, 0xC8, 0x90, 0xFF),            /* cloudsCol - light golden */
		0,
		BLOCK_STONE, BLOCK_GRAVEL,
		0.5f, 1.0f, 0, 1, 1,                              /* flat terrain, 0 trees (oases only) */
		GEN_TIME_CYCLE, false, true, true, 1, false, false, false, false, true, false,
		0, 0,                                              /* nightSkyCol, nightFogCol (defaults) */
		"Planting cacti", "Filling edge sand", "Flooding water"
	},
	/* GEN_THEME_WINTER (5) */
	{
		BLOCK_GRASS, BLOCK_DIRT,
		BLOCK_ICE, BLOCK_ICE,                              /* all water as ice in winter theme */
		BLOCK_ICE, BLOCK_BEDROCK,
		0,
		BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT,
		PackedCol_Make(0xC0, 0xD8, 0xF0, 0xFF),            /* skyCol - light blue */
		PackedCol_Make(0xE0, 0xE8, 0xF0, 0xFF),            /* fogCol - very light blue */
		PackedCol_Make(0xF0, 0xF0, 0xF0, 0xFF),            /* cloudsCol - white */
		0,
		BLOCK_STONE, BLOCK_GRAVEL,
		1.0f, 1.0f, 1, 1, 1,
		GEN_TIME_CYCLE, true, true, true, false, false, false, false, false, false, false,
		0, 0,                                              /* nightSkyCol, nightFogCol (defaults) */
		"Planting trees", "Flooding edge water", "Flooding water"
	},
	/* GEN_THEME_MOON (6) */
	{
		BLOCK_COBBLE, BLOCK_STONE,
		BLOCK_GRAVEL, BLOCK_GRAVEL,
		0, BLOCK_COBBLE,
		0,
		BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT,
		PackedCol_Make(0x00, 0x00, 0x00, 0xFF),            /* skyCol - black */
		PackedCol_Make(0x00, 0x00, 0x00, 0xFF),            /* fogCol - black */
		PackedCol_Make(0x38, 0x38, 0x38, 0xFF),            /* cloudsCol - light gray */
		0,
		BLOCK_STONE, BLOCK_GRAVEL,
		0.5f, 1.0f, 0, 1, 1,
		GEN_TIME_CYCLE, false, true, false, false, false, false, false, false, false, false,
		0, 0,                                              /* nightSkyCol, nightFogCol (defaults) */
		"Planting trees", "Flooding edge water", "Flooding water"
	},
	/* GEN_THEME_JUNGLE (7) */
	{
		BLOCK_GRASS, BLOCK_DIRT,                           /* surfaceBlock, fillBlock */
		BLOCK_STILL_WATER, BLOCK_STILL_WATER,              /* fluidBlock, edgeFluidBlock */
		BLOCK_STILL_WATER, BLOCK_BEDROCK,                  /* edgeBlock, sidesBlock */
		0,
		BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT,              /* caveFillBlock, gardenSurface, gardenFill */
		0, 0, 0, 0,                                        /* sky, fog, clouds, shadow (defaults) */
		BLOCK_STONE, BLOCK_GRAVEL,
		1.0f, 1.0f, 4, 4, 1,                              /* 4x trees, 4x flowers */
		GEN_TIME_CYCLE, false, true, true, false, true, false, false, false, false, true,
		0, 0,                                              /* nightSkyCol, nightFogCol (defaults) */
		"Planting trees", "Flooding edge water", "Flooding water"
	},
	/* GEN_THEME_PLAINS (8) */
	{
		BLOCK_GRASS, BLOCK_DIRT,
		BLOCK_GRASS, BLOCK_GRASS,
		0, BLOCK_GRASS,
		2,                                                  /* edgeHeightOffset (offset from height/2) */
		BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT,
		0, 0, 0, 0,
		BLOCK_STONE, BLOCK_GRAVEL,
		0.5f, 1.0f, 0, 0, 1,
		GEN_TIME_CYCLE, false, true, false, false, false, false, false, false, false, false,
		0, 0,                                              /* nightSkyCol, nightFogCol (defaults) */
		"Planting trees", "Flooding edge water", "Flooding water"
	},
};

struct GenThemeData Gen_CustomTheme;
struct OreDefinition Gen_CustomOres[MAX_CUSTOM_ORES] = {
	{ BLOCK_COAL_ORE,    0.9f, true },
	{ BLOCK_IRON_ORE,    0.7f, true },
	{ BLOCK_GOLD_ORE,    0.5f, true },
	{ BLOCK_RED_ORE,     0.6f, true },
	{ BLOCK_DIAMOND_ORE, 0.4f, true },
	{ 0, 0.0f, false },
	{ 0, 0.0f, false },
	{ 0, 0.0f, false },
	{ 0, 0.0f, false },
	{ 0, 0.0f, false },
};

const struct GenThemeData* Gen_GetTheme(void) {
	if (Gen_Theme == GEN_THEME_CUSTOM) return &Gen_CustomTheme;
	return &Gen_Themes[Gen_Theme];
}

void CustomTheme_CopyFrom(int themeIndex) {
	Gen_CustomTheme = Gen_Themes[themeIndex];
	/* Replace 0 sentinel (meaning "use engine default") with actual default colors,
	   so custom theme always has explicit color values for the UI to display */
	if (!Gen_CustomTheme.skyCol)     Gen_CustomTheme.skyCol     = ENV_DEFAULT_SKY_COLOR;
	if (!Gen_CustomTheme.fogCol)     Gen_CustomTheme.fogCol     = ENV_DEFAULT_FOG_COLOR;
	if (!Gen_CustomTheme.cloudsCol)  Gen_CustomTheme.cloudsCol  = ENV_DEFAULT_CLOUDS_COLOR;
	if (!Gen_CustomTheme.shadowCol)  Gen_CustomTheme.shadowCol  = ENV_DEFAULT_SHADOW_COLOR;
}

void GenTheme_ApplyEnvironment(void) {
	const struct GenThemeData* t = Gen_GetTheme();
	if (t->skyCol)     Env_SetSkyCol(t->skyCol);
	if (t->fogCol)     Env_SetFogCol(t->fogCol);
	if (t->cloudsCol)  Env_SetCloudsCol(t->cloudsCol);
	if (t->shadowCol)  Env_SetShadowCol(t->shadowCol);
	if (t->edgeBlock)  Env_SetEdgeBlock(t->edgeBlock);
	if (t->sidesBlock) Env_SetSidesBlock(t->sidesBlock);
	Env_SetEdgeHeightOffset(t->edgeHeightOffset);

	/* Set runtime time mode from theme */
	Gen_ActiveTimeMode = t->timeMode;

	/* Re-enable day/night cycle now that theme colors are applied.
	   OnNewMapLoaded may have already enabled it with default (wrong) colors,
	   so re-capture the correct theme colors as daytime originals. */
	if (t->timeMode == GEN_TIME_NIGHT) {
		DayNightCycle_Enable();   /* captures correct theme colors */
	} else if (t->timeMode == GEN_TIME_CYCLE && Game_DaylightCycle) {
		DayNightCycle_Enable();   /* re-capture correct theme colors */
	} else if (t->timeMode == GEN_TIME_DAY) {
		DayNightCycle_Disable();
	}
}

volatile float Gen_CurrentProgress;
volatile const char* Gen_CurrentState;
volatile static cc_bool gen_done;

/* There are two main types of multitasking: */
/*  - Pre-emptive multitasking (system automatically switches between threads) */
/*  - Cooperative multitasking (threads must be manually switched by the app) */
/*                                                                             */
/* Systems only supporting cooperative multitasking can be problematic though: */
/*   If the whole map generation was performed as a single function call, */
/*     then the game thread would not get run at all until map generation */
/*     completed - which is not a great user experience. */
/*   To avoid that, on these systems, map generation may be divided into */
/*     a series of steps so that ClassiCube can periodically switch back */
/*     to the game thread to ensure that the game itself still (slowly) runs. */
#ifdef CC_BUILD_COOPTHREADED
static int gen_step;
static cc_uint64 lastRender;

#define GEN_COOP_BEGIN \
	cc_uint64 curTime; \
	switch (gen_step) {

#define GEN_COOP_STEP(index, step) \
	case index: \
		step; \
		gen_step++; \
		curTime = Stopwatch_Measure(); \
		if (Stopwatch_ElapsedMS(lastRender, curTime) > 100) { lastRender = curTime; return; }
		/* Switch back to game thread if more than 100 milliseconds since it was last run */

#define GEN_COOP_END \
	}

static void Gen_Run(void) {
	gen_step = 0;
	lastRender = Stopwatch_Measure();
	gen_active->Generate();
}

cc_bool Gen_IsDone(void) {
	/* Resume map generation if incomplete */
	if (!gen_done) gen_active->Generate();
	return gen_done;
}
#else
/* For systems supporting preemptive threading, there's no point */
/* bothering with all the cooperative tasking shenanigans */
#define GEN_COOP_BEGIN
#define GEN_COOP_STEP(index, step) step;
#define GEN_COOP_END

static void Gen_DoGen(void) {
	gen_active->Generate();
}

static void Gen_Run(void) {
	void* thread;
	Thread_Run(&thread, Gen_DoGen, 128 * 1024, "Map gen");
	Thread_Detach(thread);
}

cc_bool Gen_IsDone(void) { return gen_done; }
#endif

static void Gen_Reset(void) {
	Gen_CurrentProgress = 0.0f;
	Gen_CurrentState    = "";
	gen_done = false;
}

void Gen_Start(const struct MapGenerator* gen, int seed,
				int width, int height, int length) {	
	World_NewMap();
	World_SetDimensions(width, height, length);
	World.Seed = seed;

	gen_active = gen;
	Gen_Reset();
	Gen_Blocks = (BlockRaw*)Mem_TryAlloc(World.Volume, 1);

	if (!Gen_Blocks || !gen->Prepare(seed)) {
		Window_ShowDialog("Out of memory", "Not enough free memory to generate a map that large.\nTry a smaller size.");
		gen_done = true;
	} else {
		Gen_Run();
	}

	GeneratingScreen_Show();
}


/*########################################################################################################################*
*-----------------------------------------------------Flatgrass gen-------------------------------------------------------*
*#########################################################################################################################*/
static void FlatgrassGen_MapSet(int yBeg, int yEnd, BlockRaw block) {
	cc_uint32 oneY = (cc_uint32)World.OneY;
	BlockRaw* ptr = Gen_Blocks;
	int y, yHeight;

	yBeg = max(yBeg, 0); yEnd = max(yEnd, 0);
	yHeight = (yEnd - yBeg) + 1;
	Gen_CurrentProgress = 0.0f;

	for (y = yBeg; y <= yEnd; y++) {
		Mem_Set(ptr + y * oneY, block, oneY);
		Gen_CurrentProgress = (float)(y - yBeg) / yHeight;
	}
}

/* Fill Y=MaxY with invisible light-blocking blocks for hell theme */
static void Gen_PlaceShadowCeiling(void) {
	int x, z;
	for (z = 0; z < World.Length; z++) {
		for (x = 0; x < World.Width; x++) {
			Gen_Blocks[World_Pack(x, World.MaxY, z)] = BLOCK_SHADOW_CEILING;
		}
	}
}

static cc_bool FlatgrassGen_Prepare(int seed) {
	return true;
}

static void FlatgrassGen_Generate(void) {
	const struct GenThemeData* t = Gen_GetTheme();
	BlockRaw surfaceBlock = t->surfaceBlock;
	BlockRaw fillBlock    = t->fillBlock;

	Gen_CurrentState = "Setting air blocks";
	FlatgrassGen_MapSet(World.Height / 2, World.MaxY, BLOCK_AIR);

	Gen_CurrentState = "Setting fill blocks";
	FlatgrassGen_MapSet(0, World.Height / 2 - 2, fillBlock);

	Gen_CurrentState = "Setting surface blocks";
	FlatgrassGen_MapSet(World.Height / 2 - 1, World.Height / 2 - 1, surfaceBlock);

	if (t->hasSnowLayer) {
		if (surfaceBlock == BLOCK_GRASS) {
			FlatgrassGen_MapSet(World.Height / 2 - 1, World.Height / 2 - 1, BLOCK_SNOWY_GRASS);
		}
		FlatgrassGen_MapSet(World.Height / 2, World.Height / 2, BLOCK_SNOW);
	}

	gen_done = true;
}

static void FlatgrassGen_Setup(void) {
	GenTheme_ApplyEnvironment();
}

const struct MapGenerator FlatgrassGen = {
	FlatgrassGen_Prepare,
	FlatgrassGen_Generate,
	FlatgrassGen_Setup
};


/*########################################################################################################################*
*---------------------------------------------------Noise generation------------------------------------------------------*
*#########################################################################################################################*/
#define NOISE_TABLE_SIZE 512
static void ImprovedNoise_Init(cc_uint8* p, RNGState* rnd) {
	cc_uint8 tmp;
	int i, j;
	for (i = 0; i < 256; i++) { p[i] = i; }

	/* shuffle randomly using fisher-yates */
	for (i = 0; i < 256; i++) {
		j   = Random_Range(rnd, i, 256);
		tmp = p[i]; p[i] = p[j]; p[j] = tmp;
	}

	for (i = 0; i < 256; i++) {
		p[i + 256] = p[i];
	}
}

/* Normally, calculating Grad involves a function call + switch. However, the table combinations
  can be directly packed into a set of bit flags (where each 2 bit combination indicates either -1, 0 1).
  This avoids needing to call another function that performs branching */
#define X_FLAGS 0x46552222
#define Y_FLAGS 0x2222550A
#define Grad(hash, x, y) (((X_FLAGS >> (hash)) & 3) - 1) * (x) + (((Y_FLAGS >> (hash)) & 3) - 1) * (y);

static float ImprovedNoise_Calc(const cc_uint8* p, float x, float y) {
	int xFloor, yFloor, X, Y;
	float u, v;
	int A, B, hash;
	float g22, g12, c1;
	float g21, g11, c2;

	xFloor = x >= 0 ? (int)x : (int)x - 1;
	yFloor = y >= 0 ? (int)y : (int)y - 1;
	X = xFloor & 0xFF; Y = yFloor & 0xFF;
	x -= xFloor;       y -= yFloor;

	u = x * x * x * (x * (x * 6 - 15) + 10); /* Fade(x) */
	v = y * y * y * (y * (y * 6 - 15) + 10); /* Fade(y) */
	A = p[X] + Y; B = p[X + 1] + Y;

	hash = (p[p[A]] & 0xF) << 1;
	g22  = Grad(hash, x,     y); /* Grad(p[p[A], x,     y) */
	hash = (p[p[B]] & 0xF) << 1;
	g12  = Grad(hash, x - 1, y); /* Grad(p[p[B], x - 1, y) */
	c1   = g22 + u * (g12 - g22);

	hash = (p[p[A + 1]] & 0xF) << 1;
	g21  = Grad(hash, x,     y - 1); /* Grad(p[p[A + 1], x,     y - 1) */
	hash = (p[p[B + 1]] & 0xF) << 1;
	g11  = Grad(hash, x - 1, y - 1); /* Grad(p[p[B + 1], x - 1, y - 1) */
	c2   = g21 + u * (g11 - g21);

	return c1 + v * (c2 - c1);
}


struct OctaveNoise { cc_uint8 p[8][NOISE_TABLE_SIZE]; int octaves; };
static void OctaveNoise_Init(struct OctaveNoise* n, RNGState* rnd, int octaves) {
	int i;
	n->octaves = octaves;
	
	for (i = 0; i < octaves; i++) {
		ImprovedNoise_Init(n->p[i], rnd);
	}
}

static float OctaveNoise_Calc(const struct OctaveNoise* n, float x, float y) {
	float amplitude = 1, freq = 1;
	float sum = 0;
	int i;

	for (i = 0; i < n->octaves; i++) {
		sum += ImprovedNoise_Calc(n->p[i], x * freq, y * freq) * amplitude;
		amplitude *= 2.0f;
		freq *= 0.5f;
	}
	return sum;
}


struct CombinedNoise { struct OctaveNoise noise1, noise2; };
static void CombinedNoise_Init(struct CombinedNoise* n, RNGState* rnd, int octaves1, int octaves2) {
	OctaveNoise_Init(&n->noise1, rnd, octaves1);
	OctaveNoise_Init(&n->noise2, rnd, octaves2);
}

static float CombinedNoise_Calc(const struct CombinedNoise* n, float x, float y) {
	float offset = OctaveNoise_Calc(&n->noise2, x, y);
	return OctaveNoise_Calc(&n->noise1, x + offset, y);
}


/*########################################################################################################################*
*--------------------------------------------------3D Noise generation----------------------------------------------------*
*#########################################################################################################################*/
/* 3D Improved Perlin noise using Ken Perlin's 2002 improved algorithm.
   Uses 16 gradient directions selected by the low 4 bits of the hash. */
static float Grad3D(int hash, float x, float y, float z) {
	float u, v;
	hash &= 15;
	u = hash < 8 ? x : y;
	v = hash < 4 ? y : (hash == 12 || hash == 14 ? x : z);
	return ((hash & 1) == 0 ? u : -u) + ((hash & 2) == 0 ? v : -v);
}

static float ImprovedNoise_Calc3D(const cc_uint8* p, float x, float y, float z) {
	int xFloor, yFloor, zFloor, X, Y, Z;
	float u, v, w;
	int A, AA, AB, B, BA, BB;
	float g1, g2, g3, g4, g5, g6, g7, g8;
	float l1, l2, l3, l4, m1, m2;

	xFloor = x >= 0 ? (int)x : (int)x - 1;
	yFloor = y >= 0 ? (int)y : (int)y - 1;
	zFloor = z >= 0 ? (int)z : (int)z - 1;
	X = xFloor & 0xFF; Y = yFloor & 0xFF; Z = zFloor & 0xFF;
	x -= xFloor; y -= yFloor; z -= zFloor;

	u = x * x * x * (x * (x * 6 - 15) + 10); /* Fade(x) */
	v = y * y * y * (y * (y * 6 - 15) + 10); /* Fade(y) */
	w = z * z * z * (z * (z * 6 - 15) + 10); /* Fade(z) */

	A  = p[X] + Y;
	AA = p[A] + Z;
	AB = p[A + 1] + Z;
	B  = p[X + 1] + Y;
	BA = p[B] + Z;
	BB = p[B + 1] + Z;

	/* Trilinear interpolation of 8 gradient dot products */
	g1 = Grad3D(p[AA],     x,     y,     z);
	g2 = Grad3D(p[BA],     x - 1, y,     z);
	g3 = Grad3D(p[AB],     x,     y - 1, z);
	g4 = Grad3D(p[BB],     x - 1, y - 1, z);
	g5 = Grad3D(p[AA + 1], x,     y,     z - 1);
	g6 = Grad3D(p[BA + 1], x - 1, y,     z - 1);
	g7 = Grad3D(p[AB + 1], x,     y - 1, z - 1);
	g8 = Grad3D(p[BB + 1], x - 1, y - 1, z - 1);

	l1 = g1 + u * (g2 - g1);
	l2 = g3 + u * (g4 - g3);
	l3 = g5 + u * (g6 - g5);
	l4 = g7 + u * (g8 - g7);

	m1 = l1 + v * (l2 - l1);
	m2 = l3 + v * (l4 - l3);

	return m1 + w * (m2 - m1);
}

/* 3D octave noise: up to 16 octaves of 3D Perlin noise */
#define OCTAVE3D_MAX 16
struct OctaveNoise3D { cc_uint8 p[OCTAVE3D_MAX][NOISE_TABLE_SIZE]; int octaves; };

static void OctaveNoise3D_Init(struct OctaveNoise3D* n, RNGState* rnd, int octaves) {
	int i;
	n->octaves = octaves;
	for (i = 0; i < octaves; i++) {
		ImprovedNoise_Init(n->p[i], rnd);
	}
}

static float OctaveNoise3D_Calc(const struct OctaveNoise3D* n, float x, float y, float z) {
	float amplitude = 1, freq = 1;
	float sum = 0;
	int i;
	for (i = 0; i < n->octaves; i++) {
		sum += ImprovedNoise_Calc3D(n->p[i], x * freq, y * freq, z * freq) * amplitude;
		amplitude *= 2.0f;
		freq *= 0.5f;
	}
	return sum;
}


/*########################################################################################################################*
*----------------------------------------------------Notchy map gen-------------------------------------------------------*
*#########################################################################################################################*/
static int waterLevel, minHeight;
static cc_int16* heightmap;
static RNGState rnd;

static void NotchyGen_FillOblateSpheroid(int x, int y, int z, float radius, BlockRaw block) {
	int xBeg = Math_Floor(max(x - radius, 0));
	int xEnd = Math_Floor(min(x + radius, World.MaxX));
	int yBeg = Math_Floor(max(y - radius, 0));
	int yEnd = Math_Floor(min(y + radius, World.MaxY));
	int zBeg = Math_Floor(max(z - radius, 0));
	int zEnd = Math_Floor(min(z + radius, World.MaxZ));

	float radiusSq = radius * radius;
	int index;
	int xx, yy, zz, dx, dy, dz;

	for (yy = yBeg; yy <= yEnd; yy++) { dy = yy - y;
		for (zz = zBeg; zz <= zEnd; zz++) { dz = zz - z;
			for (xx = xBeg; xx <= xEnd; xx++) { dx = xx - x;

				if ((dx * dx + 2 * dy * dy + dz * dz) < radiusSq) {
					index = World_Pack(xx, yy, zz);
					if (Gen_Blocks[index] == Gen_GetTheme()->stoneBlock || Gen_Blocks[index] == BLOCK_DIRT) {
						/* Ores cannot generate in dirt unless hell theme + caves generator */
						if (Gen_Blocks[index] == BLOCK_DIRT && block != BLOCK_AIR) {
							if (!(Gen_Theme == GEN_THEME_HELL && gen_active == &CavesGen))
								continue;
						}
						Gen_Blocks[index] = block;
					}
				}
			}
		}
	}
}

#if CC_BUILD_MAXSTACK <= (32 * 1024)
	#define STACK_FAST 512
#else
	#define STACK_FAST 8192
#endif

static void NotchyGen_FloodFill(int index, BlockRaw block) {
	int* stack;
	int stack_default[STACK_FAST]; /* avoid allocating memory if possible */
	int count = 0, limit = STACK_FAST;
	int x, y, z;

	stack = stack_default;
	if (index < 0) return; /* y below map, don't bother starting */
	stack[count++] = index;

	while (count) {
		index = stack[--count];

		if (Gen_Blocks[index] != BLOCK_AIR) continue;
		Gen_Blocks[index] = block;

		x = index  % World.Width;
		y = index  / World.OneY;
		z = (index / World.Width) % World.Length;

		/* need to increase stack */
		if (count >= limit - FACE_COUNT) {
			Utils_Resize((void**)&stack, &limit, 4, STACK_FAST, STACK_FAST);
		}

		if (x > 0)          { stack[count++] = index - 1; }
		if (x < World.MaxX) { stack[count++] = index + 1; }
		if (z > 0)          { stack[count++] = index - World.Width; }
		if (z < World.MaxZ) { stack[count++] = index + World.Width; }
		if (y > 0)          { stack[count++] = index - World.OneY; }
	}
	if (limit > STACK_FAST) Mem_Free(stack);
}


static void NotchyGen_CreateHeightmap(void) {
	float hLow, hHigh, height;
	int hIndex = 0, adjHeight;
	int x, z;

#if CC_BUILD_MAXSTACK <= (16 * 1024)
	struct NoiseBuffer { 
		struct CombinedNoise n1, n2;
		struct OctaveNoise n3;
	};
	void* mem = TempMem_Alloc(sizeof(struct NoiseBuffer));

	struct NoiseBuffer* buf  = (struct NoiseBuffer*)mem;
	struct CombinedNoise* n1 = &buf->n1;
	struct CombinedNoise* n2 = &buf->n2;
	struct OctaveNoise*   n3 = &buf->n3;
#else
	struct CombinedNoise _n1, *n1 = &_n1;
	struct CombinedNoise _n2, *n2 = &_n2;
	struct OctaveNoise   _n3, *n3 = &_n3;
#endif

	CombinedNoise_Init(n1, &rnd, 8, 8);
	CombinedNoise_Init(n2, &rnd, 8, 8);	
	OctaveNoise_Init(n3,   &rnd, 6);

	Gen_CurrentState = "Building heightmap";
	for (z = 0; z < World.Length; z++) {
		Gen_CurrentProgress = (float)z / World.Length;

		for (x = 0; x < World.Width; x++) {
			hLow   = CombinedNoise_Calc(n1, x * 1.3f, z * 1.3f) / 6 - 4;
			height = hLow;

			if (OctaveNoise_Calc(n3, (float)x, (float)z) <= 0) {
				hHigh = CombinedNoise_Calc(n2, x * 1.3f, z * 1.3f) / 5 + 6;
				height = max(hLow, hHigh);
			}

			height *= 0.5f;
			if (height < 0) height *= 0.8f;

			height *= Gen_GetTheme()->heightScale;

			adjHeight = (int)(height + waterLevel);
			minHeight = min(adjHeight, minHeight);
			heightmap[hIndex++] = adjHeight;
		}
	}
}

static int NotchyGen_CreateStrataFast(void) {
	cc_uint32 oneY = (cc_uint32)World.OneY;
	int stoneHeight, airHeight;
	int y;

	Gen_CurrentProgress = 0.0f;
	Gen_CurrentState    = "Filling map";
	/* Make lava layer at bottom */
	Mem_Set(Gen_Blocks, BLOCK_STILL_LAVA, oneY);

	/* Invariant: the lowest value dirtThickness can possible be is -14 */
	stoneHeight = minHeight - 14;
	/* We can quickly fill in bottom solid layers */
	for (y = 1; y <= stoneHeight; y++) {
		Mem_Set(Gen_Blocks + y * oneY, Gen_GetTheme()->stoneBlock, oneY);
		Gen_CurrentProgress = (float)y / World.Height;
	}

	/* Fill in rest of map wih air */
	airHeight = max(0, stoneHeight) + 1;
	for (y = airHeight; y < World.Height; y++) {
		Mem_Set(Gen_Blocks + y * oneY, BLOCK_AIR, oneY);
		Gen_CurrentProgress = (float)y / World.Height;
	}

	/* if stoneHeight is <= 0, then no layer is fully stone */
	return max(stoneHeight, 1);
}

static void NotchyGen_CreateStrata(void) {
	int dirtThickness, dirtHeight;
	int minStoneY, stoneHeight;
	int hIndex = 0, maxY = World.MaxY, index = 0;
	int x, y, z;
	struct OctaveNoise n;

	/* Try to bulk fill bottom of the map if possible */
	minStoneY = NotchyGen_CreateStrataFast();
	OctaveNoise_Init(&n, &rnd, 8);

	Gen_CurrentState = "Creating strata";
	for (z = 0; z < World.Length; z++) {
		Gen_CurrentProgress = (float)z / World.Length;

		for (x = 0; x < World.Width; x++) {
			dirtThickness = (int)(OctaveNoise_Calc(&n, (float)x, (float)z) / 24 - 4);
			dirtHeight    = heightmap[hIndex++];
			stoneHeight   = dirtHeight + dirtThickness;

			stoneHeight = min(stoneHeight, maxY);
			dirtHeight  = min(dirtHeight,  maxY);

			index = World_Pack(x, minStoneY, z);
			for (y = minStoneY; y <= stoneHeight; y++) {
				Gen_Blocks[index] = Gen_GetTheme()->stoneBlock; index += World.OneY;
			}

			stoneHeight = max(stoneHeight, 0);
			index = World_Pack(x, (stoneHeight + 1), z);
			for (y = stoneHeight + 1; y <= dirtHeight; y++) {
				Gen_Blocks[index] = Gen_GetTheme()->fillBlock; index += World.OneY;
			}
		}
	}
}

static void NotchyGen_CarveCaves(void) {
	int cavesCount, caveLen;
	float caveX, caveY, caveZ;
	float theta, deltaTheta, phi, deltaPhi;
	float caveRadius, radius;
	int cenX, cenY, cenZ;
	int i, j;

	cavesCount       = (int)(World.Volume / 8192 * Gen_GetTheme()->caveFreqScale);
	Gen_CurrentState = "Carving caves";
	for (i = 0; i < cavesCount; i++) {
		Gen_CurrentProgress = (float)i / cavesCount;

		caveX = (float)Random_Next(&rnd, World.Width);
		caveY = (float)Random_Next(&rnd, World.Height);
		caveZ = (float)Random_Next(&rnd, World.Length);

		caveLen = (int)(Random_Float(&rnd) * Random_Float(&rnd) * 200.0f);
		theta   = Random_Float(&rnd) * 2.0f * MATH_PI; deltaTheta = 0.0f;
		phi     = Random_Float(&rnd) * 2.0f * MATH_PI; deltaPhi   = 0.0f;
		caveRadius = Random_Float(&rnd) * Random_Float(&rnd);

		for (j = 0; j < caveLen; j++) {
			caveX += Math_SinF(theta) * Math_CosF(phi);
			caveZ += Math_CosF(theta) * Math_CosF(phi);
			caveY += Math_SinF(phi);

			theta      = theta + deltaTheta * 0.2f;
			deltaTheta = deltaTheta * 0.9f + Random_Float(&rnd) - Random_Float(&rnd);
			phi        = phi * 0.5f + deltaPhi * 0.25f;
			deltaPhi   = deltaPhi  * 0.75f + Random_Float(&rnd) - Random_Float(&rnd);
			if (Random_Float(&rnd) < 0.25f) continue;

			cenX = (int)(caveX + (Random_Next(&rnd, 4) - 2) * 0.2f);
			cenY = (int)(caveY + (Random_Next(&rnd, 4) - 2) * 0.2f);
			cenZ = (int)(caveZ + (Random_Next(&rnd, 4) - 2) * 0.2f);

			radius = (World.Height - cenY) / (float)World.Height;
			radius = 1.2f + (radius * 3.5f + 1.0f) * caveRadius;
			radius = radius * Math_SinF(j * MATH_PI / caveLen);
			NotchyGen_FillOblateSpheroid(cenX, cenY, cenZ, radius, BLOCK_AIR);
		}
	}
}

static void NotchyGen_CarveOreVeins(float abundance, const char* state, BlockRaw block) {
	int numVeins, veinLen;
	float veinX, veinY, veinZ;
	float theta, deltaTheta, phi, deltaPhi;
	float radius;
	int i, j;

	numVeins         = (int)(World.Volume * abundance / 16384);
	Gen_CurrentState = state;
	for (i = 0; i < numVeins; i++) {
		Gen_CurrentProgress = (float)i / numVeins;

		veinX = (float)Random_Next(&rnd, World.Width);
		veinY = (float)Random_Next(&rnd, World.Height);
		veinZ = (float)Random_Next(&rnd, World.Length);

		veinLen = (int)(Random_Float(&rnd) * Random_Float(&rnd) * 75 * abundance);
		theta = Random_Float(&rnd) * 2.0f * MATH_PI; deltaTheta = 0.0f;
		phi   = Random_Float(&rnd) * 2.0f * MATH_PI; deltaPhi   = 0.0f;

		for (j = 0; j < veinLen; j++) {
			veinX += Math_SinF(theta) * Math_CosF(phi);
			veinZ += Math_CosF(theta) * Math_CosF(phi);
			veinY += Math_SinF(phi);

			theta      = deltaTheta * 0.2f;
			deltaTheta = deltaTheta * 0.9f + Random_Float(&rnd) - Random_Float(&rnd);
			phi        = phi * 0.5f + deltaPhi * 0.25f;
			deltaPhi   = deltaPhi   * 0.9f + Random_Float(&rnd) - Random_Float(&rnd);

			radius = abundance * Math_SinF(j * MATH_PI / veinLen) + 1.0f;
			NotchyGen_FillOblateSpheroid((int)veinX, (int)veinY, (int)veinZ, radius, block);
		}
	}
}

static void NotchyGen_CarveAllOres(void) {
	int i;
	if (Gen_Theme == GEN_THEME_CUSTOM) {
		for (i = 0; i < MAX_CUSTOM_ORES; i++) {
			if (!Gen_CustomOres[i].enabled) continue;
			NotchyGen_CarveOreVeins(Gen_CustomOres[i].abundance,
				"Carving custom ore", Gen_CustomOres[i].block);
		}
	} else {
		NotchyGen_CarveOreVeins(0.9f, "Carving coal ore",    BLOCK_COAL_ORE);
		NotchyGen_CarveOreVeins(0.7f, "Carving iron ore",    BLOCK_IRON_ORE);
		NotchyGen_CarveOreVeins(0.5f, "Carving gold ore",    BLOCK_GOLD_ORE);
		NotchyGen_CarveOreVeins(0.6f, "Carving red ore",     BLOCK_RED_ORE);
		NotchyGen_CarveOreVeins(0.4f, "Carving diamond ore", BLOCK_DIAMOND_ORE);
	}
}

static void NotchyGen_FloodFillWaterBorders(void) {
	const struct GenThemeData* t = Gen_GetTheme();
	int waterY = waterLevel - 1;
	int index1, index2;
	int x, z;
	BlockRaw fluidBlock = t->edgeFluidBlock;

	/* Desert: no edge flooding */
	if (Gen_Theme == GEN_THEME_DESERT) return;

	Gen_CurrentState = t->edgeFloodMsg;

	index1 = World_Pack(0, waterY, 0);
	index2 = World_Pack(0, waterY, World.Length - 1);
	for (x = 0; x < World.Width; x++) {
		Gen_CurrentProgress = 0.0f + ((float)x / World.Width) * 0.5f;

		NotchyGen_FloodFill(index1, fluidBlock);
		NotchyGen_FloodFill(index2, fluidBlock);
		index1++; index2++;
	}

	index1 = World_Pack(0,             waterY, 0);
	index2 = World_Pack(World.Width - 1, waterY, 0);
	for (z = 0; z < World.Length; z++) {
		Gen_CurrentProgress = 0.5f + ((float)z / World.Length) * 0.5f;

		NotchyGen_FloodFill(index1, fluidBlock);
		NotchyGen_FloodFill(index2, fluidBlock);
		index1 += World.Width; index2 += World.Width;
	}
}

static void NotchyGen_FloodFillWater(void) {
	const struct GenThemeData* t = Gen_GetTheme();
	int numSources;
	int i, x, y, z;
	BlockRaw fluidBlock = t->fluidBlock;

	/* Desert: no internal water */
	if (Gen_Theme == GEN_THEME_DESERT) return;

	numSources       = World.Width * World.Length / 800;
	Gen_CurrentState = t->internalFloodMsg;
	for (i = 0; i < numSources; i++) {
		Gen_CurrentProgress = (float)i / numSources;

		x = Random_Next(&rnd, World.Width);
		z = Random_Next(&rnd, World.Length);
		y = waterLevel - Random_Range(&rnd, 1, 3);
		NotchyGen_FloodFill(World_Pack(x, y, z), fluidBlock);
	}
}

static void NotchyGen_FloodFillLava(void) {
	int numSources;
	int i, x, y, z;

	/* Desert: no lava either */
	if (Gen_Theme == GEN_THEME_DESERT) return;

	numSources       = World.Width * World.Length / 20000;
	Gen_CurrentState = "Flooding lava";
	for (i = 0; i < numSources; i++) {
		Gen_CurrentProgress = (float)i / numSources;

		x = Random_Next(&rnd, World.Width);
		z = Random_Next(&rnd, World.Length);
		y = (int)((waterLevel - 3) * Random_Float(&rnd) * Random_Float(&rnd));
		NotchyGen_FloodFill(World_Pack(x, y, z), BLOCK_STILL_LAVA);
	}
}

static void NotchyGen_CreateSurfaceLayer(void) {	
	int hIndex = 0, index;
	BlockRaw above;
	int x, y, z;
#if CC_BUILD_MAXSTACK <= (16 * 1024)
	struct NoiseBuffer { 
		struct OctaveNoise n1, n2;
	};
	struct NoiseBuffer* buf = TempMem_Alloc(sizeof(struct NoiseBuffer));
	struct OctaveNoise* n1 = &buf->n1;
	struct OctaveNoise* n2 = &buf->n2;
#else
	struct OctaveNoise _n1, _n2;
	struct OctaveNoise* n1 = &_n1;
	struct OctaveNoise* n2 = &_n2;
#endif

	OctaveNoise_Init(n1, &rnd, 8);
	OctaveNoise_Init(n2, &rnd, 8);

	Gen_CurrentState = "Creating surface";
	for (z = 0; z < World.Length; z++) {
		Gen_CurrentProgress = (float)z / World.Length;

		for (x = 0; x < World.Width; x++) {
			y = heightmap[hIndex++];
			if (y < 0 || y >= World.Height) continue;

			index = World_Pack(x, y, z);
			above = y >= World.MaxY ? BLOCK_AIR : Gen_Blocks[index + World.OneY];

			if (Gen_Theme == GEN_THEME_DESERT) {
				/* Desert: all exposed surface is sand */
				if (above == BLOCK_STILL_WATER && (OctaveNoise_Calc(n2, (float)x, (float)z) > 12)) {
					Gen_Blocks[index] = Gen_GetTheme()->underwaterBlock;
				} else if (above == BLOCK_AIR) {
					Gen_Blocks[index] = BLOCK_SAND;
					/* Replace dirt below with sand too */
					if (y >= 1) {
						Gen_Blocks[World_Pack(x, y - 1, z)] = BLOCK_SAND;
					}
				}
			} else if (Gen_Theme == GEN_THEME_HELL) {
				/* Hell: no grass, just dirt; gravel underwater */
				if (above == BLOCK_STILL_WATER && (OctaveNoise_Calc(n2, (float)x, (float)z) > 12)) {
					Gen_Blocks[index] = Gen_GetTheme()->underwaterBlock;
				} else if (above == BLOCK_AIR) {
					Gen_Blocks[index] = BLOCK_DIRT;
				}
			} else if (Gen_Theme == GEN_THEME_PARADISE) {
				/* Paradise: more beaches (lower threshold = more sand near water) */
				if (above == BLOCK_STILL_WATER && (OctaveNoise_Calc(n2, (float)x, (float)z) > 12)) {
					Gen_Blocks[index] = Gen_GetTheme()->underwaterBlock;
				} else if (above == BLOCK_AIR) {
					Gen_Blocks[index] = (y <= waterLevel + 2 && (OctaveNoise_Calc(n1, (float)x, (float)z) > 2)) ? BLOCK_SAND : BLOCK_GRASS;
				}
			} else if (Gen_Theme == GEN_THEME_WINTER) {
				/* Winter: grass on surface (will appear snowy when snow is on top), water/ice, gravel underwater */
				if ((above == BLOCK_STILL_WATER || above == BLOCK_ICE) && (OctaveNoise_Calc(n2, (float)x, (float)z) > 12)) {
					Gen_Blocks[index] = Gen_GetTheme()->underwaterBlock;
				} else if (above == BLOCK_AIR) {
					Gen_Blocks[index] = BLOCK_GRASS;
				}
			} else if (Gen_Theme == GEN_THEME_WOODS || Gen_Theme == GEN_THEME_NORMAL) {
				/* Normal / Woods */
				if (above == BLOCK_STILL_WATER && (OctaveNoise_Calc(n2, (float)x, (float)z) > 12)) {
					Gen_Blocks[index] = Gen_GetTheme()->underwaterBlock;
				} else if (above == BLOCK_AIR) {
					Gen_Blocks[index] = (y <= waterLevel && (OctaveNoise_Calc(n1, (float)x, (float)z) > 8)) ? BLOCK_SAND : BLOCK_GRASS;
				}
			} else if (Gen_Theme == GEN_THEME_JUNGLE) {
				/* Jungle: same as normal, gravel underwater, sand near beaches, grass elsewhere */
				if (above == BLOCK_STILL_WATER && (OctaveNoise_Calc(n2, (float)x, (float)z) > 12)) {
					Gen_Blocks[index] = Gen_GetTheme()->underwaterBlock;
				} else if (above == BLOCK_AIR) {
					Gen_Blocks[index] = (y <= waterLevel && (OctaveNoise_Calc(n1, (float)x, (float)z) > 8)) ? BLOCK_SAND : BLOCK_GRASS;
				}
			} else {
				/* Other / Custom: use theme blocks, normal-style beaches */
				if ((above == Gen_GetTheme()->fluidBlock || above == Gen_GetTheme()->edgeFluidBlock) && (OctaveNoise_Calc(n2, (float)x, (float)z) > 12)) {
					Gen_Blocks[index] = Gen_GetTheme()->underwaterBlock;
				} else if (above == BLOCK_AIR) {
					Gen_Blocks[index] = Gen_GetTheme()->surfaceBlock;
				}
			}
		}
	}
}
static void NotchyGen_FreezeTopWater(void) {
	int x, y, z, index;
	BlockRaw block, above;

	if (Gen_Theme != GEN_THEME_WINTER) return;

	Gen_CurrentState = "Freezing top water";
	for (z = 0; z < World.Length; z++) {
		Gen_CurrentProgress = (float)z / World.Length;
		for (x = 0; x < World.Width; x++) {
			for (y = World.Height - 1; y >= 0; y--) {
				index = World_Pack(x, y, z);
				block = Gen_Blocks[index];
				
				if (block == BLOCK_STILL_WATER) {
					/* Check if air is above - if so, freeze this water block */
					above = (y + 1 >= World.Height) ? BLOCK_AIR : Gen_Blocks[index + World.OneY];
					if (above == BLOCK_AIR) {
						Gen_Blocks[index] = BLOCK_ICE;
					}
					break; /* Found water column, no need to check lower blocks */
				}
			}
		}
	}
}
static void NotchyGen_PlaceSnowLayer(void) {
	int hIndex = 0, index;
	BlockRaw above, current;
	int x, y, z;

	if (!Gen_GetTheme()->hasSnowLayer) return;

	Gen_CurrentState = "Placing snow layer";
	for (z = 0; z < World.Length; z++) {
		Gen_CurrentProgress = (float)z / World.Length;

		for (x = 0; x < World.Width; x++) {
			y = heightmap[hIndex++];
			if (y < 0 || y >= World.MaxY) continue;

			/* Place snow on top of exposed blocks at heightmap level */
			index = World_Pack(x, y + 1, z);
			above = (y + 1 >= World.Height) ? BLOCK_AIR : Gen_Blocks[index];
			
			if (above == BLOCK_AIR) {
				Gen_Blocks[index] = BLOCK_SNOW;
				/* Convert grass below to snowy grass */
				{ int belowIdx = World_Pack(x, y, z);
				  if (Gen_Blocks[belowIdx] == BLOCK_GRASS) Gen_Blocks[belowIdx] = BLOCK_SNOWY_GRASS; }
			}
		}
	}

	/* Second pass: place snow on top of all leaf blocks */
	Gen_CurrentState = "Placing snow on trees";
	for (z = 0; z < World.Length; z++) {
		Gen_CurrentProgress = (float)z / World.Length;

		for (x = 0; x < World.Width; x++) {
			for (y = 0; y < World.Height - 1; y++) {
				index = World_Pack(x, y, z);
				current = Gen_Blocks[index];

				/* If we found leaves, place snow on top if there's air above */
				if (current == BLOCK_LEAVES) {
					index = World_Pack(x, y + 1, z);
					above = Gen_Blocks[index];
					if (above == BLOCK_AIR) {
						Gen_Blocks[index] = BLOCK_SNOW;
					}
				}
			}
		}
	}
}

static void NotchyGen_PlantFlowers(void) {
	int numPatches;
	BlockRaw block;
	int patchX,  patchZ;
	int flowerX, flowerY, flowerZ;
	int i, j, k, index;

	if (Game_Version.Version < VERSION_0023) return;
	if (!Gen_GetTheme()->generateFlowers) return;

	numPatches       = World.Width * World.Length / 3000;
	numPatches      *= Gen_GetTheme()->flowerPatchMul;
	Gen_CurrentState = "Planting flowers";

	for (i = 0; i < numPatches; i++) {
		Gen_CurrentProgress = (float)i / numPatches;

		block  = (BlockRaw)(BLOCK_DANDELION + Random_Next(&rnd, 2));
		patchX = Random_Next(&rnd, World.Width);
		patchZ = Random_Next(&rnd, World.Length);

		for (j = 0; j < 10; j++) {
			flowerX = patchX; flowerZ = patchZ;
			for (k = 0; k < 5; k++) {
				flowerX += Random_Next(&rnd, 6) - Random_Next(&rnd, 6);
				flowerZ += Random_Next(&rnd, 6) - Random_Next(&rnd, 6);

				if (!World_ContainsXZ(flowerX, flowerZ)) continue;
				flowerY = heightmap[flowerZ * World.Width + flowerX] + 1;
				if (flowerY <= 0 || flowerY >= World.Height) continue;

				index = World_Pack(flowerX, flowerY, flowerZ);
				if (Gen_Blocks[index] == BLOCK_AIR && Gen_Blocks[index - World.OneY] == Gen_GetTheme()->surfaceBlock)
					Gen_Blocks[index] = block;
			}
		}
	}
}

static void NotchyGen_PlantMushrooms(void) {
	int numPatches, groundHeight;
	BlockRaw block;
	int patchX, patchY, patchZ;
	int mushX,  mushY,  mushZ;
	int i, j, k, index;

	if (Game_Version.Version < VERSION_0023) return;
	numPatches       = World.Volume / 2000;
	numPatches      *= Gen_GetTheme()->mushroomPatchMul;
	Gen_CurrentState = "Planting mushrooms";

	for (i = 0; i < numPatches; i++) {
		Gen_CurrentProgress = (float)i / numPatches;

		block  = (BlockRaw)(BLOCK_BROWN_SHROOM + Random_Next(&rnd, 2));
		patchX = Random_Next(&rnd, World.Width);
		patchY = Random_Next(&rnd, World.Height);
		patchZ = Random_Next(&rnd, World.Length);

		for (j = 0; j < 20; j++) {
			mushX = patchX; mushY = patchY; mushZ = patchZ;
			for (k = 0; k < 5; k++) {
				mushX += Random_Next(&rnd, 6) - Random_Next(&rnd, 6);
				mushZ += Random_Next(&rnd, 6) - Random_Next(&rnd, 6);

				if (!World_ContainsXZ(mushX, mushZ)) continue;
				groundHeight = heightmap[mushZ * World.Width + mushX];
				if (mushY >= (groundHeight - 1)) continue;

				index = World_Pack(mushX, mushY, mushZ);
				if (Gen_Blocks[index] == BLOCK_AIR && Gen_Blocks[index - World.OneY] == Gen_GetTheme()->stoneBlock)
					Gen_Blocks[index] = block;
			}
		}
	}
}

static void NotchyGen_PlantTrees(void) {
	int numPatches, numCactiPatches;
	int patchX, patchZ;
	int treeX, treeY, treeZ;
	int treeHeight, index, count;
	BlockRaw under;
	int i, j, k, m;
	int cactusH, cy;
	cc_bool isJungle = Gen_GetTheme()->hasJungleTrees;

	IVec3 coords_small[TREE_MAX_COUNT];
	BlockRaw blocks_small[TREE_MAX_COUNT];
	IVec3 coords_jungle[JUNGLE_TREE_MAX_COUNT];
	BlockRaw blocks_jungle[JUNGLE_TREE_MAX_COUNT];
	IVec3* coords;
	BlockRaw* blocks;

	Tree_Blocks = Gen_Blocks;
	Tree_Rnd    = &rnd;

	/* ----- Tree patches ----- */
	numPatches = World.Width * World.Length / 4000;
	numPatches *= Gen_GetTheme()->treePatchMul;

	Gen_CurrentState = Gen_GetTheme()->treePlantMsg;
	for (i = 0; i < numPatches; i++) {
		Gen_CurrentProgress = (float)i / numPatches;

		patchX = Random_Next(&rnd, World.Width);
		patchZ = Random_Next(&rnd, World.Length);

		for (j = 0; j < 20; j++) {
			treeX = patchX; treeZ = patchZ;
			for (k = 0; k < 20; k++) {
				treeX += Random_Next(&rnd, 6) - Random_Next(&rnd, 6);
				treeZ += Random_Next(&rnd, 6) - Random_Next(&rnd, 6);

				if (!World_ContainsXZ(treeX, treeZ) || Random_Float(&rnd) >= 0.25f) continue;
				treeY = heightmap[treeZ * World.Width + treeX] + 1;
				if (treeY >= World.Height) continue;

				index = World_Pack(treeX, treeY, treeZ);
				under = treeY > 0 ? Gen_Blocks[index - World.OneY] : BLOCK_AIR;

				if (under != Gen_GetTheme()->surfaceBlock && !(Gen_GetTheme()->treesOnDirt && under == Gen_GetTheme()->fillBlock))
					continue;

				/* Jungle theme: 30% chance for large 2x2 jungle tree */
				if (isJungle && Random_Float(&rnd) < 0.30f) {
					treeHeight = 18 + Random_Next(&rnd, 11); /* 18-28 blocks tall */
					coords = coords_jungle;
					blocks = blocks_jungle;
					if (JungleTreeGen_CanGrow(treeX, treeY, treeZ, treeHeight)) {
						count = JungleTreeGen_Grow(treeX, treeY, treeZ, treeHeight, coords, blocks);
						for (m = 0; m < count; m++) {
							index = World_Pack(coords[m].x, coords[m].y, coords[m].z);
							Gen_Blocks[index] = blocks[m];
						}
					}
				} else {
					treeHeight = 5 + Random_Next(&rnd, 3);
					coords = coords_small;
					blocks = blocks_small;
					if (TreeGen_CanGrow(treeX, treeY, treeZ, treeHeight)) {
						count = TreeGen_Grow(treeX, treeY, treeZ, treeHeight, coords, blocks);
						for (m = 0; m < count; m++) {
							index = World_Pack(coords[m].x, coords[m].y, coords[m].z);
							Gen_Blocks[index] = blocks[m];
						}
					}
				}
			}
		}
	}

	/* ----- Cacti patches (independent from trees) ----- */
	numCactiPatches = World.Width * World.Length / 4000;
	numCactiPatches *= Gen_GetTheme()->cactiPatchMul;

	if (numCactiPatches > 0) {
		Gen_CurrentState = "Planting cacti";
		for (i = 0; i < numCactiPatches; i++) {
			Gen_CurrentProgress = (float)i / numCactiPatches;

			patchX = Random_Next(&rnd, World.Width);
			patchZ = Random_Next(&rnd, World.Length);

			for (j = 0; j < 20; j++) {
				treeX = patchX; treeZ = patchZ;
				for (k = 0; k < 20; k++) {
					treeX += Random_Next(&rnd, 6) - Random_Next(&rnd, 6);
					treeZ += Random_Next(&rnd, 6) - Random_Next(&rnd, 6);

					if (!World_ContainsXZ(treeX, treeZ) || Random_Float(&rnd) >= 0.25f) continue;
					treeY = heightmap[treeZ * World.Width + treeX] + 1;
					if (treeY >= World.Height) continue;

					index = World_Pack(treeX, treeY, treeZ);
					under = treeY > 0 ? Gen_Blocks[index - World.OneY] : BLOCK_AIR;

					if (under == Gen_GetTheme()->surfaceBlock) {
						cactusH = 1 + Random_Next(&rnd, 3);
						for (cy = 0; cy < cactusH; cy++) {
							if (treeY + cy >= World.Height) break;
							index = World_Pack(treeX, treeY + cy, treeZ);
							if (Gen_Blocks[index] != BLOCK_AIR) break;
							Gen_Blocks[index] = BLOCK_CACTUS;
						}
					}
				}
			}
		}
	}

	if (Gen_GetTheme()->hasOases) {
		int numOases = World.Width * World.Length / 8000;
		int ox, oz, oy, oRadius, dx, dz;
		if (numOases < 3) numOases = 3;
		Gen_CurrentState = "Planting oases";
		for (i = 0; i < numOases; i++) {
			Gen_CurrentProgress = (float)i / numOases;
			ox = Random_Next(&rnd, World.Width);
			oz = Random_Next(&rnd, World.Length);
			oRadius = 6 + Random_Next(&rnd, 5); /* 6-10 block radius */

			/* Convert sand to grass in oasis patch */
			for (dz = -oRadius; dz <= oRadius; dz++) {
				for (dx = -oRadius; dx <= oRadius; dx++) {
					if (dx * dx + dz * dz > oRadius * oRadius) continue;
					if (!World_ContainsXZ(ox + dx, oz + dz)) continue;
					oy = heightmap[(oz + dz) * World.Width + (ox + dx)];
					if (oy < 0 || oy >= World.Height) continue;
					index = World_Pack(ox + dx, oy, oz + dz);
					if (Gen_Blocks[index] == BLOCK_SAND) {
						Gen_Blocks[index] = BLOCK_GRASS;
						if (oy >= 1) Gen_Blocks[World_Pack(ox + dx, oy - 1, oz + dz)] = BLOCK_DIRT;
					}
				}
			}
			/* Place flowers in oasis */
			for (m = 0; m < 15; m++) {
				int fx = ox + Random_Next(&rnd, oRadius * 2) - oRadius;
				int fz = oz + Random_Next(&rnd, oRadius * 2) - oRadius;
				int fy;
				if (!World_ContainsXZ(fx, fz)) continue;
				fy = heightmap[fz * World.Width + fx] + 1;
				if (fy <= 0 || fy >= World.Height) continue;
				index = World_Pack(fx, fy, fz);
				if (Gen_Blocks[index] == BLOCK_AIR && Gen_Blocks[index - World.OneY] == BLOCK_GRASS)
					Gen_Blocks[index] = (BlockRaw)(BLOCK_DANDELION + Random_Next(&rnd, 2));
			}
			/* Plant trees in oasis */
			for (m = 0; m < 4; m++) {
				int tx = ox + Random_Next(&rnd, oRadius) - oRadius / 2;
				int tz = oz + Random_Next(&rnd, oRadius) - oRadius / 2;
				int ty;
				if (!World_ContainsXZ(tx, tz)) continue;
				ty = heightmap[tz * World.Width + tx] + 1;
				if (ty <= 0 || ty >= World.Height) continue;
				index = World_Pack(tx, ty, tz);
				if (Gen_Blocks[index - World.OneY] == BLOCK_GRASS) {
					treeHeight = 5 + Random_Next(&rnd, 3);
					if (TreeGen_CanGrow(tx, ty, tz, treeHeight)) {
						count = TreeGen_Grow(tx, ty, tz, treeHeight, coords_small, blocks_small);
						for (j = 0; j < count; j++) {
							index = World_Pack(coords_small[j].x, coords_small[j].y, coords_small[j].z);
							Gen_Blocks[index] = blocks_small[j];
						}
					}
				}
			}
		}
	}
}

static cc_bool NotchyGen_Prepare(int seed) {
	Random_Seed(&rnd, seed);
	waterLevel = World.Height / 2;
	if (Gen_GetTheme()->raiseWaterLevel)
		waterLevel += World.Height / 8;
	minHeight  = World.Height;

	heightmap  = (cc_int16*)Mem_TryAlloc(World.Width * World.Length, 2);
	return heightmap != NULL;
}

static void NotchyGen_Generate(void) {
	GEN_COOP_BEGIN
		GEN_COOP_STEP( 0, NotchyGen_CreateHeightmap() );
		GEN_COOP_STEP( 1, NotchyGen_CreateStrata() );
		GEN_COOP_STEP( 2, NotchyGen_CarveCaves() );
		GEN_COOP_STEP( 3, NotchyGen_CarveAllOres() );

		GEN_COOP_STEP( 4, NotchyGen_FloodFillWaterBorders() );
		GEN_COOP_STEP( 5, NotchyGen_FloodFillWater() );
		GEN_COOP_STEP( 6, NotchyGen_FloodFillLava() );

		GEN_COOP_STEP( 7, NotchyGen_CreateSurfaceLayer() );
		GEN_COOP_STEP( 8, NotchyGen_PlantFlowers() );
		GEN_COOP_STEP( 9, NotchyGen_PlantMushrooms() );
		GEN_COOP_STEP(10, NotchyGen_PlantTrees() );
		GEN_COOP_STEP(11, NotchyGen_PlaceSnowLayer() );
	GEN_COOP_END

	Mem_Free(heightmap);
	heightmap = NULL;

	gen_done  = true;
}

static void NotchyGen_Setup(void) {
	GenTheme_ApplyEnvironment();
}

const struct MapGenerator NotchyGen = {
	NotchyGen_Prepare,
	NotchyGen_Generate,
	NotchyGen_Setup
};


/*########################################################################################################################*
*---------------------------------------------------Floating island gen---------------------------------------------------*
*#########################################################################################################################*/
/* Floating island generator inspired by Minecraft Indev's "Floating" world type.
   Generates multiple layers of terrain, then carves out everything below a noise-derived
   cutoff per column, leaving floating islands suspended in the air. */

static cc_int16* floatCutoff; /* per-column bottom cutoff Y */
static int floatNumLayers;

/* Finds available vertical space above a position for jungle tree growth in floating world.
   Returns the number of clear blocks above treeY (up to maxHeight).
   Stops counting when hitting a solid block (island above). */
static int FloatingGen_FindVerticalSpace(int x, int y, int z, int maxHeight) {
	int availableSpace = 0;
	int checkY, index;

	for (checkY = y; checkY < y + maxHeight && checkY < World.Height; checkY++) {
		index = World_Pack(x, checkY, z);
		if (Gen_Blocks[index] != BLOCK_AIR) {
			/* Hit a solid block - this is the ceiling */
			return availableSpace;
		}
		availableSpace++;
	}
	return availableSpace;
}

/* Generate one layer of floating islands centered at the given Y level */
static void FloatingGen_GenLayer(int layer, int layerBaseY) {
	int mapArea = World.Width * World.Length;
	int hIndex, x, z, y, index;
	float hLow, hHigh, height;
	int adjHeight, dirtHeight, stoneHeight, dirtThickness;
	float edgeX, edgeZ, edge, noise, sqrtVal, cutoffF;
	int cutoff, maxY = World.MaxY;
	int treeHeight, count;
	BlockRaw under, block, above;
	int numPatches, patchX, patchZ, treeX, treeY, treeZ;
	int flowerX, flowerY, flowerZ;
	int i, j, k, m;
	int availableSpace;
	int cactusH, cy;
	cc_bool isJungle = Gen_GetTheme()->hasJungleTrees;
	IVec3 coords_small[TREE_MAX_COUNT];
	BlockRaw blocks_small[TREE_MAX_COUNT];
	IVec3 coords_jungle[JUNGLE_TREE_MAX_COUNT];
	BlockRaw blocks_jungle[JUNGLE_TREE_MAX_COUNT];
	IVec3* coords;
	BlockRaw* blocks;

#if CC_BUILD_MAXSTACK <= (16 * 1024)
	struct NoiseBuffer { 
		struct CombinedNoise n1, n2;
		struct OctaveNoise n3, nCutoff, nStrata;
	};
	void* mem = TempMem_Alloc(sizeof(struct NoiseBuffer));
	struct NoiseBuffer* buf  = (struct NoiseBuffer*)mem;
	struct CombinedNoise* n1 = &buf->n1;
	struct CombinedNoise* n2 = &buf->n2;
	struct OctaveNoise*   n3 = &buf->n3;
	struct OctaveNoise*   nCutoff = &buf->nCutoff;
	struct OctaveNoise*   nStrata = &buf->nStrata;
#else
	struct CombinedNoise _n1, *n1 = &_n1;
	struct CombinedNoise _n2, *n2 = &_n2;
	struct OctaveNoise   _n3, *n3 = &_n3;
	struct OctaveNoise   _nCutoff, *nCutoff = &_nCutoff;
	struct OctaveNoise   _nStrata, *nStrata = &_nStrata;
#endif

	/* ----- Heightmap for this layer ----- */
	CombinedNoise_Init(n1, &rnd, 8, 8);
	CombinedNoise_Init(n2, &rnd, 8, 8);
	OctaveNoise_Init(n3,   &rnd, 6);

	Gen_CurrentState = "Building heightmap";
	hIndex = 0;
	for (z = 0; z < World.Length; z++) {
		Gen_CurrentProgress = (float)z / World.Length;
		for (x = 0; x < World.Width; x++) {
			hLow   = CombinedNoise_Calc(n1, x * 1.3f, z * 1.3f) / 6 - 4;
			height = hLow;

			if (OctaveNoise_Calc(n3, (float)x, (float)z) <= 0) {
				hHigh = CombinedNoise_Calc(n2, x * 1.3f, z * 1.3f) / 5 + 6;
				height = max(hLow, hHigh);
			}

			height *= 0.5f;
			if (height < 0) height *= 0.8f;

			adjHeight = (int)(height + layerBaseY);
			adjHeight = min(adjHeight, maxY);
			adjHeight = max(adjHeight, 0);
			heightmap[hIndex++] = adjHeight;
		}
	}

	/* ----- Compute bottom cutoffs (island undersides) ----- */
	OctaveNoise_Init(nCutoff, &rnd, 8);

	Gen_CurrentState = "Shaping islands";
	hIndex = 0;
	for (z = 0; z < World.Length; z++) {
		Gen_CurrentProgress = (float)z / World.Length;
		for (x = 0; x < World.Width; x++) {
			/* Edge falloff: 0 at center, approaches 1 at borders */
			edgeX = Math_AbsF((float)x / World.Width  * 2.0f - 1.0f);
			edgeZ = Math_AbsF((float)z / World.Length * 2.0f - 1.0f);
			edge  = max(edgeX, edgeZ);
			edge  = edge * edge * edge; /* cubic falloff */

			/* Sample noise at lower frequency for bigger island/gap features */
			noise = OctaveNoise_Calc(nCutoff, x * 1.2f, z * 1.2f) / 24.0f;

			/* Transform: sqrt(abs(noise)) * sign(noise) * 40 + base */
			/* Larger multiplier = thicker islands, deeper underbellies */
			sqrtVal = Math_SqrtF(Math_AbsF(noise));
			if (noise < 0) sqrtVal = -sqrtVal;
			cutoffF = sqrtVal * 40.0f + layerBaseY - 8;

			/* Blend toward world height at edges (thinner/no islands at borders) */
			cutoffF = cutoffF * (1.0f - edge) + edge * World.Height;

			/* Columns where cutoff exceeds the surface become gaps between islands */
			cutoff = (int)cutoffF;
			if (cutoff > layerBaseY + 4) cutoff = World.Height;

			floatCutoff[hIndex++] = (cc_int16)cutoff;
		}
	}

	/* ----- Fill strata (stone + dirt) ----- */
	OctaveNoise_Init(nStrata, &rnd, 8);

	Gen_CurrentState = "Creating strata";
	hIndex = 0;
	for (z = 0; z < World.Length; z++) {
		Gen_CurrentProgress = (float)z / World.Length;
		for (x = 0; x < World.Width; x++) {
			dirtThickness = (int)(OctaveNoise_Calc(nStrata, (float)x, (float)z) / 24 - 4);
			dirtHeight    = heightmap[hIndex];
			stoneHeight   = dirtHeight + dirtThickness;
			cutoff        = floatCutoff[hIndex];
			hIndex++;

			stoneHeight = min(stoneHeight, maxY);
			dirtHeight  = min(dirtHeight,  maxY);

			/* Fill stone from cutoff to stoneHeight */
			for (y = cutoff; y <= stoneHeight; y++) {
				if (y < 0 || y > maxY) continue;
				index = World_Pack(x, y, z);
				if (Gen_Blocks[index] == BLOCK_AIR)
					Gen_Blocks[index] = Gen_GetTheme()->stoneBlock;
			}

			/* Fill dirt from stoneHeight+1 to dirtHeight */
			for (y = max(stoneHeight + 1, cutoff); y <= dirtHeight; y++) {
				if (y < 0 || y > maxY) continue;
				index = World_Pack(x, y, z);
				if (Gen_Blocks[index] == BLOCK_AIR)
					Gen_Blocks[index] = Gen_GetTheme()->fillBlock;
			}
		}
	}

	/* ----- Surface layer ----- */
	Gen_CurrentState = "Creating surface";
	hIndex = 0;
	for (z = 0; z < World.Length; z++) {
		Gen_CurrentProgress = (float)z / World.Length;
		for (x = 0; x < World.Width; x++) {
			y = heightmap[hIndex++];
			if (y < 0 || y >= World.Height) continue;
			index = World_Pack(x, y, z);
			if (Gen_Blocks[index] != Gen_GetTheme()->fillBlock && Gen_Blocks[index] != Gen_GetTheme()->stoneBlock) continue;
			above = y >= World.MaxY ? BLOCK_AIR : Gen_Blocks[index + World.OneY];
			if (above == BLOCK_AIR) {
				Gen_Blocks[index] = Gen_GetTheme()->surfaceBlock;
			}
		}
	}

	numPatches       = World.Width * World.Length / 3000;
	numPatches      *= Gen_GetTheme()->flowerPatchMul;
	if (Gen_GetTheme()->generateFlowers) {
	Gen_CurrentState = "Planting flowers";
	for (i = 0; i < numPatches; i++) {
		Gen_CurrentProgress = (float)i / numPatches;
		block  = (BlockRaw)(BLOCK_DANDELION + Random_Next(&rnd, 2));
		patchX = Random_Next(&rnd, World.Width);
		patchZ = Random_Next(&rnd, World.Length);

		for (j = 0; j < 10; j++) {
			flowerX = patchX; flowerZ = patchZ;
			for (k = 0; k < 5; k++) {
				flowerX += Random_Next(&rnd, 6) - Random_Next(&rnd, 6);
				flowerZ += Random_Next(&rnd, 6) - Random_Next(&rnd, 6);
				if (!World_ContainsXZ(flowerX, flowerZ)) continue;
				flowerY = heightmap[flowerZ * World.Width + flowerX] + 1;
				if (flowerY <= 0 || flowerY >= World.Height) continue;
				index = World_Pack(flowerX, flowerY, flowerZ);
				if (Gen_Blocks[index] == BLOCK_AIR && Gen_Blocks[index - World.OneY] == Gen_GetTheme()->surfaceBlock)
					Gen_Blocks[index] = block;
			}
		}
	}
	}

	/* ----- Trees ----- */
	Tree_Blocks = Gen_Blocks;
	Tree_Rnd    = &rnd;
	numPatches       = World.Width * World.Length / 4000;
	numPatches      *= Gen_GetTheme()->treePatchMul;
	Gen_CurrentState = Gen_GetTheme()->treePlantMsg;

	for (i = 0; i < numPatches; i++) {
		Gen_CurrentProgress = (float)i / numPatches;
		patchX = Random_Next(&rnd, World.Width);
		patchZ = Random_Next(&rnd, World.Length);

		for (j = 0; j < 20; j++) {
			treeX = patchX; treeZ = patchZ;
			for (k = 0; k < 20; k++) {
				treeX += Random_Next(&rnd, 6) - Random_Next(&rnd, 6);
				treeZ += Random_Next(&rnd, 6) - Random_Next(&rnd, 6);
				if (!World_ContainsXZ(treeX, treeZ) || Random_Float(&rnd) >= 0.25f) continue;
				treeY = heightmap[treeZ * World.Width + treeX] + 1;
				if (treeY >= World.Height) continue;
				index = World_Pack(treeX, treeY, treeZ);
				under = treeY > 0 ? Gen_Blocks[index - World.OneY] : BLOCK_AIR;

				if (under != Gen_GetTheme()->surfaceBlock && !(Gen_GetTheme()->treesOnDirt && under == Gen_GetTheme()->fillBlock))
					continue;

				/* Jungle theme: 30% chance for large 2x2 jungle tree */
				if (isJungle && Random_Float(&rnd) < 0.30f) {
					/* Check available vertical space above the tree position */
					availableSpace = FloatingGen_FindVerticalSpace(treeX, treeY, treeZ, 30);

					/* Follow height protocol for floating islands:
					   - 18+ blocks: spawn normally (18-28 blocks tall)
					   - 12-17 blocks: shrink tree to fit available space
					   - <12 blocks: don't spawn the tree */
					if (availableSpace >= 18) {
						/* Full height jungle tree */
						treeHeight = 18 + Random_Next(&rnd, 11); /* 18-28 blocks tall */
						if (treeHeight > availableSpace) treeHeight = availableSpace;
					} else if (availableSpace >= 12) {
						/* Shrink to fit */
						treeHeight = availableSpace;
					} else {
						/* Not enough space - skip this tree */
						continue;
					}

					coords = coords_jungle;
					blocks = blocks_jungle;
					if (JungleTreeGen_CanGrow(treeX, treeY, treeZ, treeHeight)) {
						count = JungleTreeGen_Grow(treeX, treeY, treeZ, treeHeight, coords, blocks);
						for (m = 0; m < count; m++) {
							index = World_Pack(coords[m].x, coords[m].y, coords[m].z);
							Gen_Blocks[index] = blocks[m];
						}
					}
				} else {
					/* Normal tree: 5-7 blocks tall */
					treeHeight = 5 + Random_Next(&rnd, 3);
					coords = coords_small;
					blocks = blocks_small;
					if (TreeGen_CanGrow(treeX, treeY, treeZ, treeHeight)) {
						count = TreeGen_Grow(treeX, treeY, treeZ, treeHeight, coords, blocks);
						for (m = 0; m < count; m++) {
							index = World_Pack(coords[m].x, coords[m].y, coords[m].z);
							Gen_Blocks[index] = blocks[m];
						}
					}
				}
			}
		}
	}

	/* ----- Cacti patches (independent from trees) ----- */
	numPatches = World.Width * World.Length / 4000;
	numPatches *= Gen_GetTheme()->cactiPatchMul;

	if (numPatches > 0) {
		Gen_CurrentState = "Planting cacti";
		for (i = 0; i < numPatches; i++) {
			Gen_CurrentProgress = (float)i / numPatches;
			patchX = Random_Next(&rnd, World.Width);
			patchZ = Random_Next(&rnd, World.Length);

			for (j = 0; j < 20; j++) {
				treeX = patchX; treeZ = patchZ;
				for (k = 0; k < 20; k++) {
					treeX += Random_Next(&rnd, 6) - Random_Next(&rnd, 6);
					treeZ += Random_Next(&rnd, 6) - Random_Next(&rnd, 6);
					if (!World_ContainsXZ(treeX, treeZ) || Random_Float(&rnd) >= 0.25f) continue;
					treeY = heightmap[treeZ * World.Width + treeX] + 1;
					if (treeY >= World.Height) continue;
					index = World_Pack(treeX, treeY, treeZ);
					under = treeY > 0 ? Gen_Blocks[index - World.OneY] : BLOCK_AIR;

					if (under == Gen_GetTheme()->surfaceBlock) {
						cactusH = 1 + Random_Next(&rnd, 3);
						for (cy = 0; cy < cactusH; cy++) {
							if (treeY + cy >= World.Height) break;
							index = World_Pack(treeX, treeY + cy, treeZ);
							if (Gen_Blocks[index] != BLOCK_AIR) break;
							Gen_Blocks[index] = BLOCK_CACTUS;
						}
					}
				}
			}
		}
	}
	
	if (Gen_GetTheme()->hasSnowLayer) {
		Gen_CurrentState = "Placing snow layer";
		hIndex = 0;
		for (z = 0; z < World.Length; z++) {
			Gen_CurrentProgress = (float)z / World.Length;
			for (x = 0; x < World.Width; x++) {
				y = heightmap[hIndex++];
				if (y < 0 || y >= World.MaxY) continue;
				
				/* Only place snow if there's actually a solid block below it */
				index = World_Pack(x, y, z);
				if (Gen_Blocks[index] == BLOCK_AIR) continue;
				
				index = World_Pack(x, y + 1, z);
				above = (y + 1 >= World.Height) ? BLOCK_AIR : Gen_Blocks[index];
				if (above == BLOCK_AIR) {
					Gen_Blocks[index] = BLOCK_SNOW;
					/* Convert grass below to snowy grass */
					{ int belowIdx = World_Pack(x, y, z);
					  if (Gen_Blocks[belowIdx] == BLOCK_GRASS) Gen_Blocks[belowIdx] = BLOCK_SNOWY_GRASS; }
				}
			}
		}
	}

	if (Gen_GetTheme()->hasSnowLayer) {
		Gen_CurrentState = "Placing snow on trees";
		for (z = 0; z < World.Length; z++) {
			Gen_CurrentProgress = (float)z / World.Length;
			for (x = 0; x < World.Width; x++) {
				for (y = 0; y < World.Height - 1; y++) {
					index = World_Pack(x, y, z);
					block = Gen_Blocks[index];
					if (block == BLOCK_LEAVES) {
						index = World_Pack(x, y + 1, z);
						above = Gen_Blocks[index];
						if (above == BLOCK_AIR) {
							Gen_Blocks[index] = BLOCK_SNOW;
						}
					}
				}
			}
		}
	}
}

static cc_bool FloatingGen_Prepare(int seed) {
	int mapArea = World.Width * World.Length;
	Random_Seed(&rnd, seed);
	waterLevel = World.Height / 2;
	if (Gen_GetTheme()->raiseWaterLevel)
		waterLevel += World.Height / 8;
	minHeight  = World.Height;

	/* Calculate number of layers based on world height */
	floatNumLayers = (World.Height - 64) / 48 + 1;
	if (floatNumLayers < 1) floatNumLayers = 1;
	if (floatNumLayers > 4) floatNumLayers = 4;

	heightmap   = (cc_int16*)Mem_TryAlloc(mapArea, 2);
	if (!heightmap) return false;
	floatCutoff = (cc_int16*)Mem_TryAlloc(mapArea, 2);
	if (!floatCutoff) { Mem_Free(heightmap); heightmap = NULL; return false; }
	return true;
}

/* Find a spawn point on a floating island near the center of the map */
static void FloatingGen_FindSpawn(void) {
	int cx = World.Width / 2;
	int cz = World.Length / 2;
	int radius, x, y, z, index;

	Gen_CurrentState = "Finding spawn";
	/* Search outward from center, scanning downward from top */
	for (radius = 0; radius < World.Width / 2; radius += 2) {
		for (y = World.Height - 1; y > 0; y--) {
			for (x = cx - radius; x <= cx + radius; x++) {
				for (z = cz - radius; z <= cz + radius; z++) {
					if (!World_ContainsXZ(x, z)) continue;
					/* Only check perimeter of each radius ring to avoid redundancy */
					if (radius > 0 && x != cx - radius && x != cx + radius &&
					    z != cz - radius && z != cz + radius) continue;

					index = World_Pack(x, y, z);
					/* Need: solid block, air above */
					if (Gen_Blocks[index] == BLOCK_AIR) continue;
					if (y + 1 >= World.Height || Gen_Blocks[index + World.OneY] != BLOCK_AIR) continue;

					Gen_SpawnOverride.x = (float)x + 0.5f;
					Gen_SpawnOverride.y = (float)(y + 1);
					Gen_SpawnOverride.z = (float)z + 0.5f;
					return;
				}
			}
		}
	}
	
	Gen_SpawnOverride.y = -1.0f; /* reset if not found */
}

static void FloatingGen_Generate(void) {
	int layer;

	/* Fill entire map with air */
	Mem_Set(Gen_Blocks, BLOCK_AIR, World.Volume);

	/* Generate each layer of floating islands */
	for (layer = 0; layer < floatNumLayers; layer++) {
		int layerBaseY = World.Height - 32 - layer * 48;
		if (layerBaseY < 16) layerBaseY = 16;

		Gen_CurrentState = "Generating island layer";
		FloatingGen_GenLayer(layer, layerBaseY);
	}

	/* Carve caves and ore veins through all layers */
	NotchyGen_CarveCaves();
	NotchyGen_CarveAllOres();

	FloatingGen_FindSpawn();

	Mem_Free(heightmap);   heightmap   = NULL;
	Mem_Free(floatCutoff); floatCutoff = NULL;

	gen_done = true;
}

static void FloatingGen_Setup(void) {
	GenTheme_ApplyEnvironment();
	/* Floating world always uses invisible borders regardless of theme */
	Env_SetEdgeBlock(BLOCK_AIR);
	Env_SetSidesBlock(BLOCK_AIR);
	Env_SetCloudsHeight(-16);
}

const struct MapGenerator FloatingGen = {
	FloatingGen_Prepare,
	FloatingGen_Generate,
	FloatingGen_Setup
};


/*########################################################################################################################*
*-----------------------------------------------------Caves world gen-----------------------------------------------------*
*#########################################################################################################################*/
/* Caves world generator: fills the entire world with stone, then carves extensive
   cave systems and large caverns throughout. Player spawns inside a cave. */

Vec3 Gen_SpawnOverride = { 0, -1.0f, 0 };

static void CavesGen_FillStone(void) {
	BlockRaw fillBlock = Gen_GetTheme()->caveFillBlock;
	Gen_CurrentState = "Filling world";
	Mem_Set(Gen_Blocks, fillBlock, World.Volume);
}

static void CavesGen_CarveTunnels(void) {
	int cavesCount, caveLen;
	float caveX, caveY, caveZ;
	float theta, deltaTheta, phi, deltaPhi;
	float caveRadius, radius;
	int cenX, cenY, cenZ;
	int i, j;

	/* Tunnels that connect the caverns - slightly more than normal gen */
	cavesCount       = World.Volume / 4096;
	Gen_CurrentState = "Carving tunnels";
	for (i = 0; i < cavesCount; i++) {
		Gen_CurrentProgress = (float)i / cavesCount;

		caveX = (float)Random_Next(&rnd, World.Width);
		caveY = (float)Random_Next(&rnd, World.Height);
		caveZ = (float)Random_Next(&rnd, World.Length);

		caveLen    = (int)(Random_Float(&rnd) * Random_Float(&rnd) * 250.0f);
		theta      = Random_Float(&rnd) * 2.0f * MATH_PI; deltaTheta = 0.0f;
		phi        = Random_Float(&rnd) * 2.0f * MATH_PI; deltaPhi   = 0.0f;
		caveRadius = Random_Float(&rnd) * Random_Float(&rnd);

		for (j = 0; j < caveLen; j++) {
			caveX += Math_SinF(theta) * Math_CosF(phi);
			caveZ += Math_CosF(theta) * Math_CosF(phi);
			caveY += Math_SinF(phi);

			theta      = theta + deltaTheta * 0.2f;
			deltaTheta = deltaTheta * 0.9f + Random_Float(&rnd) - Random_Float(&rnd);
			phi        = phi * 0.5f + deltaPhi * 0.25f;
			deltaPhi   = deltaPhi  * 0.75f + Random_Float(&rnd) - Random_Float(&rnd);
			if (Random_Float(&rnd) < 0.25f) continue;

			cenX = (int)(caveX + (Random_Next(&rnd, 4) - 2) * 0.2f);
			cenY = (int)(caveY + (Random_Next(&rnd, 4) - 2) * 0.2f);
			cenZ = (int)(caveZ + (Random_Next(&rnd, 4) - 2) * 0.2f);

			radius = 1.2f + (0.5f + caveRadius * 2.0f);
			radius = radius * Math_SinF(j * MATH_PI / caveLen);
			NotchyGen_FillOblateSpheroid(cenX, cenY, cenZ, radius, BLOCK_AIR);
		}
	}
}

static void CavesGen_CarveCaverns(void) {
	int numCaverns, i;
	int cenX, cenY, cenZ;
	float radiusH, radiusV;
	float dx, dy, dz;
	int minX, maxX, minY, maxY, minZ, maxZ, index;
	int x, y, z;
	int hasGrass, floorY;
	int treeX, treeZ, treeY, treeHeight, count, m;
	int fx, fz, fy;
	BlockRaw flowerBlock;
	IVec3 coords[TREE_MAX_COUNT];
	BlockRaw blocks[TREE_MAX_COUNT];

	/* Large open cavern rooms */
	numCaverns       = World.Volume / 32768;
	if (numCaverns < 4) numCaverns = 4;
	Gen_CurrentState = "Carving caverns";
	for (i = 0; i < numCaverns; i++) {
		Gen_CurrentProgress = (float)i / numCaverns;

		cenX = Random_Next(&rnd, World.Width);
		cenY = Random_Next(&rnd, World.Height);
		cenZ = Random_Next(&rnd, World.Length);

		radiusH = 8.0f + Random_Float(&rnd) * 16.0f;  /* 8-24 horizontal */
		radiusV = 5.0f + Random_Float(&rnd) * 10.0f;   /* 5-15 vertical */

		minX = max(0, cenX - (int)radiusH - 1);
		maxX = min(World.MaxX, cenX + (int)radiusH + 1);
		minY = max(0, cenY - (int)radiusV - 1);
		maxY = min(World.MaxY, cenY + (int)radiusV + 1);
		minZ = max(0, cenZ - (int)radiusH - 1);
		maxZ = min(World.MaxZ, cenZ + (int)radiusH + 1);

		/* Carve the ellipsoid */
		for (y = minY; y <= maxY; y++) {
			for (z = minZ; z <= maxZ; z++) {
				for (x = minX; x <= maxX; x++) {
					dx = (float)(x - cenX) / radiusH;
					dy = (float)(y - cenY) / radiusV;
					dz = (float)(z - cenZ) / radiusH;
					if (dx * dx + dy * dy + dz * dz < 1.0f) {
						index = World_Pack(x, y, z);
						Gen_Blocks[index] = BLOCK_AIR;
					}
				}
			}
		}

		floorY = cenY - (int)radiusV;
		if (floorY < 1 || floorY >= World.MaxY) continue;

		/* ~40% of caverns get garden floors with trees/cacti and flowers */
		hasGrass = Random_Float(&rnd) < 0.4f;
		if (!Gen_GetTheme()->hasCaveGardens) hasGrass = 0;

		if (hasGrass) {
			BlockRaw gardenSurface = Gen_GetTheme()->gardenSurface;
			BlockRaw gardenFill    = Gen_GetTheme()->gardenFill;

			/* Place surface + fill on the cavern floor */
			for (z = minZ; z <= maxZ; z++) {
				for (x = minX; x <= maxX; x++) {
					dx = (float)(x - cenX) / radiusH;
					dz = (float)(z - cenZ) / radiusH;
					if (dx * dx + dz * dz >= 0.85f) continue;

					for (y = floorY; y <= floorY + 3; y++) {
						if (y < 1 || y >= World.MaxY) continue;
						index = World_Pack(x, y, z);
						if (Gen_Blocks[index] != BLOCK_AIR) continue;
						if (Gen_Blocks[index - World.OneY] == BLOCK_AIR) continue;

						Gen_Blocks[index - World.OneY] = gardenSurface;
						if (y >= 2) {
							index = World_Pack(x, y - 2, z);
							if (Gen_Blocks[index] != BLOCK_AIR && Gen_Blocks[index] != BLOCK_BEDROCK)
								Gen_Blocks[index] = gardenFill;
						}
						break;
					}
				}
			}

			if (Gen_GetTheme()->cactiPatchMul > 0) {
				for (m = 0; m < 12; m++) {
					int cactusH, cy;
					fx = cenX - (int)radiusH / 2 + Random_Next(&rnd, (int)radiusH);
					fz = cenZ - (int)radiusH / 2 + Random_Next(&rnd, (int)radiusH);
					if (!World_ContainsXZ(fx, fz)) continue;

					for (fy = floorY; fy <= floorY + 4; fy++) {
						if (fy < 1 || fy >= World.Height) continue;
						index = World_Pack(fx, fy, fz);
						if (Gen_Blocks[index] == BLOCK_AIR && Gen_Blocks[index - World.OneY] == BLOCK_SAND) {
							cactusH = 1 + Random_Next(&rnd, 3);
							for (cy = 0; cy < cactusH; cy++) {
								if (fy + cy >= World.Height) break;
								index = World_Pack(fx, fy + cy, fz);
								if (Gen_Blocks[index] != BLOCK_AIR) break;
								Gen_Blocks[index] = BLOCK_CACTUS;
							}
							break;
						}
					}
				}
			} else {
				/* Normal/Woods/Paradise: flowers and trees in garden */
				for (m = 0; m < 12; m++) {
					fx = cenX - (int)radiusH / 2 + Random_Next(&rnd, (int)radiusH);
					fz = cenZ - (int)radiusH / 2 + Random_Next(&rnd, (int)radiusH);
					if (!World_ContainsXZ(fx, fz)) continue;
					flowerBlock = (BlockRaw)(BLOCK_DANDELION + Random_Next(&rnd, 2));

					for (fy = floorY; fy <= floorY + 4; fy++) {
						if (fy < 1 || fy >= World.Height) continue;
						index = World_Pack(fx, fy, fz);
						if (Gen_Blocks[index] == BLOCK_AIR && Gen_Blocks[index - World.OneY] == BLOCK_GRASS) {
							Gen_Blocks[index] = flowerBlock;
							break;
						}
					}
				}

				/* Plant trees in the garden */
				Tree_Blocks = Gen_Blocks;
				Tree_Rnd    = &rnd;
				for (m = 0; m < 5; m++) {
					treeX = cenX - (int)radiusH / 2 + Random_Next(&rnd, (int)radiusH);
					treeZ = cenZ - (int)radiusH / 2 + Random_Next(&rnd, (int)radiusH);
					if (!World_ContainsXZ(treeX, treeZ)) continue;

					for (treeY = floorY; treeY <= floorY + 4; treeY++) {
						if (treeY < 1 || treeY >= World.Height) continue;
						index = World_Pack(treeX, treeY, treeZ);
						if (Gen_Blocks[index] == BLOCK_AIR && Gen_Blocks[index - World.OneY] == BLOCK_GRASS) {
							treeHeight = 5 + Random_Next(&rnd, 3);
							if (TreeGen_CanGrow(treeX, treeY, treeZ, treeHeight)) {
								count = TreeGen_Grow(treeX, treeY, treeZ, treeHeight, coords, blocks);
								for (y = 0; y < count; y++) {
									index = World_Pack(coords[y].x, coords[y].y, coords[y].z);
									Gen_Blocks[index] = blocks[y];
								}
							}
							break;
						}
					}
				}
			}
		} else {
			/* Non-garden room: scatter brown and red mushrooms on the floor */
			BlockRaw mushroomFloor = Gen_GetTheme()->caveFillBlock;
			for (m = 0; m < 8; m++) {
				fx = cenX - (int)radiusH / 2 + Random_Next(&rnd, (int)radiusH);
				fz = cenZ - (int)radiusH / 2 + Random_Next(&rnd, (int)radiusH);
				if (!World_ContainsXZ(fx, fz)) continue;
				flowerBlock = (BlockRaw)(BLOCK_BROWN_SHROOM + Random_Next(&rnd, 2));

				for (fy = floorY; fy <= floorY + 4; fy++) {
					if (fy < 1 || fy >= World.Height) continue;
					index = World_Pack(fx, fy, fz);
					if (Gen_Blocks[index] == BLOCK_AIR && Gen_Blocks[index - World.OneY] == mushroomFloor) {
						Gen_Blocks[index] = flowerBlock;
						break;
					}
				}
			}
		}
	}
}

/* Place lava pools at the very bottom of the world */
static void CavesGen_PlaceLavaPools(void) {
	int x, z, index;

	Gen_CurrentState = "Placing lava";
	for (z = 0; z < World.Length; z++) {
		Gen_CurrentProgress = (float)z / World.Length;
		for (x = 0; x < World.Width; x++) {
			index = World_Pack(x, 0, z);
			/* Bottom 2 layers: replace air with lava */
			if (Gen_Blocks[index] == BLOCK_AIR)
				Gen_Blocks[index] = BLOCK_STILL_LAVA;
			if (World.Height > 1) {
				index = World_Pack(x, 1, z);
				if (Gen_Blocks[index] == BLOCK_AIR)
					Gen_Blocks[index] = BLOCK_STILL_LAVA;
			}
		}
	}
}

/* Replace stone at y=0 and y=MaxY with bedrock, plus bedrock walls on all edges */
static void CavesGen_PlaceBedrock(void) {
	int x, y, z, index;

	Gen_CurrentState = "Placing bedrock";
	/* Floor and ceiling */
	for (z = 0; z < World.Length; z++) {
		for (x = 0; x < World.Width; x++) {
			index = World_Pack(x, 0, z);
			Gen_Blocks[index] = BLOCK_BEDROCK;
			index = World_Pack(x, World.MaxY, z);
			Gen_Blocks[index] = BLOCK_BEDROCK;
		}
	}
	/* Walls on all four edges (x=0, x=MaxX, z=0, z=MaxZ) */
	for (y = 0; y <= World.MaxY; y++) {
		for (x = 0; x < World.Width; x++) {
			Gen_Blocks[World_Pack(x, y, 0)]           = BLOCK_BEDROCK;
			Gen_Blocks[World_Pack(x, y, World.MaxZ)]   = BLOCK_BEDROCK;
		}
		for (z = 0; z < World.Length; z++) {
			Gen_Blocks[World_Pack(0, y, z)]            = BLOCK_BEDROCK;
			Gen_Blocks[World_Pack(World.MaxX, y, z)]   = BLOCK_BEDROCK;
		}
	}
}

/* Find a spawn point inside a cave near the center of the map */
static void CavesGen_FindSpawn(void) {
	int cx = World.Width / 2;
	int cz = World.Length / 2;
	int radius, x, y, z, index;

	Gen_CurrentState = "Finding spawn";
	/* Search outward from center, scanning downward from mid-height */
	for (radius = 0; radius < World.Width / 2; radius += 2) {
		for (y = World.Height / 2; y > 2; y--) {
			for (x = cx - radius; x <= cx + radius; x++) {
				for (z = cz - radius; z <= cz + radius; z++) {
					if (!World_ContainsXZ(x, z)) continue;
					/* Only check perimeter of each radius ring to avoid redundancy */
					if (radius > 0 && x != cx - radius && x != cx + radius &&
					    z != cz - radius && z != cz + radius) continue;

					index = World_Pack(x, y, z);
					/* Need: solid below, air at feet, air at head */
					if (Gen_Blocks[index] != BLOCK_AIR) continue;
					if (y + 1 >= World.Height || Gen_Blocks[index + World.OneY] != BLOCK_AIR) continue;
					if (y <= 0 || Gen_Blocks[index - World.OneY] == BLOCK_AIR) continue;

					Gen_SpawnOverride.x = (float)x + 0.5f;
					Gen_SpawnOverride.y = (float)y;
					Gen_SpawnOverride.z = (float)z + 0.5f;
					return;
				}
			}
		}
	}
}

static cc_bool CavesGen_Prepare(int seed) {
	Random_Seed(&rnd, seed);
	Gen_SpawnOverride.y = -1.0f; /* reset */
	return true;
}

static void CavesGen_Generate(void) {
	CavesGen_FillStone();
	CavesGen_CarveTunnels();
	CavesGen_CarveCaverns();
	if (Gen_Theme == GEN_THEME_CUSTOM) {
		NotchyGen_CarveAllOres();
	} else {
		NotchyGen_CarveOreVeins(0.9f, "Carving coal ore",    BLOCK_COAL_ORE);
		if (Gen_GetTheme()->hasExtraCaveOres) {
			NotchyGen_CarveOreVeins(0.95f, "Carving cobblestone", BLOCK_COBBLE);
			NotchyGen_CarveOreVeins(0.9f,  "Carving mossy cobblestone", BLOCK_MOSSY_ROCKS);
		}
		NotchyGen_CarveOreVeins(0.7f, "Carving iron ore",    BLOCK_IRON_ORE);
		NotchyGen_CarveOreVeins(0.5f, "Carving gold ore",    BLOCK_GOLD_ORE);
		NotchyGen_CarveOreVeins(0.6f, "Carving red ore",     BLOCK_RED_ORE);
		NotchyGen_CarveOreVeins(0.4f, "Carving diamond ore", BLOCK_DIAMOND_ORE);
	}
	CavesGen_PlaceLavaPools();
	CavesGen_PlaceBedrock();
	CavesGen_FindSpawn();

	gen_done = true;
}

static void CavesGen_Setup(void) {
	Env_SetEdgeBlock(BLOCK_BEDROCK);
	Env_SetSidesBlock(BLOCK_BEDROCK);
	Env_SetCloudsHeight(-16);
	GenTheme_ApplyEnvironment();
}

const struct MapGenerator CavesGen = {
	CavesGen_Prepare,
	CavesGen_Generate,
	CavesGen_Setup
};


/*########################################################################################################################*
*------------------------------------------------------Empty gen----------------------------------------------------------*
*#########################################################################################################################*/
/* Empty world: just a single cobblestone block in the center, invisible borders */

static cc_bool EmptyGen_Prepare(int seed) {
	Gen_SpawnOverride.y = -1.0f;
	return true;
}

static void EmptyGen_Generate(void) {
	int cx, cy, cz, index;

	Gen_CurrentState = "Generating empty world";
	Mem_Set(Gen_Blocks, BLOCK_AIR, World.Volume);

	cx = World.Width / 2;
	cy = World.Height / 2 - 1;
	cz = World.Length / 2;

	index = World_Pack(cx, cy, cz);
	Gen_Blocks[index] = BLOCK_COBBLE;

	Gen_SpawnOverride.x = (float)cx + 0.5f;
	Gen_SpawnOverride.y = (float)(cy + 1);
	Gen_SpawnOverride.z = (float)cz + 0.5f;

	gen_done = true;
}

static void EmptyGen_Setup(void) {
	Env_SetEdgeBlock(BLOCK_AIR);
	Env_SetSidesBlock(BLOCK_AIR);
	Env_SetCloudsHeight(-16);
}

const struct MapGenerator EmptyGen = {
	EmptyGen_Prepare,
	EmptyGen_Generate,
	EmptyGen_Setup
};


/*########################################################################################################################*
*-----------------------------------------------------Strange gen---------------------------------------------------------*
*#########################################################################################################################*/
/* "Strange" world: generates dungeon-like rooms and hallways.
   Multiple dungeon levels are stacked vertically, 25 blocks apart.
   Completely ignores level themes - uses its own randomly chosen wall material. */

static RNGState strange_rnd;
static BlockRaw strange_wallBlock; /* material for walls/floor/ceiling */
static cc_bool strange_hasTorches;  /* 50% chance to place torches on walls */
static cc_bool strange_isFurnished; /* 50% chance when material is wood */

/* Generation mode for strange worlds */
#define STRANGE_MODE_DUNGEON 0
#define STRANGE_MODE_LIBRARY  1
#define STRANGE_MODE_SKY      2
static int strange_mode;

/* Sky mode sub-flags */
static cc_bool strange_skyWalkways;  /* generate walkway layers */
static cc_bool strange_skyShapes;    /* generate floating shapes */

#define STRANGE_TORCH_SPACING 4  /* place a torch every N blocks along walls */

/* Deferred torch placement: collect candidates, place after all carving */
struct StrangeTorchCandidate {
	int x, y, z;       /* position to place torch */
	int wallX, wallY, wallZ; /* wall block it should lean against */
};
#define STRANGE_MAX_TORCHES 32000
static struct StrangeTorchCandidate strange_torches[STRANGE_MAX_TORCHES];
static int strange_torchCount;

/* Possible wall materials for strange worlds */
static const BlockRaw StrangeGen_Materials[] = {
	BLOCK_BRICK, BLOCK_WOOD, BLOCK_COBBLE, BLOCK_MOSSY_ROCKS,
	BLOCK_OBSIDIAN, BLOCK_STONE, BLOCK_IRON
};
#define STRANGE_MATERIAL_COUNT 7

/* Spacing between dungeon levels (floor-to-floor) */
#define STRANGE_LEVEL_SPACING 25

/* Room descriptor for dungeon generation */
struct StrangeRoom {
	int x, z;          /* top-left corner */
	int w, d;          /* width (X), depth (Z) */
	int floorY;        /* Y of the room interior floor */
	int ceilHeight;    /* interior height (2-20) */
};

#define STRANGE_MAX_ROOMS 2000
#define STRANGE_MAX_ROOMS_PER_LEVEL 200
#define STRANGE_MAX_LEVELS 32
static struct StrangeRoom strange_rooms[STRANGE_MAX_ROOMS];
static int strange_roomCount;
static int strange_levelStartIdx[STRANGE_MAX_LEVELS]; /* first room index per level */
static int strange_numLevels;

/* Carve out a box of air from (x1,y1,z1) to (x2,y2,z2) inclusive, clamped to world */
static void StrangeGen_CarveBox(int x1, int y1, int z1, int x2, int y2, int z2) {
	int x, y, z;
	x1 = max(x1, 0); y1 = max(y1, 0); z1 = max(z1, 0);
	x2 = min(x2, World.MaxX); y2 = min(y2, World.MaxY); z2 = min(z2, World.MaxZ);
	for (y = y1; y <= y2; y++) {
		for (z = z1; z <= z2; z++) {
			for (x = x1; x <= x2; x++) {
				Gen_Blocks[World_Pack(x, y, z)] = BLOCK_AIR;
			}
		}
	}
}

/* Check if two rooms overlap in XZ (with 1-block wall margin) */
static cc_bool StrangeGen_RoomsOverlap(const struct StrangeRoom* a, const struct StrangeRoom* b) {
	int ax2 = a->x + a->w;
	int az2 = a->z + a->d;
	int bx2 = b->x + b->w;
	int bz2 = b->z + b->d;
	/* Check XZ overlap with 1-block margin */
	if (ax2 + 1 < b->x || bx2 + 1 < a->x) return false;
	if (az2 + 1 < b->z || bz2 + 1 < a->z) return false;
	return true;
}

static void StrangeGen_PlaceRooms(void) {
	int level, levelBaseY, levelMaxY;
	int attempts, i, startIdx, roomsOnLevel;
	cc_bool overlaps;
	struct StrangeRoom room;
	int maxRoomW, maxRoomD, maxCeilH;

	strange_roomCount = 0;
	strange_numLevels  = 0;

	/* Scale room dimensions to world size */
	maxRoomW = min(30, World.Width  / 3);
	maxRoomD = min(30, World.Length / 3);
	if (maxRoomW < 4) maxRoomW = 4;
	if (maxRoomD < 4) maxRoomD = 4;

	for (level = 0; level < STRANGE_MAX_LEVELS; level++) {
		levelBaseY = level * STRANGE_LEVEL_SPACING;
		/* Need at least floor + 2 air blocks + ceiling within the world */
		if (levelBaseY + 3 > World.Height) break;

		/* Top of usable space: either next level's floor - 1, or top of world */
		levelMaxY = (level + 1) * STRANGE_LEVEL_SPACING - 1;
		if (levelMaxY > World.MaxY) levelMaxY = World.MaxY;

		/* Interior space available: from levelBaseY+1 to levelMaxY-1 */
		maxCeilH = levelMaxY - levelBaseY - 1;
		if (maxCeilH < 2) break;
		if (maxCeilH > 20) maxCeilH = 20;

		strange_levelStartIdx[strange_numLevels] = strange_roomCount;
		startIdx     = strange_roomCount;
		roomsOnLevel = 0;

		for (attempts = 0; attempts < 4000 && roomsOnLevel < STRANGE_MAX_ROOMS_PER_LEVEL
				&& strange_roomCount < STRANGE_MAX_ROOMS; attempts++) {
			room.w = Random_Range(&strange_rnd, 4, maxRoomW + 1);
			room.d = Random_Range(&strange_rnd, 4, maxRoomD + 1);
			room.ceilHeight = Random_Range(&strange_rnd, 2, maxCeilH + 1);
			room.x = Random_Next(&strange_rnd, World.Width  - room.w - 2) + 1;
			room.z = Random_Next(&strange_rnd, World.Length - room.d - 2) + 1;
			room.floorY = levelBaseY + 1; /* interior floor, 1 above the solid floor */

			/* Check for overlap with rooms on THIS level only */
			overlaps = false;
			for (i = startIdx; i < strange_roomCount; i++) {
				if (StrangeGen_RoomsOverlap(&room, &strange_rooms[i])) {
					overlaps = true;
					break;
				}
			}
			if (overlaps) continue;

			strange_rooms[strange_roomCount++] = room;
			roomsOnLevel++;
			/* Carve the room interior */
			StrangeGen_CarveBox(room.x, room.floorY, room.z,
				room.x + room.w - 1, room.floorY + room.ceilHeight - 1, room.z + room.d - 1);
		}

		strange_numLevels++;
		Gen_CurrentProgress = (float)(levelBaseY + STRANGE_LEVEL_SPACING) / (float)World.Height;
	}
}

/* Check if block at (x,y,z) is a solid wall/floor block (not air or torch) */
static cc_bool StrangeGen_IsSolid(int x, int y, int z) {
	BlockRaw b;
	if (x < 0 || x > World.MaxX || y < 0 || y > World.MaxY || z < 0 || z > World.MaxZ) return false;
	b = Gen_Blocks[World_Pack(x, y, z)];
	return b != BLOCK_AIR && b != BLOCK_TORCH;
}

/* Queue a torch candidate for deferred placement.
   wallX,wallY,wallZ is the position of the wall the torch should lean against. */
static void StrangeGen_QueueTorch(int x, int y, int z, int wallX, int wallY, int wallZ) {
	struct StrangeTorchCandidate* t;
	if (strange_torchCount >= STRANGE_MAX_TORCHES) return;
	if (x < 0 || x > World.MaxX || y < 0 || y > World.MaxY || z < 0 || z > World.MaxZ) return;
	t = &strange_torches[strange_torchCount++];
	t->x = x; t->y = y; t->z = z;
	t->wallX = wallX; t->wallY = wallY; t->wallZ = wallZ;
}

/* Place all queued torches, but only if position is air AND wall is still solid */
static void StrangeGen_FlushTorches(void) {
	int i;
	for (i = 0; i < strange_torchCount; i++) {
		const struct StrangeTorchCandidate* t = &strange_torches[i];
		if (Gen_Blocks[World_Pack(t->x, t->y, t->z)] != BLOCK_AIR) continue;
		if (!StrangeGen_IsSolid(t->wallX, t->wallY, t->wallZ)) continue;
		Gen_Blocks[World_Pack(t->x, t->y, t->z)] = BLOCK_TORCH;
	}
}

/* Queue a torch on a hallway wall at the top of the hallway */
static void StrangeGen_QueueHallTorch(int x, int y, int z, int hallH, int wallX, int wallZ) {
	int torchY = y + hallH - 1;
	if (torchY > World.MaxY) return;
	StrangeGen_QueueTorch(x, torchY, z, wallX, torchY, wallZ);
}

/* Carve a horizontal hallway between two rooms (assumed same level) */
static void StrangeGen_ConnectRooms(int idxA, int idxB) {
	const struct StrangeRoom* a = &strange_rooms[idxA];
	const struct StrangeRoom* b = &strange_rooms[idxB];
	int ax, az, bx, bz;
	int hallY, hallH, curX, curZ;
	int stepX, stepZ, stepCount;

	/* Center points of each room */
	ax = a->x + a->w / 2;
	az = a->z + a->d / 2;
	bx = b->x + b->w / 2;
	bz = b->z + b->d / 2;

	/* Hallway height: random 2-5 */
	hallH = Random_Range(&strange_rnd, 2, 6);
	/* Hallway floor at the same level as the rooms */
	hallY = a->floorY;
	if (hallY + hallH > World.Height) hallH = World.Height - hallY;
	if (hallH < 2) hallH = 2;

	/* Carve L-shaped path: first go along X, then along Z */
	curX = ax; curZ = az;
	stepX = (bx > ax) ? 1 : -1;
	stepCount = 0;

	/* Horizontal segment along X */
	while (curX != bx) {
		StrangeGen_CarveBox(curX, hallY, curZ,
			curX, hallY + hallH - 1, curZ + 1);
		if (strange_hasTorches && (stepCount % STRANGE_TORCH_SPACING) == 0) {
			/* Queue torch on z-1 wall side of the hallway */
			StrangeGen_QueueHallTorch(curX, hallY, curZ, hallH, curX, curZ - 1);
		}
		curX += stepX;
		stepCount++;
	}

	/* Vertical segment along Z */
	stepZ = (bz > az) ? 1 : -1;
	stepCount = 0;
	while (curZ != bz) {
		StrangeGen_CarveBox(curX, hallY, curZ,
			curX + 1, hallY + hallH - 1, curZ);
		if (strange_hasTorches && (stepCount % STRANGE_TORCH_SPACING) == 0) {
			/* Queue torch on x-1 wall side of the hallway */
			StrangeGen_QueueHallTorch(curX, hallY, curZ, hallH, curX - 1, curZ);
		}
		curZ += stepZ;
		stepCount++;
	}
}

static void StrangeGen_PlaceHallways(void) {
	int level, startIdx, endIdx, roomsOnLevel, i, a, b;

	for (level = 0; level < strange_numLevels; level++) {
		startIdx = strange_levelStartIdx[level];
		endIdx   = (level + 1 < strange_numLevels) ?
					strange_levelStartIdx[level + 1] : strange_roomCount;
		roomsOnLevel = endIdx - startIdx;
		if (roomsOnLevel < 2) continue;

		/* Chain rooms on this level */
		for (i = startIdx; i < endIdx - 1; i++) {
			StrangeGen_ConnectRooms(i, i + 1);
		}
		/* Extra random connections for loops */
		for (i = 0; i < roomsOnLevel / 3; i++) {
			a = startIdx + Random_Next(&strange_rnd, roomsOnLevel);
			b = startIdx + Random_Next(&strange_rnd, roomsOnLevel);
			if (a != b) StrangeGen_ConnectRooms(a, b);
		}

		Gen_CurrentProgress = (float)(level + 1) / (float)strange_numLevels;
	}
}

/*########################################################################################################################*
*-------------------------------------------------Furnished rooms---------------------------------------------------------*
*#########################################################################################################################*/

/* Furnish one wall side of a room.
   Room interior positions along the wall: starts at (x0,z0), steps by (dx,dz) for 'len' blocks.
   wdx,wdz: direction from room interior toward the wall block.
   facingNS: true if doors on this wall should use NS orientation. */
static void StrangeGen_FurnishWallSide(
	int x0, int z0, int dx, int dz, int len,
	int floorY, int ceilH,
	int wdx, int wdz, cc_bool facingNS)
{
	int i, x, z, wx, wz, bx, bz, roll, wy;
	cc_bool beyondOpen, placedDoor;

	for (i = 0; i < len; i++) {
		x = x0 + i * dx;
		z = z0 + i * dz;

		/* --- Furniture at floor level, inside room against wall --- */
		if (x >= 0 && x <= World.MaxX && z >= 0 && z <= World.MaxZ
			&& Gen_Blocks[World_Pack(x, floorY, z)] == BLOCK_AIR) {
			roll = Random_Next(&strange_rnd, 100);
			if (roll < 8)       Gen_Blocks[World_Pack(x, floorY, z)] = BLOCK_CHEST;
			else if (roll < 11) Gen_Blocks[World_Pack(x, floorY, z)] = BLOCK_CRAFT;
			else if (roll < 14) Gen_Blocks[World_Pack(x, floorY, z)] = BLOCK_FURNACE;
		}

		/* Skip corner positions for doors/windows */
		if (i < 1 || i >= len - 1) continue;

		/* Wall block position */
		wx = x + wdx;
		wz = z + wdz;
		if (wx < 0 || wx > World.MaxX || wz < 0 || wz > World.MaxZ) continue;

		placedDoor = false;

		/* --- Door in wall (needs 2 vertical blocks of wall material) --- */
		if (ceilH >= 2 && floorY + 1 <= World.MaxY
			&& Gen_Blocks[World_Pack(wx, floorY, wz)] == strange_wallBlock
			&& Gen_Blocks[World_Pack(wx, floorY + 1, wz)] == strange_wallBlock)
		{
			bx = wx + wdx;
			bz = wz + wdz;
			beyondOpen = false;
			if (bx >= 0 && bx <= World.MaxX && bz >= 0 && bz <= World.MaxZ)
				beyondOpen = (Gen_Blocks[World_Pack(bx, floorY, bz)] == BLOCK_AIR);

			roll = Random_Next(&strange_rnd, 100);
			if ((beyondOpen && roll < 15) || (!beyondOpen && roll < 1)) {
				if (facingNS) {
					Gen_Blocks[World_Pack(wx, floorY, wz)]     = BLOCK_DOOR_NS_BOTTOM;
					Gen_Blocks[World_Pack(wx, floorY + 1, wz)] = BLOCK_DOOR_NS_TOP;
				} else {
					Gen_Blocks[World_Pack(wx, floorY, wz)]     = BLOCK_DOOR_EW_BOTTOM;
					Gen_Blocks[World_Pack(wx, floorY + 1, wz)] = BLOCK_DOOR_EW_TOP;
				}
				placedDoor = true;
			}
		}

		/* --- Window in wall at eye level (floorY + 1) --- */
		wy = floorY + 1;
		if (!placedDoor && wy <= World.MaxY
			&& Gen_Blocks[World_Pack(wx, wy, wz)] == strange_wallBlock)
		{
			bx = wx + wdx;
			bz = wz + wdz;
			beyondOpen = false;
			if (bx >= 0 && bx <= World.MaxX && bz >= 0 && bz <= World.MaxZ)
				beyondOpen = (Gen_Blocks[World_Pack(bx, wy, bz)] == BLOCK_AIR);

			roll = Random_Next(&strange_rnd, 100);
			if ((beyondOpen && roll < 12) || (!beyondOpen && roll < 1)) {
				Gen_Blocks[World_Pack(wx, wy, wz)] = BLOCK_GLASS;
			}
		}
	}
}

/* Furnish a single room: chests, crafting tables, furnaces along walls; doors and windows in walls */
static void StrangeGen_FurnishRoom(const struct StrangeRoom* r) {
	int floorY = r->floorY;
	int ceilH  = r->ceilHeight;

	/* North wall: room interior z=r->z, wall at z-1 */
	StrangeGen_FurnishWallSide(r->x, r->z, 1, 0, r->w,
		floorY, ceilH, 0, -1, true);
	/* South wall: room interior z=r->z+d-1, wall at z+d */
	StrangeGen_FurnishWallSide(r->x, r->z + r->d - 1, 1, 0, r->w,
		floorY, ceilH, 0, 1, true);
	/* West wall: room interior x=r->x, wall at x-1 */
	StrangeGen_FurnishWallSide(r->x, r->z, 0, 1, r->d,
		floorY, ceilH, -1, 0, false);
	/* East wall: room interior x=r->x+w-1, wall at x+w */
	StrangeGen_FurnishWallSide(r->x + r->w - 1, r->z, 0, 1, r->d,
		floorY, ceilH, 1, 0, false);
}

/* Furnish rooms on a furnished level (not every room gets furniture) */
static void StrangeGen_FurnishRooms(void) {
	int i;
	for (i = 0; i < strange_roomCount; i++) {
		/* ~60% of rooms get furnished */
		if (Random_Next(&strange_rnd, 100) < 40) continue;
		StrangeGen_FurnishRoom(&strange_rooms[i]);
		Gen_CurrentProgress = (float)(i + 1) / (float)strange_roomCount;
	}
}

/* ---- Vertical connections (ladders / spiral stairs) between levels ---- */

/* Compute the XZ intersection of two room interiors.
   Returns false if they don't overlap at all. */
static cc_bool StrangeGen_RoomXZIntersect(
	const struct StrangeRoom* a, const struct StrangeRoom* b,
	int* ox1, int* oz1, int* ox2, int* oz2)
{
	*ox1 = max(a->x, b->x);
	*oz1 = max(a->z, b->z);
	*ox2 = min(a->x + a->w - 1, b->x + b->w - 1);
	*oz2 = min(a->z + a->d - 1, b->z + b->d - 1);
	return (*ox1 <= *ox2) && (*oz1 <= *oz2);
}

/* Place a 1x1 ladder shaft connecting two vertically adjacent rooms.
   A backing-wall column is placed at lx-1 so the ladder has support. */
static void StrangeGen_PlaceLadderShaft(
	const struct StrangeRoom* lower, const struct StrangeRoom* upper,
	int ox1, int oz1, int ox2, int oz2)
{
	int lx, lz, y;
	/* Place at the first column of the overlap, middle Z */
	lx = ox1;
	lz = (oz1 + oz2) / 2;
	if (lx < 1 || lx > World.MaxX || lz < 0 || lz > World.MaxZ) return;

	/* Ladders from lower room floor up to upper room floor */
	for (y = lower->floorY; y <= upper->floorY && y <= World.MaxY; y++) {
		Gen_Blocks[World_Pack(lx, y, lz)] = BLOCK_LADDER;
		/* Guarantee a backing wall at x-1 so the ladder has support */
		if (Gen_Blocks[World_Pack(lx - 1, y, lz)] == BLOCK_AIR)
			Gen_Blocks[World_Pack(lx - 1, y, lz)] = strange_wallBlock;
	}
	/* Ensure 2 blocks of headroom above the top rung in the upper room */
	for (y = upper->floorY + 1; y <= upper->floorY + 2 && y <= World.MaxY; y++) {
		if (Gen_Blocks[World_Pack(lx, y, lz)] != BLOCK_AIR)
			Gen_Blocks[World_Pack(lx, y, lz)] = BLOCK_AIR;
	}
}

/* Step offsets for a clockwise spiral around a 3x3 shaft perimeter */
static const int spiral_dx[8] = { 0, 1, 2, 2, 2, 1, 0, 0 };
static const int spiral_dz[8] = { 0, 0, 0, 1, 2, 2, 2, 1 };

/* Place a 3x3 spiral staircase with centre pillar between two rooms. */
static void StrangeGen_PlaceSpiralStairs(
	const struct StrangeRoom* lower, const struct StrangeRoom* upper,
	int ox1, int oz1, int ox2, int oz2)
{
	int sx, sz, y, step, cx, cz;
	/* Centre the 3x3 shaft inside the overlap rectangle */
	sx = ox1 + ((ox2 - ox1 + 1 - 3) / 2);
	sz = oz1 + ((oz2 - oz1 + 1 - 3) / 2);
	if (sx < 0) sx = 0;
	if (sz < 0) sz = 0;
	if (sx + 2 > World.MaxX) sx = World.MaxX - 2;
	if (sz + 2 > World.MaxZ) sz = World.MaxZ - 2;

	/* Carve the shaft from lower floor to upper floor + 2 headroom */
	StrangeGen_CarveBox(sx, lower->floorY, sz,
	                    sx + 2, min(upper->floorY + 2, World.MaxY), sz + 2);

	/* Centre pillar */
	for (y = lower->floorY; y <= upper->floorY && y <= World.MaxY; y++)
		Gen_Blocks[World_Pack(sx + 1, y, sz + 1)] = strange_wallBlock;

	/* Spiral steps around the pillar */
	step = 0;
	for (y = lower->floorY; y <= upper->floorY && y <= World.MaxY; y++) {
		cx = sx + spiral_dx[step % 8];
		cz = sz + spiral_dz[step % 8];
		Gen_Blocks[World_Pack(cx, y, cz)] = BLOCK_SLAB;
		step++;
	}
}

/* Build vertical connections between rooms that overlap in XZ on adjacent
   levels.  Only called for furnished dungeon worlds. */
static void StrangeGen_PlaceVerticalConnections(void) {
	int lev, i, j;
	int startL, endL, startU, endU;
	int ox1, oz1, ox2, oz2, ow, od;

	for (lev = 0; lev < strange_numLevels - 1; lev++) {
		startL = strange_levelStartIdx[lev];
		endL   = strange_levelStartIdx[lev + 1];
		startU = strange_levelStartIdx[lev + 1];
		endU   = (lev + 2 < strange_numLevels) ?
		          strange_levelStartIdx[lev + 2] : strange_roomCount;

		for (i = startL; i < endL; i++) {
			for (j = startU; j < endU; j++) {
				if (!StrangeGen_RoomXZIntersect(&strange_rooms[i],
						&strange_rooms[j], &ox1, &oz1, &ox2, &oz2))
					continue;
				/* 30 % chance per overlapping pair */
				if (Random_Next(&strange_rnd, 100) >= 30) continue;

				ow = ox2 - ox1 + 1;
				od = oz2 - oz1 + 1;
				/* Spiral staircase needs a 3x3 overlap; 50/50 vs ladder */
				if (ow >= 3 && od >= 3 && Random_Next(&strange_rnd, 2) == 0) {
					StrangeGen_PlaceSpiralStairs(
						&strange_rooms[i], &strange_rooms[j],
						ox1, oz1, ox2, oz2);
				} else {
					StrangeGen_PlaceLadderShaft(
						&strange_rooms[i], &strange_rooms[j],
						ox1, oz1, ox2, oz2);
				}
			}
		}
		Gen_CurrentProgress = (float)(lev + 1) / (float)(strange_numLevels - 1);
	}
}

/* Queue torches along the walls of a room at the top of the interior */
static void StrangeGen_QueueRoomTorches(const struct StrangeRoom* r) {
	int torchY, i;
	torchY = r->floorY + r->ceilHeight - 1;
	if (torchY > World.MaxY) torchY = World.MaxY;

	/* North wall (z = r->z): wall block is at z-1 */
	for (i = r->x; i < r->x + r->w; i += STRANGE_TORCH_SPACING) {
		StrangeGen_QueueTorch(i, torchY, r->z, i, torchY, r->z - 1);
	}
	/* South wall (z = r->z + r->d - 1): wall block is at z+d */
	for (i = r->x; i < r->x + r->w; i += STRANGE_TORCH_SPACING) {
		StrangeGen_QueueTorch(i, torchY, r->z + r->d - 1, i, torchY, r->z + r->d);
	}
	/* West wall (x = r->x): wall block is at x-1 */
	for (i = r->z; i < r->z + r->d; i += STRANGE_TORCH_SPACING) {
		StrangeGen_QueueTorch(r->x, torchY, i, r->x - 1, torchY, i);
	}
	/* East wall (x = r->x + r->w - 1): wall block is at x+w */
	for (i = r->z; i < r->z + r->d; i += STRANGE_TORCH_SPACING) {
		StrangeGen_QueueTorch(r->x + r->w - 1, torchY, i, r->x + r->w, torchY, i);
	}
}

static void StrangeGen_QueueAllTorches(void) {
	int i;
	for (i = 0; i < strange_roomCount; i++) {
		StrangeGen_QueueRoomTorches(&strange_rooms[i]);
		Gen_CurrentProgress = (float)(i + 1) / (float)strange_roomCount;
	}
}

static void StrangeGen_FindSpawn(void) {
	/* Spawn in the first room on the bottom level */
	if (strange_roomCount > 0) {
		const struct StrangeRoom* r = &strange_rooms[0];
		Gen_SpawnOverride.x = (float)(r->x + r->w / 2) + 0.5f;
		Gen_SpawnOverride.y = (float)(r->floorY);
		Gen_SpawnOverride.z = (float)(r->z + r->d / 2) + 0.5f;
	} else {
		/* Fallback: center of the world */
		Gen_SpawnOverride.x = (float)(World.Width  / 2) + 0.5f;
		Gen_SpawnOverride.y = (float)(World.Height / 2);
		Gen_SpawnOverride.z = (float)(World.Length / 2) + 0.5f;
	}
}

/* Place a single portal block at a random air position in the Strange world */
static void StrangeGen_PlacePortal(void) {
	int attempts, x, y, z;
	for (attempts = 0; attempts < 5000; attempts++) {
		x = Random_Next(&strange_rnd, World.Width);
		y = 1 + Random_Next(&strange_rnd, World.Height - 2); /* avoid floor/ceiling */
		z = Random_Next(&strange_rnd, World.Length);
		if (Gen_Blocks[World_Pack(x, y, z)] == BLOCK_AIR) {
			Gen_Blocks[World_Pack(x, y, z)] = BLOCK_PORTAL;
			return;
		}
	}
}

/* Place N portal blocks at random air positions */
static void StrangeGen_PlacePortals(int count) {
	int i;
	for (i = 0; i < count; i++) {
		StrangeGen_PlacePortal();
	}
}

/* Place one portal per dungeon level, searching within that level's Y range */
static void StrangeGen_PlacePortalPerLevel(void) {
	int level, levelBaseY, levelTopY;
	int attempts, x, y, z;
	for (level = 0; level < strange_numLevels; level++) {
		levelBaseY = level * STRANGE_LEVEL_SPACING;
		levelTopY  = levelBaseY + STRANGE_LEVEL_SPACING - 1;
		if (levelTopY > World.MaxY) levelTopY = World.MaxY;
		for (attempts = 0; attempts < 5000; attempts++) {
			x = Random_Next(&strange_rnd, World.Width);
			y = levelBaseY + 1 + Random_Next(&strange_rnd, levelTopY - levelBaseY - 1);
			z = Random_Next(&strange_rnd, World.Length);
			if (y >= 1 && y <= World.MaxY - 1
				&& Gen_Blocks[World_Pack(x, y, z)] == BLOCK_AIR) {
				Gen_Blocks[World_Pack(x, y, z)] = BLOCK_PORTAL;
				break;
			}
		}
	}
}

static cc_bool StrangeGen_Prepare(int seed) {
	Random_Seed(&strange_rnd, seed);
	Gen_SpawnOverride.y = -1.0f;
	strange_torchCount  = 0;

	/* Pick random wall material */
	strange_wallBlock = StrangeGen_Materials[
		Random_Next(&strange_rnd, STRANGE_MATERIAL_COUNT)];

	/* 75% chance to have torches */
	strange_hasTorches = (Random_Next(&strange_rnd, 4) != 0);

	/* 50% chance to be furnished if material is oak planks */
	strange_isFurnished = false;
	if (strange_wallBlock == BLOCK_WOOD) {
		strange_isFurnished = (Random_Next(&strange_rnd, 2) == 0);
	}

	/* 60% dungeon, 5% library, 35% sky */
	{
		int modeRoll = Random_Next(&strange_rnd, 100);
		if (modeRoll < 60) {
			strange_mode = STRANGE_MODE_DUNGEON;
		} else if (modeRoll < 65) {
			strange_mode = STRANGE_MODE_LIBRARY;
		} else {
			strange_mode = STRANGE_MODE_SKY;
		}
	}

	/* Sky mode: decide walkways/shapes */
	strange_skyWalkways = false;
	strange_skyShapes   = false;
	if (strange_mode == STRANGE_MODE_SKY) {
		int skyRoll = Random_Next(&strange_rnd, 4);
		if (skyRoll < 2) {
			strange_skyWalkways = true;  /* 50%: walkways only */
		} else if (skyRoll == 2) {
			strange_skyShapes = true;    /* 25%: shapes only */
		} else {
			strange_skyWalkways = true;  /* 25%: both */
			strange_skyShapes   = true;
		}
	}

	return true;
}

/*########################################################################################################################*
*-------------------------------------------------Library maze generation-------------------------------------------------*
*#########################################################################################################################*/

/* Generate a library-style maze: oak plank floor + ceiling, bookshelf walls, rare chests */
static void StrangeGen_GenerateLibrary(void) {
	int x, y, z, idx;

	/* Step 1: Fill world - plank floor, bookshelves, plank ceiling */
	Gen_CurrentState = "Filling library";
	Gen_CurrentProgress = 0.0f;

	/* Everything between floor and ceiling is bookshelves */
	Mem_Set(Gen_Blocks, BLOCK_BOOKSHELF, World.Volume);

	/* Oak plank floor at y=0 */
	for (z = 0; z <= World.MaxZ; z++)
		for (x = 0; x <= World.MaxX; x++)
			Gen_Blocks[World_Pack(x, 0, z)] = BLOCK_WOOD;

	/* Oak plank ceiling at y=MaxY */
	for (z = 0; z <= World.MaxZ; z++)
		for (x = 0; x <= World.MaxX; x++)
			Gen_Blocks[World_Pack(x, World.MaxY, z)] = BLOCK_WOOD;

	Gen_CurrentProgress = 0.2f;

	/* Step 2: Carve pathways through the bookshelves using the walkway
	   random-walk algorithm.  Each path segment carves a 2-block-tall
	   corridor (y=1 and y=2) so the player can walk through. */
	Gen_CurrentState = "Carving pathways";
	{
		int numPaths, i, sx, sz, cx, cz, segLen, dir, s, dx, dz;
		int pathWidth, pw;
		int maxSX, maxSZ;
		int carveY1 = 1;
		int carveY2 = World.MaxY - 1; /* full height: floor to ceiling */

		if (World.Width < 3 || World.Length < 3) goto after_carve;

		maxSX = World.MaxX - 1; if (maxSX < 2) maxSX = 2;
		maxSZ = World.MaxZ - 1; if (maxSZ < 2) maxSZ = 2;

		numPaths = (World.Width + World.Length) / 4;
		if (numPaths < 20) numPaths = 20;

		for (i = 0; i < numPaths; i++) {
			sx = Random_Range(&strange_rnd, 1, maxSX + 1);
			sz = Random_Range(&strange_rnd, 1, maxSZ + 1);
			cx = sx; cz = sz;

			{
				int segs = Random_Range(&strange_rnd, 2, 8);
				int seg;
				for (seg = 0; seg < segs; seg++) {
					segLen = Random_Range(&strange_rnd, 3, 20);
					dir = Random_Next(&strange_rnd, 4);
					switch (dir) {
					case 0: dx = 1;  dz = 0;  break;
					case 1: dx = -1; dz = 0;  break;
					case 2: dx = 0;  dz = 1;  break;
					default: dx = 0; dz = -1; break;
					}
					pathWidth = Random_Range(&strange_rnd, 1, 4);

					for (s = 0; s < segLen; s++) {
						for (pw = 0; pw < pathWidth; pw++) {
							int px, pz;
							if (dx != 0) { px = cx; pz = cz + pw; }
							else          { px = cx + pw; pz = cz; }
							if (px >= 0 && px <= World.MaxX &&
								pz >= 0 && pz <= World.MaxZ) {
								for (y = carveY1; y <= carveY2; y++)
									Gen_Blocks[World_Pack(px, y, pz)] = BLOCK_AIR;
							}
						}
						cx += dx; cz += dz;
						if (cx < 1 || cx > World.MaxX - 1 ||
							cz < 1 || cz > World.MaxZ - 1) break;
					}
				}
			}
			Gen_CurrentProgress = 0.2f + 0.6f * ((float)(i + 1) / (float)numPaths);
		}
	}
after_carve:
	Gen_CurrentProgress = 0.8f;

	/* Step 3: Rarely place chests next to bookshelf walls at floor level */
	Gen_CurrentState = "Placing chests";
	for (z = 2; z < World.MaxZ - 1; z++) {
		for (x = 2; x < World.MaxX - 1; x++) {
			idx = World_Pack(x, 1, z);
			if (Gen_Blocks[idx] != BLOCK_AIR) continue;
			/* Check if any horizontal neighbor is a bookshelf */
			if (Gen_Blocks[World_Pack(x - 1, 1, z)] != BLOCK_BOOKSHELF
				&& Gen_Blocks[World_Pack(x + 1, 1, z)] != BLOCK_BOOKSHELF
				&& Gen_Blocks[World_Pack(x, 1, z - 1)] != BLOCK_BOOKSHELF
				&& Gen_Blocks[World_Pack(x, 1, z + 1)] != BLOCK_BOOKSHELF) continue;
			/* ~1.5% chance per valid spot */
			if (Random_Next(&strange_rnd, 200) < 3) {
				Gen_Blocks[idx] = BLOCK_CHEST;
			}
		}
	}
	Gen_CurrentProgress = 0.9f;

	/* Step 4: Place ground torches every 6 blocks in carved pathways */
	if (strange_hasTorches) {
		Gen_CurrentState = "Placing torches";
		for (z = 1; z < World.MaxZ; z += 6) {
			for (x = 1; x < World.MaxX; x += 6) {
				if (Gen_Blocks[World_Pack(x, 1, z)] != BLOCK_AIR) continue;
				/* Floor below must be solid (plank floor) */
				if (Gen_Blocks[World_Pack(x, 0, z)] != BLOCK_WOOD) continue;
				Gen_Blocks[World_Pack(x, 1, z)] = BLOCK_TORCH;
			}
		}
	}
	Gen_CurrentProgress = 1.0f;

	/* Step 5: Find spawn - search for open space in a carved pathway */
	Gen_CurrentState = "Finding spawn";
	{
		int sx = World.Width / 2, sz = World.Length / 2;
		int scanX, scanZ, scanR;
		cc_bool found = false;
		/* Spiral outward from center looking for a 1x2 air column */
		for (scanR = 0; scanR < max(World.Width, World.Length) / 2 && !found; scanR++) {
			for (scanZ = sz - scanR; scanZ <= sz + scanR && !found; scanZ++) {
				for (scanX = sx - scanR; scanX <= sx + scanR && !found; scanX++) {
					if (scanX < 1 || scanX > World.MaxX - 1) continue;
					if (scanZ < 1 || scanZ > World.MaxZ - 1) continue;
					if (scanX != sx - scanR && scanX != sx + scanR
						&& scanZ != sz - scanR && scanZ != sz + scanR) continue;
					if (Gen_Blocks[World_Pack(scanX, 1, scanZ)] == BLOCK_AIR
						&& (World.MaxY < 2 || Gen_Blocks[World_Pack(scanX, 2, scanZ)] == BLOCK_AIR)) {
						Gen_SpawnOverride.x = (float)scanX + 0.5f;
						Gen_SpawnOverride.y = 1.0f;
						Gen_SpawnOverride.z = (float)scanZ + 0.5f;
						found = true;
					}
				}
			}
		}
		if (!found) {
			/* Fallback: carve a spawn pocket */
			Gen_Blocks[World_Pack(sx, 1, sz)] = BLOCK_AIR;
			if (World.MaxY >= 2) Gen_Blocks[World_Pack(sx, 2, sz)] = BLOCK_AIR;
			Gen_SpawnOverride.x = (float)sx + 0.5f;
			Gen_SpawnOverride.y = 1.0f;
			Gen_SpawnOverride.z = (float)sz + 0.5f;
		}
	}

	StrangeGen_PlacePortals(4);
	gen_done = true;
}

/*########################################################################################################################*
*--------------------------------------------------Sky world generation---------------------------------------------------*
*#########################################################################################################################*/

/* Materials for sky walkways */
static const BlockRaw SkyGen_WalkwayMaterials[] = {
	BLOCK_COBBLE, BLOCK_MOSSY_ROCKS, BLOCK_WOOD, BLOCK_OBSIDIAN, BLOCK_STONE, BLOCK_BRICK
};
#define SKY_WALKWAY_MATERIAL_COUNT 6
#define SKY_LAYER_SPACING 25

/* Generate connected walkways on a single layer at the given Y */
static void SkyGen_GenerateWalkwayLayer(int baseY, BlockRaw material) {
	int numPaths, i, sx, sz, cx, cz, segLen, dir, s, dx, dz;
	int pathWidth, pw, y;
	int maxSX, maxSZ;

	/* Need at least 3-wide world to place walkways */
	if (World.Width < 3 || World.Length < 3) return;

	maxSX = World.MaxX - 1;
	maxSZ = World.MaxZ - 1;
	if (maxSX < 2) maxSX = 2;
	if (maxSZ < 2) maxSZ = 2;

	numPaths = (World.Width + World.Length) / 4;
	if (numPaths < 20) numPaths = 20;

	for (i = 0; i < numPaths; i++) {
		sx = Random_Range(&strange_rnd, 1, maxSX + 1);
		sz = Random_Range(&strange_rnd, 1, maxSZ + 1);
		cx = sx; cz = sz;

		/* Each path makes several segments */
		{
			int segs = Random_Range(&strange_rnd, 2, 8);
			int seg;
			for (seg = 0; seg < segs; seg++) {
				segLen = Random_Range(&strange_rnd, 3, 20);
				dir = Random_Next(&strange_rnd, 4);
				switch (dir) {
				case 0: dx = 1; dz = 0; break;
				case 1: dx = -1; dz = 0; break;
				case 2: dx = 0; dz = 1; break;
				default: dx = 0; dz = -1; break;
				}
				pathWidth = Random_Range(&strange_rnd, 1, 4); /* 1-3 blocks wide */

				for (s = 0; s < segLen; s++) {
					for (pw = 0; pw < pathWidth; pw++) {
						int px, pz;
						if (dx != 0) { px = cx; pz = cz + pw; }
						else          { px = cx + pw; pz = cz; }
						if (px >= 0 && px <= World.MaxX && pz >= 0 && pz <= World.MaxZ) {
							Gen_Blocks[World_Pack(px, baseY, pz)] = material;
							/* Add fence/railing on edges occasionally */
						}
					}
					cx += dx;
					cz += dz;
					if (cx < 1 || cx > World.MaxX - 1 || cz < 1 || cz > World.MaxZ - 1) break;
				}
			}
		}
	}
}

static void SkyGen_GenerateWalkways(void) {
	int layer, layerY, numLayers;
	BlockRaw mat;

	numLayers = 0;
	for (layerY = World.Height / 3; layerY < World.Height - 5; layerY += SKY_LAYER_SPACING) {
		numLayers++;
	}
	if (numLayers < 1) numLayers = 1;

	layer = 0;
	for (layerY = World.Height / 3; layerY < World.Height - 5; layerY += SKY_LAYER_SPACING) {
		mat = SkyGen_WalkwayMaterials[Random_Next(&strange_rnd, SKY_WALKWAY_MATERIAL_COUNT)];
		SkyGen_GenerateWalkwayLayer(layerY, mat);
		layer++;
		Gen_CurrentProgress = (float)layer / (float)numLayers;
	}
}

/* Materials for floating shapes */
static const BlockRaw SkyGen_ShapeMaterials[] = {
	BLOCK_STONE, BLOCK_COBBLE, BLOCK_MOSSY_ROCKS, BLOCK_BRICK, BLOCK_WOOD,
	BLOCK_OBSIDIAN, BLOCK_DIRT, BLOCK_IRON, BLOCK_GOLD, BLOCK_SNOW_BLOCK,
	BLOCK_DIAMOND_BLOCK
};
#define SKY_SHAPE_MATERIAL_COUNT 11

static void SkyGen_GenerateShapes(void) {
	int numShapes, i, cx, cy, cz, shapeType, r, h;
	int maxR, minCY, maxCY;
	int x, y, z, x1, x2, y1, y2, z1, z2;
	int dx, dy, dz, r2, layerR, layer;
	BlockRaw mat;

	numShapes = (World.Width + World.Length) / 6;
	if (numShapes < 10) numShapes = 10;
	if (numShapes > 80) numShapes = 80;

	/* Guard bounds for Random_Range */
	maxR = World.Width / 5;
	if (maxR > 12) maxR = 12;
	if (maxR < 3)  maxR = 3;

	minCY = World.Height / 4;
	maxCY = World.Height * 3 / 4;
	if (maxCY <= minCY) maxCY = minCY + 1;

	for (i = 0; i < numShapes; i++) {
		cx = Random_Next(&strange_rnd, World.Width);
		cy = Random_Range(&strange_rnd, minCY, maxCY);
		cz = Random_Next(&strange_rnd, World.Length);
		r  = Random_Range(&strange_rnd, 2, maxR + 1);
		h  = Random_Range(&strange_rnd, 3, r * 2 + 2);
		mat = SkyGen_ShapeMaterials[Random_Next(&strange_rnd, SKY_SHAPE_MATERIAL_COUNT)];
		shapeType = Random_Next(&strange_rnd, 4);

		/* Compute bounding box clamped to world */
		x1 = cx - r; if (x1 < 0) x1 = 0;
		x2 = cx + r; if (x2 > World.MaxX) x2 = World.MaxX;
		z1 = cz - r; if (z1 < 0) z1 = 0;
		z2 = cz + r; if (z2 > World.MaxZ) z2 = World.MaxZ;
		y1 = cy - r; if (y1 < 0) y1 = 0;
		y2 = cy + r; if (y2 > World.MaxY) y2 = World.MaxY;
		r2 = r * r;

		switch (shapeType) {
		case 0: /* Cube */
			for (y = y1; y <= y2; y++)
				for (z = z1; z <= z2; z++)
					for (x = x1; x <= x2; x++)
						Gen_Blocks[World_Pack(x, y, z)] = mat;
			break;

		case 1: /* Sphere */
			for (y = y1; y <= y2; y++)
				for (z = z1; z <= z2; z++)
					for (x = x1; x <= x2; x++) {
						dx = x - cx; dy = y - cy; dz = z - cz;
						if (dx*dx + dy*dy + dz*dz <= r2)
							Gen_Blocks[World_Pack(x, y, z)] = mat;
					}
			break;

		case 2: /* Pyramid */
			y2 = cy + h - 1; if (y2 > World.MaxY) y2 = World.MaxY;
			for (layer = 0; layer < h; layer++) {
				y = cy + layer;
				if (y < 0 || y > World.MaxY) continue;
				layerR = r - (r * layer) / h;
				if (layerR < 0) layerR = 0;
				x1 = cx - layerR; if (x1 < 0) x1 = 0;
				x2 = cx + layerR; if (x2 > World.MaxX) x2 = World.MaxX;
				z1 = cz - layerR; if (z1 < 0) z1 = 0;
				z2 = cz + layerR; if (z2 > World.MaxZ) z2 = World.MaxZ;
				for (z = z1; z <= z2; z++)
					for (x = x1; x <= x2; x++)
						Gen_Blocks[World_Pack(x, y, z)] = mat;
			}
			break;

		case 3: /* Cylinder */
			y1 = cy; if (y1 < 0) y1 = 0;
			y2 = cy + h - 1; if (y2 > World.MaxY) y2 = World.MaxY;
			x1 = cx - r; if (x1 < 0) x1 = 0;
			x2 = cx + r; if (x2 > World.MaxX) x2 = World.MaxX;
			z1 = cz - r; if (z1 < 0) z1 = 0;
			z2 = cz + r; if (z2 > World.MaxZ) z2 = World.MaxZ;
			for (y = y1; y <= y2; y++)
				for (z = z1; z <= z2; z++)
					for (x = x1; x <= x2; x++) {
						dx = x - cx; dz = z - cz;
						if (dx*dx + dz*dz <= r2)
							Gen_Blocks[World_Pack(x, y, z)] = mat;
					}
			break;
		}

		Gen_CurrentProgress = (float)(i + 1) / (float)numShapes;
	}
}

static void StrangeGen_GenerateSky(void) {
	/* Step 1: Fill world with air */
	Gen_CurrentState = "Clearing sky world";
	Gen_CurrentProgress = 0.0f;
	Mem_Set(Gen_Blocks, BLOCK_AIR, World.Volume);
	Gen_CurrentProgress = 1.0f;

	/* Step 2: Walkway layers */
	if (strange_skyWalkways) {
		Gen_CurrentState = "Building walkways";
		Gen_CurrentProgress = 0.0f;
		SkyGen_GenerateWalkways();
	}

	/* Step 3: Floating shapes */
	if (strange_skyShapes) {
		Gen_CurrentState = "Placing floating shapes";
		Gen_CurrentProgress = 0.0f;
		SkyGen_GenerateShapes();
	}

	/* Step 4: Find spawn - search for a valid 1x2 air column */
	Gen_CurrentState = "Finding spawn";
	{
		int sx = World.Width / 2, sz = World.Length / 2;
		int scanX, scanZ, scanY, bestY;
		cc_bool found = false;

		/* For walkways: scan near center on each walkway layer */
		if (strange_skyWalkways) {
			int layerY;
			for (layerY = World.Height / 3; layerY < World.Height - 5 && !found; layerY += SKY_LAYER_SPACING) {
				for (scanZ = sz - 15; scanZ <= sz + 15 && !found; scanZ++) {
					for (scanX = sx - 15; scanX <= sx + 15 && !found; scanX++) {
						if (scanX < 0 || scanX > World.MaxX || scanZ < 0 || scanZ > World.MaxZ) continue;
						/* Solid ground with 2 air blocks above */
						if (Gen_Blocks[World_Pack(scanX, layerY, scanZ)] != BLOCK_AIR
							&& (layerY + 1 > World.MaxY || Gen_Blocks[World_Pack(scanX, layerY + 1, scanZ)] == BLOCK_AIR)
							&& (layerY + 2 > World.MaxY || Gen_Blocks[World_Pack(scanX, layerY + 2, scanZ)] == BLOCK_AIR)) {
							Gen_SpawnOverride.x = (float)scanX + 0.5f;
							Gen_SpawnOverride.y = (float)(layerY + 1);
							Gen_SpawnOverride.z = (float)scanZ + 0.5f;
							found = true;
						}
					}
				}
			}
		}

		/* For shapes (or walkway fallback): scan downward from top at center */
		if (!found) {
			for (scanY = World.MaxY; scanY >= 1 && !found; scanY--) {
				for (scanZ = sz - 20; scanZ <= sz + 20 && !found; scanZ++) {
					for (scanX = sx - 20; scanX <= sx + 20 && !found; scanX++) {
						if (scanX < 0 || scanX > World.MaxX || scanZ < 0 || scanZ > World.MaxZ) continue;
						if (Gen_Blocks[World_Pack(scanX, scanY, scanZ)] != BLOCK_AIR
							&& (scanY + 1 > World.MaxY || Gen_Blocks[World_Pack(scanX, scanY + 1, scanZ)] == BLOCK_AIR)
							&& (scanY + 2 > World.MaxY || Gen_Blocks[World_Pack(scanX, scanY + 2, scanZ)] == BLOCK_AIR)) {
							Gen_SpawnOverride.x = (float)scanX + 0.5f;
							Gen_SpawnOverride.y = (float)(scanY + 1);
							Gen_SpawnOverride.z = (float)scanZ + 0.5f;
							found = true;
						}
					}
				}
			}
		}

		if (!found) {
			/* Absolute fallback: center of world in air */
			Gen_SpawnOverride.x = (float)(World.Width / 2) + 0.5f;
			Gen_SpawnOverride.y = (float)(World.Height / 2);
			Gen_SpawnOverride.z = (float)(World.Length / 2) + 0.5f;
		}
	}

	StrangeGen_PlacePortal();
	gen_done = true;
}

static void StrangeGen_Generate(void) {
	if (strange_mode == STRANGE_MODE_LIBRARY) {
		StrangeGen_GenerateLibrary();
		return;
	}
	if (strange_mode == STRANGE_MODE_SKY) {
		StrangeGen_GenerateSky();
		return;
	}

	/* --- Dungeon mode --- */
	/* Step 1: Fill entire world with the wall block */
	Gen_CurrentState = "Filling dungeon walls";
	Gen_CurrentProgress = 0.0f;
	Mem_Set(Gen_Blocks, strange_wallBlock, World.Volume);
	Gen_CurrentProgress = 1.0f;

	/* Step 2: Place rooms */
	Gen_CurrentState = "Carving rooms";
	Gen_CurrentProgress = 0.0f;
	StrangeGen_PlaceRooms();

	/* Step 3: Connect rooms with hallways */
	if (strange_roomCount > 1) {
		Gen_CurrentState = "Carving hallways";
		Gen_CurrentProgress = 0.0f;
		StrangeGen_PlaceHallways();
	}

	/* Step 4: Furnish rooms (chests, crafting tables, furnaces, doors, windows) */
	if (strange_isFurnished) {
		Gen_CurrentState = "Furnishing rooms";
		Gen_CurrentProgress = 0.0f;
		StrangeGen_FurnishRooms();
	}

	/* Step 4b: Connect vertically-overlapping rooms with ladders / stairs */
	if (strange_isFurnished && strange_numLevels > 1) {
		Gen_CurrentState = "Building staircases";
		Gen_CurrentProgress = 0.0f;
		StrangeGen_PlaceVerticalConnections();
	}

	/* Step 5: Queue room torches (hallway torches already queued during carving) */
	if (strange_hasTorches) {
		Gen_CurrentState = "Queuing torches";
		Gen_CurrentProgress = 0.0f;
		StrangeGen_QueueAllTorches();
	}

	/* Step 6: Flush all torch candidates - only places if wall is still solid */
	if (strange_hasTorches && strange_torchCount > 0) {
		Gen_CurrentState = "Placing torches";
		Gen_CurrentProgress = 0.0f;
		StrangeGen_FlushTorches();
		Gen_CurrentProgress = 1.0f;
	}

	/* Step 7: Find spawn point */
	Gen_CurrentState = "Finding spawn";
	StrangeGen_FindSpawn();

	/* Step 8: Place one portal per dungeon level */
	StrangeGen_PlacePortalPerLevel();

	gen_done = true;
}

static void StrangeGen_Setup(void) {
	if (strange_mode == STRANGE_MODE_SKY) {
		/* Sky mode: use floating world appearance */
		Env_SetEdgeBlock(BLOCK_AIR);
		Env_SetSidesBlock(BLOCK_AIR);
		Env_SetCloudsHeight(-16);
		return;
	}
	/* Dungeon/Library: dark underground environment */
	Env_SetEdgeBlock(BLOCK_BEDROCK);
	Env_SetSidesBlock(BLOCK_BEDROCK);
	Env_SetCloudsHeight(-16);
	Env_SetSkyCol(PackedCol_Make(16, 16, 16, 255));
	Env_SetFogCol(PackedCol_Make(8, 8, 8, 255));
	Env_SetCloudsCol(PackedCol_Make(16, 16, 16, 255));
	Env_SetShadowCol(PackedCol_Make(32, 32, 32, 255));
}

const struct MapGenerator StrangeGen = {
	StrangeGen_Prepare,
	StrangeGen_Generate,
	StrangeGen_Setup
};


/*########################################################################################################################*
*---------------------------------------------------3D Perlin map gen-----------------------------------------------------*
*#########################################################################################################################*/
/* 3D Perlin terrain generator based on Minecraft Infdev 20100230's algorithm.
   Uses three 3D octave noise generators blended together to create terrain density.
   A height bias ensures a natural ground surface around the midpoint of the world. */

/* Noise generators are large (16 octaves * 512 bytes each), so heap-allocate them */
struct Perlin3DNoiseState {
	struct OctaveNoise3D noiseGen1;  /* 16-octave 3D noise (low selector) */
	struct OctaveNoise3D noiseGen2;  /* 16-octave 3D noise (high selector) */
	struct OctaveNoise3D noiseGen3;  /* 8-octave 3D noise (blender/selector) */
	struct OctaveNoise3D treeNoise;  /* 5-octave 2D noise for tree density */
};
static struct Perlin3DNoiseState* perlin3d_noise;

static cc_bool Perlin3DGen_Prepare(int seed) {
	Random_Seed(&rnd, seed);
	waterLevel = World.Height / 2;

	perlin3d_noise = (struct Perlin3DNoiseState*)Mem_TryAlloc(1, sizeof(struct Perlin3DNoiseState));
	if (!perlin3d_noise) return false;

	OctaveNoise3D_Init(&perlin3d_noise->noiseGen1, &rnd, 16);
	OctaveNoise3D_Init(&perlin3d_noise->noiseGen2, &rnd, 16);
	OctaveNoise3D_Init(&perlin3d_noise->noiseGen3, &rnd, 8);
	/* Advance RNG state to match Infdev (3 unused generators) */
	{ struct OctaveNoise unused; OctaveNoise_Init(&unused, &rnd, 4); }
	{ struct OctaveNoise unused; OctaveNoise_Init(&unused, &rnd, 4); }
	{ struct OctaveNoise unused; OctaveNoise_Init(&unused, &rnd, 5); }
	OctaveNoise3D_Init(&perlin3d_noise->treeNoise, &rnd, 5);
	return true;
}

/* Core 3D density function - determines terrain shape.
   Blends two noise fields using a selector noise, then subtracts a height bias
   so that density is positive (solid) below ~waterLevel and negative (air) above. */
static float Perlin3DGen_DensityAt(float bx, float by, float bz) {
	float heightBias, selector, n1val, n2val, blend, density;
	/* by is in 4-block grid units: convert to block Y, subtract waterLevel */
	heightBias = by * 4.0f - (float)waterLevel;
	if (heightBias < 0.0f) heightBias *= 3.0f; /* steeper falloff below sea level */

	/* Selector noise: 8 octaves, stretched vertically */
	selector = OctaveNoise3D_Calc(&perlin3d_noise->noiseGen3,
		bx * 684.412f / 80.0f, by * 684.412f / 400.0f, bz * 684.412f / 80.0f) / 2.0f;

	if (selector < -1.0f) {
		/* Only noiseGen1 */
		density = OctaveNoise3D_Calc(&perlin3d_noise->noiseGen1,
			bx * 684.412f, by * 984.412f, bz * 684.412f) / 512.0f - heightBias;
	} else if (selector > 1.0f) {
		/* Only noiseGen2 */
		density = OctaveNoise3D_Calc(&perlin3d_noise->noiseGen2,
			bx * 684.412f, by * 984.412f, bz * 684.412f) / 512.0f - heightBias;
	} else {
		/* Blend noiseGen1 and noiseGen2 based on selector */
		n1val = OctaveNoise3D_Calc(&perlin3d_noise->noiseGen1,
			bx * 684.412f, by * 984.412f, bz * 684.412f) / 512.0f - heightBias;
		n2val = OctaveNoise3D_Calc(&perlin3d_noise->noiseGen2,
			bx * 684.412f, by * 984.412f, bz * 684.412f) / 512.0f - heightBias;
		/* Clamp both to [-10, 10] */
		if (n1val < -10.0f) n1val = -10.0f;
		if (n1val >  10.0f) n1val =  10.0f;
		if (n2val < -10.0f) n2val = -10.0f;
		if (n2val >  10.0f) n2val =  10.0f;
		blend = (selector + 1.0f) / 2.0f; /* map [-1,1] to [0,1] */
		density = n1val + (n2val - n1val) * blend;
	}

	if (density < -10.0f) density = -10.0f;
	if (density >  10.0f) density =  10.0f;
	return density;
}

static void Perlin3DGen_Generate(void) {
	/* The world is divided into a low-resolution 3D grid.
	   Grid cells are 4x4x4 blocks. X and Z grids span the world width/length,
	   while Y grid has (Height/4 + 1) sample points.
	   Density is sampled at grid vertices, then trilinearly interpolated. */
	int gridX = World.Width  / 4;
	int gridZ = World.Length / 4;
	int gridY = World.Height / 4;
	int gx, gz, gy, sy, sx, sz;
	int bx, by, bz, index;
	float wx, wz;
	float d000, d001, d010, d011, d100, d101, d110, d111;
	float yFrac, c00, c01, c10, c11;
	float xFrac, c0, c1;
	float zFrac, density;
	BlockRaw block;

	Gen_CurrentState = "Generating terrain";
	Mem_Set(Gen_Blocks, BLOCK_AIR, World.Volume);

	for (gx = 0; gx < gridX; gx++) {
		Gen_CurrentProgress = (float)gx / gridX;
		for (gz = 0; gz < gridZ; gz++) {
			/* World-space coordinates in 4-block grid units */
			wx = (float)gx;
			wz = (float)gz;

			for (gy = 0; gy < gridY; gy++) {
				/* Sample density at 8 corners of this grid cell */
				d000 = Perlin3DGen_DensityAt(wx,        (float)gy,        wz);
				d100 = Perlin3DGen_DensityAt(wx + 1.0f, (float)gy,        wz);
				d010 = Perlin3DGen_DensityAt(wx,        (float)(gy + 1),  wz);
				d110 = Perlin3DGen_DensityAt(wx + 1.0f, (float)(gy + 1),  wz);
				d001 = Perlin3DGen_DensityAt(wx,        (float)gy,        wz + 1.0f);
				d101 = Perlin3DGen_DensityAt(wx + 1.0f, (float)gy,        wz + 1.0f);
				d011 = Perlin3DGen_DensityAt(wx,        (float)(gy + 1),  wz + 1.0f);
				d111 = Perlin3DGen_DensityAt(wx + 1.0f, (float)(gy + 1),  wz + 1.0f);

				/* Trilinear interpolation within the 4x4x4 sub-cell */
				for (sy = 0; sy < 4; sy++) {
					yFrac = (float)sy / 4.0f;
					/* Interpolate Y edges */
					c00 = d000 + (d010 - d000) * yFrac;
					c10 = d100 + (d110 - d100) * yFrac;
					c01 = d001 + (d011 - d001) * yFrac;
					c11 = d101 + (d111 - d101) * yFrac;

					for (sx = 0; sx < 4; sx++) {
						xFrac = (float)sx / 4.0f;
						/* Interpolate X */
						c0 = c00 + (c10 - c00) * xFrac;
						c1 = c01 + (c11 - c01) * xFrac;

						bx = gx * 4 + sx;
						by = gy * 4 + sy;

						for (sz = 0; sz < 4; sz++) {
							zFrac = (float)sz / 4.0f;
							/* Interpolate Z */
							density = c0 + (c1 - c0) * zFrac;

							bz = gz * 4 + sz;
							block = BLOCK_AIR;
							if (by < waterLevel) block = BLOCK_STILL_WATER;
							if (density > 0.0f)  block = BLOCK_STONE;

							index = World_Pack(bx, by, bz);
							Gen_Blocks[index] = block;
						}
					}
				}
			}
		}
	}

	/* Surface replacement pass: convert top stone to grass, sub-surface to dirt */
	Gen_CurrentState = "Creating surface";
	{ int x, z, y, depth;
	  for (x = 0; x < World.Width; x++) {
		Gen_CurrentProgress = (float)x / World.Width;
		for (z = 0; z < World.Length; z++) {
			depth = -1;
			for (y = World.MaxY; y >= 0; y--) {
				index = World_Pack(x, y, z);
				if (Gen_Blocks[index] == BLOCK_AIR || Gen_Blocks[index] == BLOCK_STILL_WATER) {
					depth = -1;
				} else if (Gen_Blocks[index] == BLOCK_STONE) {
					if (depth == -1) {
						depth = 3;
						if (y >= waterLevel - 1) {
							Gen_Blocks[index] = BLOCK_GRASS;
						} else {
							Gen_Blocks[index] = BLOCK_DIRT;
						}
					} else if (depth > 0) {
						depth--;
						Gen_Blocks[index] = BLOCK_DIRT;
					}
				}
			}
		}
	}}

	/* Carve caves and ores using the shared NotchyGen functions */
	NotchyGen_CarveCaves();
	NotchyGen_CarveAllOres();

	Mem_Free(perlin3d_noise);
	perlin3d_noise = NULL;
	gen_done = true;
}

static void Perlin3DGen_Setup(void) {
	Env_SetEdgeBlock(BLOCK_STILL_WATER);
	Env_SetSidesBlock(BLOCK_BEDROCK);
	Env_SetEdgeHeight(waterLevel);
}

const struct MapGenerator Perlin3DGen = {
	Perlin3DGen_Prepare,
	Perlin3DGen_Generate,
	Perlin3DGen_Setup
};


/*########################################################################################################################*
*----------------------------------------------------Tree generation------------------------------------------------------*
*#########################################################################################################################*/
BlockRaw* Tree_Blocks;
RNGState* Tree_Rnd;

cc_bool TreeGen_CanGrow(int treeX, int treeY, int treeZ, int treeHeight) {
	int baseHeight = treeHeight - 4;
	int index;
	int x, y, z;

	/* check tree base */
	for (y = treeY; y < treeY + baseHeight; y++) {
		for (z = treeZ - 1; z <= treeZ + 1; z++) {
			for (x = treeX - 1; x <= treeX + 1; x++) {

				if (!World_Contains(x, y, z)) return false;
				index = World_Pack(x, y, z);
				if (Tree_Blocks[index] != BLOCK_AIR) return false;
			}
		}
	}

	/* and also check canopy */
	for (y = treeY + baseHeight; y < treeY + treeHeight; y++) {
		for (z = treeZ - 2; z <= treeZ + 2; z++) {
			for (x = treeX - 2; x <= treeX + 2; x++) {

				if (!World_Contains(x, y, z)) return false;
				index = World_Pack(x, y, z);
				if (Tree_Blocks[index] != BLOCK_AIR) return false;
			}
		}
	}
	return true;
}

#define TreeGen_Place(xVal, yVal, zVal, block)\
coords[count].x = (xVal); coords[count].y = (yVal); coords[count].z = (zVal);\
blocks[count] = block; count++;

int TreeGen_Grow(int treeX, int treeY, int treeZ, int height, IVec3* coords, BlockRaw* blocks) {
	int topStart = treeY + (height - 2);
	int count = 0;
	int xx, zz, x, y, z;

	/* leaves bottom layer */
	for (y = treeY + (height - 4); y < topStart; y++) {
		for (zz = -2; zz <= 2; zz++) {
			for (xx = -2; xx <= 2; xx++) {
				x = treeX + xx; z = treeZ + zz;

				if (Math_AbsI(xx) == 2 && Math_AbsI(zz) == 2) {
					if (Random_Float(Tree_Rnd) >= 0.5f) {
						TreeGen_Place(x, y, z, BLOCK_LEAVES);
					}
				} else {
					TreeGen_Place(x, y, z, BLOCK_LEAVES);
				}
			}
		}
	}

	/* leaves top layer */
	for (; y < treeY + height; y++) {
		for (zz = -1; zz <= 1; zz++) {
			for (xx = -1; xx <= 1; xx++) {
				x = xx + treeX; z = zz + treeZ;

				if (xx == 0 || zz == 0) {
					TreeGen_Place(x, y, z, BLOCK_LEAVES);
				} else if (y == topStart && Random_Float(Tree_Rnd) >= 0.5f) {
					TreeGen_Place(x, y, z, BLOCK_LEAVES);
				}
			}
		}
	}

	/* place trunk */
	for (y = 0; y < height - 1; y++) {
		TreeGen_Place(treeX, treeY + y, treeZ, BLOCK_LOG);
	}

	/* then place dirt */
	TreeGen_Place(treeX, treeY - 1, treeZ, BLOCK_DIRT);

	return count;
}


/*########################################################################################################################*
*-----------------------------------------------Jungle tree generation----------------------------------------------------*
*#########################################################################################################################*/
cc_bool JungleTreeGen_CanGrow(int treeX, int treeY, int treeZ, int treeHeight) {
	int x, y, z, index;

	/* check 2x2 trunk space (treeX,treeZ is the min corner) */
	for (y = treeY; y < treeY + treeHeight; y++) {
		for (z = treeZ; z <= treeZ + 1; z++) {
			for (x = treeX; x <= treeX + 1; x++) {
				if (!World_Contains(x, y, z)) return false;
				index = World_Pack(x, y, z);
				if (Tree_Blocks[index] != BLOCK_AIR) return false;
			}
		}
	}

	/* check top canopy area - must fit largest possible canopy (diameter 12) */
	for (y = treeY + treeHeight - 5; y <= treeY + treeHeight + 1; y++) {
		for (z = treeZ - 6; z <= treeZ + 6; z++) {
			for (x = treeX - 6; x <= treeX + 6; x++) {
				if (!World_Contains(x, y, z)) return false;
				index = World_Pack(x, y, z);
				if (Tree_Blocks[index] != BLOCK_AIR) return false;
			}
		}
	}
	return true;
}

/*
 * Manually defined canopy layers for jungle trees.
 * Each layer is a square grid of 0/1; '1' = place leaves.
 * Grid is centered on the corner between 4 blocks (the 2x2 trunk center).
 * Origin is at (cx - halfW, cz - halfW) where halfW = width/2.
 */

/* --- Large canopy (top of tree): 3 layers, each 12x12 --- */
#define JUNGLE_BIG_W 12
static const cc_uint8 jungle_big_layer0[JUNGLE_BIG_W * JUNGLE_BIG_W] = {
	/* Bottom layer (widest) - row 0 is north edge */
	0,0,0,0,0,1,1,0,0,0,0,0,
	0,0,0,1,1,1,1,1,1,0,0,0,
	0,0,1,1,1,1,1,1,1,1,0,0,
	0,1,1,1,1,1,1,1,1,1,1,0,
	0,1,1,1,1,1,1,1,1,1,1,0,
	1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,
	0,1,1,1,1,1,1,1,1,1,1,0,
	0,1,1,1,1,1,1,1,1,1,1,0,
	0,0,1,1,1,1,1,1,1,1,0,0,
	0,0,0,1,1,1,1,1,1,0,0,0,
	0,0,0,0,0,1,1,0,0,0,0,0,
};
static const cc_uint8 jungle_big_layer1[JUNGLE_BIG_W * JUNGLE_BIG_W] = {
	/* Middle layer */
	0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,1,1,0,0,0,0,0,
	0,0,0,1,1,1,1,1,1,0,0,0,
	0,0,1,1,1,1,1,1,1,1,0,0,
	0,0,1,1,1,1,1,1,1,1,0,0,
	0,1,1,1,1,1,1,1,1,1,1,0,
	0,1,1,1,1,1,1,1,1,1,1,0,
	0,0,1,1,1,1,1,1,1,1,0,0,
	0,0,1,1,1,1,1,1,1,1,0,0,
	0,0,0,1,1,1,1,1,1,0,0,0,
	0,0,0,0,0,1,1,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,
};
static const cc_uint8 jungle_big_layer2[JUNGLE_BIG_W * JUNGLE_BIG_W] = {
	/* Top layer (smallest) */
	0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,1,1,0,0,0,0,0,
	0,0,0,1,1,1,1,1,1,0,0,0,
	0,0,0,1,1,1,1,1,1,0,0,0,
	0,0,1,1,1,1,1,1,1,1,0,0,
	0,0,1,1,1,1,1,1,1,1,0,0,
	0,0,0,1,1,1,1,1,1,0,0,0,
	0,0,0,1,1,1,1,1,1,0,0,0,
	0,0,0,0,0,1,1,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,
};
static const cc_uint8* jungle_big_layers[3] = {
	jungle_big_layer0, jungle_big_layer1, jungle_big_layer2
};

/* --- Small canopy (branch sub-canopy): 3 layers, each 6x6 --- */
#define JUNGLE_SMALL_W 6
static const cc_uint8 jungle_small_layer0[JUNGLE_SMALL_W * JUNGLE_SMALL_W] = {
	/* Bottom layer (widest) */
	0,0,1,1,0,0,
	0,1,1,1,1,0,
	1,1,1,1,1,1,
	1,1,1,1,1,1,
	0,1,1,1,1,0,
	0,0,1,1,0,0,
};
static const cc_uint8 jungle_small_layer1[JUNGLE_SMALL_W * JUNGLE_SMALL_W] = {
	/* Middle layer */
	0,0,0,0,0,0,
	0,0,1,1,0,0,
	0,1,1,1,1,0,
	0,1,1,1,1,0,
	0,0,1,1,0,0,
	0,0,0,0,0,0,
};
static const cc_uint8 jungle_small_layer2[JUNGLE_SMALL_W * JUNGLE_SMALL_W] = {
	/* Top layer, empty for now */
	0,0,0,0,0,0,
	0,0,0,0,0,0,
	0,0,0,0,0,0,
	0,0,0,0,0,0,
	0,0,0,0,0,0,
	0,0,0,0,0,0,
};
static const cc_uint8* jungle_small_layers[3] = {
	jungle_small_layer0, jungle_small_layer1, jungle_small_layer2
};

/* Places a canopy from manually defined layer bitmasks.
   cx,cz is the center corner of the 2x2 trunk (lower-left of the upper-right block).
   cy is the Y of the bottom layer. Layers go upward.
   layers is an array of 3 grid pointers, w is the grid width (must be even). */
static int JungleTreeGen_PlaceCanopy(int cx, int cy, int cz,
									const cc_uint8* const* layers, int w,
									IVec3* coords, BlockRaw* blocks, int count) {
	int halfW = w / 2;
	int layer, dx, dz, x, y, z, ox, oz;

	for (layer = 0; layer < 3; layer++) {
		const cc_uint8* grid = layers[layer];
		y = cy + layer;

		for (dz = 0; dz < w; dz++) {
			for (dx = 0; dx < w; dx++) {
				if (!grid[dz * w + dx]) continue;

				x = cx - halfW + dx;
				z = cz - halfW + dz;
				if (!World_Contains(x, y, z)) continue;

				/* Don't overwrite existing solid blocks (like trunk) */
				if (Tree_Blocks[World_Pack(x, y, z)] != BLOCK_AIR) continue;

				TreeGen_Place(x, y, z, BLOCK_LEAVES);
			}
		}
	}
	return count;
}

int JungleTreeGen_Grow(int treeX, int treeY, int treeZ, int height, IVec3* coords, BlockRaw* blocks) {
	int count = 0;
	int tx, tz, y;
	int canopyBaseY, canopyBaseX, canopyBaseZ;
	int numBranches, branchIdx, branchY, branchDir;
	int branchX, branchZ, bx, bz;

	/* --- Place top canopy first (so trunk overwrites leaf centers) --- */
	/* Center canopy on the corner shared by all 4 trunk blocks: (treeX+1, treeZ+1) */
	canopyBaseX = treeX + 1;
	canopyBaseZ = treeZ + 1;
	canopyBaseY = treeY + height - 2;
	count = JungleTreeGen_PlaceCanopy(canopyBaseX, canopyBaseY, canopyBaseZ,
									  jungle_big_layers, JUNGLE_BIG_W,
									  coords, blocks, count);

	/* --- Place sub-canopies (branches) along the trunk --- */
	numBranches = 2 + Random_Next(Tree_Rnd, 2); /* 2-3 branches */
	for (branchIdx = 0; branchIdx < numBranches; branchIdx++) {
		/* Space branches vertically along the trunk */
		branchY = treeY + (height * (2 + branchIdx)) / (numBranches + 3);
		branchY += Random_Next(Tree_Rnd, 3) - 1; /* slight variation */
		if (branchY < treeY + 4) branchY = treeY + 4;
		if (branchY > treeY + height - 6) branchY = treeY + height - 6;

		/* Pick a random direction for the branch (0=+x, 1=-x, 2=+z, 3=-z) */
		branchDir = Random_Next(Tree_Rnd, 4);
		branchX = treeX; branchZ = treeZ;
		bx = 0; bz = 0;

		switch (branchDir) {
		case 0: bx =  1; break;
		case 1: bx = -1; break;
		case 2: bz =  1; break;
		case 3: bz = -1; break;
		}

		/* Extend branch stub 1-2 blocks from trunk */
		branchX += bx * 2;
		branchZ += bz * 2;

		/* Place branch log blocks */
		if (World_Contains(treeX + bx, branchY, treeZ + bz)) {
			TreeGen_Place(treeX + bx, branchY, treeZ + bz, BLOCK_LOG);
		}
		if (World_Contains(branchX, branchY, branchZ)) {
			TreeGen_Place(branchX, branchY, branchZ, BLOCK_LOG);
		}

		/* Place sub-canopy at branch tip */
		count = JungleTreeGen_PlaceCanopy(branchX, branchY + 1, branchZ,
										  jungle_small_layers, JUNGLE_SMALL_W,
										  coords, blocks, count);
	}

	/* --- Place 2x2 trunk --- */
	for (y = 0; y < height - 1; y++) {
		for (tz = treeZ; tz <= treeZ + 1; tz++) {
			for (tx = treeX; tx <= treeX + 1; tx++) {
				if (World_Contains(tx, treeY + y, tz)) {
					TreeGen_Place(tx, treeY + y, tz, BLOCK_LOG);
				}
			}
		}
	}

	/* Place dirt beneath trunk */
	for (tz = treeZ; tz <= treeZ + 1; tz++) {
		for (tx = treeX; tx <= treeX + 1; tx++) {
			if (treeY > 0 && World_Contains(tx, treeY - 1, tz)) {
				TreeGen_Place(tx, treeY - 1, tz, BLOCK_DIRT);
			}
		}
	}

	return count;
}


/*########################################################################################################################*
*-----------------------------------------------CustomTheme Persistence---------------------------------------------------*
*#########################################################################################################################*/
/* Constructs a null-terminated option key from prefix + suffix */
static void CT_Key(char* buf, const char* prefix, const char* suffix) {
	cc_string str;
	str.buffer = buf; str.length = 0; str.capacity = STRING_SIZE;
	String_AppendConst(&str, prefix);
	String_AppendConst(&str, suffix);
	buf[str.length] = '\0';
}

/* Constructs a null-terminated option key for an ore field: prefix + "ore-" + index + "-" + field */
static void CT_OreKey(char* buf, const char* prefix, int i, const char* field) {
	cc_string str;
	str.buffer = buf; str.length = 0; str.capacity = STRING_SIZE;
	String_AppendConst(&str, prefix);
	String_AppendConst(&str, "ore-");
	String_AppendInt(&str, i);
	String_Append(&str, '-');
	String_AppendConst(&str, field);
	buf[str.length] = '\0';
}

static void CT_SaveColor(const char* key, PackedCol col) {
	cc_string str; char strBuf[STRING_SIZE];
	String_InitArray(str, strBuf);
	PackedCol_ToHex(&str, col);
	Options_Set(key, &str);
}

static PackedCol CT_LoadColor(const char* key, PackedCol def) {
	cc_uint8 rgb[3];
	if (Options_GetColor(key, rgb)) {
		return PackedCol_Make(rgb[0], rgb[1], rgb[2], 0xFF);
	}
	return def;
}

static void CT_SaveFloat(const char* key, float val) {
	cc_string str; char strBuf[STRING_SIZE];
	String_InitArray(str, strBuf);
	String_AppendFloat(&str, val, 4);
	Options_Set(key, &str);
}

/* Saves all custom theme settings using the given key prefix */
static void CT_SaveWithPrefix(const char* p) {
	struct GenThemeData* t = &Gen_CustomTheme;
	int i;
	char k[STRING_SIZE];

	/* Block settings */
	CT_Key(k, p, "surface-block");    Options_SetInt(k, t->surfaceBlock);
	CT_Key(k, p, "fill-block");       Options_SetInt(k, t->fillBlock);
	CT_Key(k, p, "fluid-block");      Options_SetInt(k, t->fluidBlock);
	CT_Key(k, p, "edge-fluid-block"); Options_SetInt(k, t->edgeFluidBlock);
	CT_Key(k, p, "edge-block");       Options_SetInt(k, t->edgeBlock);
	CT_Key(k, p, "sides-block");      Options_SetInt(k, t->sidesBlock);
	CT_Key(k, p, "edge-offset");      Options_SetInt(k, t->edgeHeightOffset);
	CT_Key(k, p, "cave-fill-block");  Options_SetInt(k, t->caveFillBlock);
	CT_Key(k, p, "garden-surface");   Options_SetInt(k, t->gardenSurface);
	CT_Key(k, p, "garden-fill");      Options_SetInt(k, t->gardenFill);
	CT_Key(k, p, "stone-block");      Options_SetInt(k, t->stoneBlock);
	CT_Key(k, p, "underwater-block"); Options_SetInt(k, t->underwaterBlock);

	/* Colors */
	CT_Key(k, p, "sky-col");    CT_SaveColor(k, t->skyCol);
	CT_Key(k, p, "fog-col");    CT_SaveColor(k, t->fogCol);
	CT_Key(k, p, "clouds-col"); CT_SaveColor(k, t->cloudsCol);
	CT_Key(k, p, "shadow-col"); CT_SaveColor(k, t->shadowCol);
	CT_Key(k, p, "night-sky-col"); CT_SaveColor(k, t->nightSkyCol);
	CT_Key(k, p, "night-fog-col"); CT_SaveColor(k, t->nightFogCol);

	/* Generation multipliers */
	CT_Key(k, p, "height-scale");       CT_SaveFloat(k, t->heightScale);
	CT_Key(k, p, "cave-freq-scale");    CT_SaveFloat(k, t->caveFreqScale);
	CT_Key(k, p, "tree-patch-mul");     Options_SetInt(k, t->treePatchMul);
	CT_Key(k, p, "flower-patch-mul");   Options_SetInt(k, t->flowerPatchMul);
	CT_Key(k, p, "mushroom-patch-mul"); Options_SetInt(k, t->mushroomPatchMul);

	/* Feature flags */
	CT_Key(k, p, "time-mode");         Options_SetInt(k, t->timeMode);
	CT_Key(k, p, "snow-layer");        Options_SetBool(k, t->hasSnowLayer);
	CT_Key(k, p, "dirt-to-grass");     Options_SetBool(k, t->dirtToGrass);
	CT_Key(k, p, "cave-gardens");      Options_SetBool(k, t->hasCaveGardens);
	CT_Key(k, p, "cacti-patch-mul");   Options_SetInt(k, t->cactiPatchMul);
	CT_Key(k, p, "generate-flowers");  Options_SetBool(k, t->generateFlowers);
	CT_Key(k, p, "extra-cave-ores");   Options_SetBool(k, t->hasExtraCaveOres);
	CT_Key(k, p, "trees-on-dirt");     Options_SetBool(k, t->treesOnDirt);
	CT_Key(k, p, "raise-water-level"); Options_SetBool(k, t->raiseWaterLevel);
	CT_Key(k, p, "has-oases");         Options_SetBool(k, t->hasOases);
	CT_Key(k, p, "has-jungle-trees");  Options_SetBool(k, t->hasJungleTrees);

	/* Ore definitions */
	for (i = 0; i < MAX_CUSTOM_ORES; i++) {
		CT_OreKey(k, p, i, "block");     Options_SetInt(k, Gen_CustomOres[i].block);
		CT_OreKey(k, p, i, "enabled");   Options_SetBool(k, Gen_CustomOres[i].enabled);
		CT_OreKey(k, p, i, "abundance"); CT_SaveFloat(k, Gen_CustomOres[i].abundance);
	}
}

/* Loads all custom theme settings from the given key prefix */
static void CT_LoadWithPrefix(const char* p) {
	struct GenThemeData* t = &Gen_CustomTheme;
	int i;
	char k[STRING_SIZE];

	/* Start from Normal theme defaults */
	*t = Gen_Themes[GEN_THEME_NORMAL];

	/* Block settings */
	CT_Key(k, p, "surface-block");    t->surfaceBlock     = Options_GetInt(k, 0, 255, BLOCK_GRASS);
	CT_Key(k, p, "fill-block");       t->fillBlock        = Options_GetInt(k, 0, 255, BLOCK_DIRT);
	CT_Key(k, p, "fluid-block");      t->fluidBlock       = Options_GetInt(k, 0, 255, BLOCK_STILL_WATER);
	CT_Key(k, p, "edge-fluid-block"); t->edgeFluidBlock   = Options_GetInt(k, 0, 255, BLOCK_STILL_WATER);
	CT_Key(k, p, "edge-block");       t->edgeBlock        = Options_GetInt(k, 0, 255, BLOCK_STILL_WATER);
	CT_Key(k, p, "sides-block");      t->sidesBlock       = Options_GetInt(k, 0, 255, BLOCK_BEDROCK);
	CT_Key(k, p, "edge-offset");      t->edgeHeightOffset = Options_GetInt(k, -64, 64, 0);
	CT_Key(k, p, "cave-fill-block");  t->caveFillBlock    = Options_GetInt(k, 0, 255, BLOCK_STONE);
	CT_Key(k, p, "garden-surface");   t->gardenSurface    = Options_GetInt(k, 0, 255, BLOCK_GRASS);
	CT_Key(k, p, "garden-fill");      t->gardenFill       = Options_GetInt(k, 0, 255, BLOCK_DIRT);
	CT_Key(k, p, "stone-block");      t->stoneBlock       = Options_GetInt(k, 0, 255, BLOCK_STONE);
	CT_Key(k, p, "underwater-block"); t->underwaterBlock   = Options_GetInt(k, 0, 255, BLOCK_GRAVEL);

	/* Colors (use actual engine defaults instead of 0 sentinel) */
	CT_Key(k, p, "sky-col");    t->skyCol    = CT_LoadColor(k, ENV_DEFAULT_SKY_COLOR);
	CT_Key(k, p, "fog-col");    t->fogCol    = CT_LoadColor(k, ENV_DEFAULT_FOG_COLOR);
	CT_Key(k, p, "clouds-col"); t->cloudsCol = CT_LoadColor(k, ENV_DEFAULT_CLOUDS_COLOR);
	CT_Key(k, p, "shadow-col"); t->shadowCol = CT_LoadColor(k, ENV_DEFAULT_SHADOW_COLOR);
	CT_Key(k, p, "night-sky-col"); t->nightSkyCol = CT_LoadColor(k, 0);
	CT_Key(k, p, "night-fog-col"); t->nightFogCol = CT_LoadColor(k, 0);

	/* Generation multipliers */
	CT_Key(k, p, "height-scale");       t->heightScale      = Options_GetFloat(k, 0.1f, 5.0f, 1.0f);
	CT_Key(k, p, "cave-freq-scale");    t->caveFreqScale    = Options_GetFloat(k, 0.0f, 10.0f, 1.0f);
	CT_Key(k, p, "tree-patch-mul");     t->treePatchMul     = Options_GetInt(k, 0, 20, 1);
	CT_Key(k, p, "flower-patch-mul");   t->flowerPatchMul   = Options_GetInt(k, 0, 20, 1);
	CT_Key(k, p, "mushroom-patch-mul"); t->mushroomPatchMul = Options_GetInt(k, 0, 20, 1);

	/* Feature flags */
	CT_Key(k, p, "time-mode");         t->timeMode         = Options_GetInt(k, 0, 2, GEN_TIME_CYCLE);
	CT_Key(k, p, "snow-layer");        t->hasSnowLayer     = Options_GetBool(k, false);
	CT_Key(k, p, "dirt-to-grass");     t->dirtToGrass      = Options_GetBool(k, true);
	CT_Key(k, p, "cave-gardens");      t->hasCaveGardens   = Options_GetBool(k, true);
	CT_Key(k, p, "cacti-patch-mul");   t->cactiPatchMul    = Options_GetInt(k, 0, 20, 0);
	CT_Key(k, p, "generate-flowers");  t->generateFlowers  = Options_GetBool(k, true);
	CT_Key(k, p, "extra-cave-ores");   t->hasExtraCaveOres = Options_GetBool(k, false);
	CT_Key(k, p, "trees-on-dirt");     t->treesOnDirt      = Options_GetBool(k, false);
	CT_Key(k, p, "raise-water-level"); t->raiseWaterLevel  = Options_GetBool(k, false);
	CT_Key(k, p, "has-oases");         t->hasOases         = Options_GetBool(k, false);
	CT_Key(k, p, "has-jungle-trees");  t->hasJungleTrees   = Options_GetBool(k, false);

	/* Status messages (always use normal defaults) */
	t->treePlantMsg     = "Planting trees";
	t->edgeFloodMsg     = "Flooding edge water";
	t->internalFloodMsg = "Flooding water";

	/* Ore definitions */
	for (i = 0; i < MAX_CUSTOM_ORES; i++) {
		CT_OreKey(k, p, i, "block");
		Gen_CustomOres[i].block = Options_GetInt(k, 0, 255,
			i < 5 ? Gen_CustomOres[i].block : 0);

		CT_OreKey(k, p, i, "enabled");
		Gen_CustomOres[i].enabled = Options_GetBool(k, i < 5);

		CT_OreKey(k, p, i, "abundance");
		Gen_CustomOres[i].abundance = Options_GetFloat(k, 0.0f, 2.0f,
			i < 5 ? Gen_CustomOres[i].abundance : 0.5f);
	}
}

#define CT_PREFIX "ct-"

void CustomTheme_Save(void) { CT_SaveWithPrefix(CT_PREFIX); }
void CustomTheme_Load(void) { CT_LoadWithPrefix(CT_PREFIX); }

/* Constructs the option key prefix for a preset slot (e.g. "ctp0-", "ctp1-") */
static void CT_PresetPrefix(char* buf, int slot) {
	cc_string str;
	str.buffer = buf; str.length = 0; str.capacity = STRING_SIZE;
	String_AppendConst(&str, "ctp");
	String_AppendInt(&str, slot);
	String_Append(&str, '-');
	buf[str.length] = '\0';
}

void CustomTheme_SavePreset(int slot) {
	char prefix[STRING_SIZE];
	char k[STRING_SIZE];
	CT_PresetPrefix(prefix, slot);
	CT_SaveWithPrefix(prefix);
	CT_Key(k, prefix, "exists");
	Options_SetBool(k, true);
}

void CustomTheme_LoadPreset(int slot) {
	char prefix[STRING_SIZE];
	CT_PresetPrefix(prefix, slot);
	CT_LoadWithPrefix(prefix);
	CT_SaveWithPrefix(CT_PREFIX); /* Also save as active custom theme */
}

cc_bool CustomTheme_HasPreset(int slot) {
	char prefix[STRING_SIZE];
	char k[STRING_SIZE];
	CT_PresetPrefix(prefix, slot);
	CT_Key(k, prefix, "exists");
	return Options_GetBool(k, false);
}

void CustomTheme_GetPresetName(int slot, cc_string* name) {
	char prefix[STRING_SIZE];
	char k[STRING_SIZE];
	CT_PresetPrefix(prefix, slot);
	CT_Key(k, prefix, "name");
	Options_Get(k, name, "");
}

void CustomTheme_SetPresetName(int slot, const cc_string* name) {
	char prefix[STRING_SIZE];
	char k[STRING_SIZE];
	CT_PresetPrefix(prefix, slot);
	CT_Key(k, prefix, "name");
	Options_Set(k, name);
}
