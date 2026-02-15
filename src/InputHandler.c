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

static cc_bool Mob_TryPunchMob(void); /* forward declaration */

static void InputHandler_DeleteBlock(void) {
	IVec3 pos, otherPos;
	BlockID old, otherBlock;

	/* Try to punch a mob first (empty hand only) */
	if (Inventory_SelectedBlock == BLOCK_AIR) {
		if (Mob_TryPunchMob()) return;
	}

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
	
	/* If breaking an iron door, also break the other half */
	if (IsIronDoorBottom(old)) {
		otherPos.x = pos.x;
		otherPos.y = pos.y + 1;
		otherPos.z = pos.z;
		if (World_Contains(otherPos.x, otherPos.y, otherPos.z)) {
			otherBlock = World_GetBlock(otherPos.x, otherPos.y, otherPos.z);
			if (IsIronDoorTop(otherBlock)) {
				Game_ChangeBlock(otherPos.x, otherPos.y, otherPos.z, BLOCK_AIR);
				Event_RaiseBlock(&UserEvents.BlockChanged, otherPos, otherBlock, BLOCK_AIR);
			}
		}
	} else if (IsIronDoorTop(old)) {
		otherPos.x = pos.x;
		otherPos.y = pos.y - 1;
		otherPos.z = pos.z;
		if (World_Contains(otherPos.x, otherPos.y, otherPos.z)) {
			otherBlock = World_GetBlock(otherPos.x, otherPos.y, otherPos.z);
			if (IsIronDoorBottom(otherBlock)) {
				Game_ChangeBlock(otherPos.x, otherPos.y, otherPos.z, BLOCK_AIR);
				Event_RaiseBlock(&UserEvents.BlockChanged, otherPos, otherBlock, BLOCK_AIR);
			}
		}
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
		
		/* If clicking on TNT with empty hand, light the fuse */
		if (targetBlock == BLOCK_TNT && Inventory_SelectedBlock == BLOCK_AIR) {
			TNT_ScheduleFuse(targetPos.x, targetPos.y, targetPos.z);
			return;
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

static RNGState mob_rng;
static cc_bool mob_rng_inited;
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
#define MOB_DEAGGRO_RANGE_SQ (24.0f * 24.0f)  /* 24 blocks */
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
#define SKELETON_BACKPEDAL_RANGE_SQ (6.0f * 6.0f)   /* backpedal if closer than 6 blocks */
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

static float    mobSkeletonShootTimer[MAX_NET_PLAYERS]; /* cooldown between shots */
static float    mobTargetYaw[MAX_NET_PLAYERS];          /* desired facing yaw (smooth turning target) */
static cc_bool  mobWalkBackwards[MAX_NET_PLAYERS];      /* true = play walk anim in reverse (skeleton backpedal) */
static float    mobSpiderLeapTimer[MAX_NET_PLAYERS];    /* cooldown between spider leaps */
static float    mobFallStartY[MAX_NET_PLAYERS];         /* Y position when mob started falling */
static cc_bool  mobIsBrownSpider[MAX_NET_PLAYERS];      /* true = brown spider variant (hostile type, passive behavior) */
static float    mobSunDamageTimer[MAX_NET_PLAYERS];     /* accumulates time in sunlight for light sensitivity */
static float    mobLavaDamageTimer[MAX_NET_PLAYERS];    /* accumulates time in lava for lava damage */
static float    mobCactusDamageTimer[MAX_NET_PLAYERS];  /* accumulates time touching cactus for cactus damage */
static cc_uint8 mobCreeperVariant[MAX_NET_PLAYERS];     /* creeper variant type (only valid when mobModelIdx == MOB_IDX_CREEPER) */

/* Creeper variant constants */
#define CREEPER_VAR_STANDARD  0  /* normal creeper: explosion attack */
#define CREEPER_VAR_SURVTEST  1  /* survival test: melee + explode on death (creepera.png) */
#define CREEPER_VAR_MELEE     2  /* melee only, never explodes (creeperb.png) */
#define CREEPER_VAR_NUKE      3  /* easter egg: 20-block radius explosion (creeperc.png) */
#define CREEPER_NUKE_POWER    20 /* explosion radius for nuke creeper */
#define CREEPER_FUSE_CANCEL_RANGE_SQ (5.0f * 5.0f) /* cancel fuse if player > 5 blocks away */

/* Arrow projectile state */
#define MAX_ARROWS 32
static cc_bool  arrowActive[MAX_ARROWS];
static int      arrowEntityId[MAX_ARROWS];    /* entity ID in Entities.List */
static Vec3     arrowVelocity[MAX_ARROWS];    /* per-tick velocity */
static float    arrowLifetime[MAX_ARROWS];    /* seconds remaining */
static IVec3    arrowStuckBlock[MAX_ARROWS];  /* block coords arrow is stuck in */
static cc_bool  arrowIsPlayerArrow[MAX_ARROWS]; /* true if shot by the player (hits mobs, not player) */

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
	return blockTop > entityY;
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
	int bx, bz, feetY;
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

	/* Check X-axis collision: block at new X, current Z (feet and head height) */
	bx = (int)Math_Floor(newX);
	bz = (int)Math_Floor(e->Position.z);
	blockedX = Mob_BlockObstructsAt(bx, feetY, bz, e->Position.y) || Mob_BlockIsSolid(bx, feetY + 1, bz);

	/* Check Z-axis collision: current X, new Z (feet and head height) */
	bx = (int)Math_Floor(e->Position.x);
	bz = (int)Math_Floor(newZ);
	blockedZ = Mob_BlockObstructsAt(bx, feetY, bz, e->Position.y) || Mob_BlockIsSolid(bx, feetY + 1, bz);

	/* Try to jump over 1-block walls */
	canJump = false;
	if ((blockedX || blockedZ) && e->OnGround) {
		bx = (int)Math_Floor(newX);
		bz = (int)Math_Floor(newZ);
		/* Can jump if block above the wall is clear (1-block wall) */
		if (Mob_BlockIsPassable(bx, feetY + 1, bz) && Mob_BlockIsPassable(bx, feetY + 2, bz)) {
			e->Velocity.y = MOB_JUMP_VEL;
			e->OnGround   = false;
			canJump = true;
		}
	}

	/* While mid-air (jumping), recheck collisions at jump height to allow movement over obstacles */
	if (!e->OnGround && e->Velocity.y > 0.0f) {
		if (blockedX) {
			bx = (int)Math_Floor(newX);
			bz = (int)Math_Floor(e->Position.z);
			if (Mob_BlockIsPassable(bx, feetY + 1, bz) && Mob_BlockIsPassable(bx, feetY + 2, bz))
				blockedX = false;
		}
		if (blockedZ) {
			bx = (int)Math_Floor(e->Position.x);
			bz = (int)Math_Floor(newZ);
			if (Mob_BlockIsPassable(bx, feetY + 1, bz) && Mob_BlockIsPassable(bx, feetY + 2, bz))
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

	/* Darken mob when burning in sunlight (light sensitivity) */
	if (id < MAX_NET_PLAYERS && mobSunDamageTimer[id] > 0.0f) {
		r = PackedCol_R(col);
		g = PackedCol_G(col);
		b = PackedCol_B(col);
		r = r * 2 / 5;
		g = g * 2 / 5;
		b = b * 2 / 5;
		col = PackedCol_Make(r, g, b, 255);
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
	}

	/* Now call original NetPlayer RenderModel (lerp + render) - will get our angles */
	origNetPlayerVTABLE->RenderModel(e, delta, t);
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
					int headY = (int)Math_Floor(newY + 1.6f); /* top of mob's head */
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
				int headY = (int)Math_Floor(newY + 1.6f);
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
			int headY = (int)Math_Floor(newY + 1.6f);
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

	Audio_PlayDigSound(SOUND_SHOOT); /* shoot.wav at 100% speed */
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
			arrowLifetime[slot] = 5.0f;
			Audio_PlayDigSound(SOUND_ARROW);
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
					Audio_PlayDigSound(SOUND_HURT);
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
						Audio_PlayDigSound(SOUND_EXPLODE_BIG);
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
			float fdz = pPos.z - e->Position.z;
			if (fdx * fdx + fdz * fdz > CREEPER_FUSE_CANCEL_RANGE_SQ) {
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
				if (Game_CreeperVariants && mobCreeperVariant[id] == CREEPER_VAR_NUKE) {
					TNT_ExplodeRadius(bx, by, bz, CREEPER_NUKE_POWER);
					Audio_PlayDigSound(SOUND_EXPLODE_BIG);
				} else {
					TNT_Explode(bx, by, bz);
				}
				Entities_Remove(id);
				mobType[id]        = MOB_TYPE_NONE;
				mobHasTarget[id]   = false;
				mobIsMoving[id]    = false;
				mobHealth[id]      = 0;
				mobDeathTimer[id]  = 0.0f;
				mobHurtFlash[id]   = 0.0f;
				mobIsAggro[id]     = false;
				mobCreeperFuse[id] = -1.0f;
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
			if (World_Contains(sx, sy, sz) && Lighting.IsLit(sx, sy, sz) && !Mob_IsInWater(e)) {
				mobSunDamageTimer[id] += delta;
				if (mobSunDamageTimer[id] >= 1.0f) {
					mobSunDamageTimer[id] -= 1.0f;
					Mob_DamageMob(id, 2, false);
				}
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

	/* Push mob out of solid blocks (prevents getting stuck in walls) */
	{
		int mx = (int)Math_Floor(e->Position.x);
		int my = (int)Math_Floor(e->Position.y);
		int mz = (int)Math_Floor(e->Position.z);
		if (World_Contains(mx, my, mz) && Mob_BlockIsSolid(mx, my, mz)) {
			e->Position.y = (float)(my + 1);
			e->next.pos.y = (float)(my + 1);
			e->prev.pos.y = (float)(my + 1);
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
			/* Climb: override gravity with upward speed */
			float climbSpeed = MOB_SPEED * MOB_HOSTILE_SPEED_FACTOR * SPIDER_CLIMB_SPEED_FACTOR * delta;
			e->Velocity.y = climbSpeed;
			e->Position.y += climbSpeed;
			e->next.pos.y = e->Position.y;
			e->prev.pos.y = e->Position.y;
		}
	}

	/* Apply horizontal knockback velocity with wall collision */
	if (e->Velocity.x != 0.0f || e->Velocity.z != 0.0f) {
		float newX = e->Position.x + e->Velocity.x;
		float newZ = e->Position.z + e->Velocity.z;
		float halfW = e->Size.x * 0.5f;
		int bx, by, bz;
		cc_bool hitX = false, hitZ = false;

		by = (int)Math_Floor(e->Position.y);

		/* Check X movement against solid blocks */
		bx = (int)Math_Floor(newX + (e->Velocity.x > 0 ? halfW : -halfW));
		bz = (int)Math_Floor(e->Position.z);
		if (Mob_BlockIsSolid(bx, by, bz) || Mob_BlockIsSolid(bx, by + 1, bz)) {
			e->Velocity.x = 0.0f;
			hitX = true;
		}

		/* Check Z movement against solid blocks */
		bx = (int)Math_Floor(e->Position.x);
		bz = (int)Math_Floor(newZ + (e->Velocity.z > 0 ? halfW : -halfW));
		if (Mob_BlockIsSolid(bx, by, bz) || Mob_BlockIsSolid(bx, by + 1, bz)) {
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
			mobIsAggro[id] = true;
		} else if (mobIsAggro[id] && distSq > MOB_DEAGGRO_RANGE_SQ) {
			mobIsAggro[id] = false;
			/* Reset to wandering */
			mobHasTarget[id]   = false;
			mobWanderPause[id] = 0.5f + Random_Float(&mob_rng) * 2.0f;
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
						Audio_PlayDigSound(SOUND_FUSE);
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
							e->Velocity.x += (dx / leapDist) * SPIDER_LEAP_HORIZONTAL * leapScale;
							e->Velocity.z += (dz / leapDist) * SPIDER_LEAP_HORIZONTAL * leapScale;
							e->Velocity.y  = SPIDER_LEAP_VERTICAL;
							e->OnGround    = false;
						}
						mobSpiderLeapTimer[id] = SPIDER_LEAP_COOLDOWN;
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
		} else {
				/* Not aggro: behave like passive mob (wander) */
			if (mobWanderPause[id] > 0.0f) {
				mobWanderPause[id] -= delta;
			} else if (!mobHasTarget[id]) {
				Mob_PickWanderTarget(id, e);
				if (!mobHasTarget[id]) {
					mobWanderPause[id] = 2.0f;
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
				dist = Math_SqrtF(rdist);
				e->Position.x += (rx / dist) * 0.05f;
				e->Position.z += (rz / dist) * 0.05f;
				e->next.pos.x = e->Position.x;
				e->next.pos.z = e->Position.z;
				e->prev.pos.x = e->Position.x;
				e->prev.pos.z = e->Position.z;
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

/* ---- Mob health/damage system ---- */
cc_bool Mob_IsMob(int id) {
	return id >= 0 && id < MAX_NET_PLAYERS && mobType[id] != MOB_TYPE_NONE;
}

cc_bool Mob_IsCreeper(int id) {
	return Mob_IsMob(id) && mobModelIdx[id] == MOB_IDX_CREEPER;
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
	Audio_PlayDigSound(SOUND_FUSE);
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
		switch (mobModelIdx[id]) {
			case MOB_IDX_SKELETON: Audio_PlayDigSound(SOUND_SKELETON_DEATH); break;
			case MOB_IDX_CREEPER:  Audio_PlayDigSound(SOUND_CREEPER_DEATH);  break;
			case MOB_IDX_SPIDER:   Audio_PlayDigSound(SOUND_SPIDER_DEATH);   break;
			case MOB_IDX_ZOMBIE:   Audio_PlayDigSound(SOUND_ZOMBIE_DEATH);   break;
			case MOB_IDX_PIG:      Audio_PlayDigSound(SOUND_PIG_DEATH);      break;
			case MOB_IDX_SHEEP:    Audio_PlayDigSound(SOUND_SHEEP);          break;
			default:               Audio_PlayDigSound(SOUND_HURT);           break;
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
		switch (mobModelIdx[id]) {
			case MOB_IDX_SKELETON: Audio_PlayDigSound(SOUND_SKELETON_HURT); break;
			case MOB_IDX_CREEPER:  Audio_PlayDigSound(SOUND_CREEPER_HURT);  break;
			case MOB_IDX_SPIDER:   Audio_PlayDigSound(SOUND_SPIDER_HURT);   break;
			case MOB_IDX_ZOMBIE:   Audio_PlayDigSound(SOUND_ZOMBIE_HURT);   break;
			case MOB_IDX_PIG:      Audio_PlayDigSound(SOUND_PIG_HURT);      break;
			case MOB_IDX_SHEEP:    Audio_PlayDigSound(SOUND_SHEEP);         break;
			default:               Audio_PlayDigSound(SOUND_HURT);          break;
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
	int targetId;

	/* Only punch with empty hand */
	if (Inventory_SelectedBlock != BLOCK_AIR) return false;

	targetId = Entities_GetClosest(p);
	if (targetId < 0 || !Mob_IsMob(targetId)) return false;

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

	Mob_DamageMob(targetId, 2, true);
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
	mobHealth[id]      = mobIsHostile[idx] ? MOB_HP_HOSTILE : MOB_HP_PASSIVE;
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
	mobCreeperVariant[id] = CREEPER_VAR_STANDARD;

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

	/* 15% chance for brown spider variant (hostile type, passive behavior) */
	if (idx == MOB_IDX_SPIDER && Game_SpiderVariants && Random_Next(&mob_rng, 100) < 15) {
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
	mobHealth[id]      = mobIsHostile[idx] ? MOB_HP_HOSTILE : MOB_HP_PASSIVE;
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

	/* 15% chance for brown spider variant (hostile type, passive behavior) */
	if (idx == MOB_IDX_SPIDER && Game_SpiderVariants && Random_Next(&mob_rng, 100) < 15) {
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

static cc_bool BindTriggered_SpawnMob(int key, struct InputDevice* device) {
	if (Gui.InputGrab) return false;
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
	mobHealth[id]      = MOB_HP_HOSTILE;
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
	mobCreeperVariant[id] = CREEPER_VAR_NUKE;

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
		lightMode = Game_LightRestrictSpawning ? 1 : 0;
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
	arrowLifetime[slot] = 5.0f;
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
		if (!was) Gui_Remove(s);
		return;
	}
#endif

	if (InputHandler_IsShutdown(key)) {
		Window_RequestClose(); return;
	} else if (InputBind_Claims(BIND_SCREENSHOT, key, device) && !was) {
		Game_ScreenshotRequested = true; return;
	}
	
	/* TAB key shoots arrow - must be before bind loop and screen handlers
	   to prevent tab list screen from consuming the event */
	if (key == CCKEY_TAB && !was && !Gui.InputGrab) {
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
	
	ScheduledTask_Add(1.0 / 20, Arrow_ScheduledTick);
	ScheduledTask_Add(1.0, Mob_NaturalSpawnTick);

	Commands_Register(&BoomCommand);

	Event_Register_(&UserEvents.BlockChanged, NULL, Arrow_OnBlockChanged);
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

static void OnNewMap(void) {
	Mob_RemoveAllMobs();
	mobSpawnTimer = 0.0f;
}

struct IGameComponent InputHandler_Component = {
	OnInit,   /* Init  */
	OnFree,   /* Free  */
	NULL,     /* Reset */
	OnNewMap, /* OnNewMap */
};
