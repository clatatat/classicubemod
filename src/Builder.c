#include "Builder.h"
#include "Constants.h"
#include "World.h"
#include "Funcs.h"
#include "Lighting.h"
#include "Platform.h"
#include "MapRenderer.h"
#include "Graphics.h"
#include "Drawer.h"
#include "ExtMath.h"
#include "Block.h"
#include "PackedCol.h"
#include "TexturePack.h"
#include "Game.h"
#include "Options.h"
#include "BlockPhysics.h"

/* Tile locations for flowing liquid procedural animations (from Animations.c) */
#define WATER_FLOW_TEX_LOC_B      207
#define LAVA_FLOW_TEX_LOC_B       208
#define WATER_FLOW_DIAG_TEX_LOC_B 209
#define LAVA_FLOW_DIAG_TEX_LOC_B  210

/* UV rotation table for top face of flowing liquid (4 rotations, 4 vertices each).
   Vertices: v0=(x+1,z), v1=(x,z), v2=(x,z+1), v3=(x+1,z+1)
   Each entry is {U_scale, V_scale} mapping to [0..UV2_Scale] x [vOrigin..v_bot].
   Rotation 0: default (+Z flow), 1: 90 CW (+X), 2: 180 (-Z), 3: 270 CCW (-X) */
static const float flowUV[4][4][2] = {
	{{1,0},{0,0},{0,1},{1,1}},
	{{0,0},{0,1},{1,1},{1,0}},
	{{0,1},{1,1},{1,0},{0,0}},
	{{1,1},{1,0},{0,0},{0,1}}
};

int Builder_SidesLevel, Builder_EdgeLevel;
/* Packs an index into the 16x16x16 count array. Coordinates range from 0 to 15. */
#define Builder_PackCount(xx, yy, zz) ((((yy) << 8) | ((zz) << 4) | (xx)) * FACE_COUNT)
/* Packs an index into the 18x18x18 chunk array. Coordinates range from -1 to 16. */
#define Builder_PackChunk(xx, yy, zz) (((yy) + 1) * EXTCHUNK_SIZE_2 + ((zz) + 1) * EXTCHUNK_SIZE + ((xx) + 1))

#define IsLeverBlock(b) ((b) == BLOCK_LEVER || (b) == BLOCK_LEVER_ON)
#define IsCropBlock(b)  ((b) >= BLOCK_WHEAT_0 && (b) <= BLOCK_WHEAT_7)
#define IsRailBlock(b)  ((b) == BLOCK_RAIL || ((b) >= BLOCK_RAIL_EW && (b) <= BLOCK_RAIL_CURVE_NE))
#if defined EXTENDED_BLOCKS
#define IsStairBlock(b) ((b) == BLOCK_WOOD_STAIRS || (b) == BLOCK_COBBLE_STAIRS || ((b) >= BLOCK_WOOD_STAIRS_0 && (b) <= BLOCK_COBBLE_STAIRS_3))
#else
#define IsStairBlock(b) ((b) == BLOCK_WOOD_STAIRS || (b) == BLOCK_COBBLE_STAIRS)
#endif

/* Non-sprite detail blocks that can be skipped in far chunks */
#define IsDetailBlock(b) ( \
	(b) == BLOCK_RED_ORE_DUST || (b) == BLOCK_LIT_RED_ORE_DUST || \
	(b) == BLOCK_LEVER || (b) == BLOCK_LEVER_ON || \
	(b) == BLOCK_BUTTON || (b) == BLOCK_BUTTON_PRESSED || \
	(b) == BLOCK_PRESSURE_PLATE || (b) == BLOCK_PRESSURE_PLATE_PRESSED || \
	(b) == BLOCK_STONE_PLATE || (b) == BLOCK_STONE_PLATE_PRESSED || \
	(b) == BLOCK_LADDER || \
	(b) == BLOCK_SIGN_WALL || (b) == BLOCK_SIGN_FLOOR)

cc_bool Builder_SkipDetailBlocks;

static BlockID* Builder_Chunk;
static cc_uint8* Builder_Counts;
static int* Builder_BitFlags;
static int Builder_X, Builder_Y, Builder_Z;
static BlockID Builder_Block;
static int Builder_ChunkIndex;
static cc_bool Builder_FullBright;
static int Builder_ChunkEndX, Builder_ChunkEndZ;
static int Builder_Offsets[FACE_COUNT] = { -1,1, -EXTCHUNK_SIZE,EXTCHUNK_SIZE, -EXTCHUNK_SIZE_2,EXTCHUNK_SIZE_2 };

static int (*Builder_StretchXLiquid)(int countIndex, int x, int y, int z, int chunkIndex, BlockID block);
static int (*Builder_StretchX)(int countIndex, int x, int y, int z, int chunkIndex, BlockID block, Face face);
static int (*Builder_StretchZ)(int countIndex, int x, int y, int z, int chunkIndex, BlockID block, Face face);
static void (*Builder_RenderBlock)(int countsIndex, int x, int y, int z);
static void (*Builder_PrePrepareChunk)(void);
static void (*Builder_PostPrepareChunk)(void);

/* Contains state for vertices for a portion of a chunk mesh (vertices that are in a 1D atlas) */
struct Builder1DPart {
	/* Union to save on memory, since chunk building is divided into counting then building phases */
	union FaceData {
		struct VertexTextured* vertices[FACE_COUNT];
		int count[FACE_COUNT];
	} faces;
	int sCount, sOffset;
};

/* Part builder data, for both normal and translucent parts.
The first ATLAS1D_MAX_ATLASES parts are for normal parts, remainder are for translucent parts. */
static CC_BIG_VAR struct Builder1DPart Builder_Parts[ATLAS1D_MAX_ATLASES * 2];
static struct VertexTextured* Builder_Vertices;

static int Builder1DPart_VerticesCount(struct Builder1DPart* part) {
	int i, count = part->sCount;
	for (i = 0; i < FACE_COUNT; i++) { count += part->faces.count[i]; }
	return count;
}

static int Builder1DPart_CalcOffsets(struct Builder1DPart* part, int offset) {
	int i, counts[FACE_COUNT];
	part->sOffset = offset;

	/* Have to copy first due to count and vertices fields being a union */
	for (i = 0; i < FACE_COUNT; i++)
	{
		counts[i] = part->faces.count[i];
	}

	offset += part->sCount;
	for (i = 0; i < FACE_COUNT; i++) 
	{
		part->faces.vertices[i] = &Builder_Vertices[offset];
		offset += counts[i];
	}
	return offset;
}

static int Builder_TotalVerticesCount(void) {
	int i, count = 0;
	for (i = 0; i < ATLAS1D_MAX_ATLASES * 2; i++) {
		count += Builder1DPart_VerticesCount(&Builder_Parts[i]);
	}
	return count;
}


/*########################################################################################################################*
*----------------------------------------------------Base mesh builder----------------------------------------------------*
*#########################################################################################################################*/
static void AddSpriteVertices(BlockID block) {
	int i = Atlas1D_Index(Block_Tex(block, FACE_XMAX));
	struct Builder1DPart* part = &Builder_Parts[i];
	part->sCount += 4 * 4;
}

static void AddCropSpriteVertices(BlockID block) {
	int i = Atlas1D_Index(Block_Tex(block, FACE_XMAX));
	struct Builder1DPart* part = &Builder_Parts[i];
	part->sCount += 8 * 4;
}

static void AddVertices(BlockID block, Face face) {
	int baseOffset = (Blocks.Draw[block] == DRAW_TRANSLUCENT) * ATLAS1D_MAX_ATLASES;
	int i = Atlas1D_Index(Block_Tex(block, face));
	struct Builder1DPart* part = &Builder_Parts[baseOffset + i];
	part->faces.count[face] += 4;
}

/* Like AddVertices, but for finite liquid blocks:
   non-source blocks use flowing animation tiles; top face may use diagonal tile */
static void AddVerticesFiniteLiquid(BlockID block, Face face) {
	int baseOffset = (Blocks.Draw[block] == DRAW_TRANSLUCENT) * ATLAS1D_MAX_ATLASES;
	TextureLoc loc = Block_Tex(block, face);
	int i;
	struct Builder1DPart* part;

	/* Non-bottom faces of flowing liquid use the flowing animation tile */
	if (face != FACE_YMIN) {
		int packIdx  = World_Pack(Builder_X, Builder_Y, Builder_Z);
		int rawLevel = Physics.FlowLevels[packIdx];
		int level    = rawLevel & 0x7F;
		if (level > 0) {
			cc_bool isWater = (block == BLOCK_WATER || block == BLOCK_STILL_WATER);
			loc = isWater ? WATER_FLOW_TEX_LOC_B : LAVA_FLOW_TEX_LOC_B;

			/* Top face might use the diagonal tile depending on flow direction */
			if (face == FACE_YMAX) {
				Vec3 flow;
				float ax, az;
				Physics_GetFlowVector(Builder_X, Builder_Y, Builder_Z, isWater, &flow);
				ax = Math_AbsF(flow.x);
				az = Math_AbsF(flow.z);
				/* Diagonal when neither axis is dominant (2:1 ratio threshold) */
				if (ax >= 0.001f && az >= 0.001f && ax <= az * 2.0f && az <= ax * 2.0f) {
					loc = isWater ? WATER_FLOW_DIAG_TEX_LOC_B : LAVA_FLOW_DIAG_TEX_LOC_B;
				}
			}
		}
	}

	i    = Atlas1D_Index(loc);
	part = &Builder_Parts[baseOffset + i];
	part->faces.count[face] += 4;
}

#if CC_GFX_BACKEND == CC_GFX_BACKEND_GL11
static void BuildPartVbs(struct ChunkPartInfo* info) {
	/* Sprites vertices are stored before chunk face sides */
	int i, count, offset = info->offset + info->spriteCount;
	for (i = 0; i < FACE_COUNT; i++) {
		count = info->counts[i];

		if (count) {
			info->vbs[i] = Gfx_CreateVb2(&Builder_Vertices[offset], VERTEX_FORMAT_TEXTURED, count);
			offset += count;
		} else {
			info->vbs[i] = 0;
		}
	}

	count  = info->spriteCount;
	offset = info->offset;
	if (count) {
		info->vbs[i] = Gfx_CreateVb2(&Builder_Vertices[offset], VERTEX_FORMAT_TEXTURED, count);
	} else {
		info->vbs[i] = 0;
	}
}
#endif

static cc_bool SetPartInfo(struct Builder1DPart* part, int* offset, struct ChunkPartInfo* info) {
	int vCount = Builder1DPart_VerticesCount(part);
	info->offset = -1;
	if (!vCount) return false;

	info->offset = *offset;
	*offset += vCount;

	info->counts[FACE_XMIN] = part->faces.count[FACE_XMIN];
	info->counts[FACE_XMAX] = part->faces.count[FACE_XMAX];
	info->counts[FACE_ZMIN] = part->faces.count[FACE_ZMIN];
	info->counts[FACE_ZMAX] = part->faces.count[FACE_ZMAX];
	info->counts[FACE_YMIN] = part->faces.count[FACE_YMIN];
	info->counts[FACE_YMAX] = part->faces.count[FACE_YMAX];
	info->spriteCount       = part->sCount;
	return true;
}


static void PrepareChunk(int x1, int y1, int z1) {
	int xMax = min(World.Width,  x1 + CHUNK_SIZE);
	int yMax = min(World.Height, y1 + CHUNK_SIZE);
	int zMax = min(World.Length, z1 + CHUNK_SIZE);

	int cIndex, index, tileIdx;
	BlockID b;
	int x, y, z, xx, yy, zz;

#ifdef OCCLUSION
	int flags = ComputeOcclusion();
#endif
#ifdef DEBUG_OCCLUSION
	FastColour col = new FastColour(60, 60, 60, 255);
	if (flags & 1) col.R = 255; /* x */
	if (flags & 4) col.G = 255; /* y */
	if (flags & 2) col.B = 255; /* z */
	map.Sunlight = map.Shadowlight = col;
	map.SunlightXSide = map.ShadowlightXSide = col;
	map.SunlightZSide = map.ShadowlightZSide = col;
	map.SunlightYBottom = map.ShadowlightYBottom = col;
#endif
	
	for (y = y1, yy = 0; y < yMax; y++, yy++) {
		for (z = z1, zz = 0; z < zMax; z++, zz++) {
			cIndex = Builder_PackChunk(0, yy, zz);

			for (x = x1, xx = 0; x < xMax; x++, xx++, cIndex++) {
				b = Builder_Chunk[cIndex];
				if (Blocks.Draw[b] == DRAW_GAS) continue;
				index = Builder_PackCount(xx, yy, zz);

				/* Skip detail blocks in far chunks - zero counts to prevent stale 1s */
				if (Builder_SkipDetailBlocks && IsDetailBlock(b)) {
					Builder_Counts[index]     = 0;
					Builder_Counts[index + 1] = 0;
					Builder_Counts[index + 2] = 0;
					Builder_Counts[index + 3] = 0;
					Builder_Counts[index + 4] = 0;
					Builder_Counts[index + 5] = 0;
					continue;
				}

				/* Sprites can't be stretched, nor can then be they hidden by other blocks. */
				/* Note sprites are drawn using DrawSprite and not with any of the DrawXFace. */
				if (Blocks.Draw[b] == DRAW_SPRITE) {
					if (IsCropBlock(b)) { AddCropSpriteVertices(b); } else { AddSpriteVertices(b); }
					continue;
				}
				
				/* Rails only use YMAX face (drawn via Builder_DrawRail).
				   Zero all other face counts to prevent uninitialized vertex data.
				   counts buffer is pre-filled with 1, so we must explicitly zero them. */
				if (IsRailBlock(b)) {
					Builder_Counts[index + FACE_XMIN] = 0;
					Builder_Counts[index + FACE_XMAX] = 0;
					Builder_Counts[index + FACE_ZMIN] = 0;
					Builder_Counts[index + FACE_ZMAX] = 0;
					Builder_Counts[index + FACE_YMIN] = 0;
					AddVertices(b, FACE_YMAX);
					continue;
				}
				
				/* Stairs use custom rendering (two sub-blocks per stair).
				   Zero all face counts and add 2x vertices per face for both sub-blocks. */
				if (IsStairBlock(b)) {
					Builder_Counts[index + FACE_XMIN] = 0;
					Builder_Counts[index + FACE_XMAX] = 0;
					Builder_Counts[index + FACE_ZMIN] = 0;
					Builder_Counts[index + FACE_ZMAX] = 0;
					Builder_Counts[index + FACE_YMIN] = 0;
					Builder_Counts[index + FACE_YMAX] = 0;
					AddVertices(b, FACE_XMIN); AddVertices(b, FACE_XMIN);
					AddVertices(b, FACE_XMAX); AddVertices(b, FACE_XMAX);
					AddVertices(b, FACE_ZMIN); AddVertices(b, FACE_ZMIN);
					AddVertices(b, FACE_ZMAX); AddVertices(b, FACE_ZMAX);
					AddVertices(b, FACE_YMIN); AddVertices(b, FACE_YMIN);
					AddVertices(b, FACE_YMAX); AddVertices(b, FACE_YMAX);
					continue;
				}
				
				/* Lever blocks (DRAW_TRANSPARENT) also need sprite verts for handle */
				if (IsLeverBlock(b)) {
					int k = Atlas1D_Index(96);
					Builder_Parts[k].sCount += 4 * 4;
				}

				Builder_X = x; Builder_Y = y; Builder_Z = z;
				Builder_FullBright = Blocks.Brightness[b];
				tileIdx = b * BLOCK_COUNT;
				/* All of these function calls are inlined as they can be called tens of millions to hundreds of millions of times. */

				if (Builder_Counts[index] == 0 ||
					(x == 0 && (y < Builder_SidesLevel || (b >= BLOCK_WATER && b <= BLOCK_STILL_LAVA && y < Builder_EdgeLevel))) ||
					(x != 0 && (Blocks.Hidden[tileIdx + Builder_Chunk[cIndex - 1]] & FACE_BIT_XMIN) != 0)) {
					Builder_Counts[index] = 0;
				} else {
					Builder_Counts[index] = Builder_StretchZ(index, x, y, z, cIndex, b, FACE_XMIN);
				}

				index++;
				if (Builder_Counts[index] == 0 ||
					(x == World.MaxX && (y < Builder_SidesLevel || (b >= BLOCK_WATER && b <= BLOCK_STILL_LAVA && y < Builder_EdgeLevel))) ||
					(x != World.MaxX && (Blocks.Hidden[tileIdx + Builder_Chunk[cIndex + 1]] & FACE_BIT_XMAX) != 0)) {
					Builder_Counts[index] = 0;
				} else {
					Builder_Counts[index] = Builder_StretchZ(index, x, y, z, cIndex, b, FACE_XMAX);
				}

				index++;
				if (Builder_Counts[index] == 0 ||
					(z == 0 && (y < Builder_SidesLevel || (b >= BLOCK_WATER && b <= BLOCK_STILL_LAVA && y < Builder_EdgeLevel))) ||
					(z != 0 && (Blocks.Hidden[tileIdx + Builder_Chunk[cIndex - EXTCHUNK_SIZE]] & FACE_BIT_ZMIN) != 0)) {
					Builder_Counts[index] = 0;
				} else {
					Builder_Counts[index] = Builder_StretchX(index, x, y, z, cIndex, b, FACE_ZMIN);
				}

				index++;
				if (Builder_Counts[index] == 0 ||
					(z == World.MaxZ && (y < Builder_SidesLevel || (b >= BLOCK_WATER && b <= BLOCK_STILL_LAVA && y < Builder_EdgeLevel))) ||
					(z != World.MaxZ && (Blocks.Hidden[tileIdx + Builder_Chunk[cIndex + EXTCHUNK_SIZE]] & FACE_BIT_ZMAX) != 0)) {
					Builder_Counts[index] = 0;
				} else {
					Builder_Counts[index] = Builder_StretchX(index, x, y, z, cIndex, b, FACE_ZMAX);
				}

				index++;
				if (Builder_Counts[index] == 0 || y == 0 ||
					(Blocks.Hidden[tileIdx + Builder_Chunk[cIndex - EXTCHUNK_SIZE_2]] & FACE_BIT_YMIN) != 0) {
					Builder_Counts[index] = 0;
				} else {
					Builder_Counts[index] = Builder_StretchX(index, x, y, z, cIndex, b, FACE_YMIN);
				}

				index++;
				if (Builder_Counts[index] == 0 ||
					(Blocks.Hidden[tileIdx + Builder_Chunk[cIndex + EXTCHUNK_SIZE_2]] & FACE_BIT_YMAX) != 0) {
					Builder_Counts[index] = 0;
				} else if (b < BLOCK_WATER || b > BLOCK_STILL_LAVA) {
					Builder_Counts[index] = Builder_StretchX(index, x, y, z, cIndex, b, FACE_YMAX);
				} else {
					Builder_Counts[index] = Builder_StretchXLiquid(index, x, y, z, cIndex, b);
				}
			}
		}
	}
}

#define ReadChunkBody(get_block)\
for (yy = -1; yy < 17; ++yy) {\
	y = yy + y1;\
	for (zz = -1; zz < 17; ++zz) {\
\
		index  = World_Pack(x1 - 1, y, z1 + zz);\
		cIndex = Builder_PackChunk(-1, yy, zz);\
		for (xx = -1; xx < 17; ++xx, ++index, ++cIndex) {\
\
			block    = get_block;\
			allAir   = allAir   && Blocks.Draw[block] == DRAW_GAS;\
			allSolid = allSolid && Blocks.FullOpaque[block];\
			Builder_Chunk[cIndex] = block;\
		}\
	}\
}

static cc_bool ReadChunkData(int x1, int y1, int z1, cc_bool* outAllAir) {
	BlockRaw* blocks = World.Blocks;
	cc_bool allAir = true, allSolid = true;
	int index, cIndex;
	BlockID block;
	int xx, yy, zz, y;

#ifndef EXTENDED_BLOCKS
	ReadChunkBody(blocks[index]);
#else
	BlockRaw* blocks2;

	if (World.IDMask <= 0xFF) {
		ReadChunkBody(blocks[index]);
	} else {
		blocks2 = World.Blocks2;
		ReadChunkBody(blocks[index] | (blocks2[index] << 8));
	}
#endif

	*outAllAir = allAir;
	return allSolid;
}

#define ReadBorderChunkBody(get_block)\
for (yy = -1; yy < 17; ++yy) {\
	y = yy + y1;\
	if (y < 0) continue;\
	if (y >= World.Height) break;\
\
	for (zz = -1; zz < 17; ++zz) {\
		z = zz + z1;\
		if (z < 0) continue;\
		if (z >= World.Length) break;\
\
		index  = World_Pack(x1 - 1, y, z);\
		cIndex = Builder_PackChunk(-1, yy, zz);\
\
		for (xx = -1; xx < 17; ++xx, ++index, ++cIndex) {\
			x = xx + x1;\
			if (x < 0) continue;\
			if (x >= World.Width) break;\
\
			block  = get_block;\
			allAir = allAir && Blocks.Draw[block] == DRAW_GAS;\
			Builder_Chunk[cIndex] = block;\
		}\
	}\
}

static cc_bool ReadBorderChunkData(int x1, int y1, int z1, cc_bool* outAllAir) {
	BlockRaw* blocks = World.Blocks;
	BlockRaw* blocks2;
	cc_bool allAir = true;
	int index, cIndex;
	BlockID block;
	int xx, yy, zz, x, y, z;

#ifndef EXTENDED_BLOCKS
	ReadBorderChunkBody(blocks[index]);
#else
	if (World.IDMask <= 0xFF) {
		ReadBorderChunkBody(blocks[index]);
	} else {
		blocks2 = World.Blocks2;
		ReadBorderChunkBody(blocks[index] | (blocks2[index] << 8));
	}
#endif

	*outAllAir = allAir;
	return false;
}

static void OutputChunkPartsMeta(int x, int y, int z, struct ChunkInfo* info) {
	cc_bool hasNorm, hasTran;
	int partsIndex;
	int i, j, curIdx, offset;
	
	partsIndex = World_ChunkPack(x >> CHUNK_SHIFT, y >> CHUNK_SHIFT, z >> CHUNK_SHIFT);
	offset  = 0;
	hasNorm = false;
	hasTran = false;

	for (i = 0; i < MapRenderer_1DUsedCount; i++) {
		j = i + ATLAS1D_MAX_ATLASES;
		curIdx = partsIndex + i * World.ChunksCount;

		hasNorm |= SetPartInfo(&Builder_Parts[i], &offset, &MapRenderer_PartsNormal[curIdx]);
		hasTran |= SetPartInfo(&Builder_Parts[j], &offset, &MapRenderer_PartsTranslucent[curIdx]);
	}

	if (hasNorm) {
		info->normalParts      = &MapRenderer_PartsNormal[partsIndex];
	}
	if (hasTran) {
		info->translucentParts = &MapRenderer_PartsTranslucent[partsIndex];
	}
}

void Builder_MakeChunk(struct ChunkInfo* info) {
#if CC_BUILD_MAXSTACK <= (32 * 1024)
	void* mem        = TempMem_Alloc((EXTCHUNK_SIZE_3 * sizeof(BlockID)) + (CHUNK_SIZE_3 * FACE_COUNT));
	BlockID* chunk   = (BlockID*)mem;
	cc_uint8* counts = (cc_uint8*)(chunk + EXTCHUNK_SIZE_3);
#else
	BlockID chunk[EXTCHUNK_SIZE_3]; 
	cc_uint8 counts[CHUNK_SIZE_3 * FACE_COUNT]; 
#endif

#ifdef CC_BUILD_ADVLIGHTING
	int bitFlags[EXTCHUNK_SIZE_3];
#else
	int bitFlags[1];
#endif

	cc_bool allAir, allSolid, onBorder;
	int xMax, yMax, zMax, totalVerts;
	int cIndex, index;
	int x, y, z, xx, yy, zz;
	int x1 = info->centreX - 8, y1 = info->centreY - 8, z1 = info->centreZ - 8;

	Builder_Chunk  = chunk;
	Builder_Counts = counts;
	Builder_BitFlags = bitFlags;
	Builder_PrePrepareChunk();
	
	onBorder = 
		x1 == 0 || y1 == 0 || z1 == 0   || x1 + CHUNK_SIZE >= World.Width ||
		y1 + CHUNK_SIZE >= World.Height || z1 + CHUNK_SIZE >= World.Length;

	if (onBorder) {
		/* less optimal case here */
		Mem_Set(chunk, BLOCK_AIR, EXTCHUNK_SIZE_3 * sizeof(BlockID));
		allSolid = ReadBorderChunkData(x1, y1, z1, &allAir);
	} else {
		allSolid = ReadChunkData(x1, y1, z1, &allAir);
	}

	info->allAir = allAir;
	if (allAir) {
		info->occlusionFlags = 0x3F; /* All faces can see through */
		return;
	}
	if (allSolid) {
		info->occlusionFlags = 0; /* No faces can see through */
		return;
	}
	/* Mixed chunk - conservatively assume all faces can see through */
	info->occlusionFlags = 0x3F;
	Lighting.LightHint(x1 - 1, y1 - 1, z1 - 1);

	Mem_Set(counts, 1, CHUNK_SIZE_3 * FACE_COUNT);
	xMax = min(World.Width,  x1 + CHUNK_SIZE);
	yMax = min(World.Height, y1 + CHUNK_SIZE);
	zMax = min(World.Length, z1 + CHUNK_SIZE);

	Builder_ChunkEndX = xMax; Builder_ChunkEndZ = zMax;
	PrepareChunk(x1, y1, z1);

	totalVerts = Builder_TotalVerticesCount();
	if (!totalVerts) return;
	
	OutputChunkPartsMeta(x1, y1, z1, info);

#if CC_GFX_BACKEND != CC_GFX_BACKEND_GL11
	/* add an extra element to fix crashing on some GPUs */
	info->vb = Gfx_CreateVb(VERTEX_FORMAT_TEXTURED, totalVerts + 1);
	Builder_Vertices = (struct VertexTextured*)Gfx_LockVb(info->vb,
													VERTEX_FORMAT_TEXTURED, totalVerts + 1);
#else
	/* NOTE: Relies on assumption vb is ignored by GL11 Gfx_LockVb implementation */
	Builder_Vertices = (struct VertexTextured*)Gfx_LockVb(0, 
													VERTEX_FORMAT_TEXTURED, totalVerts + 1);
#endif
	Builder_PostPrepareChunk();
	/* now render the chunk */

	for (y = y1, yy = 0; y < yMax; y++, yy++) {
		for (z = z1, zz = 0; z < zMax; z++, zz++) {
			cIndex = Builder_PackChunk(0, yy, zz);

			for (x = x1, xx = 0; x < xMax; x++, xx++, cIndex++) {
				Builder_Block = chunk[cIndex];
				if (Blocks.Draw[Builder_Block] == DRAW_GAS) continue;
				if (Builder_SkipDetailBlocks && IsDetailBlock(Builder_Block)) continue;

				index = Builder_PackCount(xx, yy, zz);
				Builder_ChunkIndex = cIndex;
				Builder_RenderBlock(index, x, y, z);
			}
		}
	}

#if CC_GFX_BACKEND == CC_GFX_BACKEND_GL11
	cIndex = World_ChunkPack(x1 >> CHUNK_SHIFT, y1 >> CHUNK_SHIFT, z1 >> CHUNK_SHIFT);

	for (index = 0; index < MapRenderer_1DUsedCount; index++) {
		int curIdx = cIndex + index * World.ChunksCount;

		BuildPartVbs(&MapRenderer_PartsNormal[curIdx]);
		BuildPartVbs(&MapRenderer_PartsTranslucent[curIdx]);
	}
#else
	Gfx_UnlockVb(info->vb);
#endif
}

static cc_bool Builder_OccludedLiquid(int chunkIndex) {
	chunkIndex += EXTCHUNK_SIZE_2; /* Checking y above */
	return
		Blocks.FullOpaque[Builder_Chunk[chunkIndex]]
		&& Blocks.Draw[Builder_Chunk[chunkIndex - EXTCHUNK_SIZE]] != DRAW_GAS
		&& Blocks.Draw[Builder_Chunk[chunkIndex - 1]] != DRAW_GAS
		&& Blocks.Draw[Builder_Chunk[chunkIndex + 1]] != DRAW_GAS
		&& Blocks.Draw[Builder_Chunk[chunkIndex + EXTCHUNK_SIZE]] != DRAW_GAS;
}

/*########################################################################################################################*
*---------------------------------------------Finite liquid rendering-----------------------------------------------------*
*#########################################################################################################################*/
/* Check if a block should use slanted finite liquid rendering */
static cc_bool Builder_IsFiniteLiquid(BlockID b) {
	return Physics.FiniteLiquid && Physics.FlowLevels &&
		(b == BLOCK_WATER || b == BLOCK_STILL_WATER ||
		 b == BLOCK_LAVA  || b == BLOCK_STILL_LAVA);
}

/* Compute the liquid surface height at a corner of a block.
   Ports MC Alpha's RenderBlocks.renderBlockFluids corner averaging.
   cx, cz are 0 or 1, indicating which corner of the block at (bx,by,bz). */
static float Builder_LiquidCornerHeight(int bx, int by, int bz, int cx, int cz, cc_bool isWater) {
	float total = 0;
	int count = 0;
	int dx, dz;

	for (dx = cx - 1; dx <= cx; dx++) {
		for (dz = cz - 1; dz <= cz; dz++) {
			int nx = bx + dx, nz = bz + dz;
			BlockID nb;
			cc_bool sameLiquid;

			/* If any neighbor has same liquid above, corner is at full height */
			if (World_Contains(nx, by + 1, nz)) {
				nb = World_GetBlock(nx, by + 1, nz);
				sameLiquid = isWater ? (nb == BLOCK_WATER || nb == BLOCK_STILL_WATER)
				                     : (nb == BLOCK_LAVA  || nb == BLOCK_STILL_LAVA);
				if (sameLiquid) return 1.0f;
			}

			if (!World_Contains(nx, by, nz)) continue;
			nb = World_GetBlock(nx, by, nz);
			sameLiquid = isWater ? (nb == BLOCK_WATER || nb == BLOCK_STILL_WATER)
			                     : (nb == BLOCK_LAVA  || nb == BLOCK_STILL_LAVA);

			if (sameLiquid) {
				int rawLevel = Physics.FlowLevels[World_Pack(nx, by, nz)];
				int effLevel = (rawLevel >= 8) ? 0 : rawLevel;
				float pct = (float)(effLevel + 1) / 9.0f;
				/* Source/falling blocks get extra weight (MC Alpha *10 weighting) */
				if (rawLevel >= 8 || rawLevel == 0) {
					total += (1.0f / 9.0f) * 10.0f;
					count += 10;
				}
				total += pct * 10.0f;
				count += 10;
			} else if (Blocks.Collide[nb] < COLLIDE_SOLID) {
				/* Non-solid blocks (air, etc.) contribute to average */
				total += 1.0f;
				count += 1;
			}
			/* Solid blocks don't contribute */
		}
	}

	if (count > 0) return 1.0f - total / (float)count;
	return 0.5f;
}

/* Custom renderer for liquid blocks with per-corner surface heights (MC Alpha style).
   Handles all 6 faces with slanted top surface. */
static void Builder_RenderFiniteLiquid(int index, int x, int y, int z) {
	BlockID block = Builder_Block;
	cc_bool isWater = (block == BLOCK_WATER || block == BLOCK_STILL_WATER);
	int count_XMin, count_XMax, count_ZMin, count_ZMax, count_YMin, count_YMax;
	float h00, h10, h01, h11;
	cc_bool fullBright;
	int baseOffset, lightFlags, offset;
	struct Builder1DPart* part;
	struct VertexTextured* v;
	TextureLoc loc;
	TextureLoc flowLoc;     /* tile for side faces of flowing liquid */
	TextureLoc flowTopLoc;  /* tile for top face (may be diagonal variant) */
	int flowRot;            /* UV rotation index 0-3 for top face */
	cc_bool isFlowing;
	PackedCol col, tintCol;
	float vOrigin, v_top, v_bot;

	count_XMin = Builder_Counts[index + FACE_XMIN];
	count_XMax = Builder_Counts[index + FACE_XMAX];
	count_ZMin = Builder_Counts[index + FACE_ZMIN];
	count_ZMax = Builder_Counts[index + FACE_ZMAX];
	count_YMin = Builder_Counts[index + FACE_YMIN];
	count_YMax = Builder_Counts[index + FACE_YMAX];

	if (!count_XMin && !count_XMax && !count_ZMin &&
		!count_ZMax && !count_YMin && !count_YMax) return;

	/* Determine whether this liquid is flowing (non-source) for texture */
	{
		int packIdx = World_Pack(x, y, z);
		int rawLevel = Physics.FlowLevels[packIdx];
		int level    = rawLevel & 0x7F;
		isFlowing = (level > 0);
		flowLoc    = isWater ? WATER_FLOW_TEX_LOC_B    : LAVA_FLOW_TEX_LOC_B;
		flowTopLoc = flowLoc; /* default: cardinal tile */
		flowRot    = 0;       /* default: no UV rotation */

		/* Snap flow direction to 8 compass points and choose tile + UV rotation:
		   Cardinal (N/E/S/W): use flowLoc with 0/90/180/270 UV rotation
		   Diagonal (NE/SE/SW/NW): use diagonal tile with 0/90/180/270 UV rotation */
		if (isFlowing) {
			Vec3 flow;
			float ax, az;
			TextureLoc diagLoc = isWater ? WATER_FLOW_DIAG_TEX_LOC_B : LAVA_FLOW_DIAG_TEX_LOC_B;
			Physics_GetFlowVector(x, y, z, isWater, &flow);
			ax = Math_AbsF(flow.x);
			az = Math_AbsF(flow.z);

			if (ax < 0.001f && az < 0.001f) {
				/* No horizontal flow — keep defaults */
			} else if (ax > az * 2.0f) {
				/* Mostly X: cardinal E(+X) or W(-X)
				   Rot 3: V=+X (scroll east),  Rot 1: V=-X (scroll west) */
				flowRot = flow.x > 0 ? 3 : 1;
			} else if (az > ax * 2.0f) {
				/* Mostly Z: cardinal S(+Z) or N(-Z)
				   Rot 0: V=+Z (scroll south), Rot 2: V=-Z (scroll north) */
				flowRot = flow.z > 0 ? 0 : 2;
			} else {
				/* Diagonal: use pre-rotated 45 tile.
				   RotatePixels45 turns vertical stripes into NE-SW diagonals (in UV space).
				   Choose rotation so stripe orientation + scroll direction = perceived diagonal motion.
				   SE: NW-SE stripes + east scroll  -> rot 3
				   NE: NE-SW stripes + north scroll -> rot 2
				   NW: NW-SE stripes + west scroll  -> rot 1
				   SW: NE-SW stripes + south scroll -> rot 0 */
				flowTopLoc = diagLoc;
				if (flow.x > 0 && flow.z > 0)       flowRot = 3; /* +X +Z (SE) */
				else if (flow.x > 0 && flow.z <= 0)  flowRot = 2; /* +X -Z (NE) */
				else if (flow.x <= 0 && flow.z <= 0) flowRot = 1; /* -X -Z (NW) */
				else                                  flowRot = 0; /* -X +Z (SW) */
			}
		}
	}

	/* Compute per-corner surface heights */
	h00 = Builder_LiquidCornerHeight(x, y, z, 0, 0, isWater);
	h10 = Builder_LiquidCornerHeight(x, y, z, 1, 0, isWater);
	h01 = Builder_LiquidCornerHeight(x, y, z, 0, 1, isWater);
	h11 = Builder_LiquidCornerHeight(x, y, z, 1, 1, isWater);

	fullBright = Blocks.Brightness[block];
	baseOffset = (Blocks.Draw[block] == DRAW_TRANSLUCENT) * ATLAS1D_MAX_ATLASES;
	lightFlags = Blocks.LightOffset[block];
	tintCol    = Blocks.FogCol[block];

	/* FACE_YMAX - top face with per-corner heights, UV rotated by flow direction */
	if (count_YMax) {
		int fr = flowRot;
		float vRange;

		loc    = isFlowing ? flowTopLoc : Block_Tex(block, FACE_YMAX);
		offset = (lightFlags >> FACE_YMAX) & 1;
		part   = &Builder_Parts[baseOffset + Atlas1D_Index(loc)];
		col    = fullBright ? PACKEDCOL_WHITE : Lighting.Color_YMax_Fast(x, y + offset, z);
		if (Blocks.Tinted[block]) col = PackedCol_Tint(col, tintCol);

		vOrigin = Atlas1D_RowId(loc) * Atlas1D.InvTileSize;
		v_bot   = vOrigin + Atlas1D.InvTileSize * UV2_Scale;
		vRange  = v_bot - vOrigin;
		v       = part->faces.vertices[FACE_YMAX];

		v->x = (float)(x + 1); v->y = y + h10; v->z = (float)z;       v->Col = col;
		v->U = flowUV[fr][0][0] * UV2_Scale;
		v->V = vOrigin + flowUV[fr][0][1] * vRange; v++;
		v->x = (float)x;       v->y = y + h00; v->z = (float)z;       v->Col = col;
		v->U = flowUV[fr][1][0] * UV2_Scale;
		v->V = vOrigin + flowUV[fr][1][1] * vRange; v++;
		v->x = (float)x;       v->y = y + h01; v->z = (float)(z + 1); v->Col = col;
		v->U = flowUV[fr][2][0] * UV2_Scale;
		v->V = vOrigin + flowUV[fr][2][1] * vRange; v++;
		v->x = (float)(x + 1); v->y = y + h11; v->z = (float)(z + 1); v->Col = col;
		v->U = flowUV[fr][3][0] * UV2_Scale;
		v->V = vOrigin + flowUV[fr][3][1] * vRange; v++;
		part->faces.vertices[FACE_YMAX] = v;
	}

	/* FACE_YMIN - bottom face (flat, standard rendering) */
	if (count_YMin) {
		loc    = Block_Tex(block, FACE_YMIN);
		offset = (lightFlags >> FACE_YMIN) & 1;
		part   = &Builder_Parts[baseOffset + Atlas1D_Index(loc)];
		col    = fullBright ? PACKEDCOL_WHITE : Lighting.Color_YMin_Fast(x, y - offset, z);
		if (Blocks.Tinted[block]) col = PackedCol_Tint(col, tintCol);

		vOrigin = Atlas1D_RowId(loc) * Atlas1D.InvTileSize;
		v_bot   = vOrigin + Atlas1D.InvTileSize * UV2_Scale;
		v       = part->faces.vertices[FACE_YMIN];

		v->x = (float)(x + 1); v->y = (float)y; v->z = (float)(z + 1); v->Col = col;
		v->U = UV2_Scale; v->V = v_bot; v++;
		v->x = (float)x;       v->y = (float)y; v->z = (float)(z + 1); v->Col = col;
		v->U = 0;         v->V = v_bot; v++;
		v->x = (float)x;       v->y = (float)y; v->z = (float)z;       v->Col = col;
		v->U = 0;         v->V = vOrigin; v++;
		v->x = (float)(x + 1); v->y = (float)y; v->z = (float)z;       v->Col = col;
		v->U = UV2_Scale; v->V = vOrigin; v++;
		part->faces.vertices[FACE_YMIN] = v;
	}

	/* FACE_XMIN - left face, top edge follows h00 and h01 */
	if (count_XMin) {
		float yTop_z0 = y + h00, yTop_z1 = y + h01;
		loc    = isFlowing ? flowLoc : Block_Tex(block, FACE_XMIN);
		offset = (lightFlags >> FACE_XMIN) & 1;
		part   = &Builder_Parts[baseOffset + Atlas1D_Index(loc)];
		col    = fullBright ? PACKEDCOL_WHITE :
			x >= offset ? Lighting.Color_XSide_Fast(x - offset, y, z) : Env.SunXSide;
		if (Blocks.Tinted[block]) col = PackedCol_Tint(col, tintCol);

		vOrigin = Atlas1D_RowId(loc) * Atlas1D.InvTileSize;
		v_bot   = vOrigin + Atlas1D.InvTileSize * UV2_Scale;
		v       = part->faces.vertices[FACE_XMIN];

		v_top = vOrigin + (1.0f - h01) * Atlas1D.InvTileSize;
		v->x = (float)x; v->y = yTop_z1; v->z = (float)(z + 1); v->Col = col;
		v->U = UV2_Scale; v->V = v_top; v++;
		v_top = vOrigin + (1.0f - h00) * Atlas1D.InvTileSize;
		v->x = (float)x; v->y = yTop_z0; v->z = (float)z; v->Col = col;
		v->U = 0;         v->V = v_top; v++;
		v->x = (float)x; v->y = (float)y; v->z = (float)z; v->Col = col;
		v->U = 0;         v->V = v_bot; v++;
		v->x = (float)x; v->y = (float)y; v->z = (float)(z + 1); v->Col = col;
		v->U = UV2_Scale; v->V = v_bot; v++;
		part->faces.vertices[FACE_XMIN] = v;
	}

	/* FACE_XMAX - right face, top edge follows h10 and h11 */
	if (count_XMax) {
		float yTop_z0 = y + h10, yTop_z1 = y + h11;
		loc    = isFlowing ? flowLoc : Block_Tex(block, FACE_XMAX);
		offset = (lightFlags >> FACE_XMAX) & 1;
		part   = &Builder_Parts[baseOffset + Atlas1D_Index(loc)];
		col    = fullBright ? PACKEDCOL_WHITE :
			x <= (World.MaxX - offset) ? Lighting.Color_XSide_Fast(x + offset, y, z) : Env.SunXSide;
		if (Blocks.Tinted[block]) col = PackedCol_Tint(col, tintCol);

		vOrigin = Atlas1D_RowId(loc) * Atlas1D.InvTileSize;
		v_bot   = vOrigin + Atlas1D.InvTileSize * UV2_Scale;
		v       = part->faces.vertices[FACE_XMAX];

		v_top = vOrigin + (1.0f - h10) * Atlas1D.InvTileSize;
		v->x = (float)(x + 1); v->y = yTop_z0; v->z = (float)z; v->Col = col;
		v->U = 1.0f;     v->V = v_top; v++;
		v_top = vOrigin + (1.0f - h11) * Atlas1D.InvTileSize;
		v->x = (float)(x + 1); v->y = yTop_z1; v->z = (float)(z + 1); v->Col = col;
		v->U = 0;         v->V = v_top; v++;
		v->x = (float)(x + 1); v->y = (float)y; v->z = (float)(z + 1); v->Col = col;
		v->U = 0;         v->V = v_bot; v++;
		v->x = (float)(x + 1); v->y = (float)y; v->z = (float)z; v->Col = col;
		v->U = 1.0f;     v->V = v_bot; v++;
		part->faces.vertices[FACE_XMAX] = v;
	}

	/* FACE_ZMIN - front face, top edge follows h00 and h10 */
	if (count_ZMin) {
		float yTop_x0 = y + h00, yTop_x1 = y + h10;
		loc    = isFlowing ? flowLoc : Block_Tex(block, FACE_ZMIN);
		offset = (lightFlags >> FACE_ZMIN) & 1;
		part   = &Builder_Parts[baseOffset + Atlas1D_Index(loc)];
		col    = fullBright ? PACKEDCOL_WHITE :
			z >= offset ? Lighting.Color_ZSide_Fast(x, y, z - offset) : Env.SunZSide;
		if (Blocks.Tinted[block]) col = PackedCol_Tint(col, tintCol);

		vOrigin = Atlas1D_RowId(loc) * Atlas1D.InvTileSize;
		v_bot   = vOrigin + Atlas1D.InvTileSize * UV2_Scale;
		v       = part->faces.vertices[FACE_ZMIN];

		v->x = (float)(x + 1); v->y = (float)y; v->z = (float)z; v->Col = col;
		v->U = 0;         v->V = v_bot; v++;
		v->x = (float)x;       v->y = (float)y; v->z = (float)z; v->Col = col;
		v->U = 1.0f;     v->V = v_bot; v++;
		v_top = vOrigin + (1.0f - h00) * Atlas1D.InvTileSize;
		v->x = (float)x;       v->y = yTop_x0; v->z = (float)z; v->Col = col;
		v->U = 1.0f;     v->V = v_top; v++;
		v_top = vOrigin + (1.0f - h10) * Atlas1D.InvTileSize;
		v->x = (float)(x + 1); v->y = yTop_x1; v->z = (float)z; v->Col = col;
		v->U = 0;         v->V = v_top; v++;
		part->faces.vertices[FACE_ZMIN] = v;
	}

	/* FACE_ZMAX - back face, top edge follows h01 and h11 */
	if (count_ZMax) {
		float yTop_x0 = y + h01, yTop_x1 = y + h11;
		loc    = isFlowing ? flowLoc : Block_Tex(block, FACE_ZMAX);
		offset = (lightFlags >> FACE_ZMAX) & 1;
		part   = &Builder_Parts[baseOffset + Atlas1D_Index(loc)];
		col    = fullBright ? PACKEDCOL_WHITE :
			z <= (World.MaxZ - offset) ? Lighting.Color_ZSide_Fast(x, y, z + offset) : Env.SunZSide;
		if (Blocks.Tinted[block]) col = PackedCol_Tint(col, tintCol);

		vOrigin = Atlas1D_RowId(loc) * Atlas1D.InvTileSize;
		v_bot   = vOrigin + Atlas1D.InvTileSize * UV2_Scale;
		v       = part->faces.vertices[FACE_ZMAX];

		v_top = vOrigin + (1.0f - h11) * Atlas1D.InvTileSize;
		v->x = (float)(x + 1); v->y = yTop_x1; v->z = (float)(z + 1); v->Col = col;
		v->U = UV2_Scale; v->V = v_top; v++;
		v_top = vOrigin + (1.0f - h01) * Atlas1D.InvTileSize;
		v->x = (float)x;       v->y = yTop_x0; v->z = (float)(z + 1); v->Col = col;
		v->U = 0;         v->V = v_top; v++;
		v->x = (float)x;       v->y = (float)y; v->z = (float)(z + 1); v->Col = col;
		v->U = 0;         v->V = v_bot; v++;
		v->x = (float)(x + 1); v->y = (float)y; v->z = (float)(z + 1); v->Col = col;
		v->U = UV2_Scale; v->V = v_bot; v++;
		part->faces.vertices[FACE_ZMAX] = v;
	}
}

static void DefaultPrePrepateChunk(void) {
	Mem_Set(Builder_Parts, 0, sizeof(Builder_Parts));
}

static void DefaultPostStretchChunk(void) {
	int i, j, offset;
	offset = 0;
	for (i = 0; i < ATLAS1D_MAX_ATLASES; i++) {
		j = i + ATLAS1D_MAX_ATLASES;

		offset = Builder1DPart_CalcOffsets(&Builder_Parts[i], offset);
		offset = Builder1DPart_CalcOffsets(&Builder_Parts[j], offset);
	}
}

static RNGState spriteRng;

#define s_u1 0.0f
#define s_u2 UV2_Scale

/* Draw a torch with proper orientation (ground standing or wall-mounted tilted) */
/* Helper: check if Builder_Block is any redstone torch variant */
#define IsRedTorch(b) ((b) == BLOCK_RED_ORE_TORCH || (b) == BLOCK_RED_ORE_TORCH_OFF \
	|| (b) == BLOCK_RED_TORCH_ON_S || (b) == BLOCK_RED_TORCH_ON_N \
	|| (b) == BLOCK_RED_TORCH_ON_E || (b) == BLOCK_RED_TORCH_ON_W \
	|| (b) == BLOCK_RED_TORCH_OFF_S || (b) == BLOCK_RED_TORCH_OFF_N \
	|| (b) == BLOCK_RED_TORCH_OFF_E || (b) == BLOCK_RED_TORCH_OFF_W \
	|| (b) == BLOCK_RED_TORCH_UNMOUNTED || (b) == BLOCK_RED_TORCH_UNMOUNTED_OFF)
#define IsAnyTorchBlock(b) ((b) == BLOCK_TORCH || IsRedTorch(b))
#ifdef EXTENDED_BLOCKS
#undef IsAnyTorchBlock
#define IsAnyTorchBlock(b) ((b) == BLOCK_TORCH || IsRedTorch(b) || ((b) >= BLOCK_TORCH_S && (b) <= BLOCK_TORCH_W))
#endif

static void Builder_DrawTorch(int x, int y, int z) {
	struct Builder1DPart* part;
	struct VertexTextured* v;
	cc_bool bright;
	PackedCol color;
	TextureLoc loc;
	float v1, v2;
	cc_uint8 facing;
	float X, Y, Z;
	/* Bottom and top coordinates for the two crossing planes */
	float x1b,y1b,z1b, x2b,z2b; /* bottom corners */
	float x1t,z1t, x2t,z2t; /* top corners (may differ for tilt) */
	float y2;
	
	X  = (float)x; Y = (float)y; Z = (float)z;
	facing = DirectionalBlock_GetFacing(x, y, z);
	
	loc = Block_Tex(Builder_Block, FACE_XMAX);
	v1  = Atlas1D_RowId(loc) * Atlas1D.InvTileSize;
	v2  = v1 + Atlas1D.InvTileSize * UV2_Scale;
	/* Red Ore Torch: use bottom 11/16 (skip top 5 rows); regular torch: bottom 10/16 (skip top 6 rows) */
	if (IsRedTorch(Builder_Block)) {
		v1  = v1 + (5.0f/16.0f) * Atlas1D.InvTileSize * UV2_Scale;
	} else {
		v1  = v1 + (6.0f/16.0f) * Atlas1D.InvTileSize * UV2_Scale;
	}
	
	if (facing == 4 || facing == 1) {
		/* Ground torch (facing=4) or default (facing=1 used as fallback) */
		/* Straight vertical torch, full-width sprite planes */
		x1b = X + 2.50f/16.0f; z1b = Z + 2.50f/16.0f; y1b = Y;
		x2b = X + 13.5f/16.0f; z2b = Z + 13.5f/16.0f;
		x1t = x1b; z1t = z1b;
		x2t = x2b; z2t = z2b;
		y2  = IsRedTorch(Builder_Block) ? Y + 11.0f/16.0f : Y + 10.0f/16.0f;
		
		/* If facing is 1 (south) but this is actually ground, use ground torch */
		if (facing != 4) {
			/* Wall torch: attached to north wall (z-1), lean toward +Z */
			float wallOff = 8.0f/16.0f;
			float leanOff = -4.0f/16.0f;
			y1b = IsRedTorch(Builder_Block) ? Y + 2.0f/16.0f : Y + 3.0f/16.0f;
			y2  = Y + 13.0f/16.0f;
			/* Bottom shifted toward wall (-Z) */
			z1b = Z + 2.50f/16.0f - wallOff;
			z2b = Z + 13.5f/16.0f - wallOff;
			/* Top shifted away from wall (+Z) */
			z1t = Z + 2.50f/16.0f + leanOff;
			z2t = Z + 13.5f/16.0f + leanOff;
		}
	} else if (facing == 0) {
		/* Wall torch: attached to south wall (z+1), lean toward -Z */
		float wallOff = 8.0f/16.0f;
		float leanOff = -4.0f/16.0f;
		y1b = IsRedTorch(Builder_Block) ? Y + 2.0f/16.0f : Y + 3.0f/16.0f;
		y2  = Y + 13.0f/16.0f;
		x1b = X + 2.50f/16.0f; x2b = X + 13.5f/16.0f;
		x1t = x1b; x2t = x2b;
		/* Bottom toward wall (+Z) */
		z1b = Z + 2.50f/16.0f + wallOff;
		z2b = Z + 13.5f/16.0f + wallOff;
		/* Top away from wall (-Z) */
		z1t = Z + 2.50f/16.0f - leanOff;
		z2t = Z + 13.5f/16.0f - leanOff;
	} else if (facing == 2) {
		/* Wall torch: attached to east wall (x+1), lean toward -X */
		float wallOff = 8.0f/16.0f;
		float leanOff = -4.0f/16.0f;
		y1b = IsRedTorch(Builder_Block) ? Y + 2.0f/16.0f : Y + 3.0f/16.0f;
		y2  = Y + 13.0f/16.0f;
		z1b = Z + 2.50f/16.0f; z2b = Z + 13.5f/16.0f;
		z1t = z1b; z2t = z2b;
		/* Bottom toward wall (+X) */
		x1b = X + 2.50f/16.0f + wallOff;
		x2b = X + 13.5f/16.0f + wallOff;
		/* Top away from wall (-X) */
		x1t = X + 2.50f/16.0f - leanOff;
		x2t = X + 13.5f/16.0f - leanOff;
	} else if (facing == 3) {
		/* Wall torch: attached to west wall (x-1), lean toward +X */
		float wallOff = 8.0f/16.0f;
		float leanOff = -4.0f/16.0f;
		y1b = IsRedTorch(Builder_Block) ? Y + 2.0f/16.0f : Y + 3.0f/16.0f;
		y2  = Y + 13.0f/16.0f;
		z1b = Z + 2.50f/16.0f; z2b = Z + 13.5f/16.0f;
		z1t = z1b; z2t = z2b;
		/* Bottom toward wall (-X) */
		x1b = X + 2.50f/16.0f - wallOff;
		x2b = X + 13.5f/16.0f - wallOff;
		/* Top away from wall (+X) */
		x1t = X + 2.50f/16.0f + leanOff;
		x2t = X + 13.5f/16.0f + leanOff;
	} else {
		/* Fallback: ground torch */
		x1b = X + 2.50f/16.0f; z1b = Z + 2.50f/16.0f; y1b = Y;
		x2b = X + 13.5f/16.0f; z2b = Z + 13.5f/16.0f;
		x1t = x1b; z1t = z1b;
		x2t = x2b; z2t = z2b;
		y2  = IsRedTorch(Builder_Block) ? Y + 11.0f/16.0f : Y + 10.0f/16.0f;
	}
	
	bright = Blocks.Brightness[Builder_Block];
	part   = &Builder_Parts[Atlas1D_Index(loc)];
	color  = bright ? PACKEDCOL_WHITE : Lighting.Color_Sprite_Fast(x, y, z);
	Block_Tint(color, Builder_Block);

	/* Draw Z axis - bottom uses (x1b,z1b)-(x2b,z2b), top uses (x1t,z1t)-(x2t,z2t) */
	v = &Builder_Vertices[part->sOffset];
	v->x = x1b; v->y = y1b; v->z = z1b; v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = x1t; v->y = y2;  v->z = z1t; v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = x2t; v->y = y2;  v->z = z2t; v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = x2b; v->y = y1b; v->z = z2b; v->Col = color; v->U = s_u1; v->V = v2; v++;

	/* Draw Z axis mirrored */
	v -= 4; v += part->sCount >> 2;
	v->x = x2b; v->y = y1b; v->z = z2b; v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = x2t; v->y = y2;  v->z = z2t; v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = x1t; v->y = y2;  v->z = z1t; v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = x1b; v->y = y1b; v->z = z1b; v->Col = color; v->U = s_u1; v->V = v2; v++;

	/* Draw X axis - cross pattern with swapped z/x corners */
	v -= 4; v += part->sCount >> 2;
	v->x = x1b; v->y = y1b; v->z = z2b; v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = x1t; v->y = y2;  v->z = z2t; v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = x2t; v->y = y2;  v->z = z1t; v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = x2b; v->y = y1b; v->z = z1b; v->Col = color; v->U = s_u1; v->V = v2; v++;

	/* Draw X axis mirrored */
	v -= 4; v += part->sCount >> 2;
	v->x = x2b; v->y = y1b; v->z = z1b; v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = x2t; v->y = y2;  v->z = z1t; v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = x1t; v->y = y2;  v->z = z2t; v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = x1b; v->y = y1b; v->z = z2b; v->Col = color; v->U = s_u1; v->V = v2; v++;

	part->sOffset += 4;
}

static void Builder_DrawLeverHandle(int x, int y, int z) {
	struct Builder1DPart* part;
	struct VertexTextured* v;
	PackedCol color;
	TextureLoc loc;
	float v1, v2;
	cc_uint8 facing;
	float X, Y, Z;
	float x1b,y1b,z1b, x2b,z2b;
	float x1t,z1t, x2t,z2t;
	float y2;
	float d_up, d_out;
	float hw = 6.5f/16.0f; /* half-width of crossed sprite planes (slightly thicker than torch) */
	cc_bool leverOn;
	float cx, cz; /* center of the base on the wall */
	
	X  = (float)x; Y = (float)y; Z = (float)z;
	facing = DirectionalBlock_GetFacing(x, y, z);
	leverOn = (Builder_Block == BLOCK_LEVER_ON);
	
	loc = 96; /* Lever handle texture */
	v1  = Atlas1D_RowId(loc) * Atlas1D.InvTileSize;
	v2  = v1 + Atlas1D.InvTileSize * UV2_Scale;
	/* Only show bottom 10/16 of texture (skip top 6 rows, like a regular torch) */
	v1  = v1 + (6.0f/16.0f) * Atlas1D.InvTileSize * UV2_Scale;
	
	d_up  = 9.0f/16.0f * 0.6428f;  /* handleLen * sin(40) */
	d_out = 9.0f/16.0f * 0.7660f;  /* handleLen * cos(40) */
	
	if (!leverOn) d_up = -d_up;
	
	/* Center the crossed sprite planes on the base center, not block center */
	switch (facing) {
		case 0: /* South wall (z+1) - base at z=13-16/16 */
			cx = X + 0.5f; cz = Z + 14.5f/16.0f;
			x1b = cx - hw; x2b = cx + hw;
			z1b = cz - hw; z2b = cz + hw;
			x1t = x1b; x2t = x2b;
			z1t = z1b - d_out; z2t = z2b - d_out;
			y1b = Y + 0.5f; y2 = Y + 0.5f + d_up;
			break;
		case 1: /* North wall (z-1) - base at z=0-3/16 */
			cx = X + 0.5f; cz = Z + 1.5f/16.0f;
			x1b = cx - hw; x2b = cx + hw;
			z1b = cz - hw; z2b = cz + hw;
			x1t = x1b; x2t = x2b;
			z1t = z1b + d_out; z2t = z2b + d_out;
			y1b = Y + 0.5f; y2 = Y + 0.5f + d_up;
			break;
		case 2: /* East wall (x+1) - base at x=13-16/16 */
			cx = X + 14.5f/16.0f; cz = Z + 0.5f;
			x1b = cx - hw; x2b = cx + hw;
			z1b = cz - hw; z2b = cz + hw;
			z1t = z1b; z2t = z2b;
			x1t = x1b - d_out; x2t = x2b - d_out;
			y1b = Y + 0.5f; y2 = Y + 0.5f + d_up;
			break;
		case 3: /* West wall (x-1) - base at x=0-3/16 */
			cx = X + 1.5f/16.0f; cz = Z + 0.5f;
			x1b = cx - hw; x2b = cx + hw;
			z1b = cz - hw; z2b = cz + hw;
			z1t = z1b; z2t = z2b;
			x1t = x1b + d_out; x2t = x2b + d_out;
			y1b = Y + 0.5f; y2 = Y + 0.5f + d_up;
			break;
		default:
			return;
	}
	
	part  = &Builder_Parts[Atlas1D_Index(loc)];
	color = Blocks.Brightness[Builder_Block] ? PACKEDCOL_WHITE : Lighting.Color_Sprite_Fast(x, y, z);
	Block_Tint(color, Builder_Block);
	
	/* Draw Z axis */
	v = &Builder_Vertices[part->sOffset];
	v->x = x1b; v->y = y1b; v->z = z1b; v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = x1t; v->y = y2;  v->z = z1t; v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = x2t; v->y = y2;  v->z = z2t; v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = x2b; v->y = y1b; v->z = z2b; v->Col = color; v->U = s_u1; v->V = v2; v++;
	
	/* Draw Z axis mirrored */
	v -= 4; v += part->sCount >> 2;
	v->x = x2b; v->y = y1b; v->z = z2b; v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = x2t; v->y = y2;  v->z = z2t; v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = x1t; v->y = y2;  v->z = z1t; v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = x1b; v->y = y1b; v->z = z1b; v->Col = color; v->U = s_u1; v->V = v2; v++;
	
	/* Draw X axis */
	v -= 4; v += part->sCount >> 2;
	v->x = x1b; v->y = y1b; v->z = z2b; v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = x1t; v->y = y2;  v->z = z2t; v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = x2t; v->y = y2;  v->z = z1t; v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = x2b; v->y = y1b; v->z = z1b; v->Col = color; v->U = s_u1; v->V = v2; v++;
	
	/* Draw X axis mirrored */
	v -= 4; v += part->sCount >> 2;
	v->x = x2b; v->y = y1b; v->z = z1b; v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = x2t; v->y = y2;  v->z = z1t; v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = x1t; v->y = y2;  v->z = z2t; v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = x1b; v->y = y1b; v->z = z2b; v->Col = color; v->U = s_u1; v->V = v2; v++;
	
	part->sOffset += 4;
}

/* Draws a crop sprite using the Minecraft Alpha hash (#) pattern:          */
/*  4 flat planes (each double-sided = 8 quads), offset 1/16 block lower.  */
/*  Planes at X +/- 0.25 spanning full Z, and Z +/- 0.25 spanning full X.  */
static void Builder_DrawCropSprite(int x, int y, int z) {
	struct Builder1DPart* part;
	struct VertexTextured* v;
	cc_bool bright;
	PackedCol color;
	TextureLoc loc;
	float v1, v2;
	float X, Y, Z;
	float y1, y2, px1, px2, pz1, pz2;
	int step;

	X = (float)x; Y = (float)y; Z = (float)z;
	y1  = Y - 1.0f/16.0f;          /* sit 1px lower onto farmland surface */
	y2  = Y + 1.0f - 1.0f/16.0f;
	px1 = X + 4.0f/16.0f;          /* X = x + 0.25 */
	px2 = X + 12.0f/16.0f;         /* X = x + 0.75 */
	pz1 = Z + 4.0f/16.0f;          /* Z = z + 0.25 */
	pz2 = Z + 12.0f/16.0f;         /* Z = z + 0.75 */

	loc = Block_Tex(Builder_Block, FACE_XMAX);
	v1  = Atlas1D_RowId(loc) * Atlas1D.InvTileSize;
	v2  = v1 + Atlas1D.InvTileSize * UV2_Scale;

	bright = Blocks.Brightness[Builder_Block];
	part   = &Builder_Parts[Atlas1D_Index(loc)];
	color  = bright ? PACKEDCOL_WHITE : Lighting.Color_Sprite_Fast(x, y, z);
	Block_Tint(color, Builder_Block);

	step = part->sCount >> 2;

	/* ---- Bucket 0: Plane 1 (X=px1, spanning Z) front + back ---- */
	v = &Builder_Vertices[part->sOffset];
	/* front */
	v->x = px1; v->y = y1; v->z = Z;       v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = px1; v->y = y2; v->z = Z;       v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = px1; v->y = y2; v->z = Z+1.0f;  v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = px1; v->y = y1; v->z = Z+1.0f;  v->Col = color; v->U = s_u1; v->V = v2; v++;
	/* back */
	v->x = px1; v->y = y1; v->z = Z+1.0f;  v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = px1; v->y = y2; v->z = Z+1.0f;  v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = px1; v->y = y2; v->z = Z;       v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = px1; v->y = y1; v->z = Z;       v->Col = color; v->U = s_u1; v->V = v2; v++;

	/* ---- Bucket 1: Plane 2 (X=px2, spanning Z) front + back ---- */
	v = &Builder_Vertices[part->sOffset + step];
	/* front */
	v->x = px2; v->y = y1; v->z = Z;       v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = px2; v->y = y2; v->z = Z;       v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = px2; v->y = y2; v->z = Z+1.0f;  v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = px2; v->y = y1; v->z = Z+1.0f;  v->Col = color; v->U = s_u1; v->V = v2; v++;
	/* back */
	v->x = px2; v->y = y1; v->z = Z+1.0f;  v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = px2; v->y = y2; v->z = Z+1.0f;  v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = px2; v->y = y2; v->z = Z;       v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = px2; v->y = y1; v->z = Z;       v->Col = color; v->U = s_u1; v->V = v2; v++;

	/* ---- Bucket 2: Plane 3 (Z=pz1, spanning X) front + back ---- */
	v = &Builder_Vertices[part->sOffset + 2 * step];
	/* front */
	v->x = X;       v->y = y1; v->z = pz1; v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = X;       v->y = y2; v->z = pz1; v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = X+1.0f;  v->y = y2; v->z = pz1; v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = X+1.0f;  v->y = y1; v->z = pz1; v->Col = color; v->U = s_u1; v->V = v2; v++;
	/* back */
	v->x = X+1.0f;  v->y = y1; v->z = pz1; v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = X+1.0f;  v->y = y2; v->z = pz1; v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = X;       v->y = y2; v->z = pz1; v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = X;       v->y = y1; v->z = pz1; v->Col = color; v->U = s_u1; v->V = v2; v++;

	/* ---- Bucket 3: Plane 4 (Z=pz2, spanning X) front + back ---- */
	v = &Builder_Vertices[part->sOffset + 3 * step];
	/* front */
	v->x = X;       v->y = y1; v->z = pz2; v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = X;       v->y = y2; v->z = pz2; v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = X+1.0f;  v->y = y2; v->z = pz2; v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = X+1.0f;  v->y = y1; v->z = pz2; v->Col = color; v->U = s_u1; v->V = v2; v++;
	/* back */
	v->x = X+1.0f;  v->y = y1; v->z = pz2; v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = X+1.0f;  v->y = y2; v->z = pz2; v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = X;       v->y = y2; v->z = pz2; v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = X;       v->y = y1; v->z = pz2; v->Col = color; v->U = s_u1; v->V = v2; v++;

	part->sOffset += 8;
}

static void Builder_DrawSprite(int x, int y, int z) {
	struct Builder1DPart* part;
	struct VertexTextured* v;
	cc_uint8 offsetType;
	cc_bool bright;
	PackedCol color;
	TextureLoc loc;
	float v1, v2;

	float X, Y, Z;
	float valX, valY, valZ;
	float x1,y1,z1, x2,y2,z2;
	cc_bool ceilingFire = false;
	
	X  = (float)x; Y = (float)y; Z = (float)z;
	x1 = X + 2.50f/16.0f; y1 = Y;        z1 = Z + 2.50f/16.0f;
	x2 = X + 13.5f/16.0f; y2 = Y + 1.0f; z2 = Z + 13.5f/16.0f;

	/* Fire orientation: wall-flat, ceiling-flat, ground sprite, or hidden */
	if (Builder_Block == BLOCK_FIRE) {
		BlockID below = (y > 0) ? World_GetBlock(x, y - 1, z) : BLOCK_AIR;
		BlockID above = World_Contains(x, y + 1, z) ? World_GetBlock(x, y + 1, z) : BLOCK_AIR;
		cc_bool solidBelow = Blocks.Collide[below] == COLLIDE_SOLID;
		cc_bool solidAbove = Blocks.Collide[above] == COLLIDE_SOLID;
		BlockID adjS = World_Contains(x, y, z + 1) ? World_GetBlock(x, y, z + 1) : BLOCK_AIR;
		BlockID adjN = World_Contains(x, y, z - 1) ? World_GetBlock(x, y, z - 1) : BLOCK_AIR;
		BlockID adjE = World_Contains(x + 1, y, z) ? World_GetBlock(x + 1, y, z) : BLOCK_AIR;
		BlockID adjW = World_Contains(x - 1, y, z) ? World_GetBlock(x - 1, y, z) : BLOCK_AIR;
		cc_bool wallS = Blocks.Collide[adjS] == COLLIDE_SOLID;
		cc_bool wallN = Blocks.Collide[adjN] == COLLIDE_SOLID;
		cc_bool wallE = Blocks.Collide[adjE] == COLLIDE_SOLID;
		cc_bool wallW = Blocks.Collide[adjW] == COLLIDE_SOLID;

		if (solidBelow) {
			/* Ground fire: default crossed sprite, keep x1/z1/x2/z2 as-is */
		} else if (wallS) {
			x1 = X; x2 = X + 1; z1 = z2 = Z + 15.0f/16.0f;
		} else if (wallN) {
			x1 = X; x2 = X + 1; z1 = z2 = Z + 1.0f/16.0f;
		} else if (wallE) {
			x1 = x2 = X + 15.0f/16.0f; z1 = Z; z2 = Z + 1;
		} else if (wallW) {
			x1 = x2 = X + 1.0f/16.0f; z1 = Z; z2 = Z + 1;
		} else if (solidAbove) {
			/* Ceiling fire: handled separately below with custom horizontal quads */
			ceilingFire = true;
		} else {
			/* No adjacent blocks at all: hide by collapsing to degenerate point */
			x1 = x2 = X; y1 = y2 = Y; z1 = z2 = Z;
		}
	}

	loc = Block_Tex(Builder_Block, FACE_XMAX);
	v1  = Atlas1D_RowId(loc) * Atlas1D.InvTileSize;
	v2  = v1 + Atlas1D.InvTileSize * UV2_Scale;

	offsetType = Blocks.SpriteOffset[Builder_Block];
	if (offsetType >= 6 && offsetType <= 7) {
		Random_Seed(&spriteRng, (x + 1217 * z) & 0x7fffffff);
		valX = Random_Range(&spriteRng, -3, 3 + 1) / 16.0f;
		valY = Random_Range(&spriteRng, 0,  3 + 1) / 16.0f;
		valZ = Random_Range(&spriteRng, -3, 3 + 1) / 16.0f;

		x1 += valX - 1.7f/16.0f; x2 += valX + 1.7f/16.0f;
		z1 += valZ - 1.7f/16.0f; z2 += valZ + 1.7f/16.0f;
		if (offsetType == 7) { y1 -= valY; y2 -= valY; }
	}
	
	bright = Blocks.Brightness[Builder_Block];
	part   = &Builder_Parts[Atlas1D_Index(loc)];
	color  = bright ? PACKEDCOL_WHITE : Lighting.Color_Sprite_Fast(x, y, z);
	Block_Tint(color, Builder_Block);

	/* Ceiling fire: custom horizontal quads (crossed sprite can't make flat planes) */
	if (ceilingFire) {
		float h = Y + 15.0f/16.0f;
		float dg = (float)x; /* degenerate point for unused quads */

		/* Horizontal quad visible from below */
		v = &Builder_Vertices[part->sOffset];
		v->x = X;       v->y = h; v->z = Z;       v->Col = color; v->U = s_u2; v->V = v2; v++;
		v->x = X;       v->y = h; v->z = Z + 1.0f; v->Col = color; v->U = s_u2; v->V = v1; v++;
		v->x = X + 1.0f; v->y = h; v->z = Z + 1.0f; v->Col = color; v->U = s_u1; v->V = v1; v++;
		v->x = X + 1.0f; v->y = h; v->z = Z;       v->Col = color; v->U = s_u1; v->V = v2; v++;

		/* Horizontal quad visible from above (reversed winding) */
		v -= 4; v += part->sCount >> 2;
		v->x = X + 1.0f; v->y = h; v->z = Z;       v->Col = color; v->U = s_u2; v->V = v2; v++;
		v->x = X + 1.0f; v->y = h; v->z = Z + 1.0f; v->Col = color; v->U = s_u2; v->V = v1; v++;
		v->x = X;       v->y = h; v->z = Z + 1.0f; v->Col = color; v->U = s_u1; v->V = v1; v++;
		v->x = X;       v->y = h; v->z = Z;       v->Col = color; v->U = s_u1; v->V = v2; v++;

		/* Degenerate quad 3 */
		v -= 4; v += part->sCount >> 2;
		v->x = dg; v->y = Y; v->z = dg; v->Col = color; v->U = 0; v->V = 0; v++;
		v->x = dg; v->y = Y; v->z = dg; v->Col = color; v->U = 0; v->V = 0; v++;
		v->x = dg; v->y = Y; v->z = dg; v->Col = color; v->U = 0; v->V = 0; v++;
		v->x = dg; v->y = Y; v->z = dg; v->Col = color; v->U = 0; v->V = 0; v++;

		/* Degenerate quad 4 */
		v -= 4; v += part->sCount >> 2;
		v->x = dg; v->y = Y; v->z = dg; v->Col = color; v->U = 0; v->V = 0; v++;
		v->x = dg; v->y = Y; v->z = dg; v->Col = color; v->U = 0; v->V = 0; v++;
		v->x = dg; v->y = Y; v->z = dg; v->Col = color; v->U = 0; v->V = 0; v++;
		v->x = dg; v->y = Y; v->z = dg; v->Col = color; v->U = 0; v->V = 0; v++;

		part->sOffset += 4;
		return;
	}

	/* Draw Z axis */
	v = &Builder_Vertices[part->sOffset];
	v->x = x1; v->y = y1; v->z = z1; v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = x1; v->y = y2; v->z = z1; v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = x2; v->y = y2; v->z = z2; v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = x2; v->y = y1; v->z = z2; v->Col = color; v->U = s_u1; v->V = v2; v++;

	/* Draw Z axis mirrored */
	v -= 4; v += part->sCount >> 2;
	v->x = x2; v->y = y1; v->z = z2; v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = x2; v->y = y2; v->z = z2; v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = x1; v->y = y2; v->z = z1; v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = x1; v->y = y1; v->z = z1; v->Col = color; v->U = s_u1; v->V = v2; v++;

	/* Draw X axis */
	v -= 4; v += part->sCount >> 2;
	v->x = x1; v->y = y1; v->z = z2; v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = x1; v->y = y2; v->z = z2; v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = x2; v->y = y2; v->z = z1; v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = x2; v->y = y1; v->z = z1; v->Col = color; v->U = s_u1; v->V = v2; v++;

	/* Draw X axis mirrored */
	v -= 4; v += part->sCount >> 2;
	v->x = x2; v->y = y1; v->z = z1; v->Col = color; v->U = s_u2; v->V = v2; v++;
	v->x = x2; v->y = y2; v->z = z1; v->Col = color; v->U = s_u2; v->V = v1; v++;
	v->x = x1; v->y = y2; v->z = z2; v->Col = color; v->U = s_u1; v->V = v1; v++;
	v->x = x1; v->y = y1; v->z = z2; v->Col = color; v->U = s_u1; v->V = v2; v++;

	part->sOffset += 4;
}


/*########################################################################################################################*
*--------------------------------------------------Normal mesh builder----------------------------------------------------*
*#########################################################################################################################*/
static PackedCol Normal_LightColor(int x, int y, int z, Face face, BlockID block) {
	int offset = (Blocks.LightOffset[block] >> face) & 1;

	switch (face) {
	case FACE_XMIN:
		return x < offset                ? Env.SunXSide : Lighting.Color_XSide_Fast(x - offset, y, z);
	case FACE_XMAX:
		return x > (World.MaxX - offset) ? Env.SunXSide : Lighting.Color_XSide_Fast(x + offset, y, z);
	case FACE_ZMIN:
		return z < offset                ? Env.SunZSide : Lighting.Color_ZSide_Fast(x, y, z - offset);
	case FACE_ZMAX:
		return z > (World.MaxZ - offset) ? Env.SunZSide : Lighting.Color_ZSide_Fast(x, y, z + offset);

	case FACE_YMIN:
		return Lighting.Color_YMin_Fast(x, y - offset, z);		
	case FACE_YMAX:
		return Lighting.Color_YMax_Fast(x, y + offset, z);
	}
	return 0; /* should never happen */
}

/* Check if a grass block has snow or snow block above it */
static cc_bool HasSnowAbove(int x, int y, int z) {
	BlockID above = World_SafeGetBlock(x, y + 1, z);
	return above == BLOCK_SNOW || above == BLOCK_SNOW_BLOCK;
}

/* Renders a stair block as two sub-blocks (MC Alpha 1.2.6 style).
   Uses the directional cache to determine which direction the step faces.
   Facing 0=North(-Z step), 1=South(+Z step), 2=West(-X step), 3=East(+X step)
   For each facing, there's a bottom half-slab on the step side and a full-height block on the back. */
static void Builder_DrawStairs(int x, int y, int z) {
	cc_uint8 facing;
	int baseOffset, lightFlags, f;
	TextureLoc loc;
	PackedCol col;
	struct Builder1DPart* part;

	/* Two sub-block bounds: A = slab (step), B = full (back) */
	float ax1, ay1, az1, ax2, ay2, az2;
	float bx1, by1, bz1, bx2, by2, bz2;
	Vec3 aMin, aMax, bMin, bMax;

	facing = Block_GetStairFacing(Builder_Block);
	baseOffset = (Blocks.Draw[Builder_Block] == DRAW_TRANSLUCENT) * ATLAS1D_MAX_ATLASES;
	lightFlags = Blocks.LightOffset[Builder_Block];

	Drawer.Tinted  = Blocks.Tinted[Builder_Block];
	Drawer.TintCol = Blocks.FogCol[Builder_Block];

	/* MC Alpha 1.2.6 RenderBlocks.renderBlockStairs - exact port */
	switch (facing) {
		case 0: /* MC meta 0: step on -X half, full on +X half */
			Vec3_Set(aMin, 0, 0, 0);     Vec3_Set(aMax, 0.5f, 0.5f, 1);
			Vec3_Set(bMin, 0.5f, 0, 0);  Vec3_Set(bMax, 1, 1, 1);
			break;
		case 1: /* MC meta 1: full on -X half, step on +X half */
			Vec3_Set(aMin, 0, 0, 0);     Vec3_Set(aMax, 0.5f, 1, 1);
			Vec3_Set(bMin, 0.5f, 0, 0);  Vec3_Set(bMax, 1, 0.5f, 1);
			break;
		case 2: /* MC meta 2: step on -Z half, full on +Z half */
			Vec3_Set(aMin, 0, 0, 0);     Vec3_Set(aMax, 1, 0.5f, 0.5f);
			Vec3_Set(bMin, 0, 0, 0.5f);  Vec3_Set(bMax, 1, 1, 1);
			break;
		default: /* MC meta 3: full on -Z half, step on +Z half */
			Vec3_Set(aMin, 0, 0, 0);     Vec3_Set(aMax, 1, 1, 0.5f);
			Vec3_Set(bMin, 0, 0, 0.5f);  Vec3_Set(bMax, 1, 0.5f, 1);
			break;
	}

	/* Get texture - stairs use same texture on all faces */
	loc = Block_Tex(Builder_Block, FACE_YMAX);

	/* Render both sub-blocks using the Drawer functions */
	/* Sub-block A */
	ax1 = x + aMin.x; ay1 = y + aMin.y; az1 = z + aMin.z;
	ax2 = x + aMax.x; ay2 = y + aMax.y; az2 = z + aMax.z;

	Drawer.MinBB = aMin; Drawer.MinBB.y = 1.0f - Drawer.MinBB.y;
	Drawer.MaxBB = aMax; Drawer.MaxBB.y = 1.0f - Drawer.MaxBB.y;
	Drawer.X1 = ax1; Drawer.Y1 = ay1; Drawer.Z1 = az1;
	Drawer.X2 = ax2; Drawer.Y2 = ay2; Drawer.Z2 = az2;

	part = &Builder_Parts[baseOffset + Atlas1D_Index(loc)];
	for (f = FACE_XMIN; f <= FACE_YMAX; f++) {
		col = Blocks.Brightness[Builder_Block] ? PACKEDCOL_WHITE : Normal_LightColor(x, y, z, f, Builder_Block);
		switch (f) {
			case FACE_XMIN: Drawer_XMin(1, col, loc, &part->faces.vertices[FACE_XMIN]); break;
			case FACE_XMAX: Drawer_XMax(1, col, loc, &part->faces.vertices[FACE_XMAX]); break;
			case FACE_ZMIN: Drawer_ZMin(1, col, loc, &part->faces.vertices[FACE_ZMIN]); break;
			case FACE_ZMAX: Drawer_ZMax(1, col, loc, &part->faces.vertices[FACE_ZMAX]); break;
			case FACE_YMIN: Drawer_YMin(1, col, loc, &part->faces.vertices[FACE_YMIN]); break;
			case FACE_YMAX: Drawer_YMax(1, col, loc, &part->faces.vertices[FACE_YMAX]); break;
		}
	}

	/* Sub-block B */
	bx1 = x + bMin.x; by1 = y + bMin.y; bz1 = z + bMin.z;
	bx2 = x + bMax.x; by2 = y + bMax.y; bz2 = z + bMax.z;

	Drawer.MinBB = bMin; Drawer.MinBB.y = 1.0f - Drawer.MinBB.y;
	Drawer.MaxBB = bMax; Drawer.MaxBB.y = 1.0f - Drawer.MaxBB.y;
	Drawer.X1 = bx1; Drawer.Y1 = by1; Drawer.Z1 = bz1;
	Drawer.X2 = bx2; Drawer.Y2 = by2; Drawer.Z2 = bz2;

	for (f = FACE_XMIN; f <= FACE_YMAX; f++) {
		col = Blocks.Brightness[Builder_Block] ? PACKEDCOL_WHITE : Normal_LightColor(x, y, z, f, Builder_Block);
		switch (f) {
			case FACE_XMIN: Drawer_XMin(1, col, loc, &part->faces.vertices[FACE_XMIN]); break;
			case FACE_XMAX: Drawer_XMax(1, col, loc, &part->faces.vertices[FACE_XMAX]); break;
			case FACE_ZMIN: Drawer_ZMin(1, col, loc, &part->faces.vertices[FACE_ZMIN]); break;
			case FACE_ZMAX: Drawer_ZMax(1, col, loc, &part->faces.vertices[FACE_ZMAX]); break;
			case FACE_YMIN: Drawer_YMin(1, col, loc, &part->faces.vertices[FACE_YMIN]); break;
			case FACE_YMAX: Drawer_YMax(1, col, loc, &part->faces.vertices[FACE_YMAX]); break;
		}
	}
}

/* Draws a rail block as a flat or sloped quad on the YMAX face.
   Each rail orientation is its own block ID with its own texture.
   No UV rotation needed - textures are pre-oriented in terrain.png. */
static void Builder_DrawRail(int index, int x, int y, int z) {
	struct Builder1DPart* part;
	struct VertexTextured* v;
	PackedCol col;
	TextureLoc loc;
	float vOrigin, u1, u2, v1, v2;
	float x1, x2, z1, z2;
	float yNE, yNW, ySW, ySE;
	int count_YMax, lightFlags, offset, baseOffset;
	BlockID block = Builder_Block;
	
	count_YMax = Builder_Counts[index + FACE_YMAX];
	if (!count_YMax) return;
	
	/* Each block type has the correct texture in its definition */
	loc = Block_Tex(block, FACE_YMAX);
	
	/* Calculate UV coordinates from atlas */
	vOrigin = Atlas1D_RowId(loc) * Atlas1D.InvTileSize;
	u1 = 0;
	u2 = UV2_Scale;
	v1 = vOrigin;
	v2 = vOrigin + Atlas1D.InvTileSize * UV2_Scale;
	
	/* Calculate world positions */
	lightFlags = Blocks.LightOffset[block];
	baseOffset = (Blocks.Draw[block] == DRAW_TRANSLUCENT) * ATLAS1D_MAX_ATLASES;
	
	x1 = (float)x;
	x2 = (float)x + 1.0f;
	z1 = (float)z;
	z2 = (float)z + 1.0f;
	
	/* Base Y for the flat rail surface */
	{
		float yBase = (float)y + Blocks.MaxBB[block].y;
		yNE = yNW = ySW = ySE = yBase;
		
		/* Slopes: raise the appropriate edge by 1 block based on block ID */
		switch (block) {
			case BLOCK_RAIL_ASC_E: /* ascending east: east edge (x+) raised */
				yNE += 1.0f; ySE += 1.0f; break;
			case BLOCK_RAIL_ASC_W: /* ascending west: west edge (x-) raised */
				yNW += 1.0f; ySW += 1.0f; break;
			case BLOCK_RAIL_ASC_N: /* ascending north: north edge (z-) raised */
				yNE += 1.0f; yNW += 1.0f; break;
			case BLOCK_RAIL_ASC_S: /* ascending south: south edge (z+) raised */
				ySE += 1.0f; ySW += 1.0f; break;
			default: break;
		}
	}
	
	offset = (lightFlags >> FACE_YMAX) & 1;
	col    = Lighting.Color_YMax_Fast(x, y + offset, z);
	
	part = &Builder_Parts[baseOffset + Atlas1D_Index(loc)];
	v = part->faces.vertices[FACE_YMAX];
	
	/* Emit 4 vertices - no UV rotation, textures are pre-oriented.
	   Vertex order: NE, NW, SW, SE (matching Drawer_YMax) */
	/* NE corner (x+, z-) */
	v->x = x2; v->y = yNE; v->z = z1; v->Col = col;
	v->U = u2; v->V = v1; v++;
	
	/* NW corner (x-, z-) */
	v->x = x1; v->y = yNW; v->z = z1; v->Col = col;
	v->U = u1; v->V = v1; v++;
	
	/* SW corner (x-, z+) */
	v->x = x1; v->y = ySW; v->z = z2; v->Col = col;
	v->U = u1; v->V = v2; v++;
	
	/* SE corner (x+, z+) */
	v->x = x2; v->y = ySE; v->z = z2; v->Col = col;
	v->U = u2; v->V = v2; v++;
	
	part->faces.vertices[FACE_YMAX] = v;
}

static cc_bool Normal_CanStretch(BlockID initial, int chunkIndex, int x, int y, int z, Face face) {
	BlockID cur = Builder_Chunk[chunkIndex];

	if (cur != initial || Block_IsFaceHidden(cur, Builder_Chunk[chunkIndex + Builder_Offsets[face]], face)) return false;
	/* Grass blocks with different snow state above have different textures, can't merge */
	if (cur == BLOCK_GRASS && HasSnowAbove(Builder_X, Builder_Y, Builder_Z) != HasSnowAbove(x, y, z)) return false;
	if (Builder_FullBright) return true;

	return Normal_LightColor(Builder_X, Builder_Y, Builder_Z, face, initial) == Normal_LightColor(x, y, z, face, cur);
}

static int NormalBuilder_StretchXLiquid(int countIndex, int x, int y, int z, int chunkIndex, BlockID block) {
	int count = 1; cc_bool stretchTile;
	if (Builder_OccludedLiquid(chunkIndex)) return 0;
	
	/* Finite liquid uses per-corner heights, never stretch */
	if (Builder_IsFiniteLiquid(block)) {
		AddVerticesFiniteLiquid(block, FACE_YMAX);
		return count;
	}
	
	x++;
	chunkIndex++;
	countIndex += FACE_COUNT;
	stretchTile = (Blocks.CanStretch[block] & (1 << FACE_YMAX)) != 0;

	while (x < Builder_ChunkEndX && stretchTile && Normal_CanStretch(block, chunkIndex, x, y, z, FACE_YMAX) && !Builder_OccludedLiquid(chunkIndex)) {
		Builder_Counts[countIndex] = 0;
		count++;
		x++;
		chunkIndex++;
		countIndex += FACE_COUNT;
	}
	AddVertices(block, FACE_YMAX);
	return count;
}

static int NormalBuilder_StretchX(int countIndex, int x, int y, int z, int chunkIndex, BlockID block, Face face) {
	int count = 1; cc_bool stretchTile;
	/* Finite liquid uses per-corner heights, never stretch */
	if (Builder_IsFiniteLiquid(block)) {
		AddVerticesFiniteLiquid(block, face);
		return count;
	}
	x++;
	chunkIndex++;
	countIndex += FACE_COUNT;
	stretchTile = (Blocks.CanStretch[block] & (1 << face)) != 0;

	while (x < Builder_ChunkEndX && stretchTile && Normal_CanStretch(block, chunkIndex, x, y, z, face)) {
		Builder_Counts[countIndex] = 0;
		count++;
		x++;
		chunkIndex++;
		countIndex += FACE_COUNT;
	}
	AddVertices(block, face);
	return count;
}

static int NormalBuilder_StretchZ(int countIndex, int x, int y, int z, int chunkIndex, BlockID block, Face face) {
	int count = 1; cc_bool stretchTile;
	/* Finite liquid uses per-corner heights, never stretch */
	if (Builder_IsFiniteLiquid(block)) {
		AddVerticesFiniteLiquid(block, face);
		return count;
	}
	z++;
	chunkIndex += EXTCHUNK_SIZE;
	countIndex += CHUNK_SIZE * FACE_COUNT;
	stretchTile = (Blocks.CanStretch[block] & (1 << face)) != 0;

	while (z < Builder_ChunkEndZ && stretchTile && Normal_CanStretch(block, chunkIndex, x, y, z, face)) {
		Builder_Counts[countIndex] = 0;
		count++;
		z++;
		chunkIndex += EXTCHUNK_SIZE;
		countIndex += CHUNK_SIZE * FACE_COUNT;
	}
	AddVertices(block, face);
	return count;
}

static void NormalBuilder_RenderBlock(int index, int x, int y, int z) {	
	/* counters */
	int count_XMin, count_XMax, count_ZMin;
	int count_ZMax, count_YMin, count_YMax;

	/* block state */
	Vec3 min, max;
	int baseOffset, lightFlags;
	cc_bool fullBright;

	/* per-face state */
	struct Builder1DPart* part;
	TextureLoc loc;
	PackedCol col;
	int offset;

	if (Blocks.Draw[Builder_Block] == DRAW_SPRITE) {
		if (IsAnyTorchBlock(Builder_Block)) { Builder_DrawTorch(x, y, z); return; }
		if (IsCropBlock(Builder_Block))     { Builder_DrawCropSprite(x, y, z); return; }
		Builder_DrawSprite(x, y, z); return;
	}

	if (Builder_IsFiniteLiquid(Builder_Block)) {
		Builder_RenderFiniteLiquid(index, x, y, z); return;
	}

	/* Rails use custom rendering with UV rotation for the top face */
	if (IsRailBlock(Builder_Block)) {
		Builder_DrawRail(index, x, y, z); return;
	}

	/* Stairs use custom rendering as two sub-blocks */
	if (IsStairBlock(Builder_Block)) {
		Builder_DrawStairs(x, y, z); return;
	}

	count_XMin = Builder_Counts[index + FACE_XMIN];
	count_XMax = Builder_Counts[index + FACE_XMAX];
	count_ZMin = Builder_Counts[index + FACE_ZMIN];
	count_ZMax = Builder_Counts[index + FACE_ZMAX];
	count_YMin = Builder_Counts[index + FACE_YMIN];
	count_YMax = Builder_Counts[index + FACE_YMAX];

	if (!count_XMin && !count_XMax && !count_ZMin &&
		!count_ZMax && !count_YMin && !count_YMax) return;

	fullBright = Blocks.Brightness[Builder_Block];
	baseOffset = (Blocks.Draw[Builder_Block] == DRAW_TRANSLUCENT) * ATLAS1D_MAX_ATLASES;
	lightFlags = Blocks.LightOffset[Builder_Block];

	Drawer.MinBB = Blocks.MinBB[Builder_Block]; Drawer.MinBB.y = 1.0f - Drawer.MinBB.y;
	Drawer.MaxBB = Blocks.MaxBB[Builder_Block]; Drawer.MaxBB.y = 1.0f - Drawer.MaxBB.y;

	DirectionalBlock_GetRenderBounds(Builder_Block, x, y, z, &min, &max);
	Drawer.X1 = x + min.x; Drawer.Y1 = y + min.y; Drawer.Z1 = z + min.z;
	Drawer.X2 = x + max.x; Drawer.Y2 = y + max.y; Drawer.Z2 = z + max.z;

	/* For buttons/levers/pressure plates/signs, use dynamic bounds for UV mapping too, so textures match geometry */
	if (Builder_Block == BLOCK_BUTTON || Builder_Block == BLOCK_BUTTON_PRESSED
		|| IsLeverBlock(Builder_Block)
		|| Builder_Block == BLOCK_PRESSURE_PLATE || Builder_Block == BLOCK_PRESSURE_PLATE_PRESSED
		|| Builder_Block == BLOCK_SIGN_FLOOR || Builder_Block == BLOCK_SIGN_WALL) {
		Drawer.MinBB = min; Drawer.MinBB.y = 1.0f - Drawer.MinBB.y;
		Drawer.MaxBB = max; Drawer.MaxBB.y = 1.0f - Drawer.MaxBB.y;
	}

	Drawer.Tinted  = Blocks.Tinted[Builder_Block];
	Drawer.TintCol = Blocks.FogCol[Builder_Block];

	if (count_XMin) {
		loc    = DirectionalBlock_GetTexture(Builder_Block, x, y, z, FACE_XMIN);
		offset = (lightFlags >> FACE_XMIN) & 1;
		part   = &Builder_Parts[baseOffset + Atlas1D_Index(loc)];

		col = fullBright ? PACKEDCOL_WHITE :
			x >= offset ? Lighting.Color_XSide_Fast(x - offset, y, z) : Env.SunXSide;
		Drawer_XMin(count_XMin, col, loc, &part->faces.vertices[FACE_XMIN]);
	}

	if (count_XMax) {
		loc    = DirectionalBlock_GetTexture(Builder_Block, x, y, z, FACE_XMAX);
		offset = (lightFlags >> FACE_XMAX) & 1;
		part   = &Builder_Parts[baseOffset + Atlas1D_Index(loc)];

		col = fullBright ? PACKEDCOL_WHITE :
			x <= (World.MaxX - offset) ? Lighting.Color_XSide_Fast(x + offset, y, z) : Env.SunXSide;
		Drawer_XMax(count_XMax, col, loc, &part->faces.vertices[FACE_XMAX]);
	}

	if (count_ZMin) {
		loc    = DirectionalBlock_GetTexture(Builder_Block, x, y, z, FACE_ZMIN);
		offset = (lightFlags >> FACE_ZMIN) & 1;
		part   = &Builder_Parts[baseOffset + Atlas1D_Index(loc)];

		col = fullBright ? PACKEDCOL_WHITE :
			z >= offset ? Lighting.Color_ZSide_Fast(x, y, z - offset) : Env.SunZSide;
		Drawer_ZMin(count_ZMin, col, loc, &part->faces.vertices[FACE_ZMIN]);
	}

	if (count_ZMax) {
		loc    = DirectionalBlock_GetTexture(Builder_Block, x, y, z, FACE_ZMAX);
		offset = (lightFlags >> FACE_ZMAX) & 1;
		part   = &Builder_Parts[baseOffset + Atlas1D_Index(loc)];

		col = fullBright ? PACKEDCOL_WHITE :
			z <= (World.MaxZ - offset) ? Lighting.Color_ZSide_Fast(x, y, z + offset) : Env.SunZSide;
		Drawer_ZMax(count_ZMax, col, loc, &part->faces.vertices[FACE_ZMAX]);
	}

	if (count_YMin) {
		loc    = DirectionalBlock_GetTexture(Builder_Block, x, y, z, FACE_YMIN);
		offset = (lightFlags >> FACE_YMIN) & 1;
		part   = &Builder_Parts[baseOffset + Atlas1D_Index(loc)];

		col = fullBright ? PACKEDCOL_WHITE : Lighting.Color_YMin_Fast(x, y - offset, z);
		Drawer_YMin(count_YMin, col, loc, &part->faces.vertices[FACE_YMIN]);
	}

	if (count_YMax) {
		loc    = DirectionalBlock_GetTexture(Builder_Block, x, y, z, FACE_YMAX);
		offset = (lightFlags >> FACE_YMAX) & 1;
		part   = &Builder_Parts[baseOffset + Atlas1D_Index(loc)];

		col = fullBright ? PACKEDCOL_WHITE : Lighting.Color_YMax_Fast(x, y + offset, z);
		Drawer_YMax(count_YMax, col, loc, &part->faces.vertices[FACE_YMAX]);
	}
	
	/* Draw lever handle sprite after base box faces */
	if (IsLeverBlock(Builder_Block)) {
		Builder_DrawLeverHandle(x, y, z);
	}
}

static void Builder_SetDefault(void) {
	Builder_StretchXLiquid = NULL;
	Builder_StretchX       = NULL;
	Builder_StretchZ       = NULL;
	Builder_RenderBlock    = NULL;

	Builder_PrePrepareChunk  = DefaultPrePrepateChunk;
	Builder_PostPrepareChunk = DefaultPostStretchChunk;
}

static void NormalBuilder_SetActive(void) {
	Builder_SetDefault();
	Builder_StretchXLiquid = NormalBuilder_StretchXLiquid;
	Builder_StretchX       = NormalBuilder_StretchX;
	Builder_StretchZ       = NormalBuilder_StretchZ;
	Builder_RenderBlock    = NormalBuilder_RenderBlock;
}


/*########################################################################################################################*
*-------------------------------------------------Advanced mesh builder---------------------------------------------------*
*#########################################################################################################################*/
#ifdef CC_BUILD_ADVLIGHTING
static Vec3 adv_minBB, adv_maxBB;
static int adv_initBitFlags, adv_baseOffset;
static int* adv_bitFlags;
static float adv_x1, adv_y1, adv_z1, adv_x2, adv_y2, adv_z2;
static int adv_blockX, adv_blockY, adv_blockZ;
static PackedCol adv_lerp[5], adv_lerpX[5], adv_lerpZ[5], adv_lerpY[5];
static cc_bool adv_tinted;

enum ADV_MASK {
	/* z-1 cube points */
	xM1_yM1_zM1, xM1_yCC_zM1, xM1_yP1_zM1,
	xCC_yM1_zM1, xCC_yCC_zM1, xCC_yP1_zM1,
	xP1_yM1_zM1, xP1_yCC_zM1, xP1_yP1_zM1,
	/* z cube points */
	xM1_yM1_zCC, xM1_yCC_zCC, xM1_yP1_zCC,
	xCC_yM1_zCC, xCC_yCC_zCC, xCC_yP1_zCC,
	xP1_yM1_zCC, xP1_yCC_zCC, xP1_yP1_zCC,
	/* z+1 cube points */
	xM1_yM1_zP1, xM1_yCC_zP1, xM1_yP1_zP1,
	xCC_yM1_zP1, xCC_yCC_zP1, xCC_yP1_zP1,
	xP1_yM1_zP1, xP1_yCC_zP1, xP1_yP1_zP1,
};

/* Bit-or the Adv_Lit flags with these to set the appropriate light values */
#define LIT_M1 (1 << 0)
#define LIT_CC (1 << 1)
#define LIT_P1 (1 << 2)
/* Returns a 3 bit value where */
/* - bit 0 set: Y-1 is in light */
/* - bit 1 set: Y   is in light */
/* - bit 2 set: Y+1 is in light */
static int Adv_Lit(int x, int y, int z, int cIndex) {
	int flags, offset, lightFlags;
	BlockID block;
	if (y < 0 || y >= World.Height) return LIT_M1 | LIT_CC | LIT_P1; /* all faces lit */

	/* TODO: check sides height (if sides > edges), check if edge block casts a shadow */
	if (!World_ContainsXZ(x, z)) {
		return y >= Builder_EdgeLevel ? LIT_M1 | LIT_CC | LIT_P1 : y == (Builder_EdgeLevel - 1) ? LIT_CC | LIT_P1 : 0;
	}

	flags = 0;
	block = Builder_Chunk[cIndex];
	lightFlags = Blocks.LightOffset[block];

	/* TODO using LIGHT_FLAG_SHADES_FROM_BELOW is wrong here, */
	/*  but still produces less broken results than YMIN/YMAX */

	/* Use fact Light(Y.YMin) == Light((Y-1).YMax) */
	offset = (lightFlags >> LIGHT_FLAG_SHADES_FROM_BELOW) & 1;
	flags |= Lighting.IsLit_Fast(x, y - offset, z) ? LIT_M1 : 0;
	/* Also check y-1 directly for torch light to fix torch lighting on top/bottom faces */
	if (!offset && Lighting.IsLit_Fast(x, y - 1, z)) flags |= LIT_M1;

	/* Light is same for all the horizontal faces */
	flags |= Lighting.IsLit_Fast(x, y, z) ? LIT_CC : 0;

	/* Use fact Light((Y+1).YMin) == Light(Y.YMax) */
	offset = (lightFlags >> LIGHT_FLAG_SHADES_FROM_BELOW) & 1;
	flags |= Lighting.IsLit_Fast(x, (y + 1) - offset, z) ? LIT_P1 : 0;
	/* Also check y+1 directly for torch light to fix torch lighting on top/bottom faces */
	if (offset && Lighting.IsLit_Fast(x, y + 1, z)) flags |= LIT_P1;

	/* If a block is fullbright, it should also look as if that spot is lit */
	if (Blocks.Brightness[Builder_Chunk[cIndex - 324]]) flags |= LIT_M1;
	if (Blocks.Brightness[block])                       flags |= LIT_CC;
	if (Blocks.Brightness[Builder_Chunk[cIndex + 324]]) flags |= LIT_P1;
	
	return flags;
}

static int Adv_ComputeLightFlags(int x, int y, int z, int cIndex) {
	if (Builder_FullBright) return (1 << xP1_yP1_zP1) - 1; /* all faces fully bright */

	return
		Adv_Lit(x - 1, y, z - 1, cIndex - 1 - 18) << xM1_yM1_zM1 |
		Adv_Lit(x - 1, y, z,     cIndex - 1)      << xM1_yM1_zCC |
		Adv_Lit(x - 1, y, z + 1, cIndex - 1 + 18) << xM1_yM1_zP1 |
		Adv_Lit(x,     y, z - 1, cIndex + 0 - 18) << xCC_yM1_zM1 |
		Adv_Lit(x,     y, z,     cIndex + 0)      << xCC_yM1_zCC |
		Adv_Lit(x,     y, z + 1, cIndex + 0 + 18) << xCC_yM1_zP1 |
		Adv_Lit(x + 1, y, z - 1, cIndex + 1 - 18) << xP1_yM1_zM1 |
		Adv_Lit(x + 1, y, z,     cIndex + 1)      << xP1_yM1_zCC |
		Adv_Lit(x + 1, y, z + 1, cIndex + 1 + 18) << xP1_yM1_zP1;
}

static int adv_masks[FACE_COUNT] = {
	/* XMin face */
	(1 << xM1_yM1_zM1) | (1 << xM1_yM1_zCC) | (1 << xM1_yM1_zP1) |
	(1 << xM1_yCC_zM1) | (1 << xM1_yCC_zCC) | (1 << xM1_yCC_zP1) |
	(1 << xM1_yP1_zM1) | (1 << xM1_yP1_zCC) | (1 << xM1_yP1_zP1),
	/* XMax face */
	(1 << xP1_yM1_zM1) | (1 << xP1_yM1_zCC) | (1 << xP1_yM1_zP1) |
	(1 << xP1_yP1_zM1) | (1 << xP1_yP1_zCC) | (1 << xP1_yP1_zP1) |
	(1 << xP1_yCC_zM1) | (1 << xP1_yCC_zCC) | (1 << xP1_yCC_zP1),
	/* ZMin face */
	(1 << xM1_yM1_zM1) | (1 << xCC_yM1_zM1) | (1 << xP1_yM1_zM1) |
	(1 << xM1_yCC_zM1) | (1 << xCC_yCC_zM1) | (1 << xP1_yCC_zM1) |
	(1 << xM1_yP1_zM1) | (1 << xCC_yP1_zM1) | (1 << xP1_yP1_zM1),
	/* ZMax face */
	(1 << xM1_yM1_zP1) | (1 << xCC_yM1_zP1) | (1 << xP1_yM1_zP1) |
	(1 << xM1_yCC_zP1) | (1 << xCC_yCC_zP1) | (1 << xP1_yCC_zP1) |
	(1 << xM1_yP1_zP1) | (1 << xCC_yP1_zP1) | (1 << xP1_yP1_zP1),
	/* YMin face */
	(1 << xM1_yM1_zM1) | (1 << xM1_yM1_zCC) | (1 << xM1_yM1_zP1) |
	(1 << xCC_yM1_zM1) | (1 << xCC_yM1_zCC) | (1 << xCC_yM1_zP1) |
	(1 << xP1_yM1_zM1) | (1 << xP1_yM1_zCC) | (1 << xP1_yM1_zP1),
	/* YMax face */
	(1 << xM1_yP1_zM1) | (1 << xM1_yP1_zCC) | (1 << xM1_yP1_zP1) |
	(1 << xCC_yP1_zM1) | (1 << xCC_yP1_zCC) | (1 << xCC_yP1_zP1) |
	(1 << xP1_yP1_zM1) | (1 << xP1_yP1_zCC) | (1 << xP1_yP1_zP1),
};


static cc_bool Adv_CanStretch(BlockID initial, int chunkIndex, int x, int y, int z, Face face) {
	BlockID cur = Builder_Chunk[chunkIndex];
	adv_bitFlags[chunkIndex] = Adv_ComputeLightFlags(x, y, z, chunkIndex);

	if (cur != initial) return false;
	/* Grass blocks with different snow state above have different textures, can't merge */
	if (cur == BLOCK_GRASS && HasSnowAbove(adv_blockX, adv_blockY, adv_blockZ) != HasSnowAbove(x, y, z)) return false;
	return !Block_IsFaceHidden(cur, Builder_Chunk[chunkIndex + Builder_Offsets[face]], face)
		&& (adv_initBitFlags == adv_bitFlags[chunkIndex]
		/* Check that this face is either fully bright or fully in shadow */
		&& (adv_initBitFlags == 0 || (adv_initBitFlags & adv_masks[face]) == adv_masks[face]));
}

static int Adv_StretchXLiquid(int countIndex, int x, int y, int z, int chunkIndex, BlockID block) {
	int count = 1; cc_bool stretchTile;
	if (Builder_OccludedLiquid(chunkIndex)) return 0;
	adv_initBitFlags = Adv_ComputeLightFlags(x, y, z, chunkIndex);
	adv_bitFlags[chunkIndex] = adv_initBitFlags;

	/* Finite liquid uses per-corner heights, never stretch */
	if (Builder_IsFiniteLiquid(block)) {
		AddVerticesFiniteLiquid(block, FACE_YMAX);
		return count;
	}

	x++;
	chunkIndex++;
	countIndex += FACE_COUNT;
	stretchTile = (Blocks.CanStretch[block] & (1 << FACE_YMAX)) != 0;

	while (x < Builder_ChunkEndX && stretchTile && Adv_CanStretch(block, chunkIndex, x, y, z, FACE_YMAX) && !Builder_OccludedLiquid(chunkIndex)) {
		Builder_Counts[countIndex] = 0;
		count++;
		x++;
		chunkIndex++;
		countIndex += FACE_COUNT;
	}
	AddVertices(block, FACE_YMAX);
	return count;
}

static int Adv_StretchX(int countIndex, int x, int y, int z, int chunkIndex, BlockID block, Face face) {
	int count = 1; cc_bool stretchTile;
	adv_initBitFlags = Adv_ComputeLightFlags(x, y, z, chunkIndex);
	adv_bitFlags[chunkIndex] = adv_initBitFlags;
	/* Finite liquid uses per-corner heights, never stretch */
	if (Builder_IsFiniteLiquid(block)) {
		AddVerticesFiniteLiquid(block, face);
		return count;
	}
	
	x++;
	chunkIndex++;
	countIndex += FACE_COUNT;
	stretchTile = (Blocks.CanStretch[block] & (1 << face)) != 0;

	while (x < Builder_ChunkEndX && stretchTile && Adv_CanStretch(block, chunkIndex, x, y, z, face)) {
		Builder_Counts[countIndex] = 0;
		count++;
		x++;
		chunkIndex++;
		countIndex += FACE_COUNT;
	}
	AddVertices(block, face);
	return count;
}

static int Adv_StretchZ(int countIndex, int x, int y, int z, int chunkIndex, BlockID block, Face face) {
	int count = 1; cc_bool stretchTile;
	adv_initBitFlags = Adv_ComputeLightFlags(x, y, z, chunkIndex);
	adv_bitFlags[chunkIndex] = adv_initBitFlags;
	/* Finite liquid uses per-corner heights, never stretch */
	if (Builder_IsFiniteLiquid(block)) {
		AddVerticesFiniteLiquid(block, face);
		return count;
	}

	z++;
	chunkIndex += EXTCHUNK_SIZE;
	countIndex += CHUNK_SIZE * FACE_COUNT;
	stretchTile = (Blocks.CanStretch[block] & (1 << face)) != 0;

	while (z < Builder_ChunkEndZ && stretchTile && Adv_CanStretch(block, chunkIndex, x, y, z, face)) {
		Builder_Counts[countIndex] = 0;
		count++;
		z++;
		chunkIndex += EXTCHUNK_SIZE;
		countIndex += CHUNK_SIZE * FACE_COUNT;
	}
	AddVertices(block, face);
	return count;
}


#define Adv_CountBits(F, a, b, c, d) (((F >> a) & 1) + ((F >> b) & 1) + ((F >> c) & 1) + ((F >> d) & 1))

static void Adv_DrawXMin(int count) {
	TextureLoc texLoc = DirectionalBlock_GetTexture(Builder_Block, adv_blockX, adv_blockY, adv_blockZ, FACE_XMIN);
	float vOrigin = Atlas1D_RowId(texLoc) * Atlas1D.InvTileSize;

	float u1 = adv_minBB.z, u2 = (count - 1) + adv_maxBB.z * UV2_Scale;
	float v1 = vOrigin + adv_maxBB.y * Atlas1D.InvTileSize;
	float v2 = vOrigin + adv_minBB.y * Atlas1D.InvTileSize * UV2_Scale;
	struct Builder1DPart* part = &Builder_Parts[adv_baseOffset + Atlas1D_Index(texLoc)];

	int F = adv_bitFlags[Builder_ChunkIndex];
	int aY0_Z0 = Adv_CountBits(F, xM1_yM1_zM1, xM1_yCC_zM1, xM1_yM1_zCC, xM1_yCC_zCC);
	int aY0_Z1 = Adv_CountBits(F, xM1_yM1_zP1, xM1_yCC_zP1, xM1_yM1_zCC, xM1_yCC_zCC);
	int aY1_Z0 = Adv_CountBits(F, xM1_yP1_zM1, xM1_yCC_zM1, xM1_yP1_zCC, xM1_yCC_zCC);
	int aY1_Z1 = Adv_CountBits(F, xM1_yP1_zP1, xM1_yCC_zP1, xM1_yP1_zCC, xM1_yCC_zCC);

	PackedCol tint, white = PACKEDCOL_WHITE;
	PackedCol col0_0 = Builder_FullBright ? white : adv_lerpX[aY0_Z0], col1_0 = Builder_FullBright ? white : adv_lerpX[aY1_Z0];
	PackedCol col1_1 = Builder_FullBright ? white : adv_lerpX[aY1_Z1], col0_1 = Builder_FullBright ? white : adv_lerpX[aY0_Z1];
	struct VertexTextured* vertices, v;

	if (adv_tinted) {
		tint   = Blocks.FogCol[Builder_Block];
		col0_0 = PackedCol_Tint(col0_0, tint); col1_0 = PackedCol_Tint(col1_0, tint);
		col1_1 = PackedCol_Tint(col1_1, tint); col0_1 = PackedCol_Tint(col0_1, tint);
	}

	vertices = part->faces.vertices[FACE_XMIN];
	v.x = adv_x1;
	if (aY0_Z0 + aY1_Z1 > aY0_Z1 + aY1_Z0) {
		v.y = adv_y2; v.z = adv_z1;               v.U = u1; v.V = v1; v.Col = col1_0; *vertices++ = v;
		v.y = adv_y1;                                       v.V = v2; v.Col = col0_0; *vertices++ = v;
		              v.z = adv_z2 + (count - 1); v.U = u2;           v.Col = col0_1; *vertices++ = v;
		v.y = adv_y2;                                       v.V = v1; v.Col = col1_1; *vertices++ = v;
	} else {
		v.y = adv_y2; v.z = adv_z2 + (count - 1); v.U = u2; v.V = v1; v.Col = col1_1; *vertices++ = v;
		              v.z = adv_z1;               v.U = u1;           v.Col = col1_0; *vertices++ = v;
		v.y = adv_y1;                                       v.V = v2; v.Col = col0_0; *vertices++ = v;
		              v.z = adv_z2 + (count - 1); v.U = u2;           v.Col = col0_1; *vertices++ = v;
	}
	part->faces.vertices[FACE_XMIN] = vertices;
}

static void Adv_DrawXMax(int count) {
	TextureLoc texLoc = DirectionalBlock_GetTexture(Builder_Block, adv_blockX, adv_blockY, adv_blockZ, FACE_XMAX);
	float vOrigin = Atlas1D_RowId(texLoc) * Atlas1D.InvTileSize;

	float u1 = (count - adv_minBB.z), u2 = (1 - adv_maxBB.z) * UV2_Scale;
	float v1 = vOrigin + adv_maxBB.y * Atlas1D.InvTileSize;
	float v2 = vOrigin + adv_minBB.y * Atlas1D.InvTileSize * UV2_Scale;
	struct Builder1DPart* part = &Builder_Parts[adv_baseOffset + Atlas1D_Index(texLoc)];

	int F = adv_bitFlags[Builder_ChunkIndex];
	int aY0_Z0 = Adv_CountBits(F, xP1_yM1_zM1, xP1_yCC_zM1, xP1_yM1_zCC, xP1_yCC_zCC);
	int aY0_Z1 = Adv_CountBits(F, xP1_yM1_zP1, xP1_yCC_zP1, xP1_yM1_zCC, xP1_yCC_zCC);
	int aY1_Z0 = Adv_CountBits(F, xP1_yP1_zM1, xP1_yCC_zM1, xP1_yP1_zCC, xP1_yCC_zCC);
	int aY1_Z1 = Adv_CountBits(F, xP1_yP1_zP1, xP1_yCC_zP1, xP1_yP1_zCC, xP1_yCC_zCC);

	PackedCol tint, white = PACKEDCOL_WHITE;
	PackedCol col0_0 = Builder_FullBright ? white : adv_lerpX[aY0_Z0], col1_0 = Builder_FullBright ? white : adv_lerpX[aY1_Z0];
	PackedCol col1_1 = Builder_FullBright ? white : adv_lerpX[aY1_Z1], col0_1 = Builder_FullBright ? white : adv_lerpX[aY0_Z1];
	struct VertexTextured* vertices, v;

	if (adv_tinted) {
		tint   = Blocks.FogCol[Builder_Block];
		col0_0 = PackedCol_Tint(col0_0, tint); col1_0 = PackedCol_Tint(col1_0, tint);
		col1_1 = PackedCol_Tint(col1_1, tint); col0_1 = PackedCol_Tint(col0_1, tint);
	}

	vertices = part->faces.vertices[FACE_XMAX];
	v.x = adv_x2;
	if (aY0_Z0 + aY1_Z1 > aY0_Z1 + aY1_Z0) {
		v.y = adv_y2; v.z = adv_z1;               v.U = u1; v.V = v1; v.Col = col1_0; *vertices++ = v;
		              v.z = adv_z2 + (count - 1); v.U = u2;           v.Col = col1_1; *vertices++ = v;
		v.y = adv_y1;                                       v.V = v2; v.Col = col0_1; *vertices++ = v;
		              v.z = adv_z1;               v.U = u1;           v.Col = col0_0; *vertices++ = v;
	} else {
		v.y = adv_y2; v.z = adv_z2 + (count - 1); v.U = u2; v.V = v1; v.Col = col1_1; *vertices++ = v;
		v.y = adv_y1;                                       v.V = v2; v.Col = col0_1; *vertices++ = v;
		              v.z = adv_z1;               v.U = u1;           v.Col = col0_0; *vertices++ = v;
		v.y = adv_y2;                                       v.V = v1; v.Col = col1_0; *vertices++ = v;
	}
	part->faces.vertices[FACE_XMAX] = vertices;
}

static void Adv_DrawZMin(int count) {
	TextureLoc texLoc = DirectionalBlock_GetTexture(Builder_Block, adv_blockX, adv_blockY, adv_blockZ, FACE_ZMIN);
	float vOrigin = Atlas1D_RowId(texLoc) * Atlas1D.InvTileSize;

	float u1 = (count - adv_minBB.x), u2 = (1 - adv_maxBB.x) * UV2_Scale;
	float v1 = vOrigin + adv_maxBB.y * Atlas1D.InvTileSize;
	float v2 = vOrigin + adv_minBB.y * Atlas1D.InvTileSize * UV2_Scale;
	struct Builder1DPart* part = &Builder_Parts[adv_baseOffset + Atlas1D_Index(texLoc)];

	int F = adv_bitFlags[Builder_ChunkIndex];
	int aX0_Y0 = Adv_CountBits(F, xM1_yM1_zM1, xM1_yCC_zM1, xCC_yM1_zM1, xCC_yCC_zM1);
	int aX0_Y1 = Adv_CountBits(F, xM1_yP1_zM1, xM1_yCC_zM1, xCC_yP1_zM1, xCC_yCC_zM1);
	int aX1_Y0 = Adv_CountBits(F, xP1_yM1_zM1, xP1_yCC_zM1, xCC_yM1_zM1, xCC_yCC_zM1);
	int aX1_Y1 = Adv_CountBits(F, xP1_yP1_zM1, xP1_yCC_zM1, xCC_yP1_zM1, xCC_yCC_zM1);

	PackedCol tint, white = PACKEDCOL_WHITE;
	PackedCol col0_0 = Builder_FullBright ? white : adv_lerpZ[aX0_Y0], col1_0 = Builder_FullBright ? white : adv_lerpZ[aX1_Y0];
	PackedCol col1_1 = Builder_FullBright ? white : adv_lerpZ[aX1_Y1], col0_1 = Builder_FullBright ? white : adv_lerpZ[aX0_Y1];
	struct VertexTextured* vertices, v;

	if (adv_tinted) {
		tint   = Blocks.FogCol[Builder_Block];
		col0_0 = PackedCol_Tint(col0_0, tint); col1_0 = PackedCol_Tint(col1_0, tint);
		col1_1 = PackedCol_Tint(col1_1, tint); col0_1 = PackedCol_Tint(col0_1, tint);
	}

	vertices = part->faces.vertices[FACE_ZMIN];
	v.z = adv_z1;
	if (aX1_Y1 + aX0_Y0 > aX0_Y1 + aX1_Y0) {
		v.x = adv_x2 + (count - 1); v.y = adv_y1; v.U = u2; v.V = v2; v.Col = col1_0; *vertices++ = v;
		v.x = adv_x1;                             v.U = u1;           v.Col = col0_0; *vertices++ = v;
		                            v.y = adv_y2;           v.V = v1; v.Col = col0_1; *vertices++ = v;
		v.x = adv_x2 + (count - 1);               v.U = u2;           v.Col = col1_1; *vertices++ = v;
	} else {
		v.x = adv_x1;               v.y = adv_y1; v.U = u1; v.V = v2; v.Col = col0_0; *vertices++ = v;
		                            v.y = adv_y2;           v.V = v1; v.Col = col0_1; *vertices++ = v;
		v.x = adv_x2 + (count - 1);               v.U = u2;           v.Col = col1_1; *vertices++ = v;
		                            v.y = adv_y1;           v.V = v2; v.Col = col1_0; *vertices++ = v;
	}
	part->faces.vertices[FACE_ZMIN] = vertices;
}

static void Adv_DrawZMax(int count) {
	TextureLoc texLoc = DirectionalBlock_GetTexture(Builder_Block, adv_blockX, adv_blockY, adv_blockZ, FACE_ZMAX);
	float vOrigin = Atlas1D_RowId(texLoc) * Atlas1D.InvTileSize;

	float u1 = adv_minBB.x, u2 = (count - 1) + adv_maxBB.x * UV2_Scale;
	float v1 = vOrigin + adv_maxBB.y * Atlas1D.InvTileSize;
	float v2 = vOrigin + adv_minBB.y * Atlas1D.InvTileSize * UV2_Scale;
	struct Builder1DPart* part = &Builder_Parts[adv_baseOffset + Atlas1D_Index(texLoc)];

	int F = adv_bitFlags[Builder_ChunkIndex];
	int aX0_Y0 = Adv_CountBits(F, xM1_yM1_zP1, xM1_yCC_zP1, xCC_yM1_zP1, xCC_yCC_zP1);
	int aX1_Y0 = Adv_CountBits(F, xP1_yM1_zP1, xP1_yCC_zP1, xCC_yM1_zP1, xCC_yCC_zP1);
	int aX0_Y1 = Adv_CountBits(F, xM1_yP1_zP1, xM1_yCC_zP1, xCC_yP1_zP1, xCC_yCC_zP1);
	int aX1_Y1 = Adv_CountBits(F, xP1_yP1_zP1, xP1_yCC_zP1, xCC_yP1_zP1, xCC_yCC_zP1);

	PackedCol tint, white = PACKEDCOL_WHITE;
	PackedCol col1_1 = Builder_FullBright ? white : adv_lerpZ[aX1_Y1], col1_0 = Builder_FullBright ? white : adv_lerpZ[aX1_Y0];
	PackedCol col0_0 = Builder_FullBright ? white : adv_lerpZ[aX0_Y0], col0_1 = Builder_FullBright ? white : adv_lerpZ[aX0_Y1];
	struct VertexTextured* vertices, v;

	if (adv_tinted) {
		tint   = Blocks.FogCol[Builder_Block];
		col0_0 = PackedCol_Tint(col0_0, tint); col1_0 = PackedCol_Tint(col1_0, tint);
		col1_1 = PackedCol_Tint(col1_1, tint); col0_1 = PackedCol_Tint(col0_1, tint);
	}

	vertices = part->faces.vertices[FACE_ZMAX];
	v.z = adv_z2;
	if (aX1_Y1 + aX0_Y0 > aX0_Y1 + aX1_Y0) {
		v.x = adv_x1;               v.y = adv_y2; v.U = u1; v.V = v1; v.Col = col0_1; *vertices++ = v;
		                            v.y = adv_y1;           v.V = v2; v.Col = col0_0; *vertices++ = v;
		v.x = adv_x2 + (count - 1);               v.U = u2;           v.Col = col1_0; *vertices++ = v;
		                            v.y = adv_y2;           v.V = v1; v.Col = col1_1; *vertices++ = v;
	} else {
		v.x = adv_x2 + (count - 1); v.y = adv_y2; v.U = u2; v.V = v1; v.Col = col1_1; *vertices++ = v;
		v.x = adv_x1;                             v.U = u1;           v.Col = col0_1; *vertices++ = v;
		                            v.y = adv_y1;           v.V = v2; v.Col = col0_0; *vertices++ = v;
		v.x = adv_x2 + (count - 1);               v.U = u2;           v.Col = col1_0; *vertices++ = v;
	}
	part->faces.vertices[FACE_ZMAX] = vertices;
}

static void Adv_DrawYMin(int count) {
	TextureLoc texLoc = DirectionalBlock_GetTexture(Builder_Block, adv_blockX, adv_blockY, adv_blockZ, FACE_YMIN);
	float vOrigin = Atlas1D_RowId(texLoc) * Atlas1D.InvTileSize;

	float u1 = adv_minBB.x, u2 = (count - 1) + adv_maxBB.x * UV2_Scale;
	float v1 = vOrigin + adv_minBB.z * Atlas1D.InvTileSize;
	float v2 = vOrigin + adv_maxBB.z * Atlas1D.InvTileSize * UV2_Scale;
	struct Builder1DPart* part = &Builder_Parts[adv_baseOffset + Atlas1D_Index(texLoc)];

	int F = adv_bitFlags[Builder_ChunkIndex];
	int aX0_Z0 = Adv_CountBits(F, xM1_yM1_zM1, xM1_yM1_zCC, xCC_yM1_zM1, xCC_yM1_zCC);
	int aX1_Z0 = Adv_CountBits(F, xP1_yM1_zM1, xP1_yM1_zCC, xCC_yM1_zM1, xCC_yM1_zCC);
	int aX0_Z1 = Adv_CountBits(F, xM1_yM1_zP1, xM1_yM1_zCC, xCC_yM1_zP1, xCC_yM1_zCC);
	int aX1_Z1 = Adv_CountBits(F, xP1_yM1_zP1, xP1_yM1_zCC, xCC_yM1_zP1, xCC_yM1_zCC);

	PackedCol tint, white = PACKEDCOL_WHITE;
	PackedCol col0_1 = Builder_FullBright ? white : adv_lerpY[aX0_Z1], col1_1 = Builder_FullBright ? white : adv_lerpY[aX1_Z1];
	PackedCol col1_0 = Builder_FullBright ? white : adv_lerpY[aX1_Z0], col0_0 = Builder_FullBright ? white : adv_lerpY[aX0_Z0];
	struct VertexTextured* vertices, v;

	if (adv_tinted) {
		tint   = Blocks.FogCol[Builder_Block];
		col0_0 = PackedCol_Tint(col0_0, tint); col1_0 = PackedCol_Tint(col1_0, tint);
		col1_1 = PackedCol_Tint(col1_1, tint); col0_1 = PackedCol_Tint(col0_1, tint);
	}

	vertices = part->faces.vertices[FACE_YMIN];
	v.y = adv_y1;
	if (aX0_Z1 + aX1_Z0 > aX0_Z0 + aX1_Z1) {
		v.x = adv_x2 + (count - 1); v.z = adv_z2; v.U = u2; v.V = v2; v.Col = col1_1; *vertices++ = v;
		v.x = adv_x1;                             v.U = u1;           v.Col = col0_1; *vertices++ = v;
		                            v.z = adv_z1;           v.V = v1; v.Col = col0_0; *vertices++ = v;
		v.x = adv_x2 + (count - 1);               v.U = u2;           v.Col = col1_0; *vertices++ = v;
	} else {
		v.x = adv_x1;               v.z = adv_z2; v.U = u1; v.V = v2; v.Col = col0_1; *vertices++ = v;
		                            v.z = adv_z1;           v.V = v1; v.Col = col0_0; *vertices++ = v;
		v.x = adv_x2 + (count - 1);               v.U = u2;           v.Col = col1_0; *vertices++ = v;
		                            v.z = adv_z2;           v.V = v2; v.Col = col1_1; *vertices++ = v;
	}
	part->faces.vertices[FACE_YMIN] = vertices;
}

static void Adv_DrawYMax(int count) {
	TextureLoc texLoc = DirectionalBlock_GetTexture(Builder_Block, adv_blockX, adv_blockY, adv_blockZ, FACE_YMAX);
	float vOrigin = Atlas1D_RowId(texLoc) * Atlas1D.InvTileSize;

	float u1 = adv_minBB.x, u2 = (count - 1) + adv_maxBB.x * UV2_Scale;
	float v1 = vOrigin + adv_minBB.z * Atlas1D.InvTileSize;
	float v2 = vOrigin + adv_maxBB.z * Atlas1D.InvTileSize * UV2_Scale;
	struct Builder1DPart* part = &Builder_Parts[adv_baseOffset + Atlas1D_Index(texLoc)];

	int F = adv_bitFlags[Builder_ChunkIndex];
	int aX0_Z0 = Adv_CountBits(F, xM1_yP1_zM1, xM1_yP1_zCC, xCC_yP1_zM1, xCC_yP1_zCC);
	int aX1_Z0 = Adv_CountBits(F, xP1_yP1_zM1, xP1_yP1_zCC, xCC_yP1_zM1, xCC_yP1_zCC);
	int aX0_Z1 = Adv_CountBits(F, xM1_yP1_zP1, xM1_yP1_zCC, xCC_yP1_zP1, xCC_yP1_zCC);
	int aX1_Z1 = Adv_CountBits(F, xP1_yP1_zP1, xP1_yP1_zCC, xCC_yP1_zP1, xCC_yP1_zCC);

	PackedCol tint, white = PACKEDCOL_WHITE;
	PackedCol col0_0 = Builder_FullBright ? white : adv_lerp[aX0_Z0], col1_0 = Builder_FullBright ? white : adv_lerp[aX1_Z0];
	PackedCol col1_1 = Builder_FullBright ? white : adv_lerp[aX1_Z1], col0_1 = Builder_FullBright ? white : adv_lerp[aX0_Z1];
	struct VertexTextured* vertices, v;

	if (adv_tinted) {
		tint   = Blocks.FogCol[Builder_Block];
		col0_0 = PackedCol_Tint(col0_0, tint); col1_0 = PackedCol_Tint(col1_0, tint);
		col1_1 = PackedCol_Tint(col1_1, tint); col0_1 = PackedCol_Tint(col0_1, tint);
	}

	vertices = part->faces.vertices[FACE_YMAX];
	v.y = adv_y2;
	if (aX0_Z0 + aX1_Z1 > aX0_Z1 + aX1_Z0) {
		v.x = adv_x2 + (count - 1); v.z = adv_z1; v.U = u2; v.V = v1; v.Col = col1_0; *vertices++ = v;
		v.x = adv_x1;                             v.U = u1;           v.Col = col0_0; *vertices++ = v;
		                            v.z = adv_z2;           v.V = v2; v.Col = col0_1; *vertices++ = v;
		v.x = adv_x2 + (count - 1);               v.U = u2;           v.Col = col1_1; *vertices++ = v;
	} else {
		v.x = adv_x1;               v.z = adv_z1; v.U = u1; v.V = v1; v.Col = col0_0; *vertices++ = v;
		                            v.z = adv_z2;           v.V = v2; v.Col = col0_1; *vertices++ = v;
		v.x = adv_x2 + (count - 1);               v.U = u2;           v.Col = col1_1; *vertices++ = v;
		                            v.z = adv_z1;           v.V = v1; v.Col = col1_0; *vertices++ = v;
	}
	part->faces.vertices[FACE_YMAX] = vertices;
}

static void Adv_RenderBlock(int index, int x, int y, int z) {
	Vec3 min, max;
	int count_XMin, count_XMax, count_ZMin;
	int count_ZMax, count_YMin, count_YMax;

	if (Blocks.Draw[Builder_Block] == DRAW_SPRITE) {
		if (IsAnyTorchBlock(Builder_Block)) { Builder_DrawTorch(x, y, z); return; }
		if (IsCropBlock(Builder_Block))     { Builder_DrawCropSprite(x, y, z); return; }
		Builder_DrawSprite(x, y, z); return;
	}

	if (Builder_IsFiniteLiquid(Builder_Block)) {
		Builder_RenderFiniteLiquid(index, x, y, z); return;
	}

	/* Rails use custom rendering with UV rotation for the top face */
	if (IsRailBlock(Builder_Block)) {
		Builder_DrawRail(index, x, y, z); return;
	}

	/* Stairs use custom rendering as two sub-blocks */
	if (IsStairBlock(Builder_Block)) {
		Builder_DrawStairs(x, y, z); return;
	}

	count_XMin = Builder_Counts[index + FACE_XMIN];
	count_XMax = Builder_Counts[index + FACE_XMAX];
	count_ZMin = Builder_Counts[index + FACE_ZMIN];
	count_ZMax = Builder_Counts[index + FACE_ZMAX];
	count_YMin = Builder_Counts[index + FACE_YMIN];
	count_YMax = Builder_Counts[index + FACE_YMAX];

	if (!count_XMin && !count_XMax && !count_ZMin &&
		!count_ZMax && !count_YMin && !count_YMax) return;

	Builder_FullBright = Blocks.Brightness[Builder_Block];
	adv_baseOffset = (Blocks.Draw[Builder_Block] == DRAW_TRANSLUCENT) * ATLAS1D_MAX_ATLASES;
	adv_tinted     = Blocks.Tinted[Builder_Block];

	DirectionalBlock_GetRenderBounds(Builder_Block, x, y, z, &min, &max);
	adv_x1 = x + min.x; adv_y1 = y + min.y; adv_z1 = z + min.z;
	adv_x2 = x + max.x; adv_y2 = y + max.y; adv_z2 = z + max.z;

	adv_minBB = Blocks.MinBB[Builder_Block]; adv_maxBB = Blocks.MaxBB[Builder_Block];
	adv_minBB.y = 1.0f - adv_minBB.y; adv_maxBB.y = 1.0f - adv_maxBB.y;

	/* For buttons/levers/pressure plates, use dynamic bounds for UV mapping */
	if (Builder_Block == BLOCK_BUTTON || Builder_Block == BLOCK_BUTTON_PRESSED
		|| IsLeverBlock(Builder_Block)
		|| Builder_Block == BLOCK_PRESSURE_PLATE || Builder_Block == BLOCK_PRESSURE_PLATE_PRESSED) {
		adv_minBB = min; adv_minBB.y = 1.0f - adv_minBB.y;
		adv_maxBB = max; adv_maxBB.y = 1.0f - adv_maxBB.y;
	}

	adv_blockX = x; adv_blockY = y; adv_blockZ = z;

	if (count_XMin) Adv_DrawXMin(count_XMin);
	if (count_XMax) Adv_DrawXMax(count_XMax);
	if (count_ZMin) Adv_DrawZMin(count_ZMin);
	if (count_ZMax) Adv_DrawZMax(count_ZMax);
	if (count_YMin) Adv_DrawYMin(count_YMin);
	if (count_YMax) Adv_DrawYMax(count_YMax);
	
	/* Draw lever handle sprite after base box faces */
	if (IsLeverBlock(Builder_Block)) {
		Builder_DrawLeverHandle(x, y, z);
	}
}

static void Adv_PrePrepareChunk(void) {
	int i;
	DefaultPrePrepateChunk();
	adv_bitFlags = Builder_BitFlags;

	for (i = 0; i <= 4; i++) {
		adv_lerp[i]  = PackedCol_Lerp(Env.ShadowCol,   Env.SunCol,   i / 4.0f);
		adv_lerpX[i] = PackedCol_Lerp(Env.ShadowXSide, Env.SunXSide, i / 4.0f);
		adv_lerpZ[i] = PackedCol_Lerp(Env.ShadowZSide, Env.SunZSide, i / 4.0f);
		adv_lerpY[i] = PackedCol_Lerp(Env.ShadowYMin,  Env.SunYMin,  i / 4.0f);
	}
}

static void AdvBuilder_SetActive(void) {
	Builder_SetDefault();
	Builder_StretchXLiquid  = Adv_StretchXLiquid;
	Builder_StretchX        = Adv_StretchX;
	Builder_StretchZ        = Adv_StretchZ;
	Builder_RenderBlock     = Adv_RenderBlock;
	Builder_PrePrepareChunk = Adv_PrePrepareChunk;
}
#else
static void AdvBuilder_SetActive(void) { NormalBuilder_SetActive(); }
#endif



/*########################################################################################################################*
*-------------------------------------------------Modern mesh builder-----------------------------------------------------*
*#########################################################################################################################*/
#ifdef CC_BUILD_ADVLIGHTING
/* Fast color averaging wizardy from https://stackoverflow.com/questions/8440631/how-would-you-average-two-32-bit-colors-packed-into-an-integer */
#define AVERAGE(a, b)   ( ((((a) ^ (b)) & 0xfefefefe) >> 1) + ((a) & (b)) )

static cc_bool Modern_IsOccluded(int x, int y, int z) {
	BlockID block = World_SafeGetBlock(x, y, z);
	if (Blocks.Brightness[block] > 0) { return false; }
	/* If the block we're pulling colors from is solid, return a darker version of original and increment how many are like this */
	if (Blocks.FullOpaque[block] || (Blocks.Draw[block] == DRAW_TRANSPARENT && Blocks.BlocksLight[block] && Blocks.LightOffset[block] == 0xFF)) {
		return true;
	}
	return false;
}

static cc_bool Modern_CanStretch(BlockID initial, int chunkIndex, int x, int y, int z, Face face) {
	return false;
}

static int Modern_StretchXLiquid(int countIndex, int x, int y, int z, int chunkIndex, BlockID block) {
	int count = 1;
	if (Builder_OccludedLiquid(chunkIndex)) return 0;
	if (Builder_IsFiniteLiquid(block)) {
		AddVerticesFiniteLiquid(block, FACE_YMAX);
		return count;
	}
	AddVertices(block, FACE_YMAX);
	return count;
}

static int Modern_StretchX(int countIndex, int x, int y, int z, int chunkIndex, BlockID block, Face face) {
	int count = 1;
	if (Builder_IsFiniteLiquid(block)) {
		AddVerticesFiniteLiquid(block, face);
		return count;
	}
	AddVertices(block, face);
	return count;
}

static int Modern_StretchZ(int countIndex, int x, int y, int z, int chunkIndex, BlockID block, Face face) {
	int count = 1;
	if (Builder_IsFiniteLiquid(block)) {
		AddVerticesFiniteLiquid(block, face);
		return count;
	}
	AddVertices(block, face);
	return count;
}

static PackedCol Modern_GetColorX(PackedCol orig, int x, int y, int z, int oY, int oZ) {
	cc_bool xOccluded =  Modern_IsOccluded(x, y + oY, z     );
	cc_bool zOccluded =  Modern_IsOccluded(x, y     , z + oZ);
	cc_bool xzOccluded = Modern_IsOccluded(x, y + oY, z + oZ);

	PackedCol CoX = xOccluded ? PackedCol_Scale(orig, FANCY_AO) : Lighting.Color_XSide_Fast(x, y + oY, z     );
	PackedCol CoZ = zOccluded ? PackedCol_Scale(orig, FANCY_AO) : Lighting.Color_XSide_Fast(x, y     , z + oZ);
	PackedCol CoXoZ = (xzOccluded || (xOccluded && zOccluded)) ? PackedCol_Scale(orig, FANCY_AO) : Lighting.Color_XSide_Fast(x, y + oY, z + oZ);

	PackedCol ab = AVERAGE(CoX, CoZ);
	PackedCol cd = AVERAGE(CoXoZ, orig);
	return AVERAGE(ab, cd);
}
static void Modern_DrawXMin(int count, int x, int y, int z) {
	TextureLoc texLoc = DirectionalBlock_GetTexture(Builder_Block, x, y, z, FACE_XMIN);
	float vOrigin = Atlas1D_RowId(texLoc) * Atlas1D.InvTileSize;

	float u1 = adv_minBB.z, u2 = (count - 1) + adv_maxBB.z * UV2_Scale;
	float v1 = vOrigin + adv_maxBB.y * Atlas1D.InvTileSize;
	float v2 = vOrigin + adv_minBB.y * Atlas1D.InvTileSize * UV2_Scale;
	struct Builder1DPart* part = &Builder_Parts[adv_baseOffset + Atlas1D_Index(texLoc)];

	PackedCol tint, white = PACKEDCOL_WHITE;
	int offset = 1;// (Blocks.LightOffset[Builder_Block] >> FACE_XMIN) & 1;
	PackedCol orig = Lighting.Color_XSide_Fast(x-offset, y, z);
	PackedCol col0_0 = Builder_FullBright ? white : Modern_GetColorX(orig, x-offset, y, z, -1, -1);
	PackedCol col1_0 = Builder_FullBright ? white : Modern_GetColorX(orig, x-offset, y, z, 1, -1);
	PackedCol col1_1 = Builder_FullBright ? white : Modern_GetColorX(orig, x-offset, y, z, 1, 1);
	PackedCol col0_1 = Builder_FullBright ? white : Modern_GetColorX(orig, x-offset, y, z, -1, 1);
	struct VertexTextured* vertices, v;

	if (adv_tinted) {
		tint   = Blocks.FogCol[Builder_Block];
		col0_0 = PackedCol_Tint(col0_0, tint); col1_0 = PackedCol_Tint(col1_0, tint);
		col1_1 = PackedCol_Tint(col1_1, tint); col0_1 = PackedCol_Tint(col0_1, tint);
	}

	vertices = part->faces.vertices[FACE_XMIN];
	v.x = adv_x1;
		v.y = adv_y2; v.z = adv_z2 + (count - 1); v.U = u2; v.V = v1; v.Col = col1_1; *vertices++ = v;
		              v.z = adv_z1;               v.U = u1;           v.Col = col1_0; *vertices++ = v;
		v.y = adv_y1;                                       v.V = v2; v.Col = col0_0; *vertices++ = v;
		              v.z = adv_z2 + (count - 1); v.U = u2;           v.Col = col0_1; *vertices++ = v;
	part->faces.vertices[FACE_XMIN] = vertices;
}

static void Modern_DrawXMax(int count, int x, int y, int z) {
	TextureLoc texLoc = DirectionalBlock_GetTexture(Builder_Block, x, y, z, FACE_XMAX);
	float vOrigin = Atlas1D_RowId(texLoc) * Atlas1D.InvTileSize;

	float u1 = (count - adv_minBB.z), u2 = (1 - adv_maxBB.z) * UV2_Scale;
	float v1 = vOrigin + adv_maxBB.y * Atlas1D.InvTileSize;
	float v2 = vOrigin + adv_minBB.y * Atlas1D.InvTileSize * UV2_Scale;
	struct Builder1DPart* part = &Builder_Parts[adv_baseOffset + Atlas1D_Index(texLoc)];

	PackedCol tint, white = PACKEDCOL_WHITE;
	int offset = 1;// (Blocks.LightOffset[Builder_Block] >> FACE_XMAX) & 1;
	PackedCol orig = Lighting.Color_XSide_Fast(x+offset, y, z);
	PackedCol col0_0 = Builder_FullBright ? white : Modern_GetColorX(orig, x+offset, y, z, -1, -1);
	PackedCol col1_0 = Builder_FullBright ? white : Modern_GetColorX(orig, x+offset, y, z, 1, -1);
	PackedCol col1_1 = Builder_FullBright ? white : Modern_GetColorX(orig, x+offset, y, z, 1, 1);
	PackedCol col0_1 = Builder_FullBright ? white : Modern_GetColorX(orig, x+offset, y, z, -1, 1);
	struct VertexTextured* vertices, v;

	if (adv_tinted) {
		tint   = Blocks.FogCol[Builder_Block];
		col0_0 = PackedCol_Tint(col0_0, tint); col1_0 = PackedCol_Tint(col1_0, tint);
		col1_1 = PackedCol_Tint(col1_1, tint); col0_1 = PackedCol_Tint(col0_1, tint);
	}

	vertices = part->faces.vertices[FACE_XMAX];
	v.x = adv_x2;
		v.y = adv_y2; v.z = adv_z2 + (count - 1); v.U = u2; v.V = v1; v.Col = col1_1; *vertices++ = v;
		v.y = adv_y1;                                       v.V = v2; v.Col = col0_1; *vertices++ = v;
		              v.z = adv_z1;               v.U = u1;           v.Col = col0_0; *vertices++ = v;
		v.y = adv_y2;                                       v.V = v1; v.Col = col1_0; *vertices++ = v;
	part->faces.vertices[FACE_XMAX] = vertices;
}

static PackedCol Modern_GetColorZ(PackedCol orig, int x, int y, int z, int oX, int oY) {
	cc_bool xOccluded  = Modern_IsOccluded(x + oX, y     , z);
	cc_bool zOccluded  = Modern_IsOccluded(x,      y + oY, z);
	cc_bool xzOccluded = Modern_IsOccluded(x + oX, y + oY, z);

	PackedCol CoX   =                                xOccluded ? PackedCol_Scale(orig, FANCY_AO) : Lighting.Color_ZSide_Fast(x + oX, y     , z);
	PackedCol CoZ   =                                zOccluded ? PackedCol_Scale(orig, FANCY_AO) : Lighting.Color_ZSide_Fast(x     , y + oY, z);
	PackedCol CoXoZ = (xzOccluded || (xOccluded && zOccluded)) ? PackedCol_Scale(orig, FANCY_AO) : Lighting.Color_ZSide_Fast(x + oX, y + oY, z);

	PackedCol ab = AVERAGE(CoX, CoZ);
	PackedCol cd = AVERAGE(CoXoZ, orig);
	return AVERAGE(ab, cd);
}
static void Modern_DrawZMin(int count, int x, int y, int z) {
	TextureLoc texLoc = DirectionalBlock_GetTexture(Builder_Block, x, y, z, FACE_ZMIN);
	float vOrigin = Atlas1D_RowId(texLoc) * Atlas1D.InvTileSize;

	float u1 = (count - adv_minBB.x), u2 = (1 - adv_maxBB.x) * UV2_Scale;
	float v1 = vOrigin + adv_maxBB.y * Atlas1D.InvTileSize;
	float v2 = vOrigin + adv_minBB.y * Atlas1D.InvTileSize * UV2_Scale;
	struct Builder1DPart* part = &Builder_Parts[adv_baseOffset + Atlas1D_Index(texLoc)];

	PackedCol tint, white = PACKEDCOL_WHITE;
	int offset = 1;// (Blocks.LightOffset[Builder_Block] >> FACE_ZMIN) & 1;
	PackedCol orig = Lighting.Color_ZSide_Fast(x, y, z-offset);
	PackedCol col0_0 = Builder_FullBright ? white : Modern_GetColorZ(orig, x, y, z-offset, -1, -1);
	PackedCol col1_0 = Builder_FullBright ? white : Modern_GetColorZ(orig, x, y, z-offset, 1, -1);
	PackedCol col1_1 = Builder_FullBright ? white : Modern_GetColorZ(orig, x, y, z-offset, 1, 1);
	PackedCol col0_1 = Builder_FullBright ? white : Modern_GetColorZ(orig, x, y, z-offset, -1, 1);
	struct VertexTextured* vertices, v;

	if (adv_tinted) {
		tint   = Blocks.FogCol[Builder_Block];
		col0_0 = PackedCol_Tint(col0_0, tint); col1_0 = PackedCol_Tint(col1_0, tint);
		col1_1 = PackedCol_Tint(col1_1, tint); col0_1 = PackedCol_Tint(col0_1, tint);
	}

	vertices = part->faces.vertices[FACE_ZMIN];
	v.z = adv_z1;
		v.x = adv_x1;               v.y = adv_y1; v.U = u1; v.V = v2; v.Col = col0_0; *vertices++ = v;
		                            v.y = adv_y2;           v.V = v1; v.Col = col0_1; *vertices++ = v;
		v.x = adv_x2 + (count - 1);               v.U = u2;           v.Col = col1_1; *vertices++ = v;
		                            v.y = adv_y1;           v.V = v2; v.Col = col1_0; *vertices++ = v;
	part->faces.vertices[FACE_ZMIN] = vertices;
}

static void Modern_DrawZMax(int count, int x, int y, int z) {
	TextureLoc texLoc = DirectionalBlock_GetTexture(Builder_Block, x, y, z, FACE_ZMAX);
	float vOrigin = Atlas1D_RowId(texLoc) * Atlas1D.InvTileSize;

	float u1 = adv_minBB.x, u2 = (count - 1) + adv_maxBB.x * UV2_Scale;
	float v1 = vOrigin + adv_maxBB.y * Atlas1D.InvTileSize;
	float v2 = vOrigin + adv_minBB.y * Atlas1D.InvTileSize * UV2_Scale;
	struct Builder1DPart* part = &Builder_Parts[adv_baseOffset + Atlas1D_Index(texLoc)];

	PackedCol tint, white = PACKEDCOL_WHITE;
	int offset = 1;// (Blocks.LightOffset[Builder_Block] >> FACE_ZMAX) & 1;
	PackedCol orig = Lighting.Color_ZSide_Fast(x, y, z+offset);
	PackedCol col0_0 = Builder_FullBright ? white : Modern_GetColorZ(orig, x, y, z+offset, -1, -1);
	PackedCol col1_0 = Builder_FullBright ? white : Modern_GetColorZ(orig, x, y, z+offset, 1, -1);
	PackedCol col1_1 = Builder_FullBright ? white : Modern_GetColorZ(orig, x, y, z+offset, 1, 1);
	PackedCol col0_1 = Builder_FullBright ? white : Modern_GetColorZ(orig, x, y, z+offset, -1, 1);
	struct VertexTextured* vertices, v;

	if (adv_tinted) {
		tint   = Blocks.FogCol[Builder_Block];
		col0_0 = PackedCol_Tint(col0_0, tint); col1_0 = PackedCol_Tint(col1_0, tint);
		col1_1 = PackedCol_Tint(col1_1, tint); col0_1 = PackedCol_Tint(col0_1, tint);
	}

	vertices = part->faces.vertices[FACE_ZMAX];
	v.z = adv_z2;
		v.x = adv_x2 + (count - 1); v.y = adv_y2; v.U = u2; v.V = v1; v.Col = col1_1; *vertices++ = v;
		v.x = adv_x1;                             v.U = u1;           v.Col = col0_1; *vertices++ = v;
		                            v.y = adv_y1;           v.V = v2; v.Col = col0_0; *vertices++ = v;
		v.x = adv_x2 + (count - 1);               v.U = u2;           v.Col = col1_0; *vertices++ = v;
	part->faces.vertices[FACE_ZMAX] = vertices;
}

static PackedCol Modern_GetColorYMin(PackedCol orig, int x, int y, int z, int oX, int oZ) {
	cc_bool xOccluded  = Modern_IsOccluded(x + oX, y, z     );
	cc_bool zOccluded  = Modern_IsOccluded(x,      y, z + oZ);
	cc_bool xzOccluded = Modern_IsOccluded(x + oX, y, z + oZ);

	PackedCol CoX   =                                xOccluded ? PackedCol_Scale(orig, FANCY_AO) : Lighting.Color_YMin_Fast(x + oX, y, z     );
	PackedCol CoZ   =                                zOccluded ? PackedCol_Scale(orig, FANCY_AO) : Lighting.Color_YMin_Fast(x     , y, z + oZ);
	PackedCol CoXoZ = (xzOccluded || (xOccluded && zOccluded)) ? PackedCol_Scale(orig, FANCY_AO) : Lighting.Color_YMin_Fast(x + oX, y, z + oZ);

	PackedCol ab = AVERAGE(CoX, CoZ);
	PackedCol cd = AVERAGE(CoXoZ, orig);
	return AVERAGE(ab, cd);
}
static void Modern_DrawYMin(int count, int x, int y, int z) {
	TextureLoc texLoc = DirectionalBlock_GetTexture(Builder_Block, x, y, z, FACE_YMIN);
	float vOrigin = Atlas1D_RowId(texLoc) * Atlas1D.InvTileSize;

	float u1 = adv_minBB.x, u2 = (count - 1) + adv_maxBB.x * UV2_Scale;
	float v1 = vOrigin + adv_minBB.z * Atlas1D.InvTileSize;
	float v2 = vOrigin + adv_maxBB.z * Atlas1D.InvTileSize * UV2_Scale;
	struct Builder1DPart* part = &Builder_Parts[adv_baseOffset + Atlas1D_Index(texLoc)];

	PackedCol tint, white = PACKEDCOL_WHITE;
	int offset = 1;// (Blocks.LightOffset[Builder_Block] >> FACE_YMIN) & 1;
	PackedCol orig = Lighting.Color_YMin_Fast(x, y-offset, z);
	PackedCol col0_0 = Builder_FullBright ? white : Modern_GetColorYMin(orig, x, y-offset, z, -1, -1);
	PackedCol col1_0 = Builder_FullBright ? white : Modern_GetColorYMin(orig, x, y-offset, z,  1, -1);
	PackedCol col1_1 = Builder_FullBright ? white : Modern_GetColorYMin(orig, x, y-offset, z,  1,  1);
	PackedCol col0_1 = Builder_FullBright ? white : Modern_GetColorYMin(orig, x, y-offset, z, -1,  1);
	struct VertexTextured* vertices, v;

	if (adv_tinted) {
		tint   = Blocks.FogCol[Builder_Block];
		col0_0 = PackedCol_Tint(col0_0, tint); col1_0 = PackedCol_Tint(col1_0, tint);
		col1_1 = PackedCol_Tint(col1_1, tint); col0_1 = PackedCol_Tint(col0_1, tint);
	}

	vertices = part->faces.vertices[FACE_YMIN];
	v.y = adv_y1;
		v.x = adv_x1;               v.z = adv_z2; v.U = u1; v.V = v2; v.Col = col0_1; *vertices++ = v;
		                            v.z = adv_z1;           v.V = v1; v.Col = col0_0; *vertices++ = v;
		v.x = adv_x2 + (count - 1);               v.U = u2;           v.Col = col1_0; *vertices++ = v;
		                            v.z = adv_z2;           v.V = v2; v.Col = col1_1; *vertices++ = v;
	part->faces.vertices[FACE_YMIN] = vertices;
}

static PackedCol Modern_GetColorYMax(PackedCol orig, int x, int y, int z, int oX, int oZ) {
	cc_bool xOccluded  = Modern_IsOccluded(x + oX, y, z     );
	cc_bool zOccluded  = Modern_IsOccluded(x,      y, z + oZ);
	cc_bool xzOccluded = Modern_IsOccluded(x + oX, y, z + oZ);

	PackedCol CoX   =                                xOccluded ? PackedCol_Scale(orig, FANCY_AO) : Lighting.Color(x + oX, y, z     );
	PackedCol CoZ   =                                zOccluded ? PackedCol_Scale(orig, FANCY_AO) : Lighting.Color(x     , y, z + oZ);
	PackedCol CoXoZ = (xzOccluded || (xOccluded && zOccluded)) ? PackedCol_Scale(orig, FANCY_AO) : Lighting.Color(x + oX, y, z + oZ);

	PackedCol ab = AVERAGE(CoX, CoZ);
	PackedCol cd = AVERAGE(CoXoZ, orig);
	return AVERAGE(ab, cd);
}
static void Modern_DrawYMax(int count, int x, int y, int z) {
	TextureLoc texLoc = DirectionalBlock_GetTexture(Builder_Block, x, y, z, FACE_YMAX);
	float vOrigin = Atlas1D_RowId(texLoc) * Atlas1D.InvTileSize;

	float u1 = adv_minBB.x, u2 = (count - 1) + adv_maxBB.x * UV2_Scale;
	float v1 = vOrigin + adv_minBB.z * Atlas1D.InvTileSize;
	float v2 = vOrigin + adv_maxBB.z * Atlas1D.InvTileSize * UV2_Scale;
	struct Builder1DPart* part = &Builder_Parts[adv_baseOffset + Atlas1D_Index(texLoc)];

	PackedCol tint, white = PACKEDCOL_WHITE;
	int offset = 1;// (Blocks.LightOffset[Builder_Block] >> FACE_YMAX) & 1;
	PackedCol orig = Lighting.Color(x, y+offset, z);
	PackedCol col0_0 = Builder_FullBright ? white : Modern_GetColorYMax(orig, x, y+offset, z, -1, -1);
	PackedCol col1_0 = Builder_FullBright ? white : Modern_GetColorYMax(orig, x, y+offset, z,  1, -1);
	PackedCol col1_1 = Builder_FullBright ? white : Modern_GetColorYMax(orig, x, y+offset, z,  1,  1);
	PackedCol col0_1 = Builder_FullBright ? white : Modern_GetColorYMax(orig, x, y+offset, z, -1,  1);

	struct VertexTextured* vertices, v;

	if (adv_tinted) {
		tint   = Blocks.FogCol[Builder_Block];
		col0_0 = PackedCol_Tint(col0_0, tint); col1_0 = PackedCol_Tint(col1_0, tint);
		col1_1 = PackedCol_Tint(col1_1, tint); col0_1 = PackedCol_Tint(col0_1, tint);
	}

	vertices = part->faces.vertices[FACE_YMAX];
	v.y = adv_y2;
		v.x = adv_x1;               v.z = adv_z1; v.U = u1; v.V = v1; v.Col = col0_0; *vertices++ = v;
		                            v.z = adv_z2;           v.V = v2; v.Col = col0_1; *vertices++ = v;
		v.x = adv_x2 + (count - 1);               v.U = u2;           v.Col = col1_1; *vertices++ = v;
		                            v.z = adv_z1;           v.V = v1; v.Col = col1_0; *vertices++ = v;
	part->faces.vertices[FACE_YMAX] = vertices;
}

static void Modern_RenderBlock(int index, int x, int y, int z) {
	Vec3 min, max;
	int count_XMin, count_XMax, count_ZMin;
	int count_ZMax, count_YMin, count_YMax;

	if (Blocks.Draw[Builder_Block] == DRAW_SPRITE) {
		if (IsAnyTorchBlock(Builder_Block)) { Builder_DrawTorch(x, y, z); return; }
		if (IsCropBlock(Builder_Block))     { Builder_DrawCropSprite(x, y, z); return; }
		Builder_DrawSprite(x, y, z); return;
	}

	if (Builder_IsFiniteLiquid(Builder_Block)) {
		Builder_RenderFiniteLiquid(index, x, y, z); return;
	}

	/* Rails use custom rendering with UV rotation for the top face */
	if (IsRailBlock(Builder_Block)) {
		Builder_DrawRail(index, x, y, z); return;
	}

	/* Stairs use custom rendering as two sub-blocks */
	if (IsStairBlock(Builder_Block)) {
		Builder_DrawStairs(x, y, z); return;
	}

	count_XMin = Builder_Counts[index + FACE_XMIN];
	count_XMax = Builder_Counts[index + FACE_XMAX];
	count_ZMin = Builder_Counts[index + FACE_ZMIN];
	count_ZMax = Builder_Counts[index + FACE_ZMAX];
	count_YMin = Builder_Counts[index + FACE_YMIN];
	count_YMax = Builder_Counts[index + FACE_YMAX];

	if (!count_XMin && !count_XMax && !count_ZMin &&
		!count_ZMax && !count_YMin && !count_YMax) return;

	Builder_FullBright = Blocks.Brightness[Builder_Block];
	adv_baseOffset = (Blocks.Draw[Builder_Block] == DRAW_TRANSLUCENT) * ATLAS1D_MAX_ATLASES;
	adv_tinted = Blocks.Tinted[Builder_Block];

	DirectionalBlock_GetRenderBounds(Builder_Block, x, y, z, &min, &max);
	adv_x1 = x + min.x; adv_y1 = y + min.y; adv_z1 = z + min.z;
	adv_x2 = x + max.x; adv_y2 = y + max.y; adv_z2 = z + max.z;

	adv_minBB = Blocks.MinBB[Builder_Block]; adv_maxBB = Blocks.MaxBB[Builder_Block];
	adv_minBB.y = 1.0f - adv_minBB.y; adv_maxBB.y = 1.0f - adv_maxBB.y;

	/* For buttons/levers/pressure plates/signs, use dynamic bounds for UV mapping */
	if (Builder_Block == BLOCK_BUTTON || Builder_Block == BLOCK_BUTTON_PRESSED
		|| IsLeverBlock(Builder_Block)
		|| Builder_Block == BLOCK_PRESSURE_PLATE || Builder_Block == BLOCK_PRESSURE_PLATE_PRESSED
		|| Builder_Block == BLOCK_SIGN_FLOOR) {
		adv_minBB = min; adv_minBB.y = 1.0f - adv_minBB.y;
		adv_maxBB = max; adv_maxBB.y = 1.0f - adv_maxBB.y;
	}

	if (count_XMin) Modern_DrawXMin(count_XMin, x, y, z);
	if (count_XMax) Modern_DrawXMax(count_XMax, x, y, z);
	if (count_ZMin) Modern_DrawZMin(count_ZMin, x, y, z);
	if (count_ZMax) Modern_DrawZMax(count_ZMax, x, y, z);
	if (count_YMin) Modern_DrawYMin(count_YMin, x, y, z);
	if (count_YMax) Modern_DrawYMax(count_YMax, x, y, z);
	
	/* Draw lever handle sprite after base box faces */
	if (IsLeverBlock(Builder_Block)) {
		Builder_DrawLeverHandle(x, y, z);
	}
}

static void Modern_PrePrepareChunk(void) {
	DefaultPrePrepateChunk();
	adv_bitFlags = Builder_BitFlags;
}

static void ModernBuilder_SetActive(void) {
	Builder_SetDefault();
	Builder_StretchXLiquid =  Modern_StretchXLiquid;
	Builder_StretchX =        Modern_StretchX;
	Builder_StretchZ =        Modern_StretchZ;
	Builder_RenderBlock =     Modern_RenderBlock;
	Builder_PrePrepareChunk = Modern_PrePrepareChunk;
}
#else
static void ModernBuilder_SetActive(void) { NormalBuilder_SetActive(); }
#endif

/*########################################################################################################################*
*---------------------------------------------------Builder interface-----------------------------------------------------*
*#########################################################################################################################*/
cc_bool Builder_SmoothLighting;
void Builder_ApplyActive(void) {
	if (Builder_SmoothLighting) {
		if (Lighting_Mode != LIGHTING_MODE_CLASSIC) {
			ModernBuilder_SetActive();
		}
		else {
			AdvBuilder_SetActive();
		}
	} else {
		NormalBuilder_SetActive();
	}
}

static void OnInit(void) {
	Builder_Offsets[FACE_XMIN] = -1;
	Builder_Offsets[FACE_XMAX] =  1;
	Builder_Offsets[FACE_ZMIN] = -EXTCHUNK_SIZE;
	Builder_Offsets[FACE_ZMAX] =  EXTCHUNK_SIZE;
	Builder_Offsets[FACE_YMIN] = -EXTCHUNK_SIZE_2;
	Builder_Offsets[FACE_YMAX] =  EXTCHUNK_SIZE_2;

	if (!Game_ClassicMode) Builder_SmoothLighting = Options_GetBool(OPT_SMOOTH_LIGHTING, false);
	Builder_ApplyActive();
}

static void OnNewMapLoaded(void) {
	Builder_SidesLevel = max(0, Env_SidesHeight);
	Builder_EdgeLevel  = max(0, Env.EdgeHeight);
}

struct IGameComponent Builder_Component = {
	OnInit, /* Init */
	NULL, /* Free */
	NULL, /* Reset */
	NULL, /* OnNewMap */
	OnNewMapLoaded /* OnNewMapLoaded */
};
