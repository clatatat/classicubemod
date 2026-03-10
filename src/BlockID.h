#ifndef CC_BLOCKID_H
#define CC_BLOCKID_H
#include "Core.h" /* TODO: Remove this include when we move to external defines */
/* List of all core/standard block IDs
   Copyright 2014-2025 ClassiCube | Licensed under BSD-3
*/
CC_BEGIN_HEADER

enum BLOCKID {
	/* Classic blocks */
	BLOCK_AIR = 0,
	BLOCK_STONE = 1,
	BLOCK_GRASS = 2,
	BLOCK_DIRT = 3,
	BLOCK_COBBLE = 4,
	BLOCK_WOOD = 5,
	BLOCK_SAPLING = 6,
	BLOCK_BEDROCK = 7,
	BLOCK_WATER = 8,
	BLOCK_STILL_WATER = 9,
	BLOCK_LAVA = 10,
	BLOCK_STILL_LAVA = 11,
	BLOCK_SAND = 12,
	BLOCK_GRAVEL = 13,
	BLOCK_GOLD_ORE = 14,
	BLOCK_IRON_ORE = 15,
	BLOCK_COAL_ORE = 16,
	BLOCK_LOG = 17,
	BLOCK_LEAVES = 18,
	BLOCK_SPONGE = 19,
	BLOCK_GLASS = 20,
	BLOCK_RED = 21,
	BLOCK_ORANGE = 22,
	BLOCK_YELLOW = 23,
	BLOCK_LIME = 24,
	BLOCK_GREEN = 25,
	BLOCK_TEAL = 26,
	BLOCK_AQUA = 27,
	BLOCK_CYAN = 28,
	BLOCK_BLUE = 29,
	BLOCK_INDIGO = 30,
	BLOCK_VIOLET = 31,
	BLOCK_MAGENTA = 32,
	BLOCK_PINK = 33,
	BLOCK_BLACK = 34,
	BLOCK_GRAY = 35,
	BLOCK_WHITE = 36,
	BLOCK_DANDELION = 37,
	BLOCK_ROSE = 38,
	BLOCK_BROWN_SHROOM = 39,
	BLOCK_RED_SHROOM = 40,
	BLOCK_GOLD = 41,
	BLOCK_IRON = 42,
	BLOCK_DOUBLE_SLAB = 43,
	BLOCK_SLAB = 44,
	BLOCK_BRICK = 45,
	BLOCK_TNT = 46,
	BLOCK_BOOKSHELF = 47,
	BLOCK_MOSSY_ROCKS = 48,
	BLOCK_OBSIDIAN = 49,

	/* CPE blocks */
	BLOCK_DIAMOND_BLOCK = 50,
	BLOCK_DIAMOND_ORE = 51,
	BLOCK_RED_ORE = 52,
	BLOCK_CACTUS = 53,
	BLOCK_CRAFT = 54,
	BLOCK_FURNACE = 55,
	BLOCK_CHEST = 56,
	BLOCK_COBWEB = 57,
	BLOCK_LADDER = 58,
	BLOCK_DOOR_NS_BOTTOM = 59, /* Door facing North/South - bottom half */
	BLOCK_RED_ORE_DUST = 60, /* Redstone dust */
	BLOCK_TORCH = 61, /* Torch - ground or wall mounted */
	BLOCK_RED_ORE_TORCH = 62, /* Red Ore Torch - wall/ground mounted, self-lit */
	BLOCK_BUTTON = 63, /* Unpressed button (direction from cache) */
	BLOCK_LEVER = 64, /* Lever off (direction from cache) */
	BLOCK_PRESSURE_PLATE = 65, /* Pressure plate (unpressed) */
	BLOCK_IRON_DOOR = 66, /* Iron Door (closed NS bottom) - opened by redstone only */
	BLOCK_SNOW = 67, /* Snow layer - 2 pixels tall */
	BLOCK_ICE = 68, /* Ice block */
	BLOCK_SNOW_BLOCK = 69, /* Full snow block */
	BLOCK_STONE_PLATE = 70, /* Stone pressure plate (mob/player only, ignores items) */
	BLOCK_FIRE = 71, /* Fire block - animated sprite, spreads to flammable blocks */

	/* Survival blocks (player-placeable, above CPE range) */
	BLOCK_SIGN_WALL = 72, /* Wall-mounted wooden sign (orientation from directional cache) */
	BLOCK_SIGN_FLOOR = 73, /* Floor-mounted sign post (16-angle rotation stored in SignData) */
	BLOCK_RAIL = 74, /* Minecart rail - connects to adjacent rails, curves at corners */
	BLOCK_WOOD_STAIRS = 75,   /* Wooden plank stairs (inventory/craftable base block) */
	BLOCK_COBBLE_STAIRS = 76, /* Cobblestone stairs (inventory/craftable base block) */

	/* Max block ID used in original classic */
	BLOCK_MAX_ORIGINAL = BLOCK_OBSIDIAN,
	/* Max block ID used in original classic plus CPE blocks. */
	BLOCK_MAX_CPE = BLOCK_FIRE,
	
	/* Non-placeable blocks (auto-placed, not in inventory) */
	BLOCK_DOOR_NS_TOP = 200, /* Door facing North/South - top half */
	BLOCK_DOOR_EW_BOTTOM = 201, /* Door facing East/West - bottom half */
	BLOCK_DOOR_EW_TOP = 202, /* Door facing East/West - top half */
	BLOCK_LIT_RED_ORE_DUST = 203, /* Powered redstone dust (lit textures = unlit + 16) */
	BLOCK_RED_ORE_TORCH_OFF = 204, /* Unpowered redstone torch - ground (texture 115) */
	
	/* Wall-mounted redstone torch variants - named by direction to attach block */
	BLOCK_RED_TORCH_ON_S  = 205, /* ON, attached to block at z+1 (south) */
	BLOCK_RED_TORCH_ON_N  = 206, /* ON, attached to block at z-1 (north) */
	BLOCK_RED_TORCH_ON_E  = 207, /* ON, attached to block at x+1 (east) */
	BLOCK_RED_TORCH_ON_W  = 208, /* ON, attached to block at x-1 (west) */
	BLOCK_RED_TORCH_OFF_S = 209, /* OFF, attached to block at z+1 (south) */
	BLOCK_RED_TORCH_OFF_N = 210, /* OFF, attached to block at z-1 (north) */
	BLOCK_RED_TORCH_OFF_E = 211, /* OFF, attached to block at x+1 (east) */
	BLOCK_RED_TORCH_OFF_W = 212, /* OFF, attached to block at x-1 (west) */
	
	/* Unmounted (free-standing) redstone torch - stands upright, not attached to any block */
	BLOCK_RED_TORCH_UNMOUNTED     = 213, /* ON, free-standing */
	BLOCK_RED_TORCH_UNMOUNTED_OFF = 214, /* OFF, free-standing */
	BLOCK_BUTTON_PRESSED = 215, /* Pressed button (direction from cache) */
	BLOCK_LEVER_ON = 216, /* Lever on (direction from cache) */
	BLOCK_PRESSURE_PLATE_PRESSED = 217, /* Pressed pressure plate */
	
	/* Iron door variants (not player-placeable, auto-managed) */
	BLOCK_IRON_DOOR_NS_TOP = 218, /* Iron Door NS top (closed) */
	BLOCK_IRON_DOOR_EW_BOTTOM = 219, /* Iron Door EW bottom (closed) */
	BLOCK_IRON_DOOR_EW_TOP = 220, /* Iron Door EW top (closed) */
	BLOCK_IRON_DOOR_NS_OPEN_BOTTOM = 221, /* Iron Door NS open bottom (EW geometry) */
	BLOCK_IRON_DOOR_NS_OPEN_TOP = 222, /* Iron Door NS open top (EW geometry) */
	BLOCK_IRON_DOOR_EW_OPEN_BOTTOM = 223, /* Iron Door EW open bottom (NS geometry) */
	BLOCK_IRON_DOOR_EW_OPEN_TOP = 224, /* Iron Door EW open top (NS geometry) */
	
	/* Double chest variants (auto-placed when two chests are adjacent) */
	/* Naming: facing direction + which half (L=left, R=right from viewer looking at front) */
	BLOCK_DCHEST_S_L = 225, /* Front +Z, left half (+X block) */
	BLOCK_DCHEST_S_R = 226, /* Front +Z, right half (-X block) */
	BLOCK_DCHEST_N_L = 227, /* Front -Z, left half (-X block) */
	BLOCK_DCHEST_N_R = 228, /* Front -Z, right half (+X block) */
	BLOCK_DCHEST_E_L = 229, /* Front +X, left half (-Z block) */
	BLOCK_DCHEST_E_R = 230, /* Front +X, right half (+Z block) */
	BLOCK_DCHEST_W_L = 231, /* Front -X, left half (+Z block) */
	BLOCK_DCHEST_W_R = 232, /* Front -X, right half (-Z block) */

	BLOCK_SHADOW_CEILING = 233, /* Invisible light-blocking ceiling for hell theme */
	BLOCK_SNOWY_GRASS = 234, /* Grass with snow on top - auto-placed, not in inventory */

	BLOCK_STONE_PLATE_PRESSED = 235, /* Pressed stone pressure plate */

	BLOCK_FARMLAND_DRY = 236, /* Dry farmland - created by hoeing dirt/grass */
	BLOCK_FARMLAND_WET = 237, /* Wet farmland - dry farmland hydrated by nearby water */

	/* Wheat crop growth stages (8 stages, placed by using seeds on farmland) */
	BLOCK_WHEAT_0 = 238, /* Stage 1 - just planted */
	BLOCK_WHEAT_1 = 239,
	BLOCK_WHEAT_2 = 240,
	BLOCK_WHEAT_3 = 241,
	BLOCK_WHEAT_4 = 242,
	BLOCK_WHEAT_5 = 243,
	BLOCK_WHEAT_6 = 244,
	BLOCK_WHEAT_7 = 245, /* Stage 8 - fully grown, drops wheat */
	BLOCK_PORTAL  = 246, /* Portal block - teleports to new Strange world on contact */

	/* Rail orientation variants (non-placeable, auto-placed by rail update logic) */
	/* Alpha metadata: 0=NS, 1=EW, 2=asc_E, 3=asc_W, 4=asc_N, 5=asc_S, 6=SE, 7=SW, 8=NW, 9=NE */
	/* BLOCK_RAIL (74) is meta 0: N-S straight (also the inventory/craftable item) */
	BLOCK_RAIL_EW       = 247, /* E-W straight rail (meta 1, tex 144) */
	BLOCK_RAIL_ASC_E    = 248, /* Ascending East rail (meta 2, tex 128) */
	BLOCK_RAIL_ASC_W    = 249, /* Ascending West rail (meta 3, tex 128) */
	BLOCK_RAIL_ASC_N    = 250, /* Ascending North rail (meta 4, tex 128) */
	BLOCK_RAIL_ASC_S    = 251, /* Ascending South rail (meta 5, tex 128) */
	BLOCK_RAIL_CURVE_SE = 252, /* SE curve rail (meta 6, tex 112) */
	BLOCK_RAIL_CURVE_SW = 253, /* SW curve rail (meta 7, tex 146) */
	BLOCK_RAIL_CURVE_NW = 254, /* NW curve rail (meta 8, tex 147) */
	BLOCK_RAIL_CURVE_NE = 255, /* NE curve rail (meta 9, tex 145) */

#if defined EXTENDED_BLOCKS
	/* Stair directional variants (placed in world, not in inventory) */
	/* Named by MC Alpha facing metadata: 0=step -X, 1=step +X, 2=step -Z, 3=step +Z */
	BLOCK_WOOD_STAIRS_0   = 256, /* Wooden stairs, step on -X half */
	BLOCK_WOOD_STAIRS_1   = 257, /* Wooden stairs, step on +X half */
	BLOCK_WOOD_STAIRS_2   = 258, /* Wooden stairs, step on -Z half */
	BLOCK_WOOD_STAIRS_3   = 259, /* Wooden stairs, step on +Z half */
	BLOCK_COBBLE_STAIRS_0 = 260, /* Cobblestone stairs, step on -X half */
	BLOCK_COBBLE_STAIRS_1 = 261, /* Cobblestone stairs, step on +X half */
	BLOCK_COBBLE_STAIRS_2 = 262, /* Cobblestone stairs, step on -Z half */
	BLOCK_COBBLE_STAIRS_3 = 263, /* Cobblestone stairs, step on +Z half */

	/* Ladder directional variants (placed in world, not in inventory) */
	/* Named by the wall the ladder attaches to (where support block is) */
	BLOCK_LADDER_S = 264, /* Attached to south wall (+Z), 2/16 thick at +Z edge */
	BLOCK_LADDER_N = 265, /* Attached to north wall (-Z), 2/16 thick at -Z edge */
	BLOCK_LADDER_E = 266, /* Attached to east wall (+X), 2/16 thick at +X edge */
	BLOCK_LADDER_W = 267, /* Attached to west wall (-X), 2/16 thick at -X edge */

	/* Wooden door 4-direction variants (MC Alpha 1.2.6 style)
	   Layout per direction: closed_bottom, closed_top, open_bottom, open_top
	   XOR 2 toggles open/closed, XOR 1 toggles bottom/top
	   Dir d closed wall = (d+3)&3, open wall = d
	   Walls: 0=Z-(north), 1=X+(east), 2=Z+(south), 3=X-(west) */
	BLOCK_DOOR_D0_BOTTOM      = 268, /* Dir 0 (facing S) closed, X- wall */
	BLOCK_DOOR_D0_TOP         = 269,
	BLOCK_DOOR_D0_OPEN_BOTTOM = 270, /* Dir 0 open, Z- wall */
	BLOCK_DOOR_D0_OPEN_TOP    = 271,
	BLOCK_DOOR_D1_BOTTOM      = 272, /* Dir 1 (facing W) closed, Z- wall */
	BLOCK_DOOR_D1_TOP         = 273,
	BLOCK_DOOR_D1_OPEN_BOTTOM = 274, /* Dir 1 open, X+ wall */
	BLOCK_DOOR_D1_OPEN_TOP    = 275,
	BLOCK_DOOR_D2_BOTTOM      = 276, /* Dir 2 (facing N) closed, X+ wall */
	BLOCK_DOOR_D2_TOP         = 277,
	BLOCK_DOOR_D2_OPEN_BOTTOM = 278, /* Dir 2 open, Z+ wall */
	BLOCK_DOOR_D2_OPEN_TOP    = 279,
	BLOCK_DOOR_D3_BOTTOM      = 280, /* Dir 3 (facing E) closed, Z+ wall */
	BLOCK_DOOR_D3_TOP         = 281,
	BLOCK_DOOR_D3_OPEN_BOTTOM = 282, /* Dir 3 open, X- wall */
	BLOCK_DOOR_D3_OPEN_TOP    = 283,

	/* Iron door 4-direction variants (same layout as wooden) */
	BLOCK_IRON_DOOR_D0_BOTTOM      = 284, /* Dir 0 closed, X- wall */
	BLOCK_IRON_DOOR_D0_TOP         = 285,
	BLOCK_IRON_DOOR_D0_OPEN_BOTTOM = 286, /* Dir 0 open, Z- wall */
	BLOCK_IRON_DOOR_D0_OPEN_TOP    = 287,
	BLOCK_IRON_DOOR_D1_BOTTOM      = 288, /* Dir 1 closed, Z- wall */
	BLOCK_IRON_DOOR_D1_TOP         = 289,
	BLOCK_IRON_DOOR_D1_OPEN_BOTTOM = 290, /* Dir 1 open, X+ wall */
	BLOCK_IRON_DOOR_D1_OPEN_TOP    = 291,
	BLOCK_IRON_DOOR_D2_BOTTOM      = 292, /* Dir 2 closed, X+ wall */
	BLOCK_IRON_DOOR_D2_TOP         = 293,
	BLOCK_IRON_DOOR_D2_OPEN_BOTTOM = 294, /* Dir 2 open, Z+ wall */
	BLOCK_IRON_DOOR_D2_OPEN_TOP    = 295,
	BLOCK_IRON_DOOR_D3_BOTTOM      = 296, /* Dir 3 closed, Z+ wall */
	BLOCK_IRON_DOOR_D3_TOP         = 297,
	BLOCK_IRON_DOOR_D3_OPEN_BOTTOM = 298, /* Dir 3 open, X- wall */
	BLOCK_IRON_DOOR_D3_OPEN_TOP    = 299,

	/* Wall-mounted regular torch variants - named by direction to attach block */
	BLOCK_TORCH_S = 300, /* Attached to block at z+1 (south) */
	BLOCK_TORCH_N = 301, /* Attached to block at z-1 (north) */
	BLOCK_TORCH_E = 302, /* Attached to block at x+1 (east) */
	BLOCK_TORCH_W = 303, /* Attached to block at x-1 (west) */

	BLOCK_MAX_DEFINED = 0x2FF,
#elif defined CC_BUILD_TINYMEM
	BLOCK_MAX_DEFINED = BLOCK_MAX_CPE,
#else
	BLOCK_MAX_DEFINED = 0xFF,
#endif
	BLOCK_COUNT = (BLOCK_MAX_DEFINED + 1)
};

CC_END_HEADER
#endif
