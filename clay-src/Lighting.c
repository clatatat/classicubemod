#include "Lighting.h"
#include "Block.h"
#include "Funcs.h"
#include "MapRenderer.h"
#include "Platform.h"
#include "World.h"
#include "Logger.h"
#include "Event.h"
#include "Game.h"
#include "String_.h"
#include "Chat.h"
#include "ExtMath.h"
#include "Options.h"
#include "Builder.h"

const char* const LightingMode_Names[LIGHTING_MODE_COUNT] = { "Classic", "Fancy" };

cc_uint8 Lighting_Mode;
cc_bool  Lighting_ModeLockedByServer;
cc_bool  Lighting_ModeSetByServer;
cc_uint8 Lighting_ModeUserCached;
struct _Lighting Lighting;
#define Lighting_Pack(x, z) ((x) + World.Width * (z))

void Lighting_SetMode(cc_uint8 mode, cc_bool fromServer) {
	cc_uint8 oldMode = Lighting_Mode;
	Lighting_Mode    = mode;

	Event_RaiseLightingMode(&WorldEvents.LightingModeChanged, oldMode, fromServer);
}


/*########################################################################################################################*
*----------------------------------------------------Classic lighting-----------------------------------------------------*
*#########################################################################################################################*/
static cc_int16* classic_heightmap;
static cc_uint8* torch_lightmap;
#define HEIGHT_UNCALCULATED Int16_MaxValue
#define TORCH_LIGHT_RADIUS 4

#define ClassicLighting_CalcBody(get_block)\
for (y = maxY; y >= 0; y--, i -= World.OneY) {\
	block = get_block;\
\
	if (Blocks.BlocksLight[block]) {\
		offset = (Blocks.LightOffset[block] >> LIGHT_FLAG_SHADES_FROM_BELOW) & 1;\
		classic_heightmap[hIndex] = y - offset;\
		return y - offset;\
	}\
}

static int ClassicLighting_CalcHeightAt(int x, int maxY, int z, int hIndex) {
	int i = World_Pack(x, maxY, z);
	BlockID block;
	int y, offset;

#ifndef EXTENDED_BLOCKS
	ClassicLighting_CalcBody(World.Blocks[i]);
#else
	if (World.IDMask <= 0xFF) {
		ClassicLighting_CalcBody(World.Blocks[i]);
	} else {
		ClassicLighting_CalcBody(World.Blocks[i] | (World.Blocks2[i] << 8));
	}
#endif

	classic_heightmap[hIndex] = -10;
	return -10;
}

int ClassicLighting_GetLightHeight(int x, int z) {
	int hIndex = Lighting_Pack(x, z);
	int lightH = classic_heightmap[hIndex];
	return lightH == HEIGHT_UNCALCULATED ? ClassicLighting_CalcHeightAt(x, World.Height - 1, z, hIndex) : lightH;
}

/* Outside color is same as sunlight color, so we reuse when possible */
cc_bool ClassicLighting_IsLit(int x, int y, int z) {
	if (torch_lightmap && World_Contains(x, y, z) && torch_lightmap[World_Pack(x, y, z)])
		return true;
	return y > ClassicLighting_GetLightHeight(x, z);
}

cc_bool ClassicLighting_IsLit_Fast(int x, int y, int z) {
	if (torch_lightmap && World_Contains(x, y, z) && torch_lightmap[World_Pack(x, y, z)])
		return true;
	return y > classic_heightmap[Lighting_Pack(x, z)];
}


/*########################################################################################################################*
*------------------------------------------------------Torch lighting-----------------------------------------------------*
*#########################################################################################################################*/
typedef struct TorchNode_ { cc_int16 x, y, z; cc_uint8 level; } TorchNode;
static const cc_int8 torch_dirs[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};

static void TorchLight_Propagate(int srcX, int srcY, int srcZ) {
	TorchNode queue[512];
	int head, tail, idx, d, nx, ny, nz;
	cc_uint8 newLevel;

	head = 0; tail = 0;
	idx = World_Pack(srcX, srcY, srcZ);
	if (TORCH_LIGHT_RADIUS > torch_lightmap[idx])
		torch_lightmap[idx] = TORCH_LIGHT_RADIUS;
	queue[tail].x = (cc_int16)srcX; queue[tail].y = (cc_int16)srcY;
	queue[tail].z = (cc_int16)srcZ; queue[tail].level = TORCH_LIGHT_RADIUS;
	tail++;

	while (head < tail) {
		newLevel = queue[head].level - 1;
		if (newLevel == 0) { head++; continue; }

		for (d = 0; d < 6; d++) {
			nx = queue[head].x + torch_dirs[d][0];
			ny = queue[head].y + torch_dirs[d][1];
			nz = queue[head].z + torch_dirs[d][2];
			if (!World_Contains(nx, ny, nz)) continue;
			if (Blocks.BlocksLight[World_GetBlock(nx, ny, nz)]) continue;

			idx = World_Pack(nx, ny, nz);
			if (torch_lightmap[idx] >= newLevel) continue;

			torch_lightmap[idx] = newLevel;
			if (tail < 512) {
				queue[tail].x = (cc_int16)nx; queue[tail].y = (cc_int16)ny;
				queue[tail].z = (cc_int16)nz; queue[tail].level = newLevel;
				tail++;
			}
		}
		head++;
	}
}

static void TorchLight_Remove(int srcX, int srcY, int srcZ) {
	int x, y, z, r;
	int minX, maxX, minY, maxY, minZ, maxZ;

	r = TORCH_LIGHT_RADIUS;
	/* Clear light in affected area */
	minX = max(srcX - r, 0); maxX = min(srcX + r, World.MaxX);
	minY = max(srcY - r, 0); maxY = min(srcY + r, World.MaxY);
	minZ = max(srcZ - r, 0); maxZ = min(srcZ + r, World.MaxZ);

	for (y = minY; y <= maxY; y++)
		for (z = minZ; z <= maxZ; z++)
			for (x = minX; x <= maxX; x++)
				torch_lightmap[World_Pack(x, y, z)] = 0;

	/* Re-propagate from nearby torches and red ore torches */
	minX = max(srcX - 2*r, 0); maxX = min(srcX + 2*r, World.MaxX);
	minY = max(srcY - 2*r, 0); maxY = min(srcY + 2*r, World.MaxY);
	minZ = max(srcZ - 2*r, 0); maxZ = min(srcZ + 2*r, World.MaxZ);

	for (y = minY; y <= maxY; y++)
		for (z = minZ; z <= maxZ; z++)
			for (x = minX; x <= maxX; x++) {
				BlockID b = World_GetBlock(x, y, z);
				if (b == BLOCK_TORCH)
					TorchLight_Propagate(x, y, z);
			}
}

static void TorchLight_RefreshChunks(int srcX, int srcY, int srcZ) {
	int cx, cy, cz, r;
	int minCX, maxCX, minCY, maxCY, minCZ, maxCZ;

	r = TORCH_LIGHT_RADIUS;
	minCX = max(srcX - r, 0) >> CHUNK_SHIFT;
	maxCX = min(srcX + r, World.MaxX) >> CHUNK_SHIFT;
	minCY = max(srcY - r, 0) >> CHUNK_SHIFT;
	maxCY = min(srcY + r, World.MaxY) >> CHUNK_SHIFT;
	minCZ = max(srcZ - r, 0) >> CHUNK_SHIFT;
	maxCZ = min(srcZ + r, World.MaxZ) >> CHUNK_SHIFT;

	for (cy = minCY; cy <= maxCY; cy++)
		for (cz = minCZ; cz <= maxCZ; cz++)
			for (cx = minCX; cx <= maxCX; cx++)
				MapRenderer_RefreshChunk(cx, cy, cz);
}

static void TorchLight_ScanWorld(void) {
	int x, y, z;
	BlockID block;

	for (y = 0; y < World.Height; y++)
		for (z = 0; z < World.Length; z++)
			for (x = 0; x < World.Width; x++) {
				block = World_GetBlock(x, y, z);
				if (block == BLOCK_TORCH)
					TorchLight_Propagate(x, y, z);
			}
}

static PackedCol ClassicLighting_Color(int x, int y, int z) {
	if (!World_Contains(x, y, z)) return Env.SunCol;
	if (torch_lightmap && torch_lightmap[World_Pack(x, y, z)]) return Env.SunCol;
	return y > ClassicLighting_GetLightHeight(x, z) ? Env.SunCol : Env.ShadowCol;
}

static PackedCol SmoothLighting_Color(int x, int y, int z) {
	if (!World_Contains(x, y, z)) return Env.SunCol;
	if (Blocks.Brightness[World_GetBlock(x, y, z)]) return Env.SunCol;
	if (torch_lightmap && torch_lightmap[World_Pack(x, y, z)]) return Env.SunCol;
	return y > ClassicLighting_GetLightHeight(x, z) ? Env.SunCol : Env.ShadowCol;
}

static PackedCol ClassicLighting_Color_XSide(int x, int y, int z) {
	if (!World_Contains(x, y, z)) return Env.SunXSide;
	if (torch_lightmap && torch_lightmap[World_Pack(x, y, z)]) return Env.SunXSide;
	return y > ClassicLighting_GetLightHeight(x, z) ? Env.SunXSide : Env.ShadowXSide;
}

static PackedCol ClassicLighting_Color_Sprite_Fast(int x, int y, int z) {
	if (torch_lightmap && World_Contains(x, y, z) && torch_lightmap[World_Pack(x, y, z)])
		return Env.SunCol;
	return y > classic_heightmap[Lighting_Pack(x, z)] ? Env.SunCol : Env.ShadowCol;
}

static PackedCol ClassicLighting_Color_YMax_Fast(int x, int y, int z) {
	if (torch_lightmap && World_Contains(x, y, z) && torch_lightmap[World_Pack(x, y, z)])
		return Env.SunCol;
	return y > classic_heightmap[Lighting_Pack(x, z)] ? Env.SunCol : Env.ShadowCol;
}

static PackedCol ClassicLighting_Color_YMin_Fast(int x, int y, int z) {
	if (torch_lightmap && World_Contains(x, y, z) && torch_lightmap[World_Pack(x, y, z)])
		return Env.SunYMin;
	return y > classic_heightmap[Lighting_Pack(x, z)] ? Env.SunYMin : Env.ShadowYMin;
}

static PackedCol ClassicLighting_Color_XSide_Fast(int x, int y, int z) {
	if (torch_lightmap && World_Contains(x, y, z) && torch_lightmap[World_Pack(x, y, z)])
		return Env.SunXSide;
	return y > classic_heightmap[Lighting_Pack(x, z)] ? Env.SunXSide : Env.ShadowXSide;
}

static PackedCol ClassicLighting_Color_ZSide_Fast(int x, int y, int z) {
	if (torch_lightmap && World_Contains(x, y, z) && torch_lightmap[World_Pack(x, y, z)])
		return Env.SunZSide;
	return y > classic_heightmap[Lighting_Pack(x, z)] ? Env.SunZSide : Env.ShadowZSide;
}

void ClassicLighting_Refresh(void) {
	int i;
	for (i = 0; i < World.Width * World.Length; i++) {
		classic_heightmap[i] = HEIGHT_UNCALCULATED;
	}
}


/*########################################################################################################################*
*----------------------------------------------------Lighting update------------------------------------------------------*
*#########################################################################################################################*/
static void ClassicLighting_UpdateLighting(int x, int y, int z, BlockID oldBlock, BlockID newBlock, int index, int lightH) {
	cc_bool didBlock  = Blocks.BlocksLight[oldBlock];
	cc_bool nowBlocks = Blocks.BlocksLight[newBlock];
	int oldOffset     = (Blocks.LightOffset[oldBlock] >> LIGHT_FLAG_SHADES_FROM_BELOW) & 1;
	int newOffset     = (Blocks.LightOffset[newBlock] >> LIGHT_FLAG_SHADES_FROM_BELOW) & 1;
	BlockID above;

	/* Two cases we need to handle here: */
	if (didBlock == nowBlocks) {
		if (!didBlock) return;              /* a) both old and new block do not block light */
		if (oldOffset == newOffset) return; /* b) both blocks blocked light at the same Y coordinate */
	}

	if ((y - newOffset) >= lightH) {
		if (nowBlocks) {
			classic_heightmap[index] = y - newOffset;
		} else {
			/* Part of the column is now visible to light, we don't know how exactly how high it should be though. */
			/* However, we know that if the block Y was above or equal to old light height, then the new light height must be <= block Y */
			ClassicLighting_CalcHeightAt(x, y, z, index);
		}
	} else if (y == lightH && oldOffset == 0) {
		/* For a solid block on top of an upside down slab, they will both have the same light height. */
		/* So we need to account for this particular case. */
		above = y == (World.Height - 1) ? BLOCK_AIR : World_GetBlock(x, y + 1, z);
		if (Blocks.BlocksLight[above]) return;

		if (nowBlocks) {
			classic_heightmap[index] = y - newOffset;
		} else {
			ClassicLighting_CalcHeightAt(x, y - 1, z, index);
		}
	}
}

static cc_bool ClassicLighting_Needs(BlockID block, BlockID other) {
	return Blocks.Draw[block] != DRAW_OPAQUE || Blocks.Draw[other] != DRAW_GAS;
}

#define ClassicLighting_NeedsNeighourBody(get_block)\
/* Update if any blocks in the chunk are affected by light change. */ \
for (; y >= minY; y--, i -= World.OneY) {\
	other    = get_block;\
	affected = y == nY ? ClassicLighting_Needs(block, other) : Blocks.Draw[other] != DRAW_GAS;\
	if (affected) return true;\
}

static cc_bool ClassicLighting_NeedsNeighour(BlockID block, int i, int minY, int y, int nY) {
	BlockID other;
	cc_bool affected;

#ifndef EXTENDED_BLOCKS
	ClassicLighting_NeedsNeighourBody(World.Blocks[i]);
#else
	if (World.IDMask <= 0xFF) {
		ClassicLighting_NeedsNeighourBody(World.Blocks[i]);
	} else {
		ClassicLighting_NeedsNeighourBody(World.Blocks[i] | (World.Blocks2[i] << 8));
	}
#endif
	return false;
}

static void ClassicLighting_ResetNeighbour(int x, int y, int z, BlockID block, int cx, int cy, int cz, int minCy, int maxCy) {
	int minY, maxY;

	if (minCy == maxCy) {
		minY = cy << CHUNK_SHIFT;

		if (ClassicLighting_NeedsNeighour(block, World_Pack(x, y, z), minY, y, y)) {
			MapRenderer_RefreshChunk(cx, cy, cz);
		}
	} else {
		for (cy = maxCy; cy >= minCy; cy--) {
			minY = (cy << CHUNK_SHIFT); 
			maxY = (cy << CHUNK_SHIFT) + CHUNK_MAX;
			if (maxY > World.MaxY) maxY = World.MaxY;

			if (ClassicLighting_NeedsNeighour(block, World_Pack(x, maxY, z), minY, maxY, y)) {
				MapRenderer_RefreshChunk(cx, cy, cz);
			}
		}
	}
}

static void ClassicLighting_ResetColumn(int cx, int cy, int cz, int minCy, int maxCy) {
	if (minCy == maxCy) {
		MapRenderer_RefreshChunk(cx, cy, cz);
	} else {
		for (cy = maxCy; cy >= minCy; cy--) {
			MapRenderer_RefreshChunk(cx, cy, cz);
		}
	}
}

static void ClassicLighting_RefreshAffected(int x, int y, int z, BlockID block, int oldHeight, int newHeight) {
	int cx = x >> CHUNK_SHIFT, bX = x & CHUNK_MASK;
	int cy = y >> CHUNK_SHIFT, bY = y & CHUNK_MASK;
	int cz = z >> CHUNK_SHIFT, bZ = z & CHUNK_MASK;

	/* NOTE: much faster to only update the chunks that are affected by the change in shadows, rather than the entire column. */
	int newCy = newHeight < 0 ? 0 : newHeight >> 4;
	int oldCy = oldHeight < 0 ? 0 : oldHeight >> 4;
	int minCy = min(oldCy, newCy), maxCy = max(oldCy, newCy);
	ClassicLighting_ResetColumn(cx, cy, cz, minCy, maxCy);

	if (bX == 0 && cx > 0) {
		ClassicLighting_ResetNeighbour(x - 1, y, z, block, cx - 1, cy, cz, minCy, maxCy);
	}
	if (bY == 0 && cy > 0 && ClassicLighting_Needs(block, World_GetBlock(x, y - 1, z))) {
		MapRenderer_RefreshChunk(cx, cy - 1, cz);
	}
	if (bZ == 0 && cz > 0) {
		ClassicLighting_ResetNeighbour(x, y, z - 1, block, cx, cy, cz - 1, minCy, maxCy);
	}

	if (bX == 15 && cx < World.ChunksX - 1) {
		ClassicLighting_ResetNeighbour(x + 1, y, z, block, cx + 1, cy, cz, minCy, maxCy);
	}
	if (bY == 15 && cy < World.ChunksY - 1 && ClassicLighting_Needs(block, World_GetBlock(x, y + 1, z))) {
		MapRenderer_RefreshChunk(cx, cy + 1, cz);
	}
	if (bZ == 15 && cz < World.ChunksZ - 1) {
		ClassicLighting_ResetNeighbour(x, y, z + 1, block, cx, cy, cz + 1, minCy, maxCy);
	}
}

void ClassicLighting_OnBlockChanged(int x, int y, int z, BlockID oldBlock, BlockID newBlock) {
	int hIndex = Lighting_Pack(x, z);
	int lightH = classic_heightmap[hIndex];
	int newHeight;

	/* Since light wasn't checked to begin with, means column never had meshes for any of its chunks built. */
	/* So we don't need to do anything. */
	if (lightH == HEIGHT_UNCALCULATED) return;

	ClassicLighting_UpdateLighting(x, y, z, oldBlock, newBlock, hIndex, lightH);
	newHeight = classic_heightmap[hIndex] + 1;
	ClassicLighting_RefreshAffected(x, y, z, newBlock, lightH + 1, newHeight);

	/* Handle torch light */
	if (torch_lightmap) {
		if (newBlock == BLOCK_TORCH) {
			TorchLight_Propagate(x, y, z);
			TorchLight_RefreshChunks(x, y, z);
		} else if (oldBlock == BLOCK_TORCH) {
			TorchLight_Remove(x, y, z);
			TorchLight_RefreshChunks(x, y, z);
		} else if (Blocks.BlocksLight[newBlock] != Blocks.BlocksLight[oldBlock]) {
			/* Light-blocking property changed near potential torch light */
			TorchLight_Remove(x, y, z);
			TorchLight_RefreshChunks(x, y, z);
		}
	}
}


/*########################################################################################################################*
*---------------------------------------------------Lighting heightmap----------------------------------------------------*
*#########################################################################################################################*/
static int Heightmap_InitialCoverage(int x1, int z1, int xCount, int zCount, int* skip) {
	int elemsLeft = 0, index = 0, curRunCount = 0;
	int x, z, hIndex, lightH;

	for (z = 0; z < zCount; z++) {
		hIndex = Lighting_Pack(x1, z1 + z);
		for (x = 0; x < xCount; x++) {
			lightH = classic_heightmap[hIndex++];

			skip[index] = 0;
			if (lightH == HEIGHT_UNCALCULATED) {
				elemsLeft++;
				curRunCount = 0;
			} else {
				skip[index - curRunCount]++;
				curRunCount++;
			}
			index++;
		}
		curRunCount = 0; /* We can only skip an entire X row at most. */
	}
	return elemsLeft;
}

#define Heightmap_CalculateBody(get_block)\
for (y = World.Height - 1; y >= 0; y--) {\
	if (elemsLeft <= 0) { return true; } \
	mapIndex = World_Pack(x1, y, z1);\
	hIndex   = Lighting_Pack(x1, z1);\
\
	for (z = 0; z < zCount; z++) {\
		baseIndex = mapIndex;\
		index = z * xCount;\
		for (x = 0; x < xCount;) {\
			curRunCount = skip[index];\
			x += curRunCount; mapIndex += curRunCount; index += curRunCount;\
\
			if (x < xCount && Blocks.BlocksLight[get_block]) {\
				lightOffset = (Blocks.LightOffset[get_block] >> LIGHT_FLAG_SHADES_FROM_BELOW) & 1;\
				classic_heightmap[hIndex + x] = (cc_int16)(y - lightOffset);\
				elemsLeft--;\
				skip[index] = 0;\
\
				offset = prevRunCount + curRunCount;\
				newRunCount = skip[index - offset] + 1;\
\
				/* consider case 1 0 1 0, where we are at last 0 */ \
				/* we need to make this 3 0 0 0 and advance by 1 */ \
				oldRunCount = (x - offset + newRunCount) < xCount ? skip[index - offset + newRunCount] : 0; \
				if (oldRunCount != 0) {\
					skip[index - offset + newRunCount] = 0; \
					newRunCount += oldRunCount; \
				} \
				skip[index - offset] = newRunCount; \
				x += oldRunCount; index += oldRunCount; mapIndex += oldRunCount; \
				prevRunCount = newRunCount; \
			} else { \
				prevRunCount = 0; \
			}\
			x++; mapIndex++; index++; \
		}\
		prevRunCount = 0;\
		hIndex += World.Width;\
		mapIndex = baseIndex + World.Width; /* advance one Z */ \
	}\
}

static cc_bool Heightmap_CalculateCoverage(int x1, int z1, int xCount, int zCount, int elemsLeft, int* skip) {
	int prevRunCount = 0, curRunCount, newRunCount, oldRunCount;
	int lightOffset, offset;
	int mapIndex, hIndex, baseIndex, index;
	int x, y, z;

#ifndef EXTENDED_BLOCKS
	Heightmap_CalculateBody(World.Blocks[mapIndex]);
#else
	if (World.IDMask <= 0xFF) {
		Heightmap_CalculateBody(World.Blocks[mapIndex]);
	} else {
		Heightmap_CalculateBody(World.Blocks[mapIndex] | (World.Blocks2[mapIndex] << 8));
	}
#endif
	return false;
}

static void Heightmap_FinishCoverage(int x1, int z1, int xCount, int zCount) {
	int x, z, hIndex, lightH;

	for (z = 0; z < zCount; z++) {
		hIndex = Lighting_Pack(x1, z1 + z);
		for (x = 0; x < xCount; x++, hIndex++) {
			lightH = classic_heightmap[hIndex];

			if (lightH == HEIGHT_UNCALCULATED) {
				classic_heightmap[hIndex] = -10;
			}
		}
	}
}


void ClassicLighting_LightHint(int startX, int startY, int startZ) {
	int x1 = max(startX, 0), x2 = min(World.Width,  startX + EXTCHUNK_SIZE);
	int z1 = max(startZ, 0), z2 = min(World.Length, startZ + EXTCHUNK_SIZE);
	int xCount = x2 - x1, zCount = z2 - z1;
	int skip[EXTCHUNK_SIZE * EXTCHUNK_SIZE];

	int elemsLeft = Heightmap_InitialCoverage(x1, z1, xCount, zCount, skip);
	if (!Heightmap_CalculateCoverage(x1, z1, xCount, zCount, elemsLeft, skip)) {
		Heightmap_FinishCoverage(x1, z1, xCount, zCount);
	}
}

void ClassicLighting_FreeState(void) {
	Mem_Free(classic_heightmap);
	classic_heightmap = NULL;
	Mem_Free(torch_lightmap);
	torch_lightmap = NULL;
}

void ClassicLighting_AllocState(void) {
	classic_heightmap = (cc_int16*)Mem_TryAlloc(World.Width * World.Length, 2);
	if (classic_heightmap) {
		ClassicLighting_Refresh();
	} else {
		World_OutOfMemory();
		return;
	}

	torch_lightmap = (cc_uint8*)Mem_TryAlloc(World.Volume, 1);
	if (torch_lightmap) {
		Mem_Set(torch_lightmap, 0, World.Volume);
		TorchLight_ScanWorld();
	}
}

static void ClassicLighting_SetActive(void) {
	cc_bool smoothLighting = false;
	if (!Game_ClassicMode) smoothLighting = Options_GetBool(OPT_SMOOTH_LIGHTING, false);

	Lighting.OnBlockChanged = ClassicLighting_OnBlockChanged;
	Lighting.Refresh        = ClassicLighting_Refresh;
	Lighting.IsLit          = ClassicLighting_IsLit;
	Lighting.Color          = smoothLighting ? SmoothLighting_Color : ClassicLighting_Color;
	Lighting.Color_XSide    = ClassicLighting_Color_XSide;

	Lighting.IsLit_Fast        = ClassicLighting_IsLit_Fast;
	Lighting.Color_Sprite_Fast = ClassicLighting_Color_Sprite_Fast;
	Lighting.Color_YMax_Fast   = ClassicLighting_Color_YMax_Fast;
	Lighting.Color_YMin_Fast   = ClassicLighting_Color_YMin_Fast;
	Lighting.Color_XSide_Fast  = ClassicLighting_Color_XSide_Fast;
	Lighting.Color_ZSide_Fast  = ClassicLighting_Color_ZSide_Fast;

	Lighting.FreeState  = ClassicLighting_FreeState;
	Lighting.AllocState = ClassicLighting_AllocState;
	Lighting.LightHint  = ClassicLighting_LightHint;
}


/*########################################################################################################################*
*---------------------------------------------------Lighting component----------------------------------------------------*
*#########################################################################################################################*/
static void Lighting_ApplyActive(void) {
	if (Lighting_Mode != LIGHTING_MODE_CLASSIC) {
		FancyLighting_SetActive();
	} else {
		ClassicLighting_SetActive();
	}
}

static void Lighting_SwitchActive(void) {
	Lighting.FreeState();
	Lighting_ApplyActive();
	Lighting.AllocState();
}

static void Lighting_HandleModeChanged(void* obj, cc_uint8 oldMode, cc_bool fromServer) {
	if (Lighting_Mode == oldMode) return;
	Builder_ApplyActive();

	if (World.Loaded) {
		Lighting_SwitchActive();
		MapRenderer_Refresh();
	} else {
		Lighting_ApplyActive();
	}
}

static void OnInit(void) {
	Lighting_Mode = Options_GetEnum(OPT_LIGHTING_MODE, LIGHTING_MODE_CLASSIC, LightingMode_Names, LIGHTING_MODE_COUNT);
	Lighting_ModeLockedByServer = false;
	Lighting_ModeSetByServer    = false;
	Lighting_ModeUserCached = Lighting_Mode;

	FancyLighting_OnInit();
	Lighting_ApplyActive();

	Event_Register_(&WorldEvents.LightingModeChanged, NULL, Lighting_HandleModeChanged);
}
static void OnReset(void)        { Lighting.FreeState(); }
static void OnNewMapLoaded(void) { Lighting.AllocState(); }

struct IGameComponent Lighting_Component = {
	OnInit,  /* Init  */
	OnReset, /* Free  */
	OnReset, /* Reset */
	OnReset, /* OnNewMap */
	OnNewMapLoaded /* OnNewMapLoaded */
};
