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

	/* Max block ID used in original classic */
	BLOCK_MAX_ORIGINAL = BLOCK_OBSIDIAN,
	/* Max block ID used in original classic plus CPE blocks. */
	BLOCK_MAX_CPE = BLOCK_IRON_DOOR,
	
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

#if defined EXTENDED_BLOCKS
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
