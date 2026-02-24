#include "Lighting.h"
#include "Block.h"
#include "Funcs.h"
#include "MapRenderer.h"
#include "Platform.h"
#include "World.h"
#include "Logger.h"
#include "Event.h"
#include "Game.h"
#include "ExtMath.h"
#include "Options.h"
#include "Queue.h"

/*########################################################################################################################*
*-------------------------------------------------Advanced lighting-------------------------------------------------------*
*#########################################################################################################################*/
/* Advanced lighting: Minecraft Alpha-style light propagation.
   - Sky light: level 15 from open sky, propagates into caves losing 1 per block
   - Block light: torches emit level 14, propagates outward losing 1 per block
   - Combined level = max(sky, block). 16-level palette lookup for face colors.
   - Smooth lighting (AO) works by averaging neighboring light levels via the Modern builder. */

#define ADV_MAX_LEVEL 15
#define ADV_TORCH_LEVEL 14
#define ADV_LAVA_LEVEL 15
#define ADV_MOB_SPAWN_THRESHOLD 7

#define ADV_SHADE_YMAX  0
#define ADV_SHADE_XSIDE 1
#define ADV_SHADE_ZSIDE 2
#define ADV_SHADE_YMIN  3
#define ADV_SHADE_COUNT 4

/* Per-block light level arrays (0-15 each) */
static cc_uint8* adv_skylight;
static cc_uint8* adv_blocklight;

/* Pre-computed color palette: [face_shade][light_level] */
static PackedCol adv_palette[ADV_SHADE_COUNT][FANCY_LIGHTING_LEVELS];

static struct Queue adv_queue;

struct AdvLightNode {
	int x, y, z;
	cc_uint8 level;
};

static const cc_int8 adv_dirs[6][3] = {
	{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
};


/*########################################################################################################################*
*------------------------------------------------------Palette------------------------------------------------------------*
*#########################################################################################################################*/
static void Adv_InitPalette(int shade, float shadeFactor) {
	int i;
	float t, factor;
	PackedCol dark;

	/* Very dark base color for deepest caves */
	dark = PackedCol_Scale(Env.ShadowCol, 0.05f);

	for (i = 0; i < FANCY_LIGHTING_LEVELS; i++) {
		t = (float)i / (float)ADV_MAX_LEVEL;
		/* Non-linear curve: quadratic with minimum 5% brightness */
		/* Gives natural light falloff similar to Minecraft Alpha */
		factor = 0.05f + 0.95f * t * t;

		adv_palette[shade][i] = PackedCol_Scale(
			PackedCol_Lerp(dark, Env.SunCol, factor),
			shadeFactor
		);
	}
}

static void Adv_InitAllPalettes(void) {
	Adv_InitPalette(ADV_SHADE_YMAX,  1.0f);
	Adv_InitPalette(ADV_SHADE_XSIDE, PACKEDCOL_SHADE_X);
	Adv_InitPalette(ADV_SHADE_ZSIDE, PACKEDCOL_SHADE_Z);
	Adv_InitPalette(ADV_SHADE_YMIN,  PACKEDCOL_SHADE_YMIN);
}


/*########################################################################################################################*
*---------------------------------------------------Light access----------------------------------------------------------*
*#########################################################################################################################*/
static cc_uint8 Adv_GetLight(int x, int y, int z) {
	int idx;
	cc_uint8 sky, blk;

	idx = World_Pack(x, y, z);
	sky = adv_skylight  ? adv_skylight[idx]  : 0;
	blk = adv_blocklight ? adv_blocklight[idx] : 0;

	return sky > blk ? sky : blk;
}

/* Whether light can pass through a block */
static cc_bool Adv_CanLightPass(BlockID block) {
	/* Transparent blocks (air, flowers, etc) always let light pass */
	if (!Blocks.BlocksLight[block]) return true;
	/* Leaves, water, ice, glass etc. also let light through */
	if (Blocks.Draw[block] == DRAW_TRANSPARENT_THICK) return true;
	if (Blocks.Draw[block] == DRAW_TRANSLUCENT) return true;
	return false;
}


/*########################################################################################################################*
*-------------------------------------------------Color functions---------------------------------------------------------*
*#########################################################################################################################*/
static PackedCol Adv_Color(int x, int y, int z) {
	if (!World_Contains(x, y, z))
		return y >= Env.EdgeHeight ? Env.SunCol : Env.ShadowCol;
	if (!adv_skylight)
		return ClassicLighting_IsLit(x, y, z) ? Env.SunCol : Env.ShadowCol;
	return adv_palette[ADV_SHADE_YMAX][Adv_GetLight(x, y, z)];
}

static PackedCol Adv_Color_XSide(int x, int y, int z) {
	if (!World_Contains(x, y, z))
		return y >= Env.EdgeHeight ? Env.SunXSide : Env.ShadowXSide;
	if (!adv_skylight)
		return ClassicLighting_IsLit(x, y, z) ? Env.SunXSide : Env.ShadowXSide;
	return adv_palette[ADV_SHADE_XSIDE][Adv_GetLight(x, y, z)];
}

static PackedCol Adv_Color_Sprite_Fast(int x, int y, int z) {
	if (!adv_skylight || !World_Contains(x, y, z))
		return ClassicLighting_IsLit_Fast(x, y, z) ? Env.SunCol : Env.ShadowCol;
	return adv_palette[ADV_SHADE_YMAX][Adv_GetLight(x, y, z)];
}

static PackedCol Adv_Color_YMax_Fast(int x, int y, int z) {
	if (!adv_skylight || !World_Contains(x, y, z))
		return ClassicLighting_IsLit_Fast(x, y, z) ? Env.SunCol : Env.ShadowCol;
	return adv_palette[ADV_SHADE_YMAX][Adv_GetLight(x, y, z)];
}

static PackedCol Adv_Color_YMin_Fast(int x, int y, int z) {
	if (!adv_skylight || !World_Contains(x, y, z))
		return ClassicLighting_IsLit_Fast(x, y, z) ? Env.SunYMin : Env.ShadowYMin;
	return adv_palette[ADV_SHADE_YMIN][Adv_GetLight(x, y, z)];
}

static PackedCol Adv_Color_XSide_Fast(int x, int y, int z) {
	if (!adv_skylight || !World_Contains(x, y, z))
		return ClassicLighting_IsLit_Fast(x, y, z) ? Env.SunXSide : Env.ShadowXSide;
	return adv_palette[ADV_SHADE_XSIDE][Adv_GetLight(x, y, z)];
}

static PackedCol Adv_Color_ZSide_Fast(int x, int y, int z) {
	if (!adv_skylight || !World_Contains(x, y, z))
		return ClassicLighting_IsLit_Fast(x, y, z) ? Env.SunZSide : Env.ShadowZSide;
	return adv_palette[ADV_SHADE_ZSIDE][Adv_GetLight(x, y, z)];
}


/*########################################################################################################################*
*-----------------------------------------------------IsLit--------------------------------------------------------------*
*#########################################################################################################################*/
static cc_bool Adv_IsLit(int x, int y, int z) {
	if (!adv_skylight) return ClassicLighting_IsLit(x, y, z);
	if (!World_Contains(x, y, z)) return y >= Env.EdgeHeight;
	return Adv_GetLight(x, y, z) > ADV_MOB_SPAWN_THRESHOLD;
}

static cc_bool Adv_IsLit_Fast(int x, int y, int z) {
	if (!adv_skylight) return ClassicLighting_IsLit_Fast(x, y, z);
	if (!World_Contains(x, y, z)) return y >= Env.EdgeHeight;
	return Adv_GetLight(x, y, z) > ADV_MOB_SPAWN_THRESHOLD;
}


/*########################################################################################################################*
*------------------------------------------------Sky light propagation----------------------------------------------------*
*#########################################################################################################################*/
static void Adv_PropagateSkyLight(void) {
	int x, y, z, h, d, nh, nx, ny, nz, nIdx, minY, maxY;
	struct AdvLightNode entry, cur;
	BlockID nb;

	/* Phase 1: Set sky light = 15 for all blocks above the heightmap */
	for (z = 0; z < World.Length; z++) {
		for (x = 0; x < World.Width; x++) {
			h = ClassicLighting_GetLightHeight(x, z);
			for (y = h + 1; y < World.Height; y++) {
				adv_skylight[World_Pack(x, y, z)] = ADV_MAX_LEVEL;
			}
		}
	}

	/* Phase 2: Seed BFS from sky-adjacent underground transparent blocks.
	   For each pair of adjacent columns with different heights, the taller
	   column has underground blocks that border sky blocks in the shorter column. */
	for (z = 0; z < World.Length; z++) {
		for (x = 0; x < World.Width; x++) {
			h = ClassicLighting_GetLightHeight(x, z);

			/* Check 4 horizontal neighbors */
			for (d = 0; d < 4; d++) {
				nx = x + adv_dirs[d][0];
				nz = z + adv_dirs[d][2];
				if (!World_ContainsXZ(nx, nz)) continue;

				nh = ClassicLighting_GetLightHeight(nx, nz);
				if (h >= nh) continue; /* neighbor is shorter or equal, no underground blocks to seed */

				/* Our column has sky from h+1 upward. Neighbor column is underground below nh+1.
				   The overlap is from max(h+1, 0) to nh where our sky meets their underground. */
				minY = h + 1;
				maxY = nh;
				if (minY < 0) minY = 0;
				if (maxY >= World.Height) maxY = World.Height - 1;

				for (y = minY; y <= maxY; y++) {
					nb = World_GetBlock(nx, y, nz);
					if (Adv_CanLightPass(nb)) {
						nIdx = World_Pack(nx, y, nz);
						if (adv_skylight[nIdx] < ADV_MAX_LEVEL - 1) {
							entry.x = nx; entry.y = y; entry.z = nz;
							entry.level = ADV_MAX_LEVEL - 1;
							Queue_Enqueue(&adv_queue, &entry);
						}
					}
				}
			}
		}
	}

	/* Phase 3: BFS - spread sky light through transparent underground blocks */
	while (adv_queue.count > 0) {
		cur = *(struct AdvLightNode*)Queue_Dequeue(&adv_queue);
		nIdx = World_Pack(cur.x, cur.y, cur.z);

		if (adv_skylight[nIdx] >= cur.level) continue;
		if (cur.level == 0) continue;

		adv_skylight[nIdx] = cur.level;

		if (cur.level <= 1) continue;

		for (d = 0; d < 6; d++) {
			nx = cur.x + adv_dirs[d][0];
			ny = cur.y + adv_dirs[d][1];
			nz = cur.z + adv_dirs[d][2];

			if (!World_Contains(nx, ny, nz)) continue;

			nIdx = World_Pack(nx, ny, nz);
			if (adv_skylight[nIdx] >= cur.level - 1) continue;

			nb = World_GetBlock(nx, ny, nz);
			if (!Adv_CanLightPass(nb)) continue;

			entry.x = nx; entry.y = ny; entry.z = nz;
			entry.level = cur.level - 1;
			Queue_Enqueue(&adv_queue, &entry);
		}
	}
}


/*########################################################################################################################*
*----------------------------------------------Block light propagation----------------------------------------------------*
*#########################################################################################################################*/
static cc_uint8 Adv_GetBlockEmission(BlockID block) {
	if (block == BLOCK_TORCH) return ADV_TORCH_LEVEL;
	if (block == BLOCK_LAVA || block == BLOCK_STILL_LAVA) return ADV_LAVA_LEVEL;
	return 0;
}

static void Adv_PropagateBlockLight(void) {
	int x, y, z, d, nx, ny, nz, nIdx;
	struct AdvLightNode entry, cur;
	BlockID block, nb;
	cc_uint8 emission;

	/* Scan world for light-emitting blocks */
	for (y = 0; y < World.Height; y++) {
		for (z = 0; z < World.Length; z++) {
			for (x = 0; x < World.Width; x++) {
				block = World_GetBlock(x, y, z);
				emission = Adv_GetBlockEmission(block);
				if (emission > 0) {
					entry.x = x; entry.y = y; entry.z = z;
					entry.level = emission;
					Queue_Enqueue(&adv_queue, &entry);
				}
			}
		}
	}

	/* BFS - spread block light through transparent blocks */
	while (adv_queue.count > 0) {
		cur = *(struct AdvLightNode*)Queue_Dequeue(&adv_queue);
		nIdx = World_Pack(cur.x, cur.y, cur.z);

		if (adv_blocklight[nIdx] >= cur.level) continue;
		if (cur.level == 0) continue;

		adv_blocklight[nIdx] = cur.level;

		if (cur.level <= 1) continue;

		for (d = 0; d < 6; d++) {
			nx = cur.x + adv_dirs[d][0];
			ny = cur.y + adv_dirs[d][1];
			nz = cur.z + adv_dirs[d][2];

			if (!World_Contains(nx, ny, nz)) continue;

			nIdx = World_Pack(nx, ny, nz);
			if (adv_blocklight[nIdx] >= cur.level - 1) continue;

			nb = World_GetBlock(nx, ny, nz);
			if (!Adv_CanLightPass(nb)) continue;

			entry.x = nx; entry.y = ny; entry.z = nz;
			entry.level = cur.level - 1;
			Queue_Enqueue(&adv_queue, &entry);
		}
	}
}


/*########################################################################################################################*
*-------------------------------------------------Block change updates----------------------------------------------------*
*#########################################################################################################################*/
static void Adv_UpdateSkyArea(int bx, int by, int bz) {
	int radius = ADV_MAX_LEVEL;
	int x, y, z, d, nx, ny, nz, idx, h;
	int minX, maxX, minY, maxY, minZ, maxZ;
	int bminX, bmaxX, bminY, bmaxY, bminZ, bmaxZ;
	cc_uint8 sky;
	struct AdvLightNode entry, cur;
	BlockID nb;

	/* Clear sky light within radius, restoring sky=15 above heightmap */
	minX = max(bx - radius, 0); maxX = min(bx + radius, World.MaxX);
	minY = max(by - radius, 0); maxY = min(by + radius, World.MaxY);
	minZ = max(bz - radius, 0); maxZ = min(bz + radius, World.MaxZ);

	for (y = minY; y <= maxY; y++)
		for (z = minZ; z <= maxZ; z++)
			for (x = minX; x <= maxX; x++) {
				idx = World_Pack(x, y, z);
				h = ClassicLighting_GetLightHeight(x, z);
				adv_skylight[idx] = (y > h) ? ADV_MAX_LEVEL : 0;
			}

	/* Seed BFS from sky=15 blocks that have underground transparent neighbors */
	for (y = minY; y <= maxY; y++)
		for (z = minZ; z <= maxZ; z++)
			for (x = minX; x <= maxX; x++) {
				idx = World_Pack(x, y, z);
				if (adv_skylight[idx] != ADV_MAX_LEVEL) continue;

				for (d = 0; d < 6; d++) {
					nx = x + adv_dirs[d][0];
					ny = y + adv_dirs[d][1];
					nz = z + adv_dirs[d][2];
					if (!World_Contains(nx, ny, nz)) continue;

					idx = World_Pack(nx, ny, nz);
					if (adv_skylight[idx] >= ADV_MAX_LEVEL - 1) continue;

					nb = World_GetBlock(nx, ny, nz);
					if (Adv_CanLightPass(nb)) {
						entry.x = nx; entry.y = ny; entry.z = nz;
						entry.level = ADV_MAX_LEVEL - 1;
						Queue_Enqueue(&adv_queue, &entry);
					}
				}
			}

	/* Seed from border shell (light re-entering from outside the cleared area) */
	bminX = max(bx - radius - 1, 0); bmaxX = min(bx + radius + 1, World.MaxX);
	bminY = max(by - radius - 1, 0); bmaxY = min(by + radius + 1, World.MaxY);
	bminZ = max(bz - radius - 1, 0); bmaxZ = min(bz + radius + 1, World.MaxZ);

	for (y = bminY; y <= bmaxY; y++)
		for (z = bminZ; z <= bmaxZ; z++)
			for (x = bminX; x <= bmaxX; x++) {
				/* Only process blocks on the border shell */
				if (x > minX && x < maxX && y > minY && y < maxY && z > minZ && z < maxZ) continue;

				idx = World_Pack(x, y, z);
				sky = adv_skylight[idx];
				if (sky <= 1) continue;

				for (d = 0; d < 6; d++) {
					nx = x + adv_dirs[d][0];
					ny = y + adv_dirs[d][1];
					nz = z + adv_dirs[d][2];
					if (!World_Contains(nx, ny, nz)) continue;
					if (nx < minX || nx > maxX || ny < minY || ny > maxY || nz < minZ || nz > maxZ) continue;

					idx = World_Pack(nx, ny, nz);
					if (adv_skylight[idx] >= sky - 1) continue;

					nb = World_GetBlock(nx, ny, nz);
					if (Adv_CanLightPass(nb)) {
						entry.x = nx; entry.y = ny; entry.z = nz;
						entry.level = sky - 1;
						Queue_Enqueue(&adv_queue, &entry);
					}
				}
			}

	/* BFS */
	while (adv_queue.count > 0) {
		cur = *(struct AdvLightNode*)Queue_Dequeue(&adv_queue);
		idx = World_Pack(cur.x, cur.y, cur.z);

		if (adv_skylight[idx] >= cur.level) continue;
		if (cur.level == 0) continue;

		adv_skylight[idx] = cur.level;
		if (cur.level <= 1) continue;

		for (d = 0; d < 6; d++) {
			nx = cur.x + adv_dirs[d][0];
			ny = cur.y + adv_dirs[d][1];
			nz = cur.z + adv_dirs[d][2];
			if (!World_Contains(nx, ny, nz)) continue;

			idx = World_Pack(nx, ny, nz);
			if (adv_skylight[idx] >= cur.level - 1) continue;

			nb = World_GetBlock(nx, ny, nz);
			if (!Adv_CanLightPass(nb)) continue;

			entry.x = nx; entry.y = ny; entry.z = nz;
			entry.level = cur.level - 1;
			Queue_Enqueue(&adv_queue, &entry);
		}
	}
}

static void Adv_UpdateBlockArea(int bx, int by, int bz) {
	int radius = ADV_TORCH_LEVEL;
	int r2 = radius * 2;
	int x, y, z, d, nx, ny, nz, idx;
	int minX, maxX, minY, maxY, minZ, maxZ;
	int sminX, smaxX, sminY, smaxY, sminZ, smaxZ;
	cc_uint8 blk;
	struct AdvLightNode entry, cur;
	BlockID block, nb;
	cc_uint8 emission;

	/* Clear block light within radius */
	minX = max(bx - radius, 0); maxX = min(bx + radius, World.MaxX);
	minY = max(by - radius, 0); maxY = min(by + radius, World.MaxY);
	minZ = max(bz - radius, 0); maxZ = min(bz + radius, World.MaxZ);

	for (y = minY; y <= maxY; y++)
		for (z = minZ; z <= maxZ; z++)
			for (x = minX; x <= maxX; x++)
				adv_blocklight[World_Pack(x, y, z)] = 0;

	/* Re-seed from light sources in double-radius area */
	sminX = max(bx - r2, 0); smaxX = min(bx + r2, World.MaxX);
	sminY = max(by - r2, 0); smaxY = min(by + r2, World.MaxY);
	sminZ = max(bz - r2, 0); smaxZ = min(bz + r2, World.MaxZ);

	for (y = sminY; y <= smaxY; y++)
		for (z = sminZ; z <= smaxZ; z++)
			for (x = sminX; x <= smaxX; x++) {
				block = World_GetBlock(x, y, z);
				emission = Adv_GetBlockEmission(block);
				if (emission > 0) {
					entry.x = x; entry.y = y; entry.z = z;
					entry.level = emission;
					Queue_Enqueue(&adv_queue, &entry);
				}
			}

	/* Seed from border blocks with existing block light (outside cleared area) */
	for (y = sminY; y <= smaxY; y++)
		for (z = sminZ; z <= smaxZ; z++)
			for (x = sminX; x <= smaxX; x++) {
				if (x >= minX && x <= maxX && y >= minY && y <= maxY && z >= minZ && z <= maxZ) continue;

				idx = World_Pack(x, y, z);
				blk = adv_blocklight[idx];
				if (blk <= 1) continue;

				for (d = 0; d < 6; d++) {
					nx = x + adv_dirs[d][0];
					ny = y + adv_dirs[d][1];
					nz = z + adv_dirs[d][2];
					if (!World_Contains(nx, ny, nz)) continue;
					if (nx < minX || nx > maxX || ny < minY || ny > maxY || nz < minZ || nz > maxZ) continue;

					idx = World_Pack(nx, ny, nz);
					if (adv_blocklight[idx] >= blk - 1) continue;

					nb = World_GetBlock(nx, ny, nz);
					if (Adv_CanLightPass(nb)) {
						entry.x = nx; entry.y = ny; entry.z = nz;
						entry.level = blk - 1;
						Queue_Enqueue(&adv_queue, &entry);
					}
				}
			}

	/* BFS */
	while (adv_queue.count > 0) {
		cur = *(struct AdvLightNode*)Queue_Dequeue(&adv_queue);
		idx = World_Pack(cur.x, cur.y, cur.z);

		if (adv_blocklight[idx] >= cur.level) continue;
		if (cur.level == 0) continue;

		adv_blocklight[idx] = cur.level;
		if (cur.level <= 1) continue;

		for (d = 0; d < 6; d++) {
			nx = cur.x + adv_dirs[d][0];
			ny = cur.y + adv_dirs[d][1];
			nz = cur.z + adv_dirs[d][2];
			if (!World_Contains(nx, ny, nz)) continue;

			idx = World_Pack(nx, ny, nz);
			if (adv_blocklight[idx] >= cur.level - 1) continue;

			nb = World_GetBlock(nx, ny, nz);
			if (!Adv_CanLightPass(nb)) continue;

			entry.x = nx; entry.y = ny; entry.z = nz;
			entry.level = cur.level - 1;
			Queue_Enqueue(&adv_queue, &entry);
		}
	}
}

static void Adv_RefreshChunksInRadius(int bx, int by, int bz, int radius) {
	int cx, cy, cz;
	int minCX = max(bx - radius, 0) >> CHUNK_SHIFT;
	int maxCX = min(bx + radius, World.MaxX) >> CHUNK_SHIFT;
	int minCY = max(by - radius, 0) >> CHUNK_SHIFT;
	int maxCY = min(by + radius, World.MaxY) >> CHUNK_SHIFT;
	int minCZ = max(bz - radius, 0) >> CHUNK_SHIFT;
	int maxCZ = min(bz + radius, World.MaxZ) >> CHUNK_SHIFT;

	for (cy = minCY; cy <= maxCY; cy++)
		for (cz = minCZ; cz <= maxCZ; cz++)
			for (cx = minCX; cx <= maxCX; cx++)
				MapRenderer_RefreshChunk(cx, cy, cz);
}

static void Adv_OnBlockChanged(int x, int y, int z, BlockID oldBlock, BlockID newBlock) {
	cc_bool lightPassChanged;
	if (oldBlock == newBlock) return;

	/* Update Classic heightmap (needed for sky light calculation) */
	ClassicLighting_OnBlockChanged(x, y, z, oldBlock, newBlock);

	lightPassChanged = Adv_CanLightPass(oldBlock) != Adv_CanLightPass(newBlock);

	/* Update sky light if light transmission changed or a block was placed/removed */
	if (adv_skylight && lightPassChanged) {
		Adv_UpdateSkyArea(x, y, z);
	}

	/* Update block light if torch/lava placed/removed or light passage changed */
	if (adv_blocklight) {
		if (Adv_GetBlockEmission(oldBlock) > 0 || Adv_GetBlockEmission(newBlock) > 0 || lightPassChanged) {
			Adv_UpdateBlockArea(x, y, z);
		}
	}

	/* Refresh chunks in the affected radius */
	Adv_RefreshChunksInRadius(x, y, z, ADV_MAX_LEVEL);
}


/*########################################################################################################################*
*-------------------------------------------------State management--------------------------------------------------------*
*#########################################################################################################################*/
static void Adv_FreeState(void) {
	ClassicLighting_FreeState();

	Mem_Free(adv_skylight);
	adv_skylight = NULL;
	Mem_Free(adv_blocklight);
	adv_blocklight = NULL;
	Queue_Clear(&adv_queue);
}

static void Adv_AllocState(void) {
	ClassicLighting_AllocState();
	Adv_InitAllPalettes();

	Queue_Init(&adv_queue, sizeof(struct AdvLightNode));

	adv_skylight = (cc_uint8*)Mem_TryAlloc(World.Volume, 1);
	adv_blocklight = (cc_uint8*)Mem_TryAlloc(World.Volume, 1);

	if (!adv_skylight || !adv_blocklight) {
		Mem_Free(adv_skylight);
		Mem_Free(adv_blocklight);
		adv_skylight = NULL;
		adv_blocklight = NULL;
		return;
	}

	Mem_Set(adv_skylight, 0, World.Volume);
	Mem_Set(adv_blocklight, 0, World.Volume);

	Adv_PropagateSkyLight();
	Adv_PropagateBlockLight();
}

static void Adv_Refresh(void) {
	ClassicLighting_Refresh();
	/* Re-propagate all lighting from scratch */
	if (adv_skylight) Mem_Set(adv_skylight, 0, World.Volume);
	if (adv_blocklight) Mem_Set(adv_blocklight, 0, World.Volume);
	if (adv_skylight) Adv_PropagateSkyLight();
	if (adv_blocklight) Adv_PropagateBlockLight();
}

static void Adv_LightHint(int startX, int startY, int startZ) {
	/* Heightmap still needs to be calculated per-chunk for the extended region */
	ClassicLighting_LightHint(startX, startY, startZ);
}


/*########################################################################################################################*
*--------------------------------------------------SetActive/Init---------------------------------------------------------*
*#########################################################################################################################*/
void AdvancedLighting_SetActive(void) {
	Lighting.OnBlockChanged = Adv_OnBlockChanged;
	Lighting.Refresh        = Adv_Refresh;
	Lighting.IsLit          = Adv_IsLit;
	Lighting.Color          = Adv_Color;
	Lighting.Color_XSide    = Adv_Color_XSide;

	Lighting.IsLit_Fast        = Adv_IsLit_Fast;
	Lighting.Color_Sprite_Fast = Adv_Color_Sprite_Fast;
	Lighting.Color_YMax_Fast   = Adv_Color_YMax_Fast;
	Lighting.Color_YMin_Fast   = Adv_Color_YMin_Fast;
	Lighting.Color_XSide_Fast  = Adv_Color_XSide_Fast;
	Lighting.Color_ZSide_Fast  = Adv_Color_ZSide_Fast;

	Lighting.FreeState  = Adv_FreeState;
	Lighting.AllocState = Adv_AllocState;
	Lighting.LightHint  = Adv_LightHint;
}

static void Adv_OnEnvVariableChanged(void* obj, int envVar) {
	if (Lighting_Mode != LIGHTING_MODE_ADVANCED) return;

	if (envVar == ENV_VAR_SUN_COLOR || envVar == ENV_VAR_SHADOW_COLOR) {
		Adv_InitAllPalettes();
		MapRenderer_Refresh();
	}
}

void AdvancedLighting_OnInit(void) {
	Event_Register_(&WorldEvents.EnvVarChanged, NULL, Adv_OnEnvVariableChanged);
}
