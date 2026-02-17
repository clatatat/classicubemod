#include "IsometricDrawer.h"
#include "Drawer.h"
#include "Graphics.h"
#include "PackedCol.h"
#include "ExtMath.h"
#include "Block.h"
#include "TexturePack.h"
#include "Block.h"
#include "Game.h"
#include "Model.h"

extern struct ModelTex items_tex;
#define ISO_STATE_ITEMS_TEX (-1)

static struct VertexTextured* iso_vertices;
static struct VertexTextured* iso_vertices_base;
static int* iso_state;

static cc_bool iso_cacheInited;
static PackedCol iso_colorXSide, iso_colorZSide, iso_colorYBottom;
static float iso_posX, iso_posY;

#define iso_cosX  (0.86602540378443864f) /* cos(30  * MATH_DEG2RAD) */
#define iso_sinX  (0.50000000000000000f) /* sin(30  * MATH_DEG2RAD) */
#define iso_cosY  (0.70710678118654752f) /* cos(-45 * MATH_DEG2RAD) */
#define iso_sinY (-0.70710678118654752f) /* sin(-45 * MATH_DEG2RAD) */

static void IsometricDrawer_InitCache(void) {
	if (iso_cacheInited) return;

	iso_cacheInited = true;
	PackedCol_GetShaded(PACKEDCOL_WHITE,
		&iso_colorXSide, &iso_colorZSide, &iso_colorYBottom);
}

static TextureLoc IsometricDrawer_GetTexLoc(BlockID block, Face face) {
	TextureLoc loc = Block_Tex(block, face);
	*iso_state++   = Atlas1D_Index(loc);
	return loc;
}

static void IsometricDrawer_Flat(BlockID block, float size) {
	int texIndex;
	TextureLoc loc = Block_Tex(block, FACE_ZMAX);
	TextureRec rec = Atlas1D_TexRec(loc, 1, &texIndex);

	struct VertexTextured* v;
	float minX, maxX, minY, maxY;
	PackedCol color;
	float scale;

	*iso_state++ = texIndex;
	color = PACKEDCOL_WHITE;
	Block_Tint(color, block);

	/* Rescale by 0.70 in Classic mode to match vanilla size */
	/* Rescale by 0.88 in Enhanced mode to be slightly nicer */
	/*  Default selected size:  54px -> 48px */
	/*  Default inventory size: 36px -> 32px */
	/*  Default hotbar size:    28px -> 24px */
	scale = Game_ClassicMode ? 0.70f : 0.88f;
	size  = Math_Ceil(size * scale);
	minX  = iso_posX - size; maxX = iso_posX + size;
	minY  = iso_posY - size; maxY = iso_posY + size;

	v = iso_vertices;
	v->x = minX; v->y = minY; v->z = 0; v->Col = color; v->U = rec.u1; v->V = rec.v1; v++;
	v->x = maxX; v->y = minY; v->z = 0; v->Col = color; v->U = rec.u2; v->V = rec.v1; v++;
	v->x = maxX; v->y = maxY; v->z = 0; v->Col = color; v->U = rec.u2; v->V = rec.v2; v++;
	v->x = minX; v->y = maxY; v->z = 0; v->Col = color; v->U = rec.u1; v->V = rec.v2; v++;
	iso_vertices = v;
}

/* Returns items.png tile index for blocks that should render as flat item sprites, -1 otherwise */
static int GetItemTexForBlock(BlockID block) {
	if (block == BLOCK_RED_ORE_DUST)    return 56;
	if (block == BLOCK_DOOR_NS_BOTTOM)  return 43;
	if (block == BLOCK_IRON_DOOR)       return 44;
	return -1;
}

/* Renders a flat quad using a tile from items.png instead of terrain.png */
static void IsometricDrawer_FlatItem(int itemTile, float size) {
	struct VertexTextured* v;
	float minX, maxX, minY, maxY;
	float u1, v1, u2, v2;
	float scale;

	*iso_state++ = ISO_STATE_ITEMS_TEX;

	/* Compute UV coords into items.png (16x16 grid of tiles) */
	u1 = (float)(itemTile % 16) / 16.0f;
	v1 = (float)(itemTile / 16) / 16.0f;
	u2 = u1 + 1.0f / 16.0f;
	v2 = v1 + 1.0f / 16.0f;

	scale = Game_ClassicMode ? 0.70f : 0.88f;
	size  = Math_Ceil(size * scale);
	minX  = iso_posX - size; maxX = iso_posX + size;
	minY  = iso_posY - size; maxY = iso_posY + size;

	v = iso_vertices;
	v->x = minX; v->y = minY; v->z = 0; v->Col = PACKEDCOL_WHITE; v->U = u1; v->V = v1; v++;
	v->x = maxX; v->y = minY; v->z = 0; v->Col = PACKEDCOL_WHITE; v->U = u2; v->V = v1; v++;
	v->x = maxX; v->y = maxY; v->z = 0; v->Col = PACKEDCOL_WHITE; v->U = u2; v->V = v2; v++;
	v->x = minX; v->y = maxY; v->z = 0; v->Col = PACKEDCOL_WHITE; v->U = u1; v->V = v2; v++;
	iso_vertices = v;
}

static void IsometricDrawer_Angled(BlockID block, float size) {
	cc_bool bright;
	Vec3 min, max;
	struct VertexTextured* beg = iso_vertices;
	struct VertexTextured* v;
	float x, y, scale;

	/* isometric coords size: cosY * -scale - sinY * scale */
	/* we need to divide by (2 * cosY), as the calling function expects size to be in pixels. */
	scale = size / (2.0f * iso_cosY);

	Drawer.MinBB = Blocks.MinBB[block]; Drawer.MinBB.y = 1.0f - Drawer.MinBB.y;
	Drawer.MaxBB = Blocks.MaxBB[block]; Drawer.MaxBB.y = 1.0f - Drawer.MaxBB.y;
	min = Blocks.MinBB[block]; max = Blocks.MaxBB[block];

	Drawer.X1 = scale * (1.0f - min.x * 2.0f);
	Drawer.X2 = scale * (1.0f - max.x * 2.0f);
	Drawer.Y1 = scale * (1.0f - min.y * 2.0f);
	Drawer.Y2 = scale * (1.0f - max.y * 2.0f);
	Drawer.Z1 = scale * (1.0f - min.z * 2.0f);
	Drawer.Z2 = scale * (1.0f - max.z * 2.0f);

	bright = Blocks.Brightness[block];
	Drawer.Tinted  = Blocks.Tinted[block];
	Drawer.TintCol = Blocks.FogCol[block];

	Drawer_XMax(1, bright ? PACKEDCOL_WHITE : iso_colorXSide,
		IsometricDrawer_GetTexLoc(block, FACE_XMAX), &iso_vertices);
	Drawer_ZMin(1, bright ? PACKEDCOL_WHITE : iso_colorZSide,
		IsometricDrawer_GetTexLoc(block, FACE_ZMIN), &iso_vertices);
	Drawer_YMax(1, PACKEDCOL_WHITE,
		IsometricDrawer_GetTexLoc(block, FACE_YMAX), &iso_vertices);

	for (v = beg; v < iso_vertices; v++)
	{
		/* Cut down form of: */
		/*   Matrix_RotateY(&rotY,  45.0f * MATH_DEG2RAD); */
		/*   Matrix_RotateX(&rotX, -30.0f * MATH_DEG2RAD); */
		/*   Matrix_Mul(&iso_transform, &rotY, &rotX); */
		/*   ...                                       */
		/*   Vec3 vec = { v.x, v.y, v.z }; */
		/*   Vec3_Transform(&vec, &vec, &iso_transform); */
		/* With all unnecessary operations either simplified or removed */
		x = v->x * iso_cosY                              + v->z * -iso_sinY;
		y = v->x * iso_sinX * iso_sinY + v->y * iso_cosX + v->z * iso_sinX * iso_cosY;

		v->x = x + iso_posX;
		v->y = y + iso_posY;
	}
}

void IsometricDrawer_BeginBatch(struct VertexTextured* vertices, int* state) {
	IsometricDrawer_InitCache();
	iso_vertices      = vertices;
	iso_vertices_base = vertices;
	iso_state         = state; /* TODO just store TextureLoc ??? */
}

void IsometricDrawer_AddBatch(BlockID block, float size, float x, float y) {
	int itemTile;
	if (Blocks.Draw[block] == DRAW_GAS) return;

	iso_posX = x; iso_posY = y;

#if CC_BUILD_FPU_MODE <= CC_FPU_MODE_MINIMAL
	IsometricDrawer_Flat(block, size);
#else
	/* Certain blocks render as flat item sprites from items.png */
	itemTile = GetItemTexForBlock(block);
	if (itemTile >= 0) {
		IsometricDrawer_FlatItem(itemTile, size);
	} else if (Blocks.Draw[block] == DRAW_SPRITE || block == BLOCK_LADDER) {
		IsometricDrawer_Flat(block, size);
	} else {
		IsometricDrawer_Angled(block, size);
	}
#endif
}

void IsometricDrawer_AddItemBatch(int itemTile, float size, float x, float y) {
	if (itemTile < 0) return;
	iso_posX = x; iso_posY = y;
	IsometricDrawer_FlatItem(itemTile, size);
}

int IsometricDrawer_EndBatch(void) {
	return (int)(iso_vertices - iso_vertices_base);
}

#if CC_BUILD_FPU_MODE <= CC_FPU_MODE_MINIMAL
	#define ISO_DRAW_HINT DRAW_HINT_SPRITE
#else
	#define ISO_DRAW_HINT DRAW_HINT_NONE
#endif
static void IsometricDrawer_BindTex(int idx) {
	if (idx == ISO_STATE_ITEMS_TEX) {
		Gfx_BindTexture(items_tex.texID);
	} else {
		Atlas1D_Bind(idx);
	}
}

void IsometricDrawer_Render(int count, int offset, int* state) {
	int i, curIdx, batchBeg, batchLen;

	curIdx   = state[0];
	batchLen = 0;
	batchBeg = offset;

	for (i = 0; i < count / 4; i++, batchLen += 4)
	{
		if (state[i] == curIdx) continue;

		/* Flush previous batch */
		IsometricDrawer_BindTex(curIdx);
		Gfx_DrawVb_IndexedTris_Range(batchLen, batchBeg, ISO_DRAW_HINT);

		/* Reset for next batch */
		curIdx   = state[i];
		batchBeg += batchLen;
		batchLen = 0;
	}

	IsometricDrawer_BindTex(curIdx);
	Gfx_DrawVb_IndexedTris_Range(batchLen, batchBeg, ISO_DRAW_HINT);
}
