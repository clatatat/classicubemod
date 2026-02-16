#include "Block.h"
#include "Funcs.h"
#include "ExtMath.h"
#include "TexturePack.h"
#include "Game.h"
#include "Entity.h"
#include "Inventory.h"
#include "Event.h"
#include "Picking.h"
#include "Lighting.h"
#include "Audio.h"
#include "World.h"

struct _BlockLists Blocks;

/*########################################################################################################################*
*-----------------------------------------------Directional block cache---------------------------------------------------*
*#########################################################################################################################*/
/* Runtime cache for directional blocks (chests/furnaces) - not saved to disk */
#define MAX_DIRECTIONAL_BLOCKS 1024

typedef struct DirectionalBlockInfo_ {
	int x, y, z;
	cc_uint8 facing; /* 0=North(-Z), 1=South(+Z), 2=West(-X), 3=East(+X) */
} DirectionalBlockInfo;

static DirectionalBlockInfo directionalCache[MAX_DIRECTIONAL_BLOCKS];
static int directionalCount = 0;
static cc_bool directionalFacing_Enabled = true;

/*########################################################################################################################*
*---------------------------------------------------Default properties----------------------------------------------------*
*#########################################################################################################################*/
#define FOG_NONE  0
#define FOG_WATER PackedCol_Make( 10,  40, 120, 255)
#define FOG_LAVA  PackedCol_Make(153,  25,   0, 255)

/* Brightness */
#define BRIT_NONE 0
#define BRIT_FULL FANCY_LIGHTING_MAX_LEVEL
#define BRIT_MAGM 10

struct SimpleBlockDef {
	const char* name;
	cc_uint8 topTexture, sideTexture, bottomTexture, height;
	PackedCol fogColor; cc_uint8 fogDensity;
	cc_uint8 brightness, blocksLight; cc_uint8 gravity;
	cc_uint8 draw, collide, digSound, stepSound;
};
static const struct SimpleBlockDef invalid_blockDef = { 
	"Invalid", 0,0,0,16, FOG_NONE,0, false,true, 100, DRAW_OPAQUE,COLLIDE_SOLID,0
};

/* Extra block definitions for survival mode blocks */
static const struct SimpleBlockDef door_ns_top_def = {"Door NS Top", 81, 81, 81, 16, FOG_NONE, 0, BRIT_NONE, false, 100, DRAW_TRANSPARENT, COLLIDE_SOLID, SOUND_NONE, SOUND_NONE};
static const struct SimpleBlockDef door_ew_bottom_def = {"Door EW Bottom", 97, 97, 97, 16, FOG_NONE, 0, BRIT_NONE, false, 100, DRAW_TRANSPARENT, COLLIDE_SOLID, SOUND_NONE, SOUND_NONE};
static const struct SimpleBlockDef door_ew_top_def = {"Door EW Top", 81, 81, 81, 16, FOG_NONE, 0, BRIT_NONE, false, 100, DRAW_TRANSPARENT, COLLIDE_SOLID, SOUND_NONE, SOUND_NONE};
static const struct SimpleBlockDef lit_red_ore_dust_def = {"Lit Red Ore Dust", 100, 102, 102, 1, FOG_NONE, 0, BRIT_FULL, true, 100, DRAW_TRANSPARENT, COLLIDE_NONE, SOUND_STONE, SOUND_STONE};
static const struct SimpleBlockDef red_ore_torch_off_def = {"Red Ore Torch Off", 115, 115, 115, 11, FOG_NONE, 0, BRIT_NONE, false, 100, DRAW_SPRITE, COLLIDE_NONE, SOUND_WOOD, SOUND_WOOD};
static const struct SimpleBlockDef red_torch_wall_on_def = {"Red Torch Wall On", 99, 99, 99, 11, FOG_NONE, 0, BRIT_FULL, false, 100, DRAW_SPRITE, COLLIDE_NONE, SOUND_WOOD, SOUND_WOOD};
static const struct SimpleBlockDef red_torch_wall_off_def = {"Red Torch Wall Off", 115, 115, 115, 11, FOG_NONE, 0, BRIT_NONE, false, 100, DRAW_SPRITE, COLLIDE_NONE, SOUND_WOOD, SOUND_WOOD};
static const struct SimpleBlockDef red_torch_unmounted_def = {"Red Torch Unmounted", 99, 99, 99, 11, FOG_NONE, 0, BRIT_FULL, false, 100, DRAW_SPRITE, COLLIDE_NONE, SOUND_WOOD, SOUND_WOOD};
static const struct SimpleBlockDef red_torch_unmounted_off_def = {"Red Torch Unmounted Off", 115, 115, 115, 11, FOG_NONE, 0, BRIT_NONE, false, 100, DRAW_SPRITE, COLLIDE_NONE, SOUND_WOOD, SOUND_WOOD};
static const struct SimpleBlockDef button_pressed_def = {"Button Pressed", 1, 1, 1, 16, FOG_NONE, 0, BRIT_NONE, false, 100, DRAW_TRANSPARENT, COLLIDE_NONE, SOUND_NONE, SOUND_NONE};
static const struct SimpleBlockDef lever_on_def = {"Lever On", 16, 16, 16, 16, FOG_NONE, 0, BRIT_NONE, false, 100, DRAW_TRANSPARENT, COLLIDE_NONE, SOUND_NONE, SOUND_NONE};
static const struct SimpleBlockDef pressure_plate_pressed_def = {"Pressure Plate Pressed", 4, 4, 4, 16, FOG_NONE, 0, BRIT_NONE, false, 100, DRAW_TRANSPARENT, COLLIDE_NONE, SOUND_STONE, SOUND_STONE};
static const struct SimpleBlockDef iron_door_def = {"Iron Door", 55, 55, 55, 16, FOG_NONE, 0, BRIT_NONE, false, 100, DRAW_TRANSPARENT_THICK, COLLIDE_SOLID, SOUND_METAL, SOUND_METAL};
static const struct SimpleBlockDef double_chest_def = {"Double Chest", 26, 27, 26, 16, FOG_NONE, 0, BRIT_NONE, true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_WOOD, SOUND_WOOD};
static const struct SimpleBlockDef shadow_ceiling_def = {"Shadow Ceiling", 0, 0, 0, 16, FOG_NONE, 0, BRIT_NONE, true, 100, DRAW_GAS, COLLIDE_SOLID, SOUND_NONE, SOUND_NONE};
static const struct SimpleBlockDef snowy_grass_def = {"Snowy Grass", 116, 118, 2, 16, FOG_NONE, 0, BRIT_NONE, true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_GRASS, SOUND_SNOW};

/* Properties for all built-in blocks (Classic and CPE blocks) */
static const struct SimpleBlockDef core_blockDefs[] = {
/*NAME                TOP SID BOT HEI FOG_COLOR  DENS  BRIGHT    BLOCKS GRAV DRAW_MODE    COLLIDE_MODE   DIG_SOUND     STEP_SOUND   */
/*                    TEX ES  TOM GHT            ITY   NESS      LIGHT  ITY                                                         */
{ "Air",               0,  0,  0, 16, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_GAS,    COLLIDE_NONE,  SOUND_NONE,   SOUND_NONE   },
{ "Stone",             1,  1,  1, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE,  SOUND_STONE  },
{ "Grass",             0,  3,  2, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_GRASS,  SOUND_GRASS  },
{ "Dirt",              2,  2,  2, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_GRAVEL, SOUND_GRAVEL },
{ "Cobblestone",      16, 16, 16, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE,  SOUND_STONE  },
{ "Wood",              4,  4,  4, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_WOOD,   SOUND_WOOD   },
{ "Sapling",          15, 15, 15, 16, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_SPRITE, COLLIDE_NONE,  SOUND_GRASS,  SOUND_NONE   },
{ "Bedrock",          17, 17, 17, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE,  SOUND_STONE  },

{ "Water",            14, 14, 14, 16, FOG_WATER,  10, BRIT_NONE,  true, 100, DRAW_TRANSLUCENT, COLLIDE_WATER, SOUND_NONE, SOUND_NONE },
{ "Still water",      14, 14, 14, 16, FOG_WATER,  10, BRIT_NONE,  true, 100, DRAW_TRANSLUCENT, COLLIDE_WATER, SOUND_NONE, SOUND_NONE },
{ "Lava",             30, 30, 30, 16, FOG_LAVA , 180, BRIT_FULL,  true, 100, DRAW_OPAQUE, COLLIDE_LAVA,  SOUND_NONE,   SOUND_NONE   },
{ "Still lava",       30, 30, 30, 16, FOG_LAVA , 180, BRIT_FULL,  true, 100, DRAW_OPAQUE, COLLIDE_LAVA,  SOUND_NONE,   SOUND_NONE   },
{ "Sand",             18, 18, 18, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_SAND,   SOUND_SAND   },
{ "Gravel",           19, 19, 19, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_GRAVEL, SOUND_GRAVEL },
{ "Gold ore",         32, 32, 32, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE,  SOUND_STONE  },
{ "Iron ore",         33, 33, 33, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE,  SOUND_STONE  },

{ "Coal ore",         34, 34, 34, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE,  SOUND_STONE  },
{ "Log",              21, 20, 21, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_WOOD,   SOUND_WOOD   },
{ "Leaves",           22, 22, 22, 16, FOG_NONE ,   0, BRIT_NONE, false,  40, DRAW_TRANSPARENT_THICK, COLLIDE_SOLID, SOUND_GRASS, SOUND_GRASS },
{ "Sponge",           48, 48, 48, 16, FOG_NONE ,   0, BRIT_NONE,  true,  90, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_GRASS,  SOUND_GRASS  },
{ "Glass",            49, 49, 49, 16, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_TRANSPARENT, COLLIDE_SOLID, SOUND_GLASS,SOUND_METAL},
{ "Red",              64, 64, 64, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },
{ "Orange",           65, 65, 65, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },
{ "Yellow",           66, 66, 66, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },
	
{ "Lime",             67, 67, 67, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },
{ "Green",            68, 68, 68, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },
{ "Teal",             69, 69, 69, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },
{ "Aqua",             70, 70, 70, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },
{ "Cyan",             71, 71, 71, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },
{ "Blue",             72, 72, 72, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },
{ "Indigo",           73, 73, 73, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },
{ "Violet",           74, 74, 74, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },

{ "Magenta",          75, 75, 75, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },
{ "Pink",             76, 76, 76, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },
{ "Black",            77, 77, 77, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },
{ "Gray",             78, 78, 78, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },
{ "White",            79, 79, 79, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },
{ "Dandelion",        13, 13, 13, 16, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_SPRITE, COLLIDE_NONE,  SOUND_GRASS,  SOUND_NONE   },
{ "Rose",             12, 12, 12, 16, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_SPRITE, COLLIDE_NONE,  SOUND_GRASS,  SOUND_NONE   },
{ "Brown mushroom",   29, 29, 29, 16, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_SPRITE, COLLIDE_NONE,  SOUND_GRASS,  SOUND_NONE   },

{ "Red mushroom",     28, 28, 28, 16, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_SPRITE, COLLIDE_NONE,  SOUND_GRASS,  SOUND_NONE   },
{ "Gold",             24, 40, 56, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_METAL,  SOUND_METAL  },
{ "Iron",             23, 39, 55, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_METAL,  SOUND_METAL  },
{ "Double slab",       6,  5,  6, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE,  SOUND_STONE  },
{ "Slab",              6,  5,  6,  8, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE,  SOUND_STONE  },
{ "Brick",             7,  7,  7, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE,  SOUND_STONE  },
{ "TNT",               9,  8, 10, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_GRASS,  SOUND_GRASS  },
{ "Bookshelf",         4, 35,  4, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_WOOD,   SOUND_WOOD   },

{ "Mossy rocks",      36, 36, 36, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE,  SOUND_STONE  },
{ "Obsidian",         37, 37, 37, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE,  SOUND_STONE  },
{ "Diamond Block",    25, 41, 57, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_METAL,  SOUND_METAL  },
{ "Diamond Ore",      50, 50, 50, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE,  SOUND_STONE  },
{ "Red Ore",          51, 51, 51, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE,  SOUND_STONE  },
{ "Cactus",           52, 53, 54, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_CLOTH,  SOUND_CLOTH  },
{ "Workbench",        45, 61,  4, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_WOOD,   SOUND_WOOD   },
{ "Furnace",           1, 47,  1, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_STONE,  SOUND_STONE  },

{ "Chest",            26, 27, 26, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_WOOD,   SOUND_WOOD   },
{ "Cobweb",           11, 11, 11, 16, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_SPRITE, COLLIDE_NONE,  SOUND_CLOTH,  SOUND_NONE   },
{ "Ladder",           83, 83, 83, 16, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_TRANSPARENT, COLLIDE_CLIMB, SOUND_WOOD, SOUND_WOOD },
{ "Door",             97, 97, 97, 16, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_TRANSPARENT, COLLIDE_SOLID, SOUND_NONE, SOUND_NONE },
{ "Red Ore Dust",     84, 86, 86, 1,  FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_TRANSPARENT, COLLIDE_NONE, SOUND_STONE, SOUND_STONE },
{ "Torch",            80, 80, 80, 10, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_SPRITE, COLLIDE_NONE,  SOUND_WOOD,   SOUND_WOOD   },
{ "Red Ore Torch",    99, 99, 99, 11, FOG_NONE ,   0, BRIT_FULL, false, 100, DRAW_SPRITE, COLLIDE_NONE,  SOUND_WOOD,   SOUND_WOOD   },
{ "Button",            1,  1,  1, 16, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_TRANSPARENT, COLLIDE_NONE, SOUND_NONE,  SOUND_NONE  },
{ "Lever",            16, 16, 16, 16, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_TRANSPARENT, COLLIDE_NONE, SOUND_NONE,  SOUND_NONE  },
{ "Pressure Plate",    4,  4,  4, 16, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_TRANSPARENT, COLLIDE_NONE, SOUND_STONE, SOUND_STONE },
{ "Iron Door",        55, 55, 55, 16, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_TRANSPARENT_THICK, COLLIDE_SOLID, SOUND_METAL, SOUND_METAL },
{ "Snow",            116,116,116,  2, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_TRANSPARENT, COLLIDE_NONE, SOUND_SNOW,  SOUND_SNOW   },
{ "Ice",             117,117,117, 16, FOG_NONE ,   0, BRIT_NONE, false, 100, DRAW_TRANSPARENT, COLLIDE_ICE, SOUND_GLASS, SOUND_STONE  },
{ "Snow Block",      116,116,116, 16, FOG_NONE ,   0, BRIT_NONE,  true, 100, DRAW_OPAQUE, COLLIDE_SOLID, SOUND_SNOW,  SOUND_SNOW   },

/*NAME                TOP SID BOT HEI FOG_COLOR  DENS  BRIGHT    BLOCKS GRAV DRAW_MODE    COLLIDE_MODE   DIG_SOUND     STEP_SOUND   */
/*                    TEX ES  TOM GHT            ITY   NESS      LIGHT  ITY                                                         */
};

/* Returns a backwards compatible collide type of a block */
static cc_uint8 DefaultSet_MapOldCollide(BlockID b, cc_uint8 collide) {
	/* BLOCK_ROPE removed - was used for climbable blocks */
	
	if ((b == BLOCK_WATER || b == BLOCK_STILL_WATER) && collide == COLLIDE_LIQUID)
		return COLLIDE_WATER;
	if ((b == BLOCK_LAVA  || b == BLOCK_STILL_LAVA)  && collide == COLLIDE_LIQUID)
		return COLLIDE_LAVA;
	return collide;
}


/*########################################################################################################################*
*---------------------------------------------------Block properties------------------------------------------------------*
*#########################################################################################################################*/
static void Block_RecalcIsLiquid(BlockID b) {
	cc_uint8 collide = Blocks.ExtendedCollide[b];
	Blocks.IsLiquid[b] =
		(collide == COLLIDE_WATER && Blocks.Draw[b] == DRAW_TRANSLUCENT) ||
		(collide == COLLIDE_LAVA  && Blocks.Draw[b] == DRAW_TRANSPARENT);
}

/* Sets the basic and extended collide types of the given block */
static void Block_SetCollide(BlockID block, cc_uint8 collide) {
	Blocks.ExtendedCollide[block] = collide;
	Block_RecalcIsLiquid(block);

	/* Reduce extended collision types to their simpler forms */
	if (collide == COLLIDE_ICE)          collide = COLLIDE_SOLID;
	if (collide == COLLIDE_SLIPPERY_ICE) collide = COLLIDE_SOLID;

	if (collide == COLLIDE_WATER) collide = COLLIDE_LIQUID;
	if (collide == COLLIDE_LAVA)  collide = COLLIDE_LIQUID;
	Blocks.Collide[block] = collide;
}

/* Sets draw type and updates related state (e.g. FullOpaque) for the given block */
static void Block_SetDrawType(BlockID block, cc_uint8 draw) {
	if (draw == DRAW_OPAQUE && Blocks.Collide[block] != COLLIDE_SOLID) draw = DRAW_TRANSPARENT;
	Blocks.Draw[block] = draw;
	Block_RecalcIsLiquid(block);

	/* Whether a block is opaque and exactly occupies a cell in the world */
	/* The mesh builder module needs this information for optimisation purposes */
	Blocks.FullOpaque[block] = draw == DRAW_OPAQUE
		&& Blocks.MinBB[block].x == 0 && Blocks.MinBB[block].y == 0 && Blocks.MinBB[block].z == 0
		&& Blocks.MaxBB[block].x == 1 && Blocks.MaxBB[block].y == 1 && Blocks.MaxBB[block].z == 1;
}

void Block_SetSide(TextureLoc texLoc, BlockID blockId) {
	int index = blockId * FACE_COUNT;
	Blocks.Textures[index + FACE_XMIN] = texLoc;
	Blocks.Textures[index + FACE_XMAX] = texLoc;
	Blocks.Textures[index + FACE_ZMIN] = texLoc;
	Blocks.Textures[index + FACE_ZMAX] = texLoc;
}

/* Sets individual textures for specific faces (for blocks with different textures on different sides) */
static void Block_SetCustomFaces(BlockID blockId, TextureLoc xMin, TextureLoc xMax, TextureLoc zMin, TextureLoc zMax) {
	int index = blockId * FACE_COUNT;
	Blocks.Textures[index + FACE_XMIN] = xMin;
	Blocks.Textures[index + FACE_XMAX] = xMax;
	Blocks.Textures[index + FACE_ZMIN] = zMin;
	Blocks.Textures[index + FACE_ZMAX] = zMax;
}

/* Sets textures for directional blocks based on facing direction */
static void Block_SetDirectionalFaces(BlockID blockId, cc_uint8 facing, TextureLoc frontTex, TextureLoc sideTex) {
	int index = blockId * FACE_COUNT;
	/* facing: 0=North(-Z), 1=South(+Z), 2=West(-X), 3=East(+X) */
	switch (facing) {
		case 0: /* North (-Z) - front faces ZMIN */
			Blocks.Textures[index + FACE_ZMIN] = frontTex;
			Blocks.Textures[index + FACE_ZMAX] = sideTex;
			Blocks.Textures[index + FACE_XMIN] = sideTex;
			Blocks.Textures[index + FACE_XMAX] = sideTex;
			break;
		case 1: /* South (+Z) - front faces ZMAX */
			Blocks.Textures[index + FACE_ZMIN] = sideTex;
			Blocks.Textures[index + FACE_ZMAX] = frontTex;
			Blocks.Textures[index + FACE_XMIN] = sideTex;
			Blocks.Textures[index + FACE_XMAX] = sideTex;
			break;
		case 2: /* West (-X) - front faces XMIN */
			Blocks.Textures[index + FACE_ZMIN] = sideTex;
			Blocks.Textures[index + FACE_ZMAX] = sideTex;
			Blocks.Textures[index + FACE_XMIN] = frontTex;
			Blocks.Textures[index + FACE_XMAX] = sideTex;
			break;
		case 3: /* East (+X) - front faces XMAX */
			Blocks.Textures[index + FACE_ZMIN] = sideTex;
			Blocks.Textures[index + FACE_ZMAX] = sideTex;
			Blocks.Textures[index + FACE_XMIN] = sideTex;
			Blocks.Textures[index + FACE_XMAX] = frontTex;
			break;
	}
}

/* Check if a block is directional (chest, furnace, ladder, torch, button, lever) */
static cc_bool IsButton(BlockID block); /* forward declaration */
static cc_bool IsLever(BlockID block); /* forward declaration */
static cc_bool IsPressurePlate(BlockID block); /* forward declaration */
static cc_bool IsIronDoor(BlockID block); /* forward declaration */
static cc_bool IsDirectionalBlock(BlockID block) {
	return block == BLOCK_CHEST || block == BLOCK_FURNACE || block == BLOCK_LADDER || block == BLOCK_TORCH
		|| block == BLOCK_RED_ORE_TORCH || block == BLOCK_RED_ORE_TORCH_OFF
		|| block == BLOCK_RED_TORCH_ON_S || block == BLOCK_RED_TORCH_ON_N
		|| block == BLOCK_RED_TORCH_ON_E || block == BLOCK_RED_TORCH_ON_W
		|| block == BLOCK_RED_TORCH_OFF_S || block == BLOCK_RED_TORCH_OFF_N
		|| block == BLOCK_RED_TORCH_OFF_E || block == BLOCK_RED_TORCH_OFF_W
		|| block == BLOCK_RED_TORCH_UNMOUNTED || block == BLOCK_RED_TORCH_UNMOUNTED_OFF
		|| IsButton(block)
		|| IsLever(block);
}

/* Calculate which direction a torch should face based on adjacent solid blocks */
/* Returns 0-3 for wall-mounted (same convention as other directional blocks), 4 for ground, 255 for no support */
static cc_uint8 CalcTorchFacing(int x, int y, int z) {
	BlockID block;
	
	/* Check walls first (prefer wall mounting) */
	/* South wall (z+1) -> facing 0 (North) */
	if (World_Contains(x, y, z + 1)) {
		block = World_GetBlock(x, y, z + 1);
		if (block != BLOCK_AIR && Blocks.Draw[block] == DRAW_OPAQUE && !IsDirectionalBlock(block)) return 0;
	}
	/* North wall (z-1) -> facing 1 (South) */
	if (World_Contains(x, y, z - 1)) {
		block = World_GetBlock(x, y, z - 1);
		if (block != BLOCK_AIR && Blocks.Draw[block] == DRAW_OPAQUE && !IsDirectionalBlock(block)) return 1;
	}
	/* East wall (x+1) -> facing 2 (West) */
	if (World_Contains(x + 1, y, z)) {
		block = World_GetBlock(x + 1, y, z);
		if (block != BLOCK_AIR && Blocks.Draw[block] == DRAW_OPAQUE && !IsDirectionalBlock(block)) return 2;
	}
	/* West wall (x-1) -> facing 3 (East) */
	if (World_Contains(x - 1, y, z)) {
		block = World_GetBlock(x - 1, y, z);
		if (block != BLOCK_AIR && Blocks.Draw[block] == DRAW_OPAQUE && !IsDirectionalBlock(block)) return 3;
	}
	/* Ground (y-1) */
	if (y > 0) {
		block = World_GetBlock(x, y - 1, z);
		if (block != BLOCK_AIR && Blocks.Draw[block] == DRAW_OPAQUE) return 4;
	}
	/* No support */
	return 255;
}

/* Helper: check if a block is any kind of redstone dust (lit or unlit) */
static cc_bool IsRedstoneDust(BlockID block) {
	return block == BLOCK_RED_ORE_DUST || block == BLOCK_LIT_RED_ORE_DUST;
}

/* Helper: check if a block is any kind of button (pressed or unpressed) */
static cc_bool IsButton(BlockID block) {
	return block == BLOCK_BUTTON || block == BLOCK_BUTTON_PRESSED;
}

static cc_bool IsButtonPressed(BlockID block) {
	return block == BLOCK_BUTTON_PRESSED;
}

static cc_bool IsButtonUnpressed(BlockID block) {
	return block == BLOCK_BUTTON;
}

/* Helper: check if a block is any kind of lever (on or off) */
static cc_bool IsLever(BlockID block) {
	return block == BLOCK_LEVER || block == BLOCK_LEVER_ON;
}

static cc_bool IsLeverOn(BlockID block) {
	return block == BLOCK_LEVER_ON;
}

/* Helper: check if a block is any kind of pressure plate (pressed or unpressed) */
static cc_bool IsPressurePlate(BlockID block) {
	return block == BLOCK_PRESSURE_PLATE || block == BLOCK_PRESSURE_PLATE_PRESSED;
}

/* Helper: check if a block is any iron door variant */
static cc_bool IsIronDoor(BlockID block) {
	return block == BLOCK_IRON_DOOR || block == BLOCK_IRON_DOOR_NS_TOP
		|| block == BLOCK_IRON_DOOR_EW_BOTTOM || block == BLOCK_IRON_DOOR_EW_TOP
		|| block == BLOCK_IRON_DOOR_NS_OPEN_BOTTOM || block == BLOCK_IRON_DOOR_NS_OPEN_TOP
		|| block == BLOCK_IRON_DOOR_EW_OPEN_BOTTOM || block == BLOCK_IRON_DOOR_EW_OPEN_TOP;
}

static cc_bool IsIronDoorBottom(BlockID block) {
	return block == BLOCK_IRON_DOOR || block == BLOCK_IRON_DOOR_EW_BOTTOM
		|| block == BLOCK_IRON_DOOR_NS_OPEN_BOTTOM || block == BLOCK_IRON_DOOR_EW_OPEN_BOTTOM;
}

static cc_bool IsIronDoorTop(BlockID block) {
	return block == BLOCK_IRON_DOOR_NS_TOP || block == BLOCK_IRON_DOOR_EW_TOP
		|| block == BLOCK_IRON_DOOR_NS_OPEN_TOP || block == BLOCK_IRON_DOOR_EW_OPEN_TOP;
}

/* Helper: check if a block is any kind of redstone torch (lit or unlit, any direction) */
static cc_bool IsRedstoneTorch(BlockID block) {
	return block == BLOCK_RED_ORE_TORCH || block == BLOCK_RED_ORE_TORCH_OFF
		|| block == BLOCK_RED_TORCH_ON_S || block == BLOCK_RED_TORCH_ON_N
		|| block == BLOCK_RED_TORCH_ON_E || block == BLOCK_RED_TORCH_ON_W
		|| block == BLOCK_RED_TORCH_OFF_S || block == BLOCK_RED_TORCH_OFF_N
		|| block == BLOCK_RED_TORCH_OFF_E || block == BLOCK_RED_TORCH_OFF_W
		|| block == BLOCK_RED_TORCH_UNMOUNTED || block == BLOCK_RED_TORCH_UNMOUNTED_OFF;
}

/* Helper: check if a block is any torch (regular or redstone) */
static cc_bool IsAnyTorch(BlockID block) {
	return block == BLOCK_TORCH || IsRedstoneTorch(block);
}

/* Get redstone dust top texture based on neighboring redstone connections */
static TextureLoc RedOreDust_GetTexture(int x, int y, int z, Face face, cc_bool isLit) {
	cc_bool north, south, east, west;
	BlockID neighbor;
	int texOffset;
	
	/* Lit redstone textures are unlit + 16 */
	texOffset = isLit ? 16 : 0;
	
	/* Only top face changes based on connections */
	if (face != FACE_YMAX) {
		/* Sides use texture 86 (transparent) - same for lit and unlit */
		return 86;
	}
	
	/* Check for redstone dust neighbors in all 4 horizontal directions */
	/* Connect to both lit and unlit redstone dust, and to blocks with dust on top/below (pour-over) */
	north = south = east = west = false;
	
	/* Direct horizontal connections (dust or torch) */
	if (IsRedstoneDust(World_GetBlock(x, y, z - 1)) || IsRedstoneTorch(World_GetBlock(x, y, z - 1))) {
		north = true;
	} else {
		/* Pour-over: check one block down to the north */
		if (y > 0 && World_GetBlock(x, y, z - 1) == BLOCK_AIR && IsRedstoneDust(World_GetBlock(x, y - 1, z - 1)))
			north = true;
		/* Pour-over: check one block up to the north (if not blocked from above) */
		if (y < World.MaxY && IsRedstoneDust(World_GetBlock(x, y + 1, z - 1)) && World_GetBlock(x, y + 1, z) == BLOCK_AIR)
			north = true;
	}
	
	if (IsRedstoneDust(World_GetBlock(x, y, z + 1)) || IsRedstoneTorch(World_GetBlock(x, y, z + 1))) {
		south = true;
	} else {
		if (y > 0 && World_GetBlock(x, y, z + 1) == BLOCK_AIR && IsRedstoneDust(World_GetBlock(x, y - 1, z + 1)))
			south = true;
		if (y < World.MaxY && IsRedstoneDust(World_GetBlock(x, y + 1, z + 1)) && World_GetBlock(x, y + 1, z) == BLOCK_AIR)
			south = true;
	}
	
	if (IsRedstoneDust(World_GetBlock(x + 1, y, z)) || IsRedstoneTorch(World_GetBlock(x + 1, y, z))) {
		east = true;
	} else {
		if (y > 0 && World_GetBlock(x + 1, y, z) == BLOCK_AIR && IsRedstoneDust(World_GetBlock(x + 1, y - 1, z)))
			east = true;
		if (y < World.MaxY && IsRedstoneDust(World_GetBlock(x + 1, y + 1, z)) && World_GetBlock(x, y + 1, z) == BLOCK_AIR)
			east = true;
	}
	
	if (IsRedstoneDust(World_GetBlock(x - 1, y, z)) || IsRedstoneTorch(World_GetBlock(x - 1, y, z))) {
		west = true;
	} else {
		if (y > 0 && World_GetBlock(x - 1, y, z) == BLOCK_AIR && IsRedstoneDust(World_GetBlock(x - 1, y - 1, z)))
			west = true;
		if (y < World.MaxY && IsRedstoneDust(World_GetBlock(x - 1, y + 1, z)) && World_GetBlock(x, y + 1, z) == BLOCK_AIR)
			west = true;
	}
	
	/* Determine texture based on connection pattern */
	/* All 4 sides connected - cross */
	if (north && south && east && west) return 84 + texOffset;
	
	/* 3 sides connected - T-junctions */
	if (north && south && east) return 94 + texOffset; /* T pointing west */
	if (north && west && east) return 93 + texOffset;  /* T pointing south */
	if (north && south && west) return 92 + texOffset; /* T pointing east */
	if (west && south && east) return 95 + texOffset;  /* T pointing north */
	
	/* 2 sides connected - lines and corners */
	if (south && east) return 88 + texOffset; /* Corner SE */
	if (south && west) return 91 + texOffset; /* Corner SW */
	if (north && east) return 90 + texOffset; /* Corner NE */
	if (north && west) return 89 + texOffset; /* Corner NW */
	
	/* Lines (or single connections treated as lines) */
	if (north || south) return 87 + texOffset; /* Vertical line */
	if (east || west) return 85 + texOffset;   /* Horizontal line */
	
	/* No connections - dot (cross texture) */
	return 84 + texOffset;
}

/* Calculate which direction a directional block should face based on adjacent blocks */
static cc_uint8 CalcDirectionalFacing(int x, int y, int z) {
	cc_bool hasNorth, hasSouth, hasWest, hasEast;
	int validDirs[4], validCount = 0;
	BlockID block;
	
	/* Check adjacent blocks - only consider solid opaque blocks, not other directional blocks */
	hasNorth = false;
	if (World_Contains(x, y, z - 1)) {
		block = World_GetBlock(x, y, z - 1);
		hasNorth = (block != BLOCK_AIR && Blocks.Draw[block] == DRAW_OPAQUE && !IsDirectionalBlock(block));
	}
	
	hasSouth = false;
	if (World_Contains(x, y, z + 1)) {
		block = World_GetBlock(x, y, z + 1);
		hasSouth = (block != BLOCK_AIR && Blocks.Draw[block] == DRAW_OPAQUE && !IsDirectionalBlock(block));
	}
	
	hasWest = false;
	if (World_Contains(x - 1, y, z)) {
		block = World_GetBlock(x - 1, y, z);
		hasWest = (block != BLOCK_AIR && Blocks.Draw[block] == DRAW_OPAQUE && !IsDirectionalBlock(block));
	}
	
	hasEast = false;
	if (World_Contains(x + 1, y, z)) {
		block = World_GetBlock(x + 1, y, z);
		hasEast = (block != BLOCK_AIR && Blocks.Draw[block] == DRAW_OPAQUE && !IsDirectionalBlock(block));
	}
	
	/* Build list of valid directions (directions NOT blocked by adjacent blocks) */
	if (!hasNorth) validDirs[validCount++] = 0; /* Can face North */
	if (!hasSouth) validDirs[validCount++] = 1; /* Can face South */
	if (!hasWest)  validDirs[validCount++] = 2; /* Can face West */
	if (!hasEast)  validDirs[validCount++] = 3; /* Can face East */
	
	/* If no valid directions (surrounded), default to South */
	if (validCount == 0) return 1;
	
	/* If only one adjacent block, face away from it */
	if (validCount == 3) {
		if (hasNorth) return 1; /* Face South */
		if (hasSouth) return 0; /* Face North */
		if (hasWest)  return 3; /* Face East */
		if (hasEast)  return 2; /* Face West */
	}
	
	/* If multiple options, pick first valid direction (prioritize South > North > East > West) */
	if (validCount == 4 || validCount == 2) {
		/* Check in priority order */
		if (!hasSouth) return 1; /* Prefer South */
		if (!hasNorth) return 0; /* Then North */
		if (!hasEast)  return 3; /* Then East */
		if (!hasWest)  return 2; /* Then West */
	}
	
	/* Fallback: return first valid direction */
	return validDirs[0];
}



/* Add or update a directional block in the cache */
static void DirectionalCache_Set(int x, int y, int z, cc_uint8 facing) {
	int i;
	
	/* Check if already exists - update it */
	for (i = 0; i < directionalCount; i++) {
		if (directionalCache[i].x == x && 
		    directionalCache[i].y == y && 
		    directionalCache[i].z == z) {
			directionalCache[i].facing = facing;
			return;
		}
	}
	
	/* Add new entry if space available */
	if (directionalCount < MAX_DIRECTIONAL_BLOCKS) {
		directionalCache[directionalCount].x = x;
		directionalCache[directionalCount].y = y;
		directionalCache[directionalCount].z = z;
		directionalCache[directionalCount].facing = facing;
		directionalCount++;
	}
}

/* Get facing direction for a directional block, returns 1 (South) if not found */
static cc_uint8 DirectionalCache_Get(int x, int y, int z) {
	int i;
	for (i = 0; i < directionalCount; i++) {
		if (directionalCache[i].x == x && 
		    directionalCache[i].y == y && 
		    directionalCache[i].z == z) {
			return directionalCache[i].facing;
		}
	}
	return 1; /* Default to South */
}

/* Remove a directional block from cache */
static void DirectionalCache_Remove(int x, int y, int z) {
	int i;
	for (i = 0; i < directionalCount; i++) {
		if (directionalCache[i].x == x && 
		    directionalCache[i].y == y && 
		    directionalCache[i].z == z) {
			/* Move last element to this position */
			directionalCount--;
			directionalCache[i] = directionalCache[directionalCount];
			return;
		}
	}
}

/* Update directional block when placed or adjacent blocks change */
static void DirectionalBlock_Update(int x, int y, int z) {
	BlockID block;
	cc_uint8 facing;
	
	if (!directionalFacing_Enabled) return;
	if (!World_Contains(x, y, z)) return;
	
	block = World_GetBlock(x, y, z);
	if (!IsDirectionalBlock(block)) {
		DirectionalCache_Remove(x, y, z);
		return;
	}
	
	/* Buttons use wall-only facing logic (like torches but no ground) */
	if (IsButton(block)) {
		facing = CalcTorchFacing(x, y, z);
		/* Buttons can't be on ground (facing 4) - only walls (0-3) */
		if (facing == 255 || facing == 4) {
			return; /* No wall support */
		}
		DirectionalCache_Set(x, y, z, facing);
		return;
	}
	
	/* Levers use same wall-only facing logic as buttons */
	if (IsLever(block)) {
		facing = CalcTorchFacing(x, y, z);
		if (facing == 255 || facing == 4) {
			return; /* No wall support */
		}
		DirectionalCache_Set(x, y, z, facing);
		return;
	}
	
	/* Torches use different facing logic (wall preference + ground fallback) */
	if (IsAnyTorch(block)) {
		/* Wall-mounted redstone torch variants have fixed facing from block ID */
		if (block == BLOCK_RED_TORCH_ON_S || block == BLOCK_RED_TORCH_OFF_S) {
			facing = 0; /* Attached to z+1 */
		} else if (block == BLOCK_RED_TORCH_ON_N || block == BLOCK_RED_TORCH_OFF_N) {
			facing = 1; /* Attached to z-1 */
		} else if (block == BLOCK_RED_TORCH_ON_E || block == BLOCK_RED_TORCH_OFF_E) {
			facing = 2; /* Attached to x+1 */
		} else if (block == BLOCK_RED_TORCH_ON_W || block == BLOCK_RED_TORCH_OFF_W) {
			facing = 3; /* Attached to x-1 */
		} else if (block == BLOCK_RED_TORCH_UNMOUNTED || block == BLOCK_RED_TORCH_UNMOUNTED_OFF) {
			facing = 4; /* Free-standing, always ground/upright */
			DirectionalCache_Set(x, y, z, facing);
			return;
		} else {
			facing = CalcTorchFacing(x, y, z);
			if (facing == 255) {
				/* No support - leave in cache with old facing, physics will break it */
				return;
			}
		}
		DirectionalCache_Set(x, y, z, facing);
		return;
	}
	
	/* All other directional blocks use adjacent block logic */
	facing = CalcDirectionalFacing(x, y, z);
	
	DirectionalCache_Set(x, y, z, facing);
}

/* Clear all directional cache */
static void DirectionalCache_Clear(void) {
	directionalCount = 0;
}

/* Scan entire world and rebuild directional cache */
static void DirectionalCache_Rebuild(void) {
	int x, y, z;
	BlockID block;
	
	DirectionalCache_Clear();
	if (!directionalFacing_Enabled) return;
	
	for (y = 0; y < World.Height; y++) {
		for (z = 0; z < World.Length; z++) {
			for (x = 0; x < World.Width; x++) {
				block = World_GetBlock(x, y, z);
				if (IsDirectionalBlock(block)) {
					DirectionalBlock_Update(x, y, z);
				}
			}
		}
	}
}

/* Get texture for a directional block face at a specific position */
TextureLoc DirectionalBlock_GetTexture(BlockID block, int x, int y, int z, Face face) {
	cc_uint8 facing;
	TextureLoc frontTex, sideTex, topTex, bottomTex;
	
	/* Red ore dust uses connection-based textures */
	if (block == BLOCK_RED_ORE_DUST) {
		return RedOreDust_GetTexture(x, y, z, face, false);
	}
	
	/* Lit red ore dust uses lit connection-based textures */
	if (block == BLOCK_LIT_RED_ORE_DUST) {
		return RedOreDust_GetTexture(x, y, z, face, true);
	}
	
	if (!directionalFacing_Enabled || !IsDirectionalBlock(block)) {
		/* Use default texture */
		return Blocks.Textures[block * FACE_COUNT + face];
	}
	
	/* Get textures for this block type */
	if (block == BLOCK_FURNACE) {
		frontTex = 46; sideTex = 47; topTex = 1; bottomTex = 1;
	} else if (block == BLOCK_CHEST) {
		frontTex = 44; sideTex = 27; topTex = 26; bottomTex = 26;
	} else if (block == BLOCK_LADDER) {
		/* Ladder uses same texture on all sides */
		frontTex = 83; sideTex = 83; topTex = 83; bottomTex = 83;
	} else {
		return Blocks.Textures[block * FACE_COUNT + face];
	}
	
	/* Ladders only render on the side they're attached to */
	if (block == BLOCK_LADDER) {
		/* Get facing direction from cache */
		facing = DirectionalCache_Get(x, y, z);
		
		/* Ladder texture shows on the visible face (facing player), other faces are transparent */
		/* facing: 0=North(-Z), 1=South(+Z), 2=West(-X), 3=East(+X) */
		switch (facing) {
			case 0: /* Facing North - attached to south wall, texture on ZMIN */
				return (face == FACE_ZMIN) ? 83 : 86;
			case 1: /* Facing South - attached to north wall, texture on ZMAX */
				return (face == FACE_ZMAX) ? 83 : 86;
			case 2: /* Facing West - attached to east wall, texture on XMIN */
				return (face == FACE_XMIN) ? 83 : 86;
			case 3: /* Facing East - attached to west wall, texture on XMAX */
				return (face == FACE_XMAX) ? 83 : 86;
		}
		return 86;
	}
	
	/* Torch uses texture 80 - sprite rendering handles display */
	if (block == BLOCK_TORCH) {
		return 80;
	}
	
	/* Red Ore Torch uses texture 99 */
	if (block == BLOCK_RED_ORE_TORCH || block == BLOCK_RED_TORCH_ON_S || block == BLOCK_RED_TORCH_ON_N || block == BLOCK_RED_TORCH_ON_E || block == BLOCK_RED_TORCH_ON_W || block == BLOCK_RED_TORCH_UNMOUNTED) {
		return 99;
	}
	
	/* Unlit Red Ore Torch uses texture 115 */
	if (block == BLOCK_RED_ORE_TORCH_OFF || block == BLOCK_RED_TORCH_OFF_S || block == BLOCK_RED_TORCH_OFF_N || block == BLOCK_RED_TORCH_OFF_E || block == BLOCK_RED_TORCH_OFF_W || block == BLOCK_RED_TORCH_UNMOUNTED_OFF) {
		return 115;
	}
	
	/* Top and bottom don't change for chest/furnace */
	if (face == FACE_YMAX) return topTex;
	if (face == FACE_YMIN) return bottomTex;
	
	/* Get facing direction from cache */
	facing = DirectionalCache_Get(x, y, z);
	
	/* Return appropriate texture based on facing and requested face */
	/* facing: 0=North(-Z), 1=South(+Z), 2=West(-X), 3=East(+X) */
	switch (facing) {
		case 0: /* Facing North (-Z) */
			return (face == FACE_ZMIN) ? frontTex : sideTex;
		case 1: /* Facing South (+Z) */
			return (face == FACE_ZMAX) ? frontTex : sideTex;
		case 2: /* Facing West (-X) */
			return (face == FACE_XMIN) ? frontTex : sideTex;
		case 3: /* Facing East (+X) */
			return (face == FACE_XMAX) ? frontTex : sideTex;
	}
	
	return sideTex;
}



/* Get render bounds for directional blocks (ladders only) based on their facing */
void DirectionalBlock_GetRenderBounds(BlockID block, int x, int y, int z, Vec3* min, Vec3* max) {
	cc_uint8 facing;
	
	/* Doors always use their custom render bounds */
	if (block == BLOCK_DOOR_NS_BOTTOM || block == BLOCK_DOOR_NS_TOP || 
	    block == BLOCK_DOOR_EW_BOTTOM || block == BLOCK_DOOR_EW_TOP ||
	    IsIronDoor(block)) {
		*min = Blocks.RenderMinBB[block];
		*max = Blocks.RenderMaxBB[block];
		return;
	}
	
	/* Only ladders, torches, and buttons need dynamic render bounds */
	if (block != BLOCK_LADDER && !IsAnyTorch(block) && !IsButton(block) && !IsLever(block)) {
		if (!directionalFacing_Enabled) {
			*min = Blocks.RenderMinBB[block];
			*max = Blocks.RenderMaxBB[block];
			return;
		}
		*min = Blocks.RenderMinBB[block];
		*max = Blocks.RenderMaxBB[block];
		return;
	}
	
	/* Get facing direction from cache */
	facing = DirectionalCache_Get(x, y, z);
	
	if (IsAnyTorch(block)) {
		switch (facing) {
			case 4: /* Ground torch */
				min->x = 7.0f/16.0f; min->y = 0; min->z = 7.0f/16.0f;
				max->x = 9.0f/16.0f; max->y = 10.0f/16.0f; max->z = 9.0f/16.0f;
				break;
			case 0: /* North - wall at +Z */
				min->x = 5.0f/16.0f; min->y = 3.0f/16.0f; min->z = 0;
				max->x = 11.0f/16.0f; max->y = 13.0f/16.0f; max->z = 1;
				break;
			case 1: /* South - wall at -Z */
				min->x = 5.0f/16.0f; min->y = 3.0f/16.0f; min->z = 0;
				max->x = 11.0f/16.0f; max->y = 13.0f/16.0f; max->z = 1;
				break;
			case 2: /* West - wall at +X */
				min->x = 0; min->y = 3.0f/16.0f; min->z = 5.0f/16.0f;
				max->x = 1; max->y = 13.0f/16.0f; max->z = 11.0f/16.0f;
				break;
			case 3: /* East - wall at -X */
				min->x = 0; min->y = 3.0f/16.0f; min->z = 5.0f/16.0f;
				max->x = 1; max->y = 13.0f/16.0f; max->z = 11.0f/16.0f;
				break;
			default:
				*min = Blocks.RenderMinBB[block];
				*max = Blocks.RenderMaxBB[block];
				break;
		}
		return;
	}
	
	if (IsButton(block)) {
		cc_bool pressed = IsButtonPressed(block);
		float depth = pressed ? 1.0f/16.0f : 2.0f/16.0f;
		switch (facing) {
			case 0: /* Attached to south wall (+Z) */
				min->x = 5.0f/16.0f; min->y = 6.0f/16.0f; min->z = 1.0f - depth;
				max->x = 11.0f/16.0f; max->y = 10.0f/16.0f; max->z = 1;
				break;
			case 1: /* Attached to north wall (-Z) */
				min->x = 5.0f/16.0f; min->y = 6.0f/16.0f; min->z = 0;
				max->x = 11.0f/16.0f; max->y = 10.0f/16.0f; max->z = depth;
				break;
			case 2: /* Attached to east wall (+X) */
				min->x = 1.0f - depth; min->y = 6.0f/16.0f; min->z = 5.0f/16.0f;
				max->x = 1; max->y = 10.0f/16.0f; max->z = 11.0f/16.0f;
				break;
			case 3: /* Attached to west wall (-X) */
				min->x = 0; min->y = 6.0f/16.0f; min->z = 5.0f/16.0f;
				max->x = depth; max->y = 10.0f/16.0f; max->z = 11.0f/16.0f;
				break;
			default:
				*min = Blocks.RenderMinBB[block];
				*max = Blocks.RenderMaxBB[block];
				break;
		}
		return;
	}
	
	/* Lever base: 6 pixels wide, 8 pixels tall, 3 pixels deep */
	if (IsLever(block)) {
		switch (facing) {
			case 0: /* Attached to south wall (+Z) */
				min->x = 5.0f/16.0f; min->y = 4.0f/16.0f; min->z = 13.0f/16.0f;
				max->x = 11.0f/16.0f; max->y = 12.0f/16.0f; max->z = 1;
				break;
			case 1: /* Attached to north wall (-Z) */
				min->x = 5.0f/16.0f; min->y = 4.0f/16.0f; min->z = 0;
				max->x = 11.0f/16.0f; max->y = 12.0f/16.0f; max->z = 3.0f/16.0f;
				break;
			case 2: /* Attached to east wall (+X) */
				min->x = 13.0f/16.0f; min->y = 4.0f/16.0f; min->z = 5.0f/16.0f;
				max->x = 1; max->y = 12.0f/16.0f; max->z = 11.0f/16.0f;
				break;
			case 3: /* Attached to west wall (-X) */
				min->x = 0; min->y = 4.0f/16.0f; min->z = 5.0f/16.0f;
				max->x = 3.0f/16.0f; max->y = 12.0f/16.0f; max->z = 11.0f/16.0f;
				break;
			default:
				*min = Blocks.RenderMinBB[block];
				*max = Blocks.RenderMaxBB[block];
				break;
		}
		return;
	}
	
	/* Set render bounds based on facing direction */
	/* facing: 0=North(-Z), 1=South(+Z), 2=West(-X), 3=East(+X) */
	
	/* Ladder bounds - thin against wall */
	switch (facing) {
		case 0: /* Facing North - attached to south wall, thin in +Z */
			min->x = 0; min->y = 0; min->z = 15.0f/16.0f;
			max->x = 1; max->y = 1; max->z = 1;
			break;
		case 1: /* Facing South - attached to north wall, thin in -Z */
			min->x = 0; min->y = 0; min->z = 0;
			max->x = 1; max->y = 1; max->z = 1.0f/16.0f;
			break;
		case 2: /* Facing West - attached to east wall, thin in +X */
			min->x = 15.0f/16.0f; min->y = 0; min->z = 0;
			max->x = 1; max->y = 1; max->z = 1;
			break;
		case 3: /* Facing East - attached to west wall, thin in -X */
			min->x = 0; min->y = 0; min->z = 0;
			max->x = 1.0f/16.0f; max->y = 1; max->z = 1;
			break;
		default:
			*min = Blocks.RenderMinBB[block];
			*max = Blocks.RenderMaxBB[block];
			break;
	}
}

/* Get facing direction from directional cache (exposed for Builder) */
cc_uint8 DirectionalBlock_GetFacing(int x, int y, int z) {
	return DirectionalCache_Get(x, y, z);
}


/*########################################################################################################################*
*--------------------------------------------------Block bounds/culling---------------------------------------------------*
*#########################################################################################################################*/
/* Calculates render min/max corners of this block */
/* Works by slightly offsetting collision min/max corners */
static void Block_CalcRenderBounds(BlockID block) {
	Vec3 min = Blocks.MinBB[block], max = Blocks.MaxBB[block];

	if (Blocks.IsLiquid[block]) {
		min.x += 0.1f/16.0f; max.x += 0.1f/16.0f;
		min.z += 0.1f/16.0f; max.z += 0.1f/16.0f;
		min.y -= 1.5f/16.0f; max.y -= 1.5f/16.0f;
	} else if (Blocks.Draw[block] == DRAW_TRANSLUCENT && Blocks.Collide[block] != COLLIDE_SOLID) {
		min.x += 0.1f/16.0f; max.x += 0.1f/16.0f;
		min.z += 0.1f/16.0f; max.z += 0.1f/16.0f;
		min.y -= 0.1f/16.0f; max.y -= 0.1f/16.0f;
	} else if (block == BLOCK_DOOR_NS_BOTTOM || block == BLOCK_DOOR_NS_TOP) {
		/* NS doors: thin render in Z (3 pixels thick, at edge) */
		min.x = 0; min.y = 0; min.z = 13.0f/16.0f;
		max.x = 1; max.y = 1; max.z = 1;
	} else if (block == BLOCK_DOOR_EW_BOTTOM || block == BLOCK_DOOR_EW_TOP) {
		/* EW doors: thin render in X (3 pixels thick, at opposite edge) */
		min.x = 0; min.y = 0; min.z = 0;
		max.x = 3.0f/16.0f; max.y = 1; max.z = 1;
	} else if (block == BLOCK_IRON_DOOR || block == BLOCK_IRON_DOOR_NS_TOP ||
	           block == BLOCK_IRON_DOOR_EW_OPEN_BOTTOM || block == BLOCK_IRON_DOOR_EW_OPEN_TOP) {
		/* Iron doors with NS geometry: thin in Z */
		min.x = 0; min.y = 0; min.z = 13.0f/16.0f;
		max.x = 1; max.y = 1; max.z = 1;
	} else if (block == BLOCK_IRON_DOOR_EW_BOTTOM || block == BLOCK_IRON_DOOR_EW_TOP ||
	           block == BLOCK_IRON_DOOR_NS_OPEN_BOTTOM || block == BLOCK_IRON_DOOR_NS_OPEN_TOP) {
		/* Iron doors with EW geometry: thin in X */
		min.x = 0; min.y = 0; min.z = 0;
		max.x = 3.0f/16.0f; max.y = 1; max.z = 1;
	}

	Blocks.RenderMinBB[block] = min; Blocks.RenderMaxBB[block] = max;
}

/* Calculates light colour offset for each face of the given block */
static void Block_CalcLightOffset(BlockID block) {
	int flags = 0xFF;
	Vec3 min = Blocks.MinBB[block], max = Blocks.MaxBB[block];

	if (min.x != 0) flags &= ~FACE_BIT_XMIN;
	if (max.x != 1) flags &= ~FACE_BIT_XMAX;
	if (min.z != 0) flags &= ~FACE_BIT_ZMIN;
	if (max.z != 1) flags &= ~FACE_BIT_ZMAX;
	if (min.y != 0) flags &= ~FACE_BIT_YMIN;
	if (max.y != 1) flags &= ~FACE_BIT_YMAX;

	if ((min.y != 0 && max.y == 1) && Blocks.Draw[block] != DRAW_GAS) {
		flags &= ~(1 << LIGHT_FLAG_SHADES_FROM_BELOW);
	}
	Blocks.LightOffset[block] = flags;
}


static float GetSpriteBB_MinX(int size, int tileX, int tileY, const struct Bitmap* bmp) {
	BitmapCol* row;
	int x, y;

	for (x = 0; x < size; x++) {
		for (y = 0; y < size; y++) {
			row = Bitmap_GetRow(bmp, tileY * size + y) + (tileX * size);
			if (BitmapCol_A(row[x])) { return (float)x / size; }
		}
	}
	return 1.0f;
}

static float GetSpriteBB_MinY(int size, int tileX, int tileY, const struct Bitmap* bmp) {
	BitmapCol* row;
	int x, y;

	for (y = size - 1; y >= 0; y--) {
		row = Bitmap_GetRow(bmp, tileY * size + y) + (tileX * size);
		for (x = 0; x < size; x++) {
			if (BitmapCol_A(row[x])) { return 1.0f - (float)(y + 1) / size; }
		}
	}
	return 1.0f;
}

static float GetSpriteBB_MaxX(int size, int tileX, int tileY, const struct Bitmap* bmp) {
	BitmapCol* row;
	int x, y;

	for (x = size - 1; x >= 0; x--) {
		for (y = 0; y < size; y++) {
			row = Bitmap_GetRow(bmp, tileY * size + y) + (tileX * size);
			if (BitmapCol_A(row[x])) { return (float)(x + 1) / size; }
		}
	}
	return 0.0f;
}

static float GetSpriteBB_MaxY(int size, int tileX, int tileY, const struct Bitmap* bmp) {
	BitmapCol* row;
	int x, y;

	for (y = 0; y < size; y++) {
		row = Bitmap_GetRow(bmp, tileY * size + y) + (tileX * size);
		for (x = 0; x < size; x++) {
			if (BitmapCol_A(row[x])) { return 1.0f - (float)y / size; }
		}
	}
	return 0.0f;
}

/* Recalculates bounding box of the given sprite block */
static void Block_RecalculateBB(BlockID block) {
	struct Bitmap* bmp = &Atlas2D.Bmp;
	int tileSize = Atlas2D.TileSize;
	TextureLoc texLoc = Block_Tex(block, FACE_XMAX);
	int x = Atlas2D_TileX(texLoc), y = Atlas2D_TileY(texLoc);

	Vec3 centre = { 0.5f, 0.0f, 0.5f };
	float minX = 0, minY = 0, maxX = 1, maxY = 1;
	Vec3 minRaw, maxRaw;

	if (y < Atlas2D.RowsCount) {
		minX = GetSpriteBB_MinX(tileSize, x, y, bmp);
		minY = GetSpriteBB_MinY(tileSize, x, y, bmp);
		maxX = GetSpriteBB_MaxX(tileSize, x, y, bmp);
		maxY = GetSpriteBB_MaxY(tileSize, x, y, bmp);
	}

	minRaw = Vec3_RotateY3(minX - 0.5f, minY, 0.0f, 45.0f * MATH_DEG2RAD);
	maxRaw = Vec3_RotateY3(maxX - 0.5f, maxY, 0.0f, 45.0f * MATH_DEG2RAD);
	Vec3_Add(&Blocks.MinBB[block], &minRaw, &centre);
	Vec3_Add(&Blocks.MaxBB[block], &maxRaw, &centre);
	Block_CalcRenderBounds(block);
}

/* Recalculates bounding boxes of all sprite blocks */
static void Block_RecalculateAllSpriteBB(void) {
	int block;
	for (block = BLOCK_AIR; block < BLOCK_COUNT; block++) {
		if (Blocks.Draw[block] != DRAW_SPRITE) continue;

		Block_RecalculateBB((BlockID)block);
	}
}


static void Block_CalcStretch(BlockID block) {
	/* faces which can be stretched on X axis */
	if (Blocks.MinBB[block].x == 0.0f && Blocks.MaxBB[block].x == 1.0f) {
		Blocks.CanStretch[block] |= 0x3C;
	} else {
		Blocks.CanStretch[block] &= 0xC3; /* ~0x3C */
	}

	/* faces which can be stretched on Z axis */
	if (Blocks.MinBB[block].z == 0.0f && Blocks.MaxBB[block].z == 1.0f) {
		Blocks.CanStretch[block] |= 0x03;
	} else {
		Blocks.CanStretch[block] &= 0xFC; /* ~0x03 */
	}

	/* Redstone dust and torches use position-dependent dynamic rendering, so */
	/*  adjacent blocks must never be merged into a single stretch */
	if (block == BLOCK_RED_ORE_DUST || block == BLOCK_LIT_RED_ORE_DUST ||
	    IsAnyTorch(block)) {
		Blocks.CanStretch[block] = 0;
	}

	#if defined CC_BUILD_PS1 || defined CC_BUILD_SATURN
	Blocks.CanStretch[block] = 0;
	#endif
}

static cc_bool Block_MightCull(BlockID block, BlockID other) {
	cc_uint8 bType, oType;
	/* Sprite blocks can never cull blocks. */
	if (Blocks.Draw[block] == DRAW_SPRITE) return false;

	/* NOTE: Water is always culled by lava */
	if ((block == BLOCK_WATER || block == BLOCK_STILL_WATER)
		&& (other == BLOCK_LAVA || other == BLOCK_STILL_LAVA))
		return true;

	/* All blocks (except for say leaves) cull with themselves */
	if (block == other) return Blocks.Draw[block] != DRAW_TRANSPARENT_THICK;

	/* An opaque neighbour (asides from lava) culls this block. */
	if (Blocks.Draw[other] == DRAW_OPAQUE && !Blocks.IsLiquid[other]) return true;
	/* Transparent/Gas blocks don't cull other blocks (except themselves) */
	if (Blocks.Draw[block] != DRAW_TRANSLUCENT || Blocks.Draw[other] != DRAW_TRANSLUCENT) return false;

	/* Some translucent blocks may still cull other translucent blocks */
	/* e.g. for water/ice, don't need to draw faces of water */
	bType = Blocks.Collide[block]; oType = Blocks.Collide[other]; 
	return (bType == COLLIDE_SOLID && oType == COLLIDE_SOLID) || bType != COLLIDE_SOLID;
}

static void Block_CalcCulling(BlockID block, BlockID other) {
	Vec3 bMin, bMax, oMin, oMax;
	cc_bool occludedX, occludedY, occludedZ, bothLiquid;
	int f;

	/* Fast path: Full opaque neighbouring blocks will always have all shared faces hidden */
	if (Blocks.FullOpaque[block] && Blocks.FullOpaque[other]) {
		Blocks.Hidden[(block * BLOCK_COUNT) + other] = 0x3F;
		return;
	}

	/* Some blocks may not cull 'other' block, in which case just skip detailed check */
	/* e.g. sprite blocks, default leaves, will not cull any other blocks */
	if (!Block_MightCull(block, other)) {	
		Blocks.Hidden[(block * BLOCK_COUNT) + other] = 0;
		return;
	}

	bMin = Blocks.MinBB[block]; bMax = Blocks.MaxBB[block];
	oMin = Blocks.MinBB[other]; oMax = Blocks.MaxBB[other];

	/* Extend offsets of liquid down to match rendered position */
	/* This isn't completely correct, but works well enough */
	if (Blocks.IsLiquid[block]) bMax.y -= 1.50f / 16.0f;
	if (Blocks.IsLiquid[other]) oMax.y -= 1.50f / 16.0f;

	bothLiquid = Blocks.IsLiquid[block] && Blocks.IsLiquid[other];
	f = 0; /* mark all faces initially 'not hidden' */

	/* Whether the 'texture region' of a face on block fits inside corresponding region on other block */
	occludedX = (bMin.z >= oMin.z && bMax.z <= oMax.z) && (bMin.y >= oMin.y && bMax.y <= oMax.y);
	occludedY = (bMin.x >= oMin.x && bMax.x <= oMax.x) && (bMin.z >= oMin.z && bMax.z <= oMax.z);
	occludedZ = (bMin.x >= oMin.x && bMax.x <= oMax.x) && (bMin.y >= oMin.y && bMax.y <= oMax.y);

	f |= occludedX && oMax.x == 1.0f && bMin.x == 0.0f ? FACE_BIT_XMIN : 0;
	f |= occludedX && oMin.x == 0.0f && bMax.x == 1.0f ? FACE_BIT_XMAX : 0;
	f |= occludedZ && oMax.z == 1.0f && bMin.z == 0.0f ? FACE_BIT_ZMIN : 0;
	f |= occludedZ && oMin.z == 0.0f && bMax.z == 1.0f ? FACE_BIT_ZMAX : 0;
	f |= occludedY && (bothLiquid || (oMax.y == 1.0f && bMin.y == 0.0f)) ? FACE_BIT_YMIN : 0;
	f |= occludedY && (bothLiquid || (oMin.y == 0.0f && bMax.y == 1.0f)) ? FACE_BIT_YMAX : 0;
	Blocks.Hidden[(block * BLOCK_COUNT) + other] = f;
}

/* Updates culling data of all blocks */
static void Block_UpdateAllCulling(void) {
	int block, neighbour;

	for (block = BLOCK_AIR; block < BLOCK_COUNT; block++) {
		Block_CalcStretch((BlockID)block);
		for (neighbour = BLOCK_AIR; neighbour < BLOCK_COUNT; neighbour++) {
			Block_CalcCulling((BlockID)block, (BlockID)neighbour);
		}
	}
}

/* Updates culling data just for this block */
/* (e.g. whether block can be stretched, visibility with other blocks) */
static void Block_UpdateCulling(BlockID block) {
	int neighbour;
	Block_CalcStretch(block);
	
	for (neighbour = BLOCK_AIR; neighbour < BLOCK_COUNT; neighbour++) {
		Block_CalcCulling(block, (BlockID)neighbour);
		Block_CalcCulling((BlockID)neighbour, block);
	}
}


/*########################################################################################################################*
*---------------------------------------------------------Block-----------------------------------------------------------*
*#########################################################################################################################*/
static cc_uint32 definedCustomBlocks[BLOCK_COUNT >> 5];
static CC_BIG_VAR char Block_NamesBuffer[STRING_SIZE * BLOCK_COUNT];
#define Block_NamePtr(i) &Block_NamesBuffer[STRING_SIZE * i]

cc_bool Block_IsCustomDefined(BlockID block) {
	return (definedCustomBlocks[block >> 5] & (1u << (block & 0x1F))) != 0;
}

/* Sets whether the given block has been changed from default */
static void Block_SetCustomDefined(BlockID block, cc_bool defined) {
	if (defined) {
		definedCustomBlocks[block >> 5] |=  (1u << (block & 0x1F));
	} else {
		definedCustomBlocks[block >> 5] &= ~(1u << (block & 0x1F));
	}
}

void Block_DefineCustom(BlockID block, cc_bool checkSprite) {
	PackedCol black  = PackedCol_Make(0, 0, 0, 255);
	cc_string name   = Block_UNSAFE_GetName(block);
	/* necessary if servers redefined core blocks, before extended collide types were added */
	cc_uint8 collide = DefaultSet_MapOldCollide(block, Blocks.Collide[block]);
	Blocks.Tinted[block] = Blocks.FogCol[block] != black && String_IndexOf(&name, '#') >= 0;

	Block_SetCollide(block,  collide);
	Block_SetDrawType(block, Blocks.Draw[block]);
	Block_CalcRenderBounds(block);
	Block_UpdateCulling(block);
	Block_CalcLightOffset(block);

	Inventory_AddDefault(block);
	Block_SetCustomDefined(block, true);
	Event_RaiseVoid(&BlockEvents.BlockDefChanged);

	if (!checkSprite) return; /* TODO eliminate this */
	/* Update sprite BoundingBox if necessary */
	if (Blocks.Draw[block] == DRAW_SPRITE) Block_RecalculateBB(block);
}

void Block_UndefineCustom(BlockID block) {
	Block_ResetProps(block);
	Block_UpdateCulling(block);

	Inventory_Remove(block);
	if (block <= BLOCK_MAX_CPE) { Inventory_AddDefault(block); }

	Block_SetCustomDefined(block, false);
	Event_RaiseVoid(&BlockEvents.BlockDefChanged);

	/* Update sprite BoundingBox if necessary */
	if (Blocks.Draw[block] == DRAW_SPRITE) Block_RecalculateBB(block);
}

void Block_ResetProps(BlockID block) {
	const struct SimpleBlockDef* def;
	cc_string name;
	
	/* Special handling for high ID door blocks */
	if (block == BLOCK_DOOR_NS_TOP) {
		def = &door_ns_top_def;
	} else if (block == BLOCK_DOOR_EW_BOTTOM) {
		def = &door_ew_bottom_def;
	} else if (block == BLOCK_DOOR_EW_TOP) {
		def = &door_ew_top_def;
	} else if (block == BLOCK_LIT_RED_ORE_DUST) {
		def = &lit_red_ore_dust_def;
	} else if (block == BLOCK_RED_ORE_TORCH_OFF) {
		def = &red_ore_torch_off_def;
	} else if (block == BLOCK_RED_TORCH_ON_S || block == BLOCK_RED_TORCH_ON_N || block == BLOCK_RED_TORCH_ON_E || block == BLOCK_RED_TORCH_ON_W) {
		def = &red_torch_wall_on_def;
	} else if (block == BLOCK_RED_TORCH_OFF_S || block == BLOCK_RED_TORCH_OFF_N || block == BLOCK_RED_TORCH_OFF_E || block == BLOCK_RED_TORCH_OFF_W) {
		def = &red_torch_wall_off_def;
	} else if (block == BLOCK_RED_TORCH_UNMOUNTED) {
		def = &red_torch_unmounted_def;
	} else if (block == BLOCK_RED_TORCH_UNMOUNTED_OFF) {
		def = &red_torch_unmounted_off_def;
	} else if (block == BLOCK_BUTTON_PRESSED) {
		def = &button_pressed_def;
	} else if (block == BLOCK_LEVER_ON) {
		def = &lever_on_def;
	} else if (block == BLOCK_PRESSURE_PLATE_PRESSED) {
		def = &pressure_plate_pressed_def;
	} else if (block == BLOCK_IRON_DOOR_NS_TOP || block == BLOCK_IRON_DOOR_EW_BOTTOM || block == BLOCK_IRON_DOOR_EW_TOP ||
	           block == BLOCK_IRON_DOOR_NS_OPEN_BOTTOM || block == BLOCK_IRON_DOOR_NS_OPEN_TOP ||
	           block == BLOCK_IRON_DOOR_EW_OPEN_BOTTOM || block == BLOCK_IRON_DOOR_EW_OPEN_TOP) {
		def = &iron_door_def;
	} else if (block >= BLOCK_DCHEST_S_L && block <= BLOCK_DCHEST_W_R) {
		def = &double_chest_def;
	} else if (block == BLOCK_SHADOW_CEILING) {
		def = &shadow_ceiling_def;
	} else if (block == BLOCK_SNOWY_GRASS) {
		def = &snowy_grass_def;
	} else {
		def = block <= Game_Version.MaxCoreBlock ? &core_blockDefs[block] : &invalid_blockDef;
	}
	
	name = String_FromReadonly(def->name);

	Blocks.BlocksLight[block] = def->blocksLight;
	Blocks.Brightness[block]  = def->brightness;
	Blocks.FogCol[block]      = def->fogColor;
	Blocks.FogDensity[block]  = def->fogDensity / 100.0f;
	Block_SetCollide(block,     def->collide);
	Blocks.DigSounds[block]   = def->digSound;
	Blocks.StepSounds[block]  = def->stepSound;
	Blocks.SpeedMultiplier[block] = 1.0f;
	Block_SetName(block, &name);
	Blocks.Tinted[block]       = false;
	Blocks.SpriteOffset[block] = 0;

	Blocks.Draw[block] = def->draw;
	if (def->draw == DRAW_SPRITE) {
		Vec3_Set(Blocks.MinBB[block], 2.50f/16.0f, 0, 2.50f/16.0f);
		Vec3_Set(Blocks.MaxBB[block], 13.5f/16.0f, 1, 13.5f/16.0f);
	} else if (block == BLOCK_DOOR_NS_BOTTOM || block == BLOCK_DOOR_NS_TOP) {
		/* North/South doors: thin collision in Z (3 pixels thick, at edge) */
		Vec3_Set(Blocks.MinBB[block], 0, 0, 13.0f/16.0f);
		Vec3_Set(Blocks.MaxBB[block], 1, 1, 1);
	} else if (block == BLOCK_DOOR_EW_BOTTOM || block == BLOCK_DOOR_EW_TOP) {
		/* East/West doors: thin collision in X (3 pixels thick, at opposite edge) */
		Vec3_Set(Blocks.MinBB[block], 0, 0, 0);
		Vec3_Set(Blocks.MaxBB[block], 3.0f/16.0f, 1, 1);
	} else if (block == BLOCK_IRON_DOOR || block == BLOCK_IRON_DOOR_NS_TOP ||
	           block == BLOCK_IRON_DOOR_EW_OPEN_BOTTOM || block == BLOCK_IRON_DOOR_EW_OPEN_TOP) {
		/* Iron doors with NS geometry: thin collision in Z (3px thick at edge) */
		Vec3_Set(Blocks.MinBB[block], 0, 0, 13.0f/16.0f);
		Vec3_Set(Blocks.MaxBB[block], 1, 1, 1);
	} else if (block == BLOCK_IRON_DOOR_EW_BOTTOM || block == BLOCK_IRON_DOOR_EW_TOP ||
	           block == BLOCK_IRON_DOOR_NS_OPEN_BOTTOM || block == BLOCK_IRON_DOOR_NS_OPEN_TOP) {
		/* Iron doors with EW geometry: thin collision in X (3px thick at edge) */
		Vec3_Set(Blocks.MinBB[block], 0, 0, 0);
		Vec3_Set(Blocks.MaxBB[block], 3.0f/16.0f, 1, 1);
	} else {		
		Vec3_Set(Blocks.MinBB[block], 0, 0,                   0);
		Vec3_Set(Blocks.MaxBB[block], 1, def->height / 16.0f, 1);
	}

	Block_SetDrawType(block, def->draw);
	Block_CalcRenderBounds(block);
	Block_CalcLightOffset(block);

	Block_Tex(block, FACE_YMAX) = def->topTexture;
	Block_Tex(block, FACE_YMIN) = def->bottomTexture;
	Block_SetSide(def->sideTexture, block);

	/* Apply custom face textures for blocks with different textures on different sides */
	if (block == BLOCK_CRAFT) {
		/* Workbench: E/W (X) = 62, N/S (Z) = 61, top = 45, bottom = 4 */
		Block_SetCustomFaces(block, 62, 62, 61, 61);
	} else if (block == BLOCK_FURNACE) {
		/* Furnace: front (Z+) = 46, other sides = 47, top/bottom = 1 */
		Block_SetCustomFaces(block, 47, 47, 46, 47);
	} else if (block == BLOCK_CHEST) {
		/* Chest: front (Z+) = 44, other sides = 27, top/bottom = 26 */
		Block_SetCustomFaces(block, 27, 27, 44, 27);
	} else if (block == BLOCK_DOOR_NS_BOTTOM || block == BLOCK_DOOR_NS_TOP) {
		/* NS doors: different textures for front (ZMAX) and back (ZMIN) faces */
		TextureLoc frontTex = (block == BLOCK_DOOR_NS_BOTTOM) ? 97 : 81;
		TextureLoc backTex = (block == BLOCK_DOOR_NS_BOTTOM) ? 130 : 114;
		Block_Tex(block, FACE_ZMIN) = backTex;  /* Back face */
		Block_Tex(block, FACE_ZMAX) = frontTex; /* Front face */
		Block_Tex(block, FACE_XMIN) = 4; /* Oak planks on thin edges */
		Block_Tex(block, FACE_XMAX) = 4;
		Block_Tex(block, FACE_YMAX) = 4; /* Oak planks on top/bottom */
		Block_Tex(block, FACE_YMIN) = 4;
	} else if (block == BLOCK_DOOR_EW_BOTTOM || block == BLOCK_DOOR_EW_TOP) {
		/* EW doors: different textures for front (XMAX) and back (XMIN) faces */
		TextureLoc frontTex = (block == BLOCK_DOOR_EW_BOTTOM) ? 97 : 81;
		TextureLoc backTex = (block == BLOCK_DOOR_EW_BOTTOM) ? 130 : 114;
		Block_Tex(block, FACE_XMIN) = backTex;  /* Back face */
		Block_Tex(block, FACE_XMAX) = frontTex; /* Front face */
		Block_Tex(block, FACE_ZMIN) = 4; /* Oak planks on thin edges */
		Block_Tex(block, FACE_ZMAX) = 4;
		Block_Tex(block, FACE_YMAX) = 4; /* Oak planks on top/bottom */
		Block_Tex(block, FACE_YMIN) = 4;
	} else if (block == BLOCK_IRON_DOOR || block == BLOCK_IRON_DOOR_EW_OPEN_BOTTOM) {
		/* Iron door NS geometry bottom: front=ZMAX, back=ZMIN */
		Block_Tex(block, FACE_ZMAX) = 98;   /* bottom front */
		Block_Tex(block, FACE_ZMIN) = 129;  /* bottom back */
		Block_Tex(block, FACE_XMIN) = 55; Block_Tex(block, FACE_XMAX) = 55;
		Block_Tex(block, FACE_YMAX) = 55; Block_Tex(block, FACE_YMIN) = 55;
	} else if (block == BLOCK_IRON_DOOR_NS_TOP || block == BLOCK_IRON_DOOR_EW_OPEN_TOP) {
		/* Iron door NS geometry top */
		Block_Tex(block, FACE_ZMAX) = 82;   /* top front */
		Block_Tex(block, FACE_ZMIN) = 113;  /* top back */
		Block_Tex(block, FACE_XMIN) = 55; Block_Tex(block, FACE_XMAX) = 55;
		Block_Tex(block, FACE_YMAX) = 55; Block_Tex(block, FACE_YMIN) = 55;
	} else if (block == BLOCK_IRON_DOOR_EW_BOTTOM || block == BLOCK_IRON_DOOR_NS_OPEN_BOTTOM) {
		/* Iron door EW geometry bottom: front=XMAX, back=XMIN */
		Block_Tex(block, FACE_XMAX) = 98;   /* bottom front */
		Block_Tex(block, FACE_XMIN) = 129;  /* bottom back */
		Block_Tex(block, FACE_ZMIN) = 55; Block_Tex(block, FACE_ZMAX) = 55;
		Block_Tex(block, FACE_YMAX) = 55; Block_Tex(block, FACE_YMIN) = 55;
	} else if (block == BLOCK_IRON_DOOR_EW_TOP || block == BLOCK_IRON_DOOR_NS_OPEN_TOP) {
		/* Iron door EW geometry top: front=XMAX, back=XMIN */
		Block_Tex(block, FACE_XMAX) = 82;   /* top front */
		Block_Tex(block, FACE_XMIN) = 113;  /* top back */
		Block_Tex(block, FACE_ZMIN) = 55; Block_Tex(block, FACE_ZMAX) = 55;
		Block_Tex(block, FACE_YMAX) = 55; Block_Tex(block, FACE_YMIN) = 55;
	} else if (block == BLOCK_DCHEST_S_L) {
		/* Front +Z, left half (+X block) */
		Block_Tex(block, FACE_ZMAX) = 43; Block_Tex(block, FACE_ZMIN) = 58;
		Block_Tex(block, FACE_XMIN) = 27; Block_Tex(block, FACE_XMAX) = 27;
		Block_Tex(block, FACE_YMAX) = 26; Block_Tex(block, FACE_YMIN) = 26;
	} else if (block == BLOCK_DCHEST_S_R) {
		Block_Tex(block, FACE_ZMAX) = 42; Block_Tex(block, FACE_ZMIN) = 59;
		Block_Tex(block, FACE_XMIN) = 27; Block_Tex(block, FACE_XMAX) = 27;
		Block_Tex(block, FACE_YMAX) = 26; Block_Tex(block, FACE_YMIN) = 26;
	} else if (block == BLOCK_DCHEST_N_L) {
		/* Front -Z, left half (-X block) */
		Block_Tex(block, FACE_ZMIN) = 43; Block_Tex(block, FACE_ZMAX) = 58;
		Block_Tex(block, FACE_XMIN) = 27; Block_Tex(block, FACE_XMAX) = 27;
		Block_Tex(block, FACE_YMAX) = 26; Block_Tex(block, FACE_YMIN) = 26;
	} else if (block == BLOCK_DCHEST_N_R) {
		Block_Tex(block, FACE_ZMIN) = 42; Block_Tex(block, FACE_ZMAX) = 59;
		Block_Tex(block, FACE_XMIN) = 27; Block_Tex(block, FACE_XMAX) = 27;
		Block_Tex(block, FACE_YMAX) = 26; Block_Tex(block, FACE_YMIN) = 26;
	} else if (block == BLOCK_DCHEST_E_L) {
		/* Front +X, left half (-Z block) */
		Block_Tex(block, FACE_XMAX) = 43; Block_Tex(block, FACE_XMIN) = 58;
		Block_Tex(block, FACE_ZMIN) = 27; Block_Tex(block, FACE_ZMAX) = 27;
		Block_Tex(block, FACE_YMAX) = 26; Block_Tex(block, FACE_YMIN) = 26;
	} else if (block == BLOCK_DCHEST_E_R) {
		Block_Tex(block, FACE_XMAX) = 42; Block_Tex(block, FACE_XMIN) = 59;
		Block_Tex(block, FACE_ZMIN) = 27; Block_Tex(block, FACE_ZMAX) = 27;
		Block_Tex(block, FACE_YMAX) = 26; Block_Tex(block, FACE_YMIN) = 26;
	} else if (block == BLOCK_DCHEST_W_L) {
		/* Front -X, left half (+Z block) */
		Block_Tex(block, FACE_XMIN) = 43; Block_Tex(block, FACE_XMAX) = 58;
		Block_Tex(block, FACE_ZMIN) = 27; Block_Tex(block, FACE_ZMAX) = 27;
		Block_Tex(block, FACE_YMAX) = 26; Block_Tex(block, FACE_YMIN) = 26;
	} else if (block == BLOCK_DCHEST_W_R) {
		Block_Tex(block, FACE_XMIN) = 42; Block_Tex(block, FACE_XMAX) = 59;
		Block_Tex(block, FACE_ZMIN) = 27; Block_Tex(block, FACE_ZMAX) = 27;
		Block_Tex(block, FACE_YMAX) = 26; Block_Tex(block, FACE_YMIN) = 26;
	} else if (block == BLOCK_LADDER) {
		/* Ladder: full collision box for proper climbing, render bounds set dynamically */
		Vec3_Set(Blocks.MinBB[block], 0, 0, 0);
		Vec3_Set(Blocks.MaxBB[block], 1, 1, 1);
		/* Default render bounds (will be overridden dynamically based on facing) */
		Vec3_Set(Blocks.RenderMinBB[block], 0, 0, 15.0f/16.0f);
		Vec3_Set(Blocks.RenderMaxBB[block], 1, 1, 1);
	} else if (block == BLOCK_TORCH) {
		/* Torch: small collision box for ground torch (default), overridden dynamically */
		Vec3_Set(Blocks.MinBB[block], 7.0f/16.0f, 0, 7.0f/16.0f);
		Vec3_Set(Blocks.MaxBB[block], 9.0f/16.0f, 10.0f/16.0f, 9.0f/16.0f);
		Vec3_Set(Blocks.RenderMinBB[block], 7.0f/16.0f, 0, 7.0f/16.0f);
		Vec3_Set(Blocks.RenderMaxBB[block], 9.0f/16.0f, 10.0f/16.0f, 9.0f/16.0f);
	} else if (block == BLOCK_RED_ORE_TORCH) {
		/* Red Ore Torch: 11/16 tall */
		Vec3_Set(Blocks.MinBB[block], 7.0f/16.0f, 0, 7.0f/16.0f);
		Vec3_Set(Blocks.MaxBB[block], 9.0f/16.0f, 11.0f/16.0f, 9.0f/16.0f);
		Vec3_Set(Blocks.RenderMinBB[block], 7.0f/16.0f, 0, 7.0f/16.0f);
		Vec3_Set(Blocks.RenderMaxBB[block], 9.0f/16.0f, 11.0f/16.0f, 9.0f/16.0f);
	} else if (block == BLOCK_RED_ORE_TORCH_OFF) {
		/* Unlit Red Ore Torch: 11/16 tall, same geometry as lit version */
		Vec3_Set(Blocks.MinBB[block], 7.0f/16.0f, 0, 7.0f/16.0f);
		Vec3_Set(Blocks.MaxBB[block], 9.0f/16.0f, 11.0f/16.0f, 9.0f/16.0f);
		Vec3_Set(Blocks.RenderMinBB[block], 7.0f/16.0f, 0, 7.0f/16.0f);
		Vec3_Set(Blocks.RenderMaxBB[block], 9.0f/16.0f, 11.0f/16.0f, 9.0f/16.0f);
	} else if (block == BLOCK_RED_TORCH_ON_S || block == BLOCK_RED_TORCH_ON_N || block == BLOCK_RED_TORCH_ON_E || block == BLOCK_RED_TORCH_ON_W ||
	           block == BLOCK_RED_TORCH_OFF_S || block == BLOCK_RED_TORCH_OFF_N || block == BLOCK_RED_TORCH_OFF_E || block == BLOCK_RED_TORCH_OFF_W) {
		/* Wall-mounted redstone torch variants: same base geometry as ground variant */
		Vec3_Set(Blocks.MinBB[block], 7.0f/16.0f, 0, 7.0f/16.0f);
		Vec3_Set(Blocks.MaxBB[block], 9.0f/16.0f, 11.0f/16.0f, 9.0f/16.0f);
		Vec3_Set(Blocks.RenderMinBB[block], 7.0f/16.0f, 0, 7.0f/16.0f);
		Vec3_Set(Blocks.RenderMaxBB[block], 9.0f/16.0f, 11.0f/16.0f, 9.0f/16.0f);
	} else if (block == BLOCK_RED_TORCH_UNMOUNTED || block == BLOCK_RED_TORCH_UNMOUNTED_OFF) {
		/* Unmounted (free-standing) redstone torch: same geometry as ground torch */
		Vec3_Set(Blocks.MinBB[block], 7.0f/16.0f, 0, 7.0f/16.0f);
		Vec3_Set(Blocks.MaxBB[block], 9.0f/16.0f, 11.0f/16.0f, 9.0f/16.0f);
		Vec3_Set(Blocks.RenderMinBB[block], 7.0f/16.0f, 0, 7.0f/16.0f);
		Vec3_Set(Blocks.RenderMaxBB[block], 9.0f/16.0f, 11.0f/16.0f, 9.0f/16.0f);
	} else if (IsButton(block)) {
		/* Button: default bounds for south wall (facing 0), dynamically rotated by DirectionalBlock_GetRenderBounds */
		Vec3_Set(Blocks.MinBB[block], 5.0f/16.0f, 6.0f/16.0f, 14.0f/16.0f);
		Vec3_Set(Blocks.MaxBB[block], 11.0f/16.0f, 10.0f/16.0f, 1);
		Vec3_Set(Blocks.RenderMinBB[block], 5.0f/16.0f, 6.0f/16.0f, 14.0f/16.0f);
		Vec3_Set(Blocks.RenderMaxBB[block], 11.0f/16.0f, 10.0f/16.0f, 1);
	} else if (IsLever(block)) {
		/* Lever base: default bounds for south wall (facing 0), 6 wide, 8 tall, 3 deep */
		Vec3_Set(Blocks.MinBB[block], 5.0f/16.0f, 4.0f/16.0f, 13.0f/16.0f);
		Vec3_Set(Blocks.MaxBB[block], 11.0f/16.0f, 12.0f/16.0f, 1);
		Vec3_Set(Blocks.RenderMinBB[block], 5.0f/16.0f, 4.0f/16.0f, 13.0f/16.0f);
		Vec3_Set(Blocks.RenderMaxBB[block], 11.0f/16.0f, 12.0f/16.0f, 1);
	} else if (block == BLOCK_PRESSURE_PLATE) {
		/* Unpressed pressure plate: full width, 2 pixels tall */
		Vec3_Set(Blocks.MinBB[block], 1.0f/16.0f, 0, 1.0f/16.0f);
		Vec3_Set(Blocks.MaxBB[block], 15.0f/16.0f, 2.0f/16.0f, 15.0f/16.0f);
		Vec3_Set(Blocks.RenderMinBB[block], 1.0f/16.0f, 0, 1.0f/16.0f);
		Vec3_Set(Blocks.RenderMaxBB[block], 15.0f/16.0f, 2.0f/16.0f, 15.0f/16.0f);
	} else if (block == BLOCK_PRESSURE_PLATE_PRESSED) {
		/* Pressed pressure plate: full width, 1 pixel tall (pushed down) */
		Vec3_Set(Blocks.MinBB[block], 1.0f/16.0f, 0, 1.0f/16.0f);
		Vec3_Set(Blocks.MaxBB[block], 15.0f/16.0f, 1.0f/16.0f, 15.0f/16.0f);
		Vec3_Set(Blocks.RenderMinBB[block], 1.0f/16.0f, 0, 1.0f/16.0f);
		Vec3_Set(Blocks.RenderMaxBB[block], 15.0f/16.0f, 1.0f/16.0f, 15.0f/16.0f);
	}

	Blocks.ParticleGravity[block] = 5.4f * (def->gravity / 100.0f);
}

STRING_REF cc_string Block_UNSAFE_GetName(BlockID block) {
	return String_FromRaw(Block_NamePtr(block), STRING_SIZE);
}

void Block_SetName(BlockID block, const cc_string* name) {
	String_CopyToRaw(Block_NamePtr(block), STRING_SIZE, name);
}

int Block_FindID(const cc_string* name) {
	cc_string blockName;
	int block;

	for (block = BLOCK_AIR; block < BLOCK_COUNT; block++) 
	{
		blockName = Block_UNSAFE_GetName(block);
		if (String_CaselessEquals(&blockName, name)) return block;
	}
	return -1;
}

int Block_Parse(const cc_string* name) {
	int b;
	if (Convert_ParseInt(name, &b) && b < BLOCK_COUNT) return b;
	return Block_FindID(name);
}

/* 0b_1000_0000 */
#define USE_MODERN_BRIGHTNESS_FLAG 1 << 7
/* 0b_0100_0000 */
#define USE_LAMP_COLOR 1 << 6
/* 0b_0000_1111 */
#define BRIGHTNESS_MASK FANCY_LIGHTING_MAX_LEVEL

/* Reads network format    0b_US--_LLLL where U = uses fancy brightness, S = uses lamp brightness, and L = brightness */
/* Into CC's native format 0b_SSSS_BBBB where S = lamp brightness and B = lava brightness */
cc_uint8 Block_ReadBrightness(cc_uint8 fullBright) {
	cc_bool useSun;
	/* If the fullBright byte does not use the flag, we should interpret it as either completely dark or casting max block light */
	if ((fullBright & USE_MODERN_BRIGHTNESS_FLAG) == 0) { return fullBright > 0 ? FANCY_LIGHTING_MAX_LEVEL : 0; }

	useSun = fullBright & USE_LAMP_COLOR;

	/* Preserve only the least significant four bits. This gives us our raw brightness level for sun or block light. */
	fullBright &= BRIGHTNESS_MASK;

	/* Sun light is stored in the upper four bits */
	if (useSun) { fullBright <<= FANCY_LIGHTING_LAMP_SHIFT; }
	return fullBright;
}

/* Writes CC's native format 0b_SSSS_BBBB where S = lamp brightness and B = lava brightness */
/* into network format       0b_US--_LLLL where U = uses fancy brightness, S = uses lamp brightness, and L = brightness */
cc_uint8 Block_WriteFullBright(cc_uint8 brightness) {
	cc_uint8 lavaBrightness, lampBrightness, fullBright;
	lavaBrightness = brightness & BRIGHTNESS_MASK;
	lampBrightness = brightness >> FANCY_LIGHTING_LAMP_SHIFT;
	fullBright     = USE_MODERN_BRIGHTNESS_FLAG;

	/* Modern brightness stored in a fullbright value is mutually exclusive between using block and using sun light */
	if (lavaBrightness > 0) {
		fullBright |= lavaBrightness;
	} else if (lampBrightness > 0) {
		fullBright |= USE_LAMP_COLOR; /* Insert flag that tells us this fullbright value should be interpreted as sun brightness */
		fullBright |= lampBrightness;
	} else {
		return 0;
	}
	return fullBright;
}

/*########################################################################################################################*
*-------------------------------------------------------AutoRotate--------------------------------------------------------*
*#########################################################################################################################*/
cc_bool AutoRotate_Enabled;

#define AR_GROUP_CORNERS 0
#define AR_GROUP_VERTICAL 1
#define AR_GROUP_DIRECTION 2
#define AR_GROUP_PILLAR  3

#define AR_EQ1(x)    (dir0 == x && dir1 == '\0')
#define AR_EQ2(x, y) (dir0 == x && dir1 == y)
static int AR_CalcGroup(const cc_string* dir) {
	char dir0, dir1;
	dir0 = dir->length > 1 ? dir->buffer[1] : '\0'; Char_MakeLower(dir0);
	dir1 = dir->length > 2 ? dir->buffer[2] : '\0'; Char_MakeLower(dir1);

	if (AR_EQ2('n','w') || AR_EQ2('n','e') || AR_EQ2('s','w') || AR_EQ2('s','e')) {
		return AR_GROUP_CORNERS;
	} else if (AR_EQ1('u') || AR_EQ1('d')) {
		return AR_GROUP_VERTICAL;
	} else if (AR_EQ1('n') || AR_EQ1('w') || AR_EQ1('s') || AR_EQ1('e')) {
		return AR_GROUP_DIRECTION;
	} else if (AR_EQ2('u','d') || AR_EQ2('w','e') || AR_EQ2('n','s')) {
		return AR_GROUP_PILLAR;
	}
	return -1;
}

/* replaces a portion of a string, appends otherwise */
static void AutoRotate_Insert(cc_string* str, int offset, const char* suffix) {
	int i = str->length - offset;

	for (; *suffix; suffix++, i++) {
		if (i < str->length) {
			str->buffer[i] = *suffix;
		} else {
			String_Append(str, *suffix);
		}
	}
}
/* finds proper rotated form of a block, based on the given name */
static int FindRotated(cc_string* name, int offset);

static int GetRotated(cc_string* name, int offset) {
	int rotated = FindRotated(name, offset);
	return rotated == -1 ? Block_FindID(name) : rotated;
}

static int RotateCorner(cc_string* name, int offset) {
	float x = Game_SelectedPos.intersect.x - (float)Game_SelectedPos.translatedPos.x;
	float z = Game_SelectedPos.intersect.z - (float)Game_SelectedPos.translatedPos.z;

	if (x < 0.5f && z < 0.5f) {
		AutoRotate_Insert(name, offset, "-NW");
	} else if (x >= 0.5f && z < 0.5f) {
		AutoRotate_Insert(name, offset, "-NE");
	} else if (x < 0.5f && z >= 0.5f) {
		AutoRotate_Insert(name, offset, "-SW");
	} else if (x >= 0.5f && z >= 0.5f) {
		AutoRotate_Insert(name, offset, "-SE");
	}
	return GetRotated(name, offset);
}

static int RotateVertical(cc_string* name, int offset) {
	float y = Game_SelectedPos.intersect.y - (float)Game_SelectedPos.translatedPos.y;

	if (y >= 0.5f) {
		AutoRotate_Insert(name, offset, "-U");
	} else {
		AutoRotate_Insert(name, offset, "-D");
	}
	return GetRotated(name, offset);
}

static int RotateFence(cc_string* name, int offset) {
	/* Fence type blocks */
	float yaw = Math_ClampAngle(Entities.CurPlayer->Base.Yaw);

	if (yaw < 45.0f || (yaw >= 135.0f && yaw < 225.0f) || yaw > 315.0f) {
		AutoRotate_Insert(name, offset, "-WE");
	} else {
		AutoRotate_Insert(name, offset, "-NS");
	}
	return GetRotated(name, offset);
}

static int RotatePillar(cc_string* name, int offset) {
	/* Thin pillar type blocks */
	Face face = Game_SelectedPos.closest;

	if (face == FACE_YMAX || face == FACE_YMIN) {
		AutoRotate_Insert(name, offset, "-UD");
	} else if (face == FACE_XMAX || face == FACE_XMIN) {
		AutoRotate_Insert(name, offset, "-WE");
	} else if (face == FACE_ZMAX || face == FACE_ZMIN) {
		AutoRotate_Insert(name, offset, "-NS");
	}
	return GetRotated(name, offset);
}

static int RotateDirection(cc_string* name, int offset) {
	float yaw = Math_ClampAngle(Entities.CurPlayer->Base.Yaw);

	if (yaw >= 45.0f && yaw < 135.0f) {
		AutoRotate_Insert(name, offset, "-E");
	} else if (yaw >= 135.0f && yaw < 225.0f) {
		AutoRotate_Insert(name, offset, "-S");
	} else if (yaw >= 225.0f && yaw < 315.0f) {
		AutoRotate_Insert(name, offset, "-W");
	} else {
		AutoRotate_Insert(name, offset, "-N");
	}
	return GetRotated(name, offset);
}

static int FindRotated(cc_string* name, int offset) {
	cc_string dir;
	int group;
	int dirIndex = String_LastIndexOfAt(name, offset, '-');
	if (dirIndex == -1) return -1; /* not a directional block */

	dir = String_UNSAFE_SubstringAt(name, dirIndex);
	dir.length -= offset;
	if (dir.length > 3) return -1;

	offset += dir.length;
	group = AR_CalcGroup(&dir);

	if (group == AR_GROUP_CORNERS) return RotateCorner(name, offset);
	if (group == AR_GROUP_VERTICAL) return RotateVertical(name, offset);
	if (group == AR_GROUP_DIRECTION) return RotateDirection(name, offset);

	if (group == AR_GROUP_PILLAR) {
		AutoRotate_Insert(name, offset, "-UD");
		if (Block_FindID(name) == -1) {
			return RotateFence(name, offset);
		} else {
			return RotatePillar(name, offset);
		}
	}
	return -1;
}

BlockID AutoRotate_RotateBlock(BlockID block) {
	cc_string str; char strBuffer[STRING_SIZE * 2];
	cc_string name;
	int rotated;
	
	name = Block_UNSAFE_GetName(block);
	String_InitArray(str, strBuffer);
	String_AppendString(&str, &name);

	/* need to copy since we change characters in name */
	rotated = FindRotated(&str, 0);
	return rotated == -1 ? block : (BlockID)rotated;
}

static void GetAutoRotateTypes(cc_string* name, int* dirTypes) {
	int dirIndex, i;
	cc_string dir;
	dirTypes[0] = -1;
	dirTypes[1] = -1;

	for (i = 0; i < 2; i++) {
		/* index of rightmost group separated by dashes */
		dirIndex = String_LastIndexOf(name, '-');
		if (dirIndex == -1) return;

		dir = String_UNSAFE_SubstringAt(name, dirIndex);
		dirTypes[i]  = AR_CalcGroup(&dir);
		name->length = dirIndex;
	}
}

cc_bool AutoRotate_BlocksShareGroup(BlockID block, BlockID other) {
	cc_string bName, oName;
	int bDirTypes[2], oDirTypes[2];

	bName = Block_UNSAFE_GetName(block);
	GetAutoRotateTypes(&bName, bDirTypes);
	if (bDirTypes[0] == -1) return false;

	oName = Block_UNSAFE_GetName(other);
	GetAutoRotateTypes(&oName, oDirTypes);
	if (oDirTypes[0] == -1) return false;

	return bDirTypes[0] == oDirTypes[0] && bDirTypes[1] == oDirTypes[1] 
		&& String_CaselessEquals(&bName, &oName);
}


/*########################################################################################################################*
*----------------------------------------------------Blocks component-----------------------------------------------------*
*#########################################################################################################################*/
static void OnBlockChanged(void* obj, IVec3 coords, BlockID old, BlockID now) {
	int x = coords.x, y = coords.y, z = coords.z;
	
	/* Update the block that was changed */
	DirectionalBlock_Update(x, y, z);
	
	/* Also update neighboring directional blocks (e.g., torches that may need to re-orient) */
	if (World_Contains(x - 1, y, z)) DirectionalBlock_Update(x - 1, y, z);
	if (World_Contains(x + 1, y, z)) DirectionalBlock_Update(x + 1, y, z);
	if (World_Contains(x, y, z - 1)) DirectionalBlock_Update(x, y, z - 1);
	if (World_Contains(x, y, z + 1)) DirectionalBlock_Update(x, y, z + 1);
	if (y > 0 && World_Contains(x, y - 1, z)) DirectionalBlock_Update(x, y - 1, z);
	if (World_Contains(x, y + 1, z)) DirectionalBlock_Update(x, y + 1, z);
}

static void OnNewMap(void* obj) {
	DirectionalCache_Rebuild();
}

static void OnNewMapLoaded(void* obj) {
	DirectionalCache_Rebuild();
}

static void OnReset(void) {
	int i, block;
	for (i = 0; i < Array_Elems(definedCustomBlocks); i++) {
		definedCustomBlocks[i] = 0;
	}

	for (block = BLOCK_AIR; block < BLOCK_COUNT; block++) {
		Block_ResetProps((BlockID)block);
	}

	Block_UpdateAllCulling();
	Block_RecalculateAllSpriteBB();

	for (block = BLOCK_AIR; block < BLOCK_COUNT; block++) {
		Blocks.CanPlace[block]  = true;
		Blocks.CanDelete[block] = true;
	}
	
	/* Remove door tops and other non-placeable blocks from inventory */
	Inventory_Remove(BLOCK_DOOR_NS_TOP);
	Inventory_Remove(BLOCK_DOOR_EW_TOP);
	Inventory_Remove(BLOCK_DOOR_EW_BOTTOM);
	Inventory_Remove(BLOCK_LIT_RED_ORE_DUST);
	Inventory_Remove(BLOCK_RED_ORE_TORCH_OFF);
	/* Remove all wall torch variants (player places generic torch, code picks variant) */
	Inventory_Remove(BLOCK_RED_TORCH_ON_S);
	Inventory_Remove(BLOCK_RED_TORCH_ON_N);
	Inventory_Remove(BLOCK_RED_TORCH_ON_E);
	Inventory_Remove(BLOCK_RED_TORCH_ON_W);
	Inventory_Remove(BLOCK_RED_TORCH_OFF_S);
	Inventory_Remove(BLOCK_RED_TORCH_OFF_N);
	Inventory_Remove(BLOCK_RED_TORCH_OFF_E);
	Inventory_Remove(BLOCK_RED_TORCH_OFF_W);
	Inventory_Remove(BLOCK_RED_TORCH_UNMOUNTED);
	Inventory_Remove(BLOCK_RED_TORCH_UNMOUNTED_OFF);
	
	/* Remove pressed button from inventory (non-placeable, auto-placed by physics) */
	Inventory_Remove(BLOCK_BUTTON_PRESSED);
	
	/* Remove lever ON from inventory (toggled by right-click, not placeable) */
	Inventory_Remove(BLOCK_LEVER_ON);
	
	/* Remove pressed pressure plate from inventory (auto-placed by physics) */
	Inventory_Remove(BLOCK_PRESSURE_PLATE_PRESSED);
	
	/* Remove iron door variants from inventory (auto-placed by physics/redstone) */
	Inventory_Remove(BLOCK_IRON_DOOR_NS_TOP);
	Inventory_Remove(BLOCK_IRON_DOOR_EW_BOTTOM);
	Inventory_Remove(BLOCK_IRON_DOOR_EW_TOP);
	Inventory_Remove(BLOCK_IRON_DOOR_NS_OPEN_BOTTOM);
	Inventory_Remove(BLOCK_IRON_DOOR_NS_OPEN_TOP);
	Inventory_Remove(BLOCK_IRON_DOOR_EW_OPEN_BOTTOM);
	Inventory_Remove(BLOCK_IRON_DOOR_EW_OPEN_TOP);
	
	/* Remove double chest variants from inventory (auto-placed) */
	Inventory_Remove(BLOCK_DCHEST_S_L);
	Inventory_Remove(BLOCK_DCHEST_S_R);
	Inventory_Remove(BLOCK_DCHEST_N_L);
	Inventory_Remove(BLOCK_DCHEST_N_R);
	Inventory_Remove(BLOCK_DCHEST_E_L);
	Inventory_Remove(BLOCK_DCHEST_E_R);
	Inventory_Remove(BLOCK_DCHEST_W_L);
	Inventory_Remove(BLOCK_DCHEST_W_R);

	/* Remove shadow ceiling from inventory (auto-placed during hell theme generation) */
	Inventory_Remove(BLOCK_SHADOW_CEILING);

	/* Remove snowy grass from inventory (auto-placed when snow is on top of grass) */
	Inventory_Remove(BLOCK_SNOWY_GRASS);

	DirectionalCache_Clear();
}

static void OnAtlasChanged(void* obj) { Block_RecalculateAllSpriteBB(); }
static void OnInit(void) {
	AutoRotate_Enabled = true;
	Event_Register_(&TextureEvents.AtlasChanged, NULL, OnAtlasChanged);
	Event_Register_(&UserEvents.BlockChanged, NULL, OnBlockChanged);
	Event_Register_(&WorldEvents.NewMap, NULL, OnNewMap);
	Event_Register_(&WorldEvents.MapLoaded, NULL, OnNewMapLoaded);
	OnReset();
}

struct IGameComponent Blocks_Component = {
	OnInit,  /* Init  */
	NULL,    /* Free  */
	OnReset, /* Reset */
};
