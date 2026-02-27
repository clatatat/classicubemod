#include "InputHandler.h"
#include "Input.h"
#include "String_.h"
#include "Event.h"
#include "Funcs.h"
#include "Options.h"
#include "Logger.h"
#include "Platform.h"
#include "Chat.h"
#include "Utils.h"
#include "Server.h"
#include "HeldBlockRenderer.h"
#include "Game.h"
#include "ExtMath.h"
#include "Camera.h"
#include "Inventory.h"
#include "World.h"
#include "Event.h"
#include "Window.h"
#include "Entity.h"
#include "Audio.h"
#include "Screens.h"
#include "Block.h"
#include "Menus.h"
#include "Gui.h"
#include "Protocol.h"
#include "AxisLinesRenderer.h"
#include "Picking.h"
#include "BlockPhysics.h"
#include "Lighting.h"
#include "Commands.h"
#include "Particle.h"
#include "TexturePack.h"
#include "Graphics.h"
#include "Stream.h"
#include "Generator.h"
#include "Model.h"

/* Forward declarations for dropped item functions */
static int DropItem_FindFreeSlot(void);
static int DropItem_EvictOldest(void);
static int DropItem_FindFreeEntity(void);
static void DropItem_Spawn(int slot, Vec3 pos, BlockID block, cc_bool isItem, int itemId);
static void DropItem_TryPickup(int slot);
static void DroppedItem_TickAll(struct ScheduledTask* task);
static void Furnace_ScheduledTick(struct ScheduledTask* task);
static cc_bool Mob_BlockIsSolid(int x, int y, int z);
static cc_bool SurvInv_ConsumeItem(int itemId);

/* Forward declaration for Mob_PlaySound (used by Skeleton_ShootArrow before definition) */
static void Mob_PlaySound(cc_uint8 type, Vec3 mobPos);

/* Forward declaration for DayNightCycle_IsNight (used by Mob_NaturalSpawnTick before definition) */
static cc_bool DayNightCycle_IsNight(void);
static cc_bool DayNightCycle_IsDark(void);

/* Forward declarations for dropped item arrays (defined later, used by BindTriggered_DropBlock) */
#define MAX_DROPPED_ITEMS 96
static float droppedItemVelocityX[MAX_DROPPED_ITEMS];
static float droppedItemVelocityY[MAX_DROPPED_ITEMS];
static float droppedItemVelocityZ[MAX_DROPPED_ITEMS];
static int   droppedItemCount[MAX_DROPPED_ITEMS]; /* how many items this entity represents */
static float droppedItemPickupDelay[MAX_DROPPED_ITEMS]; /* seconds before item can be picked up */

static cc_bool input_buttonsDown[3];
static int input_pickingId = -1;
static float input_deltaAcc;
static float input_fovIndex = -1.0f;
#ifdef CC_BUILD_WEB
static cc_bool suppressEscape;
#endif
enum MouseButton_ { MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE };

/* Forward declaration for block breaking reset */
static void BlockBreaking_Reset(void);

/* Forward declarations for mob RNG (used by BreakBlockNow for random drops) */
static RNGState mob_rng;
static cc_bool mob_rng_inited;


/*########################################################################################################################*
*---------------------------------------------------------Gamepad---------------------------------------------------------*
*#########################################################################################################################*/
static void PlayerInputPad(int port, int axis, struct LocalPlayer* p, float* xMoving, float* zMoving) {
	float x, y, angle;
	if (Gamepad_AxisBehaviour[axis] != AXIS_BEHAVIOUR_MOVEMENT) return;
	
	x = Gamepad_Devices[port].axisX[axis];
	y = Gamepad_Devices[port].axisY[axis];
	
	if (x != 0 || y != 0) {
		angle    = Math_Atan2f(x, y);
		*xMoving = Math_CosF(angle);
		*zMoving = Math_SinF(angle);
	}
}

static void PlayerInputGamepad(struct LocalPlayer* p, float* xMoving, float* zMoving) {
	int port;
	for (port = 0; port < INPUT_MAX_GAMEPADS; port++)
	{
		/* In splitscreen mode, tie a controller to a specific player*/
		if (Game_NumStates > 1 && p->index != port) continue;
		
		PlayerInputPad(port, PAD_AXIS_LEFT,  p, xMoving, zMoving);
		PlayerInputPad(port, PAD_AXIS_RIGHT, p, xMoving, zMoving);
	}
}
static struct LocalPlayerInput gamepadInput = { PlayerInputGamepad };


/*########################################################################################################################*
*---------------------------------------------------------Hotkeys---------------------------------------------------------*
*#########################################################################################################################*/
const cc_uint8 Hotkeys_LWJGL[256] = {
	0, CCKEY_ESCAPE, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', CCKEY_MINUS, CCKEY_EQUALS, CCKEY_BACKSPACE, CCKEY_TAB,
	'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', CCKEY_LBRACKET, CCKEY_RBRACKET, CCKEY_ENTER, CCKEY_LCTRL, 'A', 'S',
	'D', 'F', 'G', 'H', 'J', 'K', 'L', CCKEY_SEMICOLON, CCKEY_QUOTE, CCKEY_TILDE, CCKEY_LSHIFT, CCKEY_BACKSLASH, 'Z', 'X', 'C', 'V',
	'B', 'N', 'M', CCKEY_COMMA, CCKEY_PERIOD, CCKEY_SLASH, CCKEY_RSHIFT, 0, CCKEY_LALT, CCKEY_SPACE, CCKEY_CAPSLOCK, CCKEY_F1, CCKEY_F2, CCKEY_F3, CCKEY_F4, CCKEY_F5,
	CCKEY_F6, CCKEY_F7, CCKEY_F8, CCKEY_F9, CCKEY_F10, CCKEY_NUMLOCK, CCKEY_SCROLLLOCK, CCKEY_KP7, CCKEY_KP8, CCKEY_KP9, CCKEY_KP_MINUS, CCKEY_KP4, CCKEY_KP5, CCKEY_KP6, CCKEY_KP_PLUS, CCKEY_KP1,
	CCKEY_KP2, CCKEY_KP3, CCKEY_KP0, CCKEY_KP_DECIMAL, 0, 0, 0, CCKEY_F11, CCKEY_F12, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, CCKEY_F13, CCKEY_F14, CCKEY_F15, CCKEY_F16, CCKEY_F17, CCKEY_F18, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, CCKEY_KP_PLUS, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, CCKEY_KP_ENTER, CCKEY_RCTRL, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, CCKEY_KP_DIVIDE, 0, 0, CCKEY_RALT, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, CCKEY_PAUSE, 0, CCKEY_HOME, CCKEY_UP, CCKEY_PAGEUP, 0, CCKEY_LEFT, 0, CCKEY_RIGHT, 0, CCKEY_END,
	CCKEY_DOWN, CCKEY_PAGEDOWN, CCKEY_INSERT, CCKEY_DELETE, 0, 0, 0, 0, 0, 0, 0, CCKEY_LWIN, CCKEY_RWIN, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
struct HotkeyData CC_BIG_VAR HotkeysList[HOTKEYS_MAX_COUNT];
struct StringsBuffer CC_BIG_VAR HotkeysText;

static void Hotkeys_QuickSort(int left, int right) {
	struct HotkeyData* keys = HotkeysList; struct HotkeyData key;

	while (left < right) {
		int i = left, j = right;
		cc_uint8 pivot = keys[(i + j) >> 1].mods;

		/* partition the list */
		while (i <= j) {
			while (pivot < keys[i].mods) i++;
			while (pivot > keys[j].mods) j--;
			QuickSort_Swap_Maybe();
		}
		/* recurse into the smaller subset */
		QuickSort_Recurse(Hotkeys_QuickSort)
	}
}

static void Hotkeys_AddNewHotkey(int trigger, cc_uint8 modifiers, const cc_string* text, cc_uint8 flags) {
	struct HotkeyData hKey;
	hKey.trigger = trigger;
	hKey.mods    = modifiers;
	hKey.textIndex = HotkeysText.count;
	hKey.flags   = flags;

	if (HotkeysText.count == HOTKEYS_MAX_COUNT) {
		Chat_AddRaw("&cCannot define more than 256 hotkeys");
		return;
	}

	HotkeysList[HotkeysText.count] = hKey;
	StringsBuffer_Add(&HotkeysText, text);
	/* sort so that hotkeys with largest modifiers are first */
	Hotkeys_QuickSort(0, HotkeysText.count - 1);
}

static void Hotkeys_RemoveText(int index) {
	 struct HotkeyData* hKey = HotkeysList;
	 int i;

	for (i = 0; i < HotkeysText.count; i++, hKey++) {
		if (hKey->textIndex >= index) hKey->textIndex--;
	}
	StringsBuffer_Remove(&HotkeysText, index);
}


void Hotkeys_Add(int trigger, cc_uint8 modifiers, const cc_string* text, cc_uint8 flags) {
	struct HotkeyData* hk = HotkeysList;
	int i;

	for (i = 0; i < HotkeysText.count; i++, hk++) {		
		if (hk->trigger != trigger || hk->mods != modifiers) continue;
		Hotkeys_RemoveText(hk->textIndex);

		hk->flags     = flags;
		hk->textIndex = HotkeysText.count;
		StringsBuffer_Add(&HotkeysText, text);
		return;
	}
	Hotkeys_AddNewHotkey(trigger, modifiers, text, flags);
}

cc_bool Hotkeys_Remove(int trigger, cc_uint8 modifiers) {
	struct HotkeyData* hk = HotkeysList;
	int i, j;

	for (i = 0; i < HotkeysText.count; i++, hk++) {
		if (hk->trigger != trigger || hk->mods != modifiers) continue;
		Hotkeys_RemoveText(hk->textIndex);

		for (j = i; j < HotkeysText.count; j++) {
			HotkeysList[j] = HotkeysList[j + 1];
		}
		return true;
	}
	return false;
}

int Hotkeys_FindPartial(int key) {
	struct HotkeyData hk;
	int i, modifiers = 0;

	if (Input_IsCtrlPressed())  modifiers |= HOTKEY_MOD_CTRL;
	if (Input_IsShiftPressed()) modifiers |= HOTKEY_MOD_SHIFT;
	if (Input_IsAltPressed())   modifiers |= HOTKEY_MOD_ALT;

	for (i = 0; i < HotkeysText.count; i++) {
		hk = HotkeysList[i];
		/* e.g. if holding Ctrl and Shift, a hotkey with only Ctrl modifiers matches */
		if ((hk.mods & modifiers) == hk.mods && hk.trigger == key) return i;
	}
	return -1;
}

static const cc_string prefix = String_FromConst("hotkey-");
static void StoredHotkey_Parse(cc_string* key, cc_string* value) {
	cc_string strKey, strMods, strMore, strText;
	int trigger;
	cc_uint8 modifiers;
	cc_bool more;

	/* Format is: key&modifiers = more-input&text */
	key->length -= prefix.length; key->buffer += prefix.length;
	
	if (!String_UNSAFE_Separate(key,   '&', &strKey,  &strMods)) return;
	if (!String_UNSAFE_Separate(value, '&', &strMore, &strText)) return;
	
	trigger = Utils_ParseEnum(&strKey, INPUT_NONE, Input_StorageNames, INPUT_COUNT);
	if (trigger == INPUT_NONE) return; 
	if (!Convert_ParseUInt8(&strMods, &modifiers)) return;
	if (!Convert_ParseBool(&strMore,  &more))      return;
	
	Hotkeys_Add(trigger, modifiers, &strText, more);
}

static void StoredHotkeys_LoadAll(void) {
	cc_string entry, key, value;
	int i;

	for (i = 0; i < Options.count; i++) {
		StringsBuffer_UNSAFE_GetRaw(&Options, i, &entry);
		String_UNSAFE_Separate(&entry, '=', &key, &value);

		if (!String_CaselessStarts(&key, &prefix)) continue;
		StoredHotkey_Parse(&key, &value);
	}
}

void StoredHotkeys_Load(int trigger, cc_uint8 modifiers) {
	cc_string key, value; char keyBuffer[STRING_SIZE];
	String_InitArray(key, keyBuffer);

	String_Format2(&key, "hotkey-%c&%b", Input_StorageNames[trigger], &modifiers);
	key.buffer[key.length] = '\0'; /* TODO: Avoid this null terminator */

	Options_UNSAFE_Get(key.buffer, &value);
	StoredHotkey_Parse(&key, &value);
}

void StoredHotkeys_Remove(int trigger, cc_uint8 modifiers) {
	cc_string key; char keyBuffer[STRING_SIZE];
	String_InitArray(key, keyBuffer);

	String_Format2(&key, "hotkey-%c&%b", Input_StorageNames[trigger], &modifiers);
	Options_SetString(&key, NULL);
}

void StoredHotkeys_Add(int trigger, cc_uint8 modifiers, cc_bool moreInput, const cc_string* text) {
	cc_string key;   char keyBuffer[STRING_SIZE];
	cc_string value; char valueBuffer[STRING_SIZE * 2];
	String_InitArray(key, keyBuffer);
	String_InitArray(value, valueBuffer);

	String_Format2(&key, "hotkey-%c&%b", Input_StorageNames[trigger], &modifiers);
	String_Format2(&value, "%t&%s", &moreInput, text);
	Options_SetString(&key, &value);
}


/*########################################################################################################################*
*-----------------------------------------------------Mouse helpers-------------------------------------------------------*
*#########################################################################################################################*/
static void MouseStateUpdate(int button, cc_bool pressed) {
	struct Entity* p;
	input_buttonsDown[button] = pressed;
	if (!Server.SupportsPlayerClick) return;

	/* defer getting the targeted entity, as it's a costly operation */
	if (input_pickingId == -1) {
		p = &Entities.CurPlayer->Base;
		input_pickingId = Entities_GetClosest(p);
		
		if (input_pickingId == -1) 
			input_pickingId = ENTITIES_SELF_ID;
	}

	
	CPE_SendPlayerClick(button, pressed, (EntityID)input_pickingId, &Game_SelectedPos);	
}

static void MouseStatePress(int button) {
	input_deltaAcc  = 0;
	input_pickingId = -1;
	MouseStateUpdate(button, true);
}

static void MouseStateRelease(int button) {
	input_pickingId = -1;
	if (!input_buttonsDown[button]) return;
	MouseStateUpdate(button, false);
}

void InputHandler_OnScreensChanged(void) {
	input_deltaAcc  = 0;
	input_pickingId = -1;
	BlockBreaking_Reset();
	if (!Gui.InputGrab) return;

	/* If input is grabbed, then the mouse isn't used for picking blocks in world anymore. */
	/* So release all mouse buttons, since game stops sending PlayerClick during grabbed input */
	MouseStateRelease(MOUSE_LEFT);
	MouseStateRelease(MOUSE_RIGHT);
	MouseStateRelease(MOUSE_MIDDLE);
}

static cc_bool TouchesSolid(BlockID b) { return Blocks.Collide[b] == COLLIDE_SOLID; }
static cc_bool PushbackPlace(struct AABB* blockBB) {
	struct Entity* p        = &Entities.CurPlayer->Base;
	struct HacksComp* hacks = &Entities.CurPlayer->Hacks;
	Face closestFace;
	cc_bool insideMap;

	Vec3 pos = p->Position;
	struct AABB playerBB;
	struct LocationUpdate update;

	/* Offset position by the closest face */
	closestFace = Game_SelectedPos.closest;
	if (closestFace == FACE_XMAX) {
		pos.x = blockBB->Max.x + 0.5f;
	} else if (closestFace == FACE_ZMAX) {
		pos.z = blockBB->Max.z + 0.5f;
	} else if (closestFace == FACE_XMIN) {
		pos.x = blockBB->Min.x - 0.5f;
	} else if (closestFace == FACE_ZMIN) {
		pos.z = blockBB->Min.z - 0.5f;
	} else if (closestFace == FACE_YMAX) {
		pos.y = blockBB->Min.y + 1 + ENTITY_ADJUSTMENT;
	} else if (closestFace == FACE_YMIN) {
		pos.y = blockBB->Min.y - p->Size.y - ENTITY_ADJUSTMENT;
	}

	/* Exclude exact map boundaries, otherwise player can get stuck outside map */
	/* Being vertically above the map is acceptable though */
	insideMap =
		pos.x > 0.0f && pos.y >= 0.0f && pos.z > 0.0f &&
		pos.x < World.Width && pos.z < World.Length;
	if (!insideMap) return false;

	AABB_Make(&playerBB, &pos, &p->Size);
	if (!hacks->Noclip && Entity_TouchesAny(&playerBB, TouchesSolid)) {
		/* Don't put player inside another block */
		return false;
	}

	update.flags = LU_HAS_POS | LU_POS_ABSOLUTE_INSTANT;
	update.pos   = pos;
	p->VTABLE->SetLocation(p, &update);
	return true;
}

static cc_bool IntersectsOthers(Vec3 pos, BlockID block) {
	struct AABB blockBB, entityBB;
	struct Entity* e;
	int id;

	Vec3_Add(&blockBB.Min, &pos, &Blocks.MinBB[block]);
	Vec3_Add(&blockBB.Max, &pos, &Blocks.MaxBB[block]);
	
	for (id = 0; id < ENTITIES_MAX_COUNT; id++)	
	{
		e = Entities.List[id];
		if (!e || e == &Entities.CurPlayer->Base) continue;

		Entity_GetBounds(e, &entityBB);
		entityBB.Min.y += 1.0f / 32.0f; /* when player is exactly standing on top of ground */
		if (AABB_Intersects(&entityBB, &blockBB)) return true;
	}
	return false;
}

static cc_bool CheckIsFree(BlockID block) {
	struct Entity* p        = &Entities.CurPlayer->Base;
	struct HacksComp* hacks = &Entities.CurPlayer->Hacks;

	Vec3 pos, nextPos;
	struct AABB blockBB, playerBB;
	struct LocationUpdate update;

	/* Non solid blocks (e.g. water/flowers) can always be placed on players */
	if (Blocks.Collide[block] != COLLIDE_SOLID) return true;

	IVec3_ToVec3(&pos, &Game_SelectedPos.translatedPos);
	if (IntersectsOthers(pos, block)) return false;
	
	nextPos = p->next.pos;
	Vec3_Add(&blockBB.Min, &pos, &Blocks.MinBB[block]);
	Vec3_Add(&blockBB.Max, &pos, &Blocks.MaxBB[block]);

	/* NOTE: Need to also test against next position here, otherwise player can */
	/* fall through the block at feet as collision is performed against nextPos */
	Entity_GetBounds(p, &playerBB);
	playerBB.Min.y = min(nextPos.y, playerBB.Min.y);

	if (hacks->Noclip || !AABB_Intersects(&playerBB, &blockBB)) return true;
	if (hacks->CanPushbackBlocks && hacks->PushbackPlacing && hacks->Enabled) {
		return PushbackPlace(&blockBB);
	}

	playerBB.Min.y += 0.25f + ENTITY_ADJUSTMENT;
	if (AABB_Intersects(&playerBB, &blockBB)) return false;

	/* Push player upwards when they are jumping and trying to place a block underneath them */
	nextPos.y = pos.y + Blocks.MaxBB[block].y + ENTITY_ADJUSTMENT;

	update.flags = LU_HAS_POS | LU_POS_ABSOLUTE_INSTANT;
	update.pos   = nextPos;
	p->VTABLE->SetLocation(p, &update);
	return true;
}

/* Helper: check if a block is any iron door bottom half */
static cc_bool IsIronDoorBottom(BlockID b) {
	return b == BLOCK_IRON_DOOR || b == BLOCK_IRON_DOOR_EW_BOTTOM
		|| b == BLOCK_IRON_DOOR_NS_OPEN_BOTTOM || b == BLOCK_IRON_DOOR_EW_OPEN_BOTTOM;
}

/* Helper: check if a block is any iron door top half */
static cc_bool IsIronDoorTop(BlockID b) {
	return b == BLOCK_IRON_DOOR_NS_TOP || b == BLOCK_IRON_DOOR_EW_TOP
		|| b == BLOCK_IRON_DOOR_NS_OPEN_TOP || b == BLOCK_IRON_DOOR_EW_OPEN_TOP;
}

/*########################################################################################################################*
*---------------------------------------------------Tool/Breaking system------------------------------------------------*
*#########################################################################################################################*/
enum ToolType  { TOOL_NONE = 0, TOOL_SWORD = 1, TOOL_SHOVEL = 2, TOOL_PICKAXE = 3, TOOL_AXE = 4 };
enum ToolTier  { TIER_HAND = 0, TIER_WOOD = 1, TIER_STONE = 2, TIER_IRON = 3, TIER_DIAMOND = 4, TIER_GOLD = 5 };
static const float tierSpeed[] = { 1.0f, 1.0f, 1.5f, 2.0f, 3.0f, 1.2f };

static void GetToolInfo(int itemId, int* outType, int* outTier) {
	if (itemId >= 5 && itemId <= 8) {
		*outTier = TIER_WOOD;    *outType = (itemId - 5) + 1; return;
	}
	if (itemId >= 13 && itemId <= 16) {
		*outTier = TIER_STONE;   *outType = (itemId - 13) + 1; return;
	}
	if (itemId >= 21 && itemId <= 24) {
		*outTier = TIER_IRON;    *outType = (itemId - 21) + 1; return;
	}
	if (itemId >= 29 && itemId <= 32) {
		*outTier = TIER_DIAMOND; *outType = (itemId - 29) + 1; return;
	}
	if (itemId >= 37 && itemId <= 40) {
		*outTier = TIER_GOLD;    *outType = (itemId - 37) + 1; return;
	}
	*outType = TOOL_NONE;
	*outTier = TIER_HAND;
}

static int Block_PreferredTool(BlockID block) {
	cc_uint8 sound = Blocks.DigSounds[block];
	switch (sound) {
		case SOUND_STONE:  return TOOL_PICKAXE;
		case SOUND_METAL:  return TOOL_PICKAXE;
		case SOUND_WOOD:   return TOOL_AXE;
		case SOUND_GRAVEL: return TOOL_SHOVEL;
		case SOUND_SAND:   return TOOL_SHOVEL;
		case SOUND_SNOW:   return TOOL_SHOVEL;
		case SOUND_GRASS:  return TOOL_SHOVEL;
		default:           return TOOL_NONE;
	}
}

static cc_bool Block_RequiresTool(BlockID block) {
	cc_uint8 sound = Blocks.DigSounds[block];
	return sound == SOUND_STONE || sound == SOUND_METAL;
}

/* Returns minimum pickaxe tier needed to get drops from a block.
   -1 means block doesn't require a pickaxe for drops. */
static int Block_MinPickaxeTier(BlockID block) {
	switch (block) {
		case BLOCK_STONE:
		case BLOCK_COBBLE:
		case BLOCK_COAL_ORE:
			return TIER_WOOD;
		case BLOCK_IRON_ORE:
			return TIER_STONE;
		case BLOCK_GOLD_ORE:
		case BLOCK_DIAMOND_ORE:
		case BLOCK_RED_ORE:
		case BLOCK_DIAMOND_BLOCK:
		case BLOCK_GOLD:
		case BLOCK_IRON:
		/* Iron doors require iron or diamond pickaxe */
		case BLOCK_IRON_DOOR:
		case BLOCK_IRON_DOOR_NS_TOP:
		case BLOCK_IRON_DOOR_EW_BOTTOM:
		case BLOCK_IRON_DOOR_EW_TOP:
		case BLOCK_IRON_DOOR_NS_OPEN_BOTTOM:
		case BLOCK_IRON_DOOR_NS_OPEN_TOP:
		case BLOCK_IRON_DOOR_EW_OPEN_BOTTOM:
		case BLOCK_IRON_DOOR_EW_OPEN_TOP:
			return TIER_IRON;
		case BLOCK_OBSIDIAN:
			return TIER_DIAMOND;
		default:
			/* Other stone/metal-sound blocks (brick, mossy rocks, slab, etc.) require any pickaxe */
			if (Blocks.DigSounds[block] == SOUND_STONE || Blocks.DigSounds[block] == SOUND_METAL) {
				return TIER_WOOD;
			}
			return -1;
	}
}

/* Gold pickaxe has same mining capability as wood */
static int Block_EffectivePickTier(int tier) {
	if (tier == TIER_GOLD) return TIER_WOOD;
	return tier;
}

static float Block_BaseBreakTime(BlockID block);

/* Check if a block is instant-break */
static cc_bool Block_IsInstaMine(BlockID block) {
	return Block_BaseBreakTime(block) <= 0.0f && block != BLOCK_BEDROCK;
}

/* Check if the held tool can get drops from this block */
static cc_bool Block_CanGetDrops(BlockID block, int toolType, int toolTier) {
	int minTier;
	/* Insta-mine blocks never require a tool for drops */
	if (Block_IsInstaMine(block)) return true;
	minTier = Block_MinPickaxeTier(block);
	if (minTier < 0) return true;
	if (toolType != TOOL_PICKAXE) return false;
	return Block_EffectivePickTier(toolTier) >= minTier;
}

static float Block_BaseBreakTime(BlockID block) {
	cc_uint8 sound;

	/* Instant-break blocks */
	if (block == BLOCK_TORCH)        return 0.0f;
	if (block == BLOCK_RED_ORE_DUST) return 0.0f;
	if (block == BLOCK_LIT_RED_ORE_DUST) return 0.0f;
	if (block == BLOCK_LEAVES)       return 0.0f;
	if (block == BLOCK_LEVER)        return 0.0f;
	if (block == BLOCK_LEVER_ON)     return 0.0f;
	if (block == BLOCK_BUTTON)       return 0.0f;
	if (block == BLOCK_BUTTON_PRESSED) return 0.0f;
	if (block == BLOCK_PRESSURE_PLATE) return 0.0f;
	if (block == BLOCK_PRESSURE_PLATE_PRESSED) return 0.0f;
	if (block == BLOCK_STONE_PLATE) return 0.0f;
	if (block == BLOCK_STONE_PLATE_PRESSED) return 0.0f;
	if (block == BLOCK_LADDER)       return 0.0f;
	if (block == BLOCK_SAPLING)      return 0.0f;
	if (block == BLOCK_FIRE)         return 0.0f;
	/* Redstone torches (all variants) */
	if (block >= BLOCK_RED_TORCH_ON_S && block <= BLOCK_RED_TORCH_OFF_W) return 0.0f;
	if (block == BLOCK_RED_TORCH_UNMOUNTED)     return 0.0f;
	if (block == BLOCK_RED_TORCH_UNMOUNTED_OFF) return 0.0f;
	/* Flowers and mushrooms */
	if (block == BLOCK_DANDELION)    return 0.0f;
	if (block == BLOCK_ROSE)         return 0.0f;
	if (block == BLOCK_BROWN_SHROOM) return 0.0f;
	if (block == BLOCK_RED_SHROOM)   return 0.0f;
	/* Wheat crops */
	if (block >= BLOCK_WHEAT_0 && block <= BLOCK_WHEAT_7) return 0.0f;

	if (block == BLOCK_OBSIDIAN)     return 50.0f;
	if (block == BLOCK_IRON)         return 5.0f;
	if (block == BLOCK_GOLD)         return 3.0f;
	if (block == BLOCK_DIAMOND_BLOCK)return 5.0f;
	if (block == BLOCK_IRON_ORE)     return 3.0f;
	if (block == BLOCK_GOLD_ORE)     return 3.0f;
	if (block == BLOCK_DIAMOND_ORE)  return 3.0f;
	if (block == BLOCK_COAL_ORE)     return 3.0f;
	if (block == BLOCK_RED_ORE)      return 3.0f;

	sound = Blocks.DigSounds[block];
	switch (sound) {
		case SOUND_STONE:  return 1.3f;
		case SOUND_METAL:  return 5.0f;
		case SOUND_WOOD:   return 2.0f;
		case SOUND_GRASS:  return 1.0f;
		case SOUND_GRAVEL: return 1.0f;
		case SOUND_SAND:   return 0.9f;
		case SOUND_SNOW:   return 0.2f;
		case SOUND_GLASS:  return 0.3f;
		case SOUND_CLOTH:  return 0.8f;
		default:           return 0.4f;
	}
}

/* Block breaking state for survival mode */
static cc_bool  breaking_active;
static IVec3    breaking_pos;
static BlockID  breaking_block;
static float    breaking_progress;
static float    breaking_totalTime;
static int      breaking_crackStage = -1;
static float    breaking_soundTimer;
static float    breaking_swingTimer;
static GfxResourceID crack_vb;

static void BlockBreaking_Reset(void) {
	breaking_active     = false;
	breaking_progress   = 0.0f;
	breaking_crackStage = -1;
	breaking_soundTimer = 0.0f;
	breaking_swingTimer = 0.0f;
}

static float CalcBreakTime(BlockID block) {
	int toolType, toolTier, preferred;
	float baseTime, multiplier;

	if (block == BLOCK_BEDROCK) return -1.0f;
	baseTime = Block_BaseBreakTime(block);
	if (baseTime <= 0.0f) return 0.0f;

	GetToolInfo(Hotbar_SelectedItem, &toolType, &toolTier);
	preferred = Block_PreferredTool(block);

	/* Stone/metal without a pickaxe takes as long as obsidian with iron pickaxe (25s) */
	if (Block_RequiresTool(block) && toolType != preferred) return 25.0f;

	/* Pickaxe tier too low for this block also takes 25s (e.g. stone pickaxe on diamond ore) */
	if (preferred == TOOL_PICKAXE && toolType == TOOL_PICKAXE) {
		int minTier = Block_MinPickaxeTier(block);
		if (minTier > 0 && Block_EffectivePickTier(toolTier) < minTier) return 25.0f;
	}

	if (toolType == preferred && preferred != TOOL_NONE) {
		multiplier = tierSpeed[toolTier];
	} else {
		multiplier = 1.0f;
	}

	baseTime = baseTime / multiplier;

	/* Shovels and axes get a flat 0.2s speed bonus */
	if (toolType == preferred && (toolType == TOOL_SHOVEL || toolType == TOOL_AXE)) {
		baseTime -= 0.2f;
		if (baseTime < 0.05f) baseTime = 0.05f;
	}

	return baseTime;
}


static cc_bool Mob_TryPunchMob(void); /* forward declaration */

/* Helper: break door/iron door pairs when one half is removed */
static void BreakDoorPair(BlockID old, IVec3 pos) {
	IVec3 otherPos;
	BlockID otherBlock;

	if (old == BLOCK_DOOR_NS_BOTTOM || old == BLOCK_DOOR_EW_BOTTOM) {
		otherPos.x = pos.x; otherPos.y = pos.y + 1; otherPos.z = pos.z;
		if (World_Contains(otherPos.x, otherPos.y, otherPos.z)) {
			otherBlock = World_GetBlock(otherPos.x, otherPos.y, otherPos.z);
			if (otherBlock == BLOCK_DOOR_NS_TOP || otherBlock == BLOCK_DOOR_EW_TOP) {
				Game_ChangeBlock(otherPos.x, otherPos.y, otherPos.z, BLOCK_AIR);
				Event_RaiseBlock(&UserEvents.BlockChanged, otherPos, otherBlock, BLOCK_AIR);
			}
		}
	} else if (old == BLOCK_DOOR_NS_TOP || old == BLOCK_DOOR_EW_TOP) {
		otherPos.x = pos.x; otherPos.y = pos.y - 1; otherPos.z = pos.z;
		if (World_Contains(otherPos.x, otherPos.y, otherPos.z)) {
			otherBlock = World_GetBlock(otherPos.x, otherPos.y, otherPos.z);
			if (otherBlock == BLOCK_DOOR_NS_BOTTOM || otherBlock == BLOCK_DOOR_EW_BOTTOM) {
				Game_ChangeBlock(otherPos.x, otherPos.y, otherPos.z, BLOCK_AIR);
				Event_RaiseBlock(&UserEvents.BlockChanged, otherPos, otherBlock, BLOCK_AIR);
			}
		}
	}

	if (IsIronDoorBottom(old)) {
		otherPos.x = pos.x; otherPos.y = pos.y + 1; otherPos.z = pos.z;
		if (World_Contains(otherPos.x, otherPos.y, otherPos.z)) {
			otherBlock = World_GetBlock(otherPos.x, otherPos.y, otherPos.z);
			if (IsIronDoorTop(otherBlock)) {
				Game_ChangeBlock(otherPos.x, otherPos.y, otherPos.z, BLOCK_AIR);
				Event_RaiseBlock(&UserEvents.BlockChanged, otherPos, otherBlock, BLOCK_AIR);
			}
		}
	} else if (IsIronDoorTop(old)) {
		otherPos.x = pos.x; otherPos.y = pos.y - 1; otherPos.z = pos.z;
		if (World_Contains(otherPos.x, otherPos.y, otherPos.z)) {
			otherBlock = World_GetBlock(otherPos.x, otherPos.y, otherPos.z);
			if (IsIronDoorBottom(otherBlock)) {
				Game_ChangeBlock(otherPos.x, otherPos.y, otherPos.z, BLOCK_AIR);
				Event_RaiseBlock(&UserEvents.BlockChanged, otherPos, otherBlock, BLOCK_AIR);
			}
		}
	}
}

/* Helper: immediately destroy a block and handle side effects */
/* Helper: check if a block is any wooden door variant */
static cc_bool IsWoodDoor(BlockID b) {
	return b == BLOCK_DOOR_NS_BOTTOM || b == BLOCK_DOOR_NS_TOP ||
		b == BLOCK_DOOR_EW_BOTTOM || b == BLOCK_DOOR_EW_TOP;
}

/* Helper: check if a block is any iron door variant */
static cc_bool IsAnyIronDoor(BlockID b) {
	return IsIronDoorBottom(b) || IsIronDoorTop(b);
}

/* Helper: apply small random horizontal momentum to a dropped item */
static void DropItem_ApplyRandomMomentum(int slot) {
	float angle, speed;
	if (!mob_rng_inited) {
		Random_SeedFromCurrentTime(&mob_rng);
		mob_rng_inited = true;
	}
	angle = Random_Float(&mob_rng) * 2.0f * MATH_PI;
	speed = 0.05f + Random_Float(&mob_rng) * 0.10f;
	droppedItemVelocityX[slot] = Math_CosF(angle) * speed;
	droppedItemVelocityZ[slot] = Math_SinF(angle) * speed;
	droppedItemVelocityY[slot] = 0.15f;
}

static void BreakBlockNow(IVec3 pos, BlockID old) {
	Game_ChangeBlock(pos.x, pos.y, pos.z, BLOCK_AIR);
	Event_RaiseBlock(&UserEvents.BlockChanged, pos, old, BLOCK_AIR);
	BreakDoorPair(old, pos);

	/* In survival mode, drop the broken block */
	if (Game_SurvivalMode && old != BLOCK_BEDROCK && old != BLOCK_AIR &&
		old != BLOCK_FIRE &&
		Blocks.Draw[old] != DRAW_GAS) {
		Vec3 dropPos;
		int toolType, toolTier, slot;
		dropPos.x = (float)pos.x + 0.5f;
		dropPos.y = (float)pos.y + 0.3f;
		dropPos.z = (float)pos.z + 0.5f;

		if (!mob_rng_inited) {
			Random_SeedFromCurrentTime(&mob_rng);
			mob_rng_inited = true;
		}

		GetToolInfo(Hotbar_SelectedItem, &toolType, &toolTier);

		/* Check pickaxe tier requirement for drops */
		if (!Block_CanGetDrops(old, toolType, toolTier)) return;

		if (old == BLOCK_STONE) {
			/* Stone: gold pickaxe drops stone, otherwise cobblestone */
			slot = DropItem_FindFreeSlot();
			if (slot == -1) slot = DropItem_EvictOldest();
			if (slot != -1) {
				if (toolType == TOOL_PICKAXE && toolTier == TIER_GOLD) {
					DropItem_Spawn(slot, dropPos, BLOCK_STONE, false, 0);
				} else {
					DropItem_Spawn(slot, dropPos, BLOCK_COBBLE, false, 0);
				}
				droppedItemPickupDelay[slot] = 0.0f;
				DropItem_ApplyRandomMomentum(slot);
			}
		} else if (old == BLOCK_COAL_ORE) {
			/* Coal ore drops 1-3 coal items */
			int count = Random_Next(&mob_rng, 3) + 1;
			slot = DropItem_FindFreeSlot();
			if (slot == -1) slot = DropItem_EvictOldest();
			if (slot != -1) {
				DropItem_Spawn(slot, dropPos, BLOCK_AIR, true, ITEM_COAL);
				droppedItemCount[slot] = count;
				droppedItemPickupDelay[slot] = 0.0f;
				DropItem_ApplyRandomMomentum(slot);
			}
		} else if (old == BLOCK_DIAMOND_ORE) {
			/* Diamond ore drops 1 diamond item */
			slot = DropItem_FindFreeSlot();
			if (slot == -1) slot = DropItem_EvictOldest();
			if (slot != -1) {
				DropItem_Spawn(slot, dropPos, BLOCK_AIR, true, ITEM_DIAMOND_ITEM);
				droppedItemPickupDelay[slot] = 0.0f;
				DropItem_ApplyRandomMomentum(slot);
			}
		} else if (old == BLOCK_RED_ORE) {
			/* Red ore drops 1-10 red ore dust blocks */
			int count = Random_Next(&mob_rng, 10) + 1;
			slot = DropItem_FindFreeSlot();
			if (slot == -1) slot = DropItem_EvictOldest();
			if (slot != -1) {
				DropItem_Spawn(slot, dropPos, BLOCK_RED_ORE_DUST, false, 0);
				droppedItemCount[slot] = count;
				droppedItemPickupDelay[slot] = 0.0f;
				DropItem_ApplyRandomMomentum(slot);
			}
		} else if (old == BLOCK_LOG) {
			/* Oak logs: drop planks unless broken by axe */
			if (toolType == TOOL_AXE) {
				slot = DropItem_FindFreeSlot();
				if (slot == -1) slot = DropItem_EvictOldest();
				if (slot != -1) {
					DropItem_Spawn(slot, dropPos, BLOCK_LOG, false, 0);
					droppedItemPickupDelay[slot] = 0.0f;
					DropItem_ApplyRandomMomentum(slot);
				}
			} else {
				int numPlanks = Random_Next(&mob_rng, 6) + 1;
				slot = DropItem_FindFreeSlot();
				if (slot == -1) slot = DropItem_EvictOldest();
				if (slot != -1) {
					DropItem_Spawn(slot, dropPos, BLOCK_WOOD, false, 0);
					droppedItemCount[slot] = numPlanks;
					droppedItemPickupDelay[slot] = 0.0f;
					DropItem_ApplyRandomMomentum(slot);
				}
			}
		} else if (old == BLOCK_GRASS) {
			/* Grass blocks: drop dirt unless broken by golden shovel */
			if (toolType == TOOL_SHOVEL && toolTier == TIER_GOLD) {
				slot = DropItem_FindFreeSlot();
				if (slot == -1) slot = DropItem_EvictOldest();
				if (slot != -1) {
					DropItem_Spawn(slot, dropPos, BLOCK_GRASS, false, 0);
					droppedItemPickupDelay[slot] = 0.0f;
					DropItem_ApplyRandomMomentum(slot);
				}
			} else {
				slot = DropItem_FindFreeSlot();
				if (slot == -1) slot = DropItem_EvictOldest();
				if (slot != -1) {
					DropItem_Spawn(slot, dropPos, BLOCK_DIRT, false, 0);
					droppedItemPickupDelay[slot] = 0.0f;
					DropItem_ApplyRandomMomentum(slot);
				}
			}
		} else if (old == BLOCK_GRAVEL) {
			/* Gravel: gold shovel 50% flint, normal 1 in 5 flint, otherwise gravel */
			cc_bool dropFlint;
			if (toolType == TOOL_SHOVEL && toolTier == TIER_GOLD) {
				dropFlint = Random_Next(&mob_rng, 2) == 0;
			} else {
				dropFlint = Random_Next(&mob_rng, 5) == 0;
			}
			if (dropFlint) {
				slot = DropItem_FindFreeSlot();
				if (slot == -1) slot = DropItem_EvictOldest();
				if (slot != -1) {
					DropItem_Spawn(slot, dropPos, BLOCK_AIR, true, ITEM_FLINT);
					droppedItemPickupDelay[slot] = 0.0f;
					DropItem_ApplyRandomMomentum(slot);
				}
			} else {
				slot = DropItem_FindFreeSlot();
				if (slot == -1) slot = DropItem_EvictOldest();
				if (slot != -1) {
					DropItem_Spawn(slot, dropPos, BLOCK_GRAVEL, false, 0);
					droppedItemPickupDelay[slot] = 0.0f;
					DropItem_ApplyRandomMomentum(slot);
				}
			}
		} else if (old == BLOCK_LEAVES) {
			/* Leaves: gold axe drops leaves, otherwise 5% sapling */
			if (toolType == TOOL_AXE && toolTier == TIER_GOLD) {
				slot = DropItem_FindFreeSlot();
				if (slot == -1) slot = DropItem_EvictOldest();
				if (slot != -1) {
					DropItem_Spawn(slot, dropPos, BLOCK_LEAVES, false, 0);
					droppedItemPickupDelay[slot] = 0.0f;
					DropItem_ApplyRandomMomentum(slot);
				}
			} else if (Random_Next(&mob_rng, 20) == 0) {
				slot = DropItem_FindFreeSlot();
				if (slot == -1) slot = DropItem_EvictOldest();
				if (slot != -1) {
					DropItem_Spawn(slot, dropPos, BLOCK_SAPLING, false, 0);
					droppedItemPickupDelay[slot] = 0.0f;
					DropItem_ApplyRandomMomentum(slot);
				}
			}
		} else if (old == BLOCK_GLASS) {
			/* Glass: only drops if mined by golden pickaxe */
			if (toolType == TOOL_PICKAXE && toolTier == TIER_GOLD) {
				slot = DropItem_FindFreeSlot();
				if (slot == -1) slot = DropItem_EvictOldest();
				if (slot != -1) {
					DropItem_Spawn(slot, dropPos, BLOCK_GLASS, false, 0);
					droppedItemPickupDelay[slot] = 0.0f;
					DropItem_ApplyRandomMomentum(slot);
				}
			}
			/* Otherwise glass shatters with no drop */
		} else if (old == BLOCK_ICE) {
			/* Ice: only drops if mined by golden pickaxe */
			if (toolType == TOOL_PICKAXE && toolTier == TIER_GOLD) {
				slot = DropItem_FindFreeSlot();
				if (slot == -1) slot = DropItem_EvictOldest();
				if (slot != -1) {
					DropItem_Spawn(slot, dropPos, BLOCK_ICE, false, 0);
					droppedItemPickupDelay[slot] = 0.0f;
					DropItem_ApplyRandomMomentum(slot);
				}
			}
			/* Otherwise ice melts with no drop */
		} else {
			/* Furnace: drop its contents (input, fuel, output) before dropping the block */
			if (old == BLOCK_FURNACE) {
				struct FurnaceData fdata;
				Furnace_Remove(pos.x, pos.y, pos.z, &fdata);
				if (fdata.input.count > 0) {
					slot = DropItem_FindFreeSlot();
					if (slot == -1) slot = DropItem_EvictOldest();
					if (slot != -1) {
						DropItem_Spawn(slot, dropPos, fdata.input.block,
							fdata.input.itemId != ITEM_NONE, fdata.input.itemId);
						droppedItemCount[slot] = fdata.input.count;
						droppedItemPickupDelay[slot] = 0.0f;
						DropItem_ApplyRandomMomentum(slot);
					}
				}
				if (fdata.fuel.count > 0) {
					slot = DropItem_FindFreeSlot();
					if (slot == -1) slot = DropItem_EvictOldest();
					if (slot != -1) {
						DropItem_Spawn(slot, dropPos, fdata.fuel.block,
							fdata.fuel.itemId != ITEM_NONE, fdata.fuel.itemId);
						droppedItemCount[slot] = fdata.fuel.count;
						droppedItemPickupDelay[slot] = 0.0f;
						DropItem_ApplyRandomMomentum(slot);
					}
				}
				if (fdata.output.count > 0) {
					slot = DropItem_FindFreeSlot();
					if (slot == -1) slot = DropItem_EvictOldest();
					if (slot != -1) {
						DropItem_Spawn(slot, dropPos, fdata.output.block,
							fdata.output.itemId != ITEM_NONE, fdata.output.itemId);
						droppedItemCount[slot] = fdata.output.count;
						droppedItemPickupDelay[slot] = 0.0f;
						DropItem_ApplyRandomMomentum(slot);
					}
				}
			}
			/* Chest: drop its contents before dropping the block */
			if (old == BLOCK_CHEST || (old >= BLOCK_DCHEST_S_L && old <= BLOCK_DCHEST_W_R)) {
				struct ChestData cdata;
				int ci;
				Chest_Remove(pos.x, pos.y, pos.z, &cdata);
				for (ci = 0; ci < CHEST_SLOTS; ci++) {
					if (cdata.slots[ci].count <= 0) continue;
					slot = DropItem_FindFreeSlot();
					if (slot == -1) slot = DropItem_EvictOldest();
					if (slot != -1) {
						DropItem_Spawn(slot, dropPos, cdata.slots[ci].block,
							cdata.slots[ci].itemId != ITEM_NONE, cdata.slots[ci].itemId);
						droppedItemCount[slot] = cdata.slots[ci].count;
						droppedItemPickupDelay[slot] = 0.0f;
						DropItem_ApplyRandomMomentum(slot);
					}
				}
				/* If breaking half of a double chest, revert the other half to single */
				if (old >= BLOCK_DCHEST_S_L && old <= BLOCK_DCHEST_W_R) {
					int px, py, pz;
					if (Chest_GetPartnerPos(old, pos.x, pos.y, pos.z, &px, &py, &pz)) {
						if (World_Contains(px, py, pz)) {
							BlockID partnerBlock = World_GetBlock(px, py, pz);
							if (partnerBlock >= BLOCK_DCHEST_S_L && partnerBlock <= BLOCK_DCHEST_W_R) {
								Game_ChangeBlock(px, py, pz, BLOCK_CHEST);
							}
						}
					}
				}
			}
			/* Normal block drop - map special blocks to their drop form */
			{
				BlockID dropBlock = old;
				if (IsWoodDoor(old)) {
					dropBlock = BLOCK_DOOR_NS_BOTTOM;
				} else if (IsAnyIronDoor(old)) {
					dropBlock = BLOCK_IRON_DOOR;
				} else if (old == BLOCK_PRESSURE_PLATE_PRESSED) {
					dropBlock = BLOCK_PRESSURE_PLATE;
				} else if (old == BLOCK_STONE_PLATE_PRESSED) {
					dropBlock = BLOCK_STONE_PLATE;
				} else if (old == BLOCK_FARMLAND_DRY || old == BLOCK_FARMLAND_WET) {
					dropBlock = BLOCK_DIRT;
				} else if (old >= BLOCK_WHEAT_0 && old <= BLOCK_WHEAT_7) {
					/* Wheat crops: drop seeds and wheat based on growth stage */
					dropBlock = BLOCK_AIR; /* suppress normal block drop */
					if (!mob_rng_inited) {
						Random_SeedFromCurrentTime(&mob_rng);
						mob_rng_inited = true;
					}
					{
						int stage = old - BLOCK_WHEAT_0;
						int seedCount, wheatCount, di;
						if (stage == 7) {
							/* Fully grown: 1 wheat + 0-3 seeds */
							wheatCount = 1;
							seedCount = Random_Next(&mob_rng, 4);
						} else if (stage >= 4) {
							/* Mid growth: 0-2 seeds, no wheat */
							wheatCount = 0;
							seedCount = Random_Next(&mob_rng, 3);
						} else {
							/* Early growth: 0-1 seeds, no wheat */
							wheatCount = 0;
							seedCount = Random_Next(&mob_rng, 2);
						}
						for (di = 0; di < wheatCount; di++) {
							int ws = DropItem_FindFreeSlot();
							if (ws == -1) ws = DropItem_EvictOldest();
							if (ws != -1) {
								DropItem_Spawn(ws, dropPos, BLOCK_AIR, true, ITEM_WHEAT);
								droppedItemPickupDelay[ws] = 0.0f;
								DropItem_ApplyRandomMomentum(ws);
							}
						}
						for (di = 0; di < seedCount; di++) {
							int ss = DropItem_FindFreeSlot();
							if (ss == -1) ss = DropItem_EvictOldest();
							if (ss != -1) {
								DropItem_Spawn(ss, dropPos, BLOCK_AIR, true, ITEM_SEEDS);
								droppedItemPickupDelay[ss] = 0.0f;
								DropItem_ApplyRandomMomentum(ss);
							}
						}
					}
				} else if (old >= BLOCK_DCHEST_S_L && old <= BLOCK_DCHEST_W_R) {
					dropBlock = BLOCK_CHEST;
				}
				if (dropBlock != BLOCK_AIR) {
					slot = DropItem_FindFreeSlot();
					if (slot == -1) slot = DropItem_EvictOldest();
					if (slot != -1) {
						DropItem_Spawn(slot, dropPos, dropBlock, false, 0);
						droppedItemPickupDelay[slot] = 0.0f;
						DropItem_ApplyRandomMomentum(slot);
					}
				}
			}
		}
	}

	/* Break dependent blocks sitting on top of the broken block */
	if (pos.y + 1 < World.Height) {
		BlockID above = World_GetBlock(pos.x, pos.y + 1, pos.z);
		if (above == BLOCK_DANDELION || above == BLOCK_ROSE ||
			above == BLOCK_RED_SHROOM || above == BLOCK_BROWN_SHROOM ||
			above == BLOCK_SAPLING || above == BLOCK_RED_ORE_DUST ||
			above == BLOCK_LIT_RED_ORE_DUST ||
			above == BLOCK_PRESSURE_PLATE || above == BLOCK_PRESSURE_PLATE_PRESSED ||
			above == BLOCK_STONE_PLATE || above == BLOCK_STONE_PLATE_PRESSED ||
			(above >= BLOCK_WHEAT_0 && above <= BLOCK_WHEAT_7)) {
			IVec3 abovePos;
			abovePos.x = pos.x; abovePos.y = pos.y + 1; abovePos.z = pos.z;
			BreakBlockNow(abovePos, above);
		}
	}
}

/* Drop a block item at the given world position (called from BlockPhysics for attached blocks) */
void Physics_DropBlock(int x, int y, int z, BlockID block) {
	int slot;
	Vec3 dropPos;
	if (!Game_SurvivalMode) return;
	if (block == BLOCK_AIR || block == BLOCK_BEDROCK) return;

	dropPos.x = (float)x + 0.5f;
	dropPos.y = (float)y + 0.3f;
	dropPos.z = (float)z + 0.5f;

	slot = DropItem_FindFreeSlot();
	if (slot == -1) slot = DropItem_EvictOldest();
	if (slot != -1) {
		DropItem_Spawn(slot, dropPos, block, false, 0);
		droppedItemPickupDelay[slot] = 0.0f;
		DropItem_ApplyRandomMomentum(slot);
	}
}

static void InputHandler_DeleteBlock(void) {
	IVec3 pos;
	BlockID old;
	float breakTime;

	/* Try to punch a mob first (empty hand or holding item) */
	if (Inventory_SelectedBlock == BLOCK_AIR || Hotbar_SelectedItem != ITEM_NONE) {
		if (Mob_TryPunchMob()) return;
	}

	/* always play delete animations, even if we aren't deleting a block */
	HeldBlockRenderer_ClickAnim(true);

	pos = Game_SelectedPos.pos;
	if (!Game_SelectedPos.valid || !World_Contains(pos.x, pos.y, pos.z)) return;

	old = World_GetBlock(pos.x, pos.y, pos.z);
	if (Blocks.Draw[old] == DRAW_GAS || !Blocks.CanDelete[old]) return;

	/* Creative mode: instant break */
	if (!Game_SurvivalMode) {
		BreakBlockNow(pos, old);
		return;
	}

	/* Survival mode: timed breaking */
	breakTime = CalcBreakTime(old);

	if (breakTime < 0.0f) {
		/* Can't break (bedrock, or wrong tool for stone/metal) */
		BlockBreaking_Reset();
		return;
	}

	if (breakTime <= 0.001f) {
		/* Instant break (e.g. torches, flowers) */
		BreakBlockNow(pos, old);
		BlockBreaking_Reset();
		return;
	}

	/* Start or continue breaking */
	if (!breaking_active ||
		breaking_pos.x != pos.x || breaking_pos.y != pos.y || breaking_pos.z != pos.z) {
		breaking_active    = true;
		breaking_pos       = pos;
		breaking_block     = old;
		breaking_progress  = 0.0f;
		breaking_totalTime = breakTime;
		breaking_crackStage = 0;
	}
}

static void InputHandler_PlaceBlock(void) {
	IVec3 pos, targetPos, otherPos;
	BlockID old, block, targetBlock, otherBlock, newBlock, newOtherBlock;
	
	/* Check if right-clicking on a button or door */
	targetPos = Game_SelectedPos.pos;
	if (Game_SelectedPos.valid && World_Contains(targetPos.x, targetPos.y, targetPos.z)) {
		targetBlock = World_GetBlock(targetPos.x, targetPos.y, targetPos.z);
		
		/* If clicking on an unpressed button, press it */
		if (targetBlock == BLOCK_BUTTON) {
			newBlock = BLOCK_BUTTON_PRESSED;
			
			Game_ChangeBlock(targetPos.x, targetPos.y, targetPos.z, newBlock);
			Event_RaiseBlock(&UserEvents.BlockChanged, targetPos, targetBlock, newBlock);
			Audio_PlayDigSound(SOUND_BUTTON_ON);
			return;
		}
		
		/* If clicking on a lever, toggle it on/off */
		if (targetBlock == BLOCK_LEVER || targetBlock == BLOCK_LEVER_ON) {
			newBlock = (targetBlock == BLOCK_LEVER) ? BLOCK_LEVER_ON : BLOCK_LEVER;
			
			Game_ChangeBlock(targetPos.x, targetPos.y, targetPos.z, newBlock);
			Event_RaiseBlock(&UserEvents.BlockChanged, targetPos, targetBlock, newBlock);
			Audio_PlayDigSound((targetBlock == BLOCK_LEVER) ? SOUND_BUTTON_ON : SOUND_BUTTON_OFF);
			return;
		}

		/* If clicking on a crafting table, open the 3x3 crafting GUI */
		if (targetBlock == BLOCK_CRAFT) {
			CraftingTableScreen_Show();
			return;
		}

		/* If clicking on a furnace, open the smelting GUI */
		if (targetBlock == BLOCK_FURNACE) {
			Furnace_Open(targetPos.x, targetPos.y, targetPos.z);
			FurnaceScreen_Show();
			return;
		}

		/* If clicking on a chest or double chest, open the chest GUI */
		if (targetBlock == BLOCK_CHEST ||
		    (targetBlock >= BLOCK_DCHEST_S_L && targetBlock <= BLOCK_DCHEST_W_R)) {
			Chest_Open(targetPos.x, targetPos.y, targetPos.z);
			ChestScreen_Show();
			return;
		}

		/* If clicking on TNT with empty hand or flint and steel, light the fuse */
		if (targetBlock == BLOCK_TNT && (Inventory_SelectedBlock == BLOCK_AIR || Hotbar_SelectedItem == ITEM_FLINT_STEEL)) {
			TNT_ScheduleFuse(targetPos.x, targetPos.y, targetPos.z);
			return;
		}

		/* If clicking on grass or dirt with a hoe, convert to dry farmland */
		if ((targetBlock == BLOCK_GRASS || targetBlock == BLOCK_DIRT || targetBlock == BLOCK_SNOWY_GRASS) &&
		    Hotbar_SelectedItem >= ITEM_WOOD_HOE && Hotbar_SelectedItem <= ITEM_GOLD_HOE) {
			Game_ChangeBlock(targetPos.x, targetPos.y, targetPos.z, BLOCK_FARMLAND_DRY);
			Event_RaiseBlock(&UserEvents.BlockChanged, targetPos, targetBlock, BLOCK_FARMLAND_DRY);
			Audio_PlayDigSound(SOUND_GRAVEL);
			/* 40% chance to drop seeds when hoeing grass */
			if (targetBlock == BLOCK_GRASS || targetBlock == BLOCK_SNOWY_GRASS) {
				if (!mob_rng_inited) {
					Random_SeedFromCurrentTime(&mob_rng);
					mob_rng_inited = true;
				}
				if (Random_Next(&mob_rng, 5) < 2) {
					Vec3 seedPos;
					int seedSlot;
					seedPos.x = (float)targetPos.x + 0.5f;
					seedPos.y = (float)targetPos.y + 0.3f;
					seedPos.z = (float)targetPos.z + 0.5f;
					seedSlot = DropItem_FindFreeSlot();
					if (seedSlot == -1) seedSlot = DropItem_EvictOldest();
					if (seedSlot != -1) {
						DropItem_Spawn(seedSlot, seedPos, BLOCK_AIR, true, ITEM_SEEDS);
						droppedItemPickupDelay[seedSlot] = 0.0f;
						DropItem_ApplyRandomMomentum(seedSlot);
					}
				}
			}
			return;
		}

		/* If clicking on farmland with a shovel, convert back to dirt */
		if ((targetBlock == BLOCK_FARMLAND_DRY || targetBlock == BLOCK_FARMLAND_WET) &&
		    (Hotbar_SelectedItem == ITEM_WOOD_SHOVEL || Hotbar_SelectedItem == ITEM_STONE_SHOVEL ||
		     Hotbar_SelectedItem == ITEM_IRON_SHOVEL || Hotbar_SelectedItem == ITEM_DIAMOND_SHOVEL ||
		     Hotbar_SelectedItem == ITEM_GOLD_SHOVEL)) {
			/* If wheat is on top, break it first */
			if (targetPos.y + 1 < World.Height) {
				BlockID cropAbove = World_GetBlock(targetPos.x, targetPos.y + 1, targetPos.z);
				if (cropAbove >= BLOCK_WHEAT_0 && cropAbove <= BLOCK_WHEAT_7) {
					IVec3 cropPos;
					cropPos.x = targetPos.x; cropPos.y = targetPos.y + 1; cropPos.z = targetPos.z;
					BreakBlockNow(cropPos, cropAbove);
				}
			}
			Game_ChangeBlock(targetPos.x, targetPos.y, targetPos.z, BLOCK_DIRT);
			Event_RaiseBlock(&UserEvents.BlockChanged, targetPos, targetBlock, BLOCK_DIRT);
			Audio_PlayDigSound(SOUND_GRAVEL);
			return;
		}

		/* If clicking on farmland with seeds, plant wheat */
		if ((targetBlock == BLOCK_FARMLAND_DRY || targetBlock == BLOCK_FARMLAND_WET) &&
		    Hotbar_SelectedItem == ITEM_SEEDS && targetPos.y + 1 < World.Height) {
			BlockID aboveFarm = World_GetBlock(targetPos.x, targetPos.y + 1, targetPos.z);
			if (aboveFarm == BLOCK_AIR) {
				Game_ChangeBlock(targetPos.x, targetPos.y + 1, targetPos.z, BLOCK_WHEAT_0);
				Event_RaiseBlock(&UserEvents.BlockChanged, targetPos, targetBlock, targetBlock);
				Audio_PlayDigSound(SOUND_GRASS);
				/* Consume one seed from the held stack */
				if (Game_SurvivalMode) {
					int cnt = Hotbar_SelectedCount - 1;
					Hotbar_SetCount(Inventory.SelectedIndex, cnt);
					if (cnt <= 0) {
						Hotbar_SetItem(Inventory.SelectedIndex, ITEM_NONE);
						Inventory_Set(Inventory.SelectedIndex, BLOCK_AIR);
						Hotbar_SetCount(Inventory.SelectedIndex, 0);
					}
				}
				return;
			}
		}

		/* If clicking on a door, swap its type (NS ↔ EW to open/close) */
		if (targetBlock == BLOCK_DOOR_NS_BOTTOM || targetBlock == BLOCK_DOOR_NS_TOP ||
		    targetBlock == BLOCK_DOOR_EW_BOTTOM || targetBlock == BLOCK_DOOR_EW_TOP) {
			
			/* Determine new block type (swap NS ↔ EW) */
			if (targetBlock == BLOCK_DOOR_NS_BOTTOM) {
				newBlock = BLOCK_DOOR_EW_BOTTOM;
				newOtherBlock = BLOCK_DOOR_EW_TOP;
				otherPos.y = targetPos.y + 1;
			} else if (targetBlock == BLOCK_DOOR_NS_TOP) {
				newBlock = BLOCK_DOOR_EW_TOP;
				newOtherBlock = BLOCK_DOOR_EW_BOTTOM;
				otherPos.y = targetPos.y - 1;
			} else if (targetBlock == BLOCK_DOOR_EW_BOTTOM) {
				newBlock = BLOCK_DOOR_NS_BOTTOM;
				newOtherBlock = BLOCK_DOOR_NS_TOP;
				otherPos.y = targetPos.y + 1;
			} else { /* BLOCK_DOOR_EW_TOP */
				newBlock = BLOCK_DOOR_NS_TOP;
				newOtherBlock = BLOCK_DOOR_NS_BOTTOM;
				otherPos.y = targetPos.y - 1;
			}
			
			/* Change clicked door block */
			Game_ChangeBlock(targetPos.x, targetPos.y, targetPos.z, newBlock);
			Event_RaiseBlock(&UserEvents.BlockChanged, targetPos, targetBlock, newBlock);
			
			/* Change other half of door */
			otherPos.x = targetPos.x;
			otherPos.z = targetPos.z;
			if (World_Contains(otherPos.x, otherPos.y, otherPos.z)) {
				otherBlock = World_GetBlock(otherPos.x, otherPos.y, otherPos.z);
				Game_ChangeBlock(otherPos.x, otherPos.y, otherPos.z, newOtherBlock);
				Event_RaiseBlock(&UserEvents.BlockChanged, otherPos, otherBlock, newOtherBlock);
			}
			
			/* Play door open/close sound */
			Audio_PlayDigSound(SOUND_DOOR);
			
			return; /* Don't place a block, just swap the door */
		}
	}
	
	/* Flint and steel: place fire at the target location */
	if (Hotbar_SelectedItem == ITEM_FLINT_STEEL) {
		pos = Game_SelectedPos.translatedPos;
		if (!Game_SelectedPos.valid || !World_Contains(pos.x, pos.y, pos.z)) return;

		old = World_GetBlock(pos.x, pos.y, pos.z);
		if (Game_CanPick(old)) return; /* Can't replace a solid block */

		Game_ChangeBlock(pos.x, pos.y, pos.z, BLOCK_FIRE);
		Event_RaiseBlock(&UserEvents.BlockChanged, pos, old, BLOCK_FIRE);
		{ int ignVol = (int)(Audio_SoundsVolume * 1.5f); if (ignVol > 100) ignVol = 100; Audio_PlayDigSoundRateVolume(SOUND_IGNITE, 100, ignVol); }
		return;
	}

	pos = Game_SelectedPos.translatedPos;
	if (!Game_SelectedPos.valid || !World_Contains(pos.x, pos.y, pos.z)) return;

	old   = World_GetBlock(pos.x, pos.y, pos.z);
	block = Inventory_SelectedBlock;
	if (AutoRotate_Enabled) block = AutoRotate_RotateBlock(block);

	if (Game_CanPick(old) || !Blocks.CanPlace[block]) return;
	/* air-ish blocks can only replace over other air-ish blocks */
	if (Blocks.Draw[block] == DRAW_GAS && Blocks.Draw[old] != DRAW_GAS) return;

	/* undeletable gas blocks can't be replaced with other blocks */
	if (Blocks.Collide[old] == COLLIDE_NONE && !Blocks.CanDelete[old]) return;
	
	/* Ladders require an adjacent opaque block to attach to */
	if (block == BLOCK_LADDER) {
		cc_bool hasSupport = false;
		BlockID neighbor;
		
		/* Check all 4 horizontal neighbors for opaque blocks */
		if (World_Contains(pos.x - 1, pos.y, pos.z)) {
			neighbor = World_GetBlock(pos.x - 1, pos.y, pos.z);
			if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
		}
		if (World_Contains(pos.x + 1, pos.y, pos.z)) {
			neighbor = World_GetBlock(pos.x + 1, pos.y, pos.z);
			if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
		}
		if (World_Contains(pos.x, pos.y, pos.z - 1)) {
			neighbor = World_GetBlock(pos.x, pos.y, pos.z - 1);
			if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
		}
		if (World_Contains(pos.x, pos.y, pos.z + 1)) {
			neighbor = World_GetBlock(pos.x, pos.y, pos.z + 1);
			if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
		}
		
		if (!hasSupport) return; /* Can't place ladder without adjacent solid block */
	}
	
	/* Torch requires a wall or floor to attach to */
	/* For redstone torches, the clicked face determines which directional variant to place */
	if (block == BLOCK_TORCH || block == BLOCK_RED_ORE_TORCH || block == BLOCK_RED_ORE_TORCH_OFF) {
		Face clickedFace = Game_SelectedPos.closest;
		cc_bool hasSupport = false;
		BlockID neighbor;
		
		if (block == BLOCK_RED_ORE_TORCH) {
			/* Determine wall torch variant from the face the player clicked */
			/* Clicking a face means the attach block is on the opposite side of that face */
			switch (clickedFace) {
				case FACE_ZMIN: /* Clicked north face (-Z) - torch at z-1, attached to z+1 */
					if (World_Contains(pos.x, pos.y, pos.z + 1)) {
						neighbor = World_GetBlock(pos.x, pos.y, pos.z + 1);
						if (Blocks.Draw[neighbor] == DRAW_OPAQUE) {
							block = BLOCK_RED_TORCH_ON_S; hasSupport = true;
						}
					}
					break;
				case FACE_ZMAX: /* Clicked south face (+Z) - torch at z+1, attached to z-1 */
					if (World_Contains(pos.x, pos.y, pos.z - 1)) {
						neighbor = World_GetBlock(pos.x, pos.y, pos.z - 1);
						if (Blocks.Draw[neighbor] == DRAW_OPAQUE) {
							block = BLOCK_RED_TORCH_ON_N; hasSupport = true;
						}
					}
					break;
				case FACE_XMIN: /* Clicked west face (-X) - torch at x-1, attached to x+1 */
					if (World_Contains(pos.x + 1, pos.y, pos.z)) {
						neighbor = World_GetBlock(pos.x + 1, pos.y, pos.z);
						if (Blocks.Draw[neighbor] == DRAW_OPAQUE) {
							block = BLOCK_RED_TORCH_ON_E; hasSupport = true;
						}
					}
					break;
				case FACE_XMAX: /* Clicked east face (+X) - torch at x+1, attached to x-1 */
					if (World_Contains(pos.x - 1, pos.y, pos.z)) {
						neighbor = World_GetBlock(pos.x - 1, pos.y, pos.z);
						if (Blocks.Draw[neighbor] == DRAW_OPAQUE) {
							block = BLOCK_RED_TORCH_ON_W; hasSupport = true;
						}
					}
					break;
				case FACE_YMAX: /* Clicked top face - unmounted (free-standing) torch */
					if (World_Contains(pos.x, pos.y - 1, pos.z)) {
						neighbor = World_GetBlock(pos.x, pos.y - 1, pos.z);
						if (Blocks.Draw[neighbor] == DRAW_OPAQUE) {
							block = BLOCK_RED_TORCH_UNMOUNTED; hasSupport = true;
						}
					}
					break;
				case FACE_YMIN: /* Clicked bottom face - try ground attached, fallback to wall */
					if (World_Contains(pos.x, pos.y - 1, pos.z)) {
						neighbor = World_GetBlock(pos.x, pos.y - 1, pos.z);
						if (Blocks.Draw[neighbor] == DRAW_OPAQUE) {
							hasSupport = true; /* block stays BLOCK_RED_ORE_TORCH (ground attached) */
						}
					}
					break;
				default: break;
			}
			
			/* If the clicked face didn't provide support, try to find any support */
			if (!hasSupport) {
				/* Try walls (z+1, z-1, x+1, x-1), then ground (y-1), then unmounted */
				if (World_Contains(pos.x, pos.y, pos.z + 1)) {
					neighbor = World_GetBlock(pos.x, pos.y, pos.z + 1);
					if (Blocks.Draw[neighbor] == DRAW_OPAQUE) { block = BLOCK_RED_TORCH_ON_S; hasSupport = true; }
				}
				if (!hasSupport && World_Contains(pos.x, pos.y, pos.z - 1)) {
					neighbor = World_GetBlock(pos.x, pos.y, pos.z - 1);
					if (Blocks.Draw[neighbor] == DRAW_OPAQUE) { block = BLOCK_RED_TORCH_ON_N; hasSupport = true; }
				}
				if (!hasSupport && World_Contains(pos.x + 1, pos.y, pos.z)) {
					neighbor = World_GetBlock(pos.x + 1, pos.y, pos.z);
					if (Blocks.Draw[neighbor] == DRAW_OPAQUE) { block = BLOCK_RED_TORCH_ON_E; hasSupport = true; }
				}
				if (!hasSupport && World_Contains(pos.x - 1, pos.y, pos.z)) {
					neighbor = World_GetBlock(pos.x - 1, pos.y, pos.z);
					if (Blocks.Draw[neighbor] == DRAW_OPAQUE) { block = BLOCK_RED_TORCH_ON_W; hasSupport = true; }
				}
				if (!hasSupport && World_Contains(pos.x, pos.y - 1, pos.z)) {
					neighbor = World_GetBlock(pos.x, pos.y - 1, pos.z);
					if (Blocks.Draw[neighbor] == DRAW_OPAQUE) { block = BLOCK_RED_ORE_TORCH; hasSupport = true; }
				}
				/* If no support found, torch cannot be placed */
			}
		} else {
			/* Regular torches and unlit redstone torches: basic support check */
			if (World_Contains(pos.x - 1, pos.y, pos.z)) {
				neighbor = World_GetBlock(pos.x - 1, pos.y, pos.z);
				if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
			}
			if (!hasSupport && World_Contains(pos.x + 1, pos.y, pos.z)) {
				neighbor = World_GetBlock(pos.x + 1, pos.y, pos.z);
				if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
			}
			if (!hasSupport && World_Contains(pos.x, pos.y, pos.z - 1)) {
				neighbor = World_GetBlock(pos.x, pos.y, pos.z - 1);
				if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
			}
			if (!hasSupport && World_Contains(pos.x, pos.y, pos.z + 1)) {
				neighbor = World_GetBlock(pos.x, pos.y, pos.z + 1);
				if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
			}
			if (!hasSupport && World_Contains(pos.x, pos.y - 1, pos.z)) {
				neighbor = World_GetBlock(pos.x, pos.y - 1, pos.z);
				if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
			}
		}
		
		if (!hasSupport) return; /* Can't place torch without any support */
	}
	
	/* Button requires a wall to attach to (no ground/ceiling placement) */
	if (block == BLOCK_BUTTON) {
		cc_bool hasSupport = false;
		BlockID neighbor;
		
		/* Check all 4 horizontal neighbors for opaque blocks */
		if (World_Contains(pos.x, pos.y, pos.z + 1)) {
			neighbor = World_GetBlock(pos.x, pos.y, pos.z + 1);
			if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
		}
		if (!hasSupport && World_Contains(pos.x, pos.y, pos.z - 1)) {
			neighbor = World_GetBlock(pos.x, pos.y, pos.z - 1);
			if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
		}
		if (!hasSupport && World_Contains(pos.x + 1, pos.y, pos.z)) {
			neighbor = World_GetBlock(pos.x + 1, pos.y, pos.z);
			if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
		}
		if (!hasSupport && World_Contains(pos.x - 1, pos.y, pos.z)) {
			neighbor = World_GetBlock(pos.x - 1, pos.y, pos.z);
			if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
		}
		
		if (!hasSupport) return; /* Can't place button without any wall support */
	}
	
	/* Lever requires a wall to attach to (same as button) */
	if (block == BLOCK_LEVER) {
		cc_bool hasSupport = false;
		BlockID neighbor;
		
		if (World_Contains(pos.x, pos.y, pos.z + 1)) {
			neighbor = World_GetBlock(pos.x, pos.y, pos.z + 1);
			if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
		}
		if (!hasSupport && World_Contains(pos.x, pos.y, pos.z - 1)) {
			neighbor = World_GetBlock(pos.x, pos.y, pos.z - 1);
			if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
		}
		if (!hasSupport && World_Contains(pos.x + 1, pos.y, pos.z)) {
			neighbor = World_GetBlock(pos.x + 1, pos.y, pos.z);
			if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
		}
		if (!hasSupport && World_Contains(pos.x - 1, pos.y, pos.z)) {
			neighbor = World_GetBlock(pos.x - 1, pos.y, pos.z);
			if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
		}
		
		if (!hasSupport) return; /* Can't place lever without any wall support */
	}
	
	/* Red ore dust requires a solid block below to rest on */
	if (block == BLOCK_RED_ORE_DUST) {
		BlockID below;
		
		/* Check if block below exists and is solid */
		if (!World_Contains(pos.x, pos.y - 1, pos.z)) return;
		below = World_GetBlock(pos.x, pos.y - 1, pos.z);
		
		/* Can only place on solid opaque blocks */
		if (Blocks.Draw[below] != DRAW_OPAQUE) return;
	}
	
	/* Door placement requires space above for door top and determines orientation from player yaw */
	if (block == BLOCK_DOOR_NS_BOTTOM) {
		BlockID above, doorBottom, doorTop;
		float yaw;
		
		/* Check if space above is available */
		if (!World_Contains(pos.x, pos.y + 1, pos.z)) return;
		above = World_GetBlock(pos.x, pos.y + 1, pos.z);
		
		/* Can't place door if space above is occupied by non-replaceable block */
		if (Blocks.Draw[above] != DRAW_GAS && !Blocks.CanDelete[above]) return;
		
		/* Determine door orientation based on player's yaw */
		yaw = LocalPlayer_Instances[0].Base.Yaw;
		
		/* Normalize yaw to 0-360 degrees */
		while (yaw < 0) yaw += 360.0f;
		while (yaw >= 360.0f) yaw -= 360.0f;
		
		/* Choose NS vs EW door based on yaw quadrants:
		 * 315-45° (facing North/South): place NS door
		 * 45-135° (facing East): place EW door
		 * 135-225° (facing South/North): place NS door
		 * 225-315° (facing West): place EW door
		 */
		if ((yaw >= 315.0f || yaw < 45.0f) || (yaw >= 135.0f && yaw < 225.0f)) {
			/* Place NS door */
			doorBottom = BLOCK_DOOR_NS_BOTTOM;
			doorTop = BLOCK_DOOR_NS_TOP;
		} else {
			/* Place EW door */
			doorBottom = BLOCK_DOOR_EW_BOTTOM;
			doorTop = BLOCK_DOOR_EW_TOP;
		}
		
		block = doorBottom; /* Update block to place the correct orientation */
	}
	
	/* Iron Door placement: requires space above, determines orientation from yaw */
	if (block == BLOCK_IRON_DOOR) {
		BlockID above, doorBottom, doorTop;
		float yaw;
		
		if (!World_Contains(pos.x, pos.y + 1, pos.z)) return;
		above = World_GetBlock(pos.x, pos.y + 1, pos.z);
		if (Blocks.Draw[above] != DRAW_GAS && !Blocks.CanDelete[above]) return;
		
		yaw = LocalPlayer_Instances[0].Base.Yaw;
		while (yaw < 0) yaw += 360.0f;
		while (yaw >= 360.0f) yaw -= 360.0f;
		
		if ((yaw >= 315.0f || yaw < 45.0f) || (yaw >= 135.0f && yaw < 225.0f)) {
			doorBottom = BLOCK_IRON_DOOR; /* NS */
		} else {
			doorBottom = BLOCK_IRON_DOOR_EW_BOTTOM; /* EW */
		}
		
		block = doorBottom;
	}

	/* Chest placement: check for adjacent chests / prevent placing next to double chest */
	if (block == BLOCK_CHEST) {
		BlockID nx, nz_neg, nz_pos, nx_neg;
		int adjChestX = 0, adjChestZ = 0; /* position offset of adjacent single chest */
		int hasAdjChest = 0;
		
		/* Check +X neighbor */
		if (World_Contains(pos.x + 1, pos.y, pos.z)) {
			nx = World_GetBlock(pos.x + 1, pos.y, pos.z);
			if (nx >= BLOCK_DCHEST_S_L && nx <= BLOCK_DCHEST_W_R) return; /* next to double chest */
			if (nx == BLOCK_CHEST) { adjChestX = 1; hasAdjChest = 1; }
		}
		/* Check -X neighbor */
		if (World_Contains(pos.x - 1, pos.y, pos.z)) {
			nx_neg = World_GetBlock(pos.x - 1, pos.y, pos.z);
			if (nx_neg >= BLOCK_DCHEST_S_L && nx_neg <= BLOCK_DCHEST_W_R) return;
			if (nx_neg == BLOCK_CHEST) { adjChestX = -1; hasAdjChest = 1; }
		}
		/* Check +Z neighbor */
		if (World_Contains(pos.x, pos.y, pos.z + 1)) {
			nz_pos = World_GetBlock(pos.x, pos.y, pos.z + 1);
			if (nz_pos >= BLOCK_DCHEST_S_L && nz_pos <= BLOCK_DCHEST_W_R) return;
			if (nz_pos == BLOCK_CHEST) { adjChestZ = 1; hasAdjChest = 1; }
		}
		/* Check -Z neighbor */
		if (World_Contains(pos.x, pos.y, pos.z - 1)) {
			nz_neg = World_GetBlock(pos.x, pos.y, pos.z - 1);
			if (nz_neg >= BLOCK_DCHEST_S_L && nz_neg <= BLOCK_DCHEST_W_R) return;
			if (nz_neg == BLOCK_CHEST) { adjChestZ = -1; hasAdjChest = 1; }
		}
	}

	if (!CheckIsFree(block)) return;

	Game_ChangeBlock(pos.x, pos.y, pos.z, block);
	Event_RaiseBlock(&UserEvents.BlockChanged, pos, old, block);

	/* In survival mode, consume one block from the hotbar stack */
	if (Game_SurvivalMode && Hotbar_SelectedItem == ITEM_NONE) {
		int cnt = Hotbar_GetCount(Inventory.SelectedIndex);
		if (cnt > 1) {
			Hotbar_SetCount(Inventory.SelectedIndex, cnt - 1);
		} else {
			Inventory_Set(Inventory.SelectedIndex, BLOCK_AIR);
			Hotbar_SetCount(Inventory.SelectedIndex, 0);
		}
		Event_RaiseVoid(&UserEvents.HeldBlockChanged);
	}
	
	/* After placing chest, form double chest if adjacent to single chest */
	if (block == BLOCK_CHEST) {
		int adjX = 0, adjZ = 0, hasAdj = 0;
		BlockID nb;
		int otherX, otherZ;
		BlockID newBlock, partnerBlock;
		int blocksSide1, blocksSide2;
		BlockID b;
		
		/* Find first adjacent single chest */
		if (World_Contains(pos.x + 1, pos.y, pos.z)) {
			nb = World_GetBlock(pos.x + 1, pos.y, pos.z);
			if (nb == BLOCK_CHEST) { adjX = 1; hasAdj = 1; }
		}
		if (!hasAdj && World_Contains(pos.x - 1, pos.y, pos.z)) {
			nb = World_GetBlock(pos.x - 1, pos.y, pos.z);
			if (nb == BLOCK_CHEST) { adjX = -1; hasAdj = 1; }
		}
		if (!hasAdj && World_Contains(pos.x, pos.y, pos.z + 1)) {
			nb = World_GetBlock(pos.x, pos.y, pos.z + 1);
			if (nb == BLOCK_CHEST) { adjZ = 1; hasAdj = 1; }
		}
		if (!hasAdj && World_Contains(pos.x, pos.y, pos.z - 1)) {
			nb = World_GetBlock(pos.x, pos.y, pos.z - 1);
			if (nb == BLOCK_CHEST) { adjZ = -1; hasAdj = 1; }
		}
		
		if (hasAdj) {
			otherX = pos.x + adjX;
			otherZ = pos.z + adjZ;
			newBlock = BLOCK_AIR;
			partnerBlock = BLOCK_AIR;
			blocksSide1 = 0;
			blocksSide2 = 0;
			
			if (adjX != 0) {
				/* Pair along X axis - front can face +Z or -Z */
				/* Count opaque blocks on +Z side of both chests */
				if (World_Contains(pos.x, pos.y, pos.z + 1)) {
					b = World_GetBlock(pos.x, pos.y, pos.z + 1);
					if (Blocks.Draw[b] == DRAW_OPAQUE) blocksSide1++;
				}
				if (World_Contains(otherX, pos.y, pos.z + 1)) {
					b = World_GetBlock(otherX, pos.y, pos.z + 1);
					if (Blocks.Draw[b] == DRAW_OPAQUE) blocksSide1++;
				}
				/* Count opaque blocks on -Z side of both chests */
				if (World_Contains(pos.x, pos.y, pos.z - 1)) {
					b = World_GetBlock(pos.x, pos.y, pos.z - 1);
					if (Blocks.Draw[b] == DRAW_OPAQUE) blocksSide2++;
				}
				if (World_Contains(otherX, pos.y, pos.z - 1)) {
					b = World_GetBlock(otherX, pos.y, pos.z - 1);
					if (Blocks.Draw[b] == DRAW_OPAQUE) blocksSide2++;
				}
				
				/* blocksSide1 = +Z count, blocksSide2 = -Z count */
				/* Face the direction with fewer blocks */
				if (blocksSide2 <= blocksSide1) {
					/* Face -Z (North): fewer blocks on -Z side */
					if (adjX > 0) {
						newBlock = BLOCK_DCHEST_N_L;
						partnerBlock = BLOCK_DCHEST_N_R;
					} else {
						newBlock = BLOCK_DCHEST_N_R;
						partnerBlock = BLOCK_DCHEST_N_L;
					}
				} else {
					/* Face +Z (South): fewer blocks on +Z side */
					if (adjX > 0) {
						newBlock = BLOCK_DCHEST_S_R;
						partnerBlock = BLOCK_DCHEST_S_L;
					} else {
						newBlock = BLOCK_DCHEST_S_L;
						partnerBlock = BLOCK_DCHEST_S_R;
					}
				}
			} else {
				/* Pair along Z axis - front can face +X or -X */
				/* Count opaque blocks on +X side of both chests */
				if (World_Contains(pos.x + 1, pos.y, pos.z)) {
					b = World_GetBlock(pos.x + 1, pos.y, pos.z);
					if (Blocks.Draw[b] == DRAW_OPAQUE) blocksSide1++;
				}
				if (World_Contains(pos.x + 1, pos.y, otherZ)) {
					b = World_GetBlock(pos.x + 1, pos.y, otherZ);
					if (Blocks.Draw[b] == DRAW_OPAQUE) blocksSide1++;
				}
				/* Count opaque blocks on -X side of both chests */
				if (World_Contains(pos.x - 1, pos.y, pos.z)) {
					b = World_GetBlock(pos.x - 1, pos.y, pos.z);
					if (Blocks.Draw[b] == DRAW_OPAQUE) blocksSide2++;
				}
				if (World_Contains(pos.x - 1, pos.y, otherZ)) {
					b = World_GetBlock(pos.x - 1, pos.y, otherZ);
					if (Blocks.Draw[b] == DRAW_OPAQUE) blocksSide2++;
				}
				
				/* blocksSide1 = +X count, blocksSide2 = -X count */
				/* Face the direction with fewer blocks */
				if (blocksSide2 <= blocksSide1) {
					/* Face -X (West): fewer blocks on -X side */
					if (adjZ > 0) {
						newBlock = BLOCK_DCHEST_W_R;
						partnerBlock = BLOCK_DCHEST_W_L;
					} else {
						newBlock = BLOCK_DCHEST_W_L;
						partnerBlock = BLOCK_DCHEST_W_R;
					}
				} else {
					/* Face +X (East): fewer blocks on +X side */
					if (adjZ > 0) {
						newBlock = BLOCK_DCHEST_E_L;
						partnerBlock = BLOCK_DCHEST_E_R;
					} else {
						newBlock = BLOCK_DCHEST_E_R;
						partnerBlock = BLOCK_DCHEST_E_L;
					}
				}
			}
			
			/* Convert both blocks to double chest */
			Game_UpdateBlock(pos.x, pos.y, pos.z, newBlock);
			Game_UpdateBlock(otherX, pos.y, otherZ, partnerBlock);
		}
	}
	
	/* Auto-place door top when placing door bottom */
	if (block == BLOCK_DOOR_NS_BOTTOM || block == BLOCK_DOOR_EW_BOTTOM) {
		IVec3 topPos;
		BlockID aboveBlock, doorTop;
		
		/* Determine which top block to place */
		doorTop = (block == BLOCK_DOOR_NS_BOTTOM) ? BLOCK_DOOR_NS_TOP : BLOCK_DOOR_EW_TOP;
		
		topPos.x = pos.x;
		topPos.y = pos.y + 1;
		topPos.z = pos.z;
		
		if (World_Contains(topPos.x, topPos.y, topPos.z)) {
			aboveBlock = World_GetBlock(topPos.x, topPos.y, topPos.z);
			Game_ChangeBlock(topPos.x, topPos.y, topPos.z, doorTop);
			Event_RaiseBlock(&UserEvents.BlockChanged, topPos, aboveBlock, doorTop);
		}
	}
	
	/* Auto-place iron door top when placing iron door bottom */
	if (block == BLOCK_IRON_DOOR || block == BLOCK_IRON_DOOR_EW_BOTTOM) {
		IVec3 topPos;
		BlockID aboveBlock, doorTop;
		
		doorTop = (block == BLOCK_IRON_DOOR) ? BLOCK_IRON_DOOR_NS_TOP : BLOCK_IRON_DOOR_EW_TOP;
		
		topPos.x = pos.x;
		topPos.y = pos.y + 1;
		topPos.z = pos.z;
		
		if (World_Contains(topPos.x, topPos.y, topPos.z)) {
			aboveBlock = World_GetBlock(topPos.x, topPos.y, topPos.z);
			Game_ChangeBlock(topPos.x, topPos.y, topPos.z, doorTop);
			Event_RaiseBlock(&UserEvents.BlockChanged, topPos, aboveBlock, doorTop);
		}
	}
}

static void InputHandler_PickBlock(void) {
	IVec3 pos;
	BlockID cur;
	/* In survival mode, pick block is disabled unless cheats are on */
	if (Game_SurvivalMode && !Player_CheatsEnabled) return;
	pos = Game_SelectedPos.pos;
	if (!World_Contains(pos.x, pos.y, pos.z)) return;

	cur = World_GetBlock(pos.x, pos.y, pos.z);
	if (Blocks.Draw[cur] == DRAW_GAS) return;
	if (!(Blocks.CanPlace[cur] || Blocks.CanDelete[cur])) return;
	Inventory_PickBlock(cur);
}

#ifdef CC_BUILD_TOUCH
static cc_bool AnyBlockTouches(void);
#endif
void InputHandler_Tick(float delta) {
	cc_bool left, middle, right;
	int newStage;

	/* Per-frame breaking progress for survival mode (before delta gate) */
	left = input_buttonsDown[MOUSE_LEFT];
	if (Game_SurvivalMode && left && breaking_active) {
		IVec3 curPos = Game_SelectedPos.pos;

		if (!Game_SelectedPos.valid ||
			curPos.x != breaking_pos.x || curPos.y != breaking_pos.y || curPos.z != breaking_pos.z) {
			BlockBreaking_Reset();
		} else if (World_GetBlock(curPos.x, curPos.y, curPos.z) != breaking_block) {
			BlockBreaking_Reset();
		} else {
			breaking_progress += delta / breaking_totalTime;

			newStage = (int)(breaking_progress * 10.0f);
			if (newStage > 9) newStage = 9;
			breaking_crackStage = newStage;

			/* Play step sound at a fixed interval (~5 per second) */
			breaking_soundTimer += delta;
			if (breaking_soundTimer >= 0.20f) {
				cc_uint8 sndType = Blocks.StepSounds[breaking_block];
				int vol = Audio_SoundsVolume;
				breaking_soundTimer -= 0.20f;
				if (sndType == SOUND_GRASS || sndType == SOUND_SAND || sndType == SOUND_GRAVEL)
					vol = vol / 2;
				Audio_PlayStepSoundRate(sndType, 50, vol);
			}

			/* Re-trigger swing animation periodically (dig anim lasts 0.233s, 1.5x faster) */
			breaking_swingTimer += delta;
			if (breaking_swingTimer >= 0.233f) {
				breaking_swingTimer -= 0.233f;
				HeldBlockRenderer_ClickAnim(true);
			}

			if (breaking_progress >= 1.0f) {
				BreakBlockNow(breaking_pos, breaking_block);
				BlockBreaking_Reset();
			}
		}
	}

	input_deltaAcc += delta;
	if (Gui.InputGrab) return;

	/* Only tick 4 times per second when held down */
	if (input_deltaAcc < 0.2495f) return;
	/* NOTE: 0.2495 is used instead of 0.25 to produce delta time */
	/*  values slightly closer to the old code which measured */
	/*  elapsed time using DateTime_CurrentUTC_MS() instead */
	input_deltaAcc  = 0;

	left   = input_buttonsDown[MOUSE_LEFT];
	middle = input_buttonsDown[MOUSE_MIDDLE];
	right  = input_buttonsDown[MOUSE_RIGHT];

#ifdef CC_BUILD_TOUCH
	if (Input_TouchMode) {
		left   = (Input_HoldMode == INPUT_MODE_DELETE) && AnyBlockTouches();
		right  = (Input_HoldMode == INPUT_MODE_PLACE)  && AnyBlockTouches();
		middle = false;
	}
#endif

	if (Server.SupportsPlayerClick) {
		input_pickingId = -1;
		if (left)   MouseStateUpdate(MOUSE_LEFT,   true);
		if (right)  MouseStateUpdate(MOUSE_RIGHT,  true);
		if (middle) MouseStateUpdate(MOUSE_MIDDLE, true);
	}

	if (left) {
		if (!Game_SurvivalMode) {
			InputHandler_DeleteBlock();
		} else if (!breaking_active) {
			InputHandler_DeleteBlock();
		}
	} else if (right) {
		InputHandler_PlaceBlock();
	} else if (middle) {
		InputHandler_PickBlock();
	}
}


/*########################################################################################################################*
*------------------------------------------------------Touch support------------------------------------------------------*
*#########################################################################################################################*/
#ifdef CC_BUILD_TOUCH
static cc_bool AnyBlockTouches(void) {
	int i;
	for (i = 0; i < Pointers_Count; i++) {
		if (!(touches[i].type & TOUCH_TYPE_BLOCKS)) continue;

		/* Touch might be an 'all' type - remove 'gui' type */
		touches[i].type &= TOUCH_TYPE_BLOCKS | TOUCH_TYPE_CAMERA;
		return true;
	}
	return false;
}

/* Quickly tapping should trigger a block place/delete */
static void CheckBlockTap(int i) {
	int btn, pressed;
	if (Game.Time > touches[i].start + 0.25) return;
	if (touches[i].type != TOUCH_TYPE_ALL)   return;

	if (Input_TapMode == INPUT_MODE_PLACE) {
		btn = MOUSE_RIGHT;
	} else if (Input_TapMode == INPUT_MODE_DELETE) {
		btn = MOUSE_LEFT;
	} else { return; }

	pressed = input_buttonsDown[btn];
	MouseStatePress(btn);

	if (btn == MOUSE_LEFT) { 
		InputHandler_DeleteBlock();
	} else { 
		InputHandler_PlaceBlock();
	}
	if (!pressed) MouseStateRelease(btn);
}
#endif


/*########################################################################################################################*
*-----------------------------------------------------Input helpers-------------------------------------------------------*
*#########################################################################################################################*/
static cc_bool InputHandler_IsShutdown(int key) {
	if (key == CCKEY_F4 && Input_IsAltPressed()) return true;

	/* On macOS, Cmd+Q should also end the process */
#ifdef CC_BUILD_DARWIN
	return key == 'Q' && Input_IsWinPressed();
#else
	return false;
#endif
}

static void InputHandler_Toggle(int key, cc_bool* target, const char* enableMsg, const char* disableMsg) {
	*target = !(*target);
	if (*target) {
		Chat_Add2("%c. &ePress &a%c &eto disable.",   enableMsg,  Input_DisplayNames[key]);
	} else {
		Chat_Add2("%c. &ePress &a%c &eto re-enable.", disableMsg, Input_DisplayNames[key]);
	}
}

cc_bool InputHandler_SetFOV(int fov) {
	struct HacksComp* h = &Entities.CurPlayer->Hacks;
	if (!h->Enabled || !h->CanUseThirdPerson) return false;

	Camera.ZoomFov = fov;
	Camera_SetFov(fov);
	return true;
}

cc_bool Input_HandleMouseWheel(float delta) {
	struct HacksComp* h;
	cc_bool hotbar;

	hotbar = Input_IsAltPressed() || Input_IsCtrlPressed() || Input_IsShiftPressed();
	if (!hotbar && Camera.Active->Zoom(delta))   return true;
	if (!Bind_IsTriggered[BIND_ZOOM_SCROLL]) return false;

	h = &Entities.CurPlayer->Hacks;
	if (!h->Enabled || !h->CanUseThirdPerson) return false;

	if (input_fovIndex == -1.0f) input_fovIndex = (float)Camera.ZoomFov;
	input_fovIndex -= delta * 5.0f;

	Math_Clamp(input_fovIndex, 1.0f, Camera.DefaultFov);
	return InputHandler_SetFOV((int)input_fovIndex);
}

static void InputHandler_CheckZoomFov(void* obj) {
	struct HacksComp* h = &Entities.CurPlayer->Hacks;
	if (!h->Enabled || !h->CanUseThirdPerson) Camera_SetFov(Camera.DefaultFov);
}


static cc_bool BindTriggered_DeleteBlock(int key, struct InputDevice* device) {
	if (Gui.InputGrab) return false;
	
	MouseStatePress(MOUSE_LEFT);
	InputHandler_DeleteBlock();
	return true;
}

static void Player_ShootArrow(void);
static cc_bool BindTriggered_PlaceBlock(int key, struct InputDevice* device) {
	if (Gui.InputGrab) return false;

	/* Shoot arrow when holding a bow */
	if (Hotbar_SelectedItem == 41) {
		/* In survival mode, require and consume 1 arrow */
		if (Game_SurvivalMode && !SurvInv_ConsumeItem(ITEM_ARROW)) return true;
		Player_ShootArrow();
		return true;
	}

	/* Eat food when holding pork items */
	if (Hotbar_SelectedItem == ITEM_RAW_PORK || Hotbar_SelectedItem == ITEM_COOKED_PORK) {
		if (Player_Health < PLAYER_MAX_HEALTH) {
			int heal = (Hotbar_SelectedItem == ITEM_RAW_PORK) ? 3 : 8;
			int idx  = Inventory.SelectedIndex;
			int cnt  = Hotbar_GetCount(idx);
			if (cnt > 1) {
				Hotbar_SetCount(idx, cnt - 1);
			} else {
				Hotbar_SetItem(idx, ITEM_NONE);
				Hotbar_SetCount(idx, 0);
			}
			Event_RaiseVoid(&UserEvents.HeldBlockChanged);
			Player_Health += heal;
			if (Player_Health > PLAYER_MAX_HEALTH) Player_Health = PLAYER_MAX_HEALTH;
		}
		return true;
	}

	/* Eat mushroom stew: heal 5 HP, leave behind the bowl */
	if (Hotbar_SelectedItem == ITEM_MUSHROOM_STEW) {
		if (Player_Health < PLAYER_MAX_HEALTH) {
			int idx = Inventory.SelectedIndex;
			/* Replace stew with bowl */
			Hotbar_SetItem(idx, ITEM_BOWL);
			Hotbar_SetCount(idx, 1);
			Inventory_Set(idx, BLOCK_AIR);
			Event_RaiseVoid(&UserEvents.HeldBlockChanged);
			Player_Health += 5;
			if (Player_Health > PLAYER_MAX_HEALTH) Player_Health = PLAYER_MAX_HEALTH;
		}
		return true;
	}

	/* Eat bread: heal 5 HP, consume one */
	if (Hotbar_SelectedItem == ITEM_BREAD) {
		if (Player_Health < PLAYER_MAX_HEALTH) {
			int idx = Inventory.SelectedIndex;
			int cnt = Hotbar_GetCount(idx);
			if (cnt > 1) {
				Hotbar_SetCount(idx, cnt - 1);
			} else {
				Hotbar_SetItem(idx, ITEM_NONE);
				Hotbar_SetCount(idx, 0);
			}
			Event_RaiseVoid(&UserEvents.HeldBlockChanged);
			Player_Health += 5;
			if (Player_Health > PLAYER_MAX_HEALTH) Player_Health = PLAYER_MAX_HEALTH;
		}
		return true;
	}

	MouseStatePress(MOUSE_RIGHT);
	InputHandler_PlaceBlock();
	return true;
}

static cc_bool BindTriggered_PickBlock(int key, struct InputDevice* device) {
	if (Gui.InputGrab) return false;
	
	MouseStatePress(MOUSE_MIDDLE);
	InputHandler_PickBlock();
	return true;
}

static void BindReleased_DeleteBlock(int key, struct InputDevice* device) {
	MouseStateRelease(MOUSE_LEFT);
	BlockBreaking_Reset();
}

static void BindReleased_PlaceBlock(int key, struct InputDevice* device) {
	MouseStateRelease(MOUSE_RIGHT);
}

static void BindReleased_PickBlock(int key, struct InputDevice* device) {
	MouseStateRelease(MOUSE_MIDDLE);
}


static cc_bool BindTriggered_HideFPS(int key, struct InputDevice* device) {
	if (Gui.InputGrab) return false;
	
	Gui.ShowFPS = !Gui.ShowFPS;
	return true;
}

static cc_bool BindTriggered_Fullscreen(int key, struct InputDevice* device) {
	if (Gui.InputGrab) return false;
	
	Game_ToggleFullscreen();
	return true;
}

static cc_bool BindTriggered_Fog(int key, struct InputDevice* device) {
	if (Gui.InputGrab) return false;
	
	Game_CycleViewDistance();
	return true;
}


static cc_bool BindTriggered_HideGUI(int key, struct InputDevice* device) {
	if (Gui.InputGrab) return false;
	
	Game_HideGui = !Game_HideGui;
	return true;
}

static cc_bool BindTriggered_SmoothCamera(int key, struct InputDevice* device) {
	if (Gui.InputGrab) return false;
	
	InputHandler_Toggle(key, &Camera.Smooth,
		"  &eSmooth camera is &aenabled",
		"  &eSmooth camera is &cdisabled");
	return true;
}

static cc_bool BindTriggered_AxisLines(int key, struct InputDevice* device) {
	if (Gui.InputGrab) return false;
	
	InputHandler_Toggle(key, &AxisLinesRenderer_Enabled,
		"  &eAxis lines (&4X&e, &2Y&e, &1Z&e) now show",
		"  &eAxis lines no longer show");
	return true;
} 

static cc_bool BindTriggered_AutoRotate(int key, struct InputDevice* device) {
	if (Gui.InputGrab) return false;
	
	InputHandler_Toggle(key, &AutoRotate_Enabled,
		"  &eAuto rotate is &aenabled",
		"  &eAuto rotate is &cdisabled");
	return true;
}

static cc_bool BindTriggered_ThirdPerson(int key, struct InputDevice* device) {
	if (Gui.InputGrab) return false;
	
	Camera_CycleActive();
	return true;
}

static cc_bool BindTriggered_DropBlock(int key, struct InputDevice* device) {
	Vec3 pos;
	BlockID block;
	int slot, itemId, count;
	float yawRad, tossSpeed;
	cc_bool dropAll;

	if (Gui.InputGrab) return false;
	if (!Inventory_CheckChangeSelected()) return false;

	itemId  = Hotbar_SelectedItem;
	block   = Inventory_SelectedBlock;
	dropAll = Input_IsShiftPressed();

	/* Nothing to drop */
	if (itemId == ITEM_NONE && block == BLOCK_AIR) return true;

	/* Find free dropped item slot (evict oldest if full) */
	slot = DropItem_FindFreeSlot();
	if (slot == -1) slot = DropItem_EvictOldest();
	if (slot == -1) return true;

	/* Spawn at player eye position */
	pos = Entities.CurPlayer->Base.Position;
	pos.y += Entity_GetEyeHeight(&Entities.CurPlayer->Base);

	if (itemId != ITEM_NONE) {
		/* Drop item from hotbar */
		count = Hotbar_GetCount(Inventory.SelectedIndex);
		if (count < 1) count = 1;

		if (dropAll || count <= 1) {
			/* Shift+Q or last item: drop everything */
			DropItem_Spawn(slot, pos, BLOCK_AIR, true, itemId);
			droppedItemCount[slot] = count;
			Hotbar_SetItem(Inventory.SelectedIndex, ITEM_NONE);
			Hotbar_SetCount(Inventory.SelectedIndex, 0);
		} else {
			/* Q: drop 1, keep the rest */
			DropItem_Spawn(slot, pos, BLOCK_AIR, true, itemId);
			droppedItemCount[slot] = 1;
			Hotbar_SetCount(Inventory.SelectedIndex, count - 1);
		}
	} else {
		/* Drop block from inventory */
		count = Hotbar_GetCount(Inventory.SelectedIndex);
		if (count < 1) count = 1;

		if (dropAll || count <= 1) {
			/* Shift+Q or last block: drop everything */
			DropItem_Spawn(slot, pos, block, false, 0);
			droppedItemCount[slot] = count;
			Inventory_Set(Inventory.SelectedIndex, BLOCK_AIR);
			Hotbar_SetCount(Inventory.SelectedIndex, 0);
		} else {
			/* Q: drop 1, keep the rest */
			DropItem_Spawn(slot, pos, block, false, 0);
			droppedItemCount[slot] = 1;
			Hotbar_SetCount(Inventory.SelectedIndex, count - 1);
		}
	}

	/* Give initial forward toss velocity based on player's look direction */
	yawRad   = Entities.CurPlayer->Base.Yaw * MATH_DEG2RAD;
	tossSpeed = 0.25f;
	droppedItemVelocityX[slot] = Math_SinF(yawRad) * tossSpeed;
	droppedItemVelocityZ[slot] = -Math_CosF(yawRad) * tossSpeed;
	droppedItemVelocityY[slot] = 0.12f; /* slight upward arc */

	Event_RaiseVoid(&UserEvents.HeldBlockChanged);
	return true;
}

/* Try to consume 1 of the specified item from inventory. Returns true if successful. */
static cc_bool SurvInv_ConsumeItem(int itemId) {
	int i;
	/* Check hotbar first */
	for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR; i++) {
		if (Hotbar_GetItem(i) == itemId && Inventory_Get(i) == BLOCK_AIR && Hotbar_GetCount(i) > 0) {
			Hotbar_SetCount(i, Hotbar_GetCount(i) - 1);
			if (Hotbar_GetCount(i) <= 0) {
				Hotbar_SetItem(i, ITEM_NONE);
				Hotbar_SetCount(i, 0);
			}
			Event_RaiseVoid(&UserEvents.HeldBlockChanged);
			return true;
		}
	}
	/* Check main inventory */
	for (i = 0; i < 27; i++) {
		if (SurvInv_Main[i].itemId == itemId && SurvInv_Main[i].block == BLOCK_AIR && SurvInv_Main[i].count > 0) {
			SurvInv_Main[i].count--;
			if (SurvInv_Main[i].count <= 0) {
				SurvInv_Main[i].itemId = ITEM_NONE;
				SurvInv_Main[i].count  = 0;
			}
			return true;
		}
	}
	return false;
}

/* Add an item/block to the player's inventory (hotbar first, then main inventory) */
void SurvInv_AddItem(BlockID block, int itemId, int count) {
	int i, canFit, remaining;
	cc_bool hotbarChanged = false;
	remaining = count;

	if (itemId != ITEM_NONE && block == BLOCK_AIR) {
		/* Item pickup */
		int maxStack = Item_MaxStackSize(itemId);
		/* Stack with existing same item in hotbar */
		for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR && remaining > 0; i++) {
			if (Hotbar_GetItem(i) == itemId && Inventory_Get(i) == BLOCK_AIR &&
				Hotbar_GetCount(i) < maxStack) {
				canFit = maxStack - Hotbar_GetCount(i);
				if (canFit > remaining) canFit = remaining;
				Hotbar_SetCount(i, Hotbar_GetCount(i) + canFit);
				remaining -= canFit;
				hotbarChanged = true;
			}
		}
		/* Stack in main inventory */
		for (i = 0; i < 27 && remaining > 0; i++) {
			if (SurvInv_Main[i].itemId == itemId && SurvInv_Main[i].block == BLOCK_AIR &&
				SurvInv_Main[i].count < maxStack) {
				canFit = maxStack - SurvInv_Main[i].count;
				if (canFit > remaining) canFit = remaining;
				SurvInv_Main[i].count += canFit;
				remaining -= canFit;
			}
		}
		/* Empty hotbar slot */
		for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR && remaining > 0; i++) {
			if (Inventory_Get(i) != BLOCK_AIR || Hotbar_GetItem(i) != ITEM_NONE) continue;
			canFit = remaining;
			if (canFit > maxStack) canFit = maxStack;
			Hotbar_SetItem(i, itemId);
			Hotbar_SetCount(i, canFit);
			remaining -= canFit;
			hotbarChanged = true;
		}
		/* Empty main inventory slot */
		for (i = 0; i < 27 && remaining > 0; i++) {
			if (SurvInv_Main[i].block != BLOCK_AIR || SurvInv_Main[i].itemId != ITEM_NONE) continue;
			canFit = remaining;
			if (canFit > maxStack) canFit = maxStack;
			SurvInv_Main[i].itemId = itemId;
			SurvInv_Main[i].count  = canFit;
			remaining -= canFit;
		}
	} else {
		/* Block pickup */
		int blockMaxStack = Block_MaxStackSize(block);
		/* Stack with existing same block in hotbar */
		for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR && remaining > 0; i++) {
			if (Inventory_Get(i) == block && Hotbar_GetItem(i) == ITEM_NONE &&
				Hotbar_GetCount(i) < blockMaxStack) {
				canFit = blockMaxStack - Hotbar_GetCount(i);
				if (canFit > remaining) canFit = remaining;
				Hotbar_SetCount(i, Hotbar_GetCount(i) + canFit);
				remaining -= canFit;
				hotbarChanged = true;
			}
		}
		/* Stack in main inventory */
		for (i = 0; i < 27 && remaining > 0; i++) {
			if (SurvInv_Main[i].block == block && SurvInv_Main[i].itemId == ITEM_NONE &&
				SurvInv_Main[i].count < blockMaxStack) {
				canFit = blockMaxStack - SurvInv_Main[i].count;
				if (canFit > remaining) canFit = remaining;
				SurvInv_Main[i].count += canFit;
				remaining -= canFit;
			}
		}
		/* Empty hotbar slot */
		for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR && remaining > 0; i++) {
			if (Inventory_Get(i) != BLOCK_AIR || Hotbar_GetItem(i) != ITEM_NONE) continue;
			canFit = remaining;
			if (canFit > blockMaxStack) canFit = blockMaxStack;
			Inventory_Set(i, block);
			Hotbar_SetCount(i, canFit);
			remaining -= canFit;
			hotbarChanged = true;
		}
		/* Empty main inventory slot */
		for (i = 0; i < 27 && remaining > 0; i++) {
			if (SurvInv_Main[i].block != BLOCK_AIR || SurvInv_Main[i].itemId != ITEM_NONE) continue;
			canFit = remaining;
			if (canFit > blockMaxStack) canFit = blockMaxStack;
			SurvInv_Main[i].block = block;
			SurvInv_Main[i].count = canFit;
			remaining -= canFit;
		}
	}

	if (remaining < count) {
		Audio_PlayDigSoundRate(SOUND_PICKUP, 80 + Random_Next(&mob_rng, 41));
		if (hotbarChanged) Event_RaiseVoid(&UserEvents.HeldBlockChanged);
	}
}

/* Drop an item/block from the inventory screen held cursor */
void SurvInv_DropHeldItem(BlockID block, int itemId, int count) {
	Vec3 pos;
	int slot;
	float yawRad, tossSpeed;

	if (count < 1) return;

	slot = DropItem_FindFreeSlot();
	if (slot == -1) slot = DropItem_EvictOldest();
	if (slot == -1) return;

	pos = Entities.CurPlayer->Base.Position;
	pos.y += Entity_GetEyeHeight(&Entities.CurPlayer->Base);

	if (itemId != ITEM_NONE) {
		DropItem_Spawn(slot, pos, BLOCK_AIR, true, itemId);
	} else {
		DropItem_Spawn(slot, pos, block, false, 0);
	}
	droppedItemCount[slot] = count;

	yawRad   = Entities.CurPlayer->Base.Yaw * MATH_DEG2RAD;
	tossSpeed = 0.25f;
	droppedItemVelocityX[slot] = Math_SinF(yawRad) * tossSpeed;
	droppedItemVelocityZ[slot] = -Math_CosF(yawRad) * tossSpeed;
	droppedItemVelocityY[slot] = 0.12f;
}

static cc_bool BindTriggered_DeleteItem(int key, struct InputDevice* device) {
	if (Gui.InputGrab) return false;
	if (!Inventory_CheckChangeSelected()) return false;

	/* Delete item from hotbar if present */
	if (Hotbar_SelectedItem != ITEM_NONE) {
		Hotbar_SetItem(Inventory.SelectedIndex, ITEM_NONE);
		Hotbar_SetCount(Inventory.SelectedIndex, 0);
		Event_RaiseVoid(&UserEvents.HeldBlockChanged);
		return true;
	}

	if (Inventory_SelectedBlock == BLOCK_AIR) return true;

	/* Just delete the block, no entity spawn */
	Inventory_Set(Inventory.SelectedIndex, BLOCK_AIR);
	Hotbar_SetCount(Inventory.SelectedIndex, 0);
	Event_RaiseVoid(&UserEvents.HeldBlockChanged);
	return true;
}

static cc_bool BindTriggered_DropItemSprite(int key, struct InputDevice* device) {
	struct Screen* s;
	/* Item menu requires cheats in any mode */
	if (!Player_CheatsEnabled) return false;
	s = Gui_GetScreen(GUI_PRIORITY_INVENTORY);
	if (s) {
		Gui_Remove(s);
	} else if (!Gui.InputGrab) {
		ItemInventoryScreen_Show();
	}
	return true;
}

static cc_bool BindTriggered_IDOverlay(int key, struct InputDevice* device) {
	struct Screen* s = Gui_GetScreen(GUI_PRIORITY_TEXIDS);
	if (s) {
		Gui_Remove(s);
	} else {
		TexIdsOverlay_Show();
	}
	return true;
}

static cc_bool BindTriggered_BreakLiquids(int key, struct InputDevice* device) {
	if (Gui.InputGrab) return false;
	
	InputHandler_Toggle(key, &Game_BreakableLiquids,
		"  &eBreakable liquids is &aenabled",
		"  &eBreakable liquids is &cdisabled");
	return true;
}

static cc_bool BindTriggered_SpawnMob(int key, struct InputDevice* device);
/* forward declaration - SpawnRandomMob is defined later */

static void HandleHotkeyDown(int key) {
	struct HotkeyData* hkey;
	cc_string text;
	int i = Hotkeys_FindPartial(key);

	if (i == -1) return;
	hkey = &HotkeysList[i];
	text = StringsBuffer_UNSAFE_Get(&HotkeysText, hkey->textIndex);

	if (!(hkey->flags & HOTKEY_FLAG_STAYS_OPEN)) {
		Chat_Send(&text, false);
	} else if (!Gui.InputGrab) {
		ChatScreen_OpenInput(&text);
	}
}

static void HookInputBinds(void) {
	Bind_OnTriggered[BIND_HIDE_FPS]   = BindTriggered_HideFPS;
	Bind_OnTriggered[BIND_FULLSCREEN] = BindTriggered_Fullscreen;
	Bind_OnTriggered[BIND_FOG]        = BindTriggered_Fog;

	Bind_OnTriggered[BIND_DELETE_BLOCK] = BindTriggered_DeleteBlock;
	Bind_OnTriggered[BIND_PLACE_BLOCK]  = BindTriggered_PlaceBlock;
	Bind_OnTriggered[BIND_PICK_BLOCK]   = BindTriggered_PickBlock;

	Bind_OnReleased[BIND_DELETE_BLOCK] = BindReleased_DeleteBlock;
	Bind_OnReleased[BIND_PLACE_BLOCK]  = BindReleased_PlaceBlock;
	Bind_OnReleased[BIND_PICK_BLOCK]   = BindReleased_PickBlock;

	if (Game_ClassicMode) return;
	Bind_OnTriggered[BIND_HIDE_GUI]      = BindTriggered_HideGUI;
	Bind_OnTriggered[BIND_SMOOTH_CAMERA] = BindTriggered_SmoothCamera;
	Bind_OnTriggered[BIND_AXIS_LINES]    = BindTriggered_AxisLines;
	Bind_OnTriggered[BIND_AUTOROTATE]    = BindTriggered_AutoRotate;
	Bind_OnTriggered[BIND_THIRD_PERSON]  = BindTriggered_ThirdPerson;
	Bind_OnTriggered[BIND_DROP_BLOCK]    = BindTriggered_DropBlock;
	Bind_OnTriggered[BIND_IDOVERLAY]     = BindTriggered_IDOverlay;
	Bind_OnTriggered[BIND_BREAK_LIQUIDS] = BindTriggered_BreakLiquids;
	Bind_OnTriggered[BIND_SPAWN_MOB]     = BindTriggered_SpawnMob;
	Bind_OnTriggered[BIND_DELETE_ITEM]   = BindTriggered_DeleteItem;
	Bind_OnTriggered[BIND_DROP_ITEM]    = BindTriggered_DropItemSprite;
}


/*########################################################################################################################*
*---------------------------------------------------------Keybinds--------------------------------------------------------*
*#########################################################################################################################*/
BindTriggered Bind_OnTriggered[BIND_COUNT];
BindReleased  Bind_OnReleased[BIND_COUNT];
cc_uint8 Bind_IsTriggered[BIND_COUNT];

cc_bool KeyBind_IsPressed(InputBind binding) { return Bind_IsTriggered[binding]; }


/*########################################################################################################################*
*-----------------------------------------------------Base handlers-------------------------------------------------------*
*#########################################################################################################################*/
static void OnPointerDown(void* obj, int idx) {
	struct Screen* s;
	int i, x, y, mask;

	/* Always reset held time, otherwise quickly tapping */
	/* sometimes triggers a 'delete' in InputHandler_Tick, */
	/* and then another 'delete' in CheckBlockTap. */
	input_deltaAcc = 0;

#ifdef CC_BUILD_TOUCH
	if (Input_TouchMode && !(touches[idx].type & TOUCH_TYPE_GUI)) return;
#endif
	x = Pointers[idx].x; y = Pointers[idx].y;

	for (i = 0; i < Gui.ScreensCount; i++) {
		s = Gui_Screens[i];
		s->dirty = true;
		mask = s->VTABLE->HandlesPointerDown(s, 1 << idx, x, y);

#ifdef CC_BUILD_TOUCH
		if (mask) {
			/* Using &= mask instead of = mask is to handle one specific case */
			/*  - when clicking 'Quit game' in android version, it will call  */
			/*  Game_Free, which will in turn call InputComponent.Free.       */
			/* That resets the type of all touches to 0 - however, since it is */
			/*  called DURING HandlesPointerDown, using = mask here would undo */
			/*  the resetting of type to 0 for one of the touches states,      */
			/*  causing problems later with Input_AddTouch as it will assume that */
			/*  the aforementioned touches state is wrongly still in use */
			touches[idx].type &= mask; return;
		}
#else
		if (mask) return;
#endif
	}
}

static void OnPointerUp(void* obj, int idx) {
	struct Screen* s;
	int i, x, y;

#ifdef CC_BUILD_TOUCH
	CheckBlockTap(idx);
	if (Input_TouchMode && !(touches[idx].type & TOUCH_TYPE_GUI)) return;
#endif
	x = Pointers[idx].x; y = Pointers[idx].y;

	for (i = 0; i < Gui.ScreensCount; i++) {
		s = Gui_Screens[i];
		s->dirty = true;
		s->VTABLE->OnPointerUp(s, 1 << idx, x, y);
	}
}


/*########################################################################################################################*
*----------------------------------------------------SpawnRandomMob-------------------------------------------------------*
*#########################################################################################################################*/
static const char* const mobModelNames[] = { "pig", "sheep", "creeper", "spider", "zombie", "skeleton" };
static const char* const mobDisplayNames[] = { "Pig", "Sheep", "Creeper", "Spider", "Zombie", "Skeleton" };
/* 0=passive, 1=hostile - matches mobModelNames order */
static const cc_uint8 mobIsHostile[] = { 0, 0, 1, 1, 1, 1 };

static const struct EntityVTABLE* origNetPlayerVTABLE;
static struct EntityVTABLE mobEntity_VTABLE;
static cc_bool mob_vtable_inited;

/* Per-entity mob AI state, indexed by entity ID */
#define MOB_TYPE_NONE    0
#define MOB_TYPE_PASSIVE 1
#define MOB_TYPE_HOSTILE 2
#define MOB_GRAVITY      0.08f
#define MOB_JUMP_VEL     0.42f
#define MOB_SPEED        4.317f  /* base player walking speed in blocks/sec */
#define MOB_HOSTILE_SPEED_FACTOR 0.75f
#define MOB_PASSIVE_SPEED_FACTOR 0.4f
#define MOB_WANDER_RANGE 10
#define MOB_AGGRO_RANGE_SQ   (16.0f * 16.0f)  /* 16 blocks */
#define MOB_DEAGGRO_RANGE_SQ (24.0f * 24.0f)  /* 24 blocks - always deaggro */
#define MOB_DEAGGRO_LOS_RANGE_SQ (10.0f * 10.0f)  /* 10 blocks - deaggro if no line of sight */
#define MOB_SPACING_DIST_SQ  (2.0f * 2.0f)    /* hostile mobs stay 2 blocks apart */

static cc_uint8 mobType[MAX_NET_PLAYERS];
static Vec3     mobWanderTarget[MAX_NET_PLAYERS];
static cc_bool  mobHasTarget[MAX_NET_PLAYERS];
static float    mobWanderPause[MAX_NET_PLAYERS]; /* seconds to pause before picking new target */
static float    mobFacingYaw[MAX_NET_PLAYERS];   /* current facing yaw in degrees */
static cc_bool  mobIsMoving[MAX_NET_PLAYERS];    /* whether mob is currently walking (for animation) */
static int      mobHealth[MAX_NET_PLAYERS];      /* mob hit points */
static cc_bool  mobIsAggro[MAX_NET_PLAYERS];     /* whether hostile mob is currently aggro'd */
static float    mobHurtFlash[MAX_NET_PLAYERS];   /* hurt flash timer (seconds remaining, 0.5s) */
static float    mobDeathTimer[MAX_NET_PLAYERS];  /* death animation timer (seconds remaining) */
static float    mobDeathRotZ[MAX_NET_PLAYERS];   /* death tip-over rotation direction */
static Vec3     mobLastStuckPos[MAX_NET_PLAYERS];  /* last recorded position for stuck detection */
static float    mobStuckTimer[MAX_NET_PLAYERS];    /* time spent near the same position while walking */
static cc_uint8 mobModelIdx[MAX_NET_PLAYERS];    /* index into mobModelNames (0-5), creeper=2 */
static float    mobCreeperFuse[MAX_NET_PLAYERS]; /* creeper fuse timer (seconds remaining, -1=inactive) */

#define MOB_IDX_PIG      0
#define MOB_IDX_SHEEP    1
#define MOB_IDX_CREEPER  2
#define MOB_IDX_SPIDER   3
#define MOB_IDX_ZOMBIE   4
#define MOB_IDX_SKELETON 5
#define CREEPER_DEATH_EXPLODE_DELAY 2.0f  /* seconds after death before explosion */
#define CREEPER_ATTACK_RANGE_SQ (3.0f * 3.0f)  /* 3 blocks */
#define CREEPER_ATTACK_FUSE_TIME 3.0f     /* seconds of fuse before explosion attack */

#define SKELETON_SHOOT_RANGE_SQ  (12.0f * 12.0f)   /* shoot within 12 blocks */
#define SKELETON_BACKPEDAL_RANGE_SQ (4.0f * 4.0f)   /* backpedal if closer than 4 blocks */
#define SKELETON_PREFERRED_DIST_SQ  (10.0f * 10.0f)  /* try to maintain 10 blocks */
#define SKELETON_SPEED_FACTOR  0.5f                  /* 50% of player speed */
#define SKELETON_SHOOT_COOLDOWN 2.0f                 /* seconds between shots */
#define ARROW_SPEED 1.0f                             /* blocks per tick of arrow travel */
#define ARROW_GRAVITY 0.05f                          /* gravity per tick for arrows */
#define ARROW_DAMAGE 3                               /* damage per arrow hit */
#define PLAYER_ARROW_DAMAGE 4                        /* damage per player arrow hit */

#define SPIDER_CLIMB_SPEED_FACTOR 0.5f               /* 50% of normal walk speed */
#define SPIDER_LEAP_RANGE_SQ (4.0f * 4.0f)           /* leap if within 4 blocks */
#define SPIDER_LEAP_COOLDOWN 3.0f                     /* seconds between leaps */
#define SPIDER_LEAP_HORIZONTAL 0.6f                   /* horizontal leap strength */
#define SPIDER_LEAP_VERTICAL   0.45f                  /* vertical leap strength */
#define CREEPER_FUSE_SPEED_FACTOR 0.5f                /* 50% speed while fuse is lit */
#define MOB_TURN_SPEED 360.0f                          /* degrees per second of turning */

#define MOB_HP_HOSTILE 20
#define MOB_HP_PASSIVE 10

/* Apply health multiplier setting: 0=0.5x, 1=1.0x, 2=1.5x, 3=2.0x */
static int Mob_GetHealthWithMultiplier(int baseHealth) {
	return (baseHealth * (1 + Game_MobHealthMultiplier)) / 2;
}

static float    mobSkeletonShootTimer[MAX_NET_PLAYERS]; /* cooldown between shots */
static float    mobTargetYaw[MAX_NET_PLAYERS];          /* desired facing yaw (smooth turning target) */
static cc_bool  mobWalkBackwards[MAX_NET_PLAYERS];      /* true = play walk anim in reverse (skeleton backpedal) */
static float    mobSpiderLeapTimer[MAX_NET_PLAYERS];    /* cooldown between spider leaps */
static float    mobFallStartY[MAX_NET_PLAYERS];         /* Y position when mob started falling */
static cc_bool  mobIsBrownSpider[MAX_NET_PLAYERS];      /* true = brown spider variant (hostile type, passive behavior) */
static float    mobSunDamageTimer[MAX_NET_PLAYERS];     /* accumulates time in sunlight for light sensitivity */
static float    mobLavaDamageTimer[MAX_NET_PLAYERS];    /* accumulates time in lava for lava damage */
static float    mobCactusDamageTimer[MAX_NET_PLAYERS];  /* accumulates time touching cactus for cactus damage */
static float    mobFireDamageTimer[MAX_NET_PLAYERS];    /* accumulates time in fire for fire damage */
static float    mobOnFireTimer[MAX_NET_PLAYERS];         /* lingering fire countdown after leaving source */
static cc_uint8 mobCreeperVariant[MAX_NET_PLAYERS];     /* creeper variant type (only valid when mobModelIdx == MOB_IDX_CREEPER) */
static cc_bool  mobSheepSheared[MAX_NET_PLAYERS];       /* true = sheep has been sheared (no wool layer) */
static float    mobSheepWoolTimer[MAX_NET_PLAYERS];     /* seconds until sheared sheep regains wool */
#define SHEEP_WOOL_REGROW_MIN 120.0f  /* minimum 2 minutes */
#define SHEEP_WOOL_REGROW_MAX 300.0f  /* maximum 5 minutes */
static GfxResourceID mob_whiteTex;                      /* 1x1 solid white texture for creeper flash */

/* Melee attack system */
#define MOB_MELEE_DAMAGE        3     /* base melee damage for all enemies */
#define MOB_MELEE_RANGE_SQ      (1.5f * 1.5f) /* melee range: 1.5 blocks */
#define MOB_MELEE_COOLDOWN      1.0f  /* seconds between melee attacks */
#define SPIDER_LEAP_DAMAGE_MULT 1.5f  /* leap does 1.5x melee damage on contact */
#define SPIDER_LEAP_CONTACT_SQ  (1.8f * 1.8f) /* slightly larger than melee for leap contact */
static float    mobMeleeTimer[MAX_NET_PLAYERS];         /* cooldown between melee attacks */
static float    mobAttackAnimTimer[MAX_NET_PLAYERS];    /* timer for zombie arm swing animation */
#define MOB_ATTACK_ANIM_DURATION 0.3f /* seconds for arm swing animation */

/* Apply mob damage multiplier: 0=0.5x, 1=1.0x, 2=1.5x, 3=2.0x */
static int Mob_GetDamageWithMultiplier(int baseDamage) {
	return max(1, (baseDamage * (1 + Game_MobDamageMultiplier)) / 2);
}

/* Creeper variant constants */
#define CREEPER_VAR_STANDARD  0  /* normal creeper: explosion attack */
#define CREEPER_VAR_SURVTEST  1  /* survival test: melee + explode on death (creepera.png) */
#define CREEPER_VAR_MELEE     2  /* melee only, never explodes (creeperb.png) */
#define CREEPER_VAR_NUKE      3  /* easter egg: 20-block radius explosion (creeperc.png) */
#define CREEPER_NUKE_POWER    20 /* explosion radius for nuke creeper */
#define CREEPER_FUSE_CANCEL_RANGE_SQ (5.0f * 5.0f) /* cancel fuse if player > 5 blocks away */

/* Arrow projectile state */
#define MAX_ARROWS 64
static cc_bool  arrowActive[MAX_ARROWS];
static int      arrowEntityId[MAX_ARROWS];    /* entity ID in Entities.List */
static Vec3     arrowVelocity[MAX_ARROWS];    /* per-tick velocity */
static float    arrowLifetime[MAX_ARROWS];    /* seconds remaining */
static IVec3    arrowStuckBlock[MAX_ARROWS];  /* block coords arrow is stuck in */
static cc_bool  arrowIsPlayerArrow[MAX_ARROWS]; /* true if shot by the player (hits mobs, not player) */

/* Player damage state (survival mode) */
static float playerLavaDamageTimer;   /* accumulates time in lava */
static float playerCactusDamageTimer; /* accumulates time touching cactus */
static float playerFireDamageTimer;   /* accumulates time in fire */
static float playerOnFireTimer;       /* lingering fire countdown after leaving source */
static float playerFallStartY;        /* Y position when player started falling */
static cc_bool playerWasOnGround;     /* whether player was on ground last tick */
static float playerInvulnTimer;       /* invulnerability frames after taking damage */
#define PLAYER_INVULN_TIME 0.5f       /* seconds of invulnerability after damage */
#define SKELETON_ARROW_PLAYER_DAMAGE 2 /* damage from skeleton arrow to player */

/* Sum defense points from all equipped armor pieces */
static int Player_GetArmorPoints(void) {
	int total = 0, i, itemId;
	for (i = 0; i < 4; i++) {
		itemId = SurvInv_Armor[i].itemId;
		if (itemId > ITEM_NONE && itemId < ITEM_COUNT)
			total += ItemArmorPoints[itemId];
	}
	return total;
}

/* Apply damage to the player (survival mode only) */
void Player_Damage(int amount) {
	int armor, effective, min_effective;
	if (!Game_SurvivalMode) return;
	if (Player_Health <= 0) return;
	if (playerInvulnTimer > 0.0f) return;
	if (Player_CheatsEnabled) return; /* invincible with cheats */

	/* Armor reduction: classic Minecraft formula (pre-1.9, no toughness).
	   effective = clamp(max(armor/5, armor - damage/2), 0, 20)
	   damage_after = max(1, damage * (25 - effective) / 25) */
	armor = Player_GetArmorPoints();
	if (armor > 0) {
		effective     = armor - amount / 2;
		min_effective = armor / 5;
		if (effective < min_effective) effective = min_effective;
		if (effective > 20) effective = 20;
		amount = amount * (25 - effective) / 25;
		if (amount < 1) amount = 1;
	}

	Player_Health -= amount;
	if (Player_Health < 0) Player_Health = 0;
	playerInvulnTimer = PLAYER_INVULN_TIME;
	Audio_PlayDigSound(SOUND_HURT);
}

#define DROPPED_ITEM_HOVER_HEIGHT 0.25f  /* base hover height above ground */
#define DROPPED_ITEM_HOVER_AMPLITUDE 0.05f /* bobbing amplitude */
/* Dropped item state (MAX_DROPPED_ITEMS and velocity arrays declared at top of file) */
static cc_bool  droppedItemActive[MAX_DROPPED_ITEMS];
static int      droppedItemEntityId[MAX_DROPPED_ITEMS];  /* entity ID in Entities.List */
static BlockID  droppedItemBlock[MAX_DROPPED_ITEMS];     /* which block this item is */
static float    droppedItemLifetime[MAX_DROPPED_ITEMS];  /* seconds until despawn (180s = 3min) */
static float    droppedItemHoverTime[MAX_DROPPED_ITEMS]; /* animation phase for hover */
static cc_bool  droppedItemOnGround[MAX_DROPPED_ITEMS];  /* whether item has landed */
static cc_bool  droppedItemIsItem[MAX_DROPPED_ITEMS]; /* true = 2D item sprite, false = 3D block */
static int      droppedItemItemId[MAX_DROPPED_ITEMS]; /* items.png tile index for 2D item sprites */
static float    droppedItemGroundY[MAX_DROPPED_ITEMS]; /* base Y when item is on ground (for hover) */

static int DropItem_FindFreeSlot(void) {
	int i;
	for (i = 0; i < MAX_DROPPED_ITEMS; i++) {
		if (!droppedItemActive[i]) return i;
	}
	return -1;  /* No free slots */
}

static int DropItem_EvictOldest(void) {
	int i, oldest = -1;
	float oldestLife = 999.0f;
	for (i = 0; i < MAX_DROPPED_ITEMS; i++) {
		if (!droppedItemActive[i]) continue;
		/* Lower lifetime = older (lifetime counts down from 180) */
		if (droppedItemLifetime[i] < oldestLife) {
			oldestLife = droppedItemLifetime[i];
			oldest = i;
		}
	}
	if (oldest != -1) {
		Entities_Remove(droppedItemEntityId[oldest]);
		droppedItemActive[oldest] = false;
	}
	return oldest;
}

static int DropItem_FindFreeEntity(void) {
	int i;
	for (i = 0; i < MAX_NET_PLAYERS; i++) {
		if (!Entities.List[i]) return i;
	}
	return -1;  /* No free entity slots */
}

/* Returns items.png tile index for blocks that should render as 2D item sprites, -1 otherwise */
static int DropItem_GetItemTex(BlockID block) {
	if (block == BLOCK_RED_ORE_DUST)    return 56;
	if (block == BLOCK_DOOR_NS_BOTTOM)  return 43;
	if (block == BLOCK_IRON_DOOR)       return 44;
	return -1;
}

static void DropItem_Spawn(int slot, Vec3 pos, BlockID block, cc_bool isItem, int itemId) {
	struct NetPlayer* np;
	cc_string modelName;
	int eid, blockItemTex, itemTile;

	eid = DropItem_FindFreeEntity();
	if (eid == -1) return;

	/* Initialize NetPlayer entity */
	np = &NetPlayers_List[eid];
	NetPlayer_Init(np);
	Entities.List[eid] = &np->Base;
	Event_RaiseInt(&EntityEvents.Added, eid);

	if (isItem) {
		/* Set up 2D item sprite model - itemId is the item ID (1-56),
		   look up the tile index in items.png for rendering */
		itemTile = (itemId > 0 && itemId < ITEM_COUNT) ? ItemTextures[itemId] : 0;
		modelName = String_FromReadonly("item");
		Entity_SetModel(&np->Base, &modelName);
		np->Base.ModelBlock = itemTile;
		np->Base.ModelScale = Vec3_Create3(1.0f, 1.0f, 1.0f);
		np->Base.uScale = 0.25f;
		np->Base.vScale = 0.25f;
	} else if ((blockItemTex = DropItem_GetItemTex(block)) >= 0) {
		/* Block that renders as 2D item sprite (e.g. redstone, doors) */
		modelName = String_FromReadonly("item");
		Entity_SetModel(&np->Base, &modelName);
		np->Base.ModelBlock = blockItemTex;
		np->Base.ModelScale = Vec3_Create3(1.0f, 1.0f, 1.0f);
		np->Base.uScale = 0.25f;
		np->Base.vScale = 0.25f;
	} else {
		/* Set up 3D block model */
		modelName = String_FromReadonly("block");
		Entity_SetModel(&np->Base, &modelName);
		np->Base.ModelBlock = block;
		np->Base.ModelScale = Vec3_Create3(0.25f, 0.25f, 0.25f);
	}

	/* Set position directly - bypass interpolation to avoid jitter */
	np->Base.Position = pos;
	np->Base.next.pos = pos;
	np->Base.prev.pos = pos;

	/* Initialize dropped item state */
	droppedItemActive[slot]     = true;
	droppedItemEntityId[slot]   = eid;
	droppedItemBlock[slot]      = block;
	droppedItemLifetime[slot]   = 180.0f;
	droppedItemHoverTime[slot]  = 0.0f;
	droppedItemVelocityX[slot]  = 0.0f;
	droppedItemVelocityY[slot]  = 0.0f;
	droppedItemVelocityZ[slot]  = 0.0f;
	droppedItemOnGround[slot]   = false;
	droppedItemPickupDelay[slot] = 1.0f;  /* 1 second before pickup allowed */
	droppedItemIsItem[slot]     = isItem;
	droppedItemItemId[slot]     = isItem ? itemId : 0;
	droppedItemCount[slot]      = 1;
}

static void DropItem_TryPickup(int slot) {
	struct Entity* item;
	struct Entity* player;
	float dx, dy, dz, distSq;
	BlockID block;
	int i, pickupCount, canFit;
	cc_bool hotbarChanged = false;

	if (!mob_rng_inited) {
		Random_SeedFromCurrentTime(&mob_rng);
		mob_rng_inited = true;
	}

	item = Entities.List[droppedItemEntityId[slot]];
	player = &Entities.CurPlayer->Base;

	/* Check distance (1.5 blocks pickup range) */
	dx = item->Position.x - player->Position.x;
	dy = item->Position.y - player->Position.y;
	dz = item->Position.z - player->Position.z;
	distSq = dx * dx + dy * dy + dz * dz;

	if (distSq > (1.5f * 1.5f)) return;

	pickupCount = droppedItemCount[slot];
	if (pickupCount < 1) pickupCount = 1;

	/* Item pickups (non-block items) */
	if (droppedItemIsItem[slot]) {
		int itemId = droppedItemItemId[slot];
		int remaining = pickupCount;
		int maxStack = Item_MaxStackSize(itemId);

		/* First try to stack with existing same item in hotbar */
		for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR && remaining > 0; i++) {
			if (Hotbar_GetItem(i) == itemId && Inventory_Get(i) == BLOCK_AIR &&
				Hotbar_GetCount(i) < maxStack) {
				canFit = maxStack - Hotbar_GetCount(i);
				if (canFit > remaining) canFit = remaining;
				Hotbar_SetCount(i, Hotbar_GetCount(i) + canFit);
				remaining -= canFit;
				hotbarChanged = true;
			}
		}
		/* Then try to stack in main inventory */
		for (i = 0; i < 27 && remaining > 0; i++) {
			if (SurvInv_Main[i].itemId == itemId && SurvInv_Main[i].block == BLOCK_AIR &&
				SurvInv_Main[i].count < maxStack) {
				canFit = maxStack - SurvInv_Main[i].count;
				if (canFit > remaining) canFit = remaining;
				SurvInv_Main[i].count += canFit;
				remaining -= canFit;
			}
		}
		/* Try empty hotbar slot */
		for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR && remaining > 0; i++) {
			if (Inventory_Get(i) != BLOCK_AIR) continue;
			if (Hotbar_GetItem(i) != ITEM_NONE) continue;

			canFit = remaining;
			if (canFit > maxStack) canFit = maxStack;
			Hotbar_SetItem(i, itemId);
			Hotbar_SetCount(i, canFit);
			remaining -= canFit;
			hotbarChanged = true;
		}
		/* Try empty main inventory slot */
		for (i = 0; i < 27 && remaining > 0; i++) {
			if (SurvInv_Main[i].block != BLOCK_AIR || SurvInv_Main[i].itemId != ITEM_NONE) continue;
			canFit = remaining;
			if (canFit > maxStack) canFit = maxStack;
			SurvInv_Main[i].itemId = itemId;
			SurvInv_Main[i].count  = canFit;
			remaining -= canFit;
		}

		if (remaining < pickupCount) {
			/* Picked up at least some items */
			Audio_PlayDigSoundRate(SOUND_PICKUP, 80 + Random_Next(&mob_rng, 41));
			if (remaining <= 0) {
				Entities_Remove(droppedItemEntityId[slot]);
				droppedItemActive[slot] = false;
			} else {
				droppedItemCount[slot] = remaining;
			}
			if (hotbarChanged) Event_RaiseVoid(&UserEvents.HeldBlockChanged);
		}
		return;
	}

	block = droppedItemBlock[slot];
	{
		int remaining = pickupCount;
		int blockMaxStack = Block_MaxStackSize(block);

		/* First try to stack with existing same block in hotbar */
		for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR && remaining > 0; i++) {
			if (Inventory_Get(i) == block && Hotbar_GetItem(i) == ITEM_NONE &&
				Hotbar_GetCount(i) < blockMaxStack) {
				canFit = blockMaxStack - Hotbar_GetCount(i);
				if (canFit > remaining) canFit = remaining;
				Hotbar_SetCount(i, Hotbar_GetCount(i) + canFit);
				remaining -= canFit;
				hotbarChanged = true;
			}
		}
		/* Then try to stack in main inventory */
		for (i = 0; i < 27 && remaining > 0; i++) {
			if (SurvInv_Main[i].block == block && SurvInv_Main[i].itemId == ITEM_NONE &&
				SurvInv_Main[i].count < blockMaxStack) {
				canFit = blockMaxStack - SurvInv_Main[i].count;
				if (canFit > remaining) canFit = remaining;
				SurvInv_Main[i].count += canFit;
				remaining -= canFit;
			}
		}
		/* Try empty hotbar slot */
		for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR && remaining > 0; i++) {
			if (Inventory_Get(i) != BLOCK_AIR) continue;
			if (Hotbar_GetItem(i) != ITEM_NONE) continue;

			canFit = remaining;
			if (canFit > blockMaxStack) canFit = blockMaxStack;
			Inventory_Set(i, block);
			Hotbar_SetCount(i, canFit);
			remaining -= canFit;
			hotbarChanged = true;
		}
		/* Try empty main inventory slot */
		for (i = 0; i < 27 && remaining > 0; i++) {
			if (SurvInv_Main[i].block != BLOCK_AIR || SurvInv_Main[i].itemId != ITEM_NONE) continue;
			canFit = remaining;
			if (canFit > blockMaxStack) canFit = blockMaxStack;
			SurvInv_Main[i].block = block;
			SurvInv_Main[i].count = canFit;
			remaining -= canFit;
		}

		if (remaining < pickupCount) {
			/* Picked up at least some blocks */
			Audio_PlayDigSoundRate(SOUND_PICKUP, 80 + Random_Next(&mob_rng, 41));
			if (remaining <= 0) {
				Entities_Remove(droppedItemEntityId[slot]);
				droppedItemActive[slot] = false;
			} else {
				droppedItemCount[slot] = remaining;
			}
			if (hotbarChanged) Event_RaiseVoid(&UserEvents.HeldBlockChanged);
		}
	}
}

static void DroppedItem_TickAll(struct ScheduledTask* task) {
	int i, bx, by, bz;
	struct Entity* e;
	float delta = task->interval;
	float newY, blockTop, hoverOffset, rotY;
	BlockID below;

	/* Pause during menus */
	if (Gui_GetInputGrab()) return;

	for (i = 0; i < MAX_DROPPED_ITEMS; i++) {
		if (!droppedItemActive[i]) continue;

		e = Entities.List[droppedItemEntityId[i]];
		if (!e) { droppedItemActive[i] = false; continue; }

		/* Update lifetime */
		droppedItemLifetime[i] -= delta;
		if (droppedItemLifetime[i] <= 0.0f) {
			Entities_Remove(droppedItemEntityId[i]);
			droppedItemActive[i] = false;
			continue;
		}

		/* Destroy if in lava */
		{
			int lx = (int)Math_Floor(e->Position.x);
			int ly = (int)Math_Floor(e->Position.y);
			int lz = (int)Math_Floor(e->Position.z);
			if (World_Contains(lx, ly, lz)) {
				BlockID atBlock = World_GetBlock(lx, ly, lz);
				if (atBlock == BLOCK_LAVA || atBlock == BLOCK_STILL_LAVA) {
					Entities_Remove(droppedItemEntityId[i]);
					droppedItemActive[i] = false;
					continue;
				}
			}
		}

		/* Apply horizontal toss velocity with wall collision (stop all momentum on contact) */
		if (droppedItemVelocityX[i] != 0.0f || droppedItemVelocityZ[i] != 0.0f) {
			float newX = e->Position.x + droppedItemVelocityX[i];
			float newZ = e->Position.z + droppedItemVelocityZ[i];
			int feetY = (int)Math_Floor(e->Position.y);
			cc_bool hitBlock = false;

			/* Check X-axis wall collision */
			bx = (int)Math_Floor(newX);
			bz = (int)Math_Floor(e->Position.z);
			if (World_Contains(bx, feetY, bz) && Mob_BlockIsSolid(bx, feetY, bz)) {
				hitBlock = true;
			} else {
				e->Position.x = newX;
				e->next.pos.x = newX;
			}

			/* Check Z-axis wall collision */
			bx = (int)Math_Floor(e->Position.x);
			bz = (int)Math_Floor(newZ);
			if (World_Contains(bx, feetY, bz) && Mob_BlockIsSolid(bx, feetY, bz)) {
				hitBlock = true;
			} else {
				e->Position.z = newZ;
				e->next.pos.z = newZ;
			}

			/* Stop all horizontal momentum on any block contact */
			if (hitBlock) {
				droppedItemVelocityX[i] = 0.0f;
				droppedItemVelocityZ[i] = 0.0f;
			} else {
				/* Apply friction */
				droppedItemVelocityX[i] *= 0.92f;
				droppedItemVelocityZ[i] *= 0.92f;
				if (Math_AbsF(droppedItemVelocityX[i]) < 0.001f) droppedItemVelocityX[i] = 0.0f;
				if (Math_AbsF(droppedItemVelocityZ[i]) < 0.001f) droppedItemVelocityZ[i] = 0.0f;
			}
		}

		/* Apply gravity (same pattern as Mob_ApplyGravity) */
		if (!droppedItemOnGround[i]) {
			bx = (int)Math_Floor(e->Position.x);
			by = (int)Math_Floor(e->Position.y - 0.05f);
			bz = (int)Math_Floor(e->Position.z);

			if (World_Contains(bx, by, bz)) {
				below = World_GetBlock(bx, by, bz);
				if (Blocks.Collide[below] == COLLIDE_SOLID) {
					blockTop = (float)by + Blocks.MaxBB[below].y + DROPPED_ITEM_HOVER_HEIGHT;
					if (droppedItemVelocityY[i] <= 0.0f && e->Position.y <= blockTop + 0.05f) {
						/* Land on block (hovering above surface) */
						droppedItemVelocityY[i] = 0.0f;
						droppedItemOnGround[i]  = true;
						droppedItemGroundY[i]   = blockTop;
						e->Position.y  = blockTop;
						e->next.pos.y  = blockTop;
						e->prev.pos.y  = blockTop;
					} else {
						/* Falling above solid block */
						newY = e->Position.y + droppedItemVelocityY[i];
						if (newY <= blockTop) {
							droppedItemVelocityY[i] = 0.0f;
							droppedItemOnGround[i]  = true;
							droppedItemGroundY[i]   = blockTop;
							e->Position.y  = blockTop;
							e->next.pos.y  = blockTop;
							e->prev.pos.y  = blockTop;
						} else {
							e->Position.y  = newY;
							droppedItemVelocityY[i] -= MOB_GRAVITY;
							e->next.pos.y  = e->Position.y;
						}
					}
				} else {
					/* Air below: keep falling */
					e->Position.y += droppedItemVelocityY[i];
					droppedItemVelocityY[i] -= MOB_GRAVITY;
					e->next.pos.y = e->Position.y;
				}
			} else if (by < 0) {
				/* Below world: stop */
				droppedItemVelocityY[i] = 0.0f;
				droppedItemOnGround[i]  = true;
			} else {
				/* Outside world: keep falling */
				e->Position.y += droppedItemVelocityY[i];
				droppedItemVelocityY[i] -= MOB_GRAVITY;
				e->next.pos.y = e->Position.y;
			}
		} else {
			/* On ground: check if block below was removed */
			float baseY = droppedItemGroundY[i] - DROPPED_ITEM_HOVER_HEIGHT;
			bx = (int)Math_Floor(e->Position.x);
			by = (int)Math_Floor(baseY - 0.05f);
			bz = (int)Math_Floor(e->Position.z);
			if (!World_Contains(bx, by, bz) || Blocks.Collide[World_GetBlock(bx, by, bz)] != COLLIDE_SOLID) {
				droppedItemOnGround[i] = false;
				droppedItemVelocityY[i] = 0.0f;
			} else {
				/* Apply hover animation using stored ground Y as base */
				droppedItemHoverTime[i] += delta;
				hoverOffset = Math_SinF(droppedItemHoverTime[i] * MATH_PI * 2.0f) * DROPPED_ITEM_HOVER_AMPLITUDE;
				e->Position.y  = droppedItemGroundY[i] + hoverOffset;
				e->next.pos.y  = e->Position.y;
			}
		}

		/* Update rotation (90 degrees per second spin) - skip for 2D item sprites (they billboard) */
		if (!droppedItemIsItem[i]) {
			rotY = e->next.rotY + 90.0f * delta;
			if (rotY >= 360.0f) rotY -= 360.0f;
			e->prev.rotY = e->next.rotY;
			e->next.rotY = rotY;
		}

		/* Update pickup delay and check for pickup */
		if (droppedItemPickupDelay[i] > 0.0f) {
			droppedItemPickupDelay[i] -= delta;
		} else {
			DropItem_TryPickup(i);
		}
	}
}

static cc_bool Mob_BlockIsSolid(int x, int y, int z) {
	BlockID b;
	if (!World_Contains(x, y, z)) return false;
	b = World_GetBlock(x, y, z);
	return Blocks.Collide[b] == COLLIDE_SOLID;
}

/* Check if a block would obstruct a mob at the given entity Y position (considering partial block heights) */
static cc_bool Mob_BlockObstructsAt(int x, int blockY, int z, float entityY) {
	BlockID b;
	float blockTop;
	if (!World_Contains(x, blockY, z)) return false;
	b = World_GetBlock(x, blockY, z);
	if (Blocks.Collide[b] != COLLIDE_SOLID) return false;
	
	/* Get the actual top height of this block */
	blockTop = (float)blockY + Blocks.MaxBB[b].y;
	
	/* Block obstructs if its top is above the entity's feet */
	/* Use small epsilon to avoid false positives when mob is standing on block surface */
	return blockTop > entityY + 0.01f;
}

static cc_bool Mob_BlockIsPassable(int x, int y, int z) {
	BlockID b;
	if (!World_Contains(x, y, z)) return true;
	b = World_GetBlock(x, y, z);
	return Blocks.Collide[b] != COLLIDE_SOLID;
}

/* Checks line-of-sight between two positions (no solid blocks in the way) */
static cc_bool Mob_HasLineOfSight(Vec3 from, Vec3 to) {
	float dx = to.x - from.x;
	float dy = to.y - from.y;
	float dz = to.z - from.z;
	float dist = Math_SqrtF(dx * dx + dy * dy + dz * dz);
	float step = 0.5f;
	float t;
	if (dist < 0.1f) return true;
	for (t = step; t < dist; t += step) {
		float frac = t / dist;
		int bx = (int)Math_Floor(from.x + dx * frac);
		int by = (int)Math_Floor(from.y + dy * frac);
		int bz = (int)Math_Floor(from.z + dz * frac);
		if (Mob_BlockIsSolid(bx, by, bz)) return false;
	}
	return true;
}

static cc_bool Mob_IsInWater(struct Entity* e) {
	int bx = (int)Math_Floor(e->Position.x);
	int by = (int)Math_Floor(e->Position.y);
	int bz = (int)Math_Floor(e->Position.z);
	BlockID b;
	if (!World_Contains(bx, by, bz)) return false;
	b = World_GetBlock(bx, by, bz);
	return b == BLOCK_WATER || b == BLOCK_STILL_WATER;
}

static cc_bool Mob_IsInLava(struct Entity* e) {
	int bx = (int)Math_Floor(e->Position.x);
	int by = (int)Math_Floor(e->Position.y);
	int bz = (int)Math_Floor(e->Position.z);
	BlockID b;
	if (!World_Contains(bx, by, bz)) return false;
	b = World_GetBlock(bx, by, bz);
	return b == BLOCK_LAVA || b == BLOCK_STILL_LAVA;
}

static cc_bool Mob_IsInFire(struct Entity* e) {
	int bx = (int)Math_Floor(e->Position.x);
	int by = (int)Math_Floor(e->Position.y);
	int bz = (int)Math_Floor(e->Position.z);
	/* Check both feet and head level so wall/ceiling fire also burns */
	if (World_Contains(bx, by, bz) && World_GetBlock(bx, by, bz) == BLOCK_FIRE)
		return true;
	if (World_Contains(bx, by + 1, bz) && World_GetBlock(bx, by + 1, bz) == BLOCK_FIRE)
		return true;
	return false;
}

static cc_bool Mob_IsTouchingCactus(struct Entity* e) {
	int bx = (int)Math_Floor(e->Position.x);
	int by = (int)Math_Floor(e->Position.y);
	int bz = (int)Math_Floor(e->Position.z);
	int dx, dy;
	/* Check the block at feet and head level, plus adjacent blocks */
	for (dy = 0; dy <= 1; dy++) {
		for (dx = -1; dx <= 1; dx++) {
			int dz;
			for (dz = -1; dz <= 1; dz++) {
				int cx = bx + dx, cy = by + dy, cz = bz + dz;
				if (World_Contains(cx, cy, cz) && World_GetBlock(cx, cy, cz) == BLOCK_CACTUS)
					return true;
			}
		}
	}
	return false;
}

static void Mob_PickWanderTarget(int id, struct Entity* e) {
	int attempts;
	int tx, ty, tz;
	int ex, ey, ez;

	ex = (int)Math_Floor(e->Position.x);
	ey = (int)Math_Floor(e->Position.y);
	ez = (int)Math_Floor(e->Position.z);

	for (attempts = 0; attempts < 10; attempts++) {
		tx = ex + Random_Next(&mob_rng, MOB_WANDER_RANGE * 2 + 1) - MOB_WANDER_RANGE;
		tz = ez + Random_Next(&mob_rng, MOB_WANDER_RANGE * 2 + 1) - MOB_WANDER_RANGE;
		ty = ey;

		/* Find ground level at target */
		if (World_Contains(tx, ty, tz)) {
			/* Search down for solid ground */
			while (ty > 0 && !Mob_BlockIsSolid(tx, ty - 1, tz)) ty--;
			/* Make sure the mob can stand there (2 blocks of air above) */
			if (ty > 0 && Mob_BlockIsPassable(tx, ty, tz) && Mob_BlockIsPassable(tx, ty + 1, tz)) {
				mobWanderTarget[id].x = (float)tx + 0.5f;
				mobWanderTarget[id].y = (float)ty;
				mobWanderTarget[id].z = (float)tz + 0.5f;
				mobHasTarget[id] = true;
				return;
			}
		}
	}
	/* Failed to find target, will retry next pause */
	mobHasTarget[id] = false;
}

/* Returns true if mob is stuck at a wall it cannot jump over */
static cc_bool Mob_MoveTowards(struct Entity* e, int id, Vec3 target, float speed, float delta) {
	float dx, dz, dist, moveX, moveZ, yawRad, yawDeg;
	float newX, newZ;
	int bx, bz, feetY, headBlockY, jumpHeadY;
	cc_bool blockedX, blockedZ, canJump;

	dx = target.x - e->Position.x;
	dz = target.z - e->Position.z;
	dist = Math_SqrtF(dx * dx + dz * dz);

	if (dist < 0.1f) return false;

	/* Normalize direction and apply speed */
	moveX = (dx / dist) * speed * delta;
	moveZ = (dz / dist) * speed * delta;

	/* Update facing direction: CC's forward = (sin(yaw), -cos(yaw)), so yaw = atan2(dx, -dz) */
	yawRad = Math_Atan2f(-dz, dx);
	yawDeg = yawRad * MATH_RAD2DEG;
	if (id >= 0 && id < MAX_NET_PLAYERS) mobTargetYaw[id] = yawDeg;

	feetY = (int)Math_Floor(e->Position.y);
	newX  = e->Position.x + moveX;
	newZ  = e->Position.z + moveZ;

	/* Block Y that contains the mob's head (e.g. spiders: same as feetY, tall mobs: feetY+1) */
	headBlockY = (int)Math_Floor(e->Position.y + e->Size.y - 0.01f);

	/* Check X-axis collision: block at new X, current Z (feet and head height) */
	bx = (int)Math_Floor(newX);
	bz = (int)Math_Floor(e->Position.z);
	blockedX = Mob_BlockObstructsAt(bx, feetY, bz, e->Position.y);
	if (headBlockY > feetY) blockedX = blockedX || Mob_BlockIsSolid(bx, headBlockY, bz);

	/* Check Z-axis collision: current X, new Z (feet and head height) */
	bx = (int)Math_Floor(e->Position.x);
	bz = (int)Math_Floor(newZ);
	blockedZ = Mob_BlockObstructsAt(bx, feetY, bz, e->Position.y);
	if (headBlockY > feetY) blockedZ = blockedZ || Mob_BlockIsSolid(bx, headBlockY, bz);

	/* Try to jump over 1-block walls */
	canJump = false;
	if ((blockedX || blockedZ) && e->OnGround) {
		bx = (int)Math_Floor(newX);
		bz = (int)Math_Floor(newZ);
		/* Can jump if mob fits above the wall (check head clearance at landing height) */
		jumpHeadY = (int)Math_Floor((float)(feetY + 1) + e->Size.y - 0.01f);
		if (Mob_BlockIsPassable(bx, feetY + 1, bz) && (jumpHeadY <= feetY + 1 || Mob_BlockIsPassable(bx, jumpHeadY, bz))) {
			e->Velocity.y = MOB_JUMP_VEL;
			e->OnGround   = false;
			canJump = true;
		}
	}

	/* While mid-air (jumping), recheck collisions to allow movement over 1-block obstacles */
	/* Only unblock if mob has risen above the blocking block's top surface */
	if (!e->OnGround && e->Velocity.y > 0.0f) {
		if (blockedX) {
			bx = (int)Math_Floor(newX);
			bz = (int)Math_Floor(e->Position.z);
			if (!Mob_BlockObstructsAt(bx, feetY, bz, e->Position.y) && (headBlockY <= feetY || Mob_BlockIsPassable(bx, headBlockY, bz)))
				blockedX = false;
		}
		if (blockedZ) {
			bx = (int)Math_Floor(e->Position.x);
			bz = (int)Math_Floor(newZ);
			if (!Mob_BlockObstructsAt(bx, feetY, bz, e->Position.y) && (headBlockY <= feetY || Mob_BlockIsPassable(bx, headBlockY, bz)))
				blockedZ = false;
		}
	}

	/* Only apply movement on axes that aren't blocked */
	if (!blockedX) {
		e->Position.x  = newX;
		e->next.pos.x  = newX;
		e->prev.pos.x  = newX;
	}
	if (!blockedZ) {
		e->Position.z  = newZ;
		e->next.pos.z  = newZ;
		e->prev.pos.z  = newZ;
	}

	/* Stuck if on ground, blocked, and can't jump over */
	return e->OnGround && (blockedX || blockedZ) && !canJump;
}

/* Custom GetCol that tints mob red when hurt */
static PackedCol MobEntity_GetCol(struct Entity* e) {
	PackedCol col;
	int id, r, g, b;

	col = origNetPlayerVTABLE->GetCol(e);

	for (id = 0; id < MAX_NET_PLAYERS; id++) {
		if (Entities.List[id] == e) break;
	}

	if (id < MAX_NET_PLAYERS && mobHurtFlash[id] > 0.0f) {
		r = PackedCol_R(col);
		g = PackedCol_G(col);
		b = PackedCol_B(col);
		r = r + (255 - r) / 2;
		g = g / 2;
		b = b / 2;
		col = PackedCol_Make(r, g, b, 255);
	}

	/* Tint mob orange when burning in sunlight (light sensitivity) */
	if (id < MAX_NET_PLAYERS && (mobSunDamageTimer[id] > 0.0f || mobLavaDamageTimer[id] > 0.0f)) {
		r = PackedCol_R(col);
		g = PackedCol_G(col);
		b = PackedCol_B(col);
		r = r + (255 - r) / 3;
		g = g * 2 / 3;
		b = b / 3;
		col = PackedCol_Make(r, g, b, 255);
	}

	/* Creeper fuse flash: alternate between entirely white and normal texture */
	if (id < MAX_NET_PLAYERS && mobModelIdx[id] == MOB_IDX_CREEPER
		&& mobCreeperFuse[id] >= 0.0f) {
		float elapsed = CREEPER_ATTACK_FUSE_TIME - mobCreeperFuse[id];
		float fuseTime = CREEPER_ATTACK_FUSE_TIME;
		/* Integrated flash count: slow flashes early, fast near detonation */
		float flashCount = 3.0f * elapsed + 4.0f * elapsed * elapsed / fuseTime;
		if (((int)flashCount) % 2 == 0) {
			col = PACKEDCOL_WHITE;
		}
	}

	return col;
}

/* Custom RenderModel that forces mob facing direction before interpolation */
static void MobEntity_RenderModel(struct Entity* e, float delta, float t) {
	int id;
	float yaw;

	/* Find this entity's mob ID */
	for (id = 0; id < MAX_NET_PLAYERS; id++) {
		if (Entities.List[id] == e) break;
	}

	/* Smooth turning: interpolate mobFacingYaw toward mobTargetYaw */
	if (id < MAX_NET_PLAYERS && mobType[id] != MOB_TYPE_NONE) {
		float diff = mobTargetYaw[id] - mobFacingYaw[id];
		/* Normalize to [-180, 180] */
		while (diff > 180.0f)  diff -= 360.0f;
		while (diff < -180.0f) diff += 360.0f;
		{
			float maxTurn = MOB_TURN_SPEED * delta;
			if (diff > maxTurn)       diff = maxTurn;
			else if (diff < -maxTurn) diff = -maxTurn;
		}
		mobFacingYaw[id] += diff;

		yaw = mobFacingYaw[id];
		e->prev.yaw  = yaw;
		e->next.yaw  = yaw;
		e->prev.rotY = yaw;
		e->next.rotY = yaw;

		/* Death tip-over animation */
		if (mobDeathTimer[id] > 0.0f) {
			/* Fall over in 0.5s, then stay on ground until timer expires */
			float totalDuration = 0.5f;
			float elapsed, progress;
			if (mobModelIdx[id] == MOB_IDX_CREEPER) {
				if (Game_CreeperVariants) {
					if (mobCreeperVariant[id] == CREEPER_VAR_SURVTEST || mobCreeperVariant[id] == CREEPER_VAR_NUKE)
						totalDuration = 1.5f;
				} else if (Game_CreeperBehavior == CREEPER_EXPLODE_DEATH) {
					totalDuration = 1.5f;
				}
			}
			elapsed = totalDuration - mobDeathTimer[id];
			/* Fall-over completes in first 0.5s, then stays at 90 degrees */
			progress = elapsed / 0.5f;
			if (progress > 1.0f) progress = 1.0f;
			e->prev.rotZ = mobDeathRotZ[id] * progress;
			e->next.rotZ = mobDeathRotZ[id] * progress;
		}

		/* Zombie attack arm animation: tick timer for rendering */
		if (mobAttackAnimTimer[id] > 0.0f) {
			mobAttackAnimTimer[id] -= delta;
			if (mobAttackAnimTimer[id] < 0.0f) mobAttackAnimTimer[id] = 0.0f;
		}
	}

	/* Now call original NetPlayer RenderModel (lerp + render) - will get our angles */
	Mob_CurrentRenderingId = id;

	/* Creeper fuse: expand horizontally + flash white during fuse */
	{
		Vec3 savedScale = e->ModelScale;
		if (id < MAX_NET_PLAYERS && mobModelIdx[id] == MOB_IDX_CREEPER
			&& mobCreeperFuse[id] >= 0.0f) {
			float fuseProgress = 1.0f - (mobCreeperFuse[id] / CREEPER_ATTACK_FUSE_TIME);
			float hScale = 1.0f + fuseProgress * 0.15f; /* up to 15% wider */
			e->ModelScale.x *= hScale;
			e->ModelScale.z *= hScale;

			/* Flash: use engine's built-in white texture during "on" phase */
			{
				float elapsed = CREEPER_ATTACK_FUSE_TIME - mobCreeperFuse[id];
				float flashCount = 3.0f * elapsed + 4.0f * elapsed * elapsed / CREEPER_ATTACK_FUSE_TIME;
				if (((int)flashCount) % 2 == 0) {
					Models_UseWhiteTex = true;
				}
			}
		}
		origNetPlayerVTABLE->RenderModel(e, delta, t);
		e->ModelScale      = savedScale;
		Models_UseWhiteTex = false;
	}

	Mob_CurrentRenderingId = -1;
}

static void Mob_ApplyGravity(struct Entity* e, float delta, int id) {
	int bx, by, bz;
	BlockID below;
	float newY;
	cc_bool wasOnGround = e->OnGround;

	bx = (int)Math_Floor(e->Position.x);
	by = (int)Math_Floor(e->Position.y - 0.05f);
	bz = (int)Math_Floor(e->Position.z);

	if (World_Contains(bx, by, bz)) {
		below = World_GetBlock(bx, by, bz);
		if (Blocks.Collide[below] == COLLIDE_SOLID) {
			float blockTop = (float)by + Blocks.MaxBB[below].y;
			if (e->Velocity.y <= 0.0f && e->Position.y <= blockTop + 0.05f) {
				/* Falling/stationary and at or near block surface: land on it */
				e->Velocity.y = 0.0f;
				e->OnGround   = true;
				if (e->Position.y < blockTop) {
					e->Position.y = blockTop;
					e->next.pos.y = blockTop;
					e->prev.pos.y = blockTop;
				}
			} else if (e->Velocity.y > 0.0f) {
				/* Jumping upward: check ceiling collision */
				newY = e->Position.y + e->Velocity.y;
				{
					int headY = (int)Math_Floor(newY + e->Size.y); /* top of mob's head */
					if (Mob_BlockIsSolid(bx, headY, bz)) {
						/* Hit ceiling: stop upward movement */
						e->Velocity.y = 0.0f;
						newY = e->Position.y;
					}
				}
				e->OnGround    = false;
				e->Position.y  = newY;
				e->Velocity.y -= MOB_GRAVITY;
				e->next.pos.y  = e->Position.y;
				e->prev.pos.y  = e->Position.y;
			} else {
				/* Falling but still above block surface (partial block): keep falling */
				newY = e->Position.y + e->Velocity.y;
				if (newY <= blockTop) {
					/* Would fall past block surface: land on it */
					e->Velocity.y = 0.0f;
					e->OnGround   = true;
					e->Position.y = blockTop;
					e->next.pos.y = blockTop;
					e->prev.pos.y = blockTop;
				} else {
					e->OnGround    = false;
					e->Position.y  = newY;
					e->Velocity.y -= MOB_GRAVITY;
					e->next.pos.y  = e->Position.y;
					e->prev.pos.y  = e->Position.y;
				}
			}
		} else {
			/* In air: apply movement with ceiling check */
			newY = e->Position.y + e->Velocity.y;
			if (e->Velocity.y > 0.0f) {
				int headY = (int)Math_Floor(newY + e->Size.y);
				if (Mob_BlockIsSolid(bx, headY, bz)) {
					e->Velocity.y = 0.0f;
					newY = e->Position.y;
				}
			}
			e->OnGround    = false;
			e->Position.y  = newY;
			e->Velocity.y -= MOB_GRAVITY;
			e->next.pos.y  = e->Position.y;
			e->prev.pos.y  = e->Position.y;
		}
	} else if (by < 0) {
		e->Velocity.y = 0.0f;
		e->OnGround   = true;
	} else {
		/* Outside world bounds: also check ceiling */
		newY = e->Position.y + e->Velocity.y;
		if (e->Velocity.y > 0.0f) {
			int headY = (int)Math_Floor(newY + e->Size.y);
			if (World_Contains(bx, headY, bz) && Mob_BlockIsSolid(bx, headY, bz)) {
				e->Velocity.y = 0.0f;
				newY = e->Position.y;
			}
		}
		e->OnGround    = false;
		e->Position.y  = newY;
		e->Velocity.y -= MOB_GRAVITY;
		e->next.pos.y  = e->Position.y;
		e->prev.pos.y  = e->Position.y;
	}

	/* Fall damage tracking */
	if (!wasOnGround && e->OnGround && id >= 0 && id < MAX_NET_PLAYERS) {
		/* Just landed: apply fall damage (skip spiders - they don't take fall damage) */
		if (mobModelIdx[id] != MOB_IDX_SPIDER) {
			int fallDist = (int)(mobFallStartY[id] - e->Position.y);
			if (fallDist > 3) {
				Mob_DamageMob(id, fallDist - 3, false);
			}
		}
		mobFallStartY[id] = e->Position.y;
	} else if (wasOnGround && !e->OnGround && id >= 0 && id < MAX_NET_PLAYERS) {
		/* Just left ground: record start Y */
		mobFallStartY[id] = e->Position.y;
	}
}

/* Spawn an arrow projectile from a skeleton toward the player */
static void Skeleton_ShootArrow(struct Entity* skeleton, int skelId) {
	struct NetPlayer* np;
	struct LocationUpdate update;
	cc_string model;
	Vec3 dir, spawnPos, playerPos;
	float dx, dy, dz, dist;
	int id, slot;

	/* Find a free entity slot */
	for (id = 0; id < MAX_NET_PLAYERS; id++) {
		if (!Entities.List[id]) break;
	}
	if (id == MAX_NET_PLAYERS) return;

	/* Find a free arrow slot */
	for (slot = 0; slot < MAX_ARROWS; slot++) {
		if (!arrowActive[slot]) break;
	}
	if (slot == MAX_ARROWS) return;

	/* Calculate direction from skeleton to player */
	playerPos = Entities.CurPlayer->Base.Position;
	playerPos.y += 1.5f; /* aim at player eye level */
	spawnPos = skeleton->Position;
	spawnPos.y += 1.2f; /* skeleton's "hand" level */

	dx = playerPos.x - spawnPos.x;
	dy = playerPos.y - spawnPos.y;
	dz = playerPos.z - spawnPos.z;
	dist = Math_SqrtF(dx * dx + dy * dy + dz * dz);
	if (dist < 0.1f) return;

	/* Normalize and set velocity */
	arrowVelocity[slot].x = (dx / dist) * ARROW_SPEED;
	arrowVelocity[slot].y = (dy / dist) * ARROW_SPEED + 0.15f; /* slight arc */
	arrowVelocity[slot].z = (dz / dist) * ARROW_SPEED;

	/* Init the entity */
	np = &NetPlayers_List[id];
	NetPlayer_Init(np);
	Entities.List[id] = &np->Base;
	Event_RaiseInt(&EntityEvents.Added, id);

	model = String_FromReadonly("arrow");
	Entity_SetModel(&np->Base, &model);
	np->Base.uScale = 4.0f;
	np->Base.vScale = 4.0f;

	/* Set arrow velocity on entity for GetTransform to compute rotation */
	np->Base.Velocity = arrowVelocity[slot];

	update.flags = LU_HAS_POS;
	update.pos   = spawnPos;
	np->Base.VTABLE->SetLocation(&np->Base, &update);
	np->Base.Position = spawnPos; /* SetLocation doesn't set Position directly */

	/* Track arrow state */
	arrowActive[slot]   = true;
	arrowEntityId[slot] = id;
	arrowLifetime[slot] = 5.0f; /* 5 second flight lifetime */
	arrowIsPlayerArrow[slot] = false;

	Mob_PlaySound(SOUND_SHOOT, spawnPos); /* shoot.wav at distance-attenuated volume */
}

/* Tick all active arrow projectiles */
static void Arrow_TickAll(float delta) {
	int slot, eid;
	struct Entity* e;
	struct Entity* player;
	struct LocationUpdate update;
	Vec3 newPos;
	float dx, dy, dz, distSq;
	int bx, by, bz;

	player = &Entities.CurPlayer->Base;

	for (slot = 0; slot < MAX_ARROWS; slot++) {
		if (!arrowActive[slot]) continue;

		eid = arrowEntityId[slot];
		e   = Entities.List[eid];
		if (!e) { arrowActive[slot] = false; continue; }

		/* If arrow is stuck in a block (velocity=0), just tick lifetime */
		if (arrowVelocity[slot].x == 0.0f && arrowVelocity[slot].y == 0.0f && arrowVelocity[slot].z == 0.0f) {
			/* Player arrows can be picked up in survival mode */
			if (arrowIsPlayerArrow[slot] && Game_SurvivalMode) {
				dx = e->Position.x - player->Position.x;
				dy = e->Position.y - (player->Position.y + 0.9f);
				dz = e->Position.z - player->Position.z;
				distSq = dx * dx + dy * dy + dz * dz;
				if (distSq < 2.25f) { /* 1.5 block radius */
					/* Give player 1 arrow item */
					SurvInv_AddItem(BLOCK_AIR, ITEM_ARROW, 1);
					Entities_Remove(eid);
					arrowActive[slot] = false;
					continue;
				}
			}
			arrowLifetime[slot] -= delta;
			if (arrowLifetime[slot] <= 0.0f) {
				Entities_Remove(eid);
				arrowActive[slot] = false;
			}
			continue;
		}

		/* Apply gravity to arrow */
		arrowVelocity[slot].y -= ARROW_GRAVITY * delta * 20.0f;

		/* Move arrow */
		newPos.x = e->Position.x + arrowVelocity[slot].x * delta * 20.0f;
		newPos.y = e->Position.y + arrowVelocity[slot].y * delta * 20.0f;
		newPos.z = e->Position.z + arrowVelocity[slot].z * delta * 20.0f;

		/* Check collision with solid blocks */
		bx = (int)Math_Floor(newPos.x);
		by = (int)Math_Floor(newPos.y);
		bz = (int)Math_Floor(newPos.z);
		if (World_Contains(bx, by, bz) && Mob_BlockIsSolid(bx, by, bz)) {
			/* Hit a block: stop the arrow and let it stick there for 30 seconds */
			arrowVelocity[slot].x = 0.0f;
			arrowVelocity[slot].y = 0.0f;
			arrowVelocity[slot].z = 0.0f;
			arrowStuckBlock[slot].x = bx;
			arrowStuckBlock[slot].y = by;
			arrowStuckBlock[slot].z = bz;
			arrowLifetime[slot] = arrowIsPlayerArrow[slot] ? 20.0f : 5.0f;
			Mob_PlaySound(SOUND_ARROW, newPos);
			{
				struct LocationUpdate stickUpdate;
				stickUpdate.flags = LU_HAS_POS;
				/* Move arrow 80% toward the block so it appears embedded in the surface */
				stickUpdate.pos.x = e->Position.x + (newPos.x - e->Position.x) * 0.8f;
				stickUpdate.pos.y = e->Position.y + (newPos.y - e->Position.y) * 0.8f;
				stickUpdate.pos.z = e->Position.z + (newPos.z - e->Position.z) * 0.8f;
				e->VTABLE->SetLocation(e, &stickUpdate);
			}
			continue;
		}

		/* Check collision with player (skeleton arrows only) */
		if (!arrowIsPlayerArrow[slot]) {
			dx = newPos.x - player->Position.x;
			dy = newPos.y - (player->Position.y + 0.9f); /* player center */
			dz = newPos.z - player->Position.z;
			distSq = dx * dx + dy * dy + dz * dz;
			if (distSq < 1.0f) {
				/* Hit the player: deal damage and remove arrow */
				Player_Damage(Mob_GetDamageWithMultiplier(SKELETON_ARROW_PLAYER_DAMAGE));
				{
					/* Push player away from arrow */
					float pushX = arrowVelocity[slot].x;
					float pushZ = arrowVelocity[slot].z;
					float pushDist = Math_SqrtF(pushX * pushX + pushZ * pushZ);
					if (pushDist > 0.01f) {
						player->Velocity.x += (pushX / pushDist) * 0.4f;
						player->Velocity.z += (pushZ / pushDist) * 0.4f;
						player->Velocity.y += 0.2f;
					}
				}
				Entities_Remove(eid);
				arrowActive[slot] = false;
				continue;
			}
		}

		/* Check collision with mobs (player arrows only) */
		if (arrowIsPlayerArrow[slot]) {
			int mi;
			cc_bool hitMob = false;
			for (mi = 0; mi < MAX_NET_PLAYERS; mi++) {
				struct Entity* me = Entities.List[mi];
				if (!me || !Mob_IsMob(mi)) continue;
				if (mobDeathTimer[mi] > 0.0f) continue; /* skip dying mobs */
				dx = newPos.x - me->Position.x;
				dy = newPos.y - (me->Position.y + 0.9f);
				dz = newPos.z - me->Position.z;
				distSq = dx * dx + dy * dy + dz * dz;
				if (distSq < 1.0f) {
					/* Hit mob: 1 damage, half knockback */
					Mob_DamageMob(mi, PLAYER_ARROW_DAMAGE, false);
					{
						float pushX = arrowVelocity[slot].x;
						float pushZ = arrowVelocity[slot].z;
						float pushDist = Math_SqrtF(pushX * pushX + pushZ * pushZ);
						if (pushDist > 0.01f) {
							me->Velocity.x += (pushX / pushDist) * 0.2f;
							me->Velocity.z += (pushZ / pushDist) * 0.2f;
							me->Velocity.y += 0.15f;
						}
					}
					Entities_Remove(eid);
					arrowActive[slot] = false;
					hitMob = true;
					break;
				}
			}
			if (hitMob) continue;
		}

		/* Update arrow position and velocity on entity */
		e->Velocity = arrowVelocity[slot];

		update.flags = LU_HAS_POS;
		update.pos   = newPos;
		e->VTABLE->SetLocation(e, &update);

		/* Decrease lifetime */
		arrowLifetime[slot] -= delta;
		if (arrowLifetime[slot] <= 0.0f) {
			Entities_Remove(eid);
			arrowActive[slot] = false;
		}
	}
}

/* When a block is broken, remove any arrows stuck in that block */
static void Arrow_OnBlockChanged(void* obj, IVec3 coords, BlockID old, BlockID now) {
	int slot, eid;
	struct Entity* e;

	/* Only care about blocks being destroyed (replaced with air) */
	if (now != BLOCK_AIR) return;

	for (slot = 0; slot < MAX_ARROWS; slot++) {
		if (!arrowActive[slot]) continue;

		/* Only check stuck arrows (velocity = 0) */
		if (arrowVelocity[slot].x != 0.0f || arrowVelocity[slot].y != 0.0f || arrowVelocity[slot].z != 0.0f)
			continue;

		eid = arrowEntityId[slot];
		e   = Entities.List[eid];
		if (!e) { arrowActive[slot] = false; continue; }

		/* Check if the arrow's stuck block or visual position block matches the broken block */
		{
			int ax = (int)Math_Floor(e->Position.x);
			int ay = (int)Math_Floor(e->Position.y);
			int az = (int)Math_Floor(e->Position.z);
			cc_bool matchStuck  = (arrowStuckBlock[slot].x == coords.x && arrowStuckBlock[slot].y == coords.y && arrowStuckBlock[slot].z == coords.z);
			cc_bool matchVisual = (ax == coords.x && ay == coords.y && az == coords.z);
			if (matchStuck || matchVisual) {
				Entities_Remove(eid);
				arrowActive[slot] = false;
			}
		}
	}
}

static void Arrow_ScheduledTick(struct ScheduledTask* task) {
	/* Pause arrows when a menu is open */
	if (Gui_GetInputGrab()) return;
	Arrow_TickAll((float)task->interval);
}

/* Player environmental damage tick (20 tps) */
static void PlayerDamage_ScheduledTick(struct ScheduledTask* task) {
	struct Entity* player;
	float delta;

	if (!Game_SurvivalMode) return;
	if (Player_Health <= 0) return;
	if (Gui_GetInputGrab()) return;

	player = &Entities.CurPlayer->Base;
	delta  = (float)task->interval;

	/* Tick down invulnerability timer */
	if (playerInvulnTimer > 0.0f) {
		playerInvulnTimer -= delta;
		if (playerInvulnTimer < 0.0f) playerInvulnTimer = 0.0f;
	}

	/* Fall damage tracking (water nullifies fall damage) */
	if (Mob_IsInWater(player)) {
		/* Reset fall start while in water so no fall damage accumulates */
		playerFallStartY = player->Position.y;
	} else if (!playerWasOnGround && player->OnGround) {
		/* Just landed on solid ground */
		int fallDist = (int)(playerFallStartY - player->Position.y);
		if (fallDist > 3) {
			Player_Damage(fallDist - 3);
		}
		playerFallStartY = player->Position.y;
	} else if (playerWasOnGround && !player->OnGround) {
		/* Just left ground */
		playerFallStartY = player->Position.y;
	}
	playerWasOnGround = player->OnGround;

	/* Lava damage: 5 damage per half second */
	if (Mob_IsInLava(player)) {
		playerLavaDamageTimer += delta;
		if (playerLavaDamageTimer >= 0.5f) {
			playerLavaDamageTimer -= 0.5f;
			Player_Damage(5);
		}
		playerOnFireTimer = 3.0f; /* set lingering fire */
	} else {
		playerLavaDamageTimer = 0.0f;
	}

	/* Cactus damage: 1 damage per second */
	if (Mob_IsTouchingCactus(player)) {
		playerCactusDamageTimer += delta;
		if (playerCactusDamageTimer >= 1.0f) {
			playerCactusDamageTimer -= 1.0f;
			Player_Damage(1);
		}
	} else {
		playerCactusDamageTimer = 0.0f;
	}

	/* Fire damage: 1 damage per half second */
	if (Mob_IsInFire(player)) {
		playerFireDamageTimer += delta;
		if (playerFireDamageTimer >= 0.5f) {
			playerFireDamageTimer -= 0.5f;
			Player_Damage(1);
		}
		playerOnFireTimer = 3.0f; /* set lingering fire */
	} else if (playerOnFireTimer <= 0.0f) {
		playerFireDamageTimer = 0.0f;
	}

	/* Lingering fire: burn for 3 seconds after leaving fire/lava source */
	if (playerOnFireTimer > 0.0f && !Mob_IsInLava(player) && !Mob_IsInFire(player)) {
		if (Mob_IsInWater(player)) {
			playerOnFireTimer = 0.0f; /* water extinguishes */
		} else {
			playerOnFireTimer -= delta;
			playerFireDamageTimer += delta;
			if (playerFireDamageTimer >= 0.5f) {
				playerFireDamageTimer -= 0.5f;
				Player_Damage(1);
			}
			if (playerOnFireTimer <= 0.0f) {
				playerOnFireTimer = 0.0f;
				playerFireDamageTimer = 0.0f;
			}
		}
	}
}

static void MobEntity_Tick(struct Entity* e, float delta) {
	int id, j;
	float dx, dy, dz, distSq, dist, rx, rz, rdist;
	Vec3 playerPos;

	/* Pause mob AI when a menu is open */
	if (Gui_GetInputGrab()) return;

	/* Call original NetPlayer tick (interp, skin check, anim update) */
	origNetPlayerVTABLE->Tick(e, delta);

	/* Find this entity's ID in the entity list */
	for (id = 0; id < MAX_NET_PLAYERS; id++) {
		if (Entities.List[id] == e) break;
	}
	if (id >= MAX_NET_PLAYERS) return;

	/* Death animation: count down and remove when done */
	if (mobDeathTimer[id] > 0.0f) {
		mobDeathTimer[id] -= delta;
		if (mobDeathTimer[id] <= 0.0f) {
			/* Creeper: explode on death based on variant or behavior setting */
			if (mobModelIdx[id] == MOB_IDX_CREEPER) {
				int bx = (int)Math_Floor(e->Position.x);
				int by = (int)Math_Floor(e->Position.y);
				int bz = (int)Math_Floor(e->Position.z);
				if (Game_CreeperVariants) {
					/* Variant-specific death explosion */
					if (mobCreeperVariant[id] == CREEPER_VAR_SURVTEST) {
						TNT_Explode(bx, by, bz);
					} else if (mobCreeperVariant[id] == CREEPER_VAR_NUKE) {
						TNT_ExplodeRadius(bx, by, bz, CREEPER_NUKE_POWER);
						Mob_PlaySound(SOUND_EXPLODE_BIG, e->Position);
					}
					/* CREEPER_VAR_STANDARD and CREEPER_VAR_MELEE: no death explosion */
				} else if (Game_CreeperBehavior == CREEPER_EXPLODE_DEATH) {
					TNT_Explode(bx, by, bz);
				}
			}
			/* Animation finished, remove mob */
			Entities_Remove(id);
			mobType[id]        = MOB_TYPE_NONE;
			mobHasTarget[id]   = false;
			mobIsMoving[id]    = false;
			mobHealth[id]      = 0;
			mobDeathTimer[id]  = 0.0f;
			mobHurtFlash[id]   = 0.0f;
			mobIsAggro[id]     = false;
			mobCreeperFuse[id] = -1.0f;
		}
		return; /* Don't run AI while dying */
	}

	/* Creeper explosion attack fuse countdown */
	if (mobModelIdx[id] == MOB_IDX_CREEPER
		&& mobCreeperFuse[id] >= 0.0f) {
		/* Cancel fuse if player is more than 5 blocks away */
		{
			Vec3 pPos = Entities.CurPlayer->Base.Position;
			float fdx = pPos.x - e->Position.x;
			float fdy = pPos.y - e->Position.y;
			float fdz = pPos.z - e->Position.z;
			if (fdx * fdx + fdy * fdy + fdz * fdz > CREEPER_FUSE_CANCEL_RANGE_SQ) {
				mobCreeperFuse[id] = -1.0f;
			}
		}
		if (mobCreeperFuse[id] >= 0.0f) {
			mobCreeperFuse[id] -= delta;
			if (mobCreeperFuse[id] <= 0.0f) {
				/* Fuse done: explode and kill creeper */
				int bx = (int)Math_Floor(e->Position.x);
				int by = (int)Math_Floor(e->Position.y);
				int bz = (int)Math_Floor(e->Position.z);
				cc_bool isNuke = Game_CreeperVariants && mobCreeperVariant[id] == CREEPER_VAR_NUKE;
				Vec3 explodePos;
				explodePos.x = (float)bx + 0.5f;
				explodePos.y = (float)by + 0.5f;
				explodePos.z = (float)bz + 0.5f;

				/* Remove creeper BEFORE explosion so it doesn't take self-damage or drop loot */
				Entities_Remove(id);
				mobType[id]        = MOB_TYPE_NONE;
				mobHasTarget[id]   = false;
				mobIsMoving[id]    = false;
				mobHealth[id]      = 0;
				mobDeathTimer[id]  = 0.0f;
				mobHurtFlash[id]   = 0.0f;
				mobIsAggro[id]     = false;
				mobCreeperFuse[id] = -1.0f;

				if (isNuke) {
					TNT_ExplodeRadius(bx, by, bz, CREEPER_NUKE_POWER);
					Mob_PlaySound(SOUND_EXPLODE_BIG, explodePos);
				} else {
					TNT_Explode(bx, by, bz);
				}
				return;
			}
		}
	}

	/* Decay hurt flash */
	if (mobHurtFlash[id] > 0.0f) {
		mobHurtFlash[id] -= delta;
		if (mobHurtFlash[id] < 0.0f) mobHurtFlash[id] = 0.0f;
	}

	/* Mob light sensitivity: take damage in sunlight */
	if (Game_MobLightSensitivity > 0 && mobDeathTimer[id] <= 0.0f) {
		cc_bool shouldBurn = false;
		if (Game_MobLightSensitivity == 1) {
			/* Undead: only zombies and skeletons */
			shouldBurn = (mobModelIdx[id] == MOB_IDX_ZOMBIE || mobModelIdx[id] == MOB_IDX_SKELETON);
		} else if (Game_MobLightSensitivity == 2) {
			/* All hostile mobs (passive mobs are excluded, brown spiders count as hostile for this) */
			shouldBurn = (mobType[id] == MOB_TYPE_HOSTILE);
		}
		if (shouldBurn) {
			int sx = (int)Math_Floor(e->Position.x);
			int sy = (int)Math_Floor(e->Position.y);
			int sz = (int)Math_Floor(e->Position.z);
			if (World_Contains(sx, sy, sz) && Lighting.IsLit(sx, sy, sz) && !Mob_IsInWater(e) && !DayNightCycle_IsNight()) {
				mobSunDamageTimer[id] += delta;
				if (mobSunDamageTimer[id] >= 1.0f) {
					mobSunDamageTimer[id] -= 1.0f;
					Mob_DamageMob(id, 2, false);
				}
				mobOnFireTimer[id] = 3.0f; /* set lingering fire */
			} else {
				mobSunDamageTimer[id] = 0.0f;
			}
		}
	}

	/* Lava damage: 5 damage per half second */
	if (Mob_IsInLava(e)) {
		mobLavaDamageTimer[id] += delta;
		if (mobLavaDamageTimer[id] >= 0.5f) {
			mobLavaDamageTimer[id] -= 0.5f;
			Mob_DamageMob(id, 5, false);
		}
		mobOnFireTimer[id] = 3.0f; /* set lingering fire */
	} else {
		mobLavaDamageTimer[id] = 0.0f;
	}

	/* Cactus damage: 1 damage per second */
	if (Mob_IsTouchingCactus(e)) {
		mobCactusDamageTimer[id] += delta;
		if (mobCactusDamageTimer[id] >= 1.0f) {
			mobCactusDamageTimer[id] -= 1.0f;
			Mob_DamageMob(id, 1, false);
		}
	} else {
		mobCactusDamageTimer[id] = 0.0f;
	}

	/* Fire damage: 1 damage per half second */
	if (Mob_IsInFire(e)) {
		mobFireDamageTimer[id] += delta;
		if (mobFireDamageTimer[id] >= 0.5f) {
			mobFireDamageTimer[id] -= 0.5f;
			Mob_DamageMob(id, 1, false);
		}
		mobOnFireTimer[id] = 3.0f; /* set lingering fire */
	} else if (mobOnFireTimer[id] <= 0.0f) {
		mobFireDamageTimer[id] = 0.0f;
	}

	/* Lingering fire: burn for 3 seconds after leaving fire/lava/sun source */
	if (mobOnFireTimer[id] > 0.0f && !Mob_IsInLava(e) && !Mob_IsInFire(e)) {
		if (Mob_IsInWater(e)) {
			mobOnFireTimer[id] = 0.0f; /* water extinguishes */
		} else {
			mobOnFireTimer[id] -= delta;
			mobFireDamageTimer[id] += delta;
			if (mobFireDamageTimer[id] >= 0.5f) {
				mobFireDamageTimer[id] -= 0.5f;
				Mob_DamageMob(id, 1, false);
			}
			if (mobOnFireTimer[id] <= 0.0f) {
				mobOnFireTimer[id] = 0.0f;
				mobFireDamageTimer[id] = 0.0f;
			}
		}
	}

	/* Sheep wool regrowth: sheared sheep slowly regain wool over time */
	if (mobModelIdx[id] == MOB_IDX_SHEEP && mobSheepSheared[id]) {
		mobSheepWoolTimer[id] -= delta;
		if (mobSheepWoolTimer[id] <= 0.0f) {
			mobSheepSheared[id] = false;
			mobSheepWoolTimer[id] = 0.0f;
			/* Restore wool model */
			{
				cc_string woolModel = String_FromReadonly("sheep");
				Entity_SetModel(e, &woolModel);
			}
		}
	}

	/* Push mob out of solid blocks (prevents getting stuck in walls) */
	{
		int mx = (int)Math_Floor(e->Position.x);
		int my = (int)Math_Floor(e->Position.y);
		int mz = (int)Math_Floor(e->Position.z);
		if (World_Contains(mx, my, mz) && Mob_BlockIsSolid(mx, my, mz)) {
			/* Search upward in one step to find the first non-solid block */
			int pushY = my;
			while (pushY < World.Height && Mob_BlockIsSolid(mx, pushY, mz)) {
				pushY++;
			}
			/* Only push up if the gap is small (max 2 blocks), otherwise
			   the mob has clipped deep into a wall and should not teleport to the top */
			if (pushY - my <= 2 && pushY < World.Height) {
				BlockID topSolid = World_GetBlock(mx, pushY - 1, mz);
				float pushTop = (float)(pushY - 1) + Blocks.MaxBB[topSolid].y;
				e->Position.y = pushTop;
				e->next.pos.y = pushTop;
				e->prev.pos.y = pushTop;
			}
		}
	}

	/* Apply gravity */
	Mob_ApplyGravity(e, delta, id);

	/* Swimming: if in water, jump continuously to stay afloat */
	if (Mob_IsInWater(e)) {
		if (e->Velocity.y < 0.2f) {
			e->Velocity.y = MOB_JUMP_VEL * 0.6f;
		}
	}

	/* Spider wall climbing: climb walls when chasing the player */
	if (mobModelIdx[id] == MOB_IDX_SPIDER && Game_SpiderWallclimb && mobIsAggro[id]) {
		int sx = (int)Math_Floor(e->Position.x);
		int sy = (int)Math_Floor(e->Position.y);
		int sz = (int)Math_Floor(e->Position.z);
		/* Check if any adjacent block at foot level is solid (spider is against a wall) */
		if (Mob_BlockIsSolid(sx + 1, sy, sz) || Mob_BlockIsSolid(sx - 1, sy, sz) ||
			Mob_BlockIsSolid(sx, sy, sz + 1) || Mob_BlockIsSolid(sx, sy, sz - 1) ||
			Mob_BlockIsSolid(sx + 1, sy + 1, sz) || Mob_BlockIsSolid(sx - 1, sy + 1, sz) ||
			Mob_BlockIsSolid(sx, sy + 1, sz + 1) || Mob_BlockIsSolid(sx, sy + 1, sz - 1)) {
			/* Don't climb if the spider can fit through a gap toward the player */
			/* (spiders are only 12/16 blocks tall and can walk through 1-block gaps) */
			Vec3 plrPos = Entities.CurPlayer->Base.Position;
			float pdx = plrPos.x - e->Position.x;
			float pdz = plrPos.z - e->Position.z;
			float pdist = Math_SqrtF(pdx * pdx + pdz * pdz);
			cc_bool pathBlocked = true;

			if (pdist > 0.1f) {
				int aheadX = (int)Math_Floor(e->Position.x + pdx / pdist);
				int aheadZ = (int)Math_Floor(e->Position.z + pdz / pdist);
				/* Path is clear if block ahead at feet level is passable */
				pathBlocked = Mob_BlockIsSolid(aheadX, sy, aheadZ);
			}

			if (pathBlocked) {
				/* Climb: override gravity with upward speed */
				float climbSpeed = MOB_SPEED * MOB_HOSTILE_SPEED_FACTOR * SPIDER_CLIMB_SPEED_FACTOR * delta;
				e->Velocity.y = climbSpeed;
				e->Position.y += climbSpeed;
				e->next.pos.y = e->Position.y;
				e->prev.pos.y = e->Position.y;
			}
		}
	}

	/* Apply horizontal knockback velocity with wall collision */
	if (e->Velocity.x != 0.0f || e->Velocity.z != 0.0f) {
		float newX = e->Position.x + e->Velocity.x;
		float newZ = e->Position.z + e->Velocity.z;
		float halfW = e->Size.x * 0.5f;
		int bx, by, bz, kbHeadY;
		cc_bool hitX = false, hitZ = false;

		by = (int)Math_Floor(e->Position.y);
		kbHeadY = (int)Math_Floor(e->Position.y + e->Size.y - 0.01f);

		/* Check X movement against solid blocks */
		bx = (int)Math_Floor(newX + (e->Velocity.x > 0 ? halfW : -halfW));
		bz = (int)Math_Floor(e->Position.z);
		if (Mob_BlockIsSolid(bx, by, bz) || (kbHeadY > by && Mob_BlockIsSolid(bx, kbHeadY, bz))) {
			e->Velocity.x = 0.0f;
			hitX = true;
		}

		/* Check Z movement against solid blocks */
		bx = (int)Math_Floor(e->Position.x);
		bz = (int)Math_Floor(newZ + (e->Velocity.z > 0 ? halfW : -halfW));
		if (Mob_BlockIsSolid(bx, by, bz) || (kbHeadY > by && Mob_BlockIsSolid(bx, kbHeadY, bz))) {
			e->Velocity.z = 0.0f;
			hitZ = true;
		}

		if (!hitX) e->Position.x = newX;
		if (!hitZ) e->Position.z = newZ;
		e->next.pos.x  = e->Position.x;
		e->prev.pos.x  = e->Position.x;
		e->next.pos.z  = e->Position.z;
		e->prev.pos.z  = e->Position.z;
		/* Decay horizontal velocity (friction) */
		e->Velocity.x *= 0.8f;
		e->Velocity.z *= 0.8f;
		if (Math_AbsF(e->Velocity.x) < 0.001f) e->Velocity.x = 0.0f;
		if (Math_AbsF(e->Velocity.z) < 0.001f) e->Velocity.z = 0.0f;
	}

	/* Reset backwards flag before AI sets it */
	mobWalkBackwards[id] = false;

	/* Clamp mob position to world boundaries (prevent being knocked off map) */
	{
		float minX = 0.5f, maxX = (float)World.Width - 0.5f;
		float minZ = 0.5f, maxZ = (float)World.Length - 0.5f;
		if (e->Position.x < minX) { e->Position.x = minX; e->Velocity.x = 0.0f; }
		if (e->Position.x > maxX) { e->Position.x = maxX; e->Velocity.x = 0.0f; }
		if (e->Position.z < minZ) { e->Position.z = minZ; e->Velocity.z = 0.0f; }
		if (e->Position.z > maxZ) { e->Position.z = maxZ; e->Velocity.z = 0.0f; }
		e->next.pos.x = e->Position.x;
		e->prev.pos.x = e->Position.x;
		e->next.pos.z = e->Position.z;
		e->prev.pos.z = e->Position.z;
		/* Also prevent falling below y=0 */
		if (e->Position.y < 0.0f) {
			e->Position.y = 0.0f;
			e->Velocity.y = 0.0f;
			e->OnGround   = true;
			e->next.pos.y = 0.0f;
			e->prev.pos.y = 0.0f;
		}
	}

	/* AI behavior based on mob type */
	mobIsMoving[id] = false;

	if (mobType[id] == MOB_TYPE_HOSTILE && !mobIsBrownSpider[id]) {
		/* Check aggro/deaggro based on 3D distance to player */
		playerPos = Entities.CurPlayer->Base.Position;
		dx = playerPos.x - e->Position.x;
		dy = playerPos.y - e->Position.y;
		dz = playerPos.z - e->Position.z;
		distSq = dx * dx + dy * dy + dz * dz;

		if (!mobIsAggro[id] && distSq < MOB_AGGRO_RANGE_SQ) {
			/* Only aggro if mob has line of sight to the player */
			Vec3 mobEye, plrCenter;
			cc_bool playerCrouching = Entities.CurPlayer->Crouching;
			/* Crouching reduces aggro range to 10 blocks unless mob has direct LOS */
			cc_bool inCrouchRange = !playerCrouching || distSq < (10.0f * 10.0f);
			mobEye = e->Position; mobEye.y += e->Size.y * 0.8f;
			plrCenter = playerPos; plrCenter.y += 1.0f;
			if (Mob_HasLineOfSight(mobEye, plrCenter) && inCrouchRange) {
				mobIsAggro[id] = true;
			}
		} else if (mobIsAggro[id] && distSq > MOB_DEAGGRO_RANGE_SQ) {
			/* Always deaggro beyond 24 blocks */
			mobIsAggro[id] = false;
			mobHasTarget[id]   = false;
			mobWanderPause[id] = 0.5f + Random_Float(&mob_rng) * 2.0f;
		} else if (mobIsAggro[id] && distSq > MOB_DEAGGRO_LOS_RANGE_SQ) {
			/* Deaggro beyond 10 blocks if line of sight is blocked */
			Vec3 mobEye2, plrCenter2;
			mobEye2 = e->Position; mobEye2.y += e->Size.y * 0.8f;
			plrCenter2 = playerPos; plrCenter2.y += 1.0f;
			if (!Mob_HasLineOfSight(mobEye2, plrCenter2)) {
				mobIsAggro[id] = false;
				mobHasTarget[id]   = false;
				mobWanderPause[id] = 1.0f + Random_Float(&mob_rng) * 2.0f;
			}
		}

		if (mobIsAggro[id]) {
			/* Creeper: behavior depends on variant or global setting */
			if (mobModelIdx[id] == MOB_IDX_CREEPER) {
				cc_bool useExplosionAI = false;
				if (Game_CreeperVariants) {
					/* Standard and Nuke variants use explosion attack AI */
					useExplosionAI = (mobCreeperVariant[id] == CREEPER_VAR_STANDARD || mobCreeperVariant[id] == CREEPER_VAR_NUKE);
				} else {
					useExplosionAI = (Game_CreeperBehavior == CREEPER_EXPLOSION_ATK);
				}
				if (useExplosionAI) {
					if (distSq < CREEPER_ATTACK_RANGE_SQ && mobCreeperFuse[id] < 0.0f) {
						mobCreeperFuse[id] = CREEPER_ATTACK_FUSE_TIME;
						Mob_PlaySound(SOUND_FUSE, e->Position);
					}
					/* Keep following player at 50% speed while fuse is lit */
					if (mobCreeperFuse[id] >= 0.0f) {
						Mob_MoveTowards(e, id, playerPos, MOB_SPEED * MOB_HOSTILE_SPEED_FACTOR * CREEPER_FUSE_SPEED_FACTOR, delta);
						mobIsMoving[id] = true;
					} else {
						Mob_MoveTowards(e, id, playerPos, MOB_SPEED * MOB_HOSTILE_SPEED_FACTOR, delta);
						mobIsMoving[id] = true;
					}
				} else {
					/* Survtest and Melee variants: chase like zombie */
					Mob_MoveTowards(e, id, playerPos, MOB_SPEED * MOB_HOSTILE_SPEED_FACTOR, delta);
					mobIsMoving[id] = true;
				}
			} else if (mobModelIdx[id] == MOB_IDX_SKELETON && !Game_SkeletonShoot) {
				/* Skeleton with arrows off: chase like zombie */
				{
					float zombieSpeedFactor = (Game_ZombieSpeed + 1) * 0.25f;
					Mob_MoveTowards(e, id, playerPos, MOB_SPEED * zombieSpeedFactor, delta);
					mobIsMoving[id] = true;
				}
			} else if (mobModelIdx[id] == MOB_IDX_SKELETON && Game_SkeletonShoot) {
				/* Skeleton: shoot arrows, backpedal if too close, maintain distance */
				mobSkeletonShootTimer[id] -= delta;
				if (distSq < SKELETON_SHOOT_RANGE_SQ && mobSkeletonShootTimer[id] <= 0.0f) {
					/* Only shoot if we have line of sight to the player */
					Vec3 skelEye, plrPos;
					skelEye = e->Position; skelEye.y += 1.2f;
					plrPos  = playerPos;  plrPos.y  += 1.0f;
					if (Mob_HasLineOfSight(skelEye, plrPos)) {
						Skeleton_ShootArrow(e, id);
						mobSkeletonShootTimer[id] = SKELETON_SHOOT_COOLDOWN;
					}
				}
				if (distSq < SKELETON_BACKPEDAL_RANGE_SQ) {
					/* Too close: backpedal away from player while still facing player */
					Vec3 awayTarget;
					float awayDist = Math_SqrtF(distSq);
					if (awayDist > 0.1f) {
						float skelYawRad;
						awayTarget.x = e->Position.x - (dx / awayDist) * 8.0f;
						awayTarget.y = e->Position.y;
						awayTarget.z = e->Position.z - (dz / awayDist) * 8.0f;
						Mob_MoveTowards(e, id, awayTarget, MOB_SPEED * SKELETON_SPEED_FACTOR, delta);
						/* Override facing to look at player (not away) */
						skelYawRad = Math_Atan2f(-dz, dx);
						mobTargetYaw[id] = skelYawRad * MATH_RAD2DEG;
						mobIsMoving[id] = true;
						mobWalkBackwards[id] = true;
					}
				} else if (distSq > SKELETON_PREFERRED_DIST_SQ) {
					/* Too far: approach to preferred distance */
					Mob_MoveTowards(e, id, playerPos, MOB_SPEED * SKELETON_SPEED_FACTOR, delta);
					mobIsMoving[id] = true;
					mobWalkBackwards[id] = false;
				} else {
					/* At preferred distance: face player, idle */
					float skelYawRad = Math_Atan2f(-dz, dx);
					mobTargetYaw[id] = skelYawRad * MATH_RAD2DEG;
					mobWalkBackwards[id] = false;
				}
			} else if (mobModelIdx[id] == MOB_IDX_SPIDER) {
				/* Spider: chase + leap attack when close */
				Mob_MoveTowards(e, id, playerPos, MOB_SPEED * MOB_HOSTILE_SPEED_FACTOR, delta);
				mobIsMoving[id] = true;

				/* Leap attack when within range, on ground, and cooldown expired */
				if (Game_SpiderLeapDist != SPIDER_LEAP_DONT) {
					float leapRange = (float)(Game_SpiderLeapDist + 2); /* 1=3, 2=4, 3=5, 4=6, 5=7, 6=8 */
					float leapRangeSq = leapRange * leapRange;
					float leapScale = leapRange / 4.0f; /* scale strength relative to default (4 blocks) */
					mobSpiderLeapTimer[id] -= delta;
					if (distSq < leapRangeSq && e->OnGround && mobSpiderLeapTimer[id] <= 0.0f) {
						float leapDist = Math_SqrtF(distSq);
						if (leapDist > 0.1f) {
							/* Check for ceiling/wall above before leaping to avoid jumping into walls */
							/* when the spider could walk through a gap instead */
							int leapAboveY = (int)Math_Floor(e->Position.y) + 1;
							int leapCurBx  = (int)Math_Floor(e->Position.x);
							int leapCurBz  = (int)Math_Floor(e->Position.z);
							int leapAheadX = (int)Math_Floor(e->Position.x + (dx / leapDist));
							int leapAheadZ = (int)Math_Floor(e->Position.z + (dz / leapDist));
							cc_bool ceilingClear = !Mob_BlockIsSolid(leapCurBx, leapAboveY, leapCurBz) &&
							                       !Mob_BlockIsSolid(leapAheadX, leapAboveY, leapAheadZ);
							if (ceilingClear) {
								e->Velocity.x += (dx / leapDist) * SPIDER_LEAP_HORIZONTAL * leapScale;
								e->Velocity.z += (dz / leapDist) * SPIDER_LEAP_HORIZONTAL * leapScale;
								e->Velocity.y  = SPIDER_LEAP_VERTICAL;
								e->OnGround    = false;
							}
						}
						mobSpiderLeapTimer[id] = SPIDER_LEAP_COOLDOWN;
					}
				}

				/* Spider leap contact damage: deal 1.5x melee on contact while airborne (once per leap) */
				if (!e->OnGround && distSq < SPIDER_LEAP_CONTACT_SQ && mobMeleeTimer[id] <= 0.0f) {
					int leapDmg = Mob_GetDamageWithMultiplier((int)(MOB_MELEE_DAMAGE * SPIDER_LEAP_DAMAGE_MULT));
					Player_Damage(leapDmg);
					mobMeleeTimer[id] = MOB_MELEE_COOLDOWN; /* prevent normal melee from also firing */
					/* Same knockback as normal melee attack */
					{
						struct Entity* pe = &Entities.CurPlayer->Base;
						float kbDx = pe->Position.x - e->Position.x;
						float kbDz = pe->Position.z - e->Position.z;
						float kbDist = Math_SqrtF(kbDx * kbDx + kbDz * kbDz);
						if (kbDist > 0.001f) {
							pe->Velocity.x += (kbDx / kbDist) * 0.4f;
							pe->Velocity.z += (kbDz / kbDist) * 0.4f;
							pe->Velocity.y += 0.24f;
						}
					}
				}
			} else if (mobModelIdx[id] == MOB_IDX_ZOMBIE) {
				/* Zombie: chase at configurable speed */
				{
					float zombieSpeedFactor = (Game_ZombieSpeed + 1) * 0.25f; /* 0=25%, 1=50%, 2=75%, 3=100% */
					Mob_MoveTowards(e, id, playerPos, MOB_SPEED * zombieSpeedFactor, delta);
					mobIsMoving[id] = true;
				}
			} else {
				/* Chase the player */
				Mob_MoveTowards(e, id, playerPos, MOB_SPEED * MOB_HOSTILE_SPEED_FACTOR, delta);
				mobIsMoving[id] = true;
			}

			/* === Melee attack check for all aggro hostile mobs === */
			mobMeleeTimer[id] -= delta;
			if (distSq < MOB_MELEE_RANGE_SQ && mobMeleeTimer[id] <= 0.0f) {
				cc_bool canMelee = false;
				int meleeDmg = Mob_GetDamageWithMultiplier(MOB_MELEE_DAMAGE);

				if (mobModelIdx[id] == MOB_IDX_ZOMBIE) {
					canMelee = true;
					mobAttackAnimTimer[id] = MOB_ATTACK_ANIM_DURATION;
				} else if (mobModelIdx[id] == MOB_IDX_SKELETON && !Game_SkeletonShoot) {
					/* Skeleton with arrows off: melee like zombie */
					canMelee = true;
					mobAttackAnimTimer[id] = MOB_ATTACK_ANIM_DURATION;
				} else if (mobModelIdx[id] == MOB_IDX_SPIDER) {
					canMelee = true;
				} else if (mobModelIdx[id] == MOB_IDX_CREEPER) {
					/* Only melee variants (survtest, melee) do melee damage */
					cc_bool isMeleeVariant = false;
					if (Game_CreeperVariants) {
						isMeleeVariant = (mobCreeperVariant[id] == CREEPER_VAR_SURVTEST || mobCreeperVariant[id] == CREEPER_VAR_MELEE);
					} else {
						isMeleeVariant = (Game_CreeperBehavior != CREEPER_EXPLOSION_ATK);
					}
					canMelee = isMeleeVariant;
				}

				if (canMelee) {
					Player_Damage(meleeDmg);
					mobMeleeTimer[id] = MOB_MELEE_COOLDOWN;
					/* Knockback player away from mob (~65% of player-to-mob knockback) */
					{
						struct Entity* pe = &Entities.CurPlayer->Base;
						float kbDx = pe->Position.x - e->Position.x;
						float kbDz = pe->Position.z - e->Position.z;
						float kbDist = Math_SqrtF(kbDx * kbDx + kbDz * kbDz);
						if (kbDist > 0.001f) {
							pe->Velocity.x += (kbDx / kbDist) * 0.4f;
							pe->Velocity.z += (kbDz / kbDist) * 0.4f;
							pe->Velocity.y += 0.24f;
						}
					}
				}
			}
		} else {
				/* Not aggro: behave like passive mob (wander) */
			if (mobWanderPause[id] > 0.0f) {
				mobWanderPause[id] -= delta;
			} else if (!mobHasTarget[id]) {
				Mob_PickWanderTarget(id, e);
				if (!mobHasTarget[id]) {
					mobWanderPause[id] = 2.0f;
				} else {
					mobLastStuckPos[id] = e->Position;
					mobStuckTimer[id]   = 0.0f;
				}
			} else {
				dx = mobWanderTarget[id].x - e->Position.x;
				dz = mobWanderTarget[id].z - e->Position.z;
				distSq = dx * dx + dz * dz;
				if (distSq < 1.0f) {
					mobHasTarget[id]   = false;
					mobWanderPause[id] = 2.0f + Random_Float(&mob_rng) * 4.0f;
				} else {
					cc_bool stuck = Mob_MoveTowards(e, id, mobWanderTarget[id], MOB_SPEED * MOB_PASSIVE_SPEED_FACTOR, delta);
					mobIsMoving[id] = true;
					if (stuck) {
						mobHasTarget[id]   = false;
						mobWanderPause[id] = 0.3f + Random_Float(&mob_rng) * 0.5f;
					}
					/* Position-based stuck detection: if barely moved in 2 seconds, pick new target */
					{
						float sdx = e->Position.x - mobLastStuckPos[id].x;
						float sdz = e->Position.z - mobLastStuckPos[id].z;
						float sDistSq = sdx * sdx + sdz * sdz;
						if (sDistSq < 0.25f) {
							mobStuckTimer[id] += delta;
							if (mobStuckTimer[id] >= 2.0f) {
								mobHasTarget[id]   = false;
								mobWanderPause[id] = 0.3f + Random_Float(&mob_rng) * 0.5f;
								mobStuckTimer[id]  = 0.0f;
							}
						} else {
							mobLastStuckPos[id] = e->Position;
							mobStuckTimer[id]   = 0.0f;
						}
					}
				}
			}
		}

		/* Hostile mob spacing: push away from other nearby hostile mobs */
		for (j = 0; j < MAX_NET_PLAYERS; j++) {
			if (j == id || mobType[j] != MOB_TYPE_HOSTILE) continue;
			if (!Entities.List[j]) continue;

			rx = e->Position.x - Entities.List[j]->Position.x;
			rz = e->Position.z - Entities.List[j]->Position.z;
			rdist = rx * rx + rz * rz;

			if (rdist < MOB_SPACING_DIST_SQ && rdist > 0.001f) {
				float pushX, pushZ;
				int pushBX, pushBZ, pushFeetY;
				dist = Math_SqrtF(rdist);
				pushX = (rx / dist) * 0.05f;
				pushZ = (rz / dist) * 0.05f;
				pushFeetY = (int)Math_Floor(e->Position.y);

				/* Only push on X if destination isn't a solid block */
				pushBX = (int)Math_Floor(e->Position.x + pushX);
				pushBZ = (int)Math_Floor(e->Position.z);
				if (!Mob_BlockIsSolid(pushBX, pushFeetY, pushBZ)) {
					e->Position.x += pushX;
					e->next.pos.x = e->Position.x;
					e->prev.pos.x = e->Position.x;
				}

				/* Only push on Z if destination isn't a solid block */
				pushBX = (int)Math_Floor(e->Position.x);
				pushBZ = (int)Math_Floor(e->Position.z + pushZ);
				if (!Mob_BlockIsSolid(pushBX, pushFeetY, pushBZ)) {
					e->Position.z += pushZ;
					e->next.pos.z = e->Position.z;
					e->prev.pos.z = e->Position.z;
				}
			}
		}

	} else if (mobType[id] == MOB_TYPE_PASSIVE || mobIsBrownSpider[id]) {
		/* Wander randomly */
		if (mobWanderPause[id] > 0.0f) {
			mobWanderPause[id] -= delta;
		} else if (!mobHasTarget[id]) {
			Mob_PickWanderTarget(id, e);
			if (!mobHasTarget[id]) {
				mobWanderPause[id] = 2.0f; /* retry after 2 seconds */
			} else {
				/* Reset stuck tracking when picking a new target */
				mobLastStuckPos[id] = e->Position;
				mobStuckTimer[id]   = 0.0f;
			}
		} else {
			/* Move towards wander target */
			dx = mobWanderTarget[id].x - e->Position.x;
			dz = mobWanderTarget[id].z - e->Position.z;
			distSq = dx * dx + dz * dz;

			if (distSq < 1.0f) {
				/* Reached target, pause then pick new one */
				mobHasTarget[id]    = false;
				mobWanderPause[id]  = 2.0f + Random_Float(&mob_rng) * 4.0f;
			} else {
				cc_bool stuck = Mob_MoveTowards(e, id, mobWanderTarget[id], MOB_SPEED * MOB_PASSIVE_SPEED_FACTOR, delta);
				mobIsMoving[id] = true;
				if (stuck) {
					mobHasTarget[id]   = false;
					mobWanderPause[id] = 0.3f + Random_Float(&mob_rng) * 0.5f;
				}
				/* Position-based stuck detection: if barely moved in 2 seconds, pick new target */
				{
					float sdx = e->Position.x - mobLastStuckPos[id].x;
					float sdz = e->Position.z - mobLastStuckPos[id].z;
					float sDistSq = sdx * sdx + sdz * sdz;
					if (sDistSq < 0.25f) {
						mobStuckTimer[id] += delta;
						if (mobStuckTimer[id] >= 2.0f) {
							mobHasTarget[id]   = false;
							mobWanderPause[id] = 0.3f + Random_Float(&mob_rng) * 0.5f;
							mobStuckTimer[id]  = 0.0f;
						}
					} else {
						mobLastStuckPos[id] = e->Position;
						mobStuckTimer[id]   = 0.0f;
					}
				}
			}
		}
	}

	/* Walking animation when moving, idle when stopped */
	if (mobIsMoving[id]) {
		float walkDir = mobWalkBackwards[id] ? -1.0f : 1.0f;
		e->Anim.WalkTimeO = e->Anim.WalkTimeN;
		e->Anim.WalkTimeN += delta * 6.0f * walkDir;
		e->Anim.SwingO = e->Anim.SwingN;
		e->Anim.SwingN = 1.0f;

		/* Mob footstep sounds: play when legs cross over */
		{
			float oldLeg = Math_CosF(e->Anim.WalkTimeO);
			float newLeg = Math_CosF(e->Anim.WalkTimeN);
			if (Math_Sign(oldLeg) != Math_Sign(newLeg)) {
				/* Get block under mob's feet */
				int fx = (int)Math_Floor(e->Position.x);
				int fy = (int)Math_Floor(e->Position.y - 0.05f);
				int fz = (int)Math_Floor(e->Position.z);
				if (World_Contains(fx, fy, fz)) {
					BlockID footBlock = World_GetBlock(fx, fy, fz);
					cc_uint8 stepType = Blocks.StepSounds[footBlock];
					if (stepType != SOUND_NONE) {
						/* Volume based on distance to player */
						struct Entity* pe = &Entities.CurPlayer->Base;
						float pdx = e->Position.x - pe->Position.x;
						float pdz = e->Position.z - pe->Position.z;
						float pdy = e->Position.y - pe->Position.y;
						float pdistSq = pdx * pdx + pdy * pdy + pdz * pdz;
						int vol = 0;
						if (pdistSq < 1.0f) vol = Audio_SoundsVolume;
						else if (pdistSq < 400.0f) /* within 20 blocks */
							vol = (int)(Audio_SoundsVolume * (1.0f - Math_SqrtF(pdistSq) / 20.0f));
						if (vol > 0) {
							Audio_PlayStepSoundVolume(stepType, vol / 2);
						}
					}
				}
			}
		}
	} else {
		/* Idle: smoothly stop walk animation */
		e->Anim.WalkTimeO = e->Anim.WalkTimeN;
		e->Anim.SwingO = e->Anim.SwingN;
		e->Anim.SwingN *= 0.9f;
		if (e->Anim.SwingN < 0.01f) e->Anim.SwingN = 0.0f;
	}
}

/* ---- Mob sound helper: distance-attenuated dig sounds ---- */
static void Mob_PlaySound(cc_uint8 type, Vec3 mobPos) {
	struct Entity* pe = &Entities.CurPlayer->Base;
	float dx = mobPos.x - pe->Position.x;
	float dy = mobPos.y - pe->Position.y;
	float dz = mobPos.z - pe->Position.z;
	float distSq = dx * dx + dy * dy + dz * dz;
	int vol;

	if (distSq < 1.0f) {
		vol = Audio_SoundsVolume;
	} else if (distSq < 400.0f) { /* within 20 blocks */
		vol = (int)(Audio_SoundsVolume * (1.0f - Math_SqrtF(distSq) / 20.0f));
	} else {
		return; /* too far away to hear */
	}
	if (vol > 0) Audio_PlayDigSoundVolume(type, vol);
}

/* ---- Mob health/damage system ---- */
cc_bool Mob_IsMob(int id) {
	return id >= 0 && id < MAX_NET_PLAYERS && mobType[id] != MOB_TYPE_NONE;
}

cc_bool Mob_IsCreeper(int id) {
	return Mob_IsMob(id) && mobModelIdx[id] == MOB_IDX_CREEPER;
}

cc_bool Mob_IsBurning(int id) {
	if (!Mob_IsMob(id)) {
		/* Check if this is the player entity (ID 255) */
		if (id == ENTITIES_SELF_ID)
			return playerOnFireTimer > 0.0f || playerLavaDamageTimer > 0.0f || playerFireDamageTimer > 0.0f;
		return false;
	}
	return mobSunDamageTimer[id] > 0.0f || mobLavaDamageTimer[id] > 0.0f || mobFireDamageTimer[id] > 0.0f || mobOnFireTimer[id] > 0.0f;
}

int Mob_CurrentRenderingId = -1;

float Mob_GetAttackAnim(int entityId) {
	if (!Mob_IsMob(entityId)) return 0.0f;
	return mobAttackAnimTimer[entityId];
}

void Mob_TriggerCreeperChainExplosion(int id) {
	if (!Mob_IsCreeper(id)) return;
	if (Game_CreeperVariants) {
		/* Melee variant never chain-explodes */
		if (mobCreeperVariant[id] == CREEPER_VAR_MELEE) return;
	} else {
		if (Game_CreeperBehavior == CREEPER_DONT_EXPLODE) return;
	}
	if (mobDeathTimer[id] > 0.0f) return; /* already dying */
	if (mobCreeperFuse[id] >= 0.0f) return; /* already fusing */
	/* Kill immediately and set a short death timer for the explosion */
	mobHealth[id]     = 0;
	mobDeathTimer[id] = 0.2f; /* 0.2 second delay */
	mobDeathRotZ[id]  = (Random_Float(&mob_rng) < 0.5f) ? 90.0f : -90.0f;
	mobHasTarget[id]  = false;
	mobIsMoving[id]   = false;
	if (Entities.List[id]) Mob_PlaySound(SOUND_FUSE, Entities.List[id]->Position);
}

void Mob_DamageMob(int id, int damage, cc_bool fromPlayer) {
	struct Entity* e;

	if (id < 0 || id >= MAX_NET_PLAYERS) return;
	if (mobType[id] == MOB_TYPE_NONE) return;
	if (mobDeathTimer[id] > 0.0f) return; /* Already dying */

	e = Entities.List[id];
	mobHealth[id] -= damage;

	/* Knockback: only from player hits, and only if not already in knockback */
	if (fromPlayer && mobHurtFlash[id] <= 0.0f && e) {
		struct Entity* pe = &Entities.CurPlayer->Base;
		float dx = e->Position.x - pe->Position.x;
		float dz = e->Position.z - pe->Position.z;
		float dist = Math_SqrtF(dx * dx + dz * dz);
		if (dist > 0.001f) {
			e->Velocity.x = (dx / dist) * 0.6f;
			e->Velocity.z = (dz / dist) * 0.6f;
			e->Velocity.y = 0.35f;
		}
	}

	/* Hurt flash */
	mobHurtFlash[id] = 0.5f;

	if (mobHealth[id] <= 0) {
		/* Start death animation: 0.5s fall-over (+ 1s on ground for creeper explode-on-death) */
		float deathDuration = 0.5f;

		/* Play mob-specific death sound (NOT hurt sound) */
		if (e) {
			switch (mobModelIdx[id]) {
				case MOB_IDX_SKELETON: Mob_PlaySound(SOUND_SKELETON_DEATH, e->Position); break;
				case MOB_IDX_CREEPER:  Mob_PlaySound(SOUND_CREEPER_DEATH,  e->Position); break;
				case MOB_IDX_SPIDER:   Mob_PlaySound(SOUND_SPIDER_DEATH,   e->Position); break;
				case MOB_IDX_ZOMBIE:   Mob_PlaySound(SOUND_ZOMBIE_DEATH,   e->Position); break;
				case MOB_IDX_PIG:      Mob_PlaySound(SOUND_PIG_DEATH,      e->Position); break;
				case MOB_IDX_SHEEP:    Mob_PlaySound(SOUND_SHEEP,          e->Position); break;
				default:               Mob_PlaySound(SOUND_HURT,           e->Position); break;
			}
		}

		/* Drop loot in survival mode */
		if (Game_SurvivalMode && e) {
			Vec3 dropPos;
			int lootCount, lootSlot;
			cc_bool goldSword = (fromPlayer && Hotbar_SelectedItem == ITEM_GOLD_SWORD);
			dropPos.x = e->Position.x;
			dropPos.y = e->Position.y + 0.3f;
			dropPos.z = e->Position.z;

			switch (mobModelIdx[id]) {
				case MOB_IDX_ZOMBIE:
					/* Zombie: 0-3 feathers (gold sword: always 3) */
					lootCount = goldSword ? 3 : Random_Next(&mob_rng, 4);
					if (lootCount > 0) {
						lootSlot = DropItem_FindFreeSlot();
						if (lootSlot == -1) lootSlot = DropItem_EvictOldest();
						if (lootSlot != -1) {
							DropItem_Spawn(lootSlot, dropPos, BLOCK_AIR, true, ITEM_FEATHER);
							droppedItemCount[lootSlot] = lootCount;
							droppedItemPickupDelay[lootSlot] = 0.0f;
							DropItem_ApplyRandomMomentum(lootSlot);
						}
					}
					break;
				case MOB_IDX_SKELETON:
					/* Skeleton: 0-3 arrows (gold sword: always 3) */
					lootCount = goldSword ? 3 : Random_Next(&mob_rng, 4);
					if (lootCount > 0) {
						lootSlot = DropItem_FindFreeSlot();
						if (lootSlot == -1) lootSlot = DropItem_EvictOldest();
						if (lootSlot != -1) {
							DropItem_Spawn(lootSlot, dropPos, BLOCK_AIR, true, ITEM_ARROW);
							droppedItemCount[lootSlot] = lootCount;
							droppedItemPickupDelay[lootSlot] = 0.0f;
							DropItem_ApplyRandomMomentum(lootSlot);
						}
					}
					break;
				case MOB_IDX_SPIDER:
					/* Spider (both types): 0-3 string (gold sword: always 3) */
					lootCount = goldSword ? 3 : Random_Next(&mob_rng, 4);
					if (lootCount > 0) {
						lootSlot = DropItem_FindFreeSlot();
						if (lootSlot == -1) lootSlot = DropItem_EvictOldest();
						if (lootSlot != -1) {
							DropItem_Spawn(lootSlot, dropPos, BLOCK_AIR, true, ITEM_STRING);
							droppedItemCount[lootSlot] = lootCount;
							droppedItemPickupDelay[lootSlot] = 0.0f;
							DropItem_ApplyRandomMomentum(lootSlot);
						}
					}
					break;
				case MOB_IDX_CREEPER:
					/* Creeper loot depends on variant or global behavior */
					if (Game_CreeperVariants) {
						if (mobCreeperVariant[id] == CREEPER_VAR_STANDARD || mobCreeperVariant[id] == CREEPER_VAR_NUKE) {
							/* Explosion variants: 0-3 sulphur (gold sword: always 3) */
							lootCount = goldSword ? 3 : Random_Next(&mob_rng, 4);
							if (lootCount > 0) {
								lootSlot = DropItem_FindFreeSlot();
								if (lootSlot == -1) lootSlot = DropItem_EvictOldest();
								if (lootSlot != -1) {
									DropItem_Spawn(lootSlot, dropPos, BLOCK_AIR, true, ITEM_SULPHUR);
									droppedItemCount[lootSlot] = lootCount;
									droppedItemPickupDelay[lootSlot] = 0.0f;
									DropItem_ApplyRandomMomentum(lootSlot);
								}
							}
						} else if (mobCreeperVariant[id] == CREEPER_VAR_SURVTEST) {
							/* Survtest: explodes on death, 1 in 10 chance TNT */
							if (Random_Next(&mob_rng, 10) == 0) {
								lootSlot = DropItem_FindFreeSlot();
								if (lootSlot == -1) lootSlot = DropItem_EvictOldest();
								if (lootSlot != -1) {
									DropItem_Spawn(lootSlot, dropPos, BLOCK_TNT, false, 0);
									droppedItemPickupDelay[lootSlot] = 0.0f;
									DropItem_ApplyRandomMomentum(lootSlot);
								}
							}
						} else if (mobCreeperVariant[id] == CREEPER_VAR_MELEE) {
							/* Melee variant (don't explode): 0-3 flint (gold sword: always 3) */
							lootCount = goldSword ? 3 : Random_Next(&mob_rng, 4);
							if (lootCount > 0) {
								lootSlot = DropItem_FindFreeSlot();
								if (lootSlot == -1) lootSlot = DropItem_EvictOldest();
								if (lootSlot != -1) {
									DropItem_Spawn(lootSlot, dropPos, BLOCK_AIR, true, ITEM_FLINT);
									droppedItemCount[lootSlot] = lootCount;
									droppedItemPickupDelay[lootSlot] = 0.0f;
									DropItem_ApplyRandomMomentum(lootSlot);
								}
							}
						}
					} else {
						/* Global creeper behavior setting */
						if (Game_CreeperBehavior == CREEPER_EXPLOSION_ATK) {
							/* Explode on attack (default): 0-3 sulphur (gold sword: always 3) */
							lootCount = goldSword ? 3 : Random_Next(&mob_rng, 4);
							if (lootCount > 0) {
								lootSlot = DropItem_FindFreeSlot();
								if (lootSlot == -1) lootSlot = DropItem_EvictOldest();
								if (lootSlot != -1) {
									DropItem_Spawn(lootSlot, dropPos, BLOCK_AIR, true, ITEM_SULPHUR);
									droppedItemCount[lootSlot] = lootCount;
									droppedItemPickupDelay[lootSlot] = 0.0f;
									DropItem_ApplyRandomMomentum(lootSlot);
								}
							}
						} else if (Game_CreeperBehavior == CREEPER_EXPLODE_DEATH) {
							/* Explode on death: 1 in 10 chance TNT */
							if (Random_Next(&mob_rng, 10) == 0) {
								lootSlot = DropItem_FindFreeSlot();
								if (lootSlot == -1) lootSlot = DropItem_EvictOldest();
								if (lootSlot != -1) {
									DropItem_Spawn(lootSlot, dropPos, BLOCK_TNT, false, 0);
									droppedItemPickupDelay[lootSlot] = 0.0f;
									DropItem_ApplyRandomMomentum(lootSlot);
								}
							}
						} else {
							/* Don't explode: 0-3 flint (gold sword: always 3) */
							lootCount = goldSword ? 3 : Random_Next(&mob_rng, 4);
							if (lootCount > 0) {
								lootSlot = DropItem_FindFreeSlot();
								if (lootSlot == -1) lootSlot = DropItem_EvictOldest();
								if (lootSlot != -1) {
									DropItem_Spawn(lootSlot, dropPos, BLOCK_AIR, true, ITEM_FLINT);
									droppedItemCount[lootSlot] = lootCount;
									droppedItemPickupDelay[lootSlot] = 0.0f;
									DropItem_ApplyRandomMomentum(lootSlot);
								}
							}
						}
					}
					break;
				case MOB_IDX_PIG:
					/* Pig: 0-2 pork (gold sword: always 2) */
					/* Drop cooked porkchop if pig was on fire or in lava */
					{
						int porkItem = ITEM_RAW_PORK;
						if (mobOnFireTimer[id] > 0.0f || Mob_IsInLava(e) || Mob_IsInFire(e)) {
							porkItem = ITEM_COOKED_PORK;
						}
						lootCount = goldSword ? 2 : Random_Next(&mob_rng, 3);
						if (lootCount > 0) {
							lootSlot = DropItem_FindFreeSlot();
							if (lootSlot == -1) lootSlot = DropItem_EvictOldest();
							if (lootSlot != -1) {
								DropItem_Spawn(lootSlot, dropPos, BLOCK_AIR, true, porkItem);
								droppedItemCount[lootSlot] = lootCount;
								droppedItemPickupDelay[lootSlot] = 0.0f;
								DropItem_ApplyRandomMomentum(lootSlot);
							}
						}
					}
					break;
				case MOB_IDX_SHEEP:
					/* Sheep: drop 1-3 wool if not yet sheared AND killed by player (gold sword: always 3) */
					if (!mobSheepSheared[id] && fromPlayer) {
						int woolCount = goldSword ? 3 : Random_Next(&mob_rng, 3) + 1;
						mobSheepSheared[id] = true;
						lootSlot = DropItem_FindFreeSlot();
						if (lootSlot == -1) lootSlot = DropItem_EvictOldest();
						if (lootSlot != -1) {
							DropItem_Spawn(lootSlot, dropPos, BLOCK_WHITE, false, 0);
							droppedItemCount[lootSlot] = woolCount;
							droppedItemPickupDelay[lootSlot] = 0.0f;
							DropItem_ApplyRandomMomentum(lootSlot);
						}
					}
					break;
			}
		}

		if (mobModelIdx[id] == MOB_IDX_CREEPER) {
			if (Game_CreeperVariants) {
				/* Only survtest and nuke explode on death, need extended timer */
				if (mobCreeperVariant[id] == CREEPER_VAR_SURVTEST || mobCreeperVariant[id] == CREEPER_VAR_NUKE)
					deathDuration = 1.5f;
			} else if (Game_CreeperBehavior == CREEPER_EXPLODE_DEATH) {
				deathDuration = 1.5f;
			}
		}
		mobHealth[id]     = 0;
		mobDeathTimer[id] = deathDuration;
		mobDeathRotZ[id]  = (Random_Float(&mob_rng) < 0.5f) ? 90.0f : -90.0f;
		mobHasTarget[id]  = false;
		mobIsMoving[id]   = false;
	} else {
		/* Play mob-specific hurt sound */
		if (e) {
			switch (mobModelIdx[id]) {
				case MOB_IDX_SKELETON: Mob_PlaySound(SOUND_SKELETON_HURT, e->Position); break;
				case MOB_IDX_CREEPER:  Mob_PlaySound(SOUND_CREEPER_HURT,  e->Position); break;
				case MOB_IDX_SPIDER:   Mob_PlaySound(SOUND_SPIDER_HURT,   e->Position); break;
				case MOB_IDX_ZOMBIE:   Mob_PlaySound(SOUND_ZOMBIE_HURT,   e->Position); break;
				case MOB_IDX_PIG:      Mob_PlaySound(SOUND_PIG_HURT,      e->Position); break;
				case MOB_IDX_SHEEP:    Mob_PlaySound(SOUND_SHEEP,         e->Position); break;
				default:               Mob_PlaySound(SOUND_HURT,          e->Position); break;
			}
		}

		/* Sheep wool shearing: on first hit by player, drop 1-3 white cloth and remove wool layer */
		if (Game_SurvivalMode && e && fromPlayer && mobModelIdx[id] == MOB_IDX_SHEEP && !mobSheepSheared[id]) {
			cc_bool goldSwordShear = (fromPlayer && Hotbar_SelectedItem == ITEM_GOLD_SWORD);
			int woolCount = goldSwordShear ? 3 : Random_Next(&mob_rng, 3) + 1;
			Vec3 woolPos;
			int woolSlot;
			mobSheepSheared[id] = true;
			mobSheepWoolTimer[id] = SHEEP_WOOL_REGROW_MIN + Random_Float(&mob_rng) * (SHEEP_WOOL_REGROW_MAX - SHEEP_WOOL_REGROW_MIN);
			{
				cc_string shearedModel = String_FromReadonly("sheep_nofur");
				Entity_SetModel(e, &shearedModel);
			}
			woolPos.x = e->Position.x;
			woolPos.y = e->Position.y + 0.3f;
			woolPos.z = e->Position.z;
			woolSlot = DropItem_FindFreeSlot();
			if (woolSlot == -1) woolSlot = DropItem_EvictOldest();
			if (woolSlot != -1) {
				DropItem_Spawn(woolSlot, woolPos, BLOCK_WHITE, false, 0);
				droppedItemCount[woolSlot] = woolCount;
				droppedItemPickupDelay[woolSlot] = 0.0f;
				DropItem_ApplyRandomMomentum(woolSlot);
			}
		}
	}
}

void Mob_RemoveAllMobs(void) {
	int i;
	for (i = 0; i < MAX_NET_PLAYERS; i++) {
		if (mobType[i] != MOB_TYPE_NONE) {
			Entities_Remove(i);
			mobType[i]        = MOB_TYPE_NONE;
			mobHasTarget[i]   = false;
			mobIsMoving[i]    = false;
			mobHealth[i]      = 0;
			mobDeathTimer[i]  = 0.0f;
			mobHurtFlash[i]   = 0.0f;
			mobIsAggro[i]     = false;
			mobCreeperFuse[i] = -1.0f;
			mobSkeletonShootTimer[i] = 0.0f;
			mobWalkBackwards[i] = false;
			mobTargetYaw[i] = 0.0f;
			mobSpiderLeapTimer[i] = 0.0f;
			mobFallStartY[i] = 0.0f;
			mobIsBrownSpider[i] = false;
			mobSunDamageTimer[i] = 0.0f;
			mobLavaDamageTimer[i] = 0.0f;
			mobCactusDamageTimer[i] = 0.0f;
			mobCreeperVariant[i] = CREEPER_VAR_STANDARD;
			mobSheepSheared[i] = false;
		}
	}
	/* Remove all active arrows */
	for (i = 0; i < MAX_ARROWS; i++) {
		if (arrowActive[i]) {
			Entities_Remove(arrowEntityId[i]);
			arrowActive[i] = false;
		}
	}
}

/* Try to punch a mob with empty hand. Returns true if a mob was hit. */
static cc_bool Mob_TryPunchMob(void) {
	struct Entity* p = &Entities.CurPlayer->Base;
	int targetId, damage, itemId;

	/* Can attack with empty hand or while holding an item (not a block) */
	if (Inventory_SelectedBlock != BLOCK_AIR && Hotbar_SelectedItem == ITEM_NONE) return false;

	targetId = Entities_GetClosest(p);
	if (targetId < 0 || !Mob_IsMob(targetId)) return false;

	/* Mob is invulnerable during hurt flash (red tint) */
	if (mobHurtFlash[targetId] > 0.0f) return false;

	/* Range check: max 4 blocks */
	{
		struct Entity* target = Entities.List[targetId];
		float tdx = target->Position.x - p->Position.x;
		float tdy = target->Position.y - p->Position.y;
		float tdz = target->Position.z - p->Position.z;
		if (tdx * tdx + tdy * tdy + tdz * tdz > 16.0f) return false;
	}

	/* Play punch animation */
	HeldBlockRenderer_ClickAnim(true);

	/* Determine damage from held item */
	itemId = Hotbar_SelectedItem;
	if (itemId > ITEM_NONE && itemId < ITEM_COUNT) {
		damage = ItemDamage[itemId];
		if (damage <= 0) damage = ITEM_BARE_HAND_DAMAGE;
	} else {
		/* Creative mode: instant kill with bare hand */
		damage = Game_SurvivalMode ? ITEM_BARE_HAND_DAMAGE : 100;
	}

	Mob_DamageMob(targetId, damage, true);
	return true;
}

static void SpawnRandomMob(void) {
	struct LocalPlayer* p;
	struct Entity* pe;
	struct NetPlayer* np;
	struct LocationUpdate update;
	cc_string model, name;
	Vec3 dir, spawnPos;
	int id, idx;
	float yawRad;

	if (!mob_rng_inited) {
		Random_SeedFromCurrentTime(&mob_rng);
		mob_rng_inited = true;
	}

	/* Find a free entity ID */
	for (id = 0; id < MAX_NET_PLAYERS; id++) {
		if (!Entities.List[id]) break;
	}
	if (id == MAX_NET_PLAYERS) {
		Chat_Add1("&cNo free entity slots to spawn mob.", NULL);
		return;
	}

	/* Pick a random mob model */
	idx = Random_Next(&mob_rng, 6);

	/* Calculate spawn position 3 blocks in front of player */
	p      = Entities.CurPlayer;
	pe     = &p->Base;
	yawRad = pe->Yaw * MATH_DEG2RAD;
	dir.x  = -Math_SinF(yawRad);
	dir.y  = 0.0f;
	dir.z  = -Math_CosF(yawRad);
	Vec3_Mul1(&dir, &dir, 3.0f);
	Vec3_Add(&spawnPos, &pe->Position, &dir);

	/* Validate spawn position: ensure mob won't spawn inside solid blocks */
	{
		int sx = (int)Math_Floor(spawnPos.x);
		int sy = (int)Math_Floor(spawnPos.y);
		int sz = (int)Math_Floor(spawnPos.z);
		/* If spawn position is inside a solid block, try moving up */
		while (sy < World.Height - 2 && (Mob_BlockIsSolid(sx, sy, sz) || Mob_BlockIsSolid(sx, sy + 1, sz))) {
			sy++;
		}
		if (Mob_BlockIsSolid(sx, sy, sz) || Mob_BlockIsSolid(sx, sy + 1, sz)) {
			Chat_Add1("&cCannot spawn mob here (blocked).", NULL);
			return;
		}
		spawnPos.y = (float)sy;
	}

	/* Init the entity */
	np = &NetPlayers_List[id];
	NetPlayer_Init(np);

	/* Set up mob VTABLE (copy from NetPlayer, override Tick) */
	if (!mob_vtable_inited) {
		origNetPlayerVTABLE = np->Base.VTABLE;
		mobEntity_VTABLE    = *origNetPlayerVTABLE;
		mobEntity_VTABLE.Tick        = MobEntity_Tick;
		mobEntity_VTABLE.RenderModel = MobEntity_RenderModel;
		mobEntity_VTABLE.GetCol      = MobEntity_GetCol;
		mob_vtable_inited            = true;
	}
	np->Base.VTABLE = &mobEntity_VTABLE;

	Entities.List[id] = &np->Base;
	Event_RaiseInt(&EntityEvents.Added, id);

	/* Set mob AI type and health */
	mobType[id]        = mobIsHostile[idx] ? MOB_TYPE_HOSTILE : MOB_TYPE_PASSIVE;
	mobHealth[id]      = Mob_GetHealthWithMultiplier(mobIsHostile[idx] ? MOB_HP_HOSTILE : MOB_HP_PASSIVE);
	mobHasTarget[id]   = false;
	mobWanderPause[id] = 0.5f + Random_Float(&mob_rng) * 2.0f;
	mobDeathTimer[id]  = 0.0f;
	mobHurtFlash[id]   = 0.0f;
	mobIsAggro[id]     = false;
	mobModelIdx[id]    = (cc_uint8)idx;
	mobCreeperFuse[id] = -1.0f;
	mobSkeletonShootTimer[id] = SKELETON_SHOOT_COOLDOWN;
	mobWalkBackwards[id] = false;
	mobTargetYaw[id] = 0.0f;
	mobSpiderLeapTimer[id] = SPIDER_LEAP_COOLDOWN;
	mobFallStartY[id] = spawnPos.y;
	mobIsBrownSpider[id] = false;
	mobSunDamageTimer[id] = 0.0f;
	mobLavaDamageTimer[id] = 0.0f;
	mobCactusDamageTimer[id] = 0.0f;
	mobFireDamageTimer[id] = 0.0f;
	mobOnFireTimer[id] = 0.0f;
	mobCreeperVariant[id] = CREEPER_VAR_STANDARD;
	mobSheepSheared[id] = false;

	/* Set model */
	model = String_FromReadonly(mobModelNames[idx]);
	Entity_SetModel(&np->Base, &model);

	/* Creeper variants: 50% standard, 25% survival test, 25% melee */
	if (idx == MOB_IDX_CREEPER && Game_CreeperVariants) {
		int roll = Random_Next(&mob_rng, 100);
		if (roll < 25) {
			mobCreeperVariant[id] = CREEPER_VAR_SURVTEST;
			model = String_FromReadonly("creepera");
			Entity_SetModel(&np->Base, &model);
		} else if (roll < 50) {
			mobCreeperVariant[id] = CREEPER_VAR_MELEE;
			model = String_FromReadonly("creeperb");
			Entity_SetModel(&np->Base, &model);
		}
	}

	/* 3% chance for brown spider variant (hostile type, passive behavior) */
	if (idx == MOB_IDX_SPIDER && Game_SpiderVariants && Random_Next(&mob_rng, 100) < 3) {
		model = String_FromReadonly("spiderb");
		Entity_SetModel(&np->Base, &model);
		mobIsBrownSpider[id] = true;
	}

	/* Set name */
	name = String_FromReadonly(mobDisplayNames[idx]);
	Entity_SetName(&np->Base, &name);

	/* Teleport to spawn position */
	Mem_Set(&update, 0, sizeof(update));
	update.flags = LU_HAS_POS | LU_HAS_YAW | LU_HAS_PITCH;
	update.pos   = spawnPos;
	update.yaw   = (float)Random_Next(&mob_rng, 360);
	update.pitch = 0.0f;
	np->Base.VTABLE->SetLocation(&np->Base, &update);

	Chat_Add1("&aSpawned %c", mobDisplayNames[idx]);
}

/* Spawn a mob at an arbitrary position. Returns true on success. */
static cc_bool SpawnMobAt(Vec3 spawnPos, int idx) {
	struct NetPlayer* np;
	struct LocationUpdate update;
	cc_string model, name;
	int id;

	if (!mob_rng_inited) {
		Random_SeedFromCurrentTime(&mob_rng);
		mob_rng_inited = true;
	}

	/* Find a free entity ID */
	for (id = 0; id < MAX_NET_PLAYERS; id++) {
		if (!Entities.List[id]) break;
	}
	if (id == MAX_NET_PLAYERS) return false;

	/* Validate spawn position: ensure mob won't spawn inside solid blocks */
	{
		int sx = (int)Math_Floor(spawnPos.x);
		int sy = (int)Math_Floor(spawnPos.y);
		int sz = (int)Math_Floor(spawnPos.z);
		while (sy < World.Height - 2 && (Mob_BlockIsSolid(sx, sy, sz) || Mob_BlockIsSolid(sx, sy + 1, sz))) {
			sy++;
		}
		if (Mob_BlockIsSolid(sx, sy, sz) || Mob_BlockIsSolid(sx, sy + 1, sz)) return false;
		spawnPos.y = (float)sy;
	}

	/* Init the entity */
	np = &NetPlayers_List[id];
	NetPlayer_Init(np);

	if (!mob_vtable_inited) {
		origNetPlayerVTABLE = np->Base.VTABLE;
		mobEntity_VTABLE    = *origNetPlayerVTABLE;
		mobEntity_VTABLE.Tick        = MobEntity_Tick;
		mobEntity_VTABLE.RenderModel = MobEntity_RenderModel;
		mobEntity_VTABLE.GetCol      = MobEntity_GetCol;
		mob_vtable_inited            = true;
	}
	np->Base.VTABLE = &mobEntity_VTABLE;

	Entities.List[id] = &np->Base;
	Event_RaiseInt(&EntityEvents.Added, id);

	mobType[id]        = mobIsHostile[idx] ? MOB_TYPE_HOSTILE : MOB_TYPE_PASSIVE;
	mobHealth[id]      = Mob_GetHealthWithMultiplier(mobIsHostile[idx] ? MOB_HP_HOSTILE : MOB_HP_PASSIVE);
	mobHasTarget[id]   = false;
	mobWanderPause[id] = 0.5f + Random_Float(&mob_rng) * 2.0f;
	mobDeathTimer[id]  = 0.0f;
	mobHurtFlash[id]   = 0.0f;
	mobIsAggro[id]     = false;
	mobModelIdx[id]    = (cc_uint8)idx;
	mobCreeperFuse[id] = -1.0f;
	mobSkeletonShootTimer[id] = SKELETON_SHOOT_COOLDOWN;
	mobWalkBackwards[id] = false;
	mobTargetYaw[id] = 0.0f;
	mobSpiderLeapTimer[id] = SPIDER_LEAP_COOLDOWN;
	mobFallStartY[id] = spawnPos.y;
	mobIsBrownSpider[id] = false;
	mobSunDamageTimer[id] = 0.0f;
	mobCreeperVariant[id] = CREEPER_VAR_STANDARD;
	mobSheepSheared[id] = false;

	model = String_FromReadonly(mobModelNames[idx]);
	Entity_SetModel(&np->Base, &model);

	/* Creeper variants: 50% standard, 25% survival test, 25% melee */
	if (idx == MOB_IDX_CREEPER && Game_CreeperVariants) {
		int roll = Random_Next(&mob_rng, 100);
		if (roll < 25) {
			/* Survival test variant: melee + explode on death */
			mobCreeperVariant[id] = CREEPER_VAR_SURVTEST;
			model = String_FromReadonly("creepera");
			Entity_SetModel(&np->Base, &model);
		} else if (roll < 50) {
			/* Melee variant: melee only, never explodes */
			mobCreeperVariant[id] = CREEPER_VAR_MELEE;
			model = String_FromReadonly("creeperb");
			Entity_SetModel(&np->Base, &model);
		}
		/* else: standard variant (50%), uses default creeper model */
	}

	/* 3% chance for brown spider variant (hostile type, passive behavior) */
	if (idx == MOB_IDX_SPIDER && Game_SpiderVariants && Random_Next(&mob_rng, 100) < 3) {
		model = String_FromReadonly("spiderb");
		Entity_SetModel(&np->Base, &model);
		mobIsBrownSpider[id] = true;
	}

	name = String_FromReadonly(mobDisplayNames[idx]);
	Entity_SetName(&np->Base, &name);

	Mem_Set(&update, 0, sizeof(update));
	update.flags = LU_HAS_POS | LU_HAS_YAW | LU_HAS_PITCH;
	update.pos   = spawnPos;
	update.yaw   = (float)Random_Next(&mob_rng, 360);
	update.pitch = 0.0f;
	np->Base.VTABLE->SetLocation(&np->Base, &update);
	return true;
}

/* Spawn a mob at a specific entity ID and position (used by save/load).
   Unlike SpawnMobAt, this takes a fixed entity ID and skips solid-block validation. */
void Mob_SpawnAt(int id, int modelIdx, Vec3 pos) {
	struct NetPlayer* np;
	struct LocationUpdate update;
	cc_string model, name;

	if (!mob_rng_inited) {
		Random_SeedFromCurrentTime(&mob_rng);
		mob_rng_inited = true;
	}

	/* Create 1x1 solid white texture for creeper flash effect (once) */
	if (!mob_whiteTex) {
		BitmapCol whitePixel = BITMAPCOLOR_WHITE;
		struct Bitmap bmp;
		Bitmap_Init(bmp, 1, 1, &whitePixel);
		mob_whiteTex = Gfx_CreateTexture(&bmp, 0, false);
	}

	if (id < 0 || id >= MAX_NET_PLAYERS) return;
	if (modelIdx < 0 || modelIdx >= (int)Array_Elems(mobModelNames)) return;

	/* Remove existing entity at this slot if any */
	if (Entities.List[id]) {
		Entities_Remove((EntityID)id);
	}

	np = &NetPlayers_List[id];
	NetPlayer_Init(np);

	if (!mob_vtable_inited) {
		origNetPlayerVTABLE = np->Base.VTABLE;
		mobEntity_VTABLE    = *origNetPlayerVTABLE;
		mobEntity_VTABLE.Tick        = MobEntity_Tick;
		mobEntity_VTABLE.RenderModel = MobEntity_RenderModel;
		mobEntity_VTABLE.GetCol      = MobEntity_GetCol;
		mob_vtable_inited            = true;
	}
	np->Base.VTABLE = &mobEntity_VTABLE;

	Entities.List[id] = &np->Base;
	Event_RaiseInt(&EntityEvents.Added, id);

	mobType[id]        = mobIsHostile[modelIdx] ? MOB_TYPE_HOSTILE : MOB_TYPE_PASSIVE;
	mobHealth[id]      = Mob_GetHealthWithMultiplier(mobIsHostile[modelIdx] ? MOB_HP_HOSTILE : MOB_HP_PASSIVE);
	mobHasTarget[id]   = false;
	mobWanderPause[id] = 0.5f;
	mobDeathTimer[id]  = 0.0f;
	mobHurtFlash[id]   = 0.0f;
	mobIsAggro[id]     = false;
	mobModelIdx[id]    = (cc_uint8)modelIdx;
	mobCreeperFuse[id] = -1.0f;
	mobSkeletonShootTimer[id] = SKELETON_SHOOT_COOLDOWN;
	mobWalkBackwards[id] = false;
	mobTargetYaw[id] = 0.0f;
	mobSpiderLeapTimer[id] = SPIDER_LEAP_COOLDOWN;
	mobFallStartY[id] = pos.y;
	mobIsBrownSpider[id] = false;
	mobSunDamageTimer[id] = 0.0f;
	mobCreeperVariant[id] = CREEPER_VAR_STANDARD;
	mobFacingYaw[id] = 0.0f;
	mobSheepSheared[id] = false;

	model = String_FromReadonly(mobModelNames[modelIdx]);
	Entity_SetModel(&np->Base, &model);

	name = String_FromReadonly(mobDisplayNames[modelIdx]);
	Entity_SetName(&np->Base, &name);

	Mem_Set(&update, 0, sizeof(update));
	update.flags = LU_HAS_POS | LU_HAS_YAW | LU_HAS_PITCH;
	update.pos   = pos;
	update.yaw   = 0.0f;
	update.pitch = 0.0f;
	np->Base.VTABLE->SetLocation(&np->Base, &update);
}

static cc_bool BindTriggered_SpawnMob(int key, struct InputDevice* device) {
	if (Gui.InputGrab) return false;
	/* Block mob spawning in survival unless cheats enabled */
	if (Game_SurvivalMode && !Player_CheatsEnabled) return false;
	SpawnRandomMob();
	return true;
}

/*########################################################################################################################*
*------------------------------------------------------/boom Command------------------------------------------------------*
*#########################################################################################################################*/
/* Spawn a nuke creeper at the player's position (easter egg command) */
static void BoomCommand_Execute(const cc_string* args, int argsCount) {
	struct Entity* pe = &Entities.CurPlayer->Base;
	Vec3 spawnPos = pe->Position;
	struct NetPlayer* np;
	struct LocationUpdate update;
	cc_string model, name;
	int id;

	if (!mob_rng_inited) {
		Random_SeedFromCurrentTime(&mob_rng);
		mob_rng_inited = true;
	}

	/* Find a free entity ID */
	for (id = 0; id < MAX_NET_PLAYERS; id++) {
		if (!Entities.List[id]) break;
	}
	if (id == MAX_NET_PLAYERS) {
		Chat_AddRaw("&e/client boom: &cNo free entity slots!");
		return;
	}

	/* Init the entity */
	np = &NetPlayers_List[id];
	NetPlayer_Init(np);

	if (!mob_vtable_inited) {
		origNetPlayerVTABLE = np->Base.VTABLE;
		mobEntity_VTABLE    = *origNetPlayerVTABLE;
		mobEntity_VTABLE.Tick        = MobEntity_Tick;
		mobEntity_VTABLE.RenderModel = MobEntity_RenderModel;
		mobEntity_VTABLE.GetCol      = MobEntity_GetCol;
		mob_vtable_inited            = true;
	}
	np->Base.VTABLE = &mobEntity_VTABLE;

	Entities.List[id] = &np->Base;
	Event_RaiseInt(&EntityEvents.Added, id);

	mobType[id]        = MOB_TYPE_HOSTILE;
	mobHealth[id]      = Mob_GetHealthWithMultiplier(MOB_HP_HOSTILE);
	mobHasTarget[id]   = false;
	mobWanderPause[id] = 0.5f + Random_Float(&mob_rng) * 2.0f;
	mobDeathTimer[id]  = 0.0f;
	mobHurtFlash[id]   = 0.0f;
	mobIsAggro[id]     = false;
	mobModelIdx[id]    = MOB_IDX_CREEPER;
	mobCreeperFuse[id] = -1.0f;
	mobSkeletonShootTimer[id] = 0.0f;
	mobWalkBackwards[id] = false;
	mobTargetYaw[id] = 0.0f;
	mobSpiderLeapTimer[id] = 0.0f;
	mobFallStartY[id] = spawnPos.y;
	mobIsBrownSpider[id] = false;
	mobSunDamageTimer[id] = 0.0f;
	mobLavaDamageTimer[id] = 0.0f;
	mobCactusDamageTimer[id] = 0.0f;
	mobFireDamageTimer[id] = 0.0f;
	mobOnFireTimer[id] = 0.0f;
	mobCreeperVariant[id] = CREEPER_VAR_NUKE;
	mobSheepSheared[id] = false;

	model = String_FromReadonly("creeperc");
	Entity_SetModel(&np->Base, &model);

	name = String_FromReadonly("Creeper");
	Entity_SetName(&np->Base, &name);

	Mem_Set(&update, 0, sizeof(update));
	update.flags = LU_HAS_POS | LU_HAS_YAW | LU_HAS_PITCH;
	update.pos   = spawnPos;
	update.yaw   = (float)Random_Next(&mob_rng, 360);
	update.pitch = 0.0f;
	np->Base.VTABLE->SetLocation(&np->Base, &update);

	Chat_AddRaw("&cNuclear creeper spawned! Run.");
}

static struct ChatCommand BoomCommand = {
	"Boom", BoomCommand_Execute,
	COMMAND_FLAG_SINGLEPLAYER_ONLY,
	{
		"&a/client boom",
		"&eSpawns a special creeper at your position.",
	}
};

/*########################################################################################################################*
*---------------------------------------------------Natural Mob Spawning--------------------------------------------------*
*#########################################################################################################################*/
/* Spawn rate intervals in seconds: Off, 60, 30, 15, 5 */
static const float MobSpawnRate_Intervals[] = { 0.0f, 60.0f, 30.0f, 15.0f, 5.0f };
static float mobSpawnTimer;

/* Count currently alive mobs of given type (hostile or passive) */
static int Mob_CountType(int type) {
	int i, count = 0;
	for (i = 0; i < MAX_NET_PLAYERS; i++) {
		if (mobType[i] == type) count++;
	}
	return count;
}

/* Find a valid natural spawn position around the player */
static cc_bool Mob_IsSpawnableBlock(BlockID b) {
	return b == BLOCK_DIRT || b == BLOCK_STONE || b == BLOCK_GRASS ||
	       b == BLOCK_SAND || b == BLOCK_GRAVEL;
}

/* lightMode: 0 = no light check, 1 = hostile (must be shadow), 2 = passive (must be lit) */
static cc_bool Mob_FindNaturalSpawnPos(Vec3* outPos, int lightMode) {
	struct Entity* pe = &Entities.CurPlayer->Base;
	int px, py, pz, tx, ty, tz, attempts;
	BlockID groundBlock;

	px = (int)Math_Floor(pe->Position.x);
	py = (int)Math_Floor(pe->Position.y);
	pz = (int)Math_Floor(pe->Position.z);

	for (attempts = 0; attempts < 20; attempts++) {
		/* Random position 16-32 blocks from player */
		tx = px + (Random_Next(&mob_rng, 33) - 16);
		tz = pz + (Random_Next(&mob_rng, 33) - 16);

		/* Skip if too close (within 8 blocks) */
		{
			int distX = tx - px;
			int distZ = tz - pz;
			if (distX < 0) distX = -distX;
			if (distZ < 0) distZ = -distZ;
			if (distX < 8 && distZ < 8) continue;
		}

		ty = py;

		if (!World_Contains(tx, ty, tz)) continue;

		/* Find ground level: search down for solid ground */
		while (ty > 0 && !Mob_BlockIsSolid(tx, ty - 1, tz)) ty--;
		if (ty <= 0) continue;

		/* Check ground block is a spawnable type */
		groundBlock = World_GetBlock(tx, ty - 1, tz);
		if (!Mob_IsSpawnableBlock(groundBlock)) continue;

		/* Ensure 2 blocks of air above */
		if (!Mob_BlockIsPassable(tx, ty, tz) || !Mob_BlockIsPassable(tx, ty + 1, tz)) continue;

		/* Don't spawn underwater or in lava */
		{
			BlockID b0 = World_GetBlock(tx, ty, tz);
			BlockID b1 = World_GetBlock(tx, ty + 1, tz);
			if (b0 == BLOCK_WATER || b0 == BLOCK_STILL_WATER ||
				b1 == BLOCK_WATER || b1 == BLOCK_STILL_WATER) continue;
			if (b0 == BLOCK_LAVA || b0 == BLOCK_STILL_LAVA ||
				b1 == BLOCK_LAVA || b1 == BLOCK_STILL_LAVA) continue;
		}

		/* Light check: hostile must be in shadow, passive must be in light */
		if (lightMode == 1 && Lighting.IsLit(tx, ty, tz)) continue;
		if (lightMode == 2 && !Lighting.IsLit(tx, ty, tz)) continue;

		outPos->x = (float)tx + 0.5f;
		outPos->y = (float)ty;
		outPos->z = (float)tz + 0.5f;
		return true;
	}
	return false;
}

static void Mob_NaturalSpawnTick(struct ScheduledTask* task) {
	float interval;
	int groupSize, mobIdx, i;
	int hostileCount, passiveCount;
	int lightMode;
	Vec3 spawnPos;

	/* Pause spawning when a menu is open */
	if (Gui_GetInputGrab()) return;

	/* Spawn rate 0 = disabled */
	if (Game_MobSpawnRate == 0) return;

	if (!mob_rng_inited) {
		Random_SeedFromCurrentTime(&mob_rng);
		mob_rng_inited = true;
	}

	interval = MobSpawnRate_Intervals[Game_MobSpawnRate];
	mobSpawnTimer += (float)task->interval;
	if (mobSpawnTimer < interval) return;
	mobSpawnTimer = 0.0f;

	/* Cap mob counts */
	hostileCount = Mob_CountType(MOB_TYPE_HOSTILE);
	passiveCount = Mob_CountType(MOB_TYPE_PASSIVE);

	/* Try to spawn a group of passive mobs */
	if (Game_PassiveSpawning && passiveCount < 20) {
		/* lightMode: 0 = no restriction, 2 = must be lit */
		lightMode = Game_LightRestrictSpawning ? 2 : 0;
		groupSize = 1 + Random_Next(&mob_rng, 5); /* 1-5 */
		for (i = 0; i < groupSize && passiveCount < 20; i++) {
			/* Pick a random passive mob (pig=0 or sheep=1) */
			mobIdx = Random_Next(&mob_rng, 2);
			if (Mob_FindNaturalSpawnPos(&spawnPos, lightMode)) {
				if (SpawnMobAt(spawnPos, mobIdx)) passiveCount++;
			}
		}
	}

	/* Try to spawn a group of hostile mobs */
	if (Game_EnemySpawning && hostileCount < 20) {
		/* lightMode: 0 = no restriction, 1 = must be shadow */
		/* During night (near full darkness), hostiles can spawn on surface too */
		if (DayNightCycle_IsDark()) {
			lightMode = 0;
		} else {
			lightMode = Game_LightRestrictSpawning ? 1 : 0;
		}
		groupSize = 1 + Random_Next(&mob_rng, 5); /* 1-5 */
		for (i = 0; i < groupSize && hostileCount < 20; i++) {
			/* Pick a random hostile mob (creeper=2, spider=3, zombie=4, skeleton=5) */
			mobIdx = 2 + Random_Next(&mob_rng, 4);
			if (Mob_FindNaturalSpawnPos(&spawnPos, lightMode)) {
				if (SpawnMobAt(spawnPos, mobIdx)) hostileCount++;
			}
		}
	}
}

/* Temporary: shoot an arrow from the player in their look direction */
#define PLAYER_ARROW_KB_FACTOR 0.5f  /* half knockback compared to skeleton arrows */
static void Player_ShootArrow(void) {
	struct Entity* pe = &Entities.CurPlayer->Base;
	struct NetPlayer* np;
	struct LocationUpdate update;
	cc_string model;
	Vec3 spawnPos, dir;
	float yawRad, pitchRad;
	int id, slot;

	/* Find a free entity slot */
	for (id = 0; id < MAX_NET_PLAYERS; id++) {
		if (!Entities.List[id]) break;
	}
	if (id == MAX_NET_PLAYERS) return;

	/* Find a free arrow slot */
	for (slot = 0; slot < MAX_ARROWS; slot++) {
		if (!arrowActive[slot]) break;
	}
	if (slot == MAX_ARROWS) return;

	/* Spawn from player eye level */
	spawnPos = pe->Position;
	spawnPos.y += 1.5f;

	/* Direction from player's yaw and pitch (same formula as Vec3_GetDirVector) */
	yawRad   = pe->Yaw   * MATH_DEG2RAD;
	pitchRad = pe->Pitch  * MATH_DEG2RAD;
	dir.x = -Math_CosF(pitchRad) * -Math_SinF(yawRad);
	dir.y = -Math_SinF(pitchRad);
	dir.z = -Math_CosF(pitchRad) * Math_CosF(yawRad);

	arrowVelocity[slot].x = dir.x * ARROW_SPEED;
	arrowVelocity[slot].y = dir.y * ARROW_SPEED + 0.1f; /* slight arc */
	arrowVelocity[slot].z = dir.z * ARROW_SPEED;

	/* Init the entity */
	np = &NetPlayers_List[id];
	NetPlayer_Init(np);
	Entities.List[id] = &np->Base;
	Event_RaiseInt(&EntityEvents.Added, id);

	model = String_FromReadonly("arrow");
	Entity_SetModel(&np->Base, &model);
	np->Base.uScale = 4.0f;
	np->Base.vScale = 4.0f;
	np->Base.Velocity = arrowVelocity[slot];

	update.flags = LU_HAS_POS;
	update.pos   = spawnPos;
	np->Base.VTABLE->SetLocation(&np->Base, &update);
	np->Base.Position = spawnPos; /* SetLocation doesn't set Position directly */

	arrowActive[slot]   = true;
	arrowEntityId[slot] = id;
	arrowLifetime[slot] = 20.0f;
	arrowIsPlayerArrow[slot] = true;

	Audio_PlayDigSound(SOUND_SHOOT);
}

static void OnInputDown(void* obj, int key, cc_bool was, struct InputDevice* device) {
	struct Screen* s;
	cc_bool triggered;
	int i;
	if (Input.DownHook && Input.DownHook(key, device)) return;

#ifndef CC_BUILD_WEB
	if (key == device->escapeButton && (s = Gui_GetClosable())) {
		/* Don't want holding down escape to go in and out of pause menu */
		if (!was) {
			Gui_Remove(s);
			/* Return to death screen if player is dead */
			if (Player_Health <= 0 && Game_SurvivalMode) {
				DeathScreen_Show();
			}
		}
		return;
	}
#endif

	if (InputHandler_IsShutdown(key)) {
		Window_RequestClose(); return;
	} else if (InputBind_Claims(BIND_SCREENSHOT, key, device) && !was) {
		Game_ScreenshotRequested = true; return;
	}

	/* TAB key shoots arrow in creative mode */
	if (key == CCKEY_TAB && !was && !Gui.InputGrab && !Game_SurvivalMode) {
		Player_ShootArrow();
		return;
	}

	triggered = false;
	for (i = 0; !was && i < BIND_COUNT; i++)
	{
		if (!InputBind_Claims(i, key, device)) continue;
		Bind_IsTriggered[i] |= device->type;

		if (!Bind_OnTriggered[i])              continue;
		triggered |= Bind_OnTriggered[i](key, device);
	}
	
	for (i = 0; i < Gui.ScreensCount; i++) 
	{
		s = Gui_Screens[i];
		s->dirty = true;
		if (s->VTABLE->HandlesInputDown(s, key, device)) return;
	}
	if (Gui.InputGrab) return;

	if (InputDevice_IsPause(key, device)) {
#ifdef CC_BUILD_WEB
		/* Can't do this in KeyUp, because pressing escape without having */
		/* explicitly disabled mouse lock means a KeyUp event isn't sent. */
		/* But switching to pause screen disables mouse lock, causing a KeyUp */
		/* event to be sent, triggering the active->closable case which immediately */
		/* closes the pause screen. Hence why the next KeyUp must be supressed. */
		suppressEscape = true;
#endif
		Gui_ShowPauseMenu(); return;
	}

	/* Hotkeys should not be triggered multiple times when holding down */
	if (was) return;

	if (triggered) {
	} else if (key == CCKEY_F5 && Game_ClassicMode) {
		int weather = Env.Weather == WEATHER_SUNNY ? WEATHER_RAINY : WEATHER_SUNNY;
		Env_SetWeather(weather);
	} else { HandleHotkeyDown(key); }
}

static void OnInputDownLegacy(void* obj, int key, cc_bool was, struct InputDevice* device) {
	/* Event originated from ClassiCube, ignore it */
	if (device == &NormDevice) return;

	/* Event originated from a plugin, convert it */
	OnInputDown(obj, key, was, &NormDevice);
}

static void OnInputUp(void* obj, int key, cc_bool was, struct InputDevice* device) {
	struct Screen* s;
	int i;

#ifdef CC_BUILD_WEB
	/* When closing menus (which reacquires mouse focus) in key down, */
	/* this still leaves the cursor visible. But if this is instead */
	/* done in key up, the cursor disappears as expected. */
	if (key == CCKEY_ESCAPE && (s = Gui_GetClosable())) {
		if (suppressEscape) { suppressEscape = false; return; }
		Gui_Remove(s);
		if (Player_Health <= 0 && Game_SurvivalMode) DeathScreen_Show();
		return;
	}
#endif

	for (i = 0; i < Gui.ScreensCount; i++) 
	{
		s = Gui_Screens[i];
		s->dirty = true;
		s->VTABLE->OnInputUp(s, key, device);
	}

	for (i = 0; i < BIND_COUNT; i++)
	{
		if (!InputBind_Claims(i, key, device)) continue;
		Bind_IsTriggered[i] &= ~device->type;

		if (!Bind_OnReleased[i])               continue;
		Bind_OnReleased[i](key, device);
	}
}

static int moveFlags[MAX_LOCAL_PLAYERS];

static cc_bool Player_TriggerLeft(int key,  struct InputDevice* device) {
	moveFlags[device->mappedIndex] |= FACE_BIT_XMIN;
	return Gui.InputGrab == NULL;
}
static cc_bool Player_TriggerRight(int key, struct InputDevice* device) {
	moveFlags[device->mappedIndex] |= FACE_BIT_XMAX;
	return Gui.InputGrab == NULL;
}
static cc_bool Player_TriggerUp(int key,    struct InputDevice* device) {
	moveFlags[device->mappedIndex] |= FACE_BIT_YMIN;
	return Gui.InputGrab == NULL;
}
static cc_bool Player_TriggerDown(int key,  struct InputDevice* device) {
	moveFlags[device->mappedIndex] |= FACE_BIT_YMAX;
	return Gui.InputGrab == NULL;
}

static void Player_ReleaseLeft(int key,  struct InputDevice* device) {
	moveFlags[device->mappedIndex] &= ~FACE_BIT_XMIN;
}
static void Player_ReleaseRight(int key, struct InputDevice* device) {
	moveFlags[device->mappedIndex] &= ~FACE_BIT_XMAX;
}
static void Player_ReleaseUp(int key,    struct InputDevice* device) {
	moveFlags[device->mappedIndex] &= ~FACE_BIT_YMIN;
}
static void Player_ReleaseDown(int key,  struct InputDevice* device) {
	moveFlags[device->mappedIndex] &= ~FACE_BIT_YMAX;
}

static void PlayerInputNormal(struct LocalPlayer* p, float* xMoving, float* zMoving) {
	int flags = moveFlags[p->index];

	if (flags & FACE_BIT_YMIN) *zMoving -= 1;
	if (flags & FACE_BIT_YMAX) *zMoving += 1;
	if (flags & FACE_BIT_XMIN) *xMoving -= 1;
	if (flags & FACE_BIT_XMAX) *xMoving += 1;
}
static struct LocalPlayerInput normalInput = { PlayerInputNormal };

/*########################################################################################################################*
*------------------------------------------------Block breaking crack overlay-------------------------------------------*
*#########################################################################################################################*/
#define CRACK_NUM_VERTICES (6 * 4)

static void BlockBreaking_BuildCrackMesh(void) {
	struct VertexTextured* v;
	TextureRec rec;
	TextureLoc texLoc;
	int atlasIdx;
	float offset, x1, y1, z1, x2, y2, z2;
	PackedCol col;

	texLoc = 240 + breaking_crackStage;
	rec    = Atlas1D_TexRec(texLoc, 1, &atlasIdx);
	offset = 0.002f;
	col    = PackedCol_Make(255, 255, 255, 220);

	/* Use block bounding box for non-full blocks (slabs, etc.) */
	x1 = (float)breaking_pos.x + Blocks.MinBB[breaking_block].x - offset;
	y1 = (float)breaking_pos.y + Blocks.MinBB[breaking_block].y - offset;
	z1 = (float)breaking_pos.z + Blocks.MinBB[breaking_block].z - offset;
	x2 = (float)breaking_pos.x + Blocks.MaxBB[breaking_block].x + offset;
	y2 = (float)breaking_pos.y + Blocks.MaxBB[breaking_block].y + offset;
	z2 = (float)breaking_pos.z + Blocks.MaxBB[breaking_block].z + offset;

	v = (struct VertexTextured*)Gfx_LockDynamicVb(crack_vb,
				VERTEX_FORMAT_TEXTURED, CRACK_NUM_VERTICES);

	/* FACE_YMIN (bottom) */
	v->x = x1; v->y = y1; v->z = z2; v->Col = col; v->U = rec.u1; v->V = rec.v2; v++;
	v->x = x1; v->y = y1; v->z = z1; v->Col = col; v->U = rec.u1; v->V = rec.v1; v++;
	v->x = x2; v->y = y1; v->z = z1; v->Col = col; v->U = rec.u2; v->V = rec.v1; v++;
	v->x = x2; v->y = y1; v->z = z2; v->Col = col; v->U = rec.u2; v->V = rec.v2; v++;

	/* FACE_YMAX (top) */
	v->x = x1; v->y = y2; v->z = z1; v->Col = col; v->U = rec.u1; v->V = rec.v1; v++;
	v->x = x1; v->y = y2; v->z = z2; v->Col = col; v->U = rec.u1; v->V = rec.v2; v++;
	v->x = x2; v->y = y2; v->z = z2; v->Col = col; v->U = rec.u2; v->V = rec.v2; v++;
	v->x = x2; v->y = y2; v->z = z1; v->Col = col; v->U = rec.u2; v->V = rec.v1; v++;

	/* FACE_ZMIN (north) */
	v->x = x1; v->y = y2; v->z = z1; v->Col = col; v->U = rec.u2; v->V = rec.v1; v++;
	v->x = x2; v->y = y2; v->z = z1; v->Col = col; v->U = rec.u1; v->V = rec.v1; v++;
	v->x = x2; v->y = y1; v->z = z1; v->Col = col; v->U = rec.u1; v->V = rec.v2; v++;
	v->x = x1; v->y = y1; v->z = z1; v->Col = col; v->U = rec.u2; v->V = rec.v2; v++;

	/* FACE_ZMAX (south) */
	v->x = x2; v->y = y2; v->z = z2; v->Col = col; v->U = rec.u2; v->V = rec.v1; v++;
	v->x = x1; v->y = y2; v->z = z2; v->Col = col; v->U = rec.u1; v->V = rec.v1; v++;
	v->x = x1; v->y = y1; v->z = z2; v->Col = col; v->U = rec.u1; v->V = rec.v2; v++;
	v->x = x2; v->y = y1; v->z = z2; v->Col = col; v->U = rec.u2; v->V = rec.v2; v++;

	/* FACE_XMIN (west) */
	v->x = x1; v->y = y2; v->z = z2; v->Col = col; v->U = rec.u2; v->V = rec.v1; v++;
	v->x = x1; v->y = y2; v->z = z1; v->Col = col; v->U = rec.u1; v->V = rec.v1; v++;
	v->x = x1; v->y = y1; v->z = z1; v->Col = col; v->U = rec.u1; v->V = rec.v2; v++;
	v->x = x1; v->y = y1; v->z = z2; v->Col = col; v->U = rec.u2; v->V = rec.v2; v++;

	/* FACE_XMAX (east) */
	v->x = x2; v->y = y2; v->z = z1; v->Col = col; v->U = rec.u2; v->V = rec.v1; v++;
	v->x = x2; v->y = y2; v->z = z2; v->Col = col; v->U = rec.u1; v->V = rec.v1; v++;
	v->x = x2; v->y = y1; v->z = z2; v->Col = col; v->U = rec.u1; v->V = rec.v2; v++;
	v->x = x2; v->y = y1; v->z = z1; v->Col = col; v->U = rec.u2; v->V = rec.v2; v++;

	Gfx_UnlockDynamicVb(crack_vb);
	Gfx_BindTexture(Atlas1D.TexIds[atlasIdx]);
}

void BlockBreaking_RenderCrack(void) {
	if (!breaking_active || breaking_crackStage < 0) return;
	if (Gfx.LostContext) return;

	if (!crack_vb)
		crack_vb = Gfx_CreateDynamicVb(VERTEX_FORMAT_TEXTURED, CRACK_NUM_VERTICES);

	Gfx_SetAlphaBlending(true);
	Gfx_SetDepthWrite(false);
	Gfx_SetVertexFormat(VERTEX_FORMAT_TEXTURED);

	BlockBreaking_BuildCrackMesh();
	Gfx_DrawVb_IndexedTris(CRACK_NUM_VERTICES);

	Gfx_SetDepthWrite(true);
	Gfx_SetAlphaBlending(false);
}


static void BlockBreaking_OnHeldChanged(void* obj) {
	float newTime;
	if (!breaking_active) return;
	newTime = CalcBreakTime(breaking_block);
	if (newTime < 0.0f) {
		BlockBreaking_Reset();
	} else if (newTime > 0.001f) {
		/* Scale progress to new total time (keep time already spent) */
		float spent = breaking_progress * breaking_totalTime;
		breaking_totalTime = newTime;
		breaking_progress  = spent / newTime;
		if (breaking_progress > 1.0f) breaking_progress = 1.0f;
	}
}

static void BlockBreaking_ContextLost(void* obj) {
	Gfx_DeleteDynamicVb(&crack_vb);
}

static void Furnace_ScheduledTick(struct ScheduledTask* task) {
	Furnace_TickAll(task->interval);
}

/*########################################################################################################################*
*---------------------------------------------------Day/Night Cycle-------------------------------------------------------*
*#########################################################################################################################*/
/* Cycle phase timing (in seconds) */
#define DN_DAY_END     600.0f   /* 10 minutes of daylight */
#define DN_SUNSET_END  650.0f   /* + 50 seconds sunset transition */
#define DN_NIGHT_END  1130.0f   /* + 8 minutes of night */
#define DN_CYCLE_END  1180.0f   /* + 50 seconds sunrise transition */
#define DN_TRANSITION_DURATION 50.0f

/* Night sky/fog/cloud target colors */
#define DN_NIGHT_SKY_COL    PackedCol_Make(5,   5,   8,   255)
#define DN_NIGHT_FOG_COL    PackedCol_Make(8,   8,   12,  255)
#define DN_NIGHT_CLOUDS_COL PackedCol_Make(15,  15,  20,  255)

static cc_bool  dn_active;
static float    dn_timer;
static PackedCol dn_origSunCol, dn_origSkyCol, dn_origFogCol, dn_origCloudsCol, dn_origShadowCol;
static PackedCol dn_lastSunCol;      /* avoid redundant Env_SetSunCol calls */
static PackedCol dn_lastShadowCol;   /* avoid redundant Env_SetShadowCol calls */

static PackedCol DayNight_LerpColor(PackedCol a, PackedCol b, float t) {
	int r = (int)(PackedCol_R(a) + (PackedCol_R(b) - PackedCol_R(a)) * t);
	int g = (int)(PackedCol_G(a) + (PackedCol_G(b) - PackedCol_G(a)) * t);
	int bl = (int)(PackedCol_B(a) + (PackedCol_B(b) - PackedCol_B(a)) * t);
	if (r < 0) r = 0; if (r > 255) r = 255;
	if (g < 0) g = 0; if (g > 255) g = 255;
	if (bl < 0) bl = 0; if (bl > 255) bl = 255;
	return PackedCol_Make(r, g, bl, 255);
}

static cc_bool DayNightCycle_IsNight(void) {
	if (Gen_ActiveTimeMode == GEN_TIME_NIGHT) return dn_active;
	return dn_active && dn_timer >= DN_DAY_END && dn_timer < DN_CYCLE_END;
}

/* Stricter check: only true when nearly or fully dark (for hostile spawning) */
static cc_bool DayNightCycle_IsDark(void) {
	/* Always-night worlds are always dark */
	float sunsetThreshold, sunriseStopAt;
	if (Gen_ActiveTimeMode == GEN_TIME_NIGHT) return dn_active;
	/* Start spawning when sunset is 90% complete, stop when sunrise is 90% complete */
	sunsetThreshold = DN_DAY_END + DN_TRANSITION_DURATION * 0.9f;
	sunriseStopAt   = DN_NIGHT_END + DN_TRANSITION_DURATION * 0.1f;
	return dn_active && dn_timer >= sunsetThreshold && dn_timer < sunriseStopAt;
}

static void DayNight_ApplyColors(float t) {
	/* t: 0.0 = full day, 1.0 = full night */
	const struct GenThemeData* theme = Gen_GetTheme();
	/* Use per-theme night colors if set, otherwise use defaults */
	PackedCol nightSkyTarget    = theme->nightSkyCol ? theme->nightSkyCol : DN_NIGHT_SKY_COL;
	PackedCol nightFogTarget    = theme->nightFogCol ? theme->nightFogCol : DN_NIGHT_FOG_COL;
	/* Night sun target: slightly darker than shadow color (~80% of original shadow brightness) */
	PackedCol nightSunTarget = PackedCol_Make(
		PackedCol_R(dn_origShadowCol) * 4 / 5,
		PackedCol_G(dn_origShadowCol) * 4 / 5,
		PackedCol_B(dn_origShadowCol) * 4 / 5, 255);
	/* Night shadow target: same as sun at night */
	PackedCol nightShadowTarget = nightSunTarget;
	PackedCol sunCol    = DayNight_LerpColor(dn_origSunCol,    nightSunTarget,      t);
	PackedCol shadowCol = DayNight_LerpColor(dn_origShadowCol, nightShadowTarget,   t);
	PackedCol skyCol    = DayNight_LerpColor(dn_origSkyCol,    nightSkyTarget,      t);
	PackedCol fogCol    = DayNight_LerpColor(dn_origFogCol,    nightFogTarget,      t);
	PackedCol cloudsCol = DayNight_LerpColor(dn_origCloudsCol, DN_NIGHT_CLOUDS_COL, t);

	/* Only update sun/shadow colors if changed (triggers expensive chunk rebuild) */
	if (sunCol != dn_lastSunCol) {
		dn_lastSunCol = sunCol;
		Env_SetSunCol(sunCol);
	}
	if (shadowCol != dn_lastShadowCol) {
		dn_lastShadowCol = shadowCol;
		Env_SetShadowCol(shadowCol);
	}
	/* Sky/fog/clouds are cheap to update */
	Env_SetSkyCol(skyCol);
	Env_SetFogCol(fogCol);
	Env_SetCloudsCol(cloudsCol);
}

static void DayNightCycle_Tick(struct ScheduledTask* task) {
	float t;
	if (!dn_active) return;

	/* For always-night worlds, stay frozen at full night */
	if (Gen_ActiveTimeMode == GEN_TIME_NIGHT) {
		DayNight_ApplyColors(1.0f);
		return;
	}

	dn_timer += (float)task->interval;

	while (dn_timer >= DN_CYCLE_END) {
		dn_timer -= DN_CYCLE_END; /* wrap around */
	}

	if (dn_timer < DN_DAY_END) {
		/* Daytime: full original colors */
		DayNight_ApplyColors(0.0f);
	} else if (dn_timer < DN_SUNSET_END) {
		/* Sunset transition */
		t = (dn_timer - DN_DAY_END) / DN_TRANSITION_DURATION;
		DayNight_ApplyColors(t);
	} else if (dn_timer < DN_NIGHT_END) {
		/* Night: full dark */
		DayNight_ApplyColors(1.0f);
	} else {
		/* Sunrise transition */
		t = 1.0f - (dn_timer - DN_NIGHT_END) / DN_TRANSITION_DURATION;
		DayNight_ApplyColors(t);
	}
}

void DayNightCycle_Enable(void) {
	if (!World.Loaded) return;

	/* Capture current environment colors as "daytime" originals */
	dn_origSunCol    = Env.SunCol;
	dn_origShadowCol = Env.ShadowCol;
	dn_origSkyCol    = Env.SkyCol;
	dn_origFogCol    = Env.FogCol;
	dn_origCloudsCol = Env.CloudsCol;
	dn_lastSunCol    = Env.SunCol;
	dn_lastShadowCol = Env.ShadowCol;
	dn_timer         = 0.0f;
	dn_active        = true;
}

void DayNightCycle_SetTimer(float time) {
	dn_timer = time;
	/* Immediately apply the colors for the new time */
	if (!dn_active) return;
	if (dn_timer < DN_DAY_END) {
		DayNight_ApplyColors(0.0f);
	} else if (dn_timer < DN_SUNSET_END) {
		DayNight_ApplyColors((dn_timer - DN_DAY_END) / DN_TRANSITION_DURATION);
	} else if (dn_timer < DN_NIGHT_END) {
		DayNight_ApplyColors(1.0f);
	} else {
		DayNight_ApplyColors(1.0f - (dn_timer - DN_NIGHT_END) / DN_TRANSITION_DURATION);
	}
}

void DayNightCycle_RestoreOriginalColors(void) {
	if (!dn_active) return;
	/* Temporarily set env to original daytime colors (for saving .cw) */
	Env.SunCol    = dn_origSunCol;
	Env.ShadowCol = dn_origShadowCol;
	Env.SkyCol    = dn_origSkyCol;
	Env.FogCol    = dn_origFogCol;
	Env.CloudsCol = dn_origCloudsCol;
}

void DayNightCycle_ReapplyCurrentColors(void) {
	if (!dn_active) return;
	/* Re-apply colors for the current timer position */
	dn_lastSunCol    = 0;
	dn_lastShadowCol = 0;
	if (dn_timer < DN_DAY_END) {
		DayNight_ApplyColors(0.0f);
	} else if (dn_timer < DN_SUNSET_END) {
		DayNight_ApplyColors((dn_timer - DN_DAY_END) / DN_TRANSITION_DURATION);
	} else if (dn_timer < DN_NIGHT_END) {
		DayNight_ApplyColors(1.0f);
	} else {
		DayNight_ApplyColors(1.0f - (dn_timer - DN_NIGHT_END) / DN_TRANSITION_DURATION);
	}
}

void DayNightCycle_Disable(void) {
	if (dn_active && World.Loaded) {
		/* Restore original daytime colors */
		Env_SetSunCol(dn_origSunCol);
		Env_SetShadowCol(dn_origShadowCol);
		Env_SetSkyCol(dn_origSkyCol);
		Env_SetFogCol(dn_origFogCol);
		Env_SetCloudsCol(dn_origCloudsCol);
	}
	dn_active = false;
}

/*########################################################################################################################*
*-----------------------------------------------Ambient fire sound tick---------------------------------------------------*
*#########################################################################################################################*/
/* Periodically check for nearby fire blocks and play fire.wav with distance-based volume */
static void FireAmbient_Tick(struct ScheduledTask* task) {
	struct Entity* pe = &Entities.CurPlayer->Base;
	float px, py, pz;
	int ix, iy, iz, radius, bestDistSq;
	int dx, dy, dz, distSq, vol, rate;

	if (!World.Loaded) return;
	px = pe->Position.x; py = pe->Position.y; pz = pe->Position.z;

	/* Search within 16 blocks for the nearest fire */
	radius = 16;
	bestDistSq = radius * radius + 1;

	for (iy = (int)py - radius; iy <= (int)py + radius; iy++) {
		if (iy < 0 || iy > World.MaxY) continue;
		for (iz = (int)pz - radius; iz <= (int)pz + radius; iz++) {
			if (iz < 0 || iz > World.MaxZ) continue;
			for (ix = (int)px - radius; ix <= (int)px + radius; ix++) {
				if (ix < 0 || ix > World.MaxX) continue;
				if (World_GetBlock(ix, iy, iz) != BLOCK_FIRE) continue;

				dx = ix - (int)px; dy = iy - (int)py; dz = iz - (int)pz;
				distSq = dx * dx + dy * dy + dz * dz;
				if (distSq < bestDistSq) bestDistSq = distSq;
			}
		}
	}

	if (bestDistSq > radius * radius) return; /* no fire nearby */

	/* Volume falls off with distance */
	vol = (int)(Audio_SoundsVolume * 1.5f * (1.0f - Math_SqrtF((float)bestDistSq) / (float)radius));
	if (vol > Audio_SoundsVolume) vol = Audio_SoundsVolume;
	if (vol <= 0) return;

	rate = 80 + Random_Next(&mob_rng, 41); /* 80-120 */
	Audio_PlayDigSoundRateVolume(SOUND_FIRE_AMBIENT, rate, vol);
}

static void OnInit(void) {
	LocalPlayerInput_Add(&normalInput);
	LocalPlayerInput_Add(&gamepadInput);
	HookInputBinds();

	ScheduledTask_Add(1.0 / 20, Arrow_ScheduledTick);
	ScheduledTask_Add(1.0 / 20.0, DroppedItem_TickAll);
	ScheduledTask_Add(1.0 / 20.0, PlayerDamage_ScheduledTick);
	ScheduledTask_Add(1.0, Mob_NaturalSpawnTick);
	ScheduledTask_Add(1.0 / 20.0, Furnace_ScheduledTick);
	ScheduledTask_Add(2.0, DayNightCycle_Tick);
	ScheduledTask_Add(1.0, FireAmbient_Tick);

	Commands_Register(&BoomCommand);

	Event_Register_(&UserEvents.BlockChanged, NULL, Arrow_OnBlockChanged);
	Event_Register_(&PointerEvents.Down,  NULL, OnPointerDown);
	Event_Register_(&PointerEvents.Up,    NULL, OnPointerUp);
	Event_Register_(&InputEvents._down,   NULL, OnInputDownLegacy);
	Event_Register_(&InputEvents.Down2,   NULL, OnInputDown);
	Event_Register_(&InputEvents.Up2,     NULL, OnInputUp);

	Event_Register_(&UserEvents.HackPermsChanged, NULL, InputHandler_CheckZoomFov);
	Event_Register_(&UserEvents.HeldBlockChanged, NULL, BlockBreaking_OnHeldChanged);
	Event_Register_(&GfxEvents.ContextLost, NULL, BlockBreaking_ContextLost);
	StoredHotkeys_LoadAll();

	Bind_OnTriggered[BIND_FORWARD] = Player_TriggerUp;
	Bind_OnTriggered[BIND_BACK]    = Player_TriggerDown;
	Bind_OnTriggered[BIND_LEFT]    = Player_TriggerLeft;
	Bind_OnTriggered[BIND_RIGHT]   = Player_TriggerRight;

	Bind_OnReleased[BIND_FORWARD] = Player_ReleaseUp;
	Bind_OnReleased[BIND_BACK]    = Player_ReleaseDown;
	Bind_OnReleased[BIND_LEFT]    = Player_ReleaseLeft;
	Bind_OnReleased[BIND_RIGHT]   = Player_ReleaseRight;
}

/*########################################################################################################################*
*-------------------------------------------------World settings save/load----------------------------------------------*
*#########################################################################################################################*/
/* Extensible key-value format for per-world settings.
 * Version 1 layout:
 *   [0]     version (1 byte)
 *   [1-3]   reserved (3 bytes)
 *   [4-7]   Player_Health (4 bytes, int)
 *   [8-11]  dn_timer (4 bytes, float)
 *   [12]    Game_SurvivalMode (1 byte, bool)
 *   [13]    Game_DaylightCycle (1 byte, bool)
 *   [14]    Player_CheatsEnabled (1 byte, bool)
 *   [15]    Gen_ActiveTimeMode (1 byte, 0=Cycle 1=Day 2=Night)
 */
#define WORLDSETTINGS_VERSION 1
#define WORLDSETTINGS_SIZE    16

void WorldSettings_SaveToFile(const cc_string* path) {
	cc_uint8 buf[WORLDSETTINGS_SIZE];
	struct Stream stream;
	cc_filepath raw;
	cc_result res;

	Platform_EncodePath(&raw, path);
	res = Stream_CreatePath(&stream, &raw);
	if (res) { Logger_IOWarn2(res, "saving world settings", &raw); return; }

	Mem_Set(buf, 0, WORLDSETTINGS_SIZE);
	buf[0] = WORLDSETTINGS_VERSION;
	Mem_Copy(buf + 4,  &Player_Health, 4);
	Mem_Copy(buf + 8,  &dn_timer, 4);
	buf[12] = Game_SurvivalMode ? 1 : 0;
	buf[13] = Game_DaylightCycle ? 1 : 0;
	buf[14] = Player_CheatsEnabled ? 1 : 0;
	buf[15] = Gen_ActiveTimeMode;

	Stream_Write(&stream, buf, WORLDSETTINGS_SIZE);
	stream.Close(&stream);
}

void WorldSettings_LoadFromFile(const cc_string* path) {
	cc_uint8 buf[WORLDSETTINGS_SIZE];
	struct Stream stream;
	cc_filepath raw;
	cc_result res;
	float savedTimer;

	Platform_EncodePath(&raw, path);
	res = Stream_OpenPath(&stream, &raw);
	if (res) return; /* File doesn't exist = new world, use defaults */

	Stream_Read(&stream, buf, WORLDSETTINGS_SIZE);
	stream.Close(&stream);

	if (buf[0] != WORLDSETTINGS_VERSION) return;

	/* Restore player health */
	Mem_Copy(&Player_Health, buf + 4, 4);
	if (Player_Health < 0) Player_Health = 0;
	if (Player_Health > PLAYER_MAX_HEALTH) Player_Health = PLAYER_MAX_HEALTH;

	/* Restore day/night timer */
	Mem_Copy(&savedTimer, buf + 8, 4);
	if (dn_active && savedTimer >= 0.0f) {
		DayNightCycle_SetTimer(savedTimer);
	}

	/* Restore game mode flags */
	Game_SurvivalMode = buf[12];
	Game_DaylightCycle = buf[13];
	Player_CheatsEnabled = buf[14];

	/* Restore time mode and reconfigure day/night cycle */
	Gen_ActiveTimeMode = buf[15];
	if (Gen_ActiveTimeMode == GEN_TIME_NIGHT) {
		/* Always-night world: ensure cycle is active and frozen at night */
		if (!dn_active) DayNightCycle_Enable();
		DayNight_ApplyColors(1.0f);
	} else if (Gen_ActiveTimeMode == GEN_TIME_CYCLE && Game_DaylightCycle) {
		/* Normal cycle: should already be active from OnNewMapLoaded */
		if (!dn_active) DayNightCycle_Enable();
		if (savedTimer >= 0.0f) DayNightCycle_SetTimer(savedTimer);
	} else {
		/* Day mode or cycle disabled: turn off any active cycle */
		DayNightCycle_Disable();
	}

	/* Re-apply survival mode hacks restriction */
	if (Game_SurvivalMode && !Player_CheatsEnabled) {
		Entities.CurPlayer->Hacks.Enabled = false;
		HacksComp_Update(&Entities.CurPlayer->Hacks);
	}
}

/*########################################################################################################################*
*-------------------------------------------------Entity save/load-------------------------------------------------*
*#########################################################################################################################*/
#define ENTITY_SAVE_VERSION 1

void Entities_SaveToFile(const cc_string* path) {
	cc_uint8 buf[64];
	struct Stream stream;
	cc_filepath raw;
	cc_result res;
	struct Entity* e;
	int i, numMobs = 0, numDrops = 0;

	Platform_EncodePath(&raw, path);
	res = Stream_CreatePath(&stream, &raw);
	if (res) { Logger_IOWarn2(res, "saving entities", &raw); return; }

	/* Header */
	buf[0] = ENTITY_SAVE_VERSION;
	buf[1] = buf[2] = buf[3] = 0;
	Stream_Write(&stream, buf, 4);

	/* Count active mobs */
	for (i = 0; i < MAX_NET_PLAYERS; i++) {
		if (mobType[i] != MOB_TYPE_NONE && Entities.List[i]) numMobs++;
	}
	Stream_Write(&stream, (cc_uint8*)&numMobs, 4);

	/* Write each active mob */
	for (i = 0; i < MAX_NET_PLAYERS; i++) {
		if (mobType[i] == MOB_TYPE_NONE) continue;
		e = Entities.List[i];
		if (!e) continue;

		/* entityId(1), mobType(1), modelIdx(1), creeperVar(1),
		   posXYZ(12), yaw(4), health(4), isAggro(1) = 25 bytes */
		buf[0] = (cc_uint8)i;
		buf[1] = mobType[i];
		buf[2] = mobModelIdx[i];
		buf[3] = mobCreeperVariant[i];
		Mem_Copy(buf + 4,  &e->Position.x, 4);
		Mem_Copy(buf + 8,  &e->Position.y, 4);
		Mem_Copy(buf + 12, &e->Position.z, 4);
		Mem_Copy(buf + 16, &mobFacingYaw[i], 4);
		Mem_Copy(buf + 20, &mobHealth[i], 4);
		buf[24] = mobIsAggro[i] ? 1 : 0;
		Stream_Write(&stream, buf, 25);
	}

	/* Count active drops */
	for (i = 0; i < MAX_DROPPED_ITEMS; i++) {
		if (droppedItemActive[i] && Entities.List[droppedItemEntityId[i]]) numDrops++;
	}
	Stream_Write(&stream, (cc_uint8*)&numDrops, 4);

	/* Write each active drop */
	for (i = 0; i < MAX_DROPPED_ITEMS; i++) {
		struct Entity* de;
		if (!droppedItemActive[i]) continue;
		de = Entities.List[droppedItemEntityId[i]];
		if (!de) continue;

		/* posXYZ(12), block(2), isItem(1), itemId(4), count(4), lifetime(4), velXYZ(12) = 39 bytes */
		Mem_Copy(buf + 0,  &de->Position.x, 4);
		Mem_Copy(buf + 4,  &de->Position.y, 4);
		Mem_Copy(buf + 8,  &de->Position.z, 4);
		Mem_Copy(buf + 12, &droppedItemBlock[i], 2);
		buf[14] = droppedItemIsItem[i] ? 1 : 0;
		Mem_Copy(buf + 15, &droppedItemItemId[i], 4);
		Mem_Copy(buf + 19, &droppedItemCount[i], 4);
		Mem_Copy(buf + 23, &droppedItemLifetime[i], 4);
		Mem_Copy(buf + 27, &droppedItemVelocityX[i], 4);
		Mem_Copy(buf + 31, &droppedItemVelocityY[i], 4);
		Mem_Copy(buf + 35, &droppedItemVelocityZ[i], 4);
		Stream_Write(&stream, buf, 39);
	}

	stream.Close(&stream);
}

void Entities_LoadFromFile(const cc_string* path) {
	cc_uint8 buf[64];
	struct Stream stream;
	cc_filepath raw;
	cc_result res;
	int i, numMobs, numDrops;
	int entityId, slot;
	Vec3 pos;
	BlockID block;
	float lifetime;

	Platform_EncodePath(&raw, path);
	res = Stream_OpenPath(&stream, &raw);
	if (res) return; /* File doesn't exist = no saved entities */

	/* Header */
	Stream_Read(&stream, buf, 4);
	if (buf[0] != ENTITY_SAVE_VERSION) { stream.Close(&stream); return; }

	/* Clear existing mob state */
	for (i = 0; i < MAX_NET_PLAYERS; i++) {
		if (mobType[i] != MOB_TYPE_NONE && Entities.List[i]) {
			Entities_Remove((EntityID)i);
		}
		mobType[i] = MOB_TYPE_NONE;
	}

	/* Clear existing drops */
	for (i = 0; i < MAX_DROPPED_ITEMS; i++) {
		if (droppedItemActive[i]) {
			Entities_Remove(droppedItemEntityId[i]);
			droppedItemActive[i] = false;
		}
	}

	/* Clear existing arrows */
	for (i = 0; i < MAX_ARROWS; i++) {
		if (arrowActive[i]) {
			Entities_Remove(arrowEntityId[i]);
			arrowActive[i] = false;
		}
	}

	/* Read mobs */
	Stream_Read(&stream, (cc_uint8*)&numMobs, 4);
	for (i = 0; i < numMobs; i++) {
		Stream_Read(&stream, buf, 25);
		entityId = buf[0];

		Mem_Copy(&pos.x, buf + 4,  4);
		Mem_Copy(&pos.y, buf + 8,  4);
		Mem_Copy(&pos.z, buf + 12, 4);

		/* Spawn mob at saved position with saved model index */
		Mob_SpawnAt(entityId, buf[2], pos);

		/* Override with saved state */
		mobType[entityId] = buf[1];
		mobCreeperVariant[entityId] = buf[3];
		Mem_Copy(&mobFacingYaw[entityId], buf + 16, 4);
		Mem_Copy(&mobHealth[entityId], buf + 20, 4);
		mobIsAggro[entityId] = buf[24];

		/* Restore creeper/spider variant model if needed */
		if (buf[2] == MOB_IDX_CREEPER && Entities.List[entityId]) {
			cc_string model;
			if (buf[3] == CREEPER_VAR_SURVTEST) {
				model = String_FromReadonly("creepera");
				Entity_SetModel(Entities.List[entityId], &model);
			} else if (buf[3] == CREEPER_VAR_MELEE) {
				model = String_FromReadonly("creeperb");
				Entity_SetModel(Entities.List[entityId], &model);
			}
		}
	}

	/* Read drops */
	Stream_Read(&stream, (cc_uint8*)&numDrops, 4);
	for (i = 0; i < numDrops; i++) {
		int itemId, count;
		cc_bool isItem;

		Stream_Read(&stream, buf, 39);
		Mem_Copy(&pos.x, buf + 0,  4);
		Mem_Copy(&pos.y, buf + 4,  4);
		Mem_Copy(&pos.z, buf + 8,  4);
		Mem_Copy(&block, buf + 12, 2);
		isItem = buf[14];
		Mem_Copy(&itemId, buf + 15, 4);
		Mem_Copy(&count,  buf + 19, 4);
		Mem_Copy(&lifetime, buf + 23, 4);

		slot = DropItem_FindFreeSlot();
		if (slot == -1) slot = DropItem_EvictOldest();
		if (slot == -1) continue;

		DropItem_Spawn(slot, pos, block, isItem, itemId);
		droppedItemCount[slot] = count;
		droppedItemLifetime[slot] = lifetime;
		droppedItemPickupDelay[slot] = 0.0f;

		/* Restore velocity */
		Mem_Copy(&droppedItemVelocityX[slot], buf + 27, 4);
		Mem_Copy(&droppedItemVelocityY[slot], buf + 31, 4);
		Mem_Copy(&droppedItemVelocityZ[slot], buf + 35, 4);
	}

	stream.Close(&stream);
}


static void OnFree(void) {
	HotkeysText.count = 0;
	Gfx_DeleteDynamicVb(&crack_vb);
}

static void OnNewMap(void) {
	Mob_RemoveAllMobs();
	mobSpawnTimer = 0.0f;
	BlockBreaking_Reset();
	dn_active = false;
	Gen_ActiveTimeMode = GEN_TIME_CYCLE;
	/* Reset player damage state */
	playerLavaDamageTimer   = 0.0f;
	playerCactusDamageTimer = 0.0f;
	playerFireDamageTimer   = 0.0f;
	playerOnFireTimer       = 0.0f;
	playerFallStartY        = 0.0f;
	playerWasOnGround       = true;
	playerInvulnTimer       = 0.0f;
}

static void OnNewMapLoaded(void) {
	/* Auto-start day/night cycle based on time mode.
	   For generated maps, GenTheme_ApplyEnvironment will re-call DayNightCycle_Enable
	   after setting correct theme colors, so capturing defaults here is OK. */
	if (Gen_ActiveTimeMode == GEN_TIME_NIGHT) {
		DayNightCycle_Enable();
	} else if (Gen_ActiveTimeMode == GEN_TIME_CYCLE && Game_DaylightCycle) {
		DayNightCycle_Enable();
	}
}

struct IGameComponent InputHandler_Component = {
	OnInit,          /* Init  */
	OnFree,          /* Free  */
	NULL,            /* Reset */
	OnNewMap,        /* OnNewMap */
	OnNewMapLoaded   /* OnNewMapLoaded */
};
