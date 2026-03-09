#include "Signs.h"
#include "String_.h"
#include "Block.h"
#include "Platform.h"
#include "Drawer2D.h"
#include "Graphics.h"
#include "Inventory.h"
#include "Funcs.h"
#include "Game.h"
#include "Event.h"
#include "Chat.h"
#include "TexturePack.h"
#include "ExtMath.h"
#include "World.h"
#include "Window.h"
#include "Lighting.h"

/* Pixel dimensions for the pre-rasterized sign texture.
   128 wide x 48 tall = 4 lines of 12px each. */
#define SIGN_TEX_W 128
#define SIGN_TEX_H  48
#define SIGN_LINE_H  12

/* Board geometry offsets (in block-local coordinates, range 0..1) */
#define SIGN_X_MIN (1.0f / 16.0f)
#define SIGN_X_MAX (15.0f / 16.0f)
#define SIGN_Y_MIN (4.0f / 16.0f)
#define SIGN_Y_MAX (12.0f / 16.0f)
/* Front-face Z/X offset from the block's LOCAL origin (min corner of block).
   Facing 0 & 2: text face is the near side of the board — slightly inward from 14/16 to avoid Z-fighting.
   Facing 1 & 3: text face is the far  side of the board — slightly outward from  2/16 to avoid Z-fighting. */
#define SIGN_NEAR_FACE (14.95f / 16.0f)
#define SIGN_FAR_FACE  (1.05f  / 16.0f)


/*########################################################################################################################*
*---------------------------------------------------Sign texture cache----------------------------------------------------*
*#########################################################################################################################*/

/* One cache entry per placed sign: stores world position + cached GPU texture. */
struct SignTexEntry {
	int x, y, z;
	struct Texture tex; /* tex.ID == 0 means not yet uploaded */
	cc_bool dirty;      /* true means tex must be (re-)rasterized */
};

static struct SignTexEntry signCache[MAX_SIGNS];
static int signCacheCount;

static GfxResourceID signs_VB;
static struct FontDesc sign_font;
static cc_bool sign_fontReady;


/* Find an existing cache entry for (x,y,z), or create a new blank one. */
static struct SignTexEntry* SignCache_FindOrAdd(int x, int y, int z) {
	int i;
	for (i = 0; i < signCacheCount; i++) {
		if (signCache[i].x == x && signCache[i].y == y && signCache[i].z == z)
			return &signCache[i];
	}
	if (signCacheCount >= MAX_SIGNS) return NULL;
	Mem_Set(&signCache[signCacheCount], 0, sizeof(struct SignTexEntry));
	signCache[signCacheCount].x     = x;
	signCache[signCacheCount].y     = y;
	signCache[signCacheCount].z     = z;
	signCache[signCacheCount].dirty = true;
	return &signCache[signCacheCount++];
}

/* Remove cache entries whose position no longer matches any sign in Signs[]. */
static void SignCache_Purge(void) {
	int i, j, found;
	for (i = signCacheCount - 1; i >= 0; i--) {
		found = 0;
		for (j = 0; j < Sign_Count; j++) {
			if (Signs[j].x == signCache[i].x &&
				Signs[j].y == signCache[i].y &&
				Signs[j].z == signCache[i].z) { found = 1; break; }
		}
		if (!found) {
			Gfx_DeleteTexture(&signCache[i].tex.ID);
			signCacheCount--;
			if (i < signCacheCount) signCache[i] = signCache[signCacheCount];
		}
	}
}

void Signs_InvalidateAt(int x, int y, int z) {
	int i;
	for (i = 0; i < signCacheCount; i++) {
		if (signCache[i].x == x && signCache[i].y == y && signCache[i].z == z) {
			signCache[i].dirty = true;
			return;
		}
	}
}

void Signs_Free(void) {
	int i;
	for (i = 0; i < signCacheCount; i++) {
		Gfx_DeleteTexture(&signCache[i].tex.ID);
	}
	signCacheCount = 0;
	Gfx_DeleteDynamicVb(&signs_VB);
	if (sign_fontReady) { Font_Free(&sign_font); sign_fontReady = false; }
}

/* Called when a new world begins loading — clear all sign data and cached textures */
static void Signs_OnNewMap(void* obj) {
	int i;
	for (i = 0; i < signCacheCount; i++) {
		Gfx_DeleteTexture(&signCache[i].tex.ID);
	}
	signCacheCount = 0;
	Sign_Count = 0;
}

/* Called when GPU context is lost (e.g. resolution change in D3D9) — free GPU resources */
static void Signs_OnContextLost(void* obj) {
	int i;
	Gfx_DeleteDynamicVb(&signs_VB);
	for (i = 0; i < signCacheCount; i++) {
		Gfx_DeleteTexture(&signCache[i].tex.ID);
		signCache[i].dirty = true; /* force re-rasterize on next render */
	}
}

void Signs_Init(void) {
	Event_Register_(&WorldEvents.NewMap,    NULL, Signs_OnNewMap);
	Event_Register_(&GfxEvents.ContextLost, NULL, Signs_OnContextLost);
}


/*########################################################################################################################*
*-------------------------------------------------Sign texture rasterizer-------------------------------------------------*
*#########################################################################################################################*/

/* Clear sign texture to transparent — the wood background comes from
   the Builder model (wall signs) or the board box (floor signs). */
static void Sign_DrawWoodBackground(struct Context2D* ctx) {
	Context2D_Clear(ctx, BitmapCol_Make(0, 0, 0, 0), 0, 0, SIGN_TEX_W, SIGN_TEX_H);
}

static void Sign_RasterizeTexture(struct SignTexEntry* entry, struct SignData* sd) {
	struct Context2D ctx;
	struct DrawTextArgs args;
	int i, textW, lineY;

	Context2D_Alloc(&ctx, SIGN_TEX_W, SIGN_TEX_H);
	Sign_DrawWoodBackground(&ctx);

	if (!sign_fontReady) {
		Font_Make(&sign_font, 8, FONT_FLAGS_NONE);
		sign_fontReady = true;
	}

	for (i = 0; i < 4; i++) {
		char colorBuf[20];
		cc_string colorStr;
		int len;
		if (!sd->lines[i][0]) continue; /* skip empty lines */
		/* Prepend &0 (black color code) to text */
		colorBuf[0] = '&'; colorBuf[1] = '0';
		len = String_Length(sd->lines[i]);
		if (len > (int)sizeof(colorBuf) - 2) len = (int)sizeof(colorBuf) - 2;
		Mem_Copy(colorBuf + 2, sd->lines[i], len);
		colorStr = String_Init(colorBuf, len + 2, sizeof(colorBuf));
		lineY   = i * SIGN_LINE_H;
		DrawTextArgs_Make(&args, &colorStr, &sign_font, false);
		textW   = Drawer2D_TextWidth(&args);
		/* Centre text horizontally */
		Context2D_DrawText(&ctx, &args, (SIGN_TEX_W - textW) / 2, lineY);
	}

	Context2D_MakeTexture(&entry->tex, &ctx);
	Context2D_Free(&ctx);
	entry->dirty = false;
}


/*########################################################################################################################*
*---------------------------------------------------Sign quad geometry----------------------------------------------------*
*#########################################################################################################################*/

/* 
   Vertex winding order for a front-facing quad in ClassiCube:
     v[0] = bottom-left,  v[1] = top-left,
     v[2] = top-right,    v[3] = bottom-right
   (CCW when viewed from the front)

   Facing conventions (match Block.c DirectionalBlock):
     0 = Facing North (board at +Z edge, text visible from -Z / North)
     1 = Facing South (board at -Z edge, text visible from +Z / South)
     2 = Facing West  (board at +X edge, text visible from -X / West)
     3 = Facing East  (board at -X edge, text visible from +X / East)
*/
static void Sign_BuildQuad(struct VertexTextured* v,
                           float bx, float by, float bz,
                           cc_uint8 facing, struct Texture* tex,
                           PackedCol col)
{
	/* Swap U so text reads left-to-right from the viewer's perspective.
	   In CC's coordinate system, +X is screen-LEFT when looking south,
	   so the texture must be horizontally mirrored relative to naive mapping. */
	float u1 = tex->uv.u2, u2 = tex->uv.u1;
	float v1 = tex->uv.v1, v2 = tex->uv.v2;
	float xScale, yScale, cx_off, cy_off, xLo, xHi, yMin, yMax;

	if (Window_Main.Width < 640 || Window_Main.Height < 480) {
		xScale = 1.125f; yScale = 0.9375f;
	} else {
		xScale = 1.25f; yScale = 1.0f;
	}
	cx_off = (SIGN_X_MIN + SIGN_X_MAX) * 0.5f;
	cy_off = (SIGN_Y_MIN + SIGN_Y_MAX) * 0.5f;
	xLo  = cx_off + (SIGN_X_MIN - cx_off) * xScale;
	xHi  = cx_off + (SIGN_X_MAX - cx_off) * xScale;
	yMin = by + cy_off + (SIGN_Y_MIN - cy_off) * yScale;
	yMax = by + cy_off + (SIGN_Y_MAX - cy_off) * yScale;

	switch (facing) {
		default:
		case 0: /* Text faces North (-Z): quad on near (-Z) face of board, which sits at +Z edge */
		{
			float z = bz + SIGN_NEAR_FACE;
			v[0].x = bx + xLo; v[0].y = yMin; v[0].z = z; v[0].Col = col; v[0].U = u1; v[0].V = v2;
			v[1].x = bx + xLo; v[1].y = yMax; v[1].z = z; v[1].Col = col; v[1].U = u1; v[1].V = v1;
			v[2].x = bx + xHi; v[2].y = yMax; v[2].z = z; v[2].Col = col; v[2].U = u2; v[2].V = v1;
			v[3].x = bx + xHi; v[3].y = yMin; v[3].z = z; v[3].Col = col; v[3].U = u2; v[3].V = v2;
			break;
		}
		case 1: /* Text faces South (+Z): quad on far (+Z) face of board, which sits at -Z edge */
		{
			float z = bz + SIGN_FAR_FACE;
			/* Mirrored on X so text reads correctly when viewed from +Z */
			v[0].x = bx + xHi; v[0].y = yMin; v[0].z = z; v[0].Col = col; v[0].U = u1; v[0].V = v2;
			v[1].x = bx + xHi; v[1].y = yMax; v[1].z = z; v[1].Col = col; v[1].U = u1; v[1].V = v1;
			v[2].x = bx + xLo; v[2].y = yMax; v[2].z = z; v[2].Col = col; v[2].U = u2; v[2].V = v1;
			v[3].x = bx + xLo; v[3].y = yMin; v[3].z = z; v[3].Col = col; v[3].U = u2; v[3].V = v2;
			break;
		}
		case 2: /* Text faces West (-X): quad on near (-X) face of board, which sits at +X edge */
		{
			float x = bx + SIGN_NEAR_FACE;
			v[0].x = x; v[0].y = yMin; v[0].z = bz + xHi; v[0].Col = col; v[0].U = u1; v[0].V = v2;
			v[1].x = x; v[1].y = yMax; v[1].z = bz + xHi; v[1].Col = col; v[1].U = u1; v[1].V = v1;
			v[2].x = x; v[2].y = yMax; v[2].z = bz + xLo; v[2].Col = col; v[2].U = u2; v[2].V = v1;
			v[3].x = x; v[3].y = yMin; v[3].z = bz + xLo; v[3].Col = col; v[3].U = u2; v[3].V = v2;
			break;
		}
		case 3: /* Text faces East (+X): quad on far (+X) face of board, which sits at -X edge */
		{
			float x = bx + SIGN_FAR_FACE;
			/* Mirrored on Z so text reads correctly when viewed from +X */
			v[0].x = x; v[0].y = yMin; v[0].z = bz + xLo; v[0].Col = col; v[0].U = u1; v[0].V = v2;
			v[1].x = x; v[1].y = yMax; v[1].z = bz + xLo; v[1].Col = col; v[1].U = u1; v[1].V = v1;
			v[2].x = x; v[2].y = yMax; v[2].z = bz + xHi; v[2].Col = col; v[2].U = u2; v[2].V = v1;
			v[3].x = x; v[3].y = yMin; v[3].z = bz + xHi; v[3].Col = col; v[3].U = u2; v[3].V = v2;
			break;
		}
	}
}


/*########################################################################################################################*
*----------------------------------------------Floor sign (sign post) geometry--------------------------------------------*
*#########################################################################################################################*/

/* Floor sign dimensions (in block units 0..1) - matching Alpha proportions */
#define FBOARD_HALF_W (7.0f / 16.0f) /* Board is 14/16 wide, half = 7/16 */
#define FBOARD_BOT   (9.0f / 16.0f)    /* Board bottom */
#define FBOARD_TOP   (1.0f)             /* Board top */
#define FBOARD_HALF_D (1.0f / 16.0f)  /* Board depth: 2/16, half = 1/16 */
#define FBOARD_VERTS 24               /* 6 faces * 4 verts per face */

/* Build the 3D board box for a floor sign using the wood plank terrain tile.
   Outputs 24 vertices (6 faces).  Rotation 0 = board faces south (+Z). */
static void FloorSign_BuildBoardBox(struct VertexTextured* v,
                                     float bx, float by, float bz,
                                     cc_uint8 rotation, TextureLoc loc,
                                     PackedCol col)
{
	float cx = bx + 0.5f, cz = bz + 0.5f;
	float yBot = by + FBOARD_BOT, yTop = by + FBOARD_TOP;
	float angle = (float)rotation * 22.5f * MATH_DEG2RAD;
	float cos_a = Math_CosF(angle), sin_a = Math_SinF(angle);
	float W = FBOARD_HALF_W, D = FBOARD_HALF_D;

	/* Terrain atlas UV base */
	float vO  = Atlas1D_RowId(loc) * Atlas1D.InvTileSize;
	float inv = Atlas1D.InvTileSize;
	/* front/back: 14/16 wide, 7/16 tall */
	float fu = 14.0f / 16.0f, fv = vO + (7.0f / 16.0f) * inv;
	/* left/right sides: 2/16 wide, 7/16 tall */
	float su = 2.0f / 16.0f;
	/* top/bottom: 14/16 wide, 2/16 deep */
	float tv = vO + (2.0f / 16.0f) * inv;

	/* 4 corner XZ positions after rotation */
	float lf_x = cx + (-W)*cos_a - ( D)*sin_a;
	float lf_z = cz + (-W)*sin_a + ( D)*cos_a;
	float rf_x = cx + ( W)*cos_a - ( D)*sin_a;
	float rf_z = cz + ( W)*sin_a + ( D)*cos_a;
	float rb_x = cx + ( W)*cos_a + ( D)*sin_a;
	float rb_z = cz + ( W)*sin_a - ( D)*cos_a;
	float lb_x = cx + (-W)*cos_a + ( D)*sin_a;
	float lb_z = cz + (-W)*sin_a - ( D)*cos_a;

	/* Front face (text side, normal toward viewer) */
	v[0].x=lf_x; v[0].y=yBot; v[0].z=lf_z; v[0].Col=col; v[0].U=0;  v[0].V=fv;
	v[1].x=lf_x; v[1].y=yTop; v[1].z=lf_z; v[1].Col=col; v[1].U=0;  v[1].V=vO;
	v[2].x=rf_x; v[2].y=yTop; v[2].z=rf_z; v[2].Col=col; v[2].U=fu; v[2].V=vO;
	v[3].x=rf_x; v[3].y=yBot; v[3].z=rf_z; v[3].Col=col; v[3].U=fu; v[3].V=fv;
	/* Back face */
	v[4].x=rb_x; v[4].y=yBot; v[4].z=rb_z; v[4].Col=col; v[4].U=0;  v[4].V=fv;
	v[5].x=rb_x; v[5].y=yTop; v[5].z=rb_z; v[5].Col=col; v[5].U=0;  v[5].V=vO;
	v[6].x=lb_x; v[6].y=yTop; v[6].z=lb_z; v[6].Col=col; v[6].U=fu; v[6].V=vO;
	v[7].x=lb_x; v[7].y=yBot; v[7].z=lb_z; v[7].Col=col; v[7].U=fu; v[7].V=fv;
	/* Top face */
	v[8].x =lf_x; v[8].y =yTop; v[8].z =lf_z; v[8].Col =col; v[8].U =0;  v[8].V =vO;
	v[9].x =lb_x; v[9].y =yTop; v[9].z =lb_z; v[9].Col =col; v[9].U =0;  v[9].V =tv;
	v[10].x=rb_x; v[10].y=yTop; v[10].z=rb_z; v[10].Col=col; v[10].U=fu; v[10].V=tv;
	v[11].x=rf_x; v[11].y=yTop; v[11].z=rf_z; v[11].Col=col; v[11].U=fu; v[11].V=vO;
	/* Bottom face */
	v[12].x=lb_x; v[12].y=yBot; v[12].z=lb_z; v[12].Col=col; v[12].U=0;  v[12].V=vO;
	v[13].x=lf_x; v[13].y=yBot; v[13].z=lf_z; v[13].Col=col; v[13].U=0;  v[13].V=tv;
	v[14].x=rf_x; v[14].y=yBot; v[14].z=rf_z; v[14].Col=col; v[14].U=fu; v[14].V=tv;
	v[15].x=rb_x; v[15].y=yBot; v[15].z=rb_z; v[15].Col=col; v[15].U=fu; v[15].V=vO;
	/* Left side */
	v[16].x=lb_x; v[16].y=yBot; v[16].z=lb_z; v[16].Col=col; v[16].U=0;  v[16].V=fv;
	v[17].x=lb_x; v[17].y=yTop; v[17].z=lb_z; v[17].Col=col; v[17].U=0;  v[17].V=vO;
	v[18].x=lf_x; v[18].y=yTop; v[18].z=lf_z; v[18].Col=col; v[18].U=su; v[18].V=vO;
	v[19].x=lf_x; v[19].y=yBot; v[19].z=lf_z; v[19].Col=col; v[19].U=su; v[19].V=fv;
	/* Right side */
	v[20].x=rf_x; v[20].y=yBot; v[20].z=rf_z; v[20].Col=col; v[20].U=0;  v[20].V=fv;
	v[21].x=rf_x; v[21].y=yTop; v[21].z=rf_z; v[21].Col=col; v[21].U=0;  v[21].V=vO;
	v[22].x=rb_x; v[22].y=yTop; v[22].z=rb_z; v[22].Col=col; v[22].U=su; v[22].V=vO;
	v[23].x=rb_x; v[23].y=yBot; v[23].z=rb_z; v[23].Col=col; v[23].U=su; v[23].V=fv;
}

/* Build the rotated text quad for the floor sign front face.
   rotation: 0-15 (each step = 22.5 degrees clockwise from south when viewed from above).
   The text quad sits slightly in front of the board to avoid z-fighting. */
static void FloorSign_BuildTextQuad(struct VertexTextured* v,
                                     float bx, float by, float bz,
                                     cc_uint8 rotation, struct Texture* tex,
                                     PackedCol col)
{
	float cx = bx + 0.5f, cz = bz + 0.5f;
	float angle = (float)rotation * 22.5f * MATH_DEG2RAD;
	float cos_a = Math_CosF(angle), sin_a = Math_SinF(angle);
	float u1 = tex->uv.u1, u2 = tex->uv.u2;
	float v1 = tex->uv.v1, v2 = tex->uv.v2;
	float xScale, yScale, cyBoard, halfW, yBot, yTop;
	float lx0, lx1, lz, rx0, rz0, rx1, rz1;

	if (Window_Main.Width < 640 || Window_Main.Height < 480) {
		xScale = 1.125f; yScale = 0.9375f;
	} else {
		xScale = 1.25f; yScale = 1.0f;
	}
	halfW   = FBOARD_HALF_W * xScale;
	cyBoard = (FBOARD_BOT + FBOARD_TOP) * 0.5f;
	yBot    = by + cyBoard + (FBOARD_BOT - cyBoard) * yScale;
	yTop    = by + cyBoard + (FBOARD_TOP - cyBoard) * yScale;

	/* Front face with z-fighting offset */
	lx0 = -halfW; lx1 = halfW;
	lz = FBOARD_HALF_D + 0.002f;

	rx0 = cx + lx0 * cos_a - lz * sin_a;
	rz0 = cz + lx0 * sin_a + lz * cos_a;
	rx1 = cx + lx1 * cos_a - lz * sin_a;
	rz1 = cz + lx1 * sin_a + lz * cos_a;

	v[0].x = rx0; v[0].y = yBot; v[0].z = rz0; v[0].Col = col; v[0].U = u1; v[0].V = v2;
	v[1].x = rx0; v[1].y = yTop; v[1].z = rz0; v[1].Col = col; v[1].U = u1; v[1].V = v1;
	v[2].x = rx1; v[2].y = yTop; v[2].z = rz1; v[2].Col = col; v[2].U = u2; v[2].V = v1;
	v[3].x = rx1; v[3].y = yBot; v[3].z = rz1; v[3].Col = col; v[3].U = u2; v[3].V = v2;
}


/*########################################################################################################################*
*--------------------------------------------------Signs_RenderText (public)----------------------------------------------*
*#########################################################################################################################*/

/* Maximum vertices needed: 24 for floor sign board box (6 faces x 4 verts) */
#define SIGN_MAX_VERTS 24

void Signs_RenderText(void) {
	struct VertexTextured verts[SIGN_MAX_VERTS];
	struct VertexTextured* dst;
	struct SignTexEntry* entry;
	struct SignData* sd;
	BlockID block;
	cc_bool hadFog;
	PackedCol signCol;
	int i;

	if (!Sign_Count) return;

	/* Sync cache: remove entries for signs that no longer exist */
	SignCache_Purge();

	if (!signs_VB)
		signs_VB = Gfx_CreateDynamicVb(VERTEX_FORMAT_TEXTURED, SIGN_MAX_VERTS);
	if (!signs_VB) return; /* context lost — skip rendering this frame */

	Gfx_SetAlphaTest(true);
	Gfx_SetFaceCulling(false);

	/* Disable fog while rendering signs (prevents fog colour bleeding into sign faces) */
	hadFog = Gfx_GetFog();
	if (hadFog) Gfx_SetFog(false);
	Gfx_SetVertexFormat(VERTEX_FORMAT_TEXTURED);

	for (i = 0; i < Sign_Count; i++) {
		sd    = &Signs[i];
		entry = SignCache_FindOrAdd(sd->x, sd->y, sd->z);
		if (!entry) continue;

		/* (Re-)rasterize text if needed */
		if (entry->dirty || !entry->tex.ID)
			Sign_RasterizeTexture(entry, sd);
		if (!entry->tex.ID) continue;

		block = World_GetBlock(sd->x, sd->y, sd->z);
		signCol = Lighting.Color(sd->x, sd->y, sd->z);

		if (block == BLOCK_SIGN_FLOOR) {
			/* --- Floor sign: 3D board box + text overlay (post from standard builder) --- */
			TextureLoc boardLoc = Block_Tex(BLOCK_SIGN_FLOOR, FACE_YMAX);

			/* Draw board box with wood plank terrain texture */
			FloorSign_BuildBoardBox(verts,
			                        (float)sd->x, (float)sd->y, (float)sd->z,
			                        sd->rotation, boardLoc, signCol);
			Gfx_BindTexture(Atlas1D.TexIds[Atlas1D_Index(boardLoc)]);
			dst = (struct VertexTextured*)Gfx_LockDynamicVb(signs_VB, VERTEX_FORMAT_TEXTURED, FBOARD_VERTS);
			Mem_Copy(dst, verts, FBOARD_VERTS * sizeof(struct VertexTextured));
			Gfx_UnlockDynamicVb(signs_VB);
			Gfx_DrawVb_IndexedTris(FBOARD_VERTS);

			/* Draw text overlay on front face */
			FloorSign_BuildTextQuad(verts,
			                        (float)sd->x, (float)sd->y, (float)sd->z,
			                        sd->rotation, &entry->tex, signCol);
			Gfx_BindTexture(entry->tex.ID);
			dst = (struct VertexTextured*)Gfx_LockDynamicVb(signs_VB, VERTEX_FORMAT_TEXTURED, 4);
			dst[0] = verts[0]; dst[1] = verts[1]; dst[2] = verts[2]; dst[3] = verts[3];
			Gfx_UnlockDynamicVb(signs_VB);
			Gfx_DrawVb_IndexedTris(4);
		} else {
			/* --- Wall sign: single text quad --- */
			cc_uint8 facing = DirectionalBlock_GetFacing(sd->x, sd->y, sd->z);
			Sign_BuildQuad(verts,
			               (float)sd->x, (float)sd->y, (float)sd->z,
			               facing, &entry->tex, signCol);
			Gfx_BindTexture(entry->tex.ID);
			dst = (struct VertexTextured*)Gfx_LockDynamicVb(signs_VB, VERTEX_FORMAT_TEXTURED, 4);
			dst[0] = verts[0]; dst[1] = verts[1]; dst[2] = verts[2]; dst[3] = verts[3];
			Gfx_UnlockDynamicVb(signs_VB);
			Gfx_DrawVb_IndexedTris(4);
		}
	}

	Gfx_SetFaceCulling(true);
	Gfx_SetAlphaTest(false);
	if (hadFog) Gfx_SetFog(true);
}
