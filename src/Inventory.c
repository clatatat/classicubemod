#include "Inventory.h"
#include "Funcs.h"
#include "Game.h"
#include "Block.h"
#include "Event.h"
#include "Chat.h"
#include "Protocol.h"
#include "Platform.h"
#include "Stream.h"
#include "Logger.h"
#include "MapRenderer.h"
#include "Entity.h"
#include "World.h"

struct _InventoryData Inventory;

/*########################################################################################################################*
*------------------------------------------------------Item data----------------------------------------------------------*
*#########################################################################################################################*/
int HotbarItems[INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR];
int HotbarCounts[INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR];
int HotbarDurability[INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR];

struct SurvInvSlot SurvInv_Main[27];
struct SurvInvSlot SurvInv_Armor[4];
struct SurvInvSlot SurvInv_Craft[4];
struct SurvInvSlot SurvInv_Output;

const char* const ItemNames[ITEM_COUNT] = {
	"Air",
	"Cloth Helmet", "Cloth Chestplate", "Cloth Pants", "Cloth Boots",
	"Wood Sword", "Wood Shovel", "Wood Pickaxe", "Wood Axe",
	"Chainmail Helmet", "Chainmail Chestplate", "Chainmail Pants", "Chainmail Boots",
	"Stone Sword", "Stone Shovel", "Stone Pickaxe", "Stone Axe",
	"Iron Helmet", "Iron Chestplate", "Iron Pants", "Iron Boots",
	"Iron Sword", "Iron Shovel", "Iron Pickaxe", "Iron Axe",
	"Diamond Helmet", "Diamond Chestplate", "Diamond Pants", "Diamond Boots",
	"Diamond Sword", "Diamond Shovel", "Diamond Pickaxe", "Diamond Axe",
	"Gold Helmet", "Gold Chestplate", "Gold Pants", "Gold Boots",
	"Gold Sword", "Gold Shovel", "Gold Pickaxe", "Gold Axe",
	"Bow", "Arrow", "Stick", "Flint & Steel", "Flint",
	"Coal", "Iron Ingot", "Gold Ingot", "Diamond",
	"Bowl", "Mushroom Stew", "Raw Pork", "Cooked Pork",
	"Sulphur", "Feather", "String",
	"Wood Hoe", "Stone Hoe", "Iron Hoe", "Diamond Hoe", "Gold Hoe",
	"Seeds", "Wheat", "Bread",
	"Bucket", "Water Bucket", "Lava Bucket",
	"Sign"
};

const int ItemTextures[ITEM_COUNT] = {
	-1,                       /* 0:  Air */
	0, 16, 32, 48,            /* 1-4:   Cloth armor */
	64, 80, 96, 112,          /* 5-8:   Wood tools */
	1, 17, 33, 49,            /* 9-12:  Chainmail armor */
	65, 81, 97, 113,          /* 13-16: Stone tools */
	2, 18, 34, 50,            /* 17-20: Iron armor */
	66, 82, 98, 114,          /* 21-24: Iron tools */
	3, 19, 35, 51,            /* 25-28: Diamond armor */
	67, 83, 99, 115,          /* 29-32: Diamond tools */
	4, 20, 36, 52,            /* 33-36: Gold armor */
	68, 84, 100, 116,         /* 37-40: Gold tools */
	21, 37, 53, 5, 6,         /* 41-45: Bow, Arrow, Stick, Flint&Steel, Flint */
	7, 23, 39, 55,            /* 46-49: Coal, Iron Ingot, Gold Ingot, Diamond */
	71, 72, 87, 88,           /* 50-53: Bowl, Mushroom Stew, Raw Pork, Cooked Pork */
	40, 24, 8,                /* 54-56: Sulphur, Feather, String */
	128, 129, 130, 131, 132,  /* 57-61: Wood Hoe, Stone Hoe, Iron Hoe, Diamond Hoe, Gold Hoe */
	9, 25, 41,                 /* 62-64: Seeds, Wheat, Bread */
	74, 75, 76,                /* 65-67: Bucket, Water Bucket, Lava Bucket */
	42                         /* 68:    Sign */
};

const int ItemDamage[ITEM_COUNT] = {
	0,                        /* 0:  Air (bare hand) */
	1, 1, 1, 1,              /* 1-4:   Cloth armor */
	4, 2, 2, 2,              /* 5-8:   Wood sword, shovel, pickaxe, axe */
	1, 1, 1, 1,              /* 9-12:  Chainmail armor */
	8, 4, 4, 4,              /* 13-16: Stone sword, shovel, pickaxe, axe */
	1, 1, 1, 1,              /* 17-20: Iron armor */
	12, 6, 6, 6,             /* 21-24: Iron sword, shovel, pickaxe, axe */
	1, 1, 1, 1,              /* 25-28: Diamond armor */
	16, 8, 8, 8,             /* 29-32: Diamond sword, shovel, pickaxe, axe */
	1, 1, 1, 1,              /* 33-36: Gold armor */
	4, 2, 2, 2,              /* 37-40: Gold sword, shovel, pickaxe, axe */
	1, 2, 2, 1, 1,           /* 41-45: Bow, Arrow, Stick, Flint&Steel, Flint */
	1, 1, 1, 1,              /* 46-49: Coal, Iron Ingot, Gold Ingot, Diamond */
	1, 1, 1, 1,              /* 50-53: Bowl, Mushroom Stew, Raw Pork, Cooked Pork */
	1, 1, 1,                 /* 54-56: Sulphur, Feather, String */
	2, 4, 6, 8, 2,           /* 57-61: Wood Hoe, Stone Hoe, Iron Hoe, Diamond Hoe, Gold Hoe */
	1, 1, 1,                  /* 62-64: Seeds, Wheat, Bread */
	1, 1, 1,                  /* 65-67: Bucket, Water Bucket, Lava Bucket */
	1                          /* 68:    Sign */
};

/* Armor defense points per item (matches vanilla Minecraft pre-1.9 values).
   Helmet=slot0, Chestplate=slot1, Leggings=slot2, Boots=slot3.
   Full sets: Cloth=7, Chainmail=12, Gold=11, Iron=15, Diamond=20. */
const int ItemArmorPoints[ITEM_COUNT] = {
	0,                        /* 0:  Air */
	1, 3, 2, 1,              /* 1-4:   Cloth armor (helm, chest, legs, boots) */
	0, 0, 0, 0,              /* 5-8:   Wood tools */
	2, 5, 4, 1,              /* 9-12:  Chainmail armor */
	0, 0, 0, 0,              /* 13-16: Stone tools */
	2, 6, 5, 2,              /* 17-20: Iron armor */
	0, 0, 0, 0,              /* 21-24: Iron tools */
	3, 8, 6, 3,              /* 25-28: Diamond armor */
	0, 0, 0, 0,              /* 29-32: Diamond tools */
	2, 5, 3, 1,              /* 33-36: Gold armor */
	0, 0, 0, 0,              /* 37-40: Gold tools */
	0, 0, 0, 0, 0,           /* 41-45: Bow, Arrow, Stick, Flint&Steel, Flint */
	0, 0, 0, 0,              /* 46-49: Coal, Iron Ingot, Gold Ingot, Diamond */
	0, 0, 0, 0,              /* 50-53: Bowl, Mushroom Stew, Raw Pork, Cooked Pork */
	0, 0, 0,                 /* 54-56: Sulphur, Feather, String */
	0, 0, 0, 0, 0,           /* 57-61: Wood Hoe, Stone Hoe, Iron Hoe, Diamond Hoe, Gold Hoe */
	0, 0, 0,                  /* 62-64: Seeds, Wheat, Bread */
	0, 0, 0,                  /* 65-67: Bucket, Water Bucket, Lava Bucket */
	0                          /* 68:    Sign */
};

/* Max durability per item (0 = no durability / infinite).
   Values from Minecraft Alpha. Tools/armor only; hoes and misc items are infinite. */
const int ItemMaxDurability[ITEM_COUNT] = {
	0,                          /* 0:  Air */
	64,  64,  64,  64,          /* 1-4:   Cloth armor */
	32,  32,  32,  32,          /* 5-8:   Wood sword/shovel/pickaxe/axe */
	128, 128, 128, 128,         /* 9-12:  Chainmail armor */
	64,  64,  64,  64,          /* 13-16: Stone sword/shovel/pickaxe/axe */
	256, 256, 256, 256,         /* 17-20: Iron armor */
	128, 128, 128, 128,         /* 21-24: Iron sword/shovel/pickaxe/axe */
	512, 512, 512, 512,         /* 25-28: Diamond armor */
	1024,1024,1024,1024,        /* 29-32: Diamond sword/shovel/pickaxe/axe */
	64,  64,  64,  64,          /* 33-36: Gold armor */
	32,  32,  32,  32,          /* 37-40: Gold sword/shovel/pickaxe/axe */
	0, 0, 0, 64, 0,             /* 41-45: Bow(inf), Arrow(inf), Stick(inf), Flint&Steel(64), Flint(inf) */
	0, 0, 0, 0,                 /* 46-49: Coal, Iron Ingot, Gold Ingot, Diamond */
	0, 0, 0, 0,                 /* 50-53: Bowl, Mushroom Stew, Raw Pork, Cooked Pork */
	0, 0, 0,                    /* 54-56: Sulphur, Feather, String */
	0, 0, 0, 0, 0,              /* 57-61: Hoes (infinite) */
	0, 0, 0,                     /* 62-64: Seeds, Wheat, Bread */
	0, 0, 0                      /* 65-67: Bucket, Water Bucket, Lava Bucket */
	, 0                          /* 68:    Sign */
};

int Item_MaxStackSize(int itemId) {
	/* Armor: not stackable */
	if (itemId >= 1  && itemId <= 4)  return 1; /* Cloth */
	if (itemId >= 9  && itemId <= 12) return 1; /* Chainmail */
	if (itemId >= 17 && itemId <= 20) return 1; /* Iron */
	if (itemId >= 25 && itemId <= 28) return 1; /* Diamond */
	if (itemId >= 33 && itemId <= 36) return 1; /* Gold */
	/* Tools/weapons: not stackable */
	if (itemId >= 5  && itemId <= 8)  return 1; /* Wood tools */
	if (itemId >= 13 && itemId <= 16) return 1; /* Stone tools */
	if (itemId >= 21 && itemId <= 24) return 1; /* Iron tools */
	if (itemId >= 29 && itemId <= 32) return 1; /* Diamond tools */
	if (itemId >= 37 && itemId <= 40) return 1; /* Gold tools */
	if (itemId >= 57 && itemId <= 61) return 1; /* Hoes */
	if (itemId == ITEM_BOW)        return 1;
	if (itemId == ITEM_FLINT_STEEL) return 1;
	/* Signs: not stackable */
	if (itemId == ITEM_SIGN) return 1;
	/* Buckets: not stackable */
	if (itemId == ITEM_BUCKET)       return 1;
	if (itemId == ITEM_WATER_BUCKET) return 1;
	if (itemId == ITEM_LAVA_BUCKET)  return 1;
	/* Food: not stackable */
	if (itemId == 51) return 1; /* Mushroom Stew */
	if (itemId == 52) return 1; /* Raw Pork */
	if (itemId == 53) return 1; /* Cooked Pork */
	if (itemId == ITEM_BREAD) return 1;
	/* Everything else stacks to 64 */
	return MAX_STACK_SIZE;
}

int Block_MaxStackSize(BlockID block) {
	/* Doors are not stackable (like Minecraft Alpha) */
	if (block == BLOCK_DOOR_NS_BOTTOM || block == BLOCK_DOOR_NS_TOP ||
		block == BLOCK_DOOR_EW_BOTTOM || block == BLOCK_DOOR_EW_TOP)
		return 1;
	if (block == BLOCK_IRON_DOOR || block == BLOCK_IRON_DOOR_NS_TOP ||
		block == BLOCK_IRON_DOOR_EW_BOTTOM || block == BLOCK_IRON_DOOR_EW_TOP ||
		block == BLOCK_IRON_DOOR_NS_OPEN_BOTTOM || block == BLOCK_IRON_DOOR_NS_OPEN_TOP ||
		block == BLOCK_IRON_DOOR_EW_OPEN_BOTTOM || block == BLOCK_IRON_DOOR_EW_OPEN_TOP)
		return 1;
	return MAX_STACK_SIZE;
}

cc_bool Inventory_CheckChangeSelected(void) {
	if (!Inventory.CanChangeSelected) {
		Chat_AddRaw("&cThe server has forbidden you from changing your held block.");
		return false;
	}
	return true;
}

void Inventory_SetSelectedIndex(int index) {
	if (!Inventory_CheckChangeSelected()) return;
	Inventory.SelectedIndex = index;
	Event_RaiseVoid(&UserEvents.HeldBlockChanged);
}

void Inventory_SetHotbarIndex(int index) {
	if (!Inventory_CheckChangeSelected() || Game_ClassicMode) return;
	Inventory.Offset = index * INVENTORY_BLOCKS_PER_HOTBAR;
	Event_RaiseVoid(&UserEvents.HeldBlockChanged);
}

void Inventory_SwitchHotbar(void) {
	int index = Inventory.Offset == 0 ? 1 : 0;
	Inventory_SetHotbarIndex(index);
}

void Inventory_SetSelectedBlock(BlockID block) {
	if (!Inventory_CheckChangeSelected()) return;

	/* Clear any item in the selected slot when placing a block */
	Hotbar_SetItem(Inventory.SelectedIndex, ITEM_NONE);

	Inventory_Set(Inventory.SelectedIndex, block);
	Event_RaiseVoid(&UserEvents.HeldBlockChanged);
	CPE_SendNotifyAction(NOTIFY_ACTION_BLOCK_LIST_SELECTED, block);
}

void Inventory_PickBlock(BlockID block) {
	int i;
	if (!Inventory_CheckChangeSelected() || Inventory_SelectedBlock == block) return;

	/* Vanilla classic client doesn't let you select these blocks */
	if (Game_PureClassic) {
		if (block == BLOCK_GRASS)       block = BLOCK_DIRT;
		if (block == BLOCK_DOUBLE_SLAB) block = BLOCK_SLAB;
	}

	/* Try to replace same block */
	for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR; i++) {
		if (Inventory_Get(i) != block) continue;
		Inventory_SetSelectedIndex(i); return;
	}

	if (AutoRotate_Enabled) {
		/* Try to replace existing autorotate variant */
		for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR; i++) {
			if (AutoRotate_BlocksShareGroup(Inventory_Get(i), block)) {
				Inventory_SetSelectedIndex(i);
				Inventory_SetSelectedBlock(block);
				return;
			}
		}
	}

	/* Is the currently selected slot truly empty? (no block AND no item) */
	if (Inventory_SelectedBlock == BLOCK_AIR && Hotbar_SelectedItem == ITEM_NONE) {
		Inventory_SetSelectedBlock(block); return;
	}

	/* Try to replace empty slots (must have no block AND no item) */
	for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR; i++) {
		if (Inventory_Get(i) != BLOCK_AIR) continue;
		if (Hotbar_GetItem(i) != ITEM_NONE) continue;
		Inventory_Set(i, block);
		Inventory_SetSelectedIndex(i); return;
	}

	/* Finally, replace the currently selected block */
	Inventory_SetSelectedBlock(block);
}

/* Returns default block that should go in the given inventory slot */
static BlockID DefaultMapping(int slot) {
	BlockID block;
	if (Game_ClassicMode) {
		if (slot < Game_Version.InventorySize) return Game_Version.Inventory[slot];
	} else if (slot < Game_Version.MaxCoreBlock) {
		block = (BlockID)(slot + 1);
		/* Skip non-placeable blocks in 200+ range */
		if (block >= 200) return BLOCK_AIR;
		return block;
	}
	return BLOCK_AIR;
}

void Inventory_ResetMapping(void) {
	int slot;
	for (slot = 0; slot < Array_Elems(Inventory.Map); slot++) {
		Inventory.Map[slot] = DefaultMapping(slot);
	}
}

void Inventory_AddDefault(BlockID block) {
	int slot;
	if (block > BLOCK_MAX_CPE) {
		Inventory.Map[block - 1] = block; return;
	}
	
	for (slot = 0; slot < BLOCK_MAX_CPE; slot++) {
		if (DefaultMapping(slot) != block) continue;
		Inventory.Map[slot] = block;
		return;
	}
}

void Inventory_Remove(BlockID block) {
	int slot;
	for (slot = 0; slot < Array_Elems(Inventory.Map); slot++) {
		if (Inventory.Map[slot] == block) Inventory.Map[slot] = BLOCK_AIR;
	}
}


/*########################################################################################################################*
*-----------------------------------------------Inventory save/load------------------------------------------------------*
*#########################################################################################################################*/
#define INV_SAVE_VERSION 2

void Inventory_SaveToFile(const cc_string* path) {
	cc_uint8 header[4];
	struct Stream stream;
	cc_filepath raw;
	cc_result res;
	struct Entity* p = &Entities.CurPlayer->Base;

	Platform_EncodePath(&raw, path);
	res = Stream_CreatePath(&stream, &raw);
	if (res) { Logger_IOWarn2(res, "saving inventory", &raw); return; }

	/* Header */
	header[0] = INV_SAVE_VERSION;
	header[1] = header[2] = header[3] = 0;
	Stream_Write(&stream, header, 4);

	/* Hotbar state */
	Stream_Write(&stream, (cc_uint8*)&Inventory.SelectedIndex, 4);
	Stream_Write(&stream, (cc_uint8*)&Inventory.Offset, 4);
	Stream_Write(&stream, (cc_uint8*)Inventory.Table, sizeof(Inventory.Table));
	Stream_Write(&stream, (cc_uint8*)HotbarItems, sizeof(HotbarItems));
	Stream_Write(&stream, (cc_uint8*)HotbarCounts, sizeof(HotbarCounts));
	Stream_Write(&stream, (cc_uint8*)HotbarDurability, sizeof(HotbarDurability));

	/* Survival inventory */
	Stream_Write(&stream, (cc_uint8*)SurvInv_Main, sizeof(SurvInv_Main));
	Stream_Write(&stream, (cc_uint8*)SurvInv_Armor, sizeof(SurvInv_Armor));
	Stream_Write(&stream, (cc_uint8*)SurvInv_Craft, sizeof(SurvInv_Craft));
	Stream_Write(&stream, (cc_uint8*)&SurvInv_Output, sizeof(SurvInv_Output));

	/* Player position and orientation */
	Stream_Write(&stream, (cc_uint8*)&p->Position.x, 4);
	Stream_Write(&stream, (cc_uint8*)&p->Position.y, 4);
	Stream_Write(&stream, (cc_uint8*)&p->Position.z, 4);
	Stream_Write(&stream, (cc_uint8*)&p->Yaw, 4);
	Stream_Write(&stream, (cc_uint8*)&p->Pitch, 4);

	stream.Close(&stream);
}

void Inventory_LoadFromFile(const cc_string* path) {
	cc_uint8 header[4];
	struct Stream stream;
	cc_filepath raw;
	cc_result res;
	struct Entity* p = &Entities.CurPlayer->Base;
	struct LocationUpdate update;
	Vec3 pos;
	float yaw, pitch;

	Platform_EncodePath(&raw, path);
	res = Stream_OpenPath(&stream, &raw);
	if (res) return; /* File doesn't exist = fresh save, use defaults */

	Stream_Read(&stream, header, 4);
	if (header[0] != INV_SAVE_VERSION) { stream.Close(&stream); return; }

	Stream_Read(&stream, (cc_uint8*)&Inventory.SelectedIndex, 4);
	Stream_Read(&stream, (cc_uint8*)&Inventory.Offset, 4);
	Stream_Read(&stream, (cc_uint8*)Inventory.Table, sizeof(Inventory.Table));
	Stream_Read(&stream, (cc_uint8*)HotbarItems, sizeof(HotbarItems));
	Stream_Read(&stream, (cc_uint8*)HotbarCounts, sizeof(HotbarCounts));
	Stream_Read(&stream, (cc_uint8*)HotbarDurability, sizeof(HotbarDurability));

	Stream_Read(&stream, (cc_uint8*)SurvInv_Main, sizeof(SurvInv_Main));
	Stream_Read(&stream, (cc_uint8*)SurvInv_Armor, sizeof(SurvInv_Armor));
	Stream_Read(&stream, (cc_uint8*)SurvInv_Craft, sizeof(SurvInv_Craft));
	Stream_Read(&stream, (cc_uint8*)&SurvInv_Output, sizeof(SurvInv_Output));

	/* Player position and orientation */
	Stream_Read(&stream, (cc_uint8*)&pos.x, 4);
	Stream_Read(&stream, (cc_uint8*)&pos.y, 4);
	Stream_Read(&stream, (cc_uint8*)&pos.z, 4);
	Stream_Read(&stream, (cc_uint8*)&yaw, 4);
	Stream_Read(&stream, (cc_uint8*)&pitch, 4);

	stream.Close(&stream);

	/* Restore player position */
	Mem_Set(&update, 0, sizeof(update));
	update.flags = LU_HAS_POS | LU_HAS_YAW | LU_HAS_PITCH;
	update.pos   = pos;
	update.yaw   = yaw;
	update.pitch = pitch;
	p->VTABLE->SetLocation(p, &update);
}


/*########################################################################################################################*
*----------------------------------------------------Crafting system----------------------------------------------------*
*#########################################################################################################################*/
struct SurvInvSlot CraftTable_Grid[9];
struct SurvInvSlot CraftTable_Output;

/* Helper: convert SurvInvSlot to tagged value for recipe matching */
static int Craft_SlotTag(const struct SurvInvSlot* s) {
	if (s->count <= 0) return CRAFT_EMPTY;
	if (s->itemId != ITEM_NONE) return CI(s->itemId);
	if (s->block  != BLOCK_AIR) return CB(s->block);
	return CRAFT_EMPTY;
}

/* ---- Tool recipe helper macros ---- */
/* Sword: M / M / Stick (1x3) */
#define SWORD_RECIPE(mat, itemOut) \
	{ 1, 3, { mat, mat, CI(ITEM_STICK), 0,0,0,0,0,0 }, BLOCK_AIR, itemOut, 1 }

/* Shovel: M / Stick / Stick (1x3) */
#define SHOVEL_RECIPE(mat, itemOut) \
	{ 1, 3, { mat, CI(ITEM_STICK), CI(ITEM_STICK), 0,0,0,0,0,0 }, BLOCK_AIR, itemOut, 1 }

/* Pickaxe: MMM / _S_ / _S_ (3x3) */
#define PICKAXE_RECIPE(mat, itemOut) \
	{ 3, 3, { mat, mat, mat, 0, CI(ITEM_STICK), 0, 0, CI(ITEM_STICK), 0 }, BLOCK_AIR, itemOut, 1 }

/* Axe left: MM / MS / _S (2x3) */
#define AXE_L_RECIPE(mat, itemOut) \
	{ 2, 3, { mat, mat, mat, CI(ITEM_STICK), 0, CI(ITEM_STICK), 0,0,0 }, BLOCK_AIR, itemOut, 1 }

/* Axe right (mirrored): MM / SM / S_ (2x3) */
#define AXE_R_RECIPE(mat, itemOut) \
	{ 2, 3, { mat, mat, CI(ITEM_STICK), mat, CI(ITEM_STICK), 0, 0,0,0 }, BLOCK_AIR, itemOut, 1 }

/* Hoe left: MM / _S / _S (2x3) */
#define HOE_L_RECIPE(mat, itemOut) \
	{ 2, 3, { mat, mat, 0, CI(ITEM_STICK), 0, CI(ITEM_STICK), 0,0,0 }, BLOCK_AIR, itemOut, 1 }

/* Hoe right (mirrored): MM / S_ / S_ (2x3) */
#define HOE_R_RECIPE(mat, itemOut) \
	{ 2, 3, { mat, mat, CI(ITEM_STICK), 0, CI(ITEM_STICK), 0, 0,0,0 }, BLOCK_AIR, itemOut, 1 }

/* ---- Armor recipe helper macros ---- */
/* Helmet: MMM / M_M (3x2) */
#define HELMET_RECIPE(mat, itemOut) \
	{ 3, 2, { mat, mat, mat, mat, 0, mat, 0,0,0 }, BLOCK_AIR, itemOut, 1 }

/* Chestplate: M_M / MMM / MMM (3x3) */
#define CHESTPLATE_RECIPE(mat, itemOut) \
	{ 3, 3, { mat, 0, mat, mat, mat, mat, mat, mat, mat }, BLOCK_AIR, itemOut, 1 }

/* Leggings: MMM / M_M / M_M (3x3) */
#define LEGGINGS_RECIPE(mat, itemOut) \
	{ 3, 3, { mat, mat, mat, mat, 0, mat, mat, 0, mat }, BLOCK_AIR, itemOut, 1 }

/* Boots: M_M / M_M (3x2) */
#define BOOTS_RECIPE(mat, itemOut) \
	{ 3, 2, { mat, 0, mat, mat, 0, mat, 0,0,0 }, BLOCK_AIR, itemOut, 1 }

static const struct CraftRecipe craftRecipes[] = {
	/* ===== 2x2 recipes (also work on 3x3) ===== */
	/* Log -> 4 Planks (1x1) */
	{ 1, 1, { CB(BLOCK_LOG), 0,0,0,0,0,0,0,0 }, BLOCK_WOOD, ITEM_NONE, 4 },
	/* Coal above Stick -> 4 Torches (1x2) */
	{ 1, 2, { CI(ITEM_COAL), CI(ITEM_STICK), 0,0,0,0,0,0,0 }, BLOCK_TORCH, ITEM_NONE, 4 },
	/* 2 Planks vertical -> 4 Sticks (1x2) */
	{ 1, 2, { CB(BLOCK_WOOD), CB(BLOCK_WOOD), 0,0,0,0,0,0,0 }, BLOCK_AIR, ITEM_STICK, 4 },
	/* 4 Planks 2x2 -> Crafting Table (2x2) */
	{ 2, 2, { CB(BLOCK_WOOD), CB(BLOCK_WOOD), CB(BLOCK_WOOD), CB(BLOCK_WOOD), 0,0,0,0,0 }, BLOCK_CRAFT, ITEM_NONE, 1 },
	/* Stick above Cobble -> Lever (1x2) */
	{ 1, 2, { CI(ITEM_STICK), CB(BLOCK_COBBLE), 0,0,0,0,0,0,0 }, BLOCK_LEVER, ITEM_NONE, 1 },
	/* Red Ore Dust above Stick -> Red Ore Torch (1x2) */
	{ 1, 2, { CB(BLOCK_RED_ORE_DUST), CI(ITEM_STICK), 0,0,0,0,0,0,0 }, BLOCK_RED_ORE_TORCH, ITEM_NONE, 1 },
	/* 2 Stone vertical -> Button (1x2) */
	{ 1, 2, { CB(BLOCK_STONE), CB(BLOCK_STONE), 0,0,0,0,0,0,0 }, BLOCK_BUTTON, ITEM_NONE, 1 },

	/* ===== Tool recipes (5 material tiers) ===== */
	/* Wood tools (material = BLOCK_WOOD as block) */
	SWORD_RECIPE(CB(BLOCK_WOOD), ITEM_WOOD_SWORD),
	SHOVEL_RECIPE(CB(BLOCK_WOOD), ITEM_WOOD_SHOVEL),
	PICKAXE_RECIPE(CB(BLOCK_WOOD), ITEM_WOOD_PICKAXE),
	AXE_L_RECIPE(CB(BLOCK_WOOD), ITEM_WOOD_AXE),
	AXE_R_RECIPE(CB(BLOCK_WOOD), ITEM_WOOD_AXE),
	HOE_L_RECIPE(CB(BLOCK_WOOD), ITEM_WOOD_HOE),
	HOE_R_RECIPE(CB(BLOCK_WOOD), ITEM_WOOD_HOE),

	/* Stone tools (material = BLOCK_COBBLE as block) */
	SWORD_RECIPE(CB(BLOCK_COBBLE), ITEM_STONE_SWORD),
	SHOVEL_RECIPE(CB(BLOCK_COBBLE), ITEM_STONE_SHOVEL),
	PICKAXE_RECIPE(CB(BLOCK_COBBLE), ITEM_STONE_PICKAXE),
	AXE_L_RECIPE(CB(BLOCK_COBBLE), ITEM_STONE_AXE),
	AXE_R_RECIPE(CB(BLOCK_COBBLE), ITEM_STONE_AXE),
	HOE_L_RECIPE(CB(BLOCK_COBBLE), ITEM_STONE_HOE),
	HOE_R_RECIPE(CB(BLOCK_COBBLE), ITEM_STONE_HOE),

	/* Iron tools (material = ITEM_IRON_INGOT as item) */
	SWORD_RECIPE(CI(ITEM_IRON_INGOT), ITEM_IRON_SWORD),
	SHOVEL_RECIPE(CI(ITEM_IRON_INGOT), ITEM_IRON_SHOVEL),
	PICKAXE_RECIPE(CI(ITEM_IRON_INGOT), ITEM_IRON_PICKAXE),
	AXE_L_RECIPE(CI(ITEM_IRON_INGOT), ITEM_IRON_AXE),
	AXE_R_RECIPE(CI(ITEM_IRON_INGOT), ITEM_IRON_AXE),
	HOE_L_RECIPE(CI(ITEM_IRON_INGOT), ITEM_IRON_HOE),
	HOE_R_RECIPE(CI(ITEM_IRON_INGOT), ITEM_IRON_HOE),

	/* Diamond tools (material = ITEM_DIAMOND as item) */
	SWORD_RECIPE(CI(ITEM_DIAMOND_ITEM), ITEM_DIAMOND_SWORD),
	SHOVEL_RECIPE(CI(ITEM_DIAMOND_ITEM), ITEM_DIAMOND_SHOVEL),
	PICKAXE_RECIPE(CI(ITEM_DIAMOND_ITEM), ITEM_DIAMOND_PICKAXE),
	AXE_L_RECIPE(CI(ITEM_DIAMOND_ITEM), ITEM_DIAMOND_AXE),
	AXE_R_RECIPE(CI(ITEM_DIAMOND_ITEM), ITEM_DIAMOND_AXE),
	HOE_L_RECIPE(CI(ITEM_DIAMOND_ITEM), ITEM_DIAMOND_HOE),
	HOE_R_RECIPE(CI(ITEM_DIAMOND_ITEM), ITEM_DIAMOND_HOE),

	/* Gold tools (material = ITEM_GOLD_INGOT as item) */
	SWORD_RECIPE(CI(ITEM_GOLD_INGOT), ITEM_GOLD_SWORD),
	SHOVEL_RECIPE(CI(ITEM_GOLD_INGOT), ITEM_GOLD_SHOVEL),
	PICKAXE_RECIPE(CI(ITEM_GOLD_INGOT), ITEM_GOLD_PICKAXE),
	AXE_L_RECIPE(CI(ITEM_GOLD_INGOT), ITEM_GOLD_AXE),
	AXE_R_RECIPE(CI(ITEM_GOLD_INGOT), ITEM_GOLD_AXE),
	HOE_L_RECIPE(CI(ITEM_GOLD_INGOT), ITEM_GOLD_HOE),
	HOE_R_RECIPE(CI(ITEM_GOLD_INGOT), ITEM_GOLD_HOE),

	/* ===== Other 3x3 recipes ===== */
	/* Bow: String Stick _ / String _ Stick / String Stick _ (3x3) */
	{ 3, 3, { CI(ITEM_STRING), CI(ITEM_STICK), 0,
	           CI(ITEM_STRING), 0, CI(ITEM_STICK),
	           CI(ITEM_STRING), CI(ITEM_STICK), 0 }, BLOCK_AIR, ITEM_BOW, 1 },

	/* Arrow: Flint / Stick / Feather (1x3) */
	{ 1, 3, { CI(ITEM_FLINT), CI(ITEM_STICK), CI(ITEM_FEATHER), 0,0,0,0,0,0 }, BLOCK_AIR, ITEM_ARROW, 4 },

	/* Slab: Cobble Cobble Cobble (3x1) -> 3 Slabs */
	{ 3, 1, { CB(BLOCK_COBBLE), CB(BLOCK_COBBLE), CB(BLOCK_COBBLE), 0,0,0,0,0,0 }, BLOCK_SLAB, ITEM_NONE, 3 },
	/* Wood Pressure Plate: 3 Planks horizontal (3x1) */
	{ 3, 1, { CB(BLOCK_WOOD), CB(BLOCK_WOOD), CB(BLOCK_WOOD), 0,0,0,0,0,0 }, BLOCK_PRESSURE_PLATE, ITEM_NONE, 1 },
	/* Stone Pressure Plate: 3 Stone blocks horizontal (3x1) */
	{ 3, 1, { CB(BLOCK_STONE), CB(BLOCK_STONE), CB(BLOCK_STONE), 0,0,0,0,0,0 }, BLOCK_STONE_PLATE, ITEM_NONE, 1 },

	/* Furnace: 8 Cobblestone ring (3x3, empty center) */
	{ 3, 3, { CB(BLOCK_COBBLE), CB(BLOCK_COBBLE), CB(BLOCK_COBBLE),
	           CB(BLOCK_COBBLE), 0,                CB(BLOCK_COBBLE),
	           CB(BLOCK_COBBLE), CB(BLOCK_COBBLE), CB(BLOCK_COBBLE) }, BLOCK_FURNACE, ITEM_NONE, 1 },

	/* Chest: 8 Wood Planks ring (3x3, empty center) */
	{ 3, 3, { CB(BLOCK_WOOD), CB(BLOCK_WOOD), CB(BLOCK_WOOD),
	           CB(BLOCK_WOOD), 0,              CB(BLOCK_WOOD),
	           CB(BLOCK_WOOD), CB(BLOCK_WOOD), CB(BLOCK_WOOD) }, BLOCK_CHEST, ITEM_NONE, 1 },

	/* Wooden Door: 6 Planks (2x3) -> 1 Door */
	{ 2, 3, { CB(BLOCK_WOOD), CB(BLOCK_WOOD),
	           CB(BLOCK_WOOD), CB(BLOCK_WOOD),
	           CB(BLOCK_WOOD), CB(BLOCK_WOOD), 0,0,0 }, BLOCK_DOOR_NS_BOTTOM, ITEM_NONE, 1 },

	/* Iron Door: 6 Iron Ingots (2x3) -> 1 Iron Door */
	{ 2, 3, { CI(ITEM_IRON_INGOT), CI(ITEM_IRON_INGOT),
	           CI(ITEM_IRON_INGOT), CI(ITEM_IRON_INGOT),
	           CI(ITEM_IRON_INGOT), CI(ITEM_IRON_INGOT), 0,0,0 }, BLOCK_IRON_DOOR, ITEM_NONE, 1 },

	/* Sign: 6 Planks + Stick (3x3) -> 1 Sign */
	{ 3, 3, { CB(BLOCK_WOOD), CB(BLOCK_WOOD), CB(BLOCK_WOOD),
	           CB(BLOCK_WOOD), CB(BLOCK_WOOD), CB(BLOCK_WOOD),
	           0,              CI(ITEM_STICK), 0 }, BLOCK_AIR, ITEM_SIGN, 1 },

	/* Ladder: 7 Sticks in H pattern (3x3) -> 1 Ladder */
	{ 3, 3, { CI(ITEM_STICK), 0,              CI(ITEM_STICK),
	           CI(ITEM_STICK), CI(ITEM_STICK), CI(ITEM_STICK),
	           CI(ITEM_STICK), 0,              CI(ITEM_STICK) }, BLOCK_LADDER, ITEM_NONE, 1 },

	/* Rail: 6 Iron Ingots + 1 Stick (3x3) -> 16 Rails */
	{ 3, 3, { CI(ITEM_IRON_INGOT), 0,              CI(ITEM_IRON_INGOT),
	           CI(ITEM_IRON_INGOT), CI(ITEM_STICK), CI(ITEM_IRON_INGOT),
	           CI(ITEM_IRON_INGOT), 0,              CI(ITEM_IRON_INGOT) }, BLOCK_RAIL, ITEM_NONE, 16 },

	/* TNT: Checkerboard of Sulphur and Sand (3x3) */
	{ 3, 3, { CI(ITEM_SULPHUR), CB(BLOCK_SAND), CI(ITEM_SULPHUR),
	           CB(BLOCK_SAND), CI(ITEM_SULPHUR), CB(BLOCK_SAND),
	           CI(ITEM_SULPHUR), CB(BLOCK_SAND), CI(ITEM_SULPHUR) }, BLOCK_TNT, ITEM_NONE, 1 },

	/* ===== Armor recipes (5 material tiers) ===== */
	/* Cloth armor (material = BLOCK_WHITE / wool) */
	HELMET_RECIPE(CB(BLOCK_WHITE), 1),
	CHESTPLATE_RECIPE(CB(BLOCK_WHITE), 2),
	LEGGINGS_RECIPE(CB(BLOCK_WHITE), 3),
	BOOTS_RECIPE(CB(BLOCK_WHITE), 4),

	/* Chainmail armor (material = ITEM_FLINT) */
	HELMET_RECIPE(CI(ITEM_FLINT), 9),
	CHESTPLATE_RECIPE(CI(ITEM_FLINT), 10),
	LEGGINGS_RECIPE(CI(ITEM_FLINT), 11),
	BOOTS_RECIPE(CI(ITEM_FLINT), 12),

	/* Iron armor (material = ITEM_IRON_INGOT) */
	HELMET_RECIPE(CI(ITEM_IRON_INGOT), 17),
	CHESTPLATE_RECIPE(CI(ITEM_IRON_INGOT), 18),
	LEGGINGS_RECIPE(CI(ITEM_IRON_INGOT), 19),
	BOOTS_RECIPE(CI(ITEM_IRON_INGOT), 20),

	/* Diamond armor (material = ITEM_DIAMOND) */
	HELMET_RECIPE(CI(ITEM_DIAMOND_ITEM), 25),
	CHESTPLATE_RECIPE(CI(ITEM_DIAMOND_ITEM), 26),
	LEGGINGS_RECIPE(CI(ITEM_DIAMOND_ITEM), 27),
	BOOTS_RECIPE(CI(ITEM_DIAMOND_ITEM), 28),

	/* Gold armor (material = ITEM_GOLD_INGOT) */
	HELMET_RECIPE(CI(ITEM_GOLD_INGOT), 33),
	CHESTPLATE_RECIPE(CI(ITEM_GOLD_INGOT), 34),
	LEGGINGS_RECIPE(CI(ITEM_GOLD_INGOT), 35),
	BOOTS_RECIPE(CI(ITEM_GOLD_INGOT), 36),

	/* ===== Storage block recipes (9 items -> block) ===== */
	/* 9 Iron Ingots -> Iron Block (3x3) */
	{ 3, 3, { CI(ITEM_IRON_INGOT), CI(ITEM_IRON_INGOT), CI(ITEM_IRON_INGOT),
	           CI(ITEM_IRON_INGOT), CI(ITEM_IRON_INGOT), CI(ITEM_IRON_INGOT),
	           CI(ITEM_IRON_INGOT), CI(ITEM_IRON_INGOT), CI(ITEM_IRON_INGOT) }, BLOCK_IRON, ITEM_NONE, 1 },
	/* 9 Gold Ingots -> Gold Block (3x3) */
	{ 3, 3, { CI(ITEM_GOLD_INGOT), CI(ITEM_GOLD_INGOT), CI(ITEM_GOLD_INGOT),
	           CI(ITEM_GOLD_INGOT), CI(ITEM_GOLD_INGOT), CI(ITEM_GOLD_INGOT),
	           CI(ITEM_GOLD_INGOT), CI(ITEM_GOLD_INGOT), CI(ITEM_GOLD_INGOT) }, BLOCK_GOLD, ITEM_NONE, 1 },
	/* 9 Diamonds -> Diamond Block (3x3) */
	{ 3, 3, { CI(ITEM_DIAMOND_ITEM), CI(ITEM_DIAMOND_ITEM), CI(ITEM_DIAMOND_ITEM),
	           CI(ITEM_DIAMOND_ITEM), CI(ITEM_DIAMOND_ITEM), CI(ITEM_DIAMOND_ITEM),
	           CI(ITEM_DIAMOND_ITEM), CI(ITEM_DIAMOND_ITEM), CI(ITEM_DIAMOND_ITEM) }, BLOCK_DIAMOND_BLOCK, ITEM_NONE, 1 },

	/* Bowl: 3 Planks in V shape (3x2) -> 4 Bowls */
	{ 3, 2, { CB(BLOCK_WOOD), 0, CB(BLOCK_WOOD),
	           0, CB(BLOCK_WOOD), 0, 0,0,0 }, BLOCK_AIR, ITEM_BOWL, 4 },

	/* Bucket: 3 Iron Ingots in V shape (3x2) -> 1 Bucket */
	{ 3, 2, { CI(ITEM_IRON_INGOT), 0, CI(ITEM_IRON_INGOT),
	           0, CI(ITEM_IRON_INGOT), 0, 0,0,0 }, BLOCK_AIR, ITEM_BUCKET, 1 },

	/* Bread: 3 Wheat in a row (3x1) */
	{ 3, 1, { CI(ITEM_WHEAT), CI(ITEM_WHEAT), CI(ITEM_WHEAT), 0,0,0,0,0,0 }, BLOCK_AIR, ITEM_BREAD, 1 },

	/* ===== De-crafting recipes (block -> 9 items, 1x1) ===== */
	/* Iron Block -> 9 Iron Ingots */
	{ 1, 1, { CB(BLOCK_IRON), 0,0,0,0,0,0,0,0 }, BLOCK_AIR, ITEM_IRON_INGOT, 9 },
	/* Gold Block -> 9 Gold Ingots */
	{ 1, 1, { CB(BLOCK_GOLD), 0,0,0,0,0,0,0,0 }, BLOCK_AIR, ITEM_GOLD_INGOT, 9 },
	/* Diamond Block -> 9 Diamonds */
	{ 1, 1, { CB(BLOCK_DIAMOND_BLOCK), 0,0,0,0,0,0,0,0 }, BLOCK_AIR, ITEM_DIAMOND_ITEM, 9 },
};
#define CRAFT_RECIPE_COUNT (int)(sizeof(craftRecipes) / sizeof(craftRecipes[0]))

/* ===== Shapeless recipes ===== */
struct ShapelessRecipe {
	int ingredients[9]; /* tagged ingredients (terminated by CRAFT_EMPTY) */
	int count;          /* number of ingredients */
	BlockID outBlock;
	int outItem;
	int outCount;
};

static const struct ShapelessRecipe shapelessRecipes[] = {
	/* Flint & Steel: Iron Ingot + Flint in any arrangement */
	{ { CI(ITEM_IRON_INGOT), CI(ITEM_FLINT) }, 2, BLOCK_AIR, ITEM_FLINT_STEEL, 1 },
	/* Mushroom Stew: Bowl + Brown Mushroom + Red Mushroom in any arrangement */
	{ { CI(ITEM_BOWL), CB(BLOCK_BROWN_SHROOM), CB(BLOCK_RED_SHROOM) }, 3, BLOCK_AIR, ITEM_MUSHROOM_STEW, 1 },
};
#define SHAPELESS_RECIPE_COUNT (int)(sizeof(shapelessRecipes) / sizeof(shapelessRecipes[0]))

/* Check if grid contents match a shapeless recipe (ingredients in any position).
   Returns shapeless recipe index or -1. */
static int Craft_FindShapelessMatch(int gridW, int gridH, const int* grid) {
	int totalSlots = gridW * gridH;
	int r, i, j, filledCount;

	for (r = 0; r < SHAPELESS_RECIPE_COUNT; r++) {
		const struct ShapelessRecipe* recipe = &shapelessRecipes[r];
		cc_bool used[9] = { false };
		cc_bool match = true;

		/* Count filled grid slots */
		filledCount = 0;
		for (i = 0; i < totalSlots; i++) {
			if (grid[i] != CRAFT_EMPTY) filledCount++;
		}
		if (filledCount != recipe->count) continue;

		/* Try to match each ingredient to a grid slot */
		for (i = 0; i < recipe->count && match; i++) {
			cc_bool found = false;
			for (j = 0; j < totalSlots; j++) {
				if (!used[j] && grid[j] == recipe->ingredients[i]) {
					used[j] = true;
					found = true;
					break;
				}
			}
			if (!found) match = false;
		}
		if (match) return r;
	}
	return -1;
}

/* Try to match a recipe against a grid. Returns recipe index or -1.
   If found, *outOX and *outOY are set to the offset where recipe matched. */
static int Craft_FindMatch(int gridW, int gridH, const int* grid, int* outOX, int* outOY) {
	int r, ox, oy, gx, gy, rx, ry;
	const struct CraftRecipe* recipe;

	for (r = 0; r < CRAFT_RECIPE_COUNT; r++) {
		recipe = &craftRecipes[r];
		if (recipe->width > gridW || recipe->height > gridH) continue;

		for (oy = 0; oy <= gridH - recipe->height; oy++) {
			for (ox = 0; ox <= gridW - recipe->width; ox++) {
				cc_bool match = true;

				for (gy = 0; gy < gridH && match; gy++) {
					for (gx = 0; gx < gridW && match; gx++) {
						int gridVal = grid[gy * gridW + gx];
						rx = gx - ox;
						ry = gy - oy;

						if (rx >= 0 && rx < recipe->width && ry >= 0 && ry < recipe->height) {
							/* Inside recipe area: must match pattern */
							if (gridVal != recipe->pattern[ry * recipe->width + rx])
								match = false;
						} else {
							/* Outside recipe area: must be empty */
							if (gridVal != CRAFT_EMPTY)
								match = false;
						}
					}
				}

				if (match) {
					*outOX = ox;
					*outOY = oy;
					return r;
				}
			}
		}
	}

	/* No shaped match — try shapeless recipes.
	   Encode as -(shapelessIdx + 2) so -1 still means "no match" */
	{
		int si = Craft_FindShapelessMatch(gridW, gridH, grid);
		if (si >= 0) {
			*outOX = 0;
			*outOY = 0;
			return -(si + 2);
		}
	}
	return -1;
}

/* Build tagged grid from SurvInvSlot array */
static void Craft_BuildGrid(const struct SurvInvSlot* slots, int count, int* grid) {
	int i;
	for (i = 0; i < count; i++) {
		grid[i] = Craft_SlotTag(&slots[i]);
	}
}

/* Set output slot from a recipe match.
   recipeIdx encodes: >= 0 for shaped, < -1 for shapeless (index = -recipeIdx - 2) */
static void Craft_SetOutput(struct SurvInvSlot* output, int recipeIdx) {
	if (recipeIdx >= 0) {
		const struct CraftRecipe* r = &craftRecipes[recipeIdx];
		output->block  = r->outBlock;
		output->itemId = r->outItem;
		output->count  = r->outCount;
	} else if (recipeIdx <= -2) {
		const struct ShapelessRecipe* r = &shapelessRecipes[-recipeIdx - 2];
		output->block  = r->outBlock;
		output->itemId = r->outItem;
		output->count  = r->outCount;
	} else {
		output->block  = BLOCK_AIR;
		output->itemId = ITEM_NONE;
		output->count  = 0;
	}
}

/* Consume materials from grid after taking output.
   recipeIdx encodes: >= 0 for shaped, < -1 for shapeless */
static void Craft_ConsumeInputs(struct SurvInvSlot* slots, int gridW, int gridH,
								int recipeIdx, int ox, int oy) {
	if (recipeIdx >= 0) {
		const struct CraftRecipe* r = &craftRecipes[recipeIdx];
		int rx, ry, gi;

		for (ry = 0; ry < r->height; ry++) {
			for (rx = 0; rx < r->width; rx++) {
				if (r->pattern[ry * r->width + rx] == CRAFT_EMPTY) continue;
				gi = (oy + ry) * gridW + (ox + rx);
				slots[gi].count--;
				if (slots[gi].count <= 0) {
					slots[gi].block  = BLOCK_AIR;
					slots[gi].itemId = ITEM_NONE;
					slots[gi].count  = 0;
				}
			}
		}
	} else if (recipeIdx <= -2) {
		/* Shapeless: consume one of each non-empty slot */
		int totalSlots = gridW * gridH;
		int i;
		for (i = 0; i < totalSlots; i++) {
			if (slots[i].count <= 0) continue;
			slots[i].count--;
			if (slots[i].count <= 0) {
				slots[i].block  = BLOCK_AIR;
				slots[i].itemId = ITEM_NONE;
				slots[i].count  = 0;
			}
		}
	}
}

void Crafting_UpdateOutput2x2(void) {
	int grid[4], ox, oy, match;
	Craft_BuildGrid(SurvInv_Craft, 4, grid);
	match = Craft_FindMatch(2, 2, grid, &ox, &oy);
	Craft_SetOutput(&SurvInv_Output, match);
}

void Crafting_UpdateOutput3x3(void) {
	int grid[9], ox, oy, match;
	Craft_BuildGrid(CraftTable_Grid, 9, grid);
	match = Craft_FindMatch(3, 3, grid, &ox, &oy);
	Craft_SetOutput(&CraftTable_Output, match);
}

void Crafting_TakeOutput2x2(void) {
	int grid[4], ox, oy, match;
	Craft_BuildGrid(SurvInv_Craft, 4, grid);
	match = Craft_FindMatch(2, 2, grid, &ox, &oy);
	Craft_ConsumeInputs(SurvInv_Craft, 2, 2, match, ox, oy);
	Crafting_UpdateOutput2x2();
}

void Crafting_TakeOutput3x3(void) {
	int grid[9], ox, oy, match;
	Craft_BuildGrid(CraftTable_Grid, 9, grid);
	match = Craft_FindMatch(3, 3, grid, &ox, &oy);
	Craft_ConsumeInputs(CraftTable_Grid, 3, 3, match, ox, oy);
	Crafting_UpdateOutput3x3();
}


/*########################################################################################################################*
*---------------------------------------------------Furnace Smelting----------------------------------------------------*
*#########################################################################################################################*/
struct FurnaceData Furnaces[MAX_FURNACES];
int Furnace_Count;
int Furnace_ViewIdx = -1;

/* Globals mirroring the currently-viewed furnace (for the screen) */
struct SurvInvSlot Furnace_Input;
struct SurvInvSlot Furnace_Fuel;
struct SurvInvSlot Furnace_Output;
float Furnace_SmeltProgress;
cc_bool Furnace_Active;
float Furnace_FuelBurnLeft;
float Furnace_FuelBurnTotal;

#define SMELT_TIME 10.0f /* seconds to smelt one item */

/* Returns burn time in seconds for a given fuel slot.
   Minecraft burn times: stick=5, wool=5, planks=15, log=15, coal=80, wooden tools=10 */
static float Furnace_GetFuelTime(struct SurvInvSlot* fuel) {
	if (fuel->count <= 0) return 0.0f;

	/* Block-based fuels */
	if (fuel->block != BLOCK_AIR && fuel->itemId == ITEM_NONE) {
		if (fuel->block == BLOCK_WOOD)  return 15.0f;  /* Planks */
		if (fuel->block == BLOCK_LOG)   return 15.0f;  /* Log */
		if (fuel->block >= BLOCK_RED && fuel->block <= BLOCK_WHITE) return 7.5f; /* Wool (half planks) */
		return 0.0f;
	}

	/* Item-based fuels */
	if (fuel->itemId != ITEM_NONE) {
		if (fuel->itemId == ITEM_COAL)   return 80.0f;
		if (fuel->itemId == ITEM_STICK)  return 7.5f;  /* Half planks */
		/* Wooden tools */
		if (fuel->itemId == ITEM_WOOD_SWORD)   return 15.0f; /* Same as planks */
		if (fuel->itemId == ITEM_WOOD_SHOVEL)  return 15.0f;
		if (fuel->itemId == ITEM_WOOD_PICKAXE) return 15.0f;
		if (fuel->itemId == ITEM_WOOD_AXE)     return 15.0f;
		if (fuel->itemId == ITEM_BOWL)         return 7.5f;  /* Same as sticks */
		return 0.0f;
	}
	return 0.0f;
}

struct SmeltRecipe {
	cc_bool isBlock;  /* true = input is block, false = input is item */
	int inputId;      /* block or item ID of input */
	cc_bool outIsBlock;
	BlockID outBlock;
	int outItem;
	int outCount;
};

static const struct SmeltRecipe smeltRecipes[] = {
	/* Iron Ore -> Iron Ingot */
	{ true, BLOCK_IRON_ORE, false, BLOCK_AIR, ITEM_IRON_INGOT, 1 },
	/* Gold Ore -> Gold Ingot */
	{ true, BLOCK_GOLD_ORE, false, BLOCK_AIR, ITEM_GOLD_INGOT, 1 },
	/* Sand -> Glass */
	{ true, BLOCK_SAND, true, BLOCK_GLASS, ITEM_NONE, 1 },
	/* Cobblestone -> Stone */
	{ true, BLOCK_COBBLE, true, BLOCK_STONE, ITEM_NONE, 1 },
	/* Raw Pork -> Cooked Pork */
	{ false, 52, false, BLOCK_AIR, 53, 1 },
};
#define SMELT_RECIPE_COUNT (int)(sizeof(smeltRecipes) / sizeof(smeltRecipes[0]))

static int Furnace_FindRecipeFor(struct SurvInvSlot* input) {
	int i;
	if (input->count <= 0) return -1;

	for (i = 0; i < SMELT_RECIPE_COUNT; i++) {
		const struct SmeltRecipe* r = &smeltRecipes[i];
		if (r->isBlock) {
			if (input->block == (BlockID)r->inputId && input->itemId == ITEM_NONE)
				return i;
		} else {
			if (input->itemId == r->inputId && input->block == BLOCK_AIR)
				return i;
		}
	}
	return -1;
}

static cc_bool Furnace_CanOutputTo(struct SurvInvSlot* output, int recipeIdx) {
	const struct SmeltRecipe* r = &smeltRecipes[recipeIdx];
	if (output->count <= 0) return true;

	if (r->outIsBlock) {
		if (output->block != r->outBlock || output->itemId != ITEM_NONE)
			return false;
	} else {
		if (output->itemId != r->outItem || output->block != BLOCK_AIR)
			return false;
	}
	/* Respect per-item/block max stack size */
	{
		int maxStack;
		if (r->outIsBlock)
			maxStack = Block_MaxStackSize(r->outBlock);
		else
			maxStack = Item_MaxStackSize(r->outItem);
		return output->count + r->outCount <= maxStack;
	}
}

static void Furnace_RefreshBlockAt(int x, int y, int z) {
	if (x >= 0 && y >= 0 && z >= 0) {
		MapRenderer_OnBlockChanged(x, y, z, BLOCK_FURNACE);
	}
}

/* Find furnace index by world position, returns -1 if not found */
static int Furnace_FindByPos(int x, int y, int z) {
	int i;
	for (i = 0; i < Furnace_Count; i++) {
		if (Furnaces[i].x == x && Furnaces[i].y == y && Furnaces[i].z == z)
			return i;
	}
	return -1;
}

/* Find or create a furnace entry at the given position */
static int Furnace_GetOrCreate(int x, int y, int z) {
	int idx = Furnace_FindByPos(x, y, z);
	if (idx >= 0) return idx;

	if (Furnace_Count >= MAX_FURNACES) return -1;
	idx = Furnace_Count++;
	Mem_Set(&Furnaces[idx], 0, sizeof(struct FurnaceData));
	Furnaces[idx].x = x;
	Furnaces[idx].y = y;
	Furnaces[idx].z = z;
	return idx;
}

/* Copy furnace array data -> globals (for screen to read) */
static void Furnace_SyncToGlobals(int idx) {
	struct FurnaceData* f = &Furnaces[idx];
	Furnace_Input         = f->input;
	Furnace_Fuel          = f->fuel;
	Furnace_Output        = f->output;
	Furnace_SmeltProgress = f->smeltProgress;
	Furnace_Active        = f->active;
	Furnace_FuelBurnLeft  = f->fuelBurnLeft;
	Furnace_FuelBurnTotal = f->fuelBurnTotal;
}

/* Copy globals -> furnace array data */
static void Furnace_SyncFromGlobals(int idx) {
	struct FurnaceData* f = &Furnaces[idx];
	f->input         = Furnace_Input;
	f->fuel          = Furnace_Fuel;
	f->output        = Furnace_Output;
	f->smeltProgress = Furnace_SmeltProgress;
	f->active        = Furnace_Active;
	f->fuelBurnLeft  = Furnace_FuelBurnLeft;
	f->fuelBurnTotal = Furnace_FuelBurnTotal;
}

/* Tick a single furnace instance. Returns true if an item was produced. */
static cc_bool Furnace_TickOne(struct FurnaceData* f, float delta) {
	int recipeIdx;
	const struct SmeltRecipe* r;
	float fuelTime;
	cc_bool wasActive = f->active;

	recipeIdx = Furnace_FindRecipeFor(&f->input);

	/* If no valid recipe, stop smelting but let fuel keep burning */
	if (recipeIdx < 0 || !Furnace_CanOutputTo(&f->output, recipeIdx)) {
		if (f->fuelBurnLeft > 0.0f) {
			f->fuelBurnLeft -= delta;
			if (f->fuelBurnLeft <= 0.0f) {
				f->fuelBurnLeft = 0.0f;
				f->fuelBurnTotal = 0.0f;
			}
			f->active = (f->fuelBurnLeft > 0.0f);
		} else {
			f->active = false;
		}
		f->smeltProgress = 0.0f;
		if (f->active != wasActive) Furnace_RefreshBlockAt(f->x, f->y, f->z);
		return false;
	}

	/* If no fuel is currently burning, try to consume a new fuel item */
	if (f->fuelBurnLeft <= 0.0f) {
		fuelTime = Furnace_GetFuelTime(&f->fuel);
		if (fuelTime <= 0.0f) {
			f->active = false;
			f->smeltProgress = 0.0f;
			f->fuelBurnTotal = 0.0f;
			if (f->active != wasActive) Furnace_RefreshBlockAt(f->x, f->y, f->z);
			return false;
		}

		/* Consume 1 fuel item */
		f->fuel.count--;
		if (f->fuel.count <= 0) {
			f->fuel.block  = BLOCK_AIR;
			f->fuel.itemId = ITEM_NONE;
			f->fuel.count  = 0;
		}
		f->fuelBurnLeft  = fuelTime;
		f->fuelBurnTotal = fuelTime;
	}

	f->active = true;
	f->fuelBurnLeft -= delta;
	if (f->fuelBurnLeft <= 0.0f) {
		f->fuelBurnLeft = 0.0f;
		f->fuelBurnTotal = 0.0f;
	}

	f->smeltProgress += delta / SMELT_TIME;

	if (f->smeltProgress >= 1.0f) {
		f->smeltProgress = 0.0f;
		r = &smeltRecipes[recipeIdx];

		/* Consume 1 input */
		f->input.count--;
		if (f->input.count <= 0) {
			f->input.block  = BLOCK_AIR;
			f->input.itemId = ITEM_NONE;
			f->input.count  = 0;
		}

		/* Produce output */
		if (f->output.count <= 0) {
			if (r->outIsBlock) {
				f->output.block  = r->outBlock;
				f->output.itemId = ITEM_NONE;
			} else {
				f->output.block  = BLOCK_AIR;
				f->output.itemId = r->outItem;
			}
			f->output.count = r->outCount;
		} else {
			f->output.count += r->outCount;
		}
		if (f->active != wasActive) Furnace_RefreshBlockAt(f->x, f->y, f->z);
		return true;
	}
	if (f->active != wasActive) Furnace_RefreshBlockAt(f->x, f->y, f->z);
	return false;
}

void Furnace_Open(int x, int y, int z) {
	int idx = Furnace_GetOrCreate(x, y, z);
	if (idx < 0) return;
	Furnace_ViewIdx = idx;
	Furnace_SyncToGlobals(idx);
}

void Furnace_Close(void) {
	if (Furnace_ViewIdx >= 0 && Furnace_ViewIdx < Furnace_Count) {
		Furnace_SyncFromGlobals(Furnace_ViewIdx);
	}
	Furnace_ViewIdx = -1;
}

void Furnace_TickAll(float delta) {
	int i;

	/* If GUI is open, push screen edits into array first */
	if (Furnace_ViewIdx >= 0 && Furnace_ViewIdx < Furnace_Count) {
		Furnace_SyncFromGlobals(Furnace_ViewIdx);
	}

	for (i = 0; i < Furnace_Count; i++) {
		Furnace_TickOne(&Furnaces[i], delta);
	}

	/* Pull updated state back to globals for screen display */
	if (Furnace_ViewIdx >= 0 && Furnace_ViewIdx < Furnace_Count) {
		Furnace_SyncToGlobals(Furnace_ViewIdx);
	}
}

cc_bool Furnace_IsActiveAt(int x, int y, int z) {
	int idx = Furnace_FindByPos(x, y, z);
	if (idx < 0) return false;
	return Furnaces[idx].active;
}

void Furnace_Remove(int x, int y, int z, struct FurnaceData* out) {
	int idx = Furnace_FindByPos(x, y, z);
	if (idx < 0) {
		if (out) Mem_Set(out, 0, sizeof(struct FurnaceData));
		return;
	}
	if (out) *out = Furnaces[idx];

	/* If removing the viewed furnace, close it */
	if (Furnace_ViewIdx == idx) Furnace_ViewIdx = -1;
	/* Shift last element into the gap */
	Furnace_Count--;
	if (idx < Furnace_Count) {
		Furnaces[idx] = Furnaces[Furnace_Count];
		/* Fix view index if it pointed to the moved element */
		if (Furnace_ViewIdx == Furnace_Count) Furnace_ViewIdx = idx;
	}
}

/* ---- container.dat save/load ---- */
static void Container_WriteSlot(struct Stream* s, struct SurvInvSlot* slot) {
	cc_uint8 buf[10];
	Stream_SetU16_BE(&buf[0], slot->block);
	Stream_SetU32_BE(&buf[2], slot->itemId);
	Stream_SetU32_BE(&buf[6], slot->count);
	Stream_Write(s, buf, 10);
}

static void Container_ReadSlot(struct Stream* s, struct SurvInvSlot* slot) {
	cc_uint8 buf[10];
	if (Stream_Read(s, buf, 10)) { Mem_Set(slot, 0, sizeof(*slot)); return; }
	slot->block  = Stream_GetU16_BE(&buf[0]);
	slot->itemId = Stream_GetU32_BE(&buf[2]);
	slot->count  = Stream_GetU32_BE(&buf[6]);
}


/*########################################################################################################################*
*-----------------------------------------------------Chest Storage------------------------------------------------------*
*#########################################################################################################################*/
struct ChestData Chests[MAX_CHESTS];
int Chest_Count;
int Chest_ViewIdx  = -1;
int Chest_ViewIdx2 = -1;


/*########################################################################################################################*
*-----------------------------------------------------Sign Storage-------------------------------------------------------*
*#########################################################################################################################*/
struct SignData Signs[MAX_SIGNS];
int Sign_Count = 0;

int Sign_FindAt(int x, int y, int z) {
	int i;
	for (i = 0; i < Sign_Count; i++) {
		if (Signs[i].x == x && Signs[i].y == y && Signs[i].z == z)
			return i;
	}
	return -1;
}

void Sign_AddAt(int x, int y, int z) {
	if (Sign_FindAt(x, y, z) >= 0) return;
	if (Sign_Count >= MAX_SIGNS) return;
	Mem_Set(&Signs[Sign_Count], 0, sizeof(struct SignData));
	Signs[Sign_Count].x = x;
	Signs[Sign_Count].y = y;
	Signs[Sign_Count].z = z;
	Sign_Count++;
}

void Sign_RemoveAt(int x, int y, int z) {
	int idx = Sign_FindAt(x, y, z);
	if (idx < 0) return;
	Sign_Count--;
	if (idx < Sign_Count)
		Signs[idx] = Signs[Sign_Count];
}

struct SurvInvSlot Chest_Slots[DCHEST_SLOTS];
int Chest_SlotCount;

/* Find chest index by world position, returns -1 if not found */
static int Chest_FindByPos(int x, int y, int z) {
	int i;
	for (i = 0; i < Chest_Count; i++) {
		if (Chests[i].x == x && Chests[i].y == y && Chests[i].z == z)
			return i;
	}
	return -1;
}

/* Find or create a chest entry at the given position */
static int Chest_GetOrCreate(int x, int y, int z) {
	int idx = Chest_FindByPos(x, y, z);
	if (idx >= 0) return idx;

	if (Chest_Count >= MAX_CHESTS) return -1;
	idx = Chest_Count++;
	Mem_Set(&Chests[idx], 0, sizeof(struct ChestData));
	Chests[idx].x = x;
	Chests[idx].y = y;
	Chests[idx].z = z;
	return idx;
}

/* Copy chest array data -> globals (for screen to read) */
static void Chest_SyncToGlobals(int idx, int idx2) {
	int i;
	struct ChestData* c = &Chests[idx];
	for (i = 0; i < CHEST_SLOTS; i++) {
		Chest_Slots[i] = c->slots[i];
	}
	if (idx2 >= 0) {
		struct ChestData* c2 = &Chests[idx2];
		for (i = 0; i < CHEST_SLOTS; i++) {
			Chest_Slots[CHEST_SLOTS + i] = c2->slots[i];
		}
		Chest_SlotCount = DCHEST_SLOTS;
	} else {
		Chest_SlotCount = CHEST_SLOTS;
	}
}

/* Copy globals -> chest array data */
static void Chest_SyncFromGlobals(int idx, int idx2) {
	int i;
	struct ChestData* c = &Chests[idx];
	for (i = 0; i < CHEST_SLOTS; i++) {
		c->slots[i] = Chest_Slots[i];
	}
	if (idx2 >= 0) {
		struct ChestData* c2 = &Chests[idx2];
		for (i = 0; i < CHEST_SLOTS; i++) {
			c2->slots[i] = Chest_Slots[CHEST_SLOTS + i];
		}
	}
}

cc_bool Chest_GetPartnerPos(BlockID block, int x, int y, int z, int* px, int* py, int* pz) {
	*py = y;
	switch (block) {
		case BLOCK_DCHEST_S_L: *px = x - 1; *pz = z;     return true; /* +X is L, partner is -X */
		case BLOCK_DCHEST_S_R: *px = x + 1; *pz = z;     return true; /* -X is R, partner is +X */
		case BLOCK_DCHEST_N_L: *px = x + 1; *pz = z;     return true; /* -X is L, partner is +X */
		case BLOCK_DCHEST_N_R: *px = x - 1; *pz = z;     return true; /* +X is R, partner is -X */
		case BLOCK_DCHEST_E_L: *px = x;     *pz = z + 1;  return true; /* -Z is L, partner is +Z */
		case BLOCK_DCHEST_E_R: *px = x;     *pz = z - 1;  return true; /* +Z is R, partner is -Z */
		case BLOCK_DCHEST_W_L: *px = x;     *pz = z - 1;  return true; /* +Z is L, partner is -Z */
		case BLOCK_DCHEST_W_R: *px = x;     *pz = z + 1;  return true; /* -Z is R, partner is +Z */
		default: return false;
	}
}

void Chest_Open(int x, int y, int z) {
	BlockID block;
	int idx, idx2 = -1;
	int px, py, pz;

	idx = Chest_GetOrCreate(x, y, z);
	if (idx < 0) return;

	/* Check if this is a double chest */
	block = World_GetBlock(x, y, z);
	if (Chest_GetPartnerPos(block, x, y, z, &px, &py, &pz)) {
		idx2 = Chest_GetOrCreate(px, py, pz);

		/* If we clicked on the right half, swap so the left half is
		   always shown on top for consistent item ordering. */
		if (block == BLOCK_DCHEST_S_R || block == BLOCK_DCHEST_N_R ||
			block == BLOCK_DCHEST_E_R || block == BLOCK_DCHEST_W_R) {
			int tmp = idx; idx = idx2; idx2 = tmp;
		}
	}

	Chest_ViewIdx  = idx;
	Chest_ViewIdx2 = idx2;
	Chest_SyncToGlobals(idx, idx2);
}

void Chest_Close(void) {
	if (Chest_ViewIdx >= 0 && Chest_ViewIdx < Chest_Count) {
		Chest_SyncFromGlobals(Chest_ViewIdx, Chest_ViewIdx2);
	}
	Chest_ViewIdx  = -1;
	Chest_ViewIdx2 = -1;
}

void Chest_Remove(int x, int y, int z, struct ChestData* out) {
	int idx = Chest_FindByPos(x, y, z);
	if (idx < 0) {
		if (out) Mem_Set(out, 0, sizeof(struct ChestData));
		return;
	}
	if (out) *out = Chests[idx];

	/* If removing the viewed chest, close it */
	if (Chest_ViewIdx == idx) Chest_ViewIdx = -1;
	if (Chest_ViewIdx2 == idx) Chest_ViewIdx2 = -1;
	/* Shift last element into the gap */
	Chest_Count--;
	if (idx < Chest_Count) {
		Chests[idx] = Chests[Chest_Count];
		/* Fix view indices if they pointed to the moved element */
		if (Chest_ViewIdx == Chest_Count) Chest_ViewIdx = idx;
		if (Chest_ViewIdx2 == Chest_Count) Chest_ViewIdx2 = idx;
	}
}

void Container_SaveToFile(const cc_string* path) {
	struct Stream s;
	cc_result res;
	cc_uint8 buf[25];
	int i;

	res = Stream_CreateFile(&s, path);
	if (res) { Logger_SysWarn(res, "creating container.dat"); return; }

	/* Write furnace count */
	Stream_SetU32_BE(buf, Furnace_Count);
	Stream_Write(&s, buf, 4);

	for (i = 0; i < Furnace_Count; i++) {
		struct FurnaceData* f = &Furnaces[i];
		/* Position */
		Stream_SetU32_BE(&buf[0], f->x);
		Stream_SetU32_BE(&buf[4], f->y);
		Stream_SetU32_BE(&buf[8], f->z);
		Stream_Write(&s, buf, 12);
		/* Slots */
		Container_WriteSlot(&s, &f->input);
		Container_WriteSlot(&s, &f->fuel);
		Container_WriteSlot(&s, &f->output);
		/* State */
		Mem_Copy(&buf[0], &f->smeltProgress, 4);
		Mem_Copy(&buf[4], &f->fuelBurnLeft,  4);
		Mem_Copy(&buf[8], &f->fuelBurnTotal, 4);
		buf[12] = f->active ? 1 : 0;
		Stream_Write(&s, buf, 13);
	}

	/* Write chest count */
	Stream_SetU32_BE(buf, Chest_Count);
	Stream_Write(&s, buf, 4);

	for (i = 0; i < Chest_Count; i++) {
		struct ChestData* c = &Chests[i];
		int j;
		/* Position */
		Stream_SetU32_BE(&buf[0], c->x);
		Stream_SetU32_BE(&buf[4], c->y);
		Stream_SetU32_BE(&buf[8], c->z);
		Stream_Write(&s, buf, 12);
		/* Slots */
		for (j = 0; j < CHEST_SLOTS; j++) {
			Container_WriteSlot(&s, &c->slots[j]);
		}
	}

	/* Write sign count */
	Stream_SetU32_BE(buf, Sign_Count);
	Stream_Write(&s, buf, 4);

	for (i = 0; i < Sign_Count; i++) {
		struct SignData* sg = &Signs[i];
		/* Position */
		Stream_SetU32_BE(&buf[0], sg->x);
		Stream_SetU32_BE(&buf[4], sg->y);
		Stream_SetU32_BE(&buf[8], sg->z);
		Stream_Write(&s, buf, 12);
		/* Text lines: 4 x 16 bytes each */
		Stream_Write(&s, sg->lines[0], 16);
		Stream_Write(&s, sg->lines[1], 16);
		Stream_Write(&s, sg->lines[2], 16);
		Stream_Write(&s, sg->lines[3], 16);
		/* Rotation: 1 byte (0-15 for floor signs, 0 for wall signs) */
		buf[0] = sg->rotation;
		Stream_Write(&s, buf, 1);
	}

	res = s.Close(&s);
	if (res) Logger_SysWarn(res, "closing container.dat");
}

void Container_LoadFromFile(const cc_string* path) {
	struct Stream s;
	cc_result res;
	cc_uint8 buf[25];
	int i, count;

	res = Stream_OpenFile(&s, path);
	if (res == ReturnCode_FileNotFound) return;
	if (res) { Logger_SysWarn(res, "opening container.dat"); return; }

	/* Read furnace count */
	if (Stream_Read(&s, buf, 4)) { s.Close(&s); return; }
	count = Stream_GetU32_BE(buf);
	if (count > MAX_FURNACES) count = MAX_FURNACES;

	Furnace_Count = count;
	for (i = 0; i < count; i++) {
		struct FurnaceData* f = &Furnaces[i];
		/* Position */
		if (Stream_Read(&s, buf, 12)) break;
		f->x = Stream_GetU32_BE(&buf[0]);
		f->y = Stream_GetU32_BE(&buf[4]);
		f->z = Stream_GetU32_BE(&buf[8]);
		/* Slots */
		Container_ReadSlot(&s, &f->input);
		Container_ReadSlot(&s, &f->fuel);
		Container_ReadSlot(&s, &f->output);
		/* State */
		if (Stream_Read(&s, buf, 13)) break;
		Mem_Copy(&f->smeltProgress, &buf[0], 4);
		Mem_Copy(&f->fuelBurnLeft,  &buf[4], 4);
		Mem_Copy(&f->fuelBurnTotal, &buf[8], 4);
		f->active = buf[12] ? true : false;
	}

	/* Read chest count */
	if (!Stream_Read(&s, buf, 4)) {
		count = Stream_GetU32_BE(buf);
		if (count > MAX_CHESTS) count = MAX_CHESTS;

		Chest_Count = count;
		for (i = 0; i < count; i++) {
			struct ChestData* c = &Chests[i];
			int j;
			/* Position */
			if (Stream_Read(&s, buf, 12)) break;
			c->x = Stream_GetU32_BE(&buf[0]);
			c->y = Stream_GetU32_BE(&buf[4]);
			c->z = Stream_GetU32_BE(&buf[8]);
			/* Slots */
			for (j = 0; j < CHEST_SLOTS; j++) {
				Container_ReadSlot(&s, &c->slots[j]);
			}
		}
	}

	/* Read sign count */
	if (!Stream_Read(&s, buf, 4)) {
		count = Stream_GetU32_BE(buf);
		if (count > MAX_SIGNS) count = MAX_SIGNS;

		Sign_Count = count;
		for (i = 0; i < count; i++) {
			struct SignData* sg = &Signs[i];
			/* Position */
			if (Stream_Read(&s, buf, 12)) break;
			sg->x = Stream_GetU32_BE(&buf[0]);
			sg->y = Stream_GetU32_BE(&buf[4]);
			sg->z = Stream_GetU32_BE(&buf[8]);
			/* Text lines */
			if (Stream_Read(&s, sg->lines[0], 16)) break;
			if (Stream_Read(&s, sg->lines[1], 16)) break;
			if (Stream_Read(&s, sg->lines[2], 16)) break;
			if (Stream_Read(&s, sg->lines[3], 16)) break;
			/* Ensure null-terminated */
			sg->lines[0][15] = 0; sg->lines[1][15] = 0;
			sg->lines[2][15] = 0; sg->lines[3][15] = 0;
			/* Rotation: 1 byte (0-15 for floor signs, 0 for wall signs) */
			if (Stream_Read(&s, buf, 1)) { sg->rotation = 0; break; }
			sg->rotation = buf[0] & 0x0F;
		}
	}

	s.Close(&s);
}


/*########################################################################################################################*
*--------------------------------------------------Inventory component----------------------------------------------------*
*#########################################################################################################################*/
static void OnReset(void) {
	Inventory_ResetMapping();

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
	
	/* Remove pressed stone pressure plate from inventory (auto-placed by physics) */
	Inventory_Remove(BLOCK_STONE_PLATE_PRESSED);
	
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

	/* Remove farmland from inventory (created by hoeing dirt/grass) */
	Inventory_Remove(BLOCK_FARMLAND_DRY);
	Inventory_Remove(BLOCK_FARMLAND_WET);

	/* Remove wheat stages from inventory (grown on farmland) */
	{ int i; for (i = BLOCK_WHEAT_0; i <= BLOCK_WHEAT_7; i++) Inventory_Remove(i); }

	/* Add sign blocks to inventory (IDs above CPE range) */
	Inventory_AddDefault(BLOCK_SIGN_WALL);
	Inventory_AddDefault(BLOCK_SIGN_FLOOR);

	/* Add portal to inventory */
	Inventory_AddDefault(BLOCK_PORTAL);

	/* Add survival-mode blocks to inventory (IDs above CPE range) */
	Inventory_AddDefault(BLOCK_RAIL);

	Inventory.CanChangeSelected = true;
	Mem_Set(HotbarItems, 0, sizeof(HotbarItems));
}

static void OnInit(void) {
	int i;
	BlockID* inv = Inventory.Table;
	OnReset();
	Inventory.BlocksPerRow = Game_Version.BlocksPerRow;
	
	/* In survival mode, start with empty hotbar */
	if (!Game_SurvivalMode) {
		for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR; i++) {
			inv[i] = Game_Version.Hotbar[i];
		}
	}
}

struct IGameComponent Inventory_Component = {
	OnInit,  /* Init  */
	NULL,    /* Free  */
	OnReset, /* Reset */
};
