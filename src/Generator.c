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

const struct MapGenerator* gen_active;
BlockRaw* Gen_Blocks;
int Gen_Theme;

const struct GenThemeData Gen_Themes[GEN_THEME_COUNT] = {
	/* GEN_THEME_NORMAL (0) */
	{
		BLOCK_GRASS, BLOCK_DIRT,                           /* surfaceBlock, fillBlock */
		BLOCK_STILL_WATER, BLOCK_STILL_WATER,              /* fluidBlock, edgeFluidBlock */
		0, 0,                                              /* edgeBlock, sidesBlock (generator default) */
		BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT,              /* caveFillBlock, gardenSurface, gardenFill */
		0, 0, 0, 0,                                        /* sky, fog, clouds, shadow (defaults) */
		1.0f, 1, 1,                                        /* heightScale, treePatchMul, flowerPatchMul */
		false, false, true, true, false, true, false, false, false, false, false,
		"Planting trees", "Flooding edge water", "Flooding water"
	},
	/* GEN_THEME_HELL (1) */
	{
		BLOCK_DIRT, BLOCK_DIRT,
		BLOCK_STILL_LAVA, BLOCK_STILL_LAVA,
		BLOCK_STILL_LAVA, BLOCK_OBSIDIAN,
		BLOCK_DIRT, BLOCK_GRASS, BLOCK_DIRT,               /* gardenSurface/Fill unused: hasCaveGardens=false */
		PackedCol_Make(0x80, 0x10, 0x10, 0xFF),            /* skyCol - dark red */
		PackedCol_Make(0x18, 0x14, 0x14, 0xFF),            /* fogCol - very dark red */
		PackedCol_Make(0x30, 0x28, 0x28, 0xFF),            /* cloudsCol - dark brown-red */
		0,            /* shadowCol - dark gray */
		1.0f, 1, 1,
		true, false, false, false, false, true, true, true, false, false, false,
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
		false, false, true, true, false, true, false, false, true, false, false,
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
		false, false, true, true, false, true, false, false, false, false, false,
		"Planting trees", "Flooding edge water", "Flooding water"
	},
	/* GEN_THEME_DESERT (4) */
	{
		BLOCK_SAND, BLOCK_SAND,
		BLOCK_STILL_WATER, BLOCK_SAND,                    /* fluidBlock, edgeFluidBlock (sand border) */
		BLOCK_SAND, BLOCK_SAND,                            /* edgeBlock, sidesBlock (sand border) */
		BLOCK_STONE, BLOCK_SAND, BLOCK_SAND,               /* caves: sand gardens */
		PackedCol_Make(0xD4, 0xB8, 0x70, 0xFF),            /* skyCol - golden tan */
		PackedCol_Make(0xD4, 0xA5, 0x50, 0xFF),            /* fogCol - sandstorm */
		PackedCol_Make(0xE0, 0xC8, 0x90, 0xFF),            /* cloudsCol - light golden */
		0,
		0.5f, 1, 1,                                        /* flat terrain */
		false, false, true, true, true, false, false, false, false, true, false,
		"Planting cacti", "Filling edge sand", "Flooding water"
	},
	/* GEN_THEME_WINTER (5) */
	{
		BLOCK_GRASS, BLOCK_DIRT,
		BLOCK_ICE, BLOCK_ICE,                              /* all water as ice in winter theme */
		BLOCK_ICE, 0,
		BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT,
		PackedCol_Make(0xC0, 0xD8, 0xF0, 0xFF),            /* skyCol - light blue */
		PackedCol_Make(0xE0, 0xE8, 0xF0, 0xFF),            /* fogCol - very light blue */
		PackedCol_Make(0xF0, 0xF0, 0xF0, 0xFF),            /* cloudsCol - white */
		0,
		1.0f, 1, 1,
		false, true, true, true, false, false, false, false, false, false, false,
		"Planting trees", "Flooding edge water", "Flooding water"
	},
	/* GEN_THEME_MOON (6) */
	{
		BLOCK_COBBLE, BLOCK_STONE,
		BLOCK_STILL_LAVA, BLOCK_GLASS,
		BLOCK_AIR, BLOCK_GLASS,
		BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT,
		PackedCol_Make(0x00, 0x00, 0x00, 0xFF),            /* skyCol - black */
		PackedCol_Make(0x00, 0x00, 0x00, 0xFF),            /* fogCol - black */
		PackedCol_Make(0x38, 0x38, 0x38, 0xFF),            /* cloudsCol - light gray */
		0,
		0.5f, 0, 1,
		false, false, true, false, false, false, false, false, false, false, false,
		"Planting trees", "Flooding edge water", "Flooding water"
	},
	/* GEN_THEME_JUNGLE (7) */
	{
		BLOCK_GRASS, BLOCK_DIRT,                           /* surfaceBlock, fillBlock */
		BLOCK_STILL_WATER, BLOCK_STILL_WATER,              /* fluidBlock, edgeFluidBlock */
		0, 0,                                              /* edgeBlock, sidesBlock (generator default) */
		BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT,              /* caveFillBlock, gardenSurface, gardenFill */
		0, 0, 0, 0,                                        /* sky, fog, clouds, shadow (defaults) */
		1.0f, 4, 4,                                        /* heightScale, 4x trees, 4x flowers */
		false, false, true, true, false, true, false, false, false, false, true,
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
	const struct GenThemeData* t = &Gen_Themes[Gen_Theme];
	BlockRaw surfaceBlock = t->surfaceBlock;
	BlockRaw fillBlock    = t->fillBlock;

	Gen_CurrentState = "Setting air blocks";
	FlatgrassGen_MapSet(World.Height / 2, World.MaxY, BLOCK_AIR);

	Gen_CurrentState = "Setting fill blocks";
	FlatgrassGen_MapSet(0, World.Height / 2 - 2, fillBlock);

	Gen_CurrentState = "Setting surface blocks";
	FlatgrassGen_MapSet(World.Height / 2 - 1, World.Height / 2 - 1, surfaceBlock);

	if (t->hasSnowLayer) {
		FlatgrassGen_MapSet(World.Height / 2, World.Height / 2, BLOCK_SNOW);
	}

	if (t->hasShadowCeiling) Gen_PlaceShadowCeiling();

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
					if (Gen_Blocks[index] == BLOCK_STONE || Gen_Blocks[index] == BLOCK_DIRT) {
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

			height *= Gen_Themes[Gen_Theme].heightScale;

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
		Mem_Set(Gen_Blocks + y * oneY, BLOCK_STONE, oneY);
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
				Gen_Blocks[index] = BLOCK_STONE; index += World.OneY;
			}

			stoneHeight = max(stoneHeight, 0);
			index = World_Pack(x, (stoneHeight + 1), z);
			for (y = stoneHeight + 1; y <= dirtHeight; y++) {
				Gen_Blocks[index] = Gen_Themes[Gen_Theme].fillBlock; index += World.OneY;
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

	cavesCount       = World.Volume / 8192;
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

static void NotchyGen_FloodFillWaterBorders(void) {
	const struct GenThemeData* t = &Gen_Themes[Gen_Theme];
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
	const struct GenThemeData* t = &Gen_Themes[Gen_Theme];
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
					Gen_Blocks[index] = BLOCK_GRAVEL;
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
					Gen_Blocks[index] = BLOCK_GRAVEL;
				} else if (above == BLOCK_AIR) {
					Gen_Blocks[index] = BLOCK_DIRT;
				}
			} else if (Gen_Theme == GEN_THEME_PARADISE) {
				/* Paradise: more beaches (lower threshold = more sand near water) */
				if (above == BLOCK_STILL_WATER && (OctaveNoise_Calc(n2, (float)x, (float)z) > 12)) {
					Gen_Blocks[index] = BLOCK_GRAVEL;
				} else if (above == BLOCK_AIR) {
					Gen_Blocks[index] = (y <= waterLevel + 2 && (OctaveNoise_Calc(n1, (float)x, (float)z) > 2)) ? BLOCK_SAND : BLOCK_GRASS;
				}
			} else if (Gen_Theme == GEN_THEME_WINTER) {
				/* Winter: grass on surface (will appear snowy when snow is on top), water/ice, gravel underwater */
				if ((above == BLOCK_STILL_WATER || above == BLOCK_ICE) && (OctaveNoise_Calc(n2, (float)x, (float)z) > 12)) {
					Gen_Blocks[index] = BLOCK_GRAVEL;
				} else if (above == BLOCK_AIR) {
					Gen_Blocks[index] = BLOCK_GRASS;
				}
			} else if (Gen_Theme == GEN_THEME_WOODS || Gen_Theme == GEN_THEME_NORMAL) {
				/* Normal / Woods */
				if (above == BLOCK_STILL_WATER && (OctaveNoise_Calc(n2, (float)x, (float)z) > 12)) {
					Gen_Blocks[index] = BLOCK_GRAVEL;
				} else if (above == BLOCK_AIR) {
					Gen_Blocks[index] = (y <= waterLevel && (OctaveNoise_Calc(n1, (float)x, (float)z) > 8)) ? BLOCK_SAND : BLOCK_GRASS;
				}
			} else if (Gen_Theme == GEN_THEME_JUNGLE) {
				/* Jungle: same as normal, gravel underwater, sand near beaches, grass elsewhere */
				if (above == BLOCK_STILL_WATER && (OctaveNoise_Calc(n2, (float)x, (float)z) > 12)) {
					Gen_Blocks[index] = BLOCK_GRAVEL;
				} else if (above == BLOCK_AIR) {
					Gen_Blocks[index] = (y <= waterLevel && (OctaveNoise_Calc(n1, (float)x, (float)z) > 8)) ? BLOCK_SAND : BLOCK_GRASS;
				}
			} else {
				/* Other / Custom */
				Gen_Blocks[index] = Gen_Themes[Gen_Theme].surfaceBlock;
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

	if (!Gen_Themes[Gen_Theme].hasSnowLayer) return;

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
	if (!Gen_Themes[Gen_Theme].generateFlowers) return;

	numPatches       = World.Width * World.Length / 3000;
	numPatches      *= Gen_Themes[Gen_Theme].flowerPatchMul;
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
				if (Gen_Blocks[index] == BLOCK_AIR && Gen_Blocks[index - World.OneY] == BLOCK_GRASS)
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
				if (Gen_Blocks[index] == BLOCK_AIR && Gen_Blocks[index - World.OneY] == BLOCK_STONE)
					Gen_Blocks[index] = block;
			}
		}
	}
}

static void NotchyGen_PlantTrees(void) {
	int numPatches;
	int patchX, patchZ;
	int treeX, treeY, treeZ;
	int treeHeight, index, count;
	BlockRaw under;
	int i, j, k, m;
	int cactusH, cy;
	cc_bool isJungle = Gen_Themes[Gen_Theme].hasJungleTrees;

	IVec3 coords_small[TREE_MAX_COUNT];
	BlockRaw blocks_small[TREE_MAX_COUNT];
	IVec3 coords_jungle[JUNGLE_TREE_MAX_COUNT];
	BlockRaw blocks_jungle[JUNGLE_TREE_MAX_COUNT];
	IVec3* coords;
	BlockRaw* blocks;

	Tree_Blocks = Gen_Blocks;
	Tree_Rnd    = &rnd;

	numPatches = World.Width * World.Length / 4000;
	numPatches *= Gen_Themes[Gen_Theme].treePatchMul;

	Gen_CurrentState = Gen_Themes[Gen_Theme].treePlantMsg;
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

				if (Gen_Themes[Gen_Theme].plantsCacti) {
					if (under == BLOCK_SAND) {
						cactusH = 1 + Random_Next(&rnd, 3);
						for (cy = 0; cy < cactusH; cy++) {
							if (treeY + cy >= World.Height) break;
							index = World_Pack(treeX, treeY + cy, treeZ);
							if (Gen_Blocks[index] != BLOCK_AIR) break;
							Gen_Blocks[index] = BLOCK_CACTUS;
						}
					}
				} else {
					if (under != BLOCK_GRASS && !(Gen_Themes[Gen_Theme].treesOnDirt && under == BLOCK_DIRT))
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
	}

	if (Gen_Themes[Gen_Theme].hasOases) {
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
	if (Gen_Themes[Gen_Theme].raiseWaterLevel)
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
		GEN_COOP_STEP( 3, NotchyGen_CarveOreVeins(0.9f, "Carving coal ore", BLOCK_COAL_ORE) );
		GEN_COOP_STEP( 4, NotchyGen_CarveOreVeins(0.7f, "Carving iron ore", BLOCK_IRON_ORE) );
		GEN_COOP_STEP( 5, NotchyGen_CarveOreVeins(0.5f, "Carving gold ore", BLOCK_GOLD_ORE) );
		GEN_COOP_STEP( 6, NotchyGen_CarveOreVeins(0.6f, "Carving red ore", BLOCK_RED_ORE) );
		GEN_COOP_STEP( 7, NotchyGen_CarveOreVeins(0.4f, "Carving diamond ore", BLOCK_DIAMOND_ORE) );

		GEN_COOP_STEP( 8, NotchyGen_FloodFillWaterBorders() );
		GEN_COOP_STEP( 9, NotchyGen_FloodFillWater() );
		GEN_COOP_STEP(10, NotchyGen_FloodFillLava() );

		GEN_COOP_STEP(11, NotchyGen_CreateSurfaceLayer() );
		GEN_COOP_STEP(12, NotchyGen_PlantFlowers() );
		GEN_COOP_STEP(13, NotchyGen_PlantMushrooms() );
		GEN_COOP_STEP(14, NotchyGen_PlantTrees() );
		GEN_COOP_STEP(15, NotchyGen_PlaceSnowLayer() );
	GEN_COOP_END

	Mem_Free(heightmap);
	heightmap = NULL;

	if (Gen_Themes[Gen_Theme].hasShadowCeiling) Gen_PlaceShadowCeiling();

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
	IVec3 coords[TREE_MAX_COUNT];
	BlockRaw blocks[TREE_MAX_COUNT];

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
					Gen_Blocks[index] = BLOCK_STONE;
			}

			/* Fill dirt from stoneHeight+1 to dirtHeight */
			for (y = max(stoneHeight + 1, cutoff); y <= dirtHeight; y++) {
				if (y < 0 || y > maxY) continue;
				index = World_Pack(x, y, z);
				if (Gen_Blocks[index] == BLOCK_AIR)
					Gen_Blocks[index] = BLOCK_DIRT;
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
			if (Gen_Blocks[index] != BLOCK_DIRT && Gen_Blocks[index] != BLOCK_STONE) continue;
			above = y >= World.MaxY ? BLOCK_AIR : Gen_Blocks[index + World.OneY];
			if (above == BLOCK_AIR) {
				Gen_Blocks[index] = Gen_Themes[Gen_Theme].surfaceBlock;
			}
		}
	}

	numPatches       = World.Width * World.Length / 3000;
	numPatches      *= Gen_Themes[Gen_Theme].flowerPatchMul;
	if (Gen_Themes[Gen_Theme].generateFlowers) {
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
				if (Gen_Blocks[index] == BLOCK_AIR && Gen_Blocks[index - World.OneY] == BLOCK_GRASS)
					Gen_Blocks[index] = block;
			}
		}
	}
	}

	/* ----- Trees / Cacti ----- */
	Tree_Blocks = Gen_Blocks;
	Tree_Rnd    = &rnd;
	numPatches       = World.Width * World.Length / 4000;
	numPatches      *= Gen_Themes[Gen_Theme].treePatchMul;
	Gen_CurrentState = Gen_Themes[Gen_Theme].treePlantMsg;

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

				if (Gen_Themes[Gen_Theme].plantsCacti) {
					if (under == BLOCK_SAND) {
						int cactusH = 1 + Random_Next(&rnd, 3);
						int cy;
						for (cy = 0; cy < cactusH; cy++) {
							if (treeY + cy >= World.Height) break;
							index = World_Pack(treeX, treeY + cy, treeZ);
							if (Gen_Blocks[index] != BLOCK_AIR) break;
							Gen_Blocks[index] = BLOCK_CACTUS;
						}
					}
				} else {
					treeHeight = 5 + Random_Next(&rnd, 3);
					if ((under == BLOCK_GRASS || (Gen_Themes[Gen_Theme].treesOnDirt && under == BLOCK_DIRT)) && TreeGen_CanGrow(treeX, treeY, treeZ, treeHeight)) {
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
	
	if (Gen_Themes[Gen_Theme].hasSnowLayer) {
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
				}
			}
		}
	}
	
	if (Gen_Themes[Gen_Theme].hasSnowLayer) {
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
	if (Gen_Themes[Gen_Theme].raiseWaterLevel)
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
	NotchyGen_CarveOreVeins(0.9f, "Carving coal ore", BLOCK_COAL_ORE);
	NotchyGen_CarveOreVeins(0.7f, "Carving iron ore", BLOCK_IRON_ORE);
	NotchyGen_CarveOreVeins(0.5f, "Carving gold ore", BLOCK_GOLD_ORE);
	NotchyGen_CarveOreVeins(0.6f, "Carving red ore", BLOCK_RED_ORE);
	NotchyGen_CarveOreVeins(0.4f, "Carving diamond ore", BLOCK_DIAMOND_ORE);

	FloatingGen_FindSpawn();

	Mem_Free(heightmap);   heightmap   = NULL;
	Mem_Free(floatCutoff); floatCutoff = NULL;

	if (Gen_Themes[Gen_Theme].hasShadowCeiling) Gen_PlaceShadowCeiling();

	gen_done = true;
}

static void FloatingGen_Setup(void) {
	Env_SetEdgeBlock(BLOCK_AIR);
	Env_SetSidesBlock(BLOCK_AIR);
	Env_SetCloudsHeight(-16);
	GenTheme_ApplyEnvironment();
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
	BlockRaw fillBlock = Gen_Themes[Gen_Theme].caveFillBlock;
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
		if (!Gen_Themes[Gen_Theme].hasCaveGardens) hasGrass = 0;

		if (hasGrass) {
			BlockRaw gardenSurface = Gen_Themes[Gen_Theme].gardenSurface;
			BlockRaw gardenFill    = Gen_Themes[Gen_Theme].gardenFill;

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

			if (Gen_Themes[Gen_Theme].plantsCacti) {
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
			BlockRaw mushroomFloor = Gen_Themes[Gen_Theme].caveFillBlock;
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
	NotchyGen_CarveOreVeins(0.9f, "Carving coal ore",    BLOCK_COAL_ORE);
	if (Gen_Themes[Gen_Theme].hasExtraCaveOres) {
		NotchyGen_CarveOreVeins(0.95f, "Carving cobblestone", BLOCK_COBBLE);
		NotchyGen_CarveOreVeins(0.9f,  "Carving mossy cobblestone", BLOCK_MOSSY_ROCKS);
	}
	NotchyGen_CarveOreVeins(0.7f, "Carving iron ore",    BLOCK_IRON_ORE);
	NotchyGen_CarveOreVeins(0.5f, "Carving gold ore",    BLOCK_GOLD_ORE);
	NotchyGen_CarveOreVeins(0.6f, "Carving red ore",     BLOCK_RED_ORE);
	NotchyGen_CarveOreVeins(0.4f, "Carving diamond ore", BLOCK_DIAMOND_ORE);
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
	0,1,1,1,1,0,
	1,1,1,1,1,1,
	1,1,1,1,1,1,
	1,1,1,1,1,1,
	1,1,1,1,1,1,
	0,1,1,1,1,0,
};
static const cc_uint8 jungle_small_layer1[JUNGLE_SMALL_W * JUNGLE_SMALL_W] = {
	/* Middle layer */
	0,0,0,0,0,0,
	0,1,1,1,1,0,
	0,1,1,1,1,0,
	0,1,1,1,1,0,
	0,1,1,1,1,0,
	0,0,0,0,0,0,
};
static const cc_uint8 jungle_small_layer2[JUNGLE_SMALL_W * JUNGLE_SMALL_W] = {
	/* Top layer */
	0,0,0,0,0,0,
	0,0,1,1,0,0,
	0,1,1,1,1,0,
	0,1,1,1,1,0,
	0,0,1,1,0,0,
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
