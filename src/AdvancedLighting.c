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

/* Nibble-packed light array: upper nibble = sky (0-15), lower nibble = block (0-15) */
static cc_uint8* adv_light;

#define ADV_GET_SKY(packed) ((packed) >> 4)
#define ADV_GET_BLK(packed) ((packed) & 0x0F)
#define ADV_SET_SKY(ptr, val) (*(ptr) = (*(ptr) & 0x0F) | ((val) << 4))
#define ADV_SET_BLK(ptr, val) (*(ptr) = (*(ptr) & 0xF0) | (val))

/* Pre-computed color palettes: sky light (changes with day/night) and block light (constant warm) */
static PackedCol adv_sky_palette[ADV_SHADE_COUNT][FANCY_LIGHTING_LEVELS];
static PackedCol adv_blk_palette[ADV_SHADE_COUNT][FANCY_LIGHTING_LEVELS];
/* Pre-resolved combined palette: for each packed (sky<<4|block) byte, the final color per shade. */
/* Eliminates per-vertex sky subtraction, double palette lookup, and BrighterCol comparison. */
static PackedCol adv_combined[ADV_SHADE_COUNT][256];

/* Pure white torch glow - stays constant regardless of day/night */
#define ADV_TORCH_COL PackedCol_Make(255, 255, 255, 255)

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
/* How much to subtract from sky light levels at night (0 = day, up to 4 = full night) */
static int adv_sky_subtraction;

static void Adv_InitSkyPalette(int shade, float shadeFactor) {
	int i;
	float t, factor;
	PackedCol dark;

	/* Very dark base color for deepest caves */
	dark = PackedCol_Scale(Env.ShadowCol, 0.05f);

	for (i = 0; i < FANCY_LIGHTING_LEVELS; i++) {
		t = (float)i / (float)ADV_MAX_LEVEL;
		/* Non-linear curve: quadratic with minimum 5% brightness */
		factor = 0.05f + 0.95f * t * t;

		adv_sky_palette[shade][i] = PackedCol_Scale(
			PackedCol_Lerp(dark, Env.SunCol, factor),
			shadeFactor
		);
	}
}

static void Adv_InitBlkPalette(int shade, float shadeFactor) {
	int i;
	float t, factor;
	PackedCol dark, torchCol;

	/* Block light uses pure white torch glow, unaffected by day/night */
	torchCol = ADV_TORCH_COL;
	dark = PackedCol_Make(3, 3, 3, 255); /* near-black neutral */

	for (i = 0; i < FANCY_LIGHTING_LEVELS; i++) {
		t = (float)i / (float)ADV_MAX_LEVEL;
		factor = 0.05f + 0.95f * t * t;

		adv_blk_palette[shade][i] = PackedCol_Scale(
			PackedCol_Lerp(dark, torchCol, factor),
			shadeFactor
		);
	}
}

static void Adv_UpdateSkySubtraction(void) {
	/* At night, sky light is fully eliminated so only torch/block light remains.
	   Full day (SunCol ~255,255,255): subtraction = 0
	   Full night (SunCol ~85,85,85):  subtraction = 15 (all sky light zeroed) */
	int avg = (PackedCol_R(Env.SunCol) + PackedCol_G(Env.SunCol) + PackedCol_B(Env.SunCol)) / 3;
	if (avg >= 250) {
		adv_sky_subtraction = 0;
	} else {
		/* Linear from 0 at avg=250 to ADV_MAX_LEVEL at avg=85 */
		adv_sky_subtraction = (250 - avg) * ADV_MAX_LEVEL / 165;
		if (adv_sky_subtraction > ADV_MAX_LEVEL) adv_sky_subtraction = ADV_MAX_LEVEL;
	}
}

static void Adv_InitAllPalettes(void) {
	Adv_UpdateSkySubtraction();

	Adv_InitSkyPalette(ADV_SHADE_YMAX,  1.0f);
	Adv_InitSkyPalette(ADV_SHADE_XSIDE, PACKEDCOL_SHADE_X);
	Adv_InitSkyPalette(ADV_SHADE_ZSIDE, PACKEDCOL_SHADE_Z);
	Adv_InitSkyPalette(ADV_SHADE_YMIN,  PACKEDCOL_SHADE_YMIN);

	Adv_InitBlkPalette(ADV_SHADE_YMAX,  1.0f);
	Adv_InitBlkPalette(ADV_SHADE_XSIDE, PACKEDCOL_SHADE_X);
	Adv_InitBlkPalette(ADV_SHADE_ZSIDE, PACKEDCOL_SHADE_Z);
	Adv_InitBlkPalette(ADV_SHADE_YMIN,  PACKEDCOL_SHADE_YMIN);

	/* Build combined palette: pre-resolve BrighterCol(sky, block) for all 256 packed values */
	{
		int shade, sky, blk, effSky;
		PackedCol skyCol, blkCol;
		int sumSky, sumBlk;

		for (shade = 0; shade < ADV_SHADE_COUNT; shade++) {
			for (sky = 0; sky < FANCY_LIGHTING_LEVELS; sky++) {
				effSky = sky - adv_sky_subtraction;
				if (effSky < 0) effSky = 0;
				skyCol = adv_sky_palette[shade][effSky];

				for (blk = 0; blk < FANCY_LIGHTING_LEVELS; blk++) {
					blkCol = adv_blk_palette[shade][blk];
					sumSky = PackedCol_R(skyCol) + PackedCol_G(skyCol) + PackedCol_B(skyCol);
					sumBlk = PackedCol_R(blkCol) + PackedCol_G(blkCol) + PackedCol_B(blkCol);
					adv_combined[shade][(sky << 4) | blk] = sumSky >= sumBlk ? skyCol : blkCol;
				}
			}
		}
	}
}


/*########################################################################################################################*
*---------------------------------------------------Light access----------------------------------------------------------*
*#########################################################################################################################*/
static cc_uint8 Adv_GetLight(int x, int y, int z) {
	int idx;
	cc_uint8 packed, sky, blk;

	if (!adv_light) return 0;
	idx = World_Pack(x, y, z);
	packed = adv_light[idx];
	sky = ADV_GET_SKY(packed);
	blk = ADV_GET_BLK(packed);

	return sky > blk ? sky : blk;
}

/* Look up pre-resolved color from combined palette (single array read + single lookup) */
static PackedCol Adv_GetColor(int x, int y, int z, int shade) {
	return adv_combined[shade][adv_light[World_Pack(x, y, z)]];
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
	if (!adv_light)
		return ClassicLighting_IsLit(x, y, z) ? Env.SunCol : Env.ShadowCol;
	return Adv_GetColor(x, y, z, ADV_SHADE_YMAX);
}

static PackedCol Adv_Color_XSide(int x, int y, int z) {
	if (!World_Contains(x, y, z))
		return y >= Env.EdgeHeight ? Env.SunXSide : Env.ShadowXSide;
	if (!adv_light)
		return ClassicLighting_IsLit(x, y, z) ? Env.SunXSide : Env.ShadowXSide;
	return Adv_GetColor(x, y, z, ADV_SHADE_XSIDE);
}

static PackedCol Adv_Color_Sprite_Fast(int x, int y, int z) {
	if (!adv_light || !World_Contains(x, y, z))
		return ClassicLighting_IsLit_Fast(x, y, z) ? Env.SunCol : Env.ShadowCol;
	return Adv_GetColor(x, y, z, ADV_SHADE_YMAX);
}

static PackedCol Adv_Color_YMax_Fast(int x, int y, int z) {
	if (!adv_light || !World_Contains(x, y, z))
		return ClassicLighting_IsLit_Fast(x, y, z) ? Env.SunCol : Env.ShadowCol;
	return Adv_GetColor(x, y, z, ADV_SHADE_YMAX);
}

static PackedCol Adv_Color_YMin_Fast(int x, int y, int z) {
	if (!adv_light || !World_Contains(x, y, z))
		return ClassicLighting_IsLit_Fast(x, y, z) ? Env.SunYMin : Env.ShadowYMin;
	return Adv_GetColor(x, y, z, ADV_SHADE_YMIN);
}

static PackedCol Adv_Color_XSide_Fast(int x, int y, int z) {
	if (!adv_light || !World_Contains(x, y, z))
		return ClassicLighting_IsLit_Fast(x, y, z) ? Env.SunXSide : Env.ShadowXSide;
	return Adv_GetColor(x, y, z, ADV_SHADE_XSIDE);
}

static PackedCol Adv_Color_ZSide_Fast(int x, int y, int z) {
	if (!adv_light || !World_Contains(x, y, z))
		return ClassicLighting_IsLit_Fast(x, y, z) ? Env.SunZSide : Env.ShadowZSide;
	return Adv_GetColor(x, y, z, ADV_SHADE_ZSIDE);
}


/*########################################################################################################################*
*-----------------------------------------------------IsLit--------------------------------------------------------------*
*#########################################################################################################################*/
static cc_bool Adv_IsLit(int x, int y, int z) {
	if (!adv_light) return ClassicLighting_IsLit(x, y, z);
	if (!World_Contains(x, y, z)) return y >= Env.EdgeHeight;
	return Adv_GetLight(x, y, z) > ADV_MOB_SPAWN_THRESHOLD;
}

static cc_bool Adv_IsLit_Fast(int x, int y, int z) {
	if (!adv_light) return ClassicLighting_IsLit_Fast(x, y, z);
	if (!World_Contains(x, y, z)) return y >= Env.EdgeHeight;
	return Adv_GetLight(x, y, z) > ADV_MOB_SPAWN_THRESHOLD;
}


/*########################################################################################################################*
*------------------------------------------------Sky light propagation----------------------------------------------------*
*#########################################################################################################################*/
/* BFS drain helper: propagates queued sky light entries */
static void Adv_DrainSkyBFS(void) {
	int d, nx, ny, nz, nIdx;
	struct AdvLightNode entry, cur;
	BlockID nb;

	while (adv_queue.count > 0) {
		cur = *(struct AdvLightNode*)Queue_Dequeue(&adv_queue);
		nIdx = World_Pack(cur.x, cur.y, cur.z);

		if (ADV_GET_SKY(adv_light[nIdx]) >= cur.level) continue;
		if (cur.level == 0) continue;

		ADV_SET_SKY(&adv_light[nIdx], cur.level);
		if (cur.level <= 1) continue;

		for (d = 0; d < 6; d++) {
			nx = cur.x + adv_dirs[d][0];
			ny = cur.y + adv_dirs[d][1];
			nz = cur.z + adv_dirs[d][2];
			if (!World_Contains(nx, ny, nz)) continue;

			nIdx = World_Pack(nx, ny, nz);
			if (ADV_GET_SKY(adv_light[nIdx]) >= cur.level - 1) continue;

			nb = World_GetBlock(nx, ny, nz);
			if (!Adv_CanLightPass(nb)) continue;

			entry.x = nx; entry.y = ny; entry.z = nz;
			entry.level = cur.level - 1;
			Queue_Enqueue(&adv_queue, &entry);
		}
	}
}

static void Adv_PropagateSkyLight(void) {
	int x, y, z, h, d, nh, nx, nz, nIdx, minY, maxY;
	struct AdvLightNode entry;
	BlockID nb;
	int seedCount;

	/* Phase 1: Set sky light = 15 for all blocks above the heightmap */
	for (z = 0; z < World.Length; z++) {
		for (x = 0; x < World.Width; x++) {
			h = ClassicLighting_GetLightHeight(x, z);
			/* GetLightHeight returns -10 for all-transparent columns, so clamp to 0 */
			y = h + 1;
			if (y < 0) y = 0;
			for (; y < World.Height; y++) {
				ADV_SET_SKY(&adv_light[World_Pack(x, y, z)], ADV_MAX_LEVEL);
			}
		}
	}

	/* Phase 1.5: Seed blocks AT the heightmap that are light-passable (e.g. water).
	   The heightmap marks blocks that have BlocksLight=true, but water/leaves etc.
	   are still transparent in our system. Seed them from the sky above. */
	for (z = 0; z < World.Length; z++) {
		for (x = 0; x < World.Width; x++) {
			h = ClassicLighting_GetLightHeight(x, z);
			if (h < 0 || h >= World.Height) continue;
			nb = World_GetBlock(x, h, z);
			if (Adv_CanLightPass(nb)) {
				entry.x = x; entry.y = h; entry.z = z;
				entry.level = ADV_MAX_LEVEL - 1;
				Queue_Enqueue(&adv_queue, &entry);
			}
		}
	}

	/* Flush Phase 1.5 seeds */
	Adv_DrainSkyBFS();

	/* Phase 2+3 combined: Seed underground transparent blocks adjacent to sky,
	   draining BFS periodically to keep peak queue size bounded.
	   Without batching, floating island worlds seed millions of entries before
	   any get consumed, causing the queue to exhaust available memory. */
	seedCount = 0;
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
					/* Verify the sky-column block at this level is actually transparent;
					   solid blocks can get sky=15 from SHADES_FROM_BELOW heightmap offset
					   and must not seed light through themselves */
					if (!Adv_CanLightPass(World_GetBlock(x, y, z))) continue;

					nb = World_GetBlock(nx, y, nz);
					if (Adv_CanLightPass(nb)) {
						nIdx = World_Pack(nx, y, nz);
						if (ADV_GET_SKY(adv_light[nIdx]) < ADV_MAX_LEVEL - 1) {
							entry.x = nx; entry.y = y; entry.z = nz;
							entry.level = ADV_MAX_LEVEL - 1;
							Queue_Enqueue(&adv_queue, &entry);
							seedCount++;
						}
					}
				}
			}

			/* Drain BFS periodically to keep queue memory bounded */
			if (seedCount >= 50000) {
				Adv_DrainSkyBFS();
				seedCount = 0;
			}
		}
	}

	/* Final drain for any remaining seeds */
	Adv_DrainSkyBFS();
}


/*########################################################################################################################*
*----------------------------------------------Block light propagation----------------------------------------------------*
*#########################################################################################################################*/
static cc_uint8 Adv_GetBlockEmission(BlockID block) {
	if (block == BLOCK_TORCH) return ADV_TORCH_LEVEL;
#if defined EXTENDED_BLOCKS
	if (block >= BLOCK_TORCH_S && block <= BLOCK_TORCH_W) return ADV_TORCH_LEVEL;
#endif
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

		if (ADV_GET_BLK(adv_light[nIdx]) >= cur.level) continue;
		if (cur.level == 0) continue;

		ADV_SET_BLK(&adv_light[nIdx], cur.level);

		if (cur.level <= 1) continue;

		for (d = 0; d < 6; d++) {
			nx = cur.x + adv_dirs[d][0];
			ny = cur.y + adv_dirs[d][1];
			nz = cur.z + adv_dirs[d][2];

			if (!World_Contains(nx, ny, nz)) continue;

			nIdx = World_Pack(nx, ny, nz);
			if (ADV_GET_BLK(adv_light[nIdx]) >= cur.level - 1) continue;

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
				ADV_SET_SKY(&adv_light[idx], (y > h) ? ADV_MAX_LEVEL : 0);
			}

	/* Seed BFS from sky=15 blocks that have underground transparent neighbors */
	for (y = minY; y <= maxY; y++)
		for (z = minZ; z <= maxZ; z++)
			for (x = minX; x <= maxX; x++) {
				idx = World_Pack(x, y, z);
				if (ADV_GET_SKY(adv_light[idx]) != ADV_MAX_LEVEL) continue;
				/* Only transparent sky blocks should seed neighbors;
				   solid blocks can get sky=15 from SHADES_FROM_BELOW offset */
				if (!Adv_CanLightPass(World_GetBlock(x, y, z))) continue;

				for (d = 0; d < 6; d++) {
					nx = x + adv_dirs[d][0];
					ny = y + adv_dirs[d][1];
					nz = z + adv_dirs[d][2];
					if (!World_Contains(nx, ny, nz)) continue;

					idx = World_Pack(nx, ny, nz);
					if (ADV_GET_SKY(adv_light[idx]) >= ADV_MAX_LEVEL - 1) continue;

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
				sky = ADV_GET_SKY(adv_light[idx]);
				if (sky <= 1) continue;
				/* Don't seed from solid blocks that incorrectly have sky light */
				if (!Adv_CanLightPass(World_GetBlock(x, y, z))) continue;

				for (d = 0; d < 6; d++) {
					nx = x + adv_dirs[d][0];
					ny = y + adv_dirs[d][1];
					nz = z + adv_dirs[d][2];
					if (!World_Contains(nx, ny, nz)) continue;
					if (nx < minX || nx > maxX || ny < minY || ny > maxY || nz < minZ || nz > maxZ) continue;

					idx = World_Pack(nx, ny, nz);
					if (ADV_GET_SKY(adv_light[idx]) >= sky - 1) continue;

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

		if (ADV_GET_SKY(adv_light[idx]) >= cur.level) continue;
		if (cur.level == 0) continue;

		ADV_SET_SKY(&adv_light[idx], cur.level);
		if (cur.level <= 1) continue;

		for (d = 0; d < 6; d++) {
			nx = cur.x + adv_dirs[d][0];
			ny = cur.y + adv_dirs[d][1];
			nz = cur.z + adv_dirs[d][2];
			if (!World_Contains(nx, ny, nz)) continue;

			idx = World_Pack(nx, ny, nz);
			if (ADV_GET_SKY(adv_light[idx]) >= cur.level - 1) continue;

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
				ADV_SET_BLK(&adv_light[World_Pack(x, y, z)], 0);

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
				blk = ADV_GET_BLK(adv_light[idx]);
				if (blk <= 1) continue;

				for (d = 0; d < 6; d++) {
					nx = x + adv_dirs[d][0];
					ny = y + adv_dirs[d][1];
					nz = z + adv_dirs[d][2];
					if (!World_Contains(nx, ny, nz)) continue;
					if (nx < minX || nx > maxX || ny < minY || ny > maxY || nz < minZ || nz > maxZ) continue;

					idx = World_Pack(nx, ny, nz);
					if (ADV_GET_BLK(adv_light[idx]) >= blk - 1) continue;

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

		if (ADV_GET_BLK(adv_light[idx]) >= cur.level) continue;
		if (cur.level == 0) continue;

		ADV_SET_BLK(&adv_light[idx], cur.level);
		if (cur.level <= 1) continue;

		for (d = 0; d < 6; d++) {
			nx = cur.x + adv_dirs[d][0];
			ny = cur.y + adv_dirs[d][1];
			nz = cur.z + adv_dirs[d][2];
			if (!World_Contains(nx, ny, nz)) continue;

			idx = World_Pack(nx, ny, nz);
			if (ADV_GET_BLK(adv_light[idx]) >= cur.level - 1) continue;

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
	if (adv_light && lightPassChanged) {
		Adv_UpdateSkyArea(x, y, z);
	}

	/* Update block light if torch/lava placed/removed or light passage changed */
	if (adv_light) {
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

	Mem_Free(adv_light);
	adv_light = NULL;
	Queue_Clear(&adv_queue);
}

static void Adv_AllocState(void) {
	ClassicLighting_AllocState();
	Adv_InitAllPalettes();

	Queue_Init(&adv_queue, sizeof(struct AdvLightNode));

	adv_light = (cc_uint8*)Mem_TryAlloc(World.Volume, 1);

	if (!adv_light) {
		return;
	}

	Mem_Set(adv_light, 0, World.Volume);

	Adv_PropagateSkyLight();
	Adv_PropagateBlockLight();
}

static void Adv_Refresh(void) {
	ClassicLighting_Refresh();
	/* Re-propagate all lighting from scratch */
	if (adv_light) Mem_Set(adv_light, 0, World.Volume);
	if (adv_light) Adv_PropagateSkyLight();
	if (adv_light) Adv_PropagateBlockLight();
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
