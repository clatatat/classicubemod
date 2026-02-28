#include "Screens.h"
#include "Widgets.h"
#include "Game.h"
#include "Event.h"
#include "Platform.h"
#include "Inventory.h"
#include "Drawer2D.h"
#include "Graphics.h"
#include "Funcs.h"
#include "TexturePack.h"
#include "Model.h"
#include "Generator.h"
#include "Server.h"
#include "Chat.h"
#include "ExtMath.h"
#include "Window.h"
#include "Camera.h"
#include "Http.h"
#include "Block.h"
#include "Menus.h"
#include "World.h"
#include "Input.h"
#include "Utils.h"
#include "Options.h"
#include "InputHandler.h"
#include "Protocol.h"
#include "Signs.h"

#define CHAT_MAX_STATUS Array_Elems(Chat_Status)
#define CHAT_MAX_BOTTOMRIGHT Array_Elems(Chat_BottomRight)
#define CHAT_MAX_CLIENTSTATUS Array_Elems(Chat_ClientStatus)

int Screen_FInput(void* s, int key, struct InputDevice* device) { return false; }
int Screen_FKeyPress(void* s, char keyChar)     { return false; }
int Screen_FText(void* s, const cc_string* str) { return false; }
int Screen_FMouseScroll(void* s, float delta)   { return false; }
int Screen_FPointer(void* s, int id, int x, int y) { return false; }

int Screen_TInput(void* s, int key, struct InputDevice* device) { return true; }
int Screen_TKeyPress(void* s, char keyChar)     { return true; }
int Screen_TText(void* s, const cc_string* str) { return true; }
int Screen_TMouseScroll(void* s, float delta)   { return true; }
int Screen_TPointer(void* s, int id, int x, int y) { return true; }

void Screen_NullFunc(void* screen) { }
void Screen_NullUpdate(void* screen, float delta) { }

/* TODO: Remove these */
struct HUDScreen;
struct ChatScreen;
static struct HUDScreen*  Gui_HUD;
static struct ChatScreen* Gui_Chat;
static cc_bool tablist_active;

static cc_bool InventoryScreen_IsHotbarActive(void);
static cc_bool SurvInv_IsScreenOpen(void);
CC_NOINLINE static cc_bool IsOnlyChatActive(void) {
	struct Screen* s;
	int i;

	for (i = 0; i < Gui.ScreensCount; i++) {
		s = Gui_Screens[i];
		if (s->grabsInput && s != (struct Screen*)Gui_Chat) return false;
	}
	return true;
}


/*########################################################################################################################*
*--------------------------------------------------------HUDScreen--------------------------------------------------------*
*#########################################################################################################################*/
static struct HUDScreen {
	Screen_Body
	struct FontDesc font;
	struct TextWidget line1, line2;
	struct TextAtlas posAtlas;
	float accumulator;
	int frames, posCount;
	cc_bool hacksChanged;
	float lastSpeed;
	int lastFov;
	int lastX, lastY, lastZ;
	int lastHealth;
	int prevHealth;       /* health before last damage, for flash animation */
	float damageFlashTimer; /* countdown for flash effect */
	struct HotbarWidget hotbar;
} HUDScreen_Instance CC_BIG_VAR;

/* Each integer can be at most 10 digits + minus prefix */
#define POSITION_VAL_CHARS 11
/* [PREFIX] [(] [X] [,] [Y] [,] [Z] [)] */
#define POSITION_HUD_CHARS (1 + 1 + POSITION_VAL_CHARS + 1 + POSITION_VAL_CHARS + 1 + POSITION_VAL_CHARS + 1)
#define HUD_MAX_VERTICES (4 + TEXTWIDGET_MAX * 2 + HOTBAR_MAX_VERTICES + POSITION_HUD_CHARS * 4 + 10 * 4)

static void HUDScreen_RemakeLine1(struct HUDScreen* s) {
	cc_string status; char statusBuffer[STRING_SIZE * 2];
	int indices, ping, fps;
	float real_fps;

	String_InitArray(status, statusBuffer);
	/* Don't remake texture when FPS isn't being shown */
	if (!Gui.ShowFPS && s->line1.tex.ID) return;
	fps = s->accumulator == 0 ? 1 : (int)(s->frames / s->accumulator);

	if (Gfx.ReducedPerfMode || (Gfx.ReducedPerfModeCooldown > 0)) {
		String_AppendConst(&status, "(low perf mode), ");
		Gfx.ReducedPerfModeCooldown--;
	} else if (fps == 0) {
		/* Running at less than 1 FPS.. */
		real_fps = s->frames / s->accumulator;
		String_Format1(&status, "%f1 fps, ", &real_fps);
	} else {
		String_Format1(&status, "%i fps, ", &fps);
	}

	if (Game_ClassicMode) {
		String_Format1(&status, "%i chunk updates", &Game.ChunkUpdates);
	} else {
		if (Game.ChunkUpdates) {
			String_Format1(&status, "%i chunks/s, ", &Game.ChunkUpdates);
		}

		indices = ICOUNT(Game_Vertices);
		String_Format1(&status, "%i vertices", &indices);

		ping = Ping_AveragePingMS();
		if (ping) String_Format1(&status, ", ping %i ms", &ping);
	}
	TextWidget_Set(&s->line1, &status, &s->font);
	s->dirty = true;
}

static void HUDScreen_BuildPosition(struct HUDScreen* s, struct VertexTextured* data) {
	struct VertexTextured* cur = data;
	struct TextAtlas* atlas = &s->posAtlas;
	struct Texture tex;
	IVec3 pos;

	/* Make "Position: " prefix */
	tex = atlas->tex; 
	tex.x     = 2 + DisplayInfo.ContentOffsetX;
	tex.width = atlas->offset;
	Gfx_Make2DQuad(&tex, PACKEDCOL_WHITE, &cur);

	IVec3_Floor(&pos, &Entities.CurPlayer->Base.Position);
	atlas->curX = tex.x + tex.width;

	/* Make (X, Y, Z) suffix */
	TextAtlas_Add(atlas,       13, &cur);
	TextAtlas_AddInt(atlas, pos.x, &cur);
	TextAtlas_Add(atlas,       11, &cur);
	TextAtlas_AddInt(atlas, pos.y, &cur);
	TextAtlas_Add(atlas,       11, &cur);
	TextAtlas_AddInt(atlas, pos.z, &cur);
	TextAtlas_Add(atlas,       14, &cur);

	s->lastX = pos.x;
	s->lastY = pos.y;
	s->lastZ = pos.z;

	s->posCount = (int)(cur - data);
}

static cc_bool HUDScreen_HasHacksChanged(struct HUDScreen* s) {
	struct HacksComp* hacks = &Entities.CurPlayer->Hacks;
	float speed = HacksComp_CalcSpeedFactor(hacks, hacks->CanSpeed);
	return speed != s->lastSpeed || Camera.Fov != s->lastFov || s->hacksChanged;
}

static void HUDScreen_RemakeLine2(struct HUDScreen* s) {
	cc_string status; char statusBuffer[STRING_SIZE * 2];
	struct HacksComp* hacks = &Entities.CurPlayer->Hacks;
	float speed;
	s->dirty = true;

	if (Game_ClassicMode) {
		TextWidget_SetConst(&s->line2, Game_Version.Name, &s->font);
		return;
	}

	speed = HacksComp_CalcSpeedFactor(hacks, hacks->CanSpeed);
	s->lastSpeed = speed; s->lastFov = Camera.Fov;
	s->hacksChanged = false;

	String_InitArray(status, statusBuffer);
	if (Camera.Fov != Camera.DefaultFov) {
		String_Format1(&status, "Zoom fov %i  ", &Camera.Fov);
	}

	if (hacks->Flying) String_AppendConst(&status, "Fly ON   ");
	if (speed)         String_Format1(&status, "Speed %f1x   ", &speed);
	if (hacks->Noclip) String_AppendConst(&status, "Noclip ON   ");

	TextWidget_Set(&s->line2, &status, &s->font);
}


static void HUDScreen_ContextLost(void* screen) {
	struct HUDScreen* s = (struct HUDScreen*)screen;
	Font_Free(&s->font);
	Screen_ContextLost(screen);

	TextAtlas_Free(&s->posAtlas);
	Elem_Free(&s->hotbar);
	Elem_Free(&s->line1);
	Elem_Free(&s->line2);
}

static void HUDScreen_ContextRecreated(void* screen) {	
	static const cc_string chars  = String_FromConst("0123456789-, ()");
	static const cc_string prefix = String_FromConst("Position: ");

	struct HUDScreen* s = (struct HUDScreen*)screen;
	Screen_UpdateVb(s);

	Font_Make(&s->font, 16, FONT_FLAGS_PADDING);
	Font_SetPadding(&s->font, 2);
	HotbarWidget_SetFont(&s->hotbar, &s->font);

	HUDScreen_RemakeLine1(s);
	TextAtlas_Make(&s->posAtlas, &chars, &s->font, &prefix);
	HUDScreen_RemakeLine2(s);
}

int HUDScreen_LayoutHotbar(void) {
	struct HUDScreen* s = &HUDScreen_Instance;
	s->hotbar.scale     = Gui_GetHotbarScale();
	Widget_Layout(&s->hotbar);
	return s->hotbar.height;
}

static void HUDScreen_Layout(void* screen) {
	struct HUDScreen* s = (struct HUDScreen*)screen;
	struct TextWidget* line1 = &s->line1;
	struct TextWidget* line2 = &s->line2;
	int posY;

	Widget_SetLocation(line1, ANCHOR_MIN, ANCHOR_MIN, 
						2 + DisplayInfo.ContentOffsetX, 2 + DisplayInfo.ContentOffsetY);
	posY = line1->y + line1->height;
	s->posAtlas.tex.y = posY;
	Widget_SetLocation(line2, ANCHOR_MIN, ANCHOR_MIN, 
						2 + DisplayInfo.ContentOffsetX, 0);

	if (Game_ClassicMode) {
		/* Swap around so 0.30 version is at top */
		line2->yOffset = line1->yOffset;
		line1->yOffset = posY;
		Widget_Layout(line1);
	} else {
		/* We can't use y in TextWidget_Make because that DPI scales it */
		line2->yOffset = posY + s->posAtlas.tex.height;
	}

	HUDScreen_LayoutHotbar();
	Widget_Layout(line2);
}

static int HUDScreen_KeyDown(void* screen, int key, struct InputDevice* device) {
	struct HUDScreen* s = (struct HUDScreen*)screen;
	return Elem_HandlesKeyDown(&s->hotbar, key, device);
}

static void HUDScreen_InputUp(void* screen, int key, struct InputDevice* device) {
	struct HUDScreen* s = (struct HUDScreen*)screen;
	if (!InventoryScreen_IsHotbarActive()) return;
	Elem_OnInputUp(&s->hotbar, key, device);
}

static int HUDscreen_PointerDown(void* screen, int id, int x, int y) {
	struct HUDScreen* s = (struct HUDScreen*)screen;
	if (Gui_TouchUI || Gui.InputGrab) {
		return Elem_HandlesPointerDown(&s->hotbar, id, x, y);
	}
	return false;
}

static void HUDScreen_PointerUp(void *screen, int id, int x, int y) {
	struct HUDScreen* s = (struct HUDScreen*)screen;
	if (!Gui_TouchUI) return;
	Elem_OnPointerUp(&s->hotbar, id, x, y);
}

static int HUDScreen_PointerMove(void *screen, int id, int x, int y) {
	struct HUDScreen* s = (struct HUDScreen*)screen;
	if (!Gui_TouchUI) return false;
	return Elem_HandlesPointerMove(&s->hotbar, id, x, y);
}

static int HUDscreen_MouseScroll(void* screen, float delta) {
	struct HUDScreen* s = (struct HUDScreen*)screen;
	/* The default scrolling behaviour (e.g. camera, zoom) needs to be checked */
	/*   BEFORE the hotbar is scrolled, but AFTER chat (maybe) handles scrolling. */
	/* Therefore need to check the default behaviour here, hacky as that may be. */
	if (Input_HandleMouseWheel(delta)) return false;

	if (!Inventory.CanChangeSelected)  return false;
	return Elem_HandlesMouseScroll(&s->hotbar, delta);
}

static void HUDScreen_HacksChanged(void* obj) {
	((struct HUDScreen*)obj)->hacksChanged = true;
}

static void HUDScreen_NeedRedrawing(void* obj) {
	((struct HUDScreen*)obj)->dirty = true;
}

static int heartsCount; /* Number of heart quads actually built (0 if not survival) */
static RNGState heartsRng; /* RNG for low-health heart shaking */
#define LOW_HEALTH_THRESHOLD 4 /* shake when health <= 4 (2 hearts) */

static void HUDScreen_Init(void* screen) {
	struct HUDScreen* s = (struct HUDScreen*)screen;
	s->maxVertices      = HUD_MAX_VERTICES;

	Random_SeedFromCurrentTime(&heartsRng);
	HotbarWidget_Create(&s->hotbar);
	TextWidget_Init(&s->line1);
	TextWidget_Init(&s->line2);
	
	s->line1.flags  |= WIDGET_FLAG_MAINSCREEN;
	s->line2.flags  |= WIDGET_FLAG_MAINSCREEN;

	Event_Register_(&UserEvents.HacksStateChanged, s, HUDScreen_HacksChanged);
	Event_Register_(&TextureEvents.AtlasChanged,   s, HUDScreen_NeedRedrawing);
	Event_Register_(&BlockEvents.BlockDefChanged,  s, HUDScreen_NeedRedrawing);
}

static void HUDScreen_Free(void* screen) {
	Event_Unregister_(&UserEvents.HacksStateChanged, screen, HUDScreen_HacksChanged);
	Event_Unregister_(&TextureEvents.AtlasChanged,   screen, HUDScreen_NeedRedrawing);
	Event_Unregister_(&BlockEvents.BlockDefChanged,  screen, HUDScreen_NeedRedrawing);
}

static void HUDScreen_UpdateFPS(struct HUDScreen* s, float delta) {
	s->frames++;
	s->accumulator += delta;
	if (s->accumulator < 1.0f) return;

	HUDScreen_RemakeLine1(s);
	s->accumulator    = 0.0f;
	s->frames         = 0;
	Game.ChunkUpdates = 0;
}

#define HEART_FLASH_DURATION 0.4f /* seconds for full flash sequence */
#define HEART_FLASH_COUNT   3     /* number of on/off blinks */

static void HUDScreen_Update(void* screen, float delta) {
	struct HUDScreen* s = (struct HUDScreen*)screen;
	IVec3 pos;

	HUDScreen_UpdateFPS(s,          delta);
	HotbarWidget_Update(&s->hotbar, delta);
	if (Game_ClassicMode) return;

	if (IsOnlyChatActive() && Gui.ShowFPS) {
		if (HUDScreen_HasHacksChanged(s)) HUDScreen_RemakeLine2(s);
	}

	IVec3_Floor(&pos, &Entities.CurPlayer->Base.Position);
	if (pos.x != s->lastX || pos.y != s->lastY || pos.z != s->lastZ) {
		s->dirty = true;
	}
	/* Rebuild HUD when player health changes */
	if (Player_Health != s->lastHealth) {
		/* Start flash animation when health decreases */
		if (Player_Health < s->lastHealth) {
			s->prevHealth = s->lastHealth;
			s->damageFlashTimer = HEART_FLASH_DURATION;
		}
		s->lastHealth = Player_Health;
		s->dirty = true;
		/* Show death screen when health reaches 0 in survival mode */
		if (Player_Health <= 0 && Game_SurvivalMode) {
			DeathScreen_Show();
		}
	}
	/* Tick flash timer and keep rebuilding while flashing */
	if (s->damageFlashTimer > 0.0f) {
		s->damageFlashTimer -= delta;
		s->dirty = true;
	}
	/* Keep rebuilding when low health so hearts shake */
	if (Player_Health > 0 && Player_Health <= LOW_HEALTH_THRESHOLD && Game_SurvivalMode) {
		s->dirty = true;
	}
}

#define CH_EXTENT 16

/* Icons.png layout: 16x16 crosshair at (0,0), then 9x9 heart tiles starting at x=16 */
#define ICON_HEART_SIZE 9
#define ICON_HEARTS_X   16  /* pixel offset where heart tiles begin */

/* Heart tile indices (offset from ICON_HEARTS_X, each 9px wide) */
#define ICON_HEART_EMPTY 0  /* x=16: empty container outline */
#define ICON_HEART_FULL  4  /* x=52: full red heart */
#define ICON_HEART_HALF  5  /* x=61: half red heart */
#define ICON_HEART_FLASH_FULL 6 /* x=70: pink/white full heart (damage flash) */
#define ICON_HEART_FLASH_HALF 7 /* x=79: pink/white half heart (damage flash) */

/* Calculate UV coordinates for a heart icon tile */
static void Icon_GetHeartUV(int heartIndex, float* u1, float* v1, float* u2, float* v2) {
	int x = ICON_HEARTS_X + heartIndex * ICON_HEART_SIZE;
	*u1 = x / 256.0f;
	*v1 = 0.0f;
	*u2 = (x + ICON_HEART_SIZE) / 256.0f;
	*v2 = ICON_HEART_SIZE / 256.0f;
}

static void HUDScreen_BuildHeartsMesh(struct VertexTextured** ptr) {
	struct HUDScreen* s = &HUDScreen_Instance;
	struct Texture tex;
	float u1, v1, u2, v2;
	int i, heartSize, heartSpacing, totalWidth, startX, startY;
	int health = Player_Health;
	cc_bool flashing = s->damageFlashTimer > 0.0f;
	cc_bool flashOn  = false;
	int flashCycle   = 0;
	int prevHealth = s->prevHealth;

	/* Determine if we're in the "on" (pink) or "off" phase of the blink */
	if (flashing) {
		float cycleLen = HEART_FLASH_DURATION / HEART_FLASH_COUNT;
		float elapsed  = HEART_FLASH_DURATION - s->damageFlashTimer;
		flashCycle     = (int)(elapsed / cycleLen);
		float inCycle  = elapsed - flashCycle * cycleLen;
		flashOn = inCycle < (cycleLen * 0.5f);
	}

	heartsCount = 0;
	if (!Game_SurvivalMode) return;

	/* Scale heart size with hotbar */
	heartSize    = (int)(9.0f * s->hotbar.scale * DisplayInfo.ScaleY);
	heartSpacing = -1;
	totalWidth   = 10 * heartSize + 9 * heartSpacing;

	/* Position hearts left-aligned with hotbar */
	startX = s->hotbar.x;
	startY = s->hotbar.y - heartSize - (int)(2.0f * s->hotbar.scale * DisplayInfo.ScaleY);

	tex.width  = heartSize;
	tex.height = heartSize;

	for (i = 0; i < 10; i++) {
		/* Calculate health state for this heart position */
		int hp = health - i * 2;
		int tileIndex;

		if (hp >= 2) {
			tileIndex = ICON_HEART_FULL;
		} else if (hp == 1) {
			tileIndex = ICON_HEART_HALF;
		} else {
			tileIndex = ICON_HEART_EMPTY;
		}

		/* During flash, blink affected hearts between pink and normal/empty */
		if (flashing) {
			int prevHp = prevHealth - i * 2;
			cc_bool affected = (prevHp >= 2 && hp < 2) || (prevHp == 1 && hp <= 0);
			if (affected) {
				if (flashOn) {
					/* "On" phase: show pink flash icon */
					tileIndex = (prevHp >= 2) ? ICON_HEART_FLASH_FULL : ICON_HEART_FLASH_HALF;
				} else if (flashCycle == 0) {
					/* First "off" phase: flash back to previous (non-empty) state */
					tileIndex = (prevHp >= 2) ? ICON_HEART_FULL : ICON_HEART_HALF;
				}
				/* Later "off" phases: keep current (empty) tileIndex as-is */
			}
		}

		Icon_GetHeartUV(tileIndex, &u1, &v1, &u2, &v2);
		tex.x = startX + i * (heartSize + heartSpacing);
		tex.y = startY;

		/* Shake hearts randomly when low health */
		if (health > 0 && health <= LOW_HEALTH_THRESHOLD) {
			tex.y += Random_Next(&heartsRng, 3) - 1; /* -1, 0, or +1 pixel */
		}
		tex.uv.u1 = u1; tex.uv.v1 = v1;
		tex.uv.u2 = u2; tex.uv.v2 = v2;

		Gfx_Make2DQuad(&tex, PACKEDCOL_WHITE, ptr);
		heartsCount++;
	}
}

static void HUDScreen_BuildCrosshairsMesh(struct VertexTextured** ptr) {
	/* Full icons.png (256x256), crosshair is 15x15 pixels from top-left */
	static struct Texture tex = { 0, Tex_Rect(0,0,0,0), Tex_UV(0.0f,0.0f, 15/256.0f,15/256.0f) };
	int extent;

	extent = (int)(CH_EXTENT * Gui_GetCrosshairScale());
	tex.x  = (Window_Main.Width  / 2) - extent;
	tex.y  = (Window_Main.Height / 2) - extent;

	tex.width  = extent * 2;
	tex.height = extent * 2;
	Gfx_Make2DQuad(&tex, PACKEDCOL_WHITE, ptr);
}

static void HUDScreen_BuildMesh(void* screen) {
	struct HUDScreen* s = (struct HUDScreen*)screen;
	struct VertexTextured* data;
	struct VertexTextured** ptr;

	data = Screen_LockVb(s);
	ptr  = &data;

	HUDScreen_BuildCrosshairsMesh(ptr);
	Widget_BuildMesh(&s->line1,  ptr);
	Widget_BuildMesh(&s->line2,  ptr);
	Widget_BuildMesh(&s->hotbar, ptr);

	if (!Game_ClassicMode) 
		HUDScreen_BuildPosition(s, data);
	/* Advance ptr past position area to place hearts after it */
	*ptr = data + POSITION_HUD_CHARS * 4;
	HUDScreen_BuildHeartsMesh(ptr);
	Gfx_UnlockDynamicVb(s->vb);
}

static void HUDScreen_Render(void* screen, float delta) {
	struct HUDScreen* s = (struct HUDScreen*)screen;
	if (Game_HideGui) return;

	Gfx_3DS_SetRenderScreen(TOP_SCREEN);

	Gfx_SetVertexFormat(VERTEX_FORMAT_TEXTURED);
	Gfx_BindDynamicVb(s->vb);
	if (Gui.ShowFPS) Widget_Render2(&s->line1, 4);

	if (Game_ClassicMode) {
		Widget_Render2(&s->line2, 8);
	} else if (IsOnlyChatActive() && Gui.ShowFPS) {
		Widget_Render2(&s->line2, 8);
		Gfx_BindTexture(s->posAtlas.tex.ID);
		Gfx_DrawVb_IndexedTris_Range(s->posCount, 12 + HOTBAR_MAX_VERTICES, DRAW_HINT_RECT);
		/* TODO swap these two lines back */
	}

	if (!Gui_GetBlocksWorld()) {
		Gfx_BindDynamicVb(s->vb);
		if (!Gui.HideHotbar && !SurvInv_IsScreenOpen()) Widget_Render2(&s->hotbar, 12);

		if (!Gui.HideCrosshair && Gui.IconsTex && !tablist_active) {
			Gfx_BindTexture(Gui.IconsTex);
			Gfx_BindDynamicVb(s->vb); /* Have to rebind for mobile right now... */
			Gfx_DrawVb_IndexedTris_Range(4, 0, DRAW_HINT_SPRITE);
		}

		/* Render health hearts (survival mode only) */
		if (heartsCount > 0 && Gui.IconsTex && !Gui.HideHotbar && !SurvInv_IsScreenOpen()) {
			Gfx_BindTexture(Gui.IconsTex);
			Gfx_BindDynamicVb(s->vb);
			Gfx_DrawVb_IndexedTris_Range(heartsCount * 4,
				12 + HOTBAR_MAX_VERTICES + POSITION_HUD_CHARS * 4, DRAW_HINT_SPRITE);
		}
	}

	Gfx_3DS_SetRenderScreen(BOTTOM_SCREEN);
}

static const struct ScreenVTABLE HUDScreen_VTABLE = {
	HUDScreen_Init,        HUDScreen_Update,    HUDScreen_Free,
	HUDScreen_Render,      HUDScreen_BuildMesh,
	HUDScreen_KeyDown,     HUDScreen_InputUp,   Screen_FKeyPress, Screen_FText,
	HUDscreen_PointerDown, HUDScreen_PointerUp, HUDScreen_PointerMove,  HUDscreen_MouseScroll,
	HUDScreen_Layout,      HUDScreen_ContextLost, HUDScreen_ContextRecreated
};
void HUDScreen_Show(void) {
	struct HUDScreen* s = &HUDScreen_Instance;
	s->VTABLE = &HUDScreen_VTABLE;
	Gui_HUD   = s;
	Gui_Add((struct Screen*)s, GUI_PRIORITY_HUD);
}


/*########################################################################################################################*
*----------------------------------------------------TabListOverlay-----------------------------------------------------*
*#########################################################################################################################*/
#ifdef CC_BUILD_NETWORKING

#define GROUP_NAME_ID UInt16_MaxValue
#define LIST_COLUMN_PADDING 5
#define LIST_NAMES_PER_COLUMN 16
#define TABLIST_MAX_ENTRIES (TABLIST_MAX_NAMES * 2)
typedef int (*TabListEntryCompare)(int x, int y);

static struct TabListOverlay {
	Screen_Body
	int x, y, width, height;
	cc_bool classic, staysOpen;
	int usedCount, elementOffset;
	struct TextWidget title;
	struct FontDesc font;
	TabListEntryCompare compare;
	cc_uint16 ids[TABLIST_MAX_ENTRIES];
	struct Texture textures[TABLIST_MAX_ENTRIES];
} TabListOverlay_Instance CC_BIG_VAR;
#define TABLIST_MAX_VERTICES (TEXTWIDGET_MAX + 4 * TABLIST_MAX_ENTRIES)

static void TabListOverlay_DrawText(struct Texture* tex, struct TabListOverlay* s, const cc_string* name) {
	cc_string tmp; char tmpBuffer[STRING_SIZE];
	struct DrawTextArgs args;

	if (Game_PureClassic) {
		String_InitArray(tmp, tmpBuffer);
		String_AppendColorless(&tmp, name);
	} else {
		tmp = *name;
	}

	DrawTextArgs_Make(&args, &tmp, &s->font, !s->classic);
	Drawer2D_MakeTextTexture(tex, &args);
}

static int TabListOverlay_GetColumnWidth(struct TabListOverlay* s, int column) {
	int i   = column * LIST_NAMES_PER_COLUMN;
	int end = min(s->usedCount, i + LIST_NAMES_PER_COLUMN);
	int maxWidth = 0;

	for (; i < end; i++) 
	{
		maxWidth = max(maxWidth, s->textures[i].width);
	}
	return maxWidth + LIST_COLUMN_PADDING + s->elementOffset;
}

static int TabListOverlay_GetColumnHeight(struct TabListOverlay* s, int column) {
	int i   = column * LIST_NAMES_PER_COLUMN;
	int end = min(s->usedCount, i + LIST_NAMES_PER_COLUMN);
	int height = 0;

	for (; i < end; i++) 
	{
		height += s->textures[i].height + 1;
	}
	return height;
}

static void TabListOverlay_SetColumnPos(struct TabListOverlay* s, int column, int x, int y) {
	struct Texture tex;
	int i   = column * LIST_NAMES_PER_COLUMN;
	int end = min(s->usedCount, i + LIST_NAMES_PER_COLUMN);

	for (; i < end; i++) 
	{
		tex = s->textures[i];
		tex.x = x; tex.y = y - 10;

		y += tex.height + 1;
		/* offset player names a bit, compared to group name */
		if (!s->classic && s->ids[i] != GROUP_NAME_ID) {
			tex.x += s->elementOffset;
		}
		s->textures[i] = tex;
	}
}

static void TabListOverlay_Layout(void* screen) {
	struct TabListOverlay* s = (struct TabListOverlay*)screen;
	int minWidth, minHeight, paddingX, paddingY;
	int i, x, y, width = 0, height = 0;
	int columns = Math_CeilDiv(s->usedCount, LIST_NAMES_PER_COLUMN);

	for (i = 0; i < columns; i++) 
	{
		width += TabListOverlay_GetColumnWidth(s,  i);
		y      = TabListOverlay_GetColumnHeight(s, i);
		height = max(height, y);
	}

	minWidth = Display_ScaleX(480);
	width    = max(width, minWidth);
	paddingX = Display_ScaleX(10);
	paddingY = Display_ScaleY(10);

	width  += paddingX * 2;
	height += paddingY * 2;

	y    = Window_UI.Height / 4 - height / 2;
	s->x = Gui_CalcPos(ANCHOR_CENTRE,          0, width , Window_UI.Width );
	s->y = Gui_CalcPos(ANCHOR_CENTRE, -max(0, y), height, Window_UI.Height);

	x = s->x + paddingX;
	y = s->y + paddingY;

	for (i = 0; i < columns; i++) 
	{
		TabListOverlay_SetColumnPos(s, i, x, y);
		x += TabListOverlay_GetColumnWidth(s, i);
	}

	s->y -= (s->title.height + paddingY);
	s->width  = width;
	minHeight = Display_ScaleY(300);
	s->height = max(minHeight, height + s->title.height);

	s->title.horAnchor = ANCHOR_CENTRE;
	s->title.yOffset   = s->y + paddingY / 2;
	Widget_Layout(&s->title);
}

static void TabListOverlay_AddName(struct TabListOverlay* s, EntityID id, int index) {
	cc_string name;
	/* insert at end of list */
	if (index == -1) { index = s->usedCount; s->usedCount++; }

	name = TabList_UNSAFE_GetList(id);
	s->ids[index] = id;
	TabListOverlay_DrawText(&s->textures[index], s, &name);
}

static void TabListOverlay_DeleteAt(struct TabListOverlay* s, int i) {
	Gfx_DeleteTexture(&s->textures[i].ID);

	for (; i < s->usedCount - 1; i++)
	{
		s->ids[i]      = s->ids[i + 1];
		s->textures[i] = s->textures[i + 1];
	}

	s->usedCount--;
	s->ids[s->usedCount]         = 0;
	s->textures[s->usedCount].ID = 0;
}

static void TabListOverlay_AddGroup(struct TabListOverlay* s, int id, int* index) {
	cc_string group;
	int i;
	group = TabList_UNSAFE_GetGroup(id);

	for (i = Array_Elems(s->ids) - 1; i > (*index); i--) 
	{
		s->ids[i]      = s->ids[i - 1];
		s->textures[i] = s->textures[i - 1];
	}
	
	s->ids[*index] = GROUP_NAME_ID;
	s->textures[*index].ID = 0; /* TODO: TEMP HACK! */
	TabListOverlay_DrawText(&s->textures[*index], s, &group);

	(*index)++;
	s->usedCount++;
}

static int TabListOverlay_GetGroupCount(struct TabListOverlay* s, int id, int i) {
	cc_string group, curGroup;
	int count;
	group = TabList_UNSAFE_GetGroup(id);

	for (count = 0; i < s->usedCount; i++, count++)
	{
		curGroup = TabList_UNSAFE_GetGroup(s->ids[i]);
		if (!String_CaselessEquals(&group, &curGroup)) break;
	}
	return count;
}

static int TabListOverlay_PlayerCompare(int x, int y) {
	cc_string xName; char xNameBuffer[STRING_SIZE];
	cc_string yName; char yNameBuffer[STRING_SIZE];
	cc_uint8 xRank, yRank;
	cc_string xNameRaw, yNameRaw;

	xRank = TabList.GroupRanks[x];
	yRank = TabList.GroupRanks[y];
	if (xRank != yRank) return (xRank < yRank ? -1 : 1);
	
	String_InitArray(xName, xNameBuffer);
	xNameRaw = TabList_UNSAFE_GetList(x);
	String_AppendColorless(&xName, &xNameRaw);

	String_InitArray(yName, yNameBuffer);
	yNameRaw = TabList_UNSAFE_GetList(y);
	String_AppendColorless(&yName, &yNameRaw);

	return String_Compare(&xName, &yName);
}

static int TabListOverlay_GroupCompare(int x, int y) {
	cc_string xGroup, yGroup;
	/* TODO: should we use colourless comparison? ClassicalSharp sorts groups with colours */
	xGroup = TabList_UNSAFE_GetGroup(x);
	yGroup = TabList_UNSAFE_GetGroup(y);
	return String_Compare(&xGroup, &yGroup);
}

static void TabListOverlay_QuickSort(int left, int right) {
	struct Texture* values = TabListOverlay_Instance.textures; struct Texture value;
	cc_uint16* keys        = TabListOverlay_Instance.ids; cc_uint16 key;
	TabListEntryCompare compareEntries = TabListOverlay_Instance.compare;

	while (left < right) {
		int i = left, j = right;
		int pivot = keys[(i + j) / 2];

		/* partition the list */
		while (i <= j) {
			while (compareEntries(pivot, keys[i]) > 0) i++;
			while (compareEntries(pivot, keys[j]) < 0) j--;
			QuickSort_Swap_KV_Maybe();
		}
		/* recurse into the smaller subset */
		QuickSort_Recurse(TabListOverlay_QuickSort)
	}
}

static void TabListOverlay_SortEntries(struct TabListOverlay* s) {
	int i, id, count;
	if (!s->usedCount) return;

	if (s->classic) {
		TabListOverlay_Instance.compare = TabListOverlay_PlayerCompare;
		TabListOverlay_QuickSort(0, s->usedCount - 1);
		return;
	}

	/* Sort the list by group */
	/* Loop backwards, since DeleteAt() reduces NamesCount */
	for (i = s->usedCount - 1; i >= 0; i--)
	{
		if (s->ids[i] != GROUP_NAME_ID) continue;
		TabListOverlay_DeleteAt(s, i);
	}
	TabListOverlay_Instance.compare = TabListOverlay_GroupCompare;
	TabListOverlay_QuickSort(0, s->usedCount - 1);

	/* Sort the entries in each group */
	TabListOverlay_Instance.compare = TabListOverlay_PlayerCompare;
	for (i = 0; i < s->usedCount; )
	{
		id = s->ids[i];
		TabListOverlay_AddGroup(s, id, &i);

		count = TabListOverlay_GetGroupCount(s, id, i);
		TabListOverlay_QuickSort(i, i + (count - 1));
		i += count;
	}
}

static void TabListOverlay_SortAndLayout(struct TabListOverlay* s) {
	TabListOverlay_SortEntries(s);
	TabListOverlay_Layout(s);
	s->dirty = true;
}

static void TabListOverlay_Add(void* obj, int id) {
	struct TabListOverlay* s = (struct TabListOverlay*)obj;
	TabListOverlay_AddName(s, id, -1);
	TabListOverlay_SortAndLayout(s);
}

static void TabListOverlay_Update(void* obj, int id) {
	struct TabListOverlay* s = (struct TabListOverlay*)obj;
	int i;
	for (i = 0; i < s->usedCount; i++)
	{
		if (s->ids[i] != id) continue;
		Gfx_DeleteTexture(&s->textures[i].ID);

		TabListOverlay_AddName(s, id, i);
		TabListOverlay_SortAndLayout(s);
		return;
	}
}

static void TabListOverlay_Remove(void* obj, int id) {
	struct TabListOverlay* s = (struct TabListOverlay*)obj;
	int i;
	for (i = 0; i < s->usedCount; i++)
	{
		if (s->ids[i] != id) continue;

		TabListOverlay_DeleteAt(s, i);
		TabListOverlay_SortAndLayout(s);
		return;
	}
}

static int TabListOverlay_PointerDown(void* screen, int id, int x, int y) {
	struct TabListOverlay* s = (struct TabListOverlay*)screen;
	cc_string text; char textBuffer[STRING_SIZE * 4];
	struct Texture tex;
	cc_string player;
	int i;

	if (!((struct Screen*)Gui_Chat)->grabsInput) return false;
	String_InitArray(text, textBuffer);

	for (i = 0; i < s->usedCount; i++)
	{
		if (!s->textures[i].ID || s->ids[i] == GROUP_NAME_ID) continue;
		tex = s->textures[i];
		if (!Gui_Contains(tex.x, tex.y, tex.width, tex.height, x, y)) continue;

		player = TabList_UNSAFE_GetPlayer(s->ids[i]);
		String_Format1(&text, "%s ", &player);
		ChatScreen_AppendInput(&text);
		return TOUCH_TYPE_GUI;
	}
	return false;
}

static void TabListOverlay_KeyUp(void* screen, int key, struct InputDevice* device) {
	struct TabListOverlay* s = (struct TabListOverlay*)screen;
	if (!InputBind_Claims(BIND_TABLIST, key, device) || s->staysOpen) return;
	Gui_Remove((struct Screen*)s);
}

static void TabListOverlay_ContextLost(void* screen) {
	struct TabListOverlay* s = (struct TabListOverlay*)screen;
	int i;
	for (i = 0; i < s->usedCount; i++)
	{
		Gfx_DeleteTexture(&s->textures[i].ID);
	}

	Elem_Free(&s->title);
	Font_Free(&s->font);
	Screen_ContextLost(screen);
}

static void TabListOverlay_ContextRecreated(void* screen) {
	struct TabListOverlay* s = (struct TabListOverlay*)screen;
	int size, id;

	size = Drawer2D.BitmappedText ? 16 : 11;
	Font_Make(&s->font, size, FONT_FLAGS_PADDING);
	s->usedCount = 0;

	TextWidget_SetConst(&s->title, "Connected players:", &s->font);
	Font_SetPadding(&s->font, 1);
	Screen_UpdateVb(screen);

	/* TODO: Just recreate instead of this? maybe */
	for (id = 0; id < TABLIST_MAX_NAMES; id++) 
	{
		if (!TabList.NameOffsets[id]) continue;
		TabListOverlay_AddName(s, (EntityID)id, -1);
	}
	TabListOverlay_SortAndLayout(s); /* TODO: Not do layout here too */
}

static void TabListOverlay_BuildMesh(void* screen) {
	struct TabListOverlay* s = (struct TabListOverlay*)screen;
	struct Screen*   grabbed = Gui_GetInputGrab();
	struct VertexTextured* v;
	struct Texture tex;
	int i;
	
	v = (struct VertexTextured*)Gfx_LockDynamicVb(s->vb,
										VERTEX_FORMAT_TEXTURED, TEXTWIDGET_MAX + s->usedCount * 4);
	Widget_BuildMesh(&s->title, &v);

	for (i = 0; i < s->usedCount; i++)
	{
		if (!s->textures[i].ID) continue;
		tex = s->textures[i];

		if (grabbed && s->ids[i] != GROUP_NAME_ID) {
			if (Gui_ContainsPointers(tex.x, tex.y, tex.width, tex.height)) tex.x += 4;
		}
		Gfx_Make2DQuad(&tex, PACKEDCOL_WHITE, &v);
	}
	Gfx_UnlockDynamicVb(s->vb);
}

static void TabListOverlay_Render(void* screen, float delta) {
	struct TabListOverlay* s = (struct TabListOverlay*)screen;
	int i, offset = 0;
	PackedCol topCol    = PackedCol_Make( 0,  0,  0, 180);
	PackedCol bottomCol = PackedCol_Make(50, 50, 50, 205);

	if (Game_HideGui || !IsOnlyChatActive()) return;

	Gfx_3DS_SetRenderScreen(TOP_SCREEN);

	Gfx_Draw2DGradient(s->x, s->y, s->width, s->height, topCol, bottomCol);

	Gfx_SetVertexFormat(VERTEX_FORMAT_TEXTURED);
	Gfx_BindDynamicVb(s->vb);
	offset = Widget_Render2(&s->title, offset);

	for (i = 0; i < s->usedCount; i++)
	{
		if (!s->textures[i].ID) continue;
		Gfx_BindTexture(s->textures[i].ID);

		Gfx_DrawVb_IndexedTris_Range(4, offset, DRAW_HINT_RECT);
		offset += 4;
	}

	Gfx_3DS_SetRenderScreen(BOTTOM_SCREEN);
}

static void TabListOverlay_Free(void* screen) {
	struct TabListOverlay* s = (struct TabListOverlay*)screen;
	tablist_active = false;
	Event_Unregister_(&TabListEvents.Added,   s, TabListOverlay_Add);
	Event_Unregister_(&TabListEvents.Changed, s, TabListOverlay_Update);
	Event_Unregister_(&TabListEvents.Removed, s, TabListOverlay_Remove);
}

static void TabListOverlay_Init(void* screen) {
	struct TabListOverlay* s = (struct TabListOverlay*)screen;
	tablist_active   = true;
	s->classic       = Gui.ClassicTabList || !Server.SupportsExtPlayerList;
	s->elementOffset = s->classic ? 0 : 10;
	s->maxVertices   = TABLIST_MAX_VERTICES;
	TextWidget_Init(&s->title);

	Event_Register_(&TabListEvents.Added,   s, TabListOverlay_Add);
	Event_Register_(&TabListEvents.Changed, s, TabListOverlay_Update);
	Event_Register_(&TabListEvents.Removed, s, TabListOverlay_Remove);
}

static const struct ScreenVTABLE TabListOverlay_VTABLE = {
	TabListOverlay_Init,        Screen_NullUpdate,     TabListOverlay_Free,
	TabListOverlay_Render,      TabListOverlay_BuildMesh,
	Screen_FInput,              TabListOverlay_KeyUp,  Screen_FKeyPress, Screen_FText,
	TabListOverlay_PointerDown, Screen_PointerUp,      Screen_FPointer,  Screen_FMouseScroll,
	TabListOverlay_Layout, TabListOverlay_ContextLost, TabListOverlay_ContextRecreated
};
void TabListOverlay_Show(cc_bool staysOpen) {
	struct TabListOverlay* s  = &TabListOverlay_Instance;
	s->VTABLE    = &TabListOverlay_VTABLE;
	s->staysOpen = staysOpen;
	Gui_Add((struct Screen*)s, GUI_PRIORITY_TABLIST);
}
#else
void TabListOverlay_Show(cc_bool staysOpen) { }
#endif


/*########################################################################################################################*
*--------------------------------------------------------ChatScreen-------------------------------------------------------*
*#########################################################################################################################*/
static struct ChatScreen {
	Screen_Body
	float chatAcc;
	cc_bool suppressNextPress;
	int chatIndex, paddingX, paddingY;
	int lastDownloadStatus;
	struct FontDesc chatFont;
	struct ChatInputWidget input;
	struct TextGroupWidget chat, clientStatus;
	struct SpecialInputWidget altText;
#ifdef CC_BUILD_TOUCH
	struct ButtonWidget send, cancel, more;
#endif

	struct Texture clientStatusTextures[CHAT_MAX_CLIENTSTATUS];
	struct Texture chatTextures[GUI_MAX_CHATLINES];
} ChatScreen_Instance CC_BIG_VAR;

static void ChatScreen_UpdateChatYOffsets(struct ChatScreen* s) {
	int pad, y;
	/* Determining chat Y requires us to know hotbar's position */
	HUDScreen_LayoutHotbar();
		
	y = min(s->input.base.y, Gui_HUD->hotbar.y);
	y -= s->input.base.yOffset; /* add some padding */
	s->altText.yOffset = Window_UI.Height - y;
	Widget_Layout(&s->altText);

	pad = s->altText.active ? 5 : 10;
	s->clientStatus.yOffset = Window_UI.Height - s->altText.y + pad;
	Widget_Layout(&s->clientStatus);
	s->chat.yOffset = s->clientStatus.yOffset + s->clientStatus.height;
	Widget_Layout(&s->chat);
}

static void ChatScreen_OnInputTextChanged(void* elem) {
	ChatScreen_UpdateChatYOffsets(&ChatScreen_Instance);
}

static cc_string ChatScreen_GetChat(int i) {
	i += ChatScreen_Instance.chatIndex;

	if (i >= 0 && i < Chat_Log.count) {
		return StringsBuffer_UNSAFE_Get(&Chat_Log, i);
	}
	return String_Empty;
}

static cc_string ChatScreen_GetClientStatus(int i) { return Chat_ClientStatus[i]; }

static void ChatScreen_FreeChatFonts(struct ChatScreen* s) {
	Font_Free(&s->chatFont);
}

static cc_bool ChatScreen_ChatUpdateFont(struct ChatScreen* s) {
	int size = (int)(8  * Gui_GetChatScale());
	Math_Clamp(size, 8, 64);

	/* don't recreate font if possible */
	/* TODO: Add function for this, don't use Display_ScaleY (Drawer2D_SameFontSize ??) */
	if (Display_ScaleY(size) == s->chatFont.size) return false;
	ChatScreen_FreeChatFonts(s);
	Font_Make(&s->chatFont, size, FONT_FLAGS_PADDING);

	ChatInputWidget_SetFont(&s->input,        &s->chatFont);
	TextGroupWidget_SetFont(&s->chat,         &s->chatFont);
	TextGroupWidget_SetFont(&s->clientStatus, &s->chatFont);
	return true;
}

static void ChatScreen_Redraw(struct ChatScreen* s) {
	TextGroupWidget_RedrawAll(&s->chat);
	TextGroupWidget_RedrawAll(&s->clientStatus);

	if (s->grabsInput) InputWidget_UpdateText(&s->input.base);
	SpecialInputWidget_Redraw(&s->altText);
}

static int ChatScreen_ClampChatIndex(int index) {
	int maxIndex = Chat_Log.count - Gui.Chatlines;
	int minIndex = min(0, maxIndex);
	Math_Clamp(index, minIndex, maxIndex);
	return index;
}

static void ChatScreen_ScrollChatBy(struct ChatScreen* s, int delta) {
	int newIndex = ChatScreen_ClampChatIndex(s->chatIndex + delta);
	delta = newIndex - s->chatIndex;
	if (Game_PureClassic) return;

	while (delta) {
		if (delta < 0) {
			/* scrolling up to oldest */
			s->chatIndex--; delta++;
			TextGroupWidget_ShiftDown(&s->chat);
		} else {
			/* scrolling down to newest */
			s->chatIndex++; delta--;
			TextGroupWidget_ShiftUp(&s->chat);
		}
	}
}

static void ChatScreen_EnterChatInput(struct ChatScreen* s, cc_bool close) {
	struct InputWidget* input;
	int defaultIndex;

	s->grabsInput = false;
	Gui_UpdateInputGrab();
	OnscreenKeyboard_Close();
	if (close) InputWidget_Clear(&s->input.base);

	input = &s->input.base;
	input->OnPressedEnter(input);
	SpecialInputWidget_SetActive(&s->altText, false);
	ChatScreen_UpdateChatYOffsets(s);

	/* Reset chat when user has scrolled up in chat history */
	defaultIndex = Chat_Log.count - Gui.Chatlines;
	if (s->chatIndex != defaultIndex) {
		s->chatIndex = defaultIndex;
		TextGroupWidget_RedrawAll(&s->chat);
	}
}

static void ChatScreen_ColCodeChanged(void* screen, int code) {
	struct ChatScreen* s = (struct ChatScreen*)screen;
	float caretAcc;
	if (Gfx.LostContext) return;

	SpecialInputWidget_UpdateCols(&s->altText);
	TextGroupWidget_RedrawAllWithCol(&s->chat,         code);
	TextGroupWidget_RedrawAllWithCol(&s->clientStatus, code);

	/* Some servers have plugins that redefine colours constantly */
	/* Preserve caret accumulator so caret blinking stays consistent */
	caretAcc = s->input.base.caretAccumulator;
	InputWidget_UpdateText(&s->input.base);
	s->input.base.caretAccumulator = caretAcc;
}

static void ChatScreen_ChatReceived(void* screen, const cc_string* msg, int type) {
	struct ChatScreen* s = (struct ChatScreen*)screen;
	if (Gfx.LostContext) return;

	if (type == MSG_TYPE_NORMAL) {
		s->dirty = true;
		s->chatIndex++;
		if (!Gui.Chatlines) return;

		TextGroupWidget_ShiftUp(&s->chat);
	} else if (type >= MSG_TYPE_CLIENTSTATUS_1 && type <= MSG_TYPE_CLIENTSTATUS_2) {
		s->dirty = true;
		TextGroupWidget_Redraw(&s->clientStatus, type - MSG_TYPE_CLIENTSTATUS_1);
		ChatScreen_UpdateChatYOffsets(s);
	}
}


static void ChatScreen_Update(void* screen, float delta) {
	
}

static void ChatScreen_DrawChatBackground(struct ChatScreen* s) {
	int usedHeight = TextGroupWidget_UsedHeight(&s->chat);
	int x = s->chat.x;
	int y = s->chat.y + s->chat.height - usedHeight;

	int width  = max(s->clientStatus.width, s->chat.width);
	int height = usedHeight + s->clientStatus.height;

	if (height > 0) {
		PackedCol backCol = PackedCol_Make(0, 0, 0, 127);
		Gfx_Draw2DFlat( x - s->paddingX,          y - s->paddingY, 
					width + s->paddingX * 2, height + s->paddingY * 2, backCol);
	}
}

static void ChatScreen_DrawChat(struct ChatScreen* s, float delta) {
	GfxResourceID texID;
	double now;
	int i, logIdx;

	Elem_Render(&s->clientStatus);

	Gfx_SetVertexFormat(VERTEX_FORMAT_TEXTURED);
	Gfx_BindDynamicVb(s->vb);
	now = Game.Time;

	if (s->grabsInput) {
		Widget_Render2(&s->chat, 0);
	} else {
		/* Only render recent chat */
		for (i = 0; i < s->chat.lines; i++) 
		{
			texID  = s->chat.textures[i].ID;
			if (!texID) continue;
			logIdx = s->chatIndex + i;

			if (logIdx < 0 || logIdx >= Chat_Log.count) continue;
			/* Only draw chat within last 10 seconds */
			if (Chat_GetLogTime(logIdx) + 10 < now) continue;
			
			Gfx_BindTexture(texID);
			Gfx_DrawVb_IndexedTris_Range(4, i * 4, DRAW_HINT_RECT);
		}
	}

	if (s->grabsInput) {
		Elem_Render(&s->input.base);
		if (s->altText.active) {
			Elem_Render(&s->altText);
		}

#ifdef CC_BUILD_TOUCH
		if (!Gui.TouchUI) return;
		Gfx_3DS_SetRenderScreen(BOTTOM_SCREEN);
		Elem_Render(&s->more);
		Elem_Render(&s->send);
		Elem_Render(&s->cancel);
		Gfx_3DS_SetRenderScreen(TOP_SCREEN);
#endif
	}
}

static void ChatScreen_ContextLost(void* screen) {
	struct ChatScreen* s = (struct ChatScreen*)screen;
	ChatScreen_FreeChatFonts(s);
	Screen_ContextLost(s);

	Elem_Free(&s->chat);
	Elem_Free(&s->input.base);
	Elem_Free(&s->altText);
	Elem_Free(&s->clientStatus);

#ifdef CC_BUILD_TOUCH
	Elem_Free(&s->more);
	Elem_Free(&s->send);
	Elem_Free(&s->cancel);
#endif
}

static void ChatScreen_ContextRecreated(void* screen) {
	struct ChatScreen* s = (struct ChatScreen*)screen;
	struct FontDesc font;
	ChatScreen_ChatUpdateFont(s);
	ChatScreen_Redraw(s);
	Screen_UpdateVb(s);

#ifdef CC_BUILD_TOUCH
	if (!Gui.TouchUI) return;
	Gui_MakeTitleFont(&font);
	ButtonWidget_SetConst(&s->more,   "More",   &font);
	ButtonWidget_SetConst(&s->send,   "Send",   &font);
	ButtonWidget_SetConst(&s->cancel, "Cancel", &font);
	Font_Free(&font);
#endif
}

static int ChatScreen_CalcMaxVertices(void* screen) {
	struct ChatScreen* s = (struct ChatScreen*)screen;
	struct TextGroupWidget* chat = &s->chat;
	/* In case chatlines is 0 */
	return max(4, chat->VTABLE->GetMaxVertices(chat));
}

static void ChatScreen_BuildMesh(void* screen) {
	struct ChatScreen* s = (struct ChatScreen*)screen;
	struct VertexTextured* data;
	struct VertexTextured** ptr;

	data = Screen_LockVb(s);
	ptr  = &data;

	Widget_BuildMesh(&s->chat, ptr);
	Gfx_UnlockDynamicVb(s->vb);
}

static void ChatScreen_Layout(void* screen) {
	struct ChatScreen* s = (struct ChatScreen*)screen;
	if (ChatScreen_ChatUpdateFont(s)) ChatScreen_Redraw(s);

	s->paddingX = Display_ScaleX(5);
	s->paddingY = Display_ScaleY(5);

	Widget_SetLocation(&s->input.base,   ANCHOR_MIN, ANCHOR_MAX,  5, 5);
	Widget_SetLocation(&s->altText,      ANCHOR_MIN, ANCHOR_MAX,  5, 5);
	Widget_SetLocation(&s->chat,         ANCHOR_MIN, ANCHOR_MAX, 10, 0);
	Widget_SetLocation(&s->clientStatus, ANCHOR_MIN, ANCHOR_MAX, 10, 0);
	ChatScreen_UpdateChatYOffsets(s);

#ifdef CC_BUILD_TOUCH
	if (Window_Main.SoftKeyboard == SOFT_KEYBOARD_SHIFT) {
		Widget_SetLocation(&s->send,   ANCHOR_MAX, ANCHOR_MAX, 10,  60);
		Widget_SetLocation(&s->cancel, ANCHOR_MAX, ANCHOR_MAX, 10,  10);
		Widget_SetLocation(&s->more,   ANCHOR_MAX, ANCHOR_MAX, 10, 110);
	} else {
		Widget_SetLocation(&s->send,   ANCHOR_MAX, ANCHOR_MIN, 10,  10);
		Widget_SetLocation(&s->cancel, ANCHOR_MAX, ANCHOR_MIN, 10,  60);
		Widget_SetLocation(&s->more,   ANCHOR_MAX, ANCHOR_MIN, 10, 110);
	}
#endif
}

static int ChatScreen_KeyPress(void* screen, char keyChar) {
	struct ChatScreen* s = (struct ChatScreen*)screen;
	if (!s->grabsInput) return false;

	if (s->suppressNextPress) {
		s->suppressNextPress = false;
		return false;
	}

	InputWidget_Append(&s->input.base, keyChar);
	return true;
}

static int ChatScreen_TextChanged(void* screen, const cc_string* str) {
	struct ChatScreen* s = (struct ChatScreen*)screen;
	if (!s->grabsInput) return false;

	InputWidget_SetText(&s->input.base, str);
	return true;
}

static int ChatScreen_KeyDown(void* screen, int key, struct InputDevice* device) {
	static const cc_string slash = String_FromConst("/");
	struct ChatScreen* s = (struct ChatScreen*)screen;
	int playerListKey    = KeyBind_Mappings[BIND_TABLIST].button1;
	cc_bool handlesList  = playerListKey != CCKEY_TAB || !Gui.TabAutocomplete || !s->grabsInput;

	if (InputBind_Claims(BIND_TABLIST, key, device) && handlesList) {
		if (!tablist_active && !Server.IsSinglePlayer) {
			TabListOverlay_Show(false);
		}
		return true;
	}

	s->suppressNextPress = false;
	/* Handle chat text input */
	if (s->grabsInput) {
#ifdef CC_BUILD_WEB
		/* See reason for this in HandleInputUp */
		if (InputBind_Claims(BIND_SEND_CHAT, key, device) || key == CCKEY_KP_ENTER) {
			ChatScreen_EnterChatInput(s, false);
#else
		if (InputBind_Claims(BIND_SEND_CHAT, key, device) || key == CCKEY_KP_ENTER || key == device->escapeButton) {
			ChatScreen_EnterChatInput(s, key == device->escapeButton);
#endif
		} else if (key == device->pageUpButton) {
			ChatScreen_ScrollChatBy(s, -Gui.Chatlines);
		} else if (key == device->pageDownButton) {
			ChatScreen_ScrollChatBy(s, +Gui.Chatlines);
		} else if (key == CCWHEEL_UP) {
			ChatScreen_ScrollChatBy(s, -1);
		} else if (key == CCWHEEL_DOWN) {
			ChatScreen_ScrollChatBy(s, +1);
		} else {
			Elem_HandlesKeyDown(&s->input.base, key, device);
		}
		return key < CCKEY_F1 || key > CCKEY_F24;
	}

	if (InputBind_Claims(BIND_CHAT, key, device)) {
		ChatScreen_OpenInput(&String_Empty);
	} else if (key == CCKEY_SLASH) {
		ChatScreen_OpenInput(&slash);
	} else if (InputBind_Claims(BIND_INVENTORY, key, device)) {
		/* Creative block menu: always in creative, needs cheats in survival */
		if (Game_SurvivalMode && !Player_CheatsEnabled) return false;
		InventoryScreen_Show();
	} else if (Game_SurvivalMode && (key == 'I' || key == 'E')) {
		SurvivalInventoryScreen_Show();
	} else {
		return false;
	}
	return true;
}

static void ChatScreen_ToggleAltInput(struct ChatScreen* s) {
	SpecialInputWidget_SetActive(&s->altText, !s->altText.active);
	ChatScreen_UpdateChatYOffsets(s);
}

static void ChatScreen_KeyUp(void* screen, int key, struct InputDevice* device) {
	struct ChatScreen* s = (struct ChatScreen*)screen;
	if (!s->grabsInput || (struct Screen*)s != Gui.InputGrab) return;

#ifdef CC_BUILD_WEB
	/* See reason for this in HandleInputUp */
	if (key == CCKEY_ESCAPE) ChatScreen_EnterChatInput(s, true);
#endif

	if (Server.SupportsFullCP437 && InputBind_Claims(BIND_EXT_INPUT, key, device)) {
		if (!Window_Main.Focused) return;
		ChatScreen_ToggleAltInput(s);
	}
}

static int ChatScreen_MouseScroll(void* screen, float delta) {
	struct ChatScreen* s = (struct ChatScreen*)screen;
	return s->grabsInput;
}

static int ChatScreen_PointerDown(void* screen, int id, int x, int y) {
	cc_string text; char textBuffer[STRING_SIZE * 4];
	struct ChatScreen* s = (struct ChatScreen*)screen;
	int height, chatY, i;
	if (Game_HideGui) return false;

	if (!s->grabsInput) {
		if (!Gui_TouchUI) return false;
		String_InitArray(text, textBuffer);

		/* Should be able to click on links with touch */
		i = TextGroupWidget_GetSelected(&s->chat, &text, x, y);
		if (!Utils_IsUrlPrefix(&text)) return false;

		if (Chat_GetLogTime(s->chatIndex + i) + 10 < Game.Time) return false;
		UrlWarningOverlay_Show(&text); return TOUCH_TYPE_GUI;
	}

#ifdef CC_BUILD_TOUCH
	if (Gui.TouchUI) {
		if (Widget_Contains(&s->send, x, y)) {
			ChatScreen_EnterChatInput(s, false); return TOUCH_TYPE_GUI;
		}
		if (Widget_Contains(&s->cancel, x, y)) {
			ChatScreen_EnterChatInput(s, true); return TOUCH_TYPE_GUI;
		}
		if (Widget_Contains(&s->more, x, y)) {
			ChatScreen_ToggleAltInput(s); return TOUCH_TYPE_GUI;
		}
	}
#endif

	if (!Widget_Contains(&s->chat, x, y)) {
		if (s->altText.active && Widget_Contains(&s->altText, x, y)) {
			Elem_HandlesPointerDown(&s->altText, id, x, y);
			ChatScreen_UpdateChatYOffsets(s);
			return TOUCH_TYPE_GUI;
		}
		Elem_HandlesPointerDown(&s->input.base, id, x, y);
		return TOUCH_TYPE_GUI;
	}

	height = TextGroupWidget_UsedHeight(&s->chat);
	chatY  = s->chat.y + s->chat.height - height;
	if (!Gui_Contains(s->chat.x, chatY, s->chat.width, height, x, y)) return false;

	String_InitArray(text, textBuffer);
	TextGroupWidget_GetSelected(&s->chat, &text, x, y);
	if (!text.length) return false;

	if (Utils_IsUrlPrefix(&text) && Process_OpenSupported) {
		UrlWarningOverlay_Show(&text);
	} else if (Gui.ClickableChat) {
		ChatScreen_AppendInput(&text);
	}
	return TOUCH_TYPE_GUI;
}

static void ChatScreen_Init(void* screen) {
	struct ChatScreen* s = (struct ChatScreen*)screen;
	ChatInputWidget_Create(&s->input);
	s->input.base.OnTextChanged = ChatScreen_OnInputTextChanged;
	SpecialInputWidget_Create(&s->altText, &s->chatFont, &s->input.base);

	TextGroupWidget_Create(&s->chat, Gui.Chatlines,
							s->chatTextures, ChatScreen_GetChat);
	TextGroupWidget_Create(&s->clientStatus, CHAT_MAX_CLIENTSTATUS,
							s->clientStatusTextures, ChatScreen_GetClientStatus);

	s->clientStatus.collapsible[0] = true;
	s->clientStatus.collapsible[1] = true;

	s->chat.underlineUrls = !Game_ClassicMode;
	s->chatIndex = Chat_Log.count - Gui.Chatlines;

	Event_Register_(&ChatEvents.ChatReceived,   s, ChatScreen_ChatReceived);
	Event_Register_(&ChatEvents.ColCodeChanged, s, ChatScreen_ColCodeChanged);
	
	s->maxVertices = ChatScreen_CalcMaxVertices(s);
	
	/* For dual screen builds, chat is still rendered on the main game screen */
	s->input.base.flags   |= WIDGET_FLAG_MAINSCREEN;
	s->altText.flags      |= WIDGET_FLAG_MAINSCREEN;
	s->chat.flags         |= WIDGET_FLAG_MAINSCREEN;
	s->clientStatus.flags |= WIDGET_FLAG_MAINSCREEN;

#ifdef CC_BUILD_TOUCH
	ButtonWidget_Init(&s->send,   100, NULL);
	ButtonWidget_Init(&s->cancel, 100, NULL);
	ButtonWidget_Init(&s->more,   100, NULL);
#endif
}

static void ChatScreen_Render(void* screen, float delta) {
	struct ChatScreen* s = (struct ChatScreen*)screen;
	Gfx_3DS_SetRenderScreen(TOP_SCREEN);

	if (s->grabsInput) s->input.base.caretAccumulator += delta;

	if (Game_HideGui && s->grabsInput) {
		Elem_Render(&s->input.base);
	}
	if (!Game_HideGui) {
		if (s->grabsInput && !Gui.ClassicChat) {
			ChatScreen_DrawChatBackground(s);
		}

		ChatScreen_DrawChat(s, delta);
	}
	Gfx_3DS_SetRenderScreen(BOTTOM_SCREEN);
}

static void ChatScreen_Free(void* screen) {
	struct ChatScreen* s = (struct ChatScreen*)screen;
	Event_Unregister_(&ChatEvents.ChatReceived,   s, ChatScreen_ChatReceived);
	Event_Unregister_(&ChatEvents.ColCodeChanged, s, ChatScreen_ColCodeChanged);
}

static const struct ScreenVTABLE ChatScreen_VTABLE = {
	ChatScreen_Init,        ChatScreen_Update, ChatScreen_Free,
	ChatScreen_Render,      ChatScreen_BuildMesh,
	ChatScreen_KeyDown,     ChatScreen_KeyUp,  ChatScreen_KeyPress, ChatScreen_TextChanged,
	ChatScreen_PointerDown, Screen_PointerUp,  Screen_FPointer,     ChatScreen_MouseScroll,
	ChatScreen_Layout, ChatScreen_ContextLost, ChatScreen_ContextRecreated
};
void ChatScreen_Show(void) {
	struct ChatScreen* s  = &ChatScreen_Instance;
	s->lastDownloadStatus = HTTP_PROGRESS_NOT_WORKING_ON;

	s->VTABLE = &ChatScreen_VTABLE;
	Gui_Chat  = s;
	Gui_Add((struct Screen*)s, GUI_PRIORITY_CHAT);
}

void ChatScreen_OpenInput(const cc_string* text) {
	struct ChatScreen* s = &ChatScreen_Instance;
	struct OpenKeyboardArgs args;
	s->suppressNextPress = true;
	s->grabsInput        = true;

	Gui_UpdateInputGrab();
	String_Copy(&s->input.base.text, text);

	OpenKeyboardArgs_Init(&args, text, KEYBOARD_TYPE_TEXT | KEYBOARD_FLAG_SEND);
	args.placeholder = "Enter chat";
	args.multiline   = true;
	args.yOffset     = 30;
	OnscreenKeyboard_Open(&args);

	Widget_SetDisabled(&s->input.base, args.opaque);
	InputWidget_UpdateText(&s->input.base);
}

void ChatScreen_AppendInput(const cc_string* text) {
	struct ChatScreen* s = &ChatScreen_Instance;
	InputWidget_AppendText(&s->input.base, text);
}

void ChatScreen_SetChatlines(int lines) {
	struct ChatScreen* s = &ChatScreen_Instance;
	Elem_Free(&s->chat);
	s->chatIndex += s->chat.lines - lines;
	s->chat.lines = lines;
	TextGroupWidget_RedrawAll(&s->chat);

	s->maxVertices = ChatScreen_CalcMaxVertices(s);
	Screen_UpdateVb(s);
	s->dirty = true;
}


/*########################################################################################################################*
*----------------------------------------------------SpecialTextScreen----------------------------------------------------*
*#########################################################################################################################*/
#ifdef CC_BUILD_NETWORKING
static struct SpecialTextScreen {
	Screen_Body
	int lastDownloadStatus;
	struct FontDesc chatFont, announcementFont, bigAnnouncementFont, smallAnnouncementFont;
	struct TextWidget announcement, bigAnnouncement, smallAnnouncement;
	struct TextGroupWidget status, bottomRight;

	struct Texture statusTextures[CHAT_MAX_STATUS];
	struct Texture bottomRightTextures[CHAT_MAX_BOTTOMRIGHT];
	struct Widget* __widgets[3 + 2];
} SpecialTextScreen_Instance CC_BIG_VAR;

static cc_string SpecialTextScreen_GetStatus(int i)       { return Chat_Status[i]; }
static cc_string SpecialTextScreen_GetBottomRight(int i)  { return Chat_BottomRight[2 - i]; }

static void SpecialTextScreen_FreeChatFonts(struct SpecialTextScreen* s) {
	Font_Free(&s->chatFont);
	Font_Free(&s->announcementFont);
	Font_Free(&s->bigAnnouncementFont);
	Font_Free(&s->smallAnnouncementFont);
}

static cc_bool SpecialTextScreen_ChatUpdateFont(struct SpecialTextScreen* s) {
	int size = (int)(8  * Gui_GetChatScale());
	Math_Clamp(size, 8, 64);

	/* don't recreate font if possible */
	/* TODO: Add function for this, don't use Display_ScaleY (Drawer2D_SameFontSize ??) */
	if (Display_ScaleY(size) == s->chatFont.size) return false;
	SpecialTextScreen_FreeChatFonts(s);
	Font_Make(&s->chatFont, size, FONT_FLAGS_PADDING);

	size = (int)(16 * Gui_GetChatScale());
	Math_Clamp(size, 8, 64);
	Font_Make(&s->announcementFont, size, FONT_FLAGS_NONE);
	size = (int)(24 * Gui_GetChatScale());
	Math_Clamp(size, 8, 64);
	Font_Make(&s->bigAnnouncementFont, size, FONT_FLAGS_NONE);
	size = (int)(8 * Gui_GetChatScale());
	Math_Clamp(size, 8, 64);
	Font_Make(&s->smallAnnouncementFont, size, FONT_FLAGS_NONE);

	TextGroupWidget_SetFont(&s->status,       &s->chatFont);
	TextGroupWidget_SetFont(&s->bottomRight,  &s->chatFont);
	return true;
}

static void SpecialTextScreen_UpdateTexpackStatus(struct SpecialTextScreen* s) {
	int progress = Http_CheckProgress(TexturePack_ReqID);
	cc_string msg; char msgBuffer[STRING_SIZE];
	if (progress == s->lastDownloadStatus) return;

	s->lastDownloadStatus = progress;
	String_InitArray(msg, msgBuffer);

	if (progress == HTTP_PROGRESS_MAKING_REQUEST) {
		String_AppendConst(&msg, "&eRetrieving texture pack..");
	} else if (progress == HTTP_PROGRESS_FETCHING_DATA) {
		String_AppendConst(&msg, "&eDownloading texture pack");
	} else if (progress >= 0 && progress <= 100) {
		String_Format1(&msg, "&eDownloading texture pack (&7%i&e%%)", &progress);
	}
	Chat_AddOf(&msg, MSG_TYPE_EXTRASTATUS_1);
}

static void SpecialTextScreen_ColCodeChanged(void* screen, int code) {
	struct SpecialTextScreen* s = (struct SpecialTextScreen*)screen;
	if (Gfx.LostContext) return;

	TextGroupWidget_RedrawAllWithCol(&s->status,       code);
	TextGroupWidget_RedrawAllWithCol(&s->bottomRight,  code);
}

static void SpecialTextScreen_ChatReceived(void* screen, const cc_string* msg, int type) {
	struct SpecialTextScreen* s = (struct SpecialTextScreen*)screen;
	if (Gfx.LostContext) return;

	if (type >= MSG_TYPE_STATUS_1 && type <= MSG_TYPE_STATUS_3) {
		/* Status[0] is for texture pack downloading message */
		/* Status[1] is for reduced performance mode message */
		TextGroupWidget_Redraw(&s->status, 2 + (type - MSG_TYPE_STATUS_1));
		s->dirty = true;
	} else if (type >= MSG_TYPE_BOTTOMRIGHT_1 && type <= MSG_TYPE_BOTTOMRIGHT_3) {
		/* Bottom3 is top most line, so need to redraw index 0 */
		TextGroupWidget_Redraw(&s->bottomRight, 2 - (type - MSG_TYPE_BOTTOMRIGHT_1));
		s->dirty = true;
	} else if (type == MSG_TYPE_ANNOUNCEMENT) {
		TextWidget_Set(&s->announcement, msg, &s->announcementFont);
		s->dirty = true;
	} else if (type == MSG_TYPE_BIGANNOUNCEMENT) {
		TextWidget_Set(&s->bigAnnouncement, msg, &s->bigAnnouncementFont);
		s->dirty = true;
	} else if (type == MSG_TYPE_SMALLANNOUNCEMENT) {
		TextWidget_Set(&s->smallAnnouncement, msg, &s->smallAnnouncementFont);
		s->dirty = true;
	} else if (type >= MSG_TYPE_EXTRASTATUS_1 && type <= MSG_TYPE_EXTRASTATUS_2) {
		/* Status[0] is for texture pack downloading message */
		/* Status[1] is for reduced performance mode message */
		TextGroupWidget_Redraw(&s->status, type - MSG_TYPE_EXTRASTATUS_1);
		s->dirty = true;
	} 
}

static void SpecialTextScreen_Redraw(struct SpecialTextScreen* s) {
	TextWidget_Set(&s->announcement,      &Chat_Announcement, &s->announcementFont);
	TextWidget_Set(&s->bigAnnouncement,   &Chat_BigAnnouncement, &s->bigAnnouncementFont);
	TextWidget_Set(&s->smallAnnouncement, &Chat_SmallAnnouncement, &s->smallAnnouncementFont);

	TextGroupWidget_RedrawAll(&s->status);
	TextGroupWidget_RedrawAll(&s->bottomRight);
}

static void SpecialTextScreen_ContextLost(void* screen) {
	struct SpecialTextScreen* s = (struct SpecialTextScreen*)screen;
	SpecialTextScreen_FreeChatFonts(s);
	Screen_ContextLost(s);
}

static void SpecialTextScreen_ContextRecreated(void* screen) {
	struct SpecialTextScreen* s = (struct SpecialTextScreen*)screen;

	SpecialTextScreen_ChatUpdateFont(s);
	SpecialTextScreen_Redraw(s);
	Screen_UpdateVb(s);
}

static void SpecialTextScreen_Layout(void* screen) {
	struct SpecialTextScreen* s = (struct SpecialTextScreen*)screen;
	if (SpecialTextScreen_ChatUpdateFont(s)) SpecialTextScreen_Redraw(s);

	Widget_SetLocation(&s->status,      ANCHOR_MAX, ANCHOR_MIN,  0, 0);
	Widget_SetLocation(&s->bottomRight, ANCHOR_MAX, ANCHOR_MAX,  0, 0);

	/* Can't use Widget_SetLocation because it DPI scales input */
	s->bottomRight.yOffset = HUDScreen_LayoutHotbar() + Display_ScaleY(15);
	Widget_Layout(&s->bottomRight);

	Widget_SetLocation(&s->announcement, ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 0);
	s->announcement.yOffset = -Window_UI.Height / 4;
	Widget_Layout(&s->announcement);

	Widget_SetLocation(&s->bigAnnouncement, ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 0);
	s->bigAnnouncement.yOffset = -Window_UI.Height / 16;
	Widget_Layout(&s->bigAnnouncement);

	Widget_SetLocation(&s->smallAnnouncement, ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 0);
	s->smallAnnouncement.yOffset = Window_UI.Height / 20;
	Widget_Layout(&s->smallAnnouncement);
}

static void SpecialTextScreen_Update(void* screen, float delta) {
	struct SpecialTextScreen* s = (struct SpecialTextScreen*)screen;
	SpecialTextScreen_UpdateTexpackStatus(s);

	/* Destroy announcement texture before even rendering it at all, */
	/* otherwise changing texture pack shows announcement for one frame */
	if (s->announcement.tex.ID && (Chat_AnnouncementLeft -= delta) <= 0) {
		Platform_LogConst("DES ANN");
		Elem_Free(&s->announcement);
		s->dirty = true;
	}

	if (s->bigAnnouncement.tex.ID && (Chat_BigAnnouncementLeft -= delta) <= 0) {
		Elem_Free(&s->bigAnnouncement);
		s->dirty = true;
	}

	if (s->smallAnnouncement.tex.ID && (Chat_SmallAnnouncementLeft -= delta) <= 0) {
		Elem_Free(&s->smallAnnouncement);
		s->dirty = true;
	}
}

static void SpecialTextScreen_Render(void* screen, float delta) {
	if (Game_HideGui || Game_PureClassic) return;

	Gfx_3DS_SetRenderScreen(TOP_SCREEN);
	Screen_Render2Widgets(screen, delta);
	Gfx_3DS_SetRenderScreen(BOTTOM_SCREEN);
}

static void SpecialTextScreen_Init(void* screen) {
	struct SpecialTextScreen* s = (struct SpecialTextScreen*)screen;

	s->widgets     = s->__widgets;
	s->numWidgets  = 0;
	s->maxWidgets  = Array_Elems(s->__widgets);

	TextGroupWidget_Add(s, &s->status, CHAT_MAX_STATUS,
							s->statusTextures, SpecialTextScreen_GetStatus);
	TextGroupWidget_Add(s, &s->bottomRight, CHAT_MAX_BOTTOMRIGHT, 
							s->bottomRightTextures, SpecialTextScreen_GetBottomRight);

	TextWidget_Add(s, &s->announcement);
	TextWidget_Add(s, &s->bigAnnouncement);
	TextWidget_Add(s, &s->smallAnnouncement);

	Event_Register_(&ChatEvents.ChatReceived,   s, SpecialTextScreen_ChatReceived);
	Event_Register_(&ChatEvents.ColCodeChanged, s, SpecialTextScreen_ColCodeChanged);

	s->maxVertices = Screen_CalcDefaultMaxVertices(s);

	s->status.collapsible[0] = true; /* Texture pack downloading status */
	s->status.collapsible[1] = true; /* Reduced performance mode status */
	
	/* For dual screen builds, chat is still rendered on the main game screen */
	s->status.flags       |= WIDGET_FLAG_MAINSCREEN;
	s->bottomRight.flags  |= WIDGET_FLAG_MAINSCREEN;

	s->bottomRight.flags       |= WIDGET_FLAG_MAINSCREEN;
	s->announcement.flags      |= WIDGET_FLAG_MAINSCREEN;
	s->bigAnnouncement.flags   |= WIDGET_FLAG_MAINSCREEN;
	s->smallAnnouncement.flags |= WIDGET_FLAG_MAINSCREEN;
}

static void SpecialTextScreen_Free(void* screen) {
	struct SpecialTextScreen* s = (struct SpecialTextScreen*)screen;
	Event_Unregister_(&ChatEvents.ChatReceived,   s, SpecialTextScreen_ChatReceived);
	Event_Unregister_(&ChatEvents.ColCodeChanged, s, SpecialTextScreen_ColCodeChanged);
}

static const struct ScreenVTABLE SpecialTextScreen_VTABLE = {
	SpecialTextScreen_Init,        SpecialTextScreen_Update, SpecialTextScreen_Free,
	SpecialTextScreen_Render,      Screen_BuildMesh,
	Screen_FInput,           Screen_InputUp,    Screen_FKeyPress,   Screen_FText,
	Screen_FPointer,         Screen_PointerUp,  Screen_FPointer,    Screen_FMouseScroll,
	SpecialTextScreen_Layout, SpecialTextScreen_ContextLost, SpecialTextScreen_ContextRecreated
};
void SpecialTextScreen_Show(void) {
	struct SpecialTextScreen* s  = &SpecialTextScreen_Instance;
	s->lastDownloadStatus = HTTP_PROGRESS_NOT_WORKING_ON;

	s->VTABLE = &SpecialTextScreen_VTABLE;
	Gui_Add((struct Screen*)s, GUI_PRIORITY_SPECIALTEXT);
}
#else
void SpecialTextScreen_Show(void) { }
#endif


/*########################################################################################################################*
*-----------------------------------------------------InventoryScreen-----------------------------------------------------*
*#########################################################################################################################*/
static struct InventoryScreen {
	Screen_Body
	struct FontDesc font;
	struct TableWidget table;
	struct TextWidget title;
	cc_bool releasedInv, deferredSelect;
	struct Widget* __widgets[2];
} InventoryScreen CC_BIG_VAR;


static void InventoryScreen_GetTitleText(cc_string* desc, BlockID block) {
	cc_string name;
	int block_ = block;
	if (Game_PureClassic) { String_AppendConst(desc, "Select block"); return; }
	if (block == BLOCK_AIR) return;

	name = Block_UNSAFE_GetName(block);
	String_AppendString(desc, &name);
	if (Game_ClassicMode) return;

	String_Format1(desc, " (ID %i&f", &block_);
	if (!Blocks.CanPlace[block])  { String_AppendConst(desc,  ", place &cNo&f"); }
	if (!Blocks.CanDelete[block]) { String_AppendConst(desc, ", delete &cNo&f"); }
	String_Append(desc, ')');
}

static void InventoryScreen_UpdateTitle(struct InventoryScreen* s, BlockID block) {
	cc_string desc; char descBuffer[STRING_SIZE * 2];

	String_InitArray(desc, descBuffer);
	InventoryScreen_GetTitleText(&desc, block);
	TextWidget_Set(&s->title, &desc, &s->font);
	s->dirty = true;
}

static void InventoryScreen_OnUpdateTitle(BlockID block) {
	InventoryScreen_UpdateTitle(&InventoryScreen, block);
}


static void InventoryScreen_OnBlockChanged(void* screen) {
	struct InventoryScreen* s = (struct InventoryScreen*)screen;
	TableWidget_OnInventoryChanged(&s->table);
}

static void InventoryScreen_NeedRedrawing(void* screen) {
	struct InventoryScreen* s = (struct InventoryScreen*)screen;
	s->dirty = true;
}

static void InventoryScreen_ContextLost(void* screen) {
	struct InventoryScreen* s = (struct InventoryScreen*)screen;
	Font_Free(&s->font);
	Screen_ContextLost(s);
	s->table.vb = 0;
}

static void InventoryScreen_ContextRecreated(void* screen) {
	struct InventoryScreen* s = (struct InventoryScreen*)screen;
	Screen_UpdateVb(s);
	s->table.vb = s->vb;

	Gui_MakeBodyFont(&s->font);
	TableWidget_RecreateTitle(&s->table, true);
}

static void InventoryScreen_MoveToSelected(struct InventoryScreen* s) {
	struct TableWidget* table = &s->table;
	int blockForTitle;
	s->deferredSelect = false;

	if (Game_ClassicMode) {
		/* Accuracy: Original classic preserves selected block across inventory menu opens */
		TableWidget_SetToIndex(table, table->selectedIndex);
		TableWidget_RecreateTitle(table, true);
	} else {
		blockForTitle = -1;
		TableWidget_SetToBlock(table, Inventory_SelectedBlock, false);
		/* When using auto rotate, if the held block is hidden, try to find another one in its autorotate group */
		if (AutoRotate_Enabled && table->selectedIndex == -1) {
			TableWidget_SetToBlock(table, Inventory_SelectedBlock, true);
			/* We still need to be able to see the name and ID of the held block */
			/* rather than the one that the cursor snapped to */
			blockForTitle = Inventory_SelectedBlock;
		}

		if (table->selectedIndex == -1) {
			/* Hidden block in inventory - display title for it still */
			InventoryScreen_OnUpdateTitle(Inventory_SelectedBlock);
		} else {
			TableWidget_RecreateTitleForBlock(table, true, blockForTitle);
		}
	}
}

static void InventoryScreen_Init(void* screen) {
	struct InventoryScreen* s = (struct InventoryScreen*)screen;
	s->widgets     = s->__widgets;
	s->numWidgets  = 0;
	s->maxWidgets  = Array_Elems(s->__widgets);
	
	TextWidget_Add(s,  &s->title);
	TableWidget_Add(s, &s->table, 22 * Options_GetFloat(OPT_INV_SCROLLBAR_SCALE, 0, 10, 1));
	s->table.blocksPerRow = Inventory.BlocksPerRow;
	s->table.UpdateTitle   = InventoryScreen_OnUpdateTitle;
	TableWidget_RecreateBlocks(&s->table);

	/* Can't immediately move to selected here, because cursor grabbed  */
	/*  status might be toggled *after* InventoryScreen_Init() is called */
	/* That causes the cursor to be moved back to the middle of the window */
	s->deferredSelect = true;

	Event_Register_(&TextureEvents.AtlasChanged,     s, InventoryScreen_NeedRedrawing);
	Event_Register_(&BlockEvents.PermissionsChanged, s, InventoryScreen_OnBlockChanged);
	Event_Register_(&BlockEvents.BlockDefChanged,    s, InventoryScreen_OnBlockChanged);

	s->maxVertices = Screen_CalcDefaultMaxVertices(s);
}

static void InventoryScreen_Free(void* screen) {
	struct InventoryScreen* s = (struct InventoryScreen*)screen;

	Event_Unregister_(&TextureEvents.AtlasChanged,     s, InventoryScreen_NeedRedrawing);
	Event_Unregister_(&BlockEvents.PermissionsChanged, s, InventoryScreen_OnBlockChanged);
	Event_Unregister_(&BlockEvents.BlockDefChanged,    s, InventoryScreen_OnBlockChanged);
}

static void InventoryScreen_Update(void* screen, float delta) {
	struct InventoryScreen* s = (struct InventoryScreen*)screen;
	if (s->deferredSelect) InventoryScreen_MoveToSelected(s);
}

static void InventoryScreen_Render(void* screen, float delta) {
	struct InventoryScreen* s = (struct InventoryScreen*)screen;
	Widget_Render2(&s->table, TEXTWIDGET_MAX);
	Widget_Render2(&s->title,              0);
}

static void InventoryScreen_Layout(void* screen) {
	struct InventoryScreen* s = (struct InventoryScreen*)screen;
	s->table.scale = Gui_GetInventoryScale();
	Widget_SetLocation(&s->table, ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 0);

	Widget_SetLocation(&s->title, ANCHOR_CENTRE, ANCHOR_MIN, 0, 0);
	/* use Table(Y) directly instead of s->title->height ??? */
	s->title.yOffset = s->table.y - s->title.height - 3;
	Widget_Layout(&s->title); /* Needed for yOffset */
}

static int InventoryScreen_KeyDown(void* screen, int key, struct InputDevice* device) {
	struct InventoryScreen* s = (struct InventoryScreen*)screen;
	struct TableWidget* table = &s->table;

	/* Accuracy: Original classic doesn't close inventory menu when B is pressed */
	if (InputBind_Claims(BIND_INVENTORY, key, device) && s->releasedInv && !Game_ClassicMode) {
		Gui_Remove((struct Screen*)s);
		CPE_SendNotifyAction(NOTIFY_ACTION_BLOCK_LIST_TOGGLED, 0);
	} else if (InputDevice_IsEnter(key, device) && table->selectedIndex != -1) {
		Inventory_SetSelectedBlock(table->blocks[table->selectedIndex]);
		Gui_Remove((struct Screen*)s);
		CPE_SendNotifyAction(NOTIFY_ACTION_BLOCK_LIST_TOGGLED, 0);
	} else if (Elem_HandlesKeyDown(table, key, device)) {
	} else {
		return Elem_HandlesKeyDown(&HUDScreen_Instance.hotbar, key, device);
	}
	return true;
}

static cc_bool InventoryScreen_IsHotbarActive(void) {
	struct Screen* grabbed = Gui.InputGrab;
	/* Only toggle hotbar when inventory or no grab screen is open */
	return !grabbed || grabbed == (struct Screen*)&InventoryScreen;
}

static void InventoryScreen_KeyUp(void* screen, int key, struct InputDevice* device) {
	struct InventoryScreen* s = (struct InventoryScreen*)screen;
	if (InputBind_Claims(BIND_INVENTORY, key, device)) s->releasedInv = true;
}

static int InventoryScreen_PointerDown(void* screen, int id, int x, int y) {
	struct InventoryScreen* s = (struct InventoryScreen*)screen;
	struct TableWidget* table = &s->table;
	cc_bool handled, hotbar;

	if (table->scroll.draggingId == id) return TOUCH_TYPE_GUI;
	if (HUDscreen_PointerDown(Gui_HUD, id, x, y)) return TOUCH_TYPE_GUI;
	handled = Elem_HandlesPointerDown(table, id, x, y);

	if (!handled || table->pendingClose) {
		hotbar = Input_IsCtrlPressed() || Input_IsShiftPressed();
		if (!hotbar) {
			Gui_Remove((struct Screen*)s);
			CPE_SendNotifyAction(NOTIFY_ACTION_BLOCK_LIST_TOGGLED, 0);
		}
	}
	return TOUCH_TYPE_GUI;
}

static void InventoryScreen_PointerUp(void* screen, int id, int x, int y) {
	struct InventoryScreen* s = (struct InventoryScreen*)screen;
	Elem_OnPointerUp(&s->table, id, x, y);
}

static int InventoryScreen_PointerMove(void* screen, int id, int x, int y) {
	struct InventoryScreen* s = (struct InventoryScreen*)screen;
	return Elem_HandlesPointerMove(&s->table, id, x, y);
}

static int InventoryScreen_MouseScroll(void* screen, float delta) {
	struct InventoryScreen* s = (struct InventoryScreen*)screen;

	cc_bool hotbar = Input_IsAltPressed() || Input_IsCtrlPressed() || Input_IsShiftPressed();
	if (hotbar) return false;
	return Elem_HandlesMouseScroll(&s->table, delta);
}

static int InventoryScreen_PadAxis(void* screen, struct PadAxisUpdate* upd) {
	struct InventoryScreen* s = (struct InventoryScreen*)screen;

	return Elem_HandlesPadAxis(&s->table, upd);
}

static const struct ScreenVTABLE InventoryScreen_VTABLE = {
	InventoryScreen_Init,        InventoryScreen_Update,    InventoryScreen_Free,
	InventoryScreen_Render,      Screen_BuildMesh,
	InventoryScreen_KeyDown,     InventoryScreen_KeyUp,     Screen_TKeyPress,            Screen_TText,
	InventoryScreen_PointerDown, InventoryScreen_PointerUp, InventoryScreen_PointerMove, InventoryScreen_MouseScroll,
	InventoryScreen_Layout,  InventoryScreen_ContextLost, InventoryScreen_ContextRecreated,
	InventoryScreen_PadAxis
};
void InventoryScreen_Show(void) {
	struct InventoryScreen* s = &InventoryScreen;
	s->grabsInput = true;
	s->closable   = true;

	s->VTABLE = &InventoryScreen_VTABLE;
	Gui_Add((struct Screen*)s, GUI_PRIORITY_INVENTORY);
	CPE_SendNotifyAction(NOTIFY_ACTION_BLOCK_LIST_TOGGLED, 1);
}

void InventoryScreen_Hide(void) {
	struct InventoryScreen* s = &InventoryScreen;
	Gui_Remove((struct Screen*)s);
	CPE_SendNotifyAction(NOTIFY_ACTION_BLOCK_LIST_TOGGLED, 0);
}


/*########################################################################################################################*
*---------------------------------------------------ItemInventoryScreen---------------------------------------------------*
*#########################################################################################################################*/
static struct ItemInventoryScreen {
	Screen_Body
	struct FontDesc font;
	struct ItemTableWidget table;
	struct TextWidget title;
	struct Widget* __widgets[2];
} ItemInventoryScreen CC_BIG_VAR;

static void ItemInventoryScreen_GetTitleText(cc_string* desc, int itemId) {
	if (itemId == ITEM_NONE || itemId <= 0 || itemId >= ITEM_COUNT) return;
	String_AppendConst(desc, ItemNames[itemId]);
}

static void ItemInventoryScreen_UpdateTitle(struct ItemInventoryScreen* s, int itemId) {
	cc_string desc; char descBuffer[STRING_SIZE * 2];

	String_InitArray(desc, descBuffer);
	ItemInventoryScreen_GetTitleText(&desc, itemId);
	TextWidget_Set(&s->title, &desc, &s->font);
	s->dirty = true;
}

static void ItemInventoryScreen_OnUpdateTitle(int itemId) {
	ItemInventoryScreen_UpdateTitle(&ItemInventoryScreen, itemId);
}

static void ItemInventoryScreen_ContextLost(void* screen) {
	struct ItemInventoryScreen* s = (struct ItemInventoryScreen*)screen;
	Font_Free(&s->font);
	Screen_ContextLost(s);
	s->table.vb = 0;
}

static void ItemInventoryScreen_ContextRecreated(void* screen) {
	struct ItemInventoryScreen* s = (struct ItemInventoryScreen*)screen;
	Screen_UpdateVb(s);
	s->table.vb = s->vb;

	Gui_MakeBodyFont(&s->font);
	ItemTableWidget_RecreateTitle(&s->table, true);
}

static void ItemInventoryScreen_Init(void* screen) {
	struct ItemInventoryScreen* s = (struct ItemInventoryScreen*)screen;
	s->widgets     = s->__widgets;
	s->numWidgets  = 0;
	s->maxWidgets  = Array_Elems(s->__widgets);

	TextWidget_Add(s,      &s->title);
	ItemTableWidget_Add(s, &s->table, 22);
	s->table.itemsPerRow  = 9;
	s->table.UpdateTitle  = ItemInventoryScreen_OnUpdateTitle;
	ItemTableWidget_RecreateItems(&s->table);

	s->maxVertices = Screen_CalcDefaultMaxVertices(s);
}

static void ItemInventoryScreen_Free(void* screen) {
}

static void ItemInventoryScreen_Update(void* screen, float delta) {
}

static void ItemInventoryScreen_Render(void* screen, float delta) {
	struct ItemInventoryScreen* s = (struct ItemInventoryScreen*)screen;
	Widget_Render2(&s->table, TEXTWIDGET_MAX);
	Widget_Render2(&s->title,              0);
}

static void ItemInventoryScreen_Layout(void* screen) {
	struct ItemInventoryScreen* s = (struct ItemInventoryScreen*)screen;
	s->table.scale = Gui_GetInventoryScale();
	Widget_SetLocation(&s->table, ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 0);

	Widget_SetLocation(&s->title, ANCHOR_CENTRE, ANCHOR_MIN, 0, 0);
	s->title.yOffset = s->table.y - s->title.height - 3;
	Widget_Layout(&s->title);
}

static int ItemInventoryScreen_KeyDown(void* screen, int key, struct InputDevice* device) {
	struct ItemInventoryScreen* s = (struct ItemInventoryScreen*)screen;
	struct ItemTableWidget* table = &s->table;

	if (InputDevice_IsEnter(key, device) && table->selectedIndex != -1) {
		/* Give the selected item to the player's current hotbar slot */
		Hotbar_SetItem(Inventory.SelectedIndex, table->items[table->selectedIndex]);
		Inventory_Set(Inventory.SelectedIndex, BLOCK_AIR);
		Event_RaiseVoid(&UserEvents.HeldBlockChanged);
		Gui_Remove((struct Screen*)s);
	} else if (Elem_HandlesKeyDown(table, key, device)) {
	} else {
		return Elem_HandlesKeyDown(&HUDScreen_Instance.hotbar, key, device);
	}
	return true;
}

static void ItemInventoryScreen_KeyUp(void* screen, int key, struct InputDevice* device) {
}

static int ItemInventoryScreen_PointerDown(void* screen, int id, int x, int y) {
	struct ItemInventoryScreen* s = (struct ItemInventoryScreen*)screen;
	struct ItemTableWidget* table = &s->table;
	cc_bool handled;

	if (table->scroll.draggingId == id) return TOUCH_TYPE_GUI;
	if (HUDscreen_PointerDown(Gui_HUD, id, x, y)) return TOUCH_TYPE_GUI;
	handled = Elem_HandlesPointerDown(table, id, x, y);

	if (!handled || table->pendingClose) {
		if (table->pendingClose && table->selectedIndex != -1) {
			/* Give item to hotbar */
			Hotbar_SetItem(Inventory.SelectedIndex, table->items[table->selectedIndex]);
			Inventory_Set(Inventory.SelectedIndex, BLOCK_AIR);
			Event_RaiseVoid(&UserEvents.HeldBlockChanged);
		}
		Gui_Remove((struct Screen*)s);
	}
	return TOUCH_TYPE_GUI;
}

static void ItemInventoryScreen_PointerUp(void* screen, int id, int x, int y) {
	struct ItemInventoryScreen* s = (struct ItemInventoryScreen*)screen;
	Elem_OnPointerUp(&s->table, id, x, y);
}

static int ItemInventoryScreen_PointerMove(void* screen, int id, int x, int y) {
	struct ItemInventoryScreen* s = (struct ItemInventoryScreen*)screen;
	return Elem_HandlesPointerMove(&s->table, id, x, y);
}

static int ItemInventoryScreen_MouseScroll(void* screen, float delta) {
	struct ItemInventoryScreen* s = (struct ItemInventoryScreen*)screen;
	return Elem_HandlesMouseScroll(&s->table, delta);
}

static int ItemInventoryScreen_PadAxis(void* screen, struct PadAxisUpdate* upd) {
	struct ItemInventoryScreen* s = (struct ItemInventoryScreen*)screen;
	return Elem_HandlesPadAxis(&s->table, upd);
}

static const struct ScreenVTABLE ItemInventoryScreen_VTABLE = {
	ItemInventoryScreen_Init,        ItemInventoryScreen_Update,    ItemInventoryScreen_Free,
	ItemInventoryScreen_Render,      Screen_BuildMesh,
	ItemInventoryScreen_KeyDown,     ItemInventoryScreen_KeyUp,     Screen_TKeyPress,            Screen_TText,
	ItemInventoryScreen_PointerDown, ItemInventoryScreen_PointerUp, ItemInventoryScreen_PointerMove, ItemInventoryScreen_MouseScroll,
	ItemInventoryScreen_Layout,  ItemInventoryScreen_ContextLost, ItemInventoryScreen_ContextRecreated,
	ItemInventoryScreen_PadAxis
};
void ItemInventoryScreen_Show(void) {
	struct ItemInventoryScreen* s = &ItemInventoryScreen;
	s->grabsInput = true;
	s->closable   = true;

	s->VTABLE = &ItemInventoryScreen_VTABLE;
	Gui_Add((struct Screen*)s, GUI_PRIORITY_INVENTORY);
}


/*########################################################################################################################*
*---------------------------------------------------SurvGUI Slot Textures-------------------------------------------------*
*#########################################################################################################################*/
/* Slot texture (slot.png, 18x18), armor overlay textures, and border texture loaded from the texture pack */
static GfxResourceID SurvGUI_SlotTex;
static GfxResourceID SurvGUI_ArmorHelmetTex;
static GfxResourceID SurvGUI_ArmorChestTex;
static GfxResourceID SurvGUI_ArmorPantsTex;
static GfxResourceID SurvGUI_ArmorBootsTex;
static GfxResourceID SurvGUI_BorderTex;

static void SlotPngProcess(struct Stream* stream, const cc_string* name) {
	Game_UpdateTexture(&SurvGUI_SlotTex, stream, name, NULL, NULL);
}
static struct TextureEntry slot_entry = { "slot.png", SlotPngProcess };

static void SlotHelmetPngProcess(struct Stream* stream, const cc_string* name) {
	Game_UpdateTexture(&SurvGUI_ArmorHelmetTex, stream, name, NULL, NULL);
}
static struct TextureEntry slot_helmet_entry = { "slot_helmet.png", SlotHelmetPngProcess };

static void SlotChestPngProcess(struct Stream* stream, const cc_string* name) {
	Game_UpdateTexture(&SurvGUI_ArmorChestTex, stream, name, NULL, NULL);
}
static struct TextureEntry slot_chest_entry = { "slot_chest.png", SlotChestPngProcess };

static void SlotPantsPngProcess(struct Stream* stream, const cc_string* name) {
	Game_UpdateTexture(&SurvGUI_ArmorPantsTex, stream, name, NULL, NULL);
}
static struct TextureEntry slot_pants_entry = { "slot_pants.png", SlotPantsPngProcess };

static void SlotBootsPngProcess(struct Stream* stream, const cc_string* name) {
	Game_UpdateTexture(&SurvGUI_ArmorBootsTex, stream, name, NULL, NULL);
}
static struct TextureEntry slot_boots_entry = { "slot_boots.png", SlotBootsPngProcess };

static void GuiBorderPngProcess(struct Stream* stream, const cc_string* name) {
	Game_UpdateTexture(&SurvGUI_BorderTex, stream, name, NULL, NULL);
}
static struct TextureEntry guiborder_entry = { "guiborder.png", GuiBorderPngProcess };

static GfxResourceID SurvGUI_ArrowEmptyTex;
static GfxResourceID SurvGUI_ArrowFullTex;

static void ArrowEmptyPngProcess(struct Stream* stream, const cc_string* name) {
	Game_UpdateTexture(&SurvGUI_ArrowEmptyTex, stream, name, NULL, NULL);
}
static struct TextureEntry arrowempty_entry = { "arrowui_empty.png", ArrowEmptyPngProcess };

static void ArrowFullPngProcess(struct Stream* stream, const cc_string* name) {
	Game_UpdateTexture(&SurvGUI_ArrowFullTex, stream, name, NULL, NULL);
}
static struct TextureEntry arrowfull_entry = { "arrowui_full.png", ArrowFullPngProcess };

static GfxResourceID SurvGUI_FlameLitTex;
static GfxResourceID SurvGUI_FlameUnlitTex;

static void FlameLitPngProcess(struct Stream* stream, const cc_string* name) {
	Game_UpdateTexture(&SurvGUI_FlameLitTex, stream, name, NULL, NULL);
}
static struct TextureEntry flamelit_entry = { "furnace_flame_lit.png", FlameLitPngProcess };

static void FlameUnlitPngProcess(struct Stream* stream, const cc_string* name) {
	Game_UpdateTexture(&SurvGUI_FlameUnlitTex, stream, name, NULL, NULL);
}
static struct TextureEntry flameunlit_entry = { "furnace_flame_unlit.png", FlameUnlitPngProcess };

void SurvGUI_RegisterTextures(void) {
	TextureEntry_Register(&slot_entry);
	TextureEntry_Register(&slot_helmet_entry);
	TextureEntry_Register(&slot_chest_entry);
	TextureEntry_Register(&slot_pants_entry);
	TextureEntry_Register(&slot_boots_entry);
	TextureEntry_Register(&guiborder_entry);
	TextureEntry_Register(&arrowempty_entry);
	TextureEntry_Register(&arrowfull_entry);
	TextureEntry_Register(&flamelit_entry);
	TextureEntry_Register(&flameunlit_entry);
}

void SurvGUI_DeleteTextures(void) {
	Gfx_DeleteTexture(&SurvGUI_SlotTex);
	Gfx_DeleteTexture(&SurvGUI_ArmorHelmetTex);
	Gfx_DeleteTexture(&SurvGUI_ArmorChestTex);
	Gfx_DeleteTexture(&SurvGUI_ArmorPantsTex);
	Gfx_DeleteTexture(&SurvGUI_ArmorBootsTex);
	Gfx_DeleteTexture(&SurvGUI_BorderTex);
	Gfx_DeleteTexture(&SurvGUI_ArrowEmptyTex);
	Gfx_DeleteTexture(&SurvGUI_ArrowFullTex);
	Gfx_DeleteTexture(&SurvGUI_FlameLitTex);
	Gfx_DeleteTexture(&SurvGUI_FlameUnlitTex);
}

/* Draws a single inventory slot at (x,y) with size cs using slot.png tinted #c6c6c6.
   Falls back to a flat dark quad if slot.png is not loaded. */
static void SurvGUI_DrawSlot(int x, int y, int cs, cc_bool hovered) {
	PackedCol slotCol  = PackedCol_Make(0xc6, 0xc6, 0xc6, 255);
	PackedCol hoverCol = PackedCol_Make(255, 255, 255, 80);
	struct Texture tex;

	if (SurvGUI_SlotTex) {
		/* slot.png is 32x32 but only the top-left 18x18 pixels are used */
		#define SLOT_UV (18.0f / 32.0f)
		Tex_SetRect(tex, x, y, cs, cs);
		Tex_SetUV(tex, 0.0f, 0.0f, SLOT_UV, SLOT_UV);
		tex.ID = SurvGUI_SlotTex;
		Texture_RenderShaded(&tex, slotCol);
		#undef SLOT_UV
	} else {
		Gfx_Draw2DFlat(x + 1, y + 1, cs - 2, cs - 2, PackedCol_Make(0xc6, 0xc6, 0xc6, 255));
	}

	if (hovered) {
		Gfx_Draw2DFlat(x + 1, y + 1, cs - 2, cs - 2, hoverCol);
	}
}

/*
 * guiborder.png layout (32 wide x 4 tall, 8 tiles of 4x4 each, 1-indexed from left):
 *   tile 1 = top-right corner
 *   tile 2 = bottom-right corner
 *   tile 3 = bottom-left corner
 *   tile 4 = top-left corner
 *   tile 5 = top side
 *   tile 6 = right side
 *   tile 7 = bottom side
 *   tile 8 = left side
 *
 * UV for tile N in a 32x4 texture: u = (N-1)/8 .. N/8, v = 0..1
 */

/* Draws one tile (or partial tile) from guiborder.png at screen rect (x,y,w,h).
   wFrac/hFrac are UV fractions (1.0 = full tile) used to clip partial last tiles. */
static void SurvGUI_DrawBorderTile(int x, int y, int w, int h, int tileN, float wFrac, float hFrac) {
	struct Texture tex;
	float u1 = (tileN - 1) / 8.0f;
	if (!SurvGUI_BorderTex || w <= 0 || h <= 0) return;
	Tex_SetRect(tex, x, y, w, h);
	Tex_SetUV(tex, u1, 0.0f, u1 + wFrac / 8.0f, hFrac);
	tex.ID = SurvGUI_BorderTex;
	Texture_Render(&tex);
}

/* Tiles tileN horizontally from (x,y) across len pixels, each stamp b×b (last may be clipped). */
static void SurvGUI_BorderHRun(int x, int y, int len, int b, int tileN) {
	int end = x + len;
	while (x < end) {
		int w = (x + b <= end) ? b : end - x;
		SurvGUI_DrawBorderTile(x, y, w, b, tileN, (float)w / b, 1.0f);
		x += b;
	}
}

/* Tiles tileN vertically from (x,y) across len pixels, each stamp b×b (last may be clipped). */
static void SurvGUI_BorderVRun(int x, int y, int len, int b, int tileN) {
	int end = y + len;
	while (y < end) {
		int h = (y + b <= end) ? b : end - y;
		SurvGUI_DrawBorderTile(x, y, b, h, tileN, 1.0f, (float)h / b);
		y += b;
	}
}

/* Draws the border frame around a GUI panel at (px,py) with size (pw,ph).
   b = border thickness in screen pixels.
   The border is drawn *outside* the panel rect so the corners overlap the edge. */
static void SurvGUI_DrawBorder(int px, int py, int pw, int ph, int b) {
	/* Outer rect that the border occupies */
	int ox = px - b, oy = py - b;
	int ow = pw + 2*b, oh = ph + 2*b;
	if (!SurvGUI_BorderTex) return;
	/* Corners — always exactly b×b */
	SurvGUI_DrawBorderTile(ox,          oy,          b, b, 4, 1.0f, 1.0f); /* top-left     */
	SurvGUI_DrawBorderTile(ox + ow - b, oy,          b, b, 1, 1.0f, 1.0f); /* top-right    */
	SurvGUI_DrawBorderTile(ox,          oy + oh - b, b, b, 3, 1.0f, 1.0f); /* bottom-left  */
	SurvGUI_DrawBorderTile(ox + ow - b, oy + oh - b, b, b, 2, 1.0f, 1.0f); /* bottom-right */
	/* Sides — tiled in b×b stamps between the corners */
	SurvGUI_BorderHRun(ox + b, oy,          ow - 2*b, b, 5); /* top    */
	SurvGUI_BorderHRun(ox + b, oy + oh - b, ow - 2*b, b, 7); /* bottom */
	SurvGUI_BorderVRun(ox,          oy + b, oh - 2*b, b, 8); /* left   */
	SurvGUI_BorderVRun(ox + ow - b, oy + b, oh - 2*b, b, 6); /* right  */
}

/* Arrow UV: only the left-most 22 of 32 pixels are used -> u2 = 22/32 = 0.6875 */
#define ARROW_UV_U2 (22.0f / 32.0f)
/* Aspect ratio of visible region: 22 wide x 16 tall */
#define ARROW_ASPECT (22.0f / 16.0f)

/* Draws the furnace flame indicator at (x,y) with size cs.
   furnace_flame_unlit.png is the background; furnace_flame_lit.png fills from the
   bottom up based on fuelPct (1.0 = fully lit, 0.0 = fully unlit).
   Both textures are 32x32 but only the top-left 18x18 region is used. */
#define FLAME_UV (18.0f / 32.0f)
static void SurvGUI_DrawFlame(int x, int y, int cs, float fuelPct) {
	struct Texture tex;

	/* Unlit background — always drawn */
	if (SurvGUI_FlameUnlitTex) {
		Tex_SetRect(tex, x, y, cs, cs);
		Tex_SetUV(tex, 0.0f, 0.0f, FLAME_UV, FLAME_UV);
		tex.ID = SurvGUI_FlameUnlitTex;
		Texture_Render(&tex);
	}

	/* Lit overlay — fills from the bottom up, shrinks from the top as fuel depletes */
	if (SurvGUI_FlameLitTex && fuelPct > 0.0f) {
		int litCols, litH;
		float litV1;
		if (fuelPct > 1.0f) fuelPct = 1.0f;
		litCols = (int)(18.0f * fuelPct);
		if (litCols < 1) litCols = 1;
		litH  = (int)(cs * litCols / 18.0f);
		if (litH < 1) litH = 1;
		litV1 = (18 - litCols) / 32.0f;
		Tex_SetRect(tex, x, y + cs - litH, cs, litH);
		Tex_SetUV(tex, 0.0f, litV1, FLAME_UV, FLAME_UV);
		tex.ID = SurvGUI_FlameLitTex;
		Texture_Render(&tex);
	}
}
#undef FLAME_UV

/* Draws the crafting/smelting arrow at (x,y) with given width.
   Height is derived from the aspect ratio.
   progress = 0.0 means fully empty, 1.0 means fully filled. */
static void SurvGUI_DrawArrow(int x, int y, int w, float progress) {
	struct Texture tex;
	int h = (int)(w / ARROW_ASPECT);

	/* Empty arrow background */
	if (SurvGUI_ArrowEmptyTex) {
		Tex_SetRect(tex, x, y, w, h);
		Tex_SetUV(tex, 0.0f, 0.0f, ARROW_UV_U2, 1.0f);
		tex.ID = SurvGUI_ArrowEmptyTex;
		Texture_Render(&tex);
	}

	/* Filled overlay (snapped to texture pixel columns) */
	if (SurvGUI_ArrowFullTex && progress > 0.0f) {
		int filledCols, fillW;
		float fillU2;
		if (progress > 1.0f) progress = 1.0f;
		filledCols = (int)(22.0f * progress);
		if (filledCols < 1) filledCols = 1;
		fillU2 = filledCols / 32.0f;
		fillW  = (int)(w * filledCols / 22.0f);
		if (fillW < 1) fillW = 1;
		Tex_SetRect(tex, x, y, fillW, h);
		Tex_SetUV(tex, 0.0f, 0.0f, fillU2, 1.0f);
		tex.ID = SurvGUI_ArrowFullTex;
		Texture_Render(&tex);
	}
}

/* Draws the armor overlay icon for the given armor slot (0=helmet,1=chest,2=pants,3=boots)
   at position (x,y) with size cs. Rendered at the same visual size as items (cs*0.76),
   centered in the slot. Only drawn when the slot is empty. */
static void SurvGUI_DrawArmorOverlay(int armorIdx, int x, int y, int cs) {
	static GfxResourceID* armorTexPtrs[4] = {
		&SurvGUI_ArmorHelmetTex,
		&SurvGUI_ArmorChestTex,
		&SurvGUI_ArmorPantsTex,
		&SurvGUI_ArmorBootsTex
	};
	struct Texture tex;
	GfxResourceID id;
	int size, ox, oy;

	if (armorIdx < 0 || armorIdx > 3) return;
	id = *armorTexPtrs[armorIdx];
	if (!id) return;

	/* Match item render size: isoSize = cs * 0.38f, item spans isoSize*2 */
	size = (int)(cs * 0.38f * 2.0f);
	ox   = x + (cs - size) / 2;
	oy   = y + (cs - size) / 2;

	Tex_SetRect(tex, ox, oy, size, size);
	Tex_SetUV(tex, 0.0f, 0.0f, 1.0f, 1.0f);
	tex.ID = id;
	Texture_Render(&tex);
}

/*########################################################################################################################*
*--------------------------------------------------SurvivalInventoryScreen------------------------------------------------*
*#########################################################################################################################*/
/* Slot layout (45 total):
 *  0-26:  Main inventory (3x9)  -> SurvInv_Main[idx]
 *  27-35: Hotbar (9)            -> Inventory_Get/Set, Hotbar_GetItem/SetItem
 *  36-39: Armor (4)             -> SurvInv_Armor[idx-36]
 *  40-43: Crafting 2x2          -> SurvInv_Craft[idx-40]
 *  44:    Output                -> SurvInv_Output
 */
#define SURVINV_SLOT_COUNT 45
#define SURVINV_ISO_VERTICES ((SURVINV_SLOT_COUNT + 1) * ISOMETRICDRAWER_MAXVERTICES)

static struct SurvivalInventoryScreen {
	Screen_Body
	struct FontDesc font;
	struct TextWidget title;

	int cellSize;
	int gridX, gridY;      /* 3x9 main grid top-left */
	int hotbarY;           /* hotbar row Y */
	int armorX, armorY;    /* armor slots top-left */
	int craftX, craftY;    /* 2x2 crafting top-left */
	int outputX, outputY;  /* output slot position */

	cc_bool  holding;
	BlockID  holdBlock;
	int      holdItem;
	int      holdCount;

	int      holdDurability; /* remaining durability of the cursor-held item */
	int hoveredSlot;       /* -1 = none */

	int isoState[SURVINV_ISO_VERTICES / 4];
	int verticesCount;

	cc_bool releasedKey;
	struct Widget* __widgets[1];
} SurvivalInventoryScreen_Instance CC_BIG_VAR;

static cc_bool inventoryScreenOpen;

static cc_bool SurvInv_IsScreenOpen(void) {
	struct Screen* grabbed = Gui.InputGrab;
	if (grabbed == (struct Screen*)&SurvivalInventoryScreen_Instance) return true;
	return inventoryScreenOpen;
}

static void SurvInv_GetSlot(int idx, BlockID* block, int* itemId) {
	if (idx < 27) {
		*block  = SurvInv_Main[idx].block;
		*itemId = SurvInv_Main[idx].itemId;
	} else if (idx < 36) {
		*block  = Inventory_Get(idx - 27);
		*itemId = Hotbar_GetItem(idx - 27);
	} else if (idx < 40) {
		*block  = SurvInv_Armor[idx - 36].block;
		*itemId = SurvInv_Armor[idx - 36].itemId;
	} else if (idx < 44) {
		*block  = SurvInv_Craft[idx - 40].block;
		*itemId = SurvInv_Craft[idx - 40].itemId;
	} else {
		*block  = SurvInv_Output.block;
		*itemId = SurvInv_Output.itemId;
	}
}

static void SurvInv_SetSlot(int idx, BlockID block, int itemId) {
	if (idx < 27) {
		SurvInv_Main[idx].block  = block;
		SurvInv_Main[idx].itemId = itemId;
	} else if (idx < 36) {
		Inventory_Set(idx - 27, block);
		Hotbar_SetItem(idx - 27, itemId);
	} else if (idx < 40) {
		SurvInv_Armor[idx - 36].block  = block;
		SurvInv_Armor[idx - 36].itemId = itemId;
	} else if (idx < 44) {
		SurvInv_Craft[idx - 40].block  = block;
		SurvInv_Craft[idx - 40].itemId = itemId;
	} else {
		SurvInv_Output.block  = block;
		SurvInv_Output.itemId = itemId;
	}
}

static int SurvInv_GetSlotCount(int idx) {
	if (idx < 27) {
		return SurvInv_Main[idx].count;
	} else if (idx < 36) {
		return Hotbar_GetCount(idx - 27);
	} else if (idx < 40) {
		return SurvInv_Armor[idx - 36].count;
	} else if (idx < 44) {
		return SurvInv_Craft[idx - 40].count;
	} else {
		return SurvInv_Output.count;
	}
}

static void SurvInv_SetSlotCount(int idx, int count) {
	if (idx < 27) {
		SurvInv_Main[idx].count = count;
	} else if (idx < 36) {
		Hotbar_SetCount(idx - 27, count);
	} else if (idx < 40) {
		SurvInv_Armor[idx - 36].count = count;
	} else if (idx < 44) {
		SurvInv_Craft[idx - 40].count = count;
	} else {
		SurvInv_Output.count = count;
	}
}

static int SurvInv_GetSlotDurability(int idx) {
	if (idx < 27)  return SurvInv_Main[idx].durability;
	if (idx < 36)  return Hotbar_GetDurability(idx - 27);
	if (idx < 40)  return SurvInv_Armor[idx - 36].durability;
	return 0;
}

static void SurvInv_SetSlotDurability(int idx, int dur) {
	if (idx < 27)       SurvInv_Main[idx].durability        = dur;
	else if (idx < 36)  Hotbar_SetDurability(idx - 27, dur);
	else if (idx < 40)  SurvInv_Armor[idx - 36].durability  = dur;
}

/* Helper: check if two slot contents are the same type (for stacking) */
static cc_bool SurvInv_SameType(BlockID b1, int i1, BlockID b2, int i2) {
	if (b1 != BLOCK_AIR && b2 != BLOCK_AIR && b1 == b2 && i1 == ITEM_NONE && i2 == ITEM_NONE) return true;
	if (i1 != ITEM_NONE && i2 != ITEM_NONE && i1 == i2 && b1 == BLOCK_AIR && b2 == BLOCK_AIR) return true;
	return false;
}

/* Helper: get max stack size for a block/item combo */
static int SurvInv_MaxStack(BlockID block, int itemId) {
	if (itemId != ITEM_NONE) return Item_MaxStackSize(itemId);
	return Block_MaxStackSize(block);
}

static void SurvInv_GetSlotPos(struct SurvivalInventoryScreen* s, int idx, int* x, int* y) {
	int cs = s->cellSize;
	if (idx < 27) {
		/* Main 3x9 grid */
		*x = s->gridX + (idx % 9) * cs;
		*y = s->gridY + (idx / 9) * cs;
	} else if (idx < 36) {
		/* Hotbar */
		*x = s->gridX + (idx - 27) * cs;
		*y = s->hotbarY;
	} else if (idx < 40) {
		/* Armor (horizontal) */
		*x = s->armorX + (idx - 36) * cs;
		*y = s->armorY;
	} else if (idx < 44) {
		/* Crafting 2x2 */
		int ci = idx - 40;
		*x = s->craftX + (ci % 2) * cs;
		*y = s->craftY + (ci / 2) * cs;
	} else {
		/* Output */
		*x = s->outputX;
		*y = s->outputY;
	}
}

static int SurvInv_HitTest(struct SurvivalInventoryScreen* s, int mx, int my) {
	int i, sx, sy;
	for (i = 0; i < SURVINV_SLOT_COUNT; i++) {
		SurvInv_GetSlotPos(s, i, &sx, &sy);
		if (Gui_Contains(sx, sy, s->cellSize, s->cellSize, mx, my)) return i;
	}
	return -1;
}


static void SurvInv_Layout(void* screen) {
	struct SurvivalInventoryScreen* s = (struct SurvivalInventoryScreen*)screen;
	float scale;
	int cs, gap, totalWidth, totalHeight, baseX, baseY;

	scale = Gui_GetInventoryScale();
	cs    = (int)(44.0f * Math_SqrtF(scale));
	if (cs < 20) cs = 20;
	s->cellSize = cs;
	gap = cs / 3;

	totalWidth  = cs * 9;
	totalHeight = cs * 2 + gap + cs * 3 + gap + cs;
	baseX = (Window_UI.Width  - totalWidth)  / 2;
	baseY = (Window_UI.Height - totalHeight) / 2;

	/* Armor: 4 horizontal slots at top left */
	s->armorX = baseX;
	s->armorY = baseY;

	/* Crafting 2x2: top right area */
	s->craftX = baseX + cs * 5 - cs / 4;
	s->craftY = baseY;

	/* Output: to the right of crafting with gap for arrow */
	s->outputX = s->craftX + cs * 3 + cs / 4;
	s->outputY = s->craftY + cs / 2;

	/* Main 3x9 grid */
	s->gridX = baseX;
	s->gridY = baseY + cs * 2 + gap;

	/* Hotbar row */
	s->hotbarY = s->gridY + cs * 3 + gap;

	/* Title above everything */
	Widget_SetLocation(&s->title, ANCHOR_CENTRE, ANCHOR_MIN, 0, 0);
	s->title.yOffset = baseY - s->title.height - 12;
	Widget_Layout(&s->title);
}

/* Stack count texture cache for survival inventory */
static struct FontDesc survInv_countFont;
static struct Texture  survInv_countTex[MAX_STACK_SIZE + 1]; /* index 0-64, use 2-64 */
static cc_bool         survInv_countTexValid[MAX_STACK_SIZE + 1];
static cc_bool         survInv_countFontValid;

static void SurvInv_FreeCountTextures(void) {
	int i;
	for (i = 2; i <= MAX_STACK_SIZE; i++) {
		if (survInv_countTexValid[i]) {
			Gfx_DeleteTexture(&survInv_countTex[i].ID);
			survInv_countTexValid[i] = false;
		}
	}
	if (survInv_countFontValid) {
		Font_Free(&survInv_countFont);
		survInv_countFontValid = false;
	}
}

static void SurvInv_EnsureCountTex(int count) {
	struct DrawTextArgs args;
	cc_string str; char buf[8];

	if (count < 2 || count > MAX_STACK_SIZE) return;
	if (survInv_countTexValid[count]) return;

	if (!survInv_countFontValid) {
		Font_Make(&survInv_countFont, 16, FONT_FLAGS_NONE);
		survInv_countFontValid = true;
	}

	String_InitArray(str, buf);
	String_AppendInt(&str, count);
	DrawTextArgs_Make(&args, &str, &survInv_countFont, true);
	Drawer2D_MakeTextTexture(&survInv_countTex[count], &args);
	survInv_countTexValid[count] = true;
}

static void SurvInv_RenderCount(int count, int slotX, int slotY, int cellSize) {
	struct Texture tex;
	if (count < 2 || count > MAX_STACK_SIZE) return;

	SurvInv_EnsureCountTex(count);
	tex = survInv_countTex[count];
	/* Position at bottom-right of slot */
	tex.x = slotX + cellSize - tex.width - 1;
	tex.y = slotY + cellSize - tex.height + 8;
	Texture_Render(&tex);
}

static void SurvInv_ContextLost(void* screen) {
	struct SurvivalInventoryScreen* s = (struct SurvivalInventoryScreen*)screen;
	Font_Free(&s->font);
	SurvInv_FreeCountTextures();
	Screen_ContextLost(s);
}

static void SurvInv_ContextRecreated(void* screen) {
	struct SurvivalInventoryScreen* s = (struct SurvivalInventoryScreen*)screen;
	Screen_UpdateVb(s);
	Gui_MakeBodyFont(&s->font);
	TextWidget_SetConst(&s->title, "Inventory", &s->font);
}

static void SurvInv_Init(void* screen) {
	struct SurvivalInventoryScreen* s = (struct SurvivalInventoryScreen*)screen;
	s->widgets     = s->__widgets;
	s->numWidgets  = 0;
	s->maxWidgets  = Array_Elems(s->__widgets);

	TextWidget_Add(s, &s->title);

	s->holding     = false;
	s->holdBlock   = BLOCK_AIR;
	s->holdItem    = ITEM_NONE;
	s->holdCount   = 0;
	s->hoveredSlot = -1;
	s->releasedKey = false;

	s->maxVertices = TEXTWIDGET_MAX + SURVINV_ISO_VERTICES;
}

static void SurvInv_Free(void* screen) {
}

static void SurvInv_Update(void* screen, float delta) {
	struct SurvivalInventoryScreen* s = (struct SurvivalInventoryScreen*)screen;
	s->dirty = true; /* Always rebuild iso mesh */
}

static void SurvInv_Render(void* screen, float delta) {
	struct SurvivalInventoryScreen* s = (struct SurvivalInventoryScreen*)screen;
	struct VertexTextured* data;
	int i, sx, sy, cs;
	BlockID block;
	int itemId;
	float isoSize;

	PackedCol bgCol = PackedCol_Make(0xc6, 0xc6, 0xc6, 255);

	cs      = s->cellSize;
	isoSize = cs * 0.38f;

	/* Background panel */
	{
		int px = s->armorX - 4, py = s->armorY - 4;
		int pw = cs * 9 + 8,    ph = s->hotbarY + cs - s->armorY + 8;
		int b  = 8;
		Gfx_Draw2DFlat(px, py, pw, ph, bgCol);
		SurvGUI_DrawBorder(px, py, pw, ph, b);
	}

	/* Slot backgrounds */
	for (i = 0; i < SURVINV_SLOT_COUNT; i++) {
		SurvInv_GetSlotPos(s, i, &sx, &sy);
		SurvGUI_DrawSlot(sx, sy, cs, i == s->hoveredSlot);
	}

	/* Crafting arrow: centered between 2x2 grid and output slot with small gaps */
	{
		int gapX   = s->craftX + cs * 2;
		int gapW   = s->outputX - gapX;
		int pad    = cs / 6;
		int arrowX = gapX + pad;
		int arrowW = gapW - pad * 2;
		int arrowH = (int)(arrowW / ARROW_ASPECT);
		int arrowY = s->craftY + (cs * 2 - arrowH) / 2;
		SurvGUI_DrawArrow(arrowX, arrowY, arrowW, 0.0f);
	}

	/* Armor overlays — drawn on empty armor slots only */
	for (i = 0; i < 4; i++) {
		SurvInv_GetSlot(36 + i, &block, &itemId);
		if (block != BLOCK_AIR || itemId != ITEM_NONE) continue;
		SurvInv_GetSlotPos(s, 36 + i, &sx, &sy);
		SurvGUI_DrawArmorOverlay(i, sx, sy, cs);
	}

	/* Build iso batch for slot contents */
	data = Screen_LockVb(s);
	{
		struct VertexTextured** ptr = &data;
		Widget_BuildMesh(&s->title, ptr);
	}

	IsometricDrawer_BeginBatch(data, s->isoState);
	for (i = 0; i < SURVINV_SLOT_COUNT; i++) {
		SurvInv_GetSlot(i, &block, &itemId);
		if (block == BLOCK_AIR && itemId == ITEM_NONE) continue;

		SurvInv_GetSlotPos(s, i, &sx, &sy);

		if (itemId != ITEM_NONE && itemId > 0 && itemId < ITEM_COUNT) {
			IsometricDrawer_AddItemBatch(ItemTextures[itemId], isoSize,
				(float)(sx + cs / 2), (float)(sy + cs / 2));
		} else if (block != BLOCK_AIR) {
			IsometricDrawer_AddBatch(block, isoSize,
				(float)(sx + cs / 2), (float)(sy + cs / 2));
		}
	}

	/* Held item at cursor */
	if (s->holding) {
		int mx = Pointers[0].x, my = Pointers[0].y;
		if (s->holdItem != ITEM_NONE && s->holdItem > 0 && s->holdItem < ITEM_COUNT) {
			IsometricDrawer_AddItemBatch(ItemTextures[s->holdItem], isoSize,
				(float)mx, (float)my);
		} else if (s->holdBlock != BLOCK_AIR) {
			IsometricDrawer_AddBatch(s->holdBlock, isoSize,
				(float)mx, (float)my);
		}
	}

	s->verticesCount = IsometricDrawer_EndBatch();
	Gfx_UnlockDynamicVb(s->vb);

	/* Render title text */
	Gfx_SetVertexFormat(VERTEX_FORMAT_TEXTURED);
	Gfx_BindDynamicVb(s->vb);
	Widget_Render2(&s->title, 0);

	/* Render iso batch */
	if (s->verticesCount) {
		IsometricDrawer_Render(s->verticesCount, TEXTWIDGET_MAX, s->isoState);
	}

	/* Render stack count numbers */
	for (i = 0; i < SURVINV_SLOT_COUNT; i++) {
		int cnt = SurvInv_GetSlotCount(i);
		if (cnt > 1) {
			SurvInv_GetSlotPos(s, i, &sx, &sy);
			SurvInv_RenderCount(cnt, sx, sy, cs);
		}
	}
	/* Held item count at cursor */
	if (s->holding && s->holdCount > 1) {
		int mx = Pointers[0].x, my = Pointers[0].y;
		SurvInv_RenderCount(s->holdCount, mx - cs / 2, my - cs / 2, cs);
	}

	/* Durability bars — drawn after everything else (no texture needed) */
	if (Game_SurvivalMode) {
		int barMaxW = cs * 10 / 16;
		int barH    = cs / 14;
		if (barH < 1) barH = 1;

		for (i = 0; i < SURVINV_SLOT_COUNT; i++) {
			BlockID slotBlock; int slotItem, maxDur, dur, stage, filled, hue, barX, barY;
			SurvInv_GetSlot(i, &slotBlock, &slotItem);
			if (slotItem <= ITEM_NONE || slotItem >= ITEM_COUNT) continue;
			maxDur = ItemMaxDurability[slotItem];
			if (maxDur == 0) continue; /* infinite */

			dur = SurvInv_GetSlotDurability(i);
			if (dur == 0) continue; /* full (never damaged) — no bar */

			SurvInv_GetSlotPos(s, i, &sx, &sy);
			barX = sx + (cs - barMaxW) / 2;
			barY = sy + cs - barH - 3;

			/* Quantise to 16 stages then map to bar width and colour */
			stage  = dur * 32 / maxDur;
			if (stage < 1) stage = 1;
			filled = barMaxW * stage / 32;
			hue    = 120 * stage / 32; /* 0° (red) → 120° (green) */

			Gfx_Draw2DFlat(barX, barY, barMaxW, barH, PackedCol_Make(0, 0, 0, 255));
			Gfx_Draw2DFlat(barX, barY, filled, barH, PackedCol_Make(
				hue <= 60 ? 255 : (120 - hue) * 255 / 60,
				hue <= 60 ? hue * 255 / 60 : 255,
				0, 255));
		}
	}
}

/* Returns true if the given item/block can be placed in the given slot.
   Armor slots 36-39 only accept the matching armor type; all other slots accept anything. */
static cc_bool SurvInv_ItemAllowedInSlot(int slot, BlockID block, int itemId) {
	int armorType;
	if (slot < 36 || slot > 39) return true; /* not an armor slot */
	if (block != BLOCK_AIR)     return false; /* armor slots hold items only */
	if (itemId <= ITEM_NONE || itemId >= ITEM_COUNT) return false;
	if (ItemArmorPoints[itemId] == 0) return false; /* not an armor item */
	/* Within each 8-item tier group: offset 0=helm, 1=chest, 2=legs, 3=boots */
	armorType = (itemId - 1) % 8;
	return armorType == (slot - 36);
}

static int SurvInv_KeyDown(void* screen, int key, struct InputDevice* device) {
	struct SurvivalInventoryScreen* s = (struct SurvivalInventoryScreen*)screen;

	if (s->releasedKey) {
		if (InputBind_Claims(BIND_INVENTORY, key, device) ||
			key == 'I' || key == 'E' || key == device->escapeButton) {
			Gui_Remove((struct Screen*)s);
			return true;
		}
	}

	/* Right-click handling for single-item operations */
	if (key == CCMOUSE_R) {
		int slot, slotCount;
		BlockID slotBlock;
		int slotItem;
		int mx = Pointers[0].x, my = Pointers[0].y;

		slot = SurvInv_HitTest(s, mx, my);
		if (slot == -1) return true;

		/* Block all right-click placement into output slot */
		if (slot == 44 && s->holding) return true;

		SurvInv_GetSlot(slot, &slotBlock, &slotItem);
		slotCount = SurvInv_GetSlotCount(slot);

		if (!s->holding) {
			/* Pick up half the stack */
			if (slotBlock == BLOCK_AIR && slotItem == ITEM_NONE) return true;
			if (slotCount <= 1) {
				/* Only 1 item: pick up the whole thing */
				s->holding        = true;
				s->holdBlock      = slotBlock;
				s->holdItem       = slotItem;
				s->holdCount      = 1;
				s->holdDurability = SurvInv_GetSlotDurability(slot);
				SurvInv_SetSlot(slot, BLOCK_AIR, ITEM_NONE);
				SurvInv_SetSlotCount(slot, 0);
				SurvInv_SetSlotDurability(slot, 0);
				if (slot == 44) Crafting_TakeOutput2x2();
			} else {
				int takeCount = slotCount / 2;
				s->holding   = true;
				s->holdBlock = slotBlock;
				s->holdItem  = slotItem;
				s->holdCount = takeCount;
				SurvInv_SetSlotCount(slot, slotCount - takeCount);
			}
		} else {
			if (slotBlock == BLOCK_AIR && slotItem == ITEM_NONE) {
				/* Place 1 item into empty slot */
				if (!SurvInv_ItemAllowedInSlot(slot, s->holdBlock, s->holdItem)) return true;
				SurvInv_SetSlot(slot, s->holdBlock, s->holdItem);
				SurvInv_SetSlotCount(slot, 1);
				SurvInv_SetSlotDurability(slot, s->holdDurability);
				s->holdCount--;
				if (s->holdCount <= 0) {
					s->holding        = false;
					s->holdBlock      = BLOCK_AIR;
					s->holdItem       = ITEM_NONE;
					s->holdCount      = 0;
					s->holdDurability = 0;
				}
			} else if (SurvInv_SameType(slotBlock, slotItem, s->holdBlock, s->holdItem) &&
					   slotCount < SurvInv_MaxStack(s->holdBlock, s->holdItem)) {
				/* Add 1 to existing stack of same type */
				SurvInv_SetSlotCount(slot, slotCount + 1);
				s->holdCount--;
				if (s->holdCount <= 0) {
					s->holding        = false;
					s->holdBlock      = BLOCK_AIR;
					s->holdItem       = ITEM_NONE;
					s->holdCount      = 0;
					s->holdDurability = 0;
				}
			}
			/* Different type: do nothing */
		}

		if (slot >= 27 && slot < 36) {
			Event_RaiseVoid(&UserEvents.HeldBlockChanged);
		}
		if (slot >= 40 && slot <= 43) {
			Crafting_UpdateOutput2x2();
		}
		s->dirty = true;
		return true;
	}

	return Elem_HandlesKeyDown(&HUDScreen_Instance.hotbar, key, device);
}

static void SurvInv_KeyUp(void* screen, int key, struct InputDevice* device) {
	struct SurvivalInventoryScreen* s = (struct SurvivalInventoryScreen*)screen;
	if (InputBind_Claims(BIND_INVENTORY, key, device) ||
		key == 'I' || key == 'E') {
		s->releasedKey = true;
	}
}

static int SurvInv_PointerDown(void* screen, int id, int x, int y) {
	struct SurvivalInventoryScreen* s = (struct SurvivalInventoryScreen*)screen;
	int slot, slotCount;
	BlockID slotBlock;
	int slotItem;

	slot = SurvInv_HitTest(s, x, y);

	if (slot == -1) {
		if (!s->holding) {
			Gui_Remove((struct Screen*)s);
		} else {
			/* Drop held item into the world */
			SurvInv_DropHeldItem(s->holdBlock, s->holdItem, s->holdCount);
			s->holding        = false;
			s->holdBlock      = BLOCK_AIR;
			s->holdItem       = ITEM_NONE;
			s->holdCount      = 0;
			s->holdDurability = 0;
			s->dirty = true;
		}
		return TOUCH_TYPE_GUI;
	}

	SurvInv_GetSlot(slot, &slotBlock, &slotItem);
	slotCount = SurvInv_GetSlotCount(slot);

	if (!s->holding) {
		/* Pick up entire stack from slot */
		if (slotBlock == BLOCK_AIR && slotItem == ITEM_NONE) return TOUCH_TYPE_GUI;
		s->holding         = true;
		s->holdBlock       = slotBlock;
		s->holdItem        = slotItem;
		s->holdCount       = slotCount;
		s->holdDurability  = SurvInv_GetSlotDurability(slot);
		if (s->holdCount < 1) s->holdCount = 1; /* Normalize: creative mode items may have count=0 */
		SurvInv_SetSlot(slot, BLOCK_AIR, ITEM_NONE);
		SurvInv_SetSlotCount(slot, 0);
		SurvInv_SetSlotDurability(slot, 0);
		/* If taking from output slot, consume crafting materials */
		if (slot == 44) Crafting_TakeOutput2x2();
	} else {
		/* Output slot: only allow adding to held stack of same type */
		if (slot == 44) {
			if (SurvInv_SameType(slotBlock, slotItem, s->holdBlock, s->holdItem) &&
				slotCount > 0) {
				int maxS = SurvInv_MaxStack(s->holdBlock, s->holdItem);
				int total = s->holdCount + slotCount;
				if (total <= maxS) {
					s->holdCount = total;
					SurvInv_SetSlot(slot, BLOCK_AIR, ITEM_NONE);
					SurvInv_SetSlotCount(slot, 0);
					Crafting_TakeOutput2x2();
				}
			}
			goto done;
		}

		if (slotBlock == BLOCK_AIR && slotItem == ITEM_NONE) {
			/* Place entire held stack into empty slot */
			if (!SurvInv_ItemAllowedInSlot(slot, s->holdBlock, s->holdItem)) goto done;
			SurvInv_SetSlot(slot, s->holdBlock, s->holdItem);
			SurvInv_SetSlotCount(slot, s->holdCount);
			SurvInv_SetSlotDurability(slot, s->holdDurability);
			s->holding        = false;
			s->holdBlock      = BLOCK_AIR;
			s->holdItem       = ITEM_NONE;
			s->holdCount      = 0;
			s->holdDurability = 0;
		} else if (SurvInv_SameType(slotBlock, slotItem, s->holdBlock, s->holdItem)) {
			/* Merge stacks of same type */
			int maxS = SurvInv_MaxStack(s->holdBlock, s->holdItem);
			int total = slotCount + s->holdCount;
			if (total <= maxS) {
				SurvInv_SetSlotCount(slot, total);
				s->holding        = false;
				s->holdBlock      = BLOCK_AIR;
				s->holdItem       = ITEM_NONE;
				s->holdCount      = 0;
				s->holdDurability = 0;
			} else {
				SurvInv_SetSlotCount(slot, maxS);
				s->holdCount = total - maxS;
			}
		} else {
			/* Swap with different type */
			if (!SurvInv_ItemAllowedInSlot(slot, s->holdBlock, s->holdItem)) goto done;
			{ int tmpDur = SurvInv_GetSlotDurability(slot);
			  BlockID tmpBlock = slotBlock;
			  int tmpItem  = slotItem;
			  int tmpCount = slotCount;
			  SurvInv_SetSlot(slot, s->holdBlock, s->holdItem);
			  SurvInv_SetSlotCount(slot, s->holdCount);
			  SurvInv_SetSlotDurability(slot, s->holdDurability);
			  s->holdBlock      = tmpBlock;
			  s->holdItem       = tmpItem;
			  s->holdCount      = tmpCount;
			  s->holdDurability = tmpDur; }
		}
	}

done:
	/* Notify hotbar changed if we modified a hotbar slot */
	if (slot >= 27 && slot < 36) {
		Event_RaiseVoid(&UserEvents.HeldBlockChanged);
	}
	/* Update crafting output if a crafting slot changed */
	if (slot >= 40 && slot <= 43) {
		Crafting_UpdateOutput2x2();
	}
	s->dirty = true;
	return TOUCH_TYPE_GUI;
}

static int SurvInv_PointerMove(void* screen, int id, int x, int y) {
	struct SurvivalInventoryScreen* s = (struct SurvivalInventoryScreen*)screen;
	s->hoveredSlot = SurvInv_HitTest(s, x, y);
	s->dirty = true;
	return s->hoveredSlot != -1;
}

static const struct ScreenVTABLE SurvivalInventoryScreen_VTABLE = {
	SurvInv_Init,        SurvInv_Update,      SurvInv_Free,
	SurvInv_Render,      Screen_BuildMesh,
	SurvInv_KeyDown,     SurvInv_KeyUp,        Screen_TKeyPress,     Screen_TText,
	SurvInv_PointerDown, Screen_PointerUp,     SurvInv_PointerMove,  Screen_TMouseScroll,
	SurvInv_Layout,      SurvInv_ContextLost,  SurvInv_ContextRecreated
};

void SurvivalInventoryScreen_Show(void) {
	struct SurvivalInventoryScreen* s = &SurvivalInventoryScreen_Instance;
	s->grabsInput = true;
	s->closable   = true;

	s->VTABLE = &SurvivalInventoryScreen_VTABLE;
	Gui_Add((struct Screen*)s, GUI_PRIORITY_INVENTORY);
}


/*########################################################################################################################*
*---------------------------------------------------CraftingTableScreen----------------------------------------------------*
*#########################################################################################################################*/
/* Slot layout (46 total):
 *  0-26:  Main inventory (3x9)  -> SurvInv_Main[idx]
 *  27-35: Hotbar (9)            -> Inventory_Get/Set, Hotbar_GetItem/SetItem
 *  36-44: Crafting 3x3          -> CraftTable_Grid[idx-36]
 *  45:    Output                -> CraftTable_Output
 */
#define CRAFTTABLE_SLOT_COUNT 46
#define CRAFTTABLE_ISO_VERTICES ((CRAFTTABLE_SLOT_COUNT + 1) * ISOMETRICDRAWER_MAXVERTICES)

static struct CraftingTableScreen {
	Screen_Body
	struct FontDesc font;
	struct TextWidget title;

	int cellSize;
	int gridX, gridY;      /* 3x9 main grid top-left */
	int hotbarY;           /* hotbar row Y */
	int craftX, craftY;    /* 3x3 crafting top-left */
	int outputX, outputY;  /* output slot position */

	cc_bool  holding;
	BlockID  holdBlock;
	int      holdItem;
	int      holdCount;

	int hoveredSlot;       /* -1 = none */

	int isoState[CRAFTTABLE_ISO_VERTICES / 4];
	int verticesCount;

	cc_bool releasedKey;
	cc_bool releasedMouse;
	struct Widget* __widgets[1];
} CraftingTableScreen_Instance CC_BIG_VAR;

static void CraftTable_GetSlot(int idx, BlockID* block, int* itemId) {
	if (idx < 27) {
		*block  = SurvInv_Main[idx].block;
		*itemId = SurvInv_Main[idx].itemId;
	} else if (idx < 36) {
		*block  = Inventory_Get(idx - 27);
		*itemId = Hotbar_GetItem(idx - 27);
	} else if (idx < 45) {
		*block  = CraftTable_Grid[idx - 36].block;
		*itemId = CraftTable_Grid[idx - 36].itemId;
	} else {
		*block  = CraftTable_Output.block;
		*itemId = CraftTable_Output.itemId;
	}
}

static void CraftTable_SetSlot(int idx, BlockID block, int itemId) {
	if (idx < 27) {
		SurvInv_Main[idx].block  = block;
		SurvInv_Main[idx].itemId = itemId;
	} else if (idx < 36) {
		Inventory_Set(idx - 27, block);
		Hotbar_SetItem(idx - 27, itemId);
	} else if (idx < 45) {
		CraftTable_Grid[idx - 36].block  = block;
		CraftTable_Grid[idx - 36].itemId = itemId;
	} else {
		CraftTable_Output.block  = block;
		CraftTable_Output.itemId = itemId;
	}
}

static int CraftTable_GetSlotCount(int idx) {
	if (idx < 27) {
		return SurvInv_Main[idx].count;
	} else if (idx < 36) {
		return Hotbar_GetCount(idx - 27);
	} else if (idx < 45) {
		return CraftTable_Grid[idx - 36].count;
	} else {
		return CraftTable_Output.count;
	}
}

static void CraftTable_SetSlotCount(int idx, int count) {
	if (idx < 27) {
		SurvInv_Main[idx].count = count;
	} else if (idx < 36) {
		Hotbar_SetCount(idx - 27, count);
	} else if (idx < 45) {
		CraftTable_Grid[idx - 36].count = count;
	} else {
		CraftTable_Output.count = count;
	}
}

static void CraftTable_GetSlotPos(struct CraftingTableScreen* s, int idx, int* x, int* y) {
	int cs = s->cellSize;
	if (idx < 27) {
		/* Main 3x9 grid */
		*x = s->gridX + (idx % 9) * cs;
		*y = s->gridY + (idx / 9) * cs;
	} else if (idx < 36) {
		/* Hotbar */
		*x = s->gridX + (idx - 27) * cs;
		*y = s->hotbarY;
	} else if (idx < 45) {
		/* Crafting 3x3 */
		int ci = idx - 36;
		*x = s->craftX + (ci % 3) * cs;
		*y = s->craftY + (ci / 3) * cs;
	} else {
		/* Output */
		*x = s->outputX;
		*y = s->outputY;
	}
}

static int CraftTable_HitTest(struct CraftingTableScreen* s, int mx, int my) {
	int i, sx, sy;
	for (i = 0; i < CRAFTTABLE_SLOT_COUNT; i++) {
		CraftTable_GetSlotPos(s, i, &sx, &sy);
		if (Gui_Contains(sx, sy, s->cellSize, s->cellSize, mx, my)) return i;
	}
	return -1;
}

/* Return items from crafting grid to player inventory on close */
static void CraftTable_ReturnItems(void) {
	int i, j;
	for (i = 0; i < 9; i++) {
		if (CraftTable_Grid[i].count <= 0) continue;

		/* Try to stack into existing same-type slots in main inventory */
		for (j = 0; j < 27; j++) {
			if (SurvInv_SameType(SurvInv_Main[j].block, SurvInv_Main[j].itemId,
								 CraftTable_Grid[i].block, CraftTable_Grid[i].itemId) &&
				SurvInv_Main[j].count > 0 &&
				SurvInv_Main[j].count + CraftTable_Grid[i].count <= SurvInv_MaxStack(CraftTable_Grid[i].block, CraftTable_Grid[i].itemId)) {
				SurvInv_Main[j].count += CraftTable_Grid[i].count;
				CraftTable_Grid[i].count = 0;
				CraftTable_Grid[i].block = BLOCK_AIR;
				CraftTable_Grid[i].itemId = ITEM_NONE;
				break;
			}
		}
		if (CraftTable_Grid[i].count <= 0) continue;

		/* Try empty main inventory slot */
		for (j = 0; j < 27; j++) {
			if (SurvInv_Main[j].count > 0) continue;
			SurvInv_Main[j] = CraftTable_Grid[i];
			CraftTable_Grid[i].count = 0;
			CraftTable_Grid[i].block = BLOCK_AIR;
			CraftTable_Grid[i].itemId = ITEM_NONE;
			break;
		}
		if (CraftTable_Grid[i].count <= 0) continue;

		/* Try empty hotbar slot */
		for (j = 0; j < INVENTORY_BLOCKS_PER_HOTBAR; j++) {
			if (Inventory_Get(j) != BLOCK_AIR || Hotbar_GetItem(j) != ITEM_NONE) continue;
			if (CraftTable_Grid[i].itemId != ITEM_NONE) {
				Hotbar_SetItem(j, CraftTable_Grid[i].itemId);
				Inventory_Set(j, BLOCK_AIR);
			} else {
				Inventory_Set(j, CraftTable_Grid[i].block);
				Hotbar_SetItem(j, ITEM_NONE);
			}
			Hotbar_SetCount(j, CraftTable_Grid[i].count);
			CraftTable_Grid[i].count = 0;
			CraftTable_Grid[i].block = BLOCK_AIR;
			CraftTable_Grid[i].itemId = ITEM_NONE;
			break;
		}
		/* If still can't fit, items are lost */
	}
}

static void CraftTable_Layout(void* screen) {
	struct CraftingTableScreen* s = (struct CraftingTableScreen*)screen;
	float scale;
	int cs, gap, totalWidth, totalHeight, baseX, baseY;
	int craftAreaWidth, craftAreaX;

	scale = Gui_GetInventoryScale();
	cs    = (int)(44.0f * Math_SqrtF(scale));
	if (cs < 20) cs = 20;
	s->cellSize = cs;
	gap = cs / 3;

	totalWidth  = cs * 9;
	totalHeight = cs * 3 + gap + cs * 3 + gap + cs;
	baseX = (Window_UI.Width  - totalWidth)  / 2;
	baseY = (Window_UI.Height - totalHeight) / 2;

	/* Center the crafting area (3x3 grid + gap + output) within the 9-wide row */
	craftAreaWidth = cs * 3 + cs * 2 + cs;  /* 3 grid cols + 2 gap cols + 1 output col */
	craftAreaX = baseX + (totalWidth - craftAreaWidth) / 2;

	/* Crafting 3x3 grid */
	s->craftX = craftAreaX;
	s->craftY = baseY;

	/* Output: to the right of crafting with extra gap for arrow */
	s->outputX = craftAreaX + cs * 5;
	s->outputY = s->craftY + cs;

	/* Main 3x9 grid */
	s->gridX = baseX;
	s->gridY = baseY + cs * 3 + gap;

	/* Hotbar row */
	s->hotbarY = s->gridY + cs * 3 + gap;

	/* Title above everything */
	Widget_SetLocation(&s->title, ANCHOR_CENTRE, ANCHOR_MIN, 0, 0);
	s->title.yOffset = baseY - s->title.height - 12;
	Widget_Layout(&s->title);
}

static void CraftTable_ContextLost(void* screen) {
	struct CraftingTableScreen* s = (struct CraftingTableScreen*)screen;
	Font_Free(&s->font);
	Screen_ContextLost(s);
}

static void CraftTable_ContextRecreated(void* screen) {
	struct CraftingTableScreen* s = (struct CraftingTableScreen*)screen;
	Screen_UpdateVb(s);
	Gui_MakeBodyFont(&s->font);
	TextWidget_SetConst(&s->title, "Crafting", &s->font);
}

static void CraftTable_Init(void* screen) {
	struct CraftingTableScreen* s = (struct CraftingTableScreen*)screen;
	s->widgets     = s->__widgets;
	s->numWidgets  = 0;
	s->maxWidgets  = Array_Elems(s->__widgets);

	TextWidget_Add(s, &s->title);

	s->holding     = false;
	s->holdBlock   = BLOCK_AIR;
	s->holdItem    = ITEM_NONE;
	s->holdCount   = 0;
	s->hoveredSlot = -1;
	s->releasedKey   = false;
	s->releasedMouse = false;

	/* Clear crafting grid and output */
	Mem_Set(CraftTable_Grid, 0, sizeof(CraftTable_Grid));
	Mem_Set(&CraftTable_Output, 0, sizeof(CraftTable_Output));

	s->maxVertices = TEXTWIDGET_MAX + CRAFTTABLE_ISO_VERTICES;
	inventoryScreenOpen = true;
}

static void CraftTable_Free(void* screen) {
	CraftTable_ReturnItems();
	inventoryScreenOpen = false;
}

static void CraftTable_Update(void* screen, float delta) {
	struct CraftingTableScreen* s = (struct CraftingTableScreen*)screen;
	s->dirty = true;
}

static void CraftTable_Render(void* screen, float delta) {
	struct CraftingTableScreen* s = (struct CraftingTableScreen*)screen;
	struct VertexTextured* data;
	int i, sx, sy, cs;
	BlockID block;
	int itemId;
	float isoSize;

	PackedCol bgCol = PackedCol_Make(0xc6, 0xc6, 0xc6, 255);

	cs      = s->cellSize;
	isoSize = cs * 0.38f;

	/* Background panel */
	{
		int px = s->gridX - 4,  py = s->craftY - 4;
		int pw = cs * 9 + 8,    ph = s->hotbarY + cs - s->craftY + 8;
		int b  = 8;
		Gfx_Draw2DFlat(px, py, pw, ph, bgCol);
		SurvGUI_DrawBorder(px, py, pw, ph, b);
	}

	/* Slot backgrounds */
	for (i = 0; i < CRAFTTABLE_SLOT_COUNT; i++) {
		CraftTable_GetSlotPos(s, i, &sx, &sy);
		SurvGUI_DrawSlot(sx, sy, cs, i == s->hoveredSlot);
	}

	/* Crafting arrow: between 3x3 grid and output slot (empty only, half-size) */
	{
		int gapX   = s->craftX + cs * 3;
		int gapW   = s->outputX - gapX;
		int arrowW = gapW / 2;
		int arrowX = gapX + (gapW - arrowW) / 2;
		int arrowH = (int)(arrowW / ARROW_ASPECT);
		int arrowY = s->craftY + (cs * 3 - arrowH) / 2;
		SurvGUI_DrawArrow(arrowX, arrowY, arrowW, 0.0f);
	}

	/* Build iso batch for slot contents */
	data = Screen_LockVb(s);
	{
		struct VertexTextured** ptr = &data;
		Widget_BuildMesh(&s->title, ptr);
	}

	IsometricDrawer_BeginBatch(data, s->isoState);
	for (i = 0; i < CRAFTTABLE_SLOT_COUNT; i++) {
		CraftTable_GetSlot(i, &block, &itemId);
		if (block == BLOCK_AIR && itemId == ITEM_NONE) continue;

		CraftTable_GetSlotPos(s, i, &sx, &sy);

		if (itemId != ITEM_NONE && itemId > 0 && itemId < ITEM_COUNT) {
			IsometricDrawer_AddItemBatch(ItemTextures[itemId], isoSize,
				(float)(sx + cs / 2), (float)(sy + cs / 2));
		} else if (block != BLOCK_AIR) {
			IsometricDrawer_AddBatch(block, isoSize,
				(float)(sx + cs / 2), (float)(sy + cs / 2));
		}
	}

	/* Held item at cursor */
	if (s->holding) {
		int mx = Pointers[0].x, my = Pointers[0].y;
		if (s->holdItem != ITEM_NONE && s->holdItem > 0 && s->holdItem < ITEM_COUNT) {
			IsometricDrawer_AddItemBatch(ItemTextures[s->holdItem], isoSize,
				(float)mx, (float)my);
		} else if (s->holdBlock != BLOCK_AIR) {
			IsometricDrawer_AddBatch(s->holdBlock, isoSize,
				(float)mx, (float)my);
		}
	}

	s->verticesCount = IsometricDrawer_EndBatch();
	Gfx_UnlockDynamicVb(s->vb);

	/* Render title text */
	Gfx_SetVertexFormat(VERTEX_FORMAT_TEXTURED);
	Gfx_BindDynamicVb(s->vb);
	Widget_Render2(&s->title, 0);

	/* Render iso batch */
	if (s->verticesCount) {
		IsometricDrawer_Render(s->verticesCount, TEXTWIDGET_MAX, s->isoState);
	}

	/* Render stack count numbers */
	for (i = 0; i < CRAFTTABLE_SLOT_COUNT; i++) {
		int cnt = CraftTable_GetSlotCount(i);
		if (cnt > 1) {
			CraftTable_GetSlotPos(s, i, &sx, &sy);
			SurvInv_RenderCount(cnt, sx, sy, cs);
		}
	}
	/* Held item count at cursor */
	if (s->holding && s->holdCount > 1) {
		int mx = Pointers[0].x, my = Pointers[0].y;
		SurvInv_RenderCount(s->holdCount, mx - cs / 2, my - cs / 2, cs);
	}
}

static int CraftTable_KeyDown(void* screen, int key, struct InputDevice* device) {
	struct CraftingTableScreen* s = (struct CraftingTableScreen*)screen;

	if (s->releasedKey) {
		if (InputBind_Claims(BIND_INVENTORY, key, device) ||
			key == 'I' || key == 'E' || key == device->escapeButton) {
			Gui_Remove((struct Screen*)s);
			return true;
		}
	}

	/* Right-click handling */
	if (key == CCMOUSE_R && s->releasedMouse) {
		int slot, slotCount;
		BlockID slotBlock;
		int slotItem;
		int mx = Pointers[0].x, my = Pointers[0].y;

		slot = CraftTable_HitTest(s, mx, my);
		if (slot == -1) return true;

		/* Block all right-click placement into output slot */
		if (slot == 45 && s->holding) return true;

		CraftTable_GetSlot(slot, &slotBlock, &slotItem);
		slotCount = CraftTable_GetSlotCount(slot);

		if (!s->holding) {
			if (slotBlock == BLOCK_AIR && slotItem == ITEM_NONE) return true;
			if (slotCount <= 1) {
				s->holding   = true;
				s->holdBlock = slotBlock;
				s->holdItem  = slotItem;
				s->holdCount = 1;
				CraftTable_SetSlot(slot, BLOCK_AIR, ITEM_NONE);
				CraftTable_SetSlotCount(slot, 0);
				if (slot == 45) Crafting_TakeOutput3x3();
			} else {
				int takeCount = slotCount / 2;
				s->holding   = true;
				s->holdBlock = slotBlock;
				s->holdItem  = slotItem;
				s->holdCount = takeCount;
				CraftTable_SetSlotCount(slot, slotCount - takeCount);
			}
		} else {
			if (slotBlock == BLOCK_AIR && slotItem == ITEM_NONE) {
				CraftTable_SetSlot(slot, s->holdBlock, s->holdItem);
				CraftTable_SetSlotCount(slot, 1);
				s->holdCount--;
				if (s->holdCount <= 0) {
					s->holding   = false;
					s->holdBlock = BLOCK_AIR;
					s->holdItem  = ITEM_NONE;
					s->holdCount = 0;
				}
			} else if (SurvInv_SameType(slotBlock, slotItem, s->holdBlock, s->holdItem) &&
					   slotCount < SurvInv_MaxStack(s->holdBlock, s->holdItem)) {
				CraftTable_SetSlotCount(slot, slotCount + 1);
				s->holdCount--;
				if (s->holdCount <= 0) {
					s->holding   = false;
					s->holdBlock = BLOCK_AIR;
					s->holdItem  = ITEM_NONE;
					s->holdCount = 0;
				}
			}
		}

		if (slot >= 27 && slot < 36) {
			Event_RaiseVoid(&UserEvents.HeldBlockChanged);
		}
		if (slot >= 36 && slot <= 44) {
			Crafting_UpdateOutput3x3();
		}
		s->dirty = true;
		return true;
	}

	return Elem_HandlesKeyDown(&HUDScreen_Instance.hotbar, key, device);
}

static void CraftTable_KeyUp(void* screen, int key, struct InputDevice* device) {
	struct CraftingTableScreen* s = (struct CraftingTableScreen*)screen;
	if (InputBind_Claims(BIND_INVENTORY, key, device) ||
		key == 'I' || key == 'E') {
		s->releasedKey = true;
	}
	if (key == CCMOUSE_R) {
		s->releasedMouse = true;
	}
}

static int CraftTable_PointerDown(void* screen, int id, int x, int y) {
	struct CraftingTableScreen* s = (struct CraftingTableScreen*)screen;
	int slot, slotCount;
	BlockID slotBlock;
	int slotItem;

	slot = CraftTable_HitTest(s, x, y);

	if (slot == -1) {
		if (!s->holding) {
			Gui_Remove((struct Screen*)s);
		} else {
			/* Drop held item into the world */
			SurvInv_DropHeldItem(s->holdBlock, s->holdItem, s->holdCount);
			s->holding   = false;
			s->holdBlock = BLOCK_AIR;
			s->holdItem  = ITEM_NONE;
			s->holdCount = 0;
			s->dirty = true;
		}
		return TOUCH_TYPE_GUI;
	}

	CraftTable_GetSlot(slot, &slotBlock, &slotItem);
	slotCount = CraftTable_GetSlotCount(slot);

	if (!s->holding) {
		if (slotBlock == BLOCK_AIR && slotItem == ITEM_NONE) return TOUCH_TYPE_GUI;
		s->holding   = true;
		s->holdBlock = slotBlock;
		s->holdItem  = slotItem;
		s->holdCount = slotCount;
		if (s->holdCount < 1) s->holdCount = 1; /* Normalize: creative mode items may have count=0 */
		CraftTable_SetSlot(slot, BLOCK_AIR, ITEM_NONE);
		CraftTable_SetSlotCount(slot, 0);
		if (slot == 45) Crafting_TakeOutput3x3();
	} else {
		/* Output slot: only allow adding to held stack of same type */
		if (slot == 45) {
			if (SurvInv_SameType(slotBlock, slotItem, s->holdBlock, s->holdItem) &&
				slotCount > 0) {
				int maxS = SurvInv_MaxStack(s->holdBlock, s->holdItem);
				int total = s->holdCount + slotCount;
				if (total <= maxS) {
					s->holdCount = total;
					CraftTable_SetSlot(slot, BLOCK_AIR, ITEM_NONE);
					CraftTable_SetSlotCount(slot, 0);
					Crafting_TakeOutput3x3();
				}
			}
			goto ct_done;
		}

		if (slotBlock == BLOCK_AIR && slotItem == ITEM_NONE) {
			CraftTable_SetSlot(slot, s->holdBlock, s->holdItem);
			CraftTable_SetSlotCount(slot, s->holdCount);
			s->holding   = false;
			s->holdBlock = BLOCK_AIR;
			s->holdItem  = ITEM_NONE;
			s->holdCount = 0;
		} else if (SurvInv_SameType(slotBlock, slotItem, s->holdBlock, s->holdItem)) {
			int maxS = SurvInv_MaxStack(s->holdBlock, s->holdItem);
			int total = slotCount + s->holdCount;
			if (total <= maxS) {
				CraftTable_SetSlotCount(slot, total);
				s->holding   = false;
				s->holdBlock = BLOCK_AIR;
				s->holdItem  = ITEM_NONE;
				s->holdCount = 0;
			} else {
				CraftTable_SetSlotCount(slot, maxS);
				s->holdCount = total - maxS;
			}
		} else {
			BlockID tmpBlock = slotBlock;
			int tmpItem      = slotItem;
			int tmpCount     = slotCount;
			CraftTable_SetSlot(slot, s->holdBlock, s->holdItem);
			CraftTable_SetSlotCount(slot, s->holdCount);
			s->holdBlock = tmpBlock;
			s->holdItem  = tmpItem;
			s->holdCount = tmpCount;
		}
	}

ct_done:
	if (slot >= 27 && slot < 36) {
		Event_RaiseVoid(&UserEvents.HeldBlockChanged);
	}
	if (slot >= 36 && slot <= 44) {
		Crafting_UpdateOutput3x3();
	}
	s->dirty = true;
	return TOUCH_TYPE_GUI;
}

static int CraftTable_PointerMove(void* screen, int id, int x, int y) {
	struct CraftingTableScreen* s = (struct CraftingTableScreen*)screen;
	s->hoveredSlot = CraftTable_HitTest(s, x, y);
	s->dirty = true;
	return s->hoveredSlot != -1;
}

static const struct ScreenVTABLE CraftingTableScreen_VTABLE = {
	CraftTable_Init,        CraftTable_Update,      CraftTable_Free,
	CraftTable_Render,      Screen_BuildMesh,
	CraftTable_KeyDown,     CraftTable_KeyUp,        Screen_TKeyPress,     Screen_TText,
	CraftTable_PointerDown, Screen_PointerUp,        CraftTable_PointerMove, Screen_TMouseScroll,
	CraftTable_Layout,      CraftTable_ContextLost,  CraftTable_ContextRecreated
};

void CraftingTableScreen_Show(void) {
	struct CraftingTableScreen* s = &CraftingTableScreen_Instance;
	s->grabsInput = true;
	s->closable   = true;

	s->VTABLE = &CraftingTableScreen_VTABLE;
	Gui_Add((struct Screen*)s, GUI_PRIORITY_INVENTORY);
}


/*########################################################################################################################*
*-------------------------------------------------------FurnaceScreen-----------------------------------------------------*
*#########################################################################################################################*/
/* Slot layout (39 total):
 *  0-26:  Main inventory (3x9)  -> SurvInv_Main[idx]
 *  27-35: Hotbar (9)            -> Inventory_Get/Set, Hotbar_GetItem/SetItem
 *  36:    Input (ore/food)      -> Furnace_Input
 *  37:    Fuel (coal)           -> Furnace_Fuel
 *  38:    Output                -> Furnace_Output
 */
#define FURNACE_SLOT_COUNT 39
#define FURNACE_ISO_VERTICES ((FURNACE_SLOT_COUNT + 1) * ISOMETRICDRAWER_MAXVERTICES)

static struct FurnaceScreen {
	Screen_Body
	struct FontDesc font;
	struct TextWidget title;

	int cellSize;
	int gridX, gridY;      /* 3x9 main grid top-left */
	int hotbarY;           /* hotbar row Y */
	int inputX, inputY;    /* input slot position */
	int fuelX, fuelY;      /* fuel slot position */
	int outputX, outputY;  /* output slot position */
	int progX, progY, progW, progH; /* smelt progress bar position */
	int fuelBarX, fuelBarY, fuelBarW, fuelBarH; /* vertical fuel bar */

	cc_bool  holding;
	BlockID  holdBlock;
	int      holdItem;
	int      holdCount;

	int hoveredSlot;       /* -1 = none */

	int isoState[FURNACE_ISO_VERTICES / 4];
	int verticesCount;

	cc_bool releasedKey;
	cc_bool releasedMouse;
	struct Widget* __widgets[1];
} FurnaceScreen_Instance CC_BIG_VAR;

static void Furnace_GetSlot(int idx, BlockID* block, int* itemId) {
	if (idx < 27) {
		*block  = SurvInv_Main[idx].block;
		*itemId = SurvInv_Main[idx].itemId;
	} else if (idx < 36) {
		*block  = Inventory_Get(idx - 27);
		*itemId = Hotbar_GetItem(idx - 27);
	} else if (idx == 36) {
		*block  = Furnace_Input.block;
		*itemId = Furnace_Input.itemId;
	} else if (idx == 37) {
		*block  = Furnace_Fuel.block;
		*itemId = Furnace_Fuel.itemId;
	} else {
		*block  = Furnace_Output.block;
		*itemId = Furnace_Output.itemId;
	}
}

static void Furnace_SetSlot(int idx, BlockID block, int itemId) {
	if (idx < 27) {
		SurvInv_Main[idx].block  = block;
		SurvInv_Main[idx].itemId = itemId;
	} else if (idx < 36) {
		Inventory_Set(idx - 27, block);
		Hotbar_SetItem(idx - 27, itemId);
	} else if (idx == 36) {
		Furnace_Input.block  = block;
		Furnace_Input.itemId = itemId;
	} else if (idx == 37) {
		Furnace_Fuel.block  = block;
		Furnace_Fuel.itemId = itemId;
	} else {
		Furnace_Output.block  = block;
		Furnace_Output.itemId = itemId;
	}
}

static int Furnace_GetSlotCount(int idx) {
	if (idx < 27)       return SurvInv_Main[idx].count;
	else if (idx < 36)  return Hotbar_GetCount(idx - 27);
	else if (idx == 36) return Furnace_Input.count;
	else if (idx == 37) return Furnace_Fuel.count;
	else                return Furnace_Output.count;
}

static void Furnace_SetSlotCount(int idx, int count) {
	if (idx < 27)       SurvInv_Main[idx].count = count;
	else if (idx < 36)  Hotbar_SetCount(idx - 27, count);
	else if (idx == 36) Furnace_Input.count = count;
	else if (idx == 37) Furnace_Fuel.count = count;
	else                Furnace_Output.count = count;
}

static void Furnace_GetSlotPos(struct FurnaceScreen* s, int idx, int* x, int* y) {
	int cs = s->cellSize;
	if (idx < 27) {
		*x = s->gridX + (idx % 9) * cs;
		*y = s->gridY + (idx / 9) * cs;
	} else if (idx < 36) {
		*x = s->gridX + (idx - 27) * cs;
		*y = s->hotbarY;
	} else if (idx == 36) {
		*x = s->inputX;
		*y = s->inputY;
	} else if (idx == 37) {
		*x = s->fuelX;
		*y = s->fuelY;
	} else {
		*x = s->outputX;
		*y = s->outputY;
	}
}

static int Furnace_HitTest(struct FurnaceScreen* s, int mx, int my) {
	int i, sx, sy;
	for (i = 0; i < FURNACE_SLOT_COUNT; i++) {
		Furnace_GetSlotPos(s, i, &sx, &sy);
		if (Gui_Contains(sx, sy, s->cellSize, s->cellSize, mx, my)) return i;
	}
	return -1;
}

/* Check if a slot/item is valid fuel */
static cc_bool Furnace_IsFuel(BlockID block, int itemId) {
	if (itemId == ITEM_COAL) return true;
	if (itemId == ITEM_STICK) return true;
	if (itemId == ITEM_BOWL) return true;
	if (itemId == ITEM_WOOD_SWORD || itemId == ITEM_WOOD_SHOVEL ||
	    itemId == ITEM_WOOD_PICKAXE || itemId == ITEM_WOOD_AXE) return true;
	if (block == BLOCK_LOG || block == BLOCK_WOOD) return true;
	if (block >= BLOCK_RED && block <= BLOCK_WHITE) return true; /* Wool */
	return false;
}

static void Furnace_Layout(void* screen) {
	struct FurnaceScreen* s = (struct FurnaceScreen*)screen;
	float scale;
	int cs, gap, totalWidth, totalHeight, baseX, baseY;
	int furnaceAreaX, furnaceAreaW;

	scale = Gui_GetInventoryScale();
	cs    = (int)(44.0f * Math_SqrtF(scale));
	if (cs < 20) cs = 20;
	s->cellSize = cs;
	gap = cs / 3;

	totalWidth  = cs * 9;
	totalHeight = cs * 3 + gap + cs * 3 + gap + cs;
	baseX = (Window_UI.Width  - totalWidth)  / 2;
	baseY = (Window_UI.Height - totalHeight) / 2;

	/* Center furnace slots (input, flame, fuel, arrow, output) in top area */
	furnaceAreaW = cs * 4; /* input/flame/fuel col + gap + arrow + output col */
	furnaceAreaX = baseX + (totalWidth - furnaceAreaW) / 2;

	/* Input: top slot */
	s->inputX = furnaceAreaX;
	s->inputY = baseY;

	/* Fuel: bottom slot (below flame) */
	s->fuelX = furnaceAreaX;
	s->fuelY = baseY + cs * 2;

	/* Output: to the right, vertically centered in the 3-row area */
	s->outputX = furnaceAreaX + cs * 3;
	s->outputY = baseY + cs;

	/* Progress bar (unused legacy fields) */
	s->progX = 0; s->progY = 0; s->progW = 0; s->progH = 0;

	/* Flame indicator: middle slot, same column as input/fuel */
	s->fuelBarW = cs;
	s->fuelBarH = cs;
	s->fuelBarX = furnaceAreaX;
	s->fuelBarY = baseY + cs;

	/* Main 3x9 grid */
	s->gridX = baseX;
	s->gridY = baseY + cs * 3 + gap;

	/* Hotbar row */
	s->hotbarY = s->gridY + cs * 3 + gap;

	/* Title above everything */
	Widget_SetLocation(&s->title, ANCHOR_CENTRE, ANCHOR_MIN, 0, 0);
	s->title.yOffset = baseY - s->title.height - 12;
	Widget_Layout(&s->title);
}

static void Furnace_ContextLost(void* screen) {
	struct FurnaceScreen* s = (struct FurnaceScreen*)screen;
	Font_Free(&s->font);
	Screen_ContextLost(s);
}

static void Furnace_ContextRecreated(void* screen) {
	struct FurnaceScreen* s = (struct FurnaceScreen*)screen;
	Screen_UpdateVb(s);
	Gui_MakeBodyFont(&s->font);
	TextWidget_SetConst(&s->title, "Furnace", &s->font);
}

static void Furnace_Init(void* screen) {
	struct FurnaceScreen* s = (struct FurnaceScreen*)screen;
	s->widgets     = s->__widgets;
	s->numWidgets  = 0;
	s->maxWidgets  = Array_Elems(s->__widgets);

	TextWidget_Add(s, &s->title);

	s->holding     = false;
	s->holdBlock   = BLOCK_AIR;
	s->holdItem    = ITEM_NONE;
	s->holdCount   = 0;
	s->hoveredSlot = -1;
	s->releasedKey   = false;
	s->releasedMouse = false;

	s->maxVertices = TEXTWIDGET_MAX + FURNACE_ISO_VERTICES;
	inventoryScreenOpen = true;
}

static void Furnace_Free(void* screen) {
	Furnace_Close();
	inventoryScreenOpen = false;
}

static void Furnace_Update(void* screen, float delta) {
	struct FurnaceScreen* s = (struct FurnaceScreen*)screen;
	s->dirty = true;
}

static void Furnace_Render(void* screen, float delta) {
	struct FurnaceScreen* s = (struct FurnaceScreen*)screen;
	struct VertexTextured* data;
	int i, sx, sy, cs;
	BlockID block;
	int itemId;
	float isoSize;

	PackedCol bgCol    = PackedCol_Make(0xc6, 0xc6, 0xc6, 255);

	cs      = s->cellSize;
	isoSize = cs * 0.38f;

	/* Background panel */
	{
		int px = s->gridX - 4,  py = s->inputY - 4;
		int pw = cs * 9 + 8,    ph = s->hotbarY + cs - s->inputY + 8;
		int b  = 8;
		Gfx_Draw2DFlat(px, py, pw, ph, bgCol);
		SurvGUI_DrawBorder(px, py, pw, ph, b);
	}

	/* Slot backgrounds */
	for (i = 0; i < FURNACE_SLOT_COUNT; i++) {
		Furnace_GetSlotPos(s, i, &sx, &sy);
		SurvGUI_DrawSlot(sx, sy, cs, i == s->hoveredSlot);
	}

	/* Smelting arrow — half-size, centered horizontally in gap, aligned to flame row */
	{
		int gapX   = s->inputX + cs;
		int gapW   = s->outputX - gapX;
		int arrowW = gapW / 2;
		int arrowX = gapX + (gapW - arrowW) / 2;
		int arrowH = (int)(arrowW / ARROW_ASPECT);
		int arrowY = s->fuelBarY + (cs - arrowH) / 2;
		float prog = (Furnace_Active && Furnace_SmeltProgress > 0.0f) ? Furnace_SmeltProgress : 0.0f;
		SurvGUI_DrawArrow(arrowX, arrowY, arrowW, prog);
	}

	/* Flame indicator (replaces old fuel bar) */
	{
		float fuelPct = (Furnace_FuelBurnTotal > 0.0f && Furnace_FuelBurnLeft > 0.0f)
			? (Furnace_FuelBurnLeft / Furnace_FuelBurnTotal) : 0.0f;
		SurvGUI_DrawFlame(s->fuelBarX, s->fuelBarY, s->fuelBarW, fuelPct);
	}

	/* Build iso batch for slot contents */
	data = Screen_LockVb(s);
	{
		struct VertexTextured** ptr = &data;
		Widget_BuildMesh(&s->title, ptr);
	}

	IsometricDrawer_BeginBatch(data, s->isoState);
	for (i = 0; i < FURNACE_SLOT_COUNT; i++) {
		Furnace_GetSlot(i, &block, &itemId);
		if (block == BLOCK_AIR && itemId == ITEM_NONE) continue;

		Furnace_GetSlotPos(s, i, &sx, &sy);

		if (itemId != ITEM_NONE && itemId > 0 && itemId < ITEM_COUNT) {
			IsometricDrawer_AddItemBatch(ItemTextures[itemId], isoSize,
				(float)(sx + cs / 2), (float)(sy + cs / 2));
		} else if (block != BLOCK_AIR) {
			IsometricDrawer_AddBatch(block, isoSize,
				(float)(sx + cs / 2), (float)(sy + cs / 2));
		}
	}

	/* Held item at cursor */
	if (s->holding) {
		int mx = Pointers[0].x, my = Pointers[0].y;
		if (s->holdItem != ITEM_NONE && s->holdItem > 0 && s->holdItem < ITEM_COUNT) {
			IsometricDrawer_AddItemBatch(ItemTextures[s->holdItem], isoSize,
				(float)mx, (float)my);
		} else if (s->holdBlock != BLOCK_AIR) {
			IsometricDrawer_AddBatch(s->holdBlock, isoSize,
				(float)mx, (float)my);
		}
	}

	s->verticesCount = IsometricDrawer_EndBatch();
	Gfx_UnlockDynamicVb(s->vb);

	/* Render title text */
	Gfx_SetVertexFormat(VERTEX_FORMAT_TEXTURED);
	Gfx_BindDynamicVb(s->vb);
	Widget_Render2(&s->title, 0);

	/* Render iso batch */
	if (s->verticesCount) {
		IsometricDrawer_Render(s->verticesCount, TEXTWIDGET_MAX, s->isoState);
	}

	/* Render stack count numbers */
	for (i = 0; i < FURNACE_SLOT_COUNT; i++) {
		int cnt = Furnace_GetSlotCount(i);
		if (cnt > 1) {
			Furnace_GetSlotPos(s, i, &sx, &sy);
			SurvInv_RenderCount(cnt, sx, sy, cs);
		}
	}
	/* Held item count at cursor */
	if (s->holding && s->holdCount > 1) {
		int mx = Pointers[0].x, my = Pointers[0].y;
		SurvInv_RenderCount(s->holdCount, mx - cs / 2, my - cs / 2, cs);
	}
}

static int Furnace_KeyDown(void* screen, int key, struct InputDevice* device) {
	struct FurnaceScreen* s = (struct FurnaceScreen*)screen;

	if (s->releasedKey) {
		if (InputBind_Claims(BIND_INVENTORY, key, device) ||
			key == 'I' || key == 'E' || key == device->escapeButton) {
			Gui_Remove((struct Screen*)s);
			return true;
		}
	}

	/* Right-click handling */
	if (key == CCMOUSE_R && s->releasedMouse) {
		int slot, slotCount;
		BlockID slotBlock;
		int slotItem;
		int mx = Pointers[0].x, my = Pointers[0].y;

		slot = Furnace_HitTest(s, mx, my);
		if (slot == -1) return true;

		/* Block all right-click placement into output slot */
		if (slot == 38 && s->holding) return true;

		/* Only allow fuel items into fuel slot */
		if (slot == 37 && s->holding && !Furnace_IsFuel(s->holdBlock, s->holdItem)) return true;

		Furnace_GetSlot(slot, &slotBlock, &slotItem);
		slotCount = Furnace_GetSlotCount(slot);

		if (!s->holding) {
			if (slotBlock == BLOCK_AIR && slotItem == ITEM_NONE) return true;
			if (slotCount <= 1) {
				s->holding   = true;
				s->holdBlock = slotBlock;
				s->holdItem  = slotItem;
				s->holdCount = 1;
				Furnace_SetSlot(slot, BLOCK_AIR, ITEM_NONE);
				Furnace_SetSlotCount(slot, 0);
			} else {
				int takeCount = slotCount / 2;
				s->holding   = true;
				s->holdBlock = slotBlock;
				s->holdItem  = slotItem;
				s->holdCount = takeCount;
				Furnace_SetSlotCount(slot, slotCount - takeCount);
			}
		} else {
			if (slotBlock == BLOCK_AIR && slotItem == ITEM_NONE) {
				Furnace_SetSlot(slot, s->holdBlock, s->holdItem);
				Furnace_SetSlotCount(slot, 1);
				s->holdCount--;
				if (s->holdCount <= 0) {
					s->holding   = false;
					s->holdBlock = BLOCK_AIR;
					s->holdItem  = ITEM_NONE;
					s->holdCount = 0;
				}
			} else if (SurvInv_SameType(slotBlock, slotItem, s->holdBlock, s->holdItem) &&
					   slotCount < SurvInv_MaxStack(s->holdBlock, s->holdItem)) {
				Furnace_SetSlotCount(slot, slotCount + 1);
				s->holdCount--;
				if (s->holdCount <= 0) {
					s->holding   = false;
					s->holdBlock = BLOCK_AIR;
					s->holdItem  = ITEM_NONE;
					s->holdCount = 0;
				}
			}
		}

		if (slot >= 27 && slot < 36) {
			Event_RaiseVoid(&UserEvents.HeldBlockChanged);
		}
		s->dirty = true;
		return true;
	}

	return Elem_HandlesKeyDown(&HUDScreen_Instance.hotbar, key, device);
}

static void Furnace_KeyUp(void* screen, int key, struct InputDevice* device) {
	struct FurnaceScreen* s = (struct FurnaceScreen*)screen;
	if (InputBind_Claims(BIND_INVENTORY, key, device) ||
		key == 'I' || key == 'E') {
		s->releasedKey = true;
	}
	if (key == CCMOUSE_R) {
		s->releasedMouse = true;
	}
}

static int Furnace_PointerDown(void* screen, int id, int x, int y) {
	struct FurnaceScreen* s = (struct FurnaceScreen*)screen;
	int slot, slotCount;
	BlockID slotBlock;
	int slotItem;

	slot = Furnace_HitTest(s, x, y);

	if (slot == -1) {
		if (!s->holding) {
			Gui_Remove((struct Screen*)s);
		} else {
			/* Drop held item into the world */
			SurvInv_DropHeldItem(s->holdBlock, s->holdItem, s->holdCount);
			s->holding   = false;
			s->holdBlock = BLOCK_AIR;
			s->holdItem  = ITEM_NONE;
			s->holdCount = 0;
			s->dirty = true;
		}
		return TOUCH_TYPE_GUI;
	}

	/* Only allow fuel items into fuel slot */
	if (slot == 37 && s->holding && !Furnace_IsFuel(s->holdBlock, s->holdItem)) {
		return TOUCH_TYPE_GUI;
	}

	Furnace_GetSlot(slot, &slotBlock, &slotItem);
	slotCount = Furnace_GetSlotCount(slot);

	if (!s->holding) {
		if (slotBlock == BLOCK_AIR && slotItem == ITEM_NONE) return TOUCH_TYPE_GUI;
		s->holding   = true;
		s->holdBlock = slotBlock;
		s->holdItem  = slotItem;
		s->holdCount = slotCount;
		if (s->holdCount < 1) s->holdCount = 1; /* Normalize: creative mode items may have count=0 */
		Furnace_SetSlot(slot, BLOCK_AIR, ITEM_NONE);
		Furnace_SetSlotCount(slot, 0);
	} else {
		/* Output slot: only allow picking up of same type to add to held stack */
		if (slot == 38) {
			if (SurvInv_SameType(slotBlock, slotItem, s->holdBlock, s->holdItem) &&
				slotCount > 0) {
				int maxS = SurvInv_MaxStack(s->holdBlock, s->holdItem);
				int total = s->holdCount + slotCount;
				if (total <= maxS) {
					s->holdCount = total;
					Furnace_SetSlot(slot, BLOCK_AIR, ITEM_NONE);
					Furnace_SetSlotCount(slot, 0);
				}
			}
			goto furnace_done;
		}

		if (slotBlock == BLOCK_AIR && slotItem == ITEM_NONE) {
			Furnace_SetSlot(slot, s->holdBlock, s->holdItem);
			Furnace_SetSlotCount(slot, s->holdCount);
			s->holding   = false;
			s->holdBlock = BLOCK_AIR;
			s->holdItem  = ITEM_NONE;
			s->holdCount = 0;
		} else if (SurvInv_SameType(slotBlock, slotItem, s->holdBlock, s->holdItem)) {
			int maxS = SurvInv_MaxStack(s->holdBlock, s->holdItem);
			int total = slotCount + s->holdCount;
			if (total <= maxS) {
				Furnace_SetSlotCount(slot, total);
				s->holding   = false;
				s->holdBlock = BLOCK_AIR;
				s->holdItem  = ITEM_NONE;
				s->holdCount = 0;
			} else {
				Furnace_SetSlotCount(slot, maxS);
				s->holdCount = total - maxS;
			}
		} else {
			BlockID tmpBlock = slotBlock;
			int tmpItem      = slotItem;
			int tmpCount     = slotCount;
			Furnace_SetSlot(slot, s->holdBlock, s->holdItem);
			Furnace_SetSlotCount(slot, s->holdCount);
			s->holdBlock = tmpBlock;
			s->holdItem  = tmpItem;
			s->holdCount = tmpCount;
		}
	}

furnace_done:
	if (slot >= 27 && slot < 36) {
		Event_RaiseVoid(&UserEvents.HeldBlockChanged);
	}
	s->dirty = true;
	return TOUCH_TYPE_GUI;
}

static int Furnace_PointerMove(void* screen, int id, int x, int y) {
	struct FurnaceScreen* s = (struct FurnaceScreen*)screen;
	s->hoveredSlot = Furnace_HitTest(s, x, y);
	s->dirty = true;
	return s->hoveredSlot != -1;
}

static const struct ScreenVTABLE FurnaceScreen_VTABLE = {
	Furnace_Init,        Furnace_Update,      Furnace_Free,
	Furnace_Render,      Screen_BuildMesh,
	Furnace_KeyDown,     Furnace_KeyUp,        Screen_TKeyPress,     Screen_TText,
	Furnace_PointerDown, Screen_PointerUp,     Furnace_PointerMove,  Screen_TMouseScroll,
	Furnace_Layout,      Furnace_ContextLost,  Furnace_ContextRecreated
};

void FurnaceScreen_Show(void) {
	struct FurnaceScreen* s = &FurnaceScreen_Instance;
	s->grabsInput = true;
	s->closable   = true;

	s->VTABLE = &FurnaceScreen_VTABLE;
	Gui_Add((struct Screen*)s, GUI_PRIORITY_INVENTORY);
}


/*########################################################################################################################*
*-------------------------------------------------------ChestScreen-------------------------------------------------------*
*#########################################################################################################################*/
/* Slot layout:
 *  Single chest (63 total):
 *   0-26:  Main inventory (3x9)  -> SurvInv_Main[idx]
 *   27-35: Hotbar (9)            -> Inventory_Get/Set, Hotbar_GetItem/SetItem
 *   36-62: Chest slots (3x9)    -> Chest_Slots[idx-36]
 *
 *  Double chest (90 total):
 *   0-26:  Main inventory (3x9)  -> SurvInv_Main[idx]
 *   27-35: Hotbar (9)            -> Inventory_Get/Set, Hotbar_GetItem/SetItem
 *   36-89: Chest slots (6x9)    -> Chest_Slots[idx-36]
 */
#define CHEST_MAX_SLOT_COUNT 90
#define CHEST_ISO_VERTICES ((CHEST_MAX_SLOT_COUNT + 1) * ISOMETRICDRAWER_MAXVERTICES)

static struct ChestScreen {
	Screen_Body
	struct FontDesc font;
	struct TextWidget title;

	int cellSize;
	int gridX, gridY;      /* 3x9 main inv top-left */
	int hotbarY;           /* hotbar row Y */
	int chestX, chestY;   /* chest slots top-left */
	int chestRows;         /* 3 for single, 6 for double */
	int totalSlots;        /* 63 for single, 90 for double */

	cc_bool  holding;
	BlockID  holdBlock;
	int      holdItem;
	int      holdCount;

	int hoveredSlot;       /* -1 = none */

	int isoState[CHEST_ISO_VERTICES / 4];
	int verticesCount;

	cc_bool releasedKey;
	cc_bool releasedMouse;
	struct Widget* __widgets[1];
} ChestScreen_Instance CC_BIG_VAR;

static void Chest_GetSlot(int idx, BlockID* block, int* itemId) {
	if (idx < 27) {
		*block  = SurvInv_Main[idx].block;
		*itemId = SurvInv_Main[idx].itemId;
	} else if (idx < 36) {
		*block  = Inventory_Get(idx - 27);
		*itemId = Hotbar_GetItem(idx - 27);
	} else {
		*block  = Chest_Slots[idx - 36].block;
		*itemId = Chest_Slots[idx - 36].itemId;
	}
}

static void Chest_SetSlot(int idx, BlockID block, int itemId) {
	if (idx < 27) {
		SurvInv_Main[idx].block  = block;
		SurvInv_Main[idx].itemId = itemId;
	} else if (idx < 36) {
		Inventory_Set(idx - 27, block);
		Hotbar_SetItem(idx - 27, itemId);
	} else {
		Chest_Slots[idx - 36].block  = block;
		Chest_Slots[idx - 36].itemId = itemId;
	}
}

static int Chest_GetSlotCount(int idx) {
	if (idx < 27) {
		return SurvInv_Main[idx].count;
	} else if (idx < 36) {
		return Hotbar_GetCount(idx - 27);
	} else {
		return Chest_Slots[idx - 36].count;
	}
}

static void Chest_SetSlotCount(int idx, int count) {
	if (idx < 27) {
		SurvInv_Main[idx].count = count;
	} else if (idx < 36) {
		Hotbar_SetCount(idx - 27, count);
	} else {
		Chest_Slots[idx - 36].count = count;
	}
}

static void ChestScr_GetSlotPos(struct ChestScreen* s, int idx, int* x, int* y) {
	int cs = s->cellSize;
	if (idx < 27) {
		/* Main 3x9 grid */
		*x = s->gridX + (idx % 9) * cs;
		*y = s->gridY + (idx / 9) * cs;
	} else if (idx < 36) {
		/* Hotbar */
		*x = s->gridX + (idx - 27) * cs;
		*y = s->hotbarY;
	} else {
		/* Chest slots - grid of chestRows x 9 */
		int ci = idx - 36;
		*x = s->chestX + (ci % 9) * cs;
		*y = s->chestY + (ci / 9) * cs;
	}
}

static int ChestScr_HitTest(struct ChestScreen* s, int mx, int my) {
	int i, sx, sy;
	for (i = 0; i < s->totalSlots; i++) {
		ChestScr_GetSlotPos(s, i, &sx, &sy);
		if (Gui_Contains(sx, sy, s->cellSize, s->cellSize, mx, my)) return i;
	}
	return -1;
}

static void ChestScr_Layout(void* screen) {
	struct ChestScreen* s = (struct ChestScreen*)screen;
	float scale;
	int cs, gap, totalWidth, totalHeight, baseX, baseY;

	scale = Gui_GetInventoryScale();
	cs    = (int)(44.0f * Math_SqrtF(scale));
	if (cs < 20) cs = 20;
	s->cellSize = cs;
	gap = cs / 3;

	totalWidth  = cs * 9;
	totalHeight = cs * s->chestRows + gap + cs * 3 + gap + cs;
	baseX = (Window_UI.Width  - totalWidth)  / 2;
	baseY = (Window_UI.Height - totalHeight) / 2;

	/* Chest slots grid at top */
	s->chestX = baseX;
	s->chestY = baseY;

	/* Main 3x9 grid below chest */
	s->gridX = baseX;
	s->gridY = baseY + cs * s->chestRows + gap;

	/* Hotbar row */
	s->hotbarY = s->gridY + cs * 3 + gap;

	/* Title above everything */
	Widget_SetLocation(&s->title, ANCHOR_CENTRE, ANCHOR_MIN, 0, 0);
	s->title.yOffset = baseY - s->title.height - 12;
	Widget_Layout(&s->title);
}

static void ChestScr_ContextLost(void* screen) {
	struct ChestScreen* s = (struct ChestScreen*)screen;
	Font_Free(&s->font);
	Screen_ContextLost(s);
}

static void ChestScr_ContextRecreated(void* screen) {
	struct ChestScreen* s = (struct ChestScreen*)screen;
	Screen_UpdateVb(s);
	Gui_MakeBodyFont(&s->font);
	if (s->chestRows == 6) {
		TextWidget_SetConst(&s->title, "Large Chest", &s->font);
	} else {
		TextWidget_SetConst(&s->title, "Chest", &s->font);
	}
}

static void ChestScr_Init(void* screen) {
	struct ChestScreen* s = (struct ChestScreen*)screen;
	s->widgets     = s->__widgets;
	s->numWidgets  = 0;
	s->maxWidgets  = Array_Elems(s->__widgets);

	TextWidget_Add(s, &s->title);

	s->holding     = false;
	s->holdBlock   = BLOCK_AIR;
	s->holdItem    = ITEM_NONE;
	s->holdCount   = 0;
	s->hoveredSlot = -1;
	s->releasedKey   = false;
	s->releasedMouse = false;

	/* Determine single vs double chest */
	if (Chest_SlotCount > CHEST_SLOTS) {
		s->chestRows  = 6;
		s->totalSlots = 90; /* 27 + 9 + 54 */
	} else {
		s->chestRows  = 3;
		s->totalSlots = 63; /* 27 + 9 + 27 */
	}

	s->maxVertices = TEXTWIDGET_MAX + CHEST_ISO_VERTICES;
	inventoryScreenOpen = true;
}

static void ChestScr_Free(void* screen) {
	Chest_Close();
	inventoryScreenOpen = false;
}

static void ChestScr_Update(void* screen, float delta) {
	struct ChestScreen* s = (struct ChestScreen*)screen;
	s->dirty = true;
}

static void ChestScr_Render(void* screen, float delta) {
	struct ChestScreen* s = (struct ChestScreen*)screen;
	struct VertexTextured* data;
	int i, sx, sy, cs;
	BlockID block;
	int itemId;
	float isoSize;

	PackedCol bgCol = PackedCol_Make(0xc6, 0xc6, 0xc6, 255);

	cs      = s->cellSize;
	isoSize = cs * 0.38f;

	/* Background panel */
	{
		int px = s->chestX - 4, py = s->chestY - 4;
		int pw = cs * 9 + 8,    ph = s->hotbarY + cs - s->chestY + 8;
		int b  = 8;
		Gfx_Draw2DFlat(px, py, pw, ph, bgCol);
		SurvGUI_DrawBorder(px, py, pw, ph, b);
	}

	/* Slot backgrounds */
	for (i = 0; i < s->totalSlots; i++) {
		ChestScr_GetSlotPos(s, i, &sx, &sy);
		SurvGUI_DrawSlot(sx, sy, cs, i == s->hoveredSlot);
	}

	/* Build iso batch for slot contents */
	data = Screen_LockVb(s);
	{
		struct VertexTextured** ptr = &data;
		Widget_BuildMesh(&s->title, ptr);
	}

	IsometricDrawer_BeginBatch(data, s->isoState);
	for (i = 0; i < s->totalSlots; i++) {
		Chest_GetSlot(i, &block, &itemId);
		if (block == BLOCK_AIR && itemId == ITEM_NONE) continue;

		ChestScr_GetSlotPos(s, i, &sx, &sy);

		if (itemId != ITEM_NONE && itemId > 0 && itemId < ITEM_COUNT) {
			IsometricDrawer_AddItemBatch(ItemTextures[itemId], isoSize,
				(float)(sx + cs / 2), (float)(sy + cs / 2));
		} else if (block != BLOCK_AIR) {
			IsometricDrawer_AddBatch(block, isoSize,
				(float)(sx + cs / 2), (float)(sy + cs / 2));
		}
	}

	/* Held item at cursor */
	if (s->holding) {
		int mx = Pointers[0].x, my = Pointers[0].y;
		if (s->holdItem != ITEM_NONE && s->holdItem > 0 && s->holdItem < ITEM_COUNT) {
			IsometricDrawer_AddItemBatch(ItemTextures[s->holdItem], isoSize,
				(float)mx, (float)my);
		} else if (s->holdBlock != BLOCK_AIR) {
			IsometricDrawer_AddBatch(s->holdBlock, isoSize,
				(float)mx, (float)my);
		}
	}

	s->verticesCount = IsometricDrawer_EndBatch();
	Gfx_UnlockDynamicVb(s->vb);

	/* Render title text */
	Gfx_SetVertexFormat(VERTEX_FORMAT_TEXTURED);
	Gfx_BindDynamicVb(s->vb);
	Widget_Render2(&s->title, 0);

	/* Render iso batch */
	if (s->verticesCount) {
		IsometricDrawer_Render(s->verticesCount, TEXTWIDGET_MAX, s->isoState);
	}

	/* Render stack count numbers */
	for (i = 0; i < s->totalSlots; i++) {
		int cnt = Chest_GetSlotCount(i);
		if (cnt > 1) {
			ChestScr_GetSlotPos(s, i, &sx, &sy);
			SurvInv_RenderCount(cnt, sx, sy, cs);
		}
	}
	/* Held item count at cursor */
	if (s->holding && s->holdCount > 1) {
		int mx = Pointers[0].x, my = Pointers[0].y;
		SurvInv_RenderCount(s->holdCount, mx - cs / 2, my - cs / 2, cs);
	}
}

static int ChestScr_KeyDown(void* screen, int key, struct InputDevice* device) {
	struct ChestScreen* s = (struct ChestScreen*)screen;

	if (s->releasedKey) {
		if (InputBind_Claims(BIND_INVENTORY, key, device) ||
			key == 'I' || key == 'E' || key == device->escapeButton) {
			Gui_Remove((struct Screen*)s);
			return true;
		}
	}

	/* Right-click handling */
	if (key == CCMOUSE_R && s->releasedMouse) {
		int slot, slotCount;
		BlockID slotBlock;
		int slotItem;
		int mx = Pointers[0].x, my = Pointers[0].y;

		slot = ChestScr_HitTest(s, mx, my);
		if (slot == -1) return true;

		Chest_GetSlot(slot, &slotBlock, &slotItem);
		slotCount = Chest_GetSlotCount(slot);

		if (!s->holding) {
			if (slotBlock == BLOCK_AIR && slotItem == ITEM_NONE) return true;
			if (slotCount <= 1) {
				s->holding   = true;
				s->holdBlock = slotBlock;
				s->holdItem  = slotItem;
				s->holdCount = 1;
				Chest_SetSlot(slot, BLOCK_AIR, ITEM_NONE);
				Chest_SetSlotCount(slot, 0);
			} else {
				int takeCount = slotCount / 2;
				s->holding   = true;
				s->holdBlock = slotBlock;
				s->holdItem  = slotItem;
				s->holdCount = takeCount;
				Chest_SetSlotCount(slot, slotCount - takeCount);
			}
		} else {
			if (slotBlock == BLOCK_AIR && slotItem == ITEM_NONE) {
				Chest_SetSlot(slot, s->holdBlock, s->holdItem);
				Chest_SetSlotCount(slot, 1);
				s->holdCount--;
				if (s->holdCount <= 0) {
					s->holding   = false;
					s->holdBlock = BLOCK_AIR;
					s->holdItem  = ITEM_NONE;
					s->holdCount = 0;
				}
			} else if (SurvInv_SameType(slotBlock, slotItem, s->holdBlock, s->holdItem) &&
					   slotCount < SurvInv_MaxStack(s->holdBlock, s->holdItem)) {
				Chest_SetSlotCount(slot, slotCount + 1);
				s->holdCount--;
				if (s->holdCount <= 0) {
					s->holding   = false;
					s->holdBlock = BLOCK_AIR;
					s->holdItem  = ITEM_NONE;
					s->holdCount = 0;
				}
			}
		}

		if (slot >= 27 && slot < 36) {
			Event_RaiseVoid(&UserEvents.HeldBlockChanged);
		}
		s->dirty = true;
		return true;
	}

	return Elem_HandlesKeyDown(&HUDScreen_Instance.hotbar, key, device);
}

static void ChestScr_KeyUp(void* screen, int key, struct InputDevice* device) {
	struct ChestScreen* s = (struct ChestScreen*)screen;
	if (InputBind_Claims(BIND_INVENTORY, key, device) ||
		key == 'I' || key == 'E') {
		s->releasedKey = true;
	}
	if (key == CCMOUSE_R) {
		s->releasedMouse = true;
	}
}

static int ChestScr_PointerDown(void* screen, int id, int x, int y) {
	struct ChestScreen* s = (struct ChestScreen*)screen;
	int slot, slotCount;
	BlockID slotBlock;
	int slotItem;

	slot = ChestScr_HitTest(s, x, y);

	if (slot == -1) {
		if (!s->holding) {
			Gui_Remove((struct Screen*)s);
		} else {
			/* Drop held item into the world */
			SurvInv_DropHeldItem(s->holdBlock, s->holdItem, s->holdCount);
			s->holding   = false;
			s->holdBlock = BLOCK_AIR;
			s->holdItem  = ITEM_NONE;
			s->holdCount = 0;
			s->dirty = true;
		}
		return TOUCH_TYPE_GUI;
	}

	Chest_GetSlot(slot, &slotBlock, &slotItem);
	slotCount = Chest_GetSlotCount(slot);

	if (!s->holding) {
		if (slotBlock == BLOCK_AIR && slotItem == ITEM_NONE) return TOUCH_TYPE_GUI;
		s->holding   = true;
		s->holdBlock = slotBlock;
		s->holdItem  = slotItem;
		s->holdCount = slotCount;
		if (s->holdCount < 1) s->holdCount = 1;
		Chest_SetSlot(slot, BLOCK_AIR, ITEM_NONE);
		Chest_SetSlotCount(slot, 0);
	} else {
		if (slotBlock == BLOCK_AIR && slotItem == ITEM_NONE) {
			Chest_SetSlot(slot, s->holdBlock, s->holdItem);
			Chest_SetSlotCount(slot, s->holdCount);
			s->holding   = false;
			s->holdBlock = BLOCK_AIR;
			s->holdItem  = ITEM_NONE;
			s->holdCount = 0;
		} else if (SurvInv_SameType(slotBlock, slotItem, s->holdBlock, s->holdItem)) {
			int maxS = SurvInv_MaxStack(s->holdBlock, s->holdItem);
			int total = slotCount + s->holdCount;
			if (total <= maxS) {
				Chest_SetSlotCount(slot, total);
				s->holding   = false;
				s->holdBlock = BLOCK_AIR;
				s->holdItem  = ITEM_NONE;
				s->holdCount = 0;
			} else {
				Chest_SetSlotCount(slot, maxS);
				s->holdCount = total - maxS;
			}
		} else {
			BlockID tmpBlock = slotBlock;
			int tmpItem      = slotItem;
			int tmpCount     = slotCount;
			Chest_SetSlot(slot, s->holdBlock, s->holdItem);
			Chest_SetSlotCount(slot, s->holdCount);
			s->holdBlock = tmpBlock;
			s->holdItem  = tmpItem;
			s->holdCount = tmpCount;
		}
	}

	if (slot >= 27 && slot < 36) {
		Event_RaiseVoid(&UserEvents.HeldBlockChanged);
	}
	s->dirty = true;
	return TOUCH_TYPE_GUI;
}

static int ChestScr_PointerMove(void* screen, int id, int x, int y) {
	struct ChestScreen* s = (struct ChestScreen*)screen;
	s->hoveredSlot = ChestScr_HitTest(s, x, y);
	s->dirty = true;
	return s->hoveredSlot != -1;
}

static const struct ScreenVTABLE ChestScreen_VTABLE = {
	ChestScr_Init,        ChestScr_Update,      ChestScr_Free,
	ChestScr_Render,      Screen_BuildMesh,
	ChestScr_KeyDown,     ChestScr_KeyUp,        Screen_TKeyPress,     Screen_TText,
	ChestScr_PointerDown, Screen_PointerUp,      ChestScr_PointerMove, Screen_TMouseScroll,
	ChestScr_Layout,      ChestScr_ContextLost,  ChestScr_ContextRecreated
};

void ChestScreen_Show(void) {
	struct ChestScreen* s = &ChestScreen_Instance;
	s->grabsInput = true;
	s->closable   = true;

	s->VTABLE = &ChestScreen_VTABLE;
	Gui_Add((struct Screen*)s, GUI_PRIORITY_INVENTORY);
}


/*########################################################################################################################*
*------------------------------------------------------LoadingScreen------------------------------------------------------*
*#########################################################################################################################*/
static struct LoadingScreen {
	Screen_Body
	struct FontDesc font;
	float progress; 
	int rows;
	
	int progX, progY, progWidth, progHeight;
	struct TextWidget title, message;
	cc_string titleStr, messageStr;
	const char* lastState;

	char _titleBuffer[STRING_SIZE];
	char _messageBuffer[STRING_SIZE];
	struct Widget* __widgets[2];
} LoadingScreen CC_BIG_VAR;
#define LOADING_TILE_SIZE 64

static void LoadingScreen_SetTitle(struct LoadingScreen* s) {
	TextWidget_Set(&s->title, &s->titleStr, &s->font);
	s->dirty = true;
}
static void LoadingScreen_SetMessage(struct LoadingScreen* s) {
	TextWidget_Set(&s->message, &s->messageStr, &s->font);
	s->dirty = true;
}

static void LoadingScreen_CalcMaxVertices(struct LoadingScreen* s) {
	s->rows = Math_CeilDiv(Window_UI.Height, LOADING_TILE_SIZE);
	s->maxVertices = Screen_CalcDefaultMaxVertices(s) + s->rows * 4;
}

static void LoadingScreen_Layout(void* screen) {
	struct LoadingScreen* s = (struct LoadingScreen*)screen;
	int oldRows, y;
	Widget_SetLocation(&s->title,   ANCHOR_CENTRE, ANCHOR_CENTRE, 0, -31);
	Widget_SetLocation(&s->message, ANCHOR_CENTRE, ANCHOR_CENTRE, 0,  17);
	y = Display_ScaleY(34);

	s->progWidth  = Display_ScaleX(200);
	s->progX      = Gui_CalcPos(ANCHOR_CENTRE, 0, s->progWidth,  Window_UI.Width);
	s->progHeight = Display_ScaleY(4);
	s->progY      = Gui_CalcPos(ANCHOR_CENTRE, y, s->progHeight, Window_UI.Height);

	oldRows = s->rows;
	LoadingScreen_CalcMaxVertices(s);
	if (oldRows == s->rows) return;
	Screen_UpdateVb(s);
}

static void LoadingScreen_ContextLost(void* screen) {
	struct LoadingScreen* s = (struct LoadingScreen*)screen;
	Font_Free(&s->font);
	Screen_ContextLost(screen);
}

static void LoadingScreen_ContextRecreated(void* screen) {
	struct LoadingScreen* s = (struct LoadingScreen*)screen;
	Gui_MakeBodyFont(&s->font);
	LoadingScreen_SetTitle(s);
	LoadingScreen_SetMessage(s);
	Screen_UpdateVb(s);
}

static void LoadingScreen_BuildMesh(void* screen) {
	struct LoadingScreen* s = (struct LoadingScreen*)screen;
	struct VertexTextured* data;
	struct VertexTextured** ptr;
	struct Texture tex;
	TextureLoc loc;
	int atlasIndex, i;

	data = Screen_LockVb(s);
	ptr  = &data;

	loc       = Block_Tex(BLOCK_DIRT, FACE_YMAX);
	Tex_SetRect(tex, 0,0, Window_UI.Width,LOADING_TILE_SIZE);
	tex.uv    = Atlas1D_TexRec(loc, 1, &atlasIndex);
	tex.uv.u2 = (float)Window_UI.Width / LOADING_TILE_SIZE;
	
	for (i = 0; i < s->rows; i++) {
		tex.y = i * LOADING_TILE_SIZE;
		Gfx_Make2DQuad(&tex, PackedCol_Make(64, 64, 64, 255), ptr);
	}

	Widget_BuildMesh(&s->title,   ptr);
	Widget_BuildMesh(&s->message, ptr);
	Gfx_UnlockDynamicVb(s->vb);
}

static void LoadingScreen_MapLoading(void* screen, float progress) {
	((struct LoadingScreen*)screen)->progress = progress;
}

static void LoadingScreen_MapLoaded(void* screen) {
	Gui_Remove((struct Screen*)screen);
}

static void LoadingScreen_Init(void* screen) {
	struct LoadingScreen* s = (struct LoadingScreen*)screen;
	s->widgets     = s->__widgets;
	s->numWidgets  = 0;
	s->maxWidgets  = Array_Elems(s->__widgets);

	TextWidget_Add(s, &s->title);
	TextWidget_Add(s, &s->message);

	LoadingScreen_CalcMaxVertices(s);
	Gfx_SetFog(false);
	Event_Register_(&WorldEvents.Loading,   s, LoadingScreen_MapLoading);
	Event_Register_(&WorldEvents.MapLoaded, s, LoadingScreen_MapLoaded);
}

static void LoadingScreen_Render(void* screen, float delta) {
	struct LoadingScreen* s = (struct LoadingScreen*)screen;
	int offset, filledWidth;
	TextureLoc loc;

	Gfx_SetVertexFormat(VERTEX_FORMAT_TEXTURED);
	Gfx_BindDynamicVb(s->vb);

	/* Draw background dirt */
	offset = 0;
	if (s->rows) {
		loc = Block_Tex(BLOCK_DIRT, FACE_YMAX);
		Atlas1D_Bind(Atlas1D_Index(loc));
		Gfx_DrawVb_IndexedTris_Range(s->rows * 4, 0, DRAW_HINT_SPRITE);
		offset = s->rows * 4;
	}

	offset = Widget_Render2(&s->title,   offset);
	offset = Widget_Render2(&s->message, offset);

	filledWidth = (int)(s->progWidth * s->progress);
	Gfx_Draw2DFlat(s->progX, s->progY, s->progWidth, 
					s->progHeight, PackedCol_Make(128, 128, 128, 255));
	Gfx_Draw2DFlat(s->progX, s->progY, filledWidth,  
					s->progHeight, PackedCol_Make(128, 255, 128, 255));
}

static void LoadingScreen_Free(void* screen) {
	struct LoadingScreen* s = (struct LoadingScreen*)screen;
	Event_Unregister_(&WorldEvents.Loading,   s, LoadingScreen_MapLoading);
	Event_Unregister_(&WorldEvents.MapLoaded, s, LoadingScreen_MapLoaded);
}

CC_NOINLINE static void LoadingScreen_ShowCommon(const cc_string* title, const cc_string* message) {
	struct LoadingScreen* s = &LoadingScreen;
	s->lastState = NULL;
	s->progress  = 0.0f;

	String_InitArray(s->titleStr,   s->_titleBuffer);
	String_AppendString(&s->titleStr,   title);
	String_InitArray(s->messageStr, s->_messageBuffer);
	String_AppendString(&s->messageStr, message);
	
	s->grabsInput  = true;
	s->blocksWorld = true;
	Gui_Add((struct Screen*)s, 
		Game_ClassicMode ? GUI_PRIORITY_OLDLOADING : GUI_PRIORITY_LOADING);
}

static const struct ScreenVTABLE LoadingScreen_VTABLE = {
	LoadingScreen_Init,   Screen_NullUpdate, LoadingScreen_Free, 
	LoadingScreen_Render, LoadingScreen_BuildMesh,
	Screen_TInput,        Screen_InputUp,    Screen_TKeyPress,   Screen_TText,
	Screen_TPointer,      Screen_PointerUp,  Screen_TPointer,    Screen_TMouseScroll,
	LoadingScreen_Layout, LoadingScreen_ContextLost, LoadingScreen_ContextRecreated
};
void LoadingScreen_Show(const cc_string* title, const cc_string* message) {
	LoadingScreen.VTABLE = &LoadingScreen_VTABLE;
	LoadingScreen_ShowCommon(title, message);
}


/*########################################################################################################################*
*--------------------------------------------------GeneratingMapScreen----------------------------------------------------*
*#########################################################################################################################*/
static void GeneratingScreen_AtlasChanged(void* obj) {
	LoadingScreen.dirty = true; /* Dirt texture may have changed */
}

static void GeneratingScreen_Init(void* screen) {
	LoadingScreen_Init(screen);
	Event_Register_(&TextureEvents.AtlasChanged,   NULL, GeneratingScreen_AtlasChanged);
}
static void GeneratingScreen_Free(void* screen) {
	LoadingScreen_Free(screen);
	Event_Unregister_(&TextureEvents.AtlasChanged, NULL, GeneratingScreen_AtlasChanged);
}

static void GeneratingScreen_EndGeneration(void) {
	struct LocationUpdate update;
	World_SetNewMap(Gen_Blocks, World.Width, World.Height, World.Length);
	if (!Gen_Blocks) { Chat_AddRaw("&cFailed to generate the map."); return; }

	/* Let the generator configure environment (e.g. floating islands hide borders) */
	if (gen_active && gen_active->Setup) gen_active->Setup();

	Gen_Blocks = NULL;
	if (Gen_SpawnOverride.y >= 0) {
		update.flags = LU_HAS_POS | LU_HAS_YAW | LU_HAS_PITCH;
		update.pos   = Gen_SpawnOverride;
		update.yaw   = 0.0f;
		update.pitch = 0.0f;
		Gen_SpawnOverride.y = -1.0f;
	} else {
		LocalPlayer_CalcDefaultSpawn(Entities.CurPlayer, &update);
	}
	LocalPlayers_MoveToSpawn(&update);
}

static void GeneratingScreen_Update(void* screen, float delta) {
	struct LoadingScreen* s    = (struct LoadingScreen*)screen;
	const char* state = (const char*)Gen_CurrentState;
	if (state == s->lastState) return;
	s->lastState = state;

	s->messageStr.length = 0;
	String_AppendConst(&s->messageStr, state);
	LoadingScreen_SetMessage(s);
}

static void GeneratingScreen_Render(void* screen, float delta) {
	struct LoadingScreen* s = (struct LoadingScreen*)screen;
	s->progress = Gen_CurrentProgress;
	LoadingScreen_Render(s, delta);
	if (Gen_IsDone()) GeneratingScreen_EndGeneration();
}

static const struct ScreenVTABLE GeneratingScreen_VTABLE = {
	GeneratingScreen_Init,   GeneratingScreen_Update, GeneratingScreen_Free,
	GeneratingScreen_Render, LoadingScreen_BuildMesh,
	Screen_TInput,           Screen_InputUp,    Screen_TKeyPress,   Screen_TText,
	Screen_TPointer,         Screen_PointerUp,  Screen_FPointer,    Screen_TMouseScroll,
	LoadingScreen_Layout, LoadingScreen_ContextLost, LoadingScreen_ContextRecreated
};
void GeneratingScreen_Show(void) {
	static const cc_string title   = String_FromConst("Generating level");
	static const cc_string message = String_FromConst("Generating..");

	LoadingScreen.VTABLE = &GeneratingScreen_VTABLE;
	LoadingScreen_ShowCommon(&title, &message);
}


/*########################################################################################################################*
*------------------------------------------------------DeathScreen--------------------------------------------------------*
*#########################################################################################################################*/
static struct DeathScreen {
	Screen_Body
	struct ButtonWidget genBtn, loadBtn;
	struct FontDesc titleFont;
	struct TextWidget title;
	struct Widget* __widgets[3];
} DeathScreen;

static void DeathScreen_Layout(void* screen) {
	struct DeathScreen* s = (struct DeathScreen*)screen;
	Widget_SetLocation(&s->title,  ANCHOR_CENTRE, ANCHOR_CENTRE, 0, -60);
	Widget_SetLocation(&s->genBtn, ANCHOR_CENTRE, ANCHOR_CENTRE, 0,  10);
	Widget_SetLocation(&s->loadBtn,ANCHOR_CENTRE, ANCHOR_CENTRE, 0,  60);
}

static void DeathScreen_ContextLost(void* screen) {
	struct DeathScreen* s = (struct DeathScreen*)screen;
	Font_Free(&s->titleFont);
	Screen_ContextLost(screen);
}

static void DeathScreen_ContextRecreated(void* screen) {
	struct DeathScreen* s = (struct DeathScreen*)screen;
	Screen_UpdateVb(screen);
	Gui_MakeTitleFont(&s->titleFont);
	TextWidget_SetConst(&s->title, "Game over!", &s->titleFont);
	ButtonWidget_SetConst(&s->genBtn,  "Generate new level", &s->titleFont);
	ButtonWidget_SetConst(&s->loadBtn, "Load level",         &s->titleFont);
}

static void DeathScreen_OnGenLevel(void* s, void* w) {
	Gui_Remove((struct Screen*)s);
	GenLevelScreen_Show();
}

static void DeathScreen_OnLoadLevel(void* s, void* w) {
	Gui_Remove((struct Screen*)s);
	LoadLevelScreen_Show();
}

static void DeathScreen_Init(void* screen) {
	struct DeathScreen* s = (struct DeathScreen*)screen;
	s->widgets     = s->__widgets;
	s->numWidgets  = 0;
	s->maxWidgets  = Array_Elems(s->__widgets);

	TextWidget_Add(s, &s->title);
	ButtonWidget_Add(s, &s->genBtn,  300, DeathScreen_OnGenLevel);
	ButtonWidget_Add(s, &s->loadBtn, 300, DeathScreen_OnLoadLevel);

	s->maxVertices = Screen_CalcDefaultMaxVertices(s);
}

static void DeathScreen_Render(void* screen, float delta) {
	PackedCol red = PackedCol_Make(70, 0, 0, 200);
	Gfx_Draw2DGradient(0, 0, Window_UI.Width, Window_UI.Height, red, red);
	Screen_Render2Widgets(screen, delta);
}

static void DeathScreen_Free(void* screen) { }

static const struct ScreenVTABLE DeathScreen_VTABLE = {
	DeathScreen_Init,    Screen_NullUpdate,  DeathScreen_Free,
	DeathScreen_Render,  Screen_BuildMesh,
	Menu_InputDown,      Screen_InputUp,     Screen_TKeyPress, Screen_TText,
	Menu_PointerDown,    Screen_PointerUp,   Menu_PointerMove, Screen_TMouseScroll,
	DeathScreen_Layout,  DeathScreen_ContextLost, DeathScreen_ContextRecreated
};

void DeathScreen_Show(void) {
	struct DeathScreen* s = &DeathScreen;

	s->grabsInput  = true;
	s->blocksWorld = false;
	s->VTABLE      = &DeathScreen_VTABLE;

	Gui_Add((struct Screen*)s, GUI_PRIORITY_DISCONNECT);
}


/*########################################################################################################################*
*----------------------------------------------------SignEditScreen-------------------------------------------------------*
*#########################################################################################################################*/
/* Modelled after Minecraft Alpha 1.0.6 GuiEditSign:
   - Dark semi-transparent overlay background
   - "Edit sign message:" title (white with shadow) centered at top
   - 4 lines of text rendered centered, active line flanked by "> " and " <"
   - Blinking cursor markers (toggle every ~6 ticks / 0.3s)
   - "Done" button at bottom
   - Up/Down/Enter to change lines, Backspace to delete, Esc or Done to save & close
   - Max 15 chars per line, printable ASCII only */
static struct SignEditScreen {
	Screen_Body
	int signX, signY, signZ;   /* world position of sign being edited */
	int editLine;              /* currently active line (0-3) */
	int updateCounter;         /* tick counter for cursor blink (every 6 ticks = 0.3s) */
	cc_bool showCursor;
	float tickAccum;           /* accumulates delta for tick counting */
	struct FontDesc titleFont;
	struct FontDesc lineFont;
	char lines[4][16];         /* local text copy, null-terminated, max 15 chars */
	struct ButtonWidget doneBtn;
	struct Widget* __widgets[1];
} SignEditScreen_Instance CC_BIG_VAR;

/* Write local lines[] back to Signs[] and invalidate cached texture */
static void SignEdit_Commit(struct SignEditScreen* s) {
	int idx = Sign_FindAt(s->signX, s->signY, s->signZ);
	if (idx >= 0) {
		int i;
		for (i = 0; i < 4; i++)
			Mem_Copy(Signs[idx].lines[i], s->lines[i], 16);
		Signs_InvalidateAt(s->signX, s->signY, s->signZ);
	}
}

static void SignEdit_DoneClick(void* s, void* w) {
	struct SignEditScreen* scr = (struct SignEditScreen*)s;
	SignEdit_Commit(scr);
	Gui_Remove((struct Screen*)scr);
}

static void SignEdit_Init(void* screen) {
	struct SignEditScreen* s = (struct SignEditScreen*)screen;
	int idx = Sign_FindAt(s->signX, s->signY, s->signZ);
	int i;
	s->widgets     = s->__widgets;
	s->numWidgets  = 0;
	s->maxWidgets  = Array_Elems(s->__widgets);
	s->maxVertices = BUTTONWIDGET_MAX;
	s->editLine    = 0;
	s->updateCounter = 0;
	s->showCursor  = true;
	s->tickAccum   = 0.0f;
	for (i = 0; i < 4; i++) {
		if (idx >= 0) Mem_Copy(s->lines[i], Signs[idx].lines[i], 16);
		else          s->lines[i][0] = '\0';
	}
	Gui_MakeTitleFont(&s->titleFont);
	Font_Make(&s->lineFont, 16, FONT_FLAGS_NONE);

	ButtonWidget_Add(s, &s->doneBtn, 200, SignEdit_DoneClick);

	s->maxVertices = Screen_CalcDefaultMaxVertices(s);
}

static void SignEdit_Free(void* screen) {
	struct SignEditScreen* s = (struct SignEditScreen*)screen;
	SignEdit_Commit(s);
	Font_Free(&s->titleFont);
	Font_Free(&s->lineFont);
}

static void SignEdit_Update(void* screen, float delta) {
	struct SignEditScreen* s = (struct SignEditScreen*)screen;
	/* Alpha blinks every 6 ticks at 20 TPS = 0.3 seconds */
	s->tickAccum += delta;
	while (s->tickAccum >= 0.05f) {
		s->tickAccum -= 0.05f;
		s->updateCounter++;
	}
	{
		cc_bool newShow = (s->updateCounter / 6) % 2 == 0;
		if (newShow != s->showCursor) {
			s->showCursor = newShow;
			s->dirty      = true;
		}
	}
}

static void SignEdit_BuildMesh(void* screen) {
	struct SignEditScreen* s = (struct SignEditScreen*)screen;
	Screen_BuildMesh(screen);
	(void)s;
}

static void SignEdit_Render(void* screen, float delta) {
	struct SignEditScreen* s = (struct SignEditScreen*)screen;
	/* Dark semi-transparent overlay like Alpha's drawDefaultBackground */
	PackedCol overlayTop    = PackedCol_Make(0x10, 0x10, 0x10, 0x60);
	PackedCol overlayBottom = PackedCol_Make(0x10, 0x10, 0x10, 0xA0);
	struct Texture lineTex = { 0 };
	struct Texture signTex = { 0 };
	struct DrawTextArgs args;
	char strBuf[32];
	cc_string str;
	int scrW = Window_Main.Width, scrH = Window_Main.Height;
	int i, len;
	int signW, signH, signX, signY;

	/* Full-screen dark gradient overlay */
	Gfx_SetAlphaBlending(true);
	Gfx_Draw2DGradient(0, 0, scrW, scrH, overlayTop, overlayBottom);

	/* Draw wood sign face behind the text lines */
	{
		struct Context2D ctx;
		struct Bitmap* atlas = &Atlas2D.Bmp;
		int tileSize = Atlas2D.TileSize;
		int srcX = Atlas2D_TileX(4) * tileSize;
		int srcY = Atlas2D_TileY(4) * tileSize;
		int dx, dy, sx, sy;
		/* Sign face proportions: wider than tall, like Minecraft's sign model */
		int texW = 128, texH = 64;

		Context2D_Alloc(&ctx, texW, texH);
		if (atlas->scan0 && tileSize > 0) {
			struct Bitmap* dst = (struct Bitmap*)&ctx;
			for (dy = 0; dy < texH; dy++) {
				BitmapCol* dstRow = Bitmap_GetRow(dst, dy);
				sy = srcY + (dy * tileSize / texH);
				if (sy >= atlas->height) continue;
				for (dx = 0; dx < texW; dx++) {
					sx = srcX + (dx * tileSize / texW);
					if (sx >= atlas->width) continue;
					dstRow[dx] = Bitmap_GetRow(atlas, sy)[sx];
				}
			}
		} else {
			Context2D_Clear(&ctx, BitmapCol_Make(70, 43, 10, 255), 0, 0, texW, texH);
		}
		Context2D_MakeTexture(&signTex, &ctx);
		Context2D_Free(&ctx);

		/* Fixed base size (192x96) scaled by DPI, matching other UI elements */
		signW = Display_ScaleX(192);
		signH = Display_ScaleY(96);
		signX = (scrW - signW) / 2;
		signY = (scrH - signH) / 2 - Display_ScaleY(30);
		signTex.x = (short)signX;
		signTex.y = (short)signY;
		signTex.width  = (short)signW;
		signTex.height = (short)signH;
		Texture_Render(&signTex);
		Gfx_DeleteTexture(&signTex.ID);
	}

	/* Title: "Edit sign message:" centered above the sign board */
	{
		static const cc_string title = String_FromConst("Edit sign message:");
		DrawTextArgs_Make(&args, &title, &s->titleFont, true);
		Drawer2D_MakeTextTexture(&lineTex, &args);
		lineTex.x = (short)((scrW - lineTex.width) / 2);
		lineTex.y = (short)Display_ScaleY(40);
		Texture_Render(&lineTex);
		Gfx_DeleteTexture(&lineTex.ID);
	}

	/* 4 sign lines centered on the sign face, black text no shadow */
	signW = Display_ScaleX(192);
	signH = Display_ScaleY(96);
	signX = (scrW - signW) / 2;
	signY = (scrH - signH) / 2 - Display_ScaleY(30);
	for (i = 0; i < 4; i++) {
		/* Even vertical distribution: small top padding + equal spacing within board */
		int lineY = signY + signH / 8 + i * (signH * 3 / 16);
		Mem_Set(strBuf, 0, sizeof(strBuf));

		/* Prepend &0 for black color */
		strBuf[0] = '&'; strBuf[1] = '0';

		if (i == s->editLine && s->showCursor) {
			/* Build "&0> text <" display for active line */
			strBuf[2] = '>'; strBuf[3] = ' ';
			Mem_Copy(strBuf + 4, s->lines[i], String_CalcLen(s->lines[i], 16));
			len = 4 + String_CalcLen(s->lines[i], 16);
			strBuf[len] = ' '; strBuf[len + 1] = '<'; strBuf[len + 2] = '\0';
		} else if (i == s->editLine) {
			/* Active line without cursor markers (blink off) */
			Mem_Copy(strBuf + 2, s->lines[i], String_CalcLen(s->lines[i], 16));
			strBuf[2 + String_CalcLen(s->lines[i], 16)] = '\0';
		} else {
			/* Inactive line */
			Mem_Copy(strBuf + 2, s->lines[i], String_CalcLen(s->lines[i], 16));
			strBuf[2 + String_CalcLen(s->lines[i], 16)] = '\0';
		}

		str = String_FromReadonly(strBuf);
		if (str.length <= 2) continue; /* skip if only "&0" with no actual text */
		DrawTextArgs_Make(&args, &str, &s->lineFont, false);
		Drawer2D_MakeTextTexture(&lineTex, &args);
		lineTex.x = (short)((scrW - lineTex.width) / 2);
		lineTex.y = (short)lineY;
		Texture_Render(&lineTex);
		Gfx_DeleteTexture(&lineTex.ID);
	}

	/* Render all widgets (Done button) the standard menu way */
	Screen_Render2Widgets(screen, delta);

	Gfx_SetAlphaBlending(false);
}

static int SignEdit_KeyDown(void* screen, int key, struct InputDevice* device) {
	struct SignEditScreen* s = (struct SignEditScreen*)screen;
	if (key == CCKEY_ESCAPE || key == device->escapeButton) {
		SignEdit_Commit(s);
		Gui_Remove((struct Screen*)s);
		return true;
	}
	if (key == CCKEY_ENTER || key == CCKEY_KP_ENTER || key == CCKEY_DOWN) {
		s->editLine = (s->editLine + 1) & 3;
		s->dirty = true;
		return true;
	}
	if (key == CCKEY_UP) {
		s->editLine = (s->editLine - 1) & 3;
		s->dirty = true;
		return true;
	}
	if (key == CCKEY_BACKSPACE) {
		int len = String_CalcLen(s->lines[s->editLine], 16);
		if (len > 0) { s->lines[s->editLine][len - 1] = '\0'; s->dirty = true; }
		return true;
	}
	return true; /* consume all keys while sign editor is open */
}

static int SignEdit_KeyPress(void* screen, char keyChar) {
	struct SignEditScreen* s = (struct SignEditScreen*)screen;
	int len = String_CalcLen(s->lines[s->editLine], 16);
	if (keyChar >= 32 && keyChar < 127 && len < 15) {
		s->lines[s->editLine][len]     = keyChar;
		s->lines[s->editLine][len + 1] = '\0';
		s->dirty = true;
	}
	return true;
}

static int SignEdit_Text(void* screen, const cc_string* str) {
	struct SignEditScreen* s = (struct SignEditScreen*)screen;
	int i, len = String_CalcLen(s->lines[s->editLine], 16);
	for (i = 0; i < str->length && len < 15; i++, len++) {
		char c = str->buffer[i];
		if (c >= 32 && c < 127) {
			s->lines[s->editLine][len]     = c;
			s->lines[s->editLine][len + 1] = '\0';
		}
	}
	s->dirty = true;
	return true;
}

static void SignEdit_Layout(void* screen) {
	struct SignEditScreen* s = (struct SignEditScreen*)screen;
	/* "Done" button centered at bottom area */
	Widget_SetLocation(&s->doneBtn, ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 120);
}

static void SignEdit_ContextLost(void* screen) {
	struct SignEditScreen* s = (struct SignEditScreen*)screen;
	Font_Free(&s->titleFont);
	Font_Free(&s->lineFont);
	Screen_ContextLost(screen);
}

static void SignEdit_ContextRecreated(void* screen) {
	struct SignEditScreen* s = (struct SignEditScreen*)screen;
	Screen_UpdateVb(screen);
	Gui_MakeTitleFont(&s->titleFont);
	Font_Make(&s->lineFont, 16, FONT_FLAGS_NONE);
	ButtonWidget_SetConst(&s->doneBtn, "Done", &s->titleFont);
	SignEdit_Layout(screen);
}

static const struct ScreenVTABLE SignEditScreen_VTABLE = {
	SignEdit_Init,          SignEdit_Update,      SignEdit_Free,
	SignEdit_Render,        SignEdit_BuildMesh,
	SignEdit_KeyDown,       Screen_InputUp,       SignEdit_KeyPress, SignEdit_Text,
	Menu_PointerDown,       Screen_PointerUp,     Menu_PointerMove, Screen_TMouseScroll,
	SignEdit_Layout,        SignEdit_ContextLost,  SignEdit_ContextRecreated
};

void SignEditScreen_Show(int x, int y, int z) {
	struct SignEditScreen* s = &SignEditScreen_Instance;
	s->signX      = x;
	s->signY      = y;
	s->signZ      = z;
	s->grabsInput = true;
	s->closable   = true;
	s->VTABLE     = &SignEditScreen_VTABLE;
	Gui_Add((struct Screen*)s, GUI_PRIORITY_INVENTORY);
}


/*########################################################################################################################*
*----------------------------------------------------DisconnectScreen-----------------------------------------------------*
*#########################################################################################################################*/
static struct DisconnectScreen {
	Screen_Body
	float delayLeft;
	cc_bool canReconnect, lastActive;
	int lastSecsLeft;
	struct ButtonWidget reconnect, quit;

	struct FontDesc titleFont, messageFont;
	struct TextWidget title, message;
	char _titleBuffer[STRING_SIZE * 2];
	char _messageBuffer[STRING_SIZE];
	cc_string titleStr, messageStr;
	struct Widget* __widgets[4];
} DisconnectScreen CC_BIG_VAR;

#define DISCONNECT_DELAY_SECS 5

static void DisconnectScreen_Layout(void* screen) {
	struct DisconnectScreen* s = (struct DisconnectScreen*)screen;
	Widget_SetLocation(&s->title,     ANCHOR_CENTRE, ANCHOR_CENTRE, 0, -30);
	Widget_SetLocation(&s->message,   ANCHOR_CENTRE, ANCHOR_CENTRE, 0,  10);
	Widget_SetLocation(&s->reconnect, ANCHOR_CENTRE, ANCHOR_CENTRE, 0,  80);
	Widget_SetLocation(&s->quit,      ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 130);
}

static void DisconnectScreen_UpdateReconnect(struct DisconnectScreen* s) {
	cc_string msg; char msgBuffer[STRING_SIZE];
	int secsLeft;
	String_InitArray(msg, msgBuffer);

	if (s->canReconnect) {
		secsLeft = Math_Ceil(s->delayLeft);

		if (secsLeft > 0) {
			String_Format1(&msg, "Reconnect in %i", &secsLeft);
		}
		Widget_SetDisabled(&s->reconnect, secsLeft > 0);
	}

	if (!msg.length) String_AppendConst(&msg, "Reconnect");
	ButtonWidget_Set(&s->reconnect, &msg, &s->titleFont);
}

static void DisconnectScreen_ContextLost(void* screen) {
	struct DisconnectScreen* s = (struct DisconnectScreen*)screen;
	Font_Free(&s->titleFont);
	Font_Free(&s->messageFont);
	Screen_ContextLost(screen);
}

static void DisconnectScreen_ContextRecreated(void* screen) {
	struct DisconnectScreen* s = (struct DisconnectScreen*)screen;
	Screen_UpdateVb(screen);

	Gui_MakeTitleFont(&s->titleFont);
	Gui_MakeBodyFont(&s->messageFont);
	TextWidget_Set(&s->title,   &s->titleStr,   &s->titleFont);
	TextWidget_Set(&s->message, &s->messageStr, &s->messageFont);

	DisconnectScreen_UpdateReconnect(s);
	ButtonWidget_SetConst(&s->quit, "Quit game", &s->titleFont);
}

static void DisconnectScreen_OnReconnect(void* s, void* w) {
	Gui_Remove((struct Screen*)s);
	Gui_ShowDefault();
	Server.BeginConnect();
}

static void DisconnectScreen_OnQuit(void* s, void* w) { 
	Window_RequestClose(); 
}

static void DisconnectScreen_Init(void* screen) {
	struct DisconnectScreen* s = (struct DisconnectScreen*)screen;
	s->widgets      = s->__widgets;
	s->numWidgets   = 0;
	s->maxWidgets   = Array_Elems(s->__widgets);

	TextWidget_Add(s, &s->title);
	TextWidget_Add(s, &s->message);

	ButtonWidget_Add(s, &s->reconnect, 300, DisconnectScreen_OnReconnect);
	ButtonWidget_Add(s, &s->quit,      300, DisconnectScreen_OnQuit);
	if (!s->canReconnect) s->reconnect.flags = WIDGET_FLAG_DISABLED;

	Game_SetMinFrameTime(1000 / 5.0f);

	s->delayLeft    = DISCONNECT_DELAY_SECS;
	s->lastSecsLeft = DISCONNECT_DELAY_SECS;
	s->maxVertices  = Screen_CalcDefaultMaxVertices(s);
}

static void DisconnectScreen_Update(void* screen, float delta) {
	struct DisconnectScreen* s = (struct DisconnectScreen*)screen;
	int secsLeft;

	if (!s->canReconnect) return;
	s->delayLeft -= delta;
	secsLeft = Math_Ceil(s->delayLeft);

	if (secsLeft < 0) secsLeft = 0;
	if (s->lastSecsLeft == secsLeft && s->reconnect.active == s->lastActive) return;
	DisconnectScreen_UpdateReconnect(s);

	s->lastSecsLeft = secsLeft;
	s->lastActive   = s->reconnect.active;
	s->dirty        = true;
}

static void DisconnectScreen_Render(void* screen, float delta) {
	PackedCol top    = PackedCol_Make(64, 32, 32, 255);
	PackedCol bottom = PackedCol_Make(80, 16, 16, 255);
	Gfx_Draw2DGradient(0, 0, Window_UI.Width, Window_UI.Height, top, bottom);

	Screen_Render2Widgets(screen, delta);
}

static void DisconnectScreen_Free(void* screen) { Game_SetFpsLimit(Game_FpsLimit); }

static const struct ScreenVTABLE DisconnectScreen_VTABLE = {
	DisconnectScreen_Init,   DisconnectScreen_Update, DisconnectScreen_Free,
	DisconnectScreen_Render, Screen_BuildMesh,
	Menu_InputDown,          Screen_InputUp,          Screen_TKeyPress, Screen_TText,
	Menu_PointerDown,        Screen_PointerUp,        Menu_PointerMove, Screen_TMouseScroll,
	DisconnectScreen_Layout, DisconnectScreen_ContextLost, DisconnectScreen_ContextRecreated
};
void DisconnectScreen_Show(const cc_string* title, const cc_string* message) {
	static const cc_string kick = String_FromConst("Kicked ");
	static const cc_string ban  = String_FromConst("Banned ");
	cc_string why; char whyBuffer[STRING_SIZE];
	struct DisconnectScreen* s = &DisconnectScreen;
	int i;

	s->grabsInput  = true;
	s->blocksWorld = true;

	String_InitArray(s->titleStr,   s->_titleBuffer);
	String_AppendString(&s->titleStr,   title);
	String_InitArray(s->messageStr, s->_messageBuffer);
	String_AppendString(&s->messageStr, message);

	String_InitArray(why, whyBuffer);
	String_AppendColorless(&why, message);
	
	s->canReconnect = !(String_CaselessStarts(&why, &kick) || String_CaselessStarts(&why, &ban));
	s->VTABLE       = &DisconnectScreen_VTABLE;

	Gui_Add((struct Screen*)s, GUI_PRIORITY_DISCONNECT);
	/* Remove other screens instead of just drawing over them to reduce GPU usage */
	for (i = Gui.ScreensCount - 1; i >= 0; i--) 
	{
		if (Gui_Screens[i] == (struct Screen*)s) continue;
		Gui_Remove(Gui_Screens[i]);
	}
}
