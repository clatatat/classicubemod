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

static cc_bool input_buttonsDown[3];
static int input_pickingId = -1;
static float input_deltaAcc;
static float input_fovIndex = -1.0f;
#ifdef CC_BUILD_WEB
static cc_bool suppressEscape;
#endif
enum MouseButton_ { MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE };


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

static void InputHandler_DeleteBlock(void) {
	IVec3 pos, otherPos;
	BlockID old, otherBlock;
	/* always play delete animations, even if we aren't deleting a block */
	HeldBlockRenderer_ClickAnim(true);

	pos = Game_SelectedPos.pos;
	if (!Game_SelectedPos.valid || !World_Contains(pos.x, pos.y, pos.z)) return;

	old = World_GetBlock(pos.x, pos.y, pos.z);
	if (Blocks.Draw[old] == DRAW_GAS || !Blocks.CanDelete[old]) return;

	Game_ChangeBlock(pos.x, pos.y, pos.z, BLOCK_AIR);
	Event_RaiseBlock(&UserEvents.BlockChanged, pos, old, BLOCK_AIR);
	
	/* If breaking a door, also break the other half */
	if (old == BLOCK_DOOR_NS_BOTTOM || old == BLOCK_DOOR_EW_BOTTOM) {
		/* Breaking bottom, remove top */
		otherPos.x = pos.x;
		otherPos.y = pos.y + 1;
		otherPos.z = pos.z;
		
		if (World_Contains(otherPos.x, otherPos.y, otherPos.z)) {
			otherBlock = World_GetBlock(otherPos.x, otherPos.y, otherPos.z);
			if (otherBlock == BLOCK_DOOR_NS_TOP || otherBlock == BLOCK_DOOR_EW_TOP) {
				Game_ChangeBlock(otherPos.x, otherPos.y, otherPos.z, BLOCK_AIR);
				Event_RaiseBlock(&UserEvents.BlockChanged, otherPos, otherBlock, BLOCK_AIR);
			}
		}
	} else if (old == BLOCK_DOOR_NS_TOP || old == BLOCK_DOOR_EW_TOP) {
		/* Breaking top, remove bottom */
		otherPos.x = pos.x;
		otherPos.y = pos.y - 1;
		otherPos.z = pos.z;
		
		if (World_Contains(otherPos.x, otherPos.y, otherPos.z)) {
			otherBlock = World_GetBlock(otherPos.x, otherPos.y, otherPos.z);
			if (otherBlock == BLOCK_DOOR_NS_BOTTOM || otherBlock == BLOCK_DOOR_EW_BOTTOM) {
				Game_ChangeBlock(otherPos.x, otherPos.y, otherPos.z, BLOCK_AIR);
				Event_RaiseBlock(&UserEvents.BlockChanged, otherPos, otherBlock, BLOCK_AIR);
			}
		}
	}
}

static void InputHandler_PlaceBlock(void) {
	IVec3 pos, targetPos, otherPos;
	BlockID old, block, targetBlock, otherBlock, newBlock, newOtherBlock;
	
	/* Check if right-clicking on a door to open/close it */
	targetPos = Game_SelectedPos.pos;
	if (Game_SelectedPos.valid && World_Contains(targetPos.x, targetPos.y, targetPos.z)) {
		targetBlock = World_GetBlock(targetPos.x, targetPos.y, targetPos.z);
		
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

	if (!CheckIsFree(block)) return;

	Game_ChangeBlock(pos.x, pos.y, pos.z, block);
	Event_RaiseBlock(&UserEvents.BlockChanged, pos, old, block);
	
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
}

static void InputHandler_PickBlock(void) {
	IVec3 pos;
	BlockID cur;
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
		InputHandler_DeleteBlock();
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

static cc_bool BindTriggered_PlaceBlock(int key, struct InputDevice* device) {
	if (Gui.InputGrab) return false;
	
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
	if (Gui.InputGrab) return false;
	
	if (Inventory_CheckChangeSelected() && Inventory_SelectedBlock != BLOCK_AIR) {
		/* Don't assign SelectedIndex directly, because we don't want held block
		switching positions if they already have air in their inventory hotbar. */
		Inventory_Set(Inventory.SelectedIndex, BLOCK_AIR);
		Event_RaiseVoid(&UserEvents.HeldBlockChanged);
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

static void OnInputDown(void* obj, int key, cc_bool was, struct InputDevice* device) {
	struct Screen* s;
	cc_bool triggered;
	int i;
	if (Input.DownHook && Input.DownHook(key, device)) return;

#ifndef CC_BUILD_WEB
	if (key == device->escapeButton && (s = Gui_GetClosable())) {
		/* Don't want holding down escape to go in and out of pause menu */
		if (!was) Gui_Remove(s);
		return;
	}
#endif

	if (InputHandler_IsShutdown(key)) {
		Window_RequestClose(); return;
	} else if (InputBind_Claims(BIND_SCREENSHOT, key, device) && !was) {
		Game_ScreenshotRequested = true; return;
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
		Gui_Remove(s); return;
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

static void OnInit(void) {
	LocalPlayerInput_Add(&normalInput);
	LocalPlayerInput_Add(&gamepadInput);
	HookInputBinds();
	
	Event_Register_(&PointerEvents.Down,  NULL, OnPointerDown);
	Event_Register_(&PointerEvents.Up,    NULL, OnPointerUp);
	Event_Register_(&InputEvents._down,   NULL, OnInputDownLegacy);
	Event_Register_(&InputEvents.Down2,   NULL, OnInputDown);
	Event_Register_(&InputEvents.Up2,     NULL, OnInputUp);

	Event_Register_(&UserEvents.HackPermsChanged, NULL, InputHandler_CheckZoomFov);
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

static void OnFree(void) {
	HotkeysText.count = 0;
}

struct IGameComponent InputHandler_Component = {
	OnInit, /* Init  */
	OnFree, /* Free  */
};
