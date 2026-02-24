#ifndef CC_INVENTORY_H
#define CC_INVENTORY_H
#include "Core.h"
#include "BlockID.h"
CC_BEGIN_HEADER


/* Manages inventory hotbar, and ordering of blocks in the inventory menu.
   Copyright 2014-2025 ClassiCube | Licensed under BSD-3
*/
struct IGameComponent;
extern struct IGameComponent Inventory_Component;

/* Number of blocks in each hotbar */
#define INVENTORY_BLOCKS_PER_HOTBAR 9
/* Number of hotbars that can be selected between */
#define INVENTORY_HOTBARS 9
#define HOTBAR_MAX_INDEX (INVENTORY_BLOCKS_PER_HOTBAR - 1)

CC_VAR extern struct _InventoryData {
	/* Stores the currently bound blocks for all hotbars. */
	BlockID Table[INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR];
	/* Mapping of indices in inventory menu to block IDs. */
	BlockID Map[BLOCK_COUNT];
	/* Currently selected index within a hotbar. */
	int SelectedIndex;
	/* Currently selected hotbar. */
	int Offset;
	/* Whether the user is allowed to change selected/held block. */
	cc_bool CanChangeSelected;
	/* Number of blocks in each row in inventory menu. */
	cc_uint8 BlocksPerRow;
} Inventory;

/* Gets the block at the nth index in the current hotbar. */
#define Inventory_Get(idx) (Inventory.Table[Inventory.Offset + (idx)])
/* Sets the block at the nth index in the current hotbar. */
#define Inventory_Set(idx, block) Inventory.Table[Inventory.Offset + (idx)] = block
/* Gets the currently selected block. */
#define Inventory_SelectedBlock Inventory_Get(Inventory.SelectedIndex)

/* Checks if the user can change their selected/held block. */
/* NOTE: Shows a message in chat if they are unable to. */
cc_bool Inventory_CheckChangeSelected(void);
/* Attempts to set the currently selected index in a hotbar. */
void Inventory_SetSelectedIndex(int index);
/* Attempts to set the currently active hotbar. */
void Inventory_SetHotbarIndex(int index);
void Inventory_SwitchHotbar(void);
/* Attempts to set the block for the selected index in the current hotbar. */
/* NOTE: If another slot is already this block, the selected index is instead changed. */
void Inventory_SetSelectedBlock(BlockID block);
/* Attempts to set the selected block in a user-friendly manner. */
/* e.g. this method tries to replace empty slots before other blocks */
void Inventory_PickBlock(BlockID block);
/* Sets all slots to contain their default associated block. */
/* NOTE: The order of default blocks may not be in order of ID. */
void Inventory_ResetMapping(void);

/* Inserts the given block at its default slot in the inventory. */
/* NOTE: Replaces (doesn't move) the block that was at that slot before. */
void Inventory_AddDefault(BlockID block);
/* Removes any slots with the given block from the inventory. */
void Inventory_Remove(BlockID block);

/* =========================== Item system =========================== */
#define ITEM_COUNT 57
#define ITEM_NONE 0

/* Named item IDs */
#define ITEM_WOOD_SWORD    5
#define ITEM_WOOD_SHOVEL   6
#define ITEM_WOOD_PICKAXE  7
#define ITEM_WOOD_AXE      8
#define ITEM_STONE_SWORD   13
#define ITEM_STONE_SHOVEL  14
#define ITEM_STONE_PICKAXE 15
#define ITEM_STONE_AXE     16
#define ITEM_IRON_SWORD    21
#define ITEM_IRON_SHOVEL   22
#define ITEM_IRON_PICKAXE  23
#define ITEM_IRON_AXE      24
#define ITEM_DIAMOND_SWORD 29
#define ITEM_DIAMOND_SHOVEL 30
#define ITEM_DIAMOND_PICKAXE 31
#define ITEM_DIAMOND_AXE   32
#define ITEM_GOLD_SWORD    37
#define ITEM_GOLD_SHOVEL   38
#define ITEM_GOLD_PICKAXE  39
#define ITEM_GOLD_AXE      40
#define ITEM_BOW           41
#define ITEM_ARROW         42
#define ITEM_STICK         43
#define ITEM_FLINT_STEEL   44
#define ITEM_FLINT         45
#define ITEM_COAL          46
#define ITEM_IRON_INGOT    47
#define ITEM_GOLD_INGOT    48
#define ITEM_DIAMOND_ITEM  49
#define ITEM_BOWL          50
#define ITEM_MUSHROOM_STEW 51
#define ITEM_RAW_PORK      52
#define ITEM_COOKED_PORK   53
#define ITEM_SULPHUR       54
#define ITEM_FEATHER       55
#define ITEM_STRING        56

/* Item names indexed by item ID (0 = Air) */
extern const char* const ItemNames[ITEM_COUNT];
/* items.png tile indices indexed by item ID (-1 = no texture) */
extern const int ItemTextures[ITEM_COUNT];
/* Melee damage dealt by each item (0 = use bare hand damage) */
extern const int ItemDamage[ITEM_COUNT];
/* Bare hand melee damage */
#define ITEM_BARE_HAND_DAMAGE 1
/* Returns the maximum stack size for a given item ID (1 for tools/armor, 64 for most items) */
int Item_MaxStackSize(int itemId);
/* Returns the maximum stack size for a given block (1 for doors, 64 for most blocks) */
int Block_MaxStackSize(BlockID block);

/* Item stored in each hotbar slot (ITEM_NONE = no item, just a block) */
extern int HotbarItems[INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR];
/* Gets the item ID at the nth index in the current hotbar. */
#define Hotbar_GetItem(idx) HotbarItems[Inventory.Offset + (idx)]
/* Sets the item ID at the nth index in the current hotbar. */
#define Hotbar_SetItem(idx, id) HotbarItems[Inventory.Offset + (idx)] = id
/* Gets the item ID of the currently selected hotbar slot. */
#define Hotbar_SelectedItem Hotbar_GetItem(Inventory.SelectedIndex)

/* =========================== Survival Inventory =========================== */
struct SurvInvSlot {
	BlockID block;   /* BLOCK_AIR if empty or holding an item */
	int     itemId;  /* ITEM_NONE if empty or holding a block */
	int     count;   /* 0 = empty, 1-64 = stack count */
};

extern struct SurvInvSlot SurvInv_Main[27];   /* 3x9 main grid */
extern struct SurvInvSlot SurvInv_Armor[4];   /* 4 armor slots */
extern struct SurvInvSlot SurvInv_Craft[4];   /* 2x2 crafting */
extern struct SurvInvSlot SurvInv_Output;     /* 1 output */

/* Stack counts for hotbar slots */
extern int HotbarCounts[INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR];
#define Hotbar_GetCount(idx) HotbarCounts[Inventory.Offset + (idx)]
#define Hotbar_SetCount(idx, cnt) HotbarCounts[Inventory.Offset + (idx)] = (cnt)
#define Hotbar_SelectedCount Hotbar_GetCount(Inventory.SelectedIndex)
#define MAX_STACK_SIZE 64

/* Saves inventory state (hotbar, items, counts, survival inventory) to file */
void Inventory_SaveToFile(const cc_string* path);
/* Loads inventory state from file (silently returns if file doesn't exist) */
void Inventory_LoadFromFile(const cc_string* path);

/* =========================== Crafting System =========================== */

/* Crafting table 3x3 grid (used when crafting table screen is open) */
extern struct SurvInvSlot CraftTable_Grid[9];
extern struct SurvInvSlot CraftTable_Output;

/* Tagged cell values for recipe matching */
#define CRAFT_EMPTY       0
#define CRAFT_BLOCK_TAG   0x10000
#define CRAFT_ITEM_TAG    0x20000
#define CB(b)  (CRAFT_BLOCK_TAG | (b))
#define CI(i)  (CRAFT_ITEM_TAG  | (i))

struct CraftRecipe {
	cc_uint8 width, height;
	int pattern[9];       /* row-major, max 3x3 */
	BlockID  outBlock;    /* BLOCK_AIR if output is item */
	int      outItem;     /* ITEM_NONE if output is block */
	int      outCount;
};

/* Updates the crafting output based on current grid contents */
void Crafting_UpdateOutput2x2(void);
void Crafting_UpdateOutput3x3(void);
/* Consumes crafting materials when output is taken */
void Crafting_TakeOutput2x2(void);
void Crafting_TakeOutput3x3(void);

/* =========================== Furnace Smelting =========================== */

#define MAX_FURNACES 64

/* Per-furnace instance data */
struct FurnaceData {
	int x, y, z;               /* world position */
	struct SurvInvSlot input;
	struct SurvInvSlot fuel;
	struct SurvInvSlot output;
	float smeltProgress;
	float fuelBurnLeft;
	float fuelBurnTotal;
	cc_bool active;
};

extern struct FurnaceData Furnaces[MAX_FURNACES];
extern int Furnace_Count;

/* "Currently open" furnace state - the screen reads/writes these globals.
   They mirror Furnaces[Furnace_ViewIdx] while the furnace GUI is open. */
extern struct SurvInvSlot Furnace_Input;
extern struct SurvInvSlot Furnace_Fuel;
extern struct SurvInvSlot Furnace_Output;
extern float Furnace_SmeltProgress;
extern cc_bool Furnace_Active;
extern float Furnace_FuelBurnLeft;
extern float Furnace_FuelBurnTotal;

/* Index into Furnaces[] of the furnace currently shown in the GUI, -1 if none */
extern int Furnace_ViewIdx;

/* Opens a furnace at the given world position (copies data to globals) */
void Furnace_Open(int x, int y, int z);
/* Closes the currently open furnace (copies globals back to array) */
void Furnace_Close(void);
/* Ticks ALL furnaces. Syncs the viewed furnace's globals around the tick. */
void Furnace_TickAll(float delta);
/* Returns true if the furnace at (x,y,z) is actively smelting */
cc_bool Furnace_IsActiveAt(int x, int y, int z);
/* Removes a furnace at the given world position. Returns its data in *out (or zeros). */
void Furnace_Remove(int x, int y, int z, struct FurnaceData* out);

/* =========================== Chest Storage =========================== */

#define MAX_CHESTS 64
#define CHEST_SLOTS 27        /* 3x9 single chest */
#define DCHEST_SLOTS 54       /* 6x9 double chest */

/* Per-chest instance data */
struct ChestData {
	int x, y, z;                        /* world position */
	struct SurvInvSlot slots[CHEST_SLOTS]; /* stored items */
};

extern struct ChestData Chests[MAX_CHESTS];
extern int Chest_Count;

/* Currently open chest state - the screen reads/writes these globals */
extern struct SurvInvSlot Chest_Slots[DCHEST_SLOTS]; /* up to 54 for double */
extern int Chest_SlotCount;  /* 27 for single, 54 for double */

/* Index into Chests[] of the chest currently shown in the GUI, -1 if none */
extern int Chest_ViewIdx;
/* Index of 2nd half for double chest, -1 if single */
extern int Chest_ViewIdx2;

/* Opens a chest at the given world position (copies data to globals) */
void Chest_Open(int x, int y, int z);
/* Closes the currently open chest (copies globals back to array) */
void Chest_Close(void);
/* Removes a chest at the given world position. Returns its data in *out (or zeros). */
void Chest_Remove(int x, int y, int z, struct ChestData* out);
/* Returns the position of the other half of a double chest, given the block type.
   Returns false if the block isn't a double chest variant. */
cc_bool Chest_GetPartnerPos(BlockID block, int x, int y, int z, int* px, int* py, int* pz);

/* Saves all furnace and chest data to container.dat */
void Container_SaveToFile(const cc_string* path);
/* Loads furnace and chest data from container.dat */
void Container_LoadFromFile(const cc_string* path);

CC_END_HEADER
#endif
