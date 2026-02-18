#include "Inventory.h"
#include "Funcs.h"
#include "Game.h"
#include "Block.h"
#include "Event.h"
#include "Chat.h"
#include "Protocol.h"
#include "Platform.h"

struct _InventoryData Inventory;

/*########################################################################################################################*
*------------------------------------------------------Item data----------------------------------------------------------*
*#########################################################################################################################*/
int HotbarItems[INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR];

const char* const ItemNames[ITEM_COUNT] = {
	"Air",
	"Leather Helmet", "Leather Chestplate", "Leather Pants", "Leather Boots",
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
	"Gunpowder", "Feather", "String"
};

const int ItemTextures[ITEM_COUNT] = {
	-1,                       /* 0:  Air */
	0, 16, 32, 48,            /* 1-4:   Leather armor */
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
	40, 24, 8                 /* 54-56: Gunpowder, Feather, String */
};

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
*--------------------------------------------------Inventory component----------------------------------------------------*
*#########################################################################################################################*/
static void OnReset(void) {
	Inventory_ResetMapping();
	Inventory.CanChangeSelected = true;
	Mem_Set(HotbarItems, 0, sizeof(HotbarItems));
}

static void OnInit(void) {
	int i;
	BlockID* inv = Inventory.Table;
	OnReset();
	Inventory.BlocksPerRow = Game_Version.BlocksPerRow;
	
	for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR; i++) {
		inv[i] = Game_Version.Hotbar[i];
	}
}

struct IGameComponent Inventory_Component = {
	OnInit,  /* Init  */
	NULL,    /* Free  */
	OnReset, /* Reset */
};
