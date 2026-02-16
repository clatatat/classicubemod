#include "Menus.h"
#include "Widgets.h"
#include "Game.h"
#include "Event.h"
#include "Platform.h"
#include "Inventory.h"
#include "Drawer2D.h"
#include "Graphics.h"
#include "Funcs.h"
#include "Model.h"
#include "Generator.h"
#include "Server.h"
#include "Chat.h"
#include "ExtMath.h"
#include "Window.h"
#include "Camera.h"
#include "Http.h"
#include "Block.h"
#include "World.h"
#include "Formats.h"
#include "BlockPhysics.h"
#include "MapRenderer.h"
#include "TexturePack.h"
#include "Audio.h"
#include "Screens.h"
#include "Gui.h"
#include "Deflate.h"
#include "Stream.h"
#include "Builder.h"
#include "Lighting.h"
#include "Logger.h"
#include "Options.h"
#include "Input.h"
#include "Utils.h"
#include "Errors.h"
#include "SystemFonts.h"
#include "EnvRenderer.h"

typedef void (*Button_GetText)(struct ButtonWidget* btn, cc_string* raw);
typedef void (*Button_SetText)(struct ButtonWidget* btn, const cc_string* raw);

struct MenuOptionMetaBool {
	Button_GetText GetText;
	Button_SetText SetText;
	Button_GetBool GetValue;
	Button_SetBool SetValue;
};

struct MenuOptionMetaEnum {
	Button_GetText GetText;
	Button_SetText SetText;
	Button_GetEnum GetValue;
	Button_SetEnum SetValue;
	const char* const* names;
	int count;
};

struct MenuOptionMetaHex {
	Button_GetText GetText;
	Button_SetText SetText;
	Button_GetHex  GetValue;
	Button_SetHex  SetValue;
	struct MenuInputDesc desc;
};

struct MenuOptionMetaInt {
	Button_GetText GetText;
	Button_SetText SetText;
	Button_GetInt  GetValue;
	Button_SetInt  SetValue;
	struct MenuInputDesc desc;
};

struct MenuOptionMetaNum {
	Button_GetText GetText;
	Button_SetText SetText;
	Button_GetNum  GetValue;
	Button_SetNum  SetValue;
	struct MenuInputDesc desc;
};

/* Describes a menu option button */
union MenuOptionMeta {
	struct MenuOptionMetaBool b;
	struct MenuOptionMetaEnum e;
	struct MenuOptionMetaHex  h;
	struct MenuOptionMetaInt  i;
	struct MenuOptionMetaNum  n;
};

/*########################################################################################################################*
*------------------------------------------------------Menu utilities-----------------------------------------------------*
*#########################################################################################################################*/
static void Menu_Remove(void* screen, int i) {
	struct Screen* s = (struct Screen*)screen;
	struct Widget** widgets = s->widgets;

	if (widgets[i]) { Elem_Free(widgets[i]); }
	widgets[i] = NULL;
}

static float Menu_Float(const cc_string* str) { float v; Convert_ParseFloat(str, &v); return v; }

static void Menu_SwitchBindsClassic(void* a, void* b) { ClassicBindingsScreen_Show(); }
static void Menu_SwitchFont(void* a, void* b)         { FontListScreen_Show(); }

static void Menu_SwitchOptions(void* a, void* b)   { OptionsGroupScreen_Show(); }
static void Menu_SwitchPause(void* a, void* b)     { Gui_ShowPauseMenu(); }


/*########################################################################################################################*
*--------------------------------------------------MenuOptionsScreen------------------------------------------------------*
*#########################################################################################################################*/
struct MenuOptionsScreen;
typedef void (*InitMenuOptions)(struct MenuOptionsScreen* s);
#define MENUOPTS_MAX_OPTS 14
static void MenuOptionsScreen_Layout(void* screen);

static struct MenuOptionsScreen {
	Screen_Body
	const char* descriptions[MENUOPTS_MAX_OPTS + 1];
	struct ButtonWidget* activeBtn;
	InitMenuOptions DoInit, DoRecreateExtra, OnHacksChanged, OnLightingModeServerChanged;
	int numButtons;
	struct FontDesc titleFont, textFont;
	struct TextGroupWidget extHelp;
	struct Texture extHelpTextures[5]; /* max lines is 5 */
	struct ButtonWidget buttons[MENUOPTS_MAX_OPTS], done;
	const char* extHelpDesc;
} MenuOptionsScreen_Instance;

static union  MenuOptionMeta menuOpts_meta[MENUOPTS_MAX_OPTS];
static struct Widget* menuOpts_widgets[MENUOPTS_MAX_OPTS + 1];

static void MenuOptionsScreen_Update(struct MenuOptionsScreen* s, struct ButtonWidget* btn) {
	struct MenuOptionMetaBool* meta = (struct MenuOptionMetaBool*)btn->meta.ptr;
	cc_string title; char titleBuffer[STRING_SIZE];
	String_InitArray(title, titleBuffer);

	String_AppendConst(&title, btn->optName);
	if (meta->GetText) {
		String_AppendConst(&title, ": ");
		meta->GetText(btn, &title);
	}	
	ButtonWidget_Set(btn, &title, &s->titleFont);
}

CC_NOINLINE static void MenuOptionsScreen_FreeExtHelp(struct MenuOptionsScreen* s) {
	Elem_Free(&s->extHelp);
	s->extHelp.lines = 0;
}

static void MenuOptionsScreen_LayoutExtHelp(struct MenuOptionsScreen* s) {
	Widget_SetLocation(&s->extHelp, ANCHOR_MIN, ANCHOR_MIN, 0, 10);
	/* If use centre align above, then each line in extended help gets */
	/* centered aligned separately - which is not the desired behaviour. */
	s->extHelp.xOffset = Window_UI.Width / 2 - s->extHelp.width / 2;
	Widget_Layout(&s->extHelp);
}

static cc_string MenuOptionsScreen_GetDesc(int i) {
	const char* desc = MenuOptionsScreen_Instance.extHelpDesc;
	cc_string descRaw, descLines[5];

	descRaw = String_FromReadonly(desc);
	String_UNSAFE_Split(&descRaw, '\n', descLines, Array_Elems(descLines));
	return descLines[i];
}

static void MenuOptionsScreen_SelectExtHelp(struct MenuOptionsScreen* s, int idx) {
	const char* desc;
	cc_string descRaw, descLines[5];

	MenuOptionsScreen_FreeExtHelp(s);
	if (s->activeBtn) return;
	desc = s->descriptions[idx];
	if (!desc) return;

	if (!s->widgets[idx]) return;
	if (s->widgets[idx]->flags & WIDGET_FLAG_DISABLED) return;

	descRaw          = String_FromReadonly(desc);
	s->extHelp.lines = String_UNSAFE_Split(&descRaw, '\n', descLines, Array_Elems(descLines));
	
	s->extHelpDesc = desc;
	TextGroupWidget_RedrawAll(&s->extHelp);
	MenuOptionsScreen_LayoutExtHelp(s);
}

static void MenuOptionsScreen_OnDone(const cc_string* value, cc_bool valid) {
	struct MenuOptionsScreen* s = &MenuOptionsScreen_Instance;
	if (valid) {
		struct ButtonWidget* btn = s->activeBtn;
		struct MenuOptionMetaBool* meta = (struct MenuOptionMetaBool*)btn->meta.ptr;
		
		meta->SetText(btn, value);
		MenuOptionsScreen_Update(s, btn);
		/* Marking screen as dirty fixes changed option widget appearing wrong */
		/*  for a few frames (e.g. Chatlines options changed from '12' to '1') */
		s->dirty = true;
	}

	if (s->selectedI >= 0) MenuOptionsScreen_SelectExtHelp(s, s->selectedI);
	s->activeBtn = NULL;
}

static int MenuOptionsScreen_PointerMove(void* screen, int id, int x, int y) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	int i = Menu_DoPointerMove(s, id, x, y);
	if (i == -1 || i == s->selectedI) return true;

	s->selectedI = i;
	if (!s->activeBtn) MenuOptionsScreen_SelectExtHelp(s, i);
	return true;
}

static void MenuOptionsScreen_BeginButtons(struct MenuOptionsScreen* s) {
	s->numButtons = 0;
}

static int MenuOptionsScreen_AddButton(struct MenuOptionsScreen* s, const char* name, Widget_LeftClick onClick,
										Button_GetText getValue, Button_SetText setValue, const char* desc) {
	struct ButtonWidget* btn;
	int i = s->numButtons;
	
	btn = &s->buttons[i];
	ButtonWidget_Add(s, btn, 300, onClick);

	btn->optName  = name;
	btn->meta.ptr = &menuOpts_meta[i];
	s->widgets[i] = (struct Widget*)btn;
	s->descriptions[i] = desc;
	
	menuOpts_meta[i].b.GetText = getValue;
	menuOpts_meta[i].b.SetText = setValue;
	s->numButtons++;
	return i;
}

static void MenuOptionsScreen_EndButtons(struct MenuOptionsScreen* s, int half, Widget_LeftClick backClick) {
	struct ButtonWidget* btn;
	int i, col, row, begRow;
	/* Auto calculate half/dividing count */
	if (half < 0) half = (s->numButtons + 1) / 2;

	begRow = 2 - half;
	if (s->numButtons & 1) begRow--;
	begRow = max(-3, begRow);
	
	for (i = 0; i < s->numButtons; i++) 
	{
		btn = &s->buttons[i];
		col = i < half ? -160 : 160;
		row = 50 * (begRow + (i < half ? i : (i - half)));
		Widget_SetLocation(btn, ANCHOR_CENTRE, ANCHOR_CENTRE, col, row);
	}
	ButtonWidget_Add(s, &s->done, 400, backClick);
}


static void MenuOptionsScreen_BoolGet(struct ButtonWidget* btn, cc_string* v) {
	struct MenuOptionMetaBool* meta = (struct MenuOptionMetaBool*)btn->meta.ptr;
	cc_bool value = meta->GetValue();
	String_AppendConst(v, value ? "ON" : "OFF");
}

static void MenuOptionsScreen_BoolClick(void* screen, void* widget) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	struct ButtonWidget* btn    = (struct ButtonWidget*)widget;
	
	struct MenuOptionMetaBool* meta = (struct MenuOptionMetaBool*)btn->meta.ptr;
	cc_bool value = meta->GetValue();

	meta->SetValue(!value);
	MenuOptionsScreen_Update(s, btn);
}

void MenuOptionsScreen_AddBool(struct MenuOptionsScreen* s, const char* name, 
									Button_GetBool getValue, Button_SetBool setValue, const char* desc) {
	int i = MenuOptionsScreen_AddButton(s, name, MenuOptionsScreen_BoolClick, 
										MenuOptionsScreen_BoolGet, NULL, desc);
	
	struct MenuOptionMetaBool* meta = &menuOpts_meta[i].b;
	meta->GetValue = getValue;
	meta->SetValue = setValue;
}


static void MenuOptionsScreen_EnumGet(struct ButtonWidget* btn, cc_string* v) {
	struct MenuOptionMetaEnum* meta = (struct MenuOptionMetaEnum*)btn->meta.ptr;
	int raw = meta->GetValue();
	String_AppendConst(v, meta->names[raw]);
}

void MenuDropdownOverlay_Show(const char* titleName,
	const char* const* names, int count,
	Button_GetEnum getValue, DropdownDone onDone);

static void MenuOptionsScreen_EnumDropdownDone(int value, cc_bool valid) {
	struct MenuOptionsScreen* s = &MenuOptionsScreen_Instance;
	struct ButtonWidget* btn = s->activeBtn;

	if (valid && btn) {
		struct MenuOptionMetaEnum* meta = (struct MenuOptionMetaEnum*)btn->meta.ptr;
		meta->SetValue(value);
		MenuOptionsScreen_Update(s, btn);
		s->dirty = true;
	}

	s->activeBtn = NULL;
	if (s->selectedI >= 0) MenuOptionsScreen_SelectExtHelp(s, s->selectedI);
}

static void MenuOptionsScreen_EnumClick(void* screen, void* widget) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	struct ButtonWidget* btn    = (struct ButtonWidget*)widget;
	struct MenuOptionMetaEnum* meta = (struct MenuOptionMetaEnum*)btn->meta.ptr;

	if (meta->count > 2 && Options_GetBool(OPT_USE_DROPDOWNS, true)) {
		MenuOptionsScreen_FreeExtHelp(s);
		s->activeBtn = btn;
		MenuDropdownOverlay_Show(btn->optName, meta->names, meta->count,
			meta->GetValue, MenuOptionsScreen_EnumDropdownDone);
	} else {
		int raw = meta->GetValue();
		raw = (raw + 1) % meta->count;
		meta->SetValue(raw);
		MenuOptionsScreen_Update(s, btn);
	}
}

void MenuOptionsScreen_AddEnum(struct MenuOptionsScreen* s, const char* name,
									const char* const* names, int namesCount,
									Button_GetEnum getValue, Button_SetEnum setValue, const char* desc) {
	int i = MenuOptionsScreen_AddButton(s, name, MenuOptionsScreen_EnumClick, 
										MenuOptionsScreen_EnumGet, NULL, desc);
	
	struct MenuOptionMetaEnum* meta = &menuOpts_meta[i].e;
	meta->GetValue = getValue;
	meta->SetValue = setValue;
	meta->names    = names;
	meta->count    = namesCount;
}


static void MenuInputOverlay_CheckStillValid(struct MenuOptionsScreen* s) {
	if (!s->activeBtn) return;
	
	if (s->activeBtn->flags & WIDGET_FLAG_DISABLED) {
		/* source button is disabled now, so close open input overlay */
		MenuInputOverlay_Close(false);
	}
}

static void MenuOptionsScreen_InputClick(void* screen, void* widget) {
	cc_string value; char valueBuffer[STRING_SIZE];
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	struct ButtonWidget* btn    = (struct ButtonWidget*)widget;
	struct MenuOptionMetaInt* meta = (struct MenuOptionMetaInt*)btn->meta.ptr;

	MenuOptionsScreen_FreeExtHelp(s);
	s->activeBtn = btn;

	String_InitArray(value, valueBuffer);
	meta->GetText(btn, &value);
	MenuInputOverlay_Show(&meta->desc, &value, MenuOptionsScreen_OnDone, Gui_TouchUI);
}


static void MenuOptionsScreen_HexGet(struct ButtonWidget* btn, cc_string* v) {
	struct MenuOptionMetaHex* meta = (struct MenuOptionMetaHex*)btn->meta.ptr;
	PackedCol raw = meta->GetValue();
	PackedCol_ToHex(v, raw);
}

static void MenuOptionsScreen_HexSet(struct ButtonWidget* btn, const cc_string* v) {
	struct MenuOptionMetaHex* meta = (struct MenuOptionMetaHex*)btn->meta.ptr;
	cc_uint8 rgb[3];
	
	PackedCol_TryParseHex(v, rgb); 
	meta->SetValue(PackedCol_Make(rgb[0], rgb[1], rgb[2], 255));
}

void MenuOptionsScreen_AddHex(struct MenuOptionsScreen* s, const char* name, PackedCol defaultValue,
									Button_GetHex getValue, Button_SetHex setValue, const char* desc) {
	int i = MenuOptionsScreen_AddButton(s, name, MenuOptionsScreen_InputClick, 
										MenuOptionsScreen_HexGet, MenuOptionsScreen_HexSet, desc);
										
	struct MenuOptionMetaHex* meta = &menuOpts_meta[i].h;
	meta->GetValue = getValue;
	meta->SetValue = setValue;
	MenuInput_Hex(meta->desc, defaultValue);
}


static void MenuOptionsScreen_IntGet(struct ButtonWidget* btn, cc_string* v) {
	struct MenuOptionMetaInt* meta = (struct MenuOptionMetaInt*)btn->meta.ptr;
	String_AppendInt(v, meta->GetValue());
}

static void MenuOptionsScreen_IntSet(struct ButtonWidget* btn, const cc_string* v) {
	struct MenuOptionMetaInt* meta = (struct MenuOptionMetaInt*)btn->meta.ptr;
	int value; 
	Convert_ParseInt(v, &value);
	meta->SetValue(value);
}

void MenuOptionsScreen_AddInt(struct MenuOptionsScreen* s, const char* name,
									int minValue, int maxValue, int defaultValue,
									Button_GetInt getValue, Button_SetInt setValue, const char* desc) {
	int i = MenuOptionsScreen_AddButton(s, name, MenuOptionsScreen_InputClick, 
										MenuOptionsScreen_IntGet, MenuOptionsScreen_IntSet, desc);
										
	struct MenuOptionMetaInt* meta = &menuOpts_meta[i].i;
	meta->GetValue = getValue;
	meta->SetValue = setValue;
	MenuInput_Int(meta->desc, minValue, maxValue, defaultValue);
}


static void MenuOptionsScreen_NumGet(struct ButtonWidget* btn, cc_string* v) {
	struct MenuOptionMetaNum* meta = (struct MenuOptionMetaNum*)btn->meta.ptr;
	meta->GetValue(v);
}

static void MenuOptionsScreen_NumSet(struct ButtonWidget* btn, const cc_string* v) {
	struct MenuOptionMetaNum* meta = (struct MenuOptionMetaNum*)btn->meta.ptr;
	meta->SetValue(v);
}

void MenuOptionsScreen_AddNum(struct MenuOptionsScreen* s, const char* name,
									float minValue, float maxValue, float defaultValue,
									Button_GetNum getValue, Button_SetNum setValue, const char* desc) {
	int i = MenuOptionsScreen_AddButton(s, name, MenuOptionsScreen_InputClick, 
										MenuOptionsScreen_NumGet, MenuOptionsScreen_NumSet, desc);
										
	struct MenuOptionMetaNum* meta = &menuOpts_meta[i].n;
	meta->GetValue = getValue;
	meta->SetValue = setValue;
	MenuInput_Float(meta->desc, minValue, maxValue, defaultValue);
}


static void MenuOptionsScreen_OnHacksChanged(void* screen) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	if (s->OnHacksChanged) s->OnHacksChanged(s);
	s->dirty = true;
}

static void MenuOptionsScreen_OnLightingModeServerChanged(void* screen, cc_uint8 oldMode, cc_bool fromServer) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	/* This event only actually matters if it's from the server */
	if (fromServer) {
		if (s->OnLightingModeServerChanged) s->OnLightingModeServerChanged(s);
		s->dirty = true;
	}
}

static void MenuOptionsScreen_Init(void* screen) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	int i;

	s->widgets    = menuOpts_widgets;
	s->numWidgets = 0;
	s->maxWidgets = MENUOPTS_MAX_OPTS + 1; /* always have back button */

	/* The various menu options screens might have different number of widgets */
	for (i = 0; i < MENUOPTS_MAX_OPTS; i++) { 
		s->widgets[i]      = NULL;
		s->descriptions[i] = NULL;
	}

	s->activeBtn   = NULL;
	s->selectedI   = -1;
	s->DoInit(s);

	TextGroupWidget_Create(&s->extHelp, 5, s->extHelpTextures, MenuOptionsScreen_GetDesc);
	s->extHelp.lines = 0;
	Event_Register_(&UserEvents.HackPermsChanged, screen, MenuOptionsScreen_OnHacksChanged);
	Event_Register_(&WorldEvents.LightingModeChanged, screen, MenuOptionsScreen_OnLightingModeServerChanged);
	
	s->maxVertices = Screen_CalcDefaultMaxVertices(s);
}
	
#define EXTHELP_PAD 5 /* padding around extended help box */
static void MenuOptionsScreen_Render(void* screen, float delta) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	struct TextGroupWidget* w;
	PackedCol tableColor = PackedCol_Make(20, 20, 20, 200);

	MenuScreen_Render2(s, delta);
	if (!s->extHelp.lines) return;

	/* Don't show tooltips on resolutions below 800x600 */
	if (Window_UI.Width < 800 || Window_UI.Height < 600) return;

	w = &s->extHelp;
	Gfx_Draw2DFlat(w->x - EXTHELP_PAD, w->y - EXTHELP_PAD, 
		w->width + EXTHELP_PAD * 2, w->height + EXTHELP_PAD * 2, tableColor);

	Elem_Render(&s->extHelp);
}

void MenuDropdownOverlay_Close(cc_bool apply);

static void MenuOptionsScreen_Free(void* screen) {
	Event_Unregister_(&UserEvents.HackPermsChanged,     screen, MenuOptionsScreen_OnHacksChanged);
	Event_Unregister_(&WorldEvents.LightingModeChanged, screen, MenuOptionsScreen_OnLightingModeServerChanged);
	MenuInputOverlay_Close(false);
	MenuDropdownOverlay_Close(false);
}

static void MenuOptionsScreen_Layout(void* screen) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	Screen_Layout(s);
	Menu_LayoutBack(&s->done);
	MenuOptionsScreen_LayoutExtHelp(s);
}

static void MenuOptionsScreen_ContextLost(void* screen) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	Font_Free(&s->titleFont);
	Font_Free(&s->textFont);
	Screen_ContextLost(s);
	Elem_Free(&s->extHelp);
}

static void MenuOptionsScreen_ContextRecreated(void* screen) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	int i;
	Gui_MakeTitleFont(&s->titleFont);
	Gui_MakeBodyFont(&s->textFont);
	Screen_UpdateVb(screen);

	for (i = 0; i < s->numButtons; i++) 
	{ 
		if (s->widgets[i]) MenuOptionsScreen_Update(s, &s->buttons[i]); 
	}

	ButtonWidget_SetConst(&s->done, "Done", &s->titleFont);
	if (s->DoRecreateExtra) s->DoRecreateExtra(s);
	TextGroupWidget_SetFont(&s->extHelp, &s->textFont);
	TextGroupWidget_RedrawAll(&s->extHelp); /* TODO: SetFont should redrawall implicitly */
}

static int MenuOptionsScreen_InputDown(void* screen, int key, struct InputDevice* device) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;

	if (key == CCMOUSE_R && !Options_GetBool(OPT_USE_DROPDOWNS, true)) {
		int idx = s->selectedI;
		if (idx >= 0 && idx < s->numButtons) {
			struct Widget* w = s->widgets[idx];
			if (w && !(w->flags & WIDGET_FLAG_DISABLED) &&
				w->MenuClick == MenuOptionsScreen_EnumClick) {
				struct ButtonWidget* btn = (struct ButtonWidget*)w;
				struct MenuOptionMetaEnum* meta = (struct MenuOptionMetaEnum*)btn->meta.ptr;
				int raw = meta->GetValue();
				raw = (raw - 1 + meta->count) % meta->count;
				meta->SetValue(raw);
				MenuOptionsScreen_Update(s, btn);
			}
		}
	}

	return Menu_InputDown(screen, key, device);
}

static const struct ScreenVTABLE MenuOptionsScreen_VTABLE = {
	MenuOptionsScreen_Init,   Screen_NullUpdate, MenuOptionsScreen_Free,
	MenuOptionsScreen_Render, Screen_BuildMesh,
	MenuOptionsScreen_InputDown, Screen_InputUp, Screen_TKeyPress, Screen_TText,
	Menu_PointerDown,         Screen_PointerUp,  MenuOptionsScreen_PointerMove, Screen_TMouseScroll,
	MenuOptionsScreen_Layout, MenuOptionsScreen_ContextLost, MenuOptionsScreen_ContextRecreated
};
void MenuOptionsScreen_Show(InitMenuOptions init) {
	struct MenuOptionsScreen* s = &MenuOptionsScreen_Instance;
	s->grabsInput = true;
	s->closable   = true;
	s->VTABLE     = &MenuOptionsScreen_VTABLE;

	s->DoInit          = init;
	s->DoRecreateExtra = NULL;
	s->OnHacksChanged  = NULL;
	Gui_Add((struct Screen*)s, GUI_PRIORITY_MENU);
}


/*########################################################################################################################*
*-------------------------------------------------MenuDropdownOverlay-----------------------------------------------------*
*#########################################################################################################################*/
#define DROPDOWN_MAX_VISIBLE 8

static struct MenuDropdownOverlay {
	Screen_Body
	struct FontDesc titleFont, textFont;
	struct TextWidget title;
	struct ButtonWidget options[DROPDOWN_MAX_VISIBLE];
	int currentValue;
	int scrollOffset;
	int visibleCount;
	int totalCount;
	float scrollAcc;
	const char* const* optionNames;
	const char* titleName;
	Button_GetEnum getValue;
	DropdownDone onDone;
	struct Widget* __widgets[1 + DROPDOWN_MAX_VISIBLE];
} MenuDropdownOverlay_Instance;

static void MenuDropdownOverlay_RedrawOptions(struct MenuDropdownOverlay* s) {
	int i, optIdx;
	cc_string text; char textBuffer[STRING_SIZE];

	for (i = 0; i < s->visibleCount; i++) {
		optIdx = s->scrollOffset + i;
		String_InitArray(text, textBuffer);

		if (optIdx == s->currentValue) {
			String_AppendConst(&text, "&a> ");
		}
		String_AppendConst(&text, s->optionNames[optIdx]);
		ButtonWidget_Set(&s->options[i], &text, &s->textFont);
	}
}

static void MenuDropdownOverlay_SetScrollOffset(struct MenuDropdownOverlay* s, int offset) {
	int maxOffset = s->totalCount - s->visibleCount;
	if (maxOffset < 0) maxOffset = 0;
	if (offset < 0) offset = 0;
	if (offset > maxOffset) offset = maxOffset;

	s->scrollOffset = offset;
	MenuDropdownOverlay_RedrawOptions(s);
}

void MenuDropdownOverlay_Close(cc_bool apply) {
	struct MenuDropdownOverlay* s = &MenuDropdownOverlay_Instance;

	Gui_Remove((struct Screen*)s);

	if (s->onDone) s->onDone(s->currentValue, apply);
	s->onDone = NULL;
}

static void MenuDropdownOverlay_OptionClick(void* screen, void* widget) {
	struct MenuDropdownOverlay* s = (struct MenuDropdownOverlay*)screen;
	struct ButtonWidget* btn = (struct ButtonWidget*)widget;
	int i;

	for (i = 0; i < s->visibleCount; i++) {
		if (&s->options[i] == btn) {
			int selected = s->scrollOffset + i;
			if (selected < s->totalCount) {
				s->currentValue = selected;
				MenuDropdownOverlay_Close(true);
			}
			return;
		}
	}
}

static void MenuDropdownOverlay_Init(void* screen) {
	struct MenuDropdownOverlay* s = (struct MenuDropdownOverlay*)screen;
	int i;

	s->widgets    = s->__widgets;
	s->numWidgets = 0;
	s->maxWidgets = Array_Elems(s->__widgets);
	s->selectedI  = -1;
	s->scrollAcc  = 0.0f;

	s->visibleCount = min(s->totalCount, DROPDOWN_MAX_VISIBLE);

	s->currentValue = s->getValue();
	if (s->currentValue < 0) s->currentValue = 0;
	if (s->currentValue >= s->totalCount) s->currentValue = s->totalCount - 1;

	/* Center scroll so current selection is visible */
	s->scrollOffset = s->currentValue - (s->visibleCount / 2);
	if (s->scrollOffset < 0) s->scrollOffset = 0;
	if (s->scrollOffset > s->totalCount - s->visibleCount)
		s->scrollOffset = s->totalCount - s->visibleCount;
	if (s->scrollOffset < 0) s->scrollOffset = 0;

	TextWidget_Add(s, &s->title);

	for (i = 0; i < s->visibleCount; i++) {
		ButtonWidget_Add(s, &s->options[i], 300, MenuDropdownOverlay_OptionClick);
	}

	s->maxVertices = Screen_CalcDefaultMaxVertices(s);
}

static int MenuDropdownOverlay_KeyDown(void* screen, int key, struct InputDevice* device) {
	struct MenuDropdownOverlay* s = (struct MenuDropdownOverlay*)screen;

	if (key == CCKEY_ESCAPE) {
		MenuDropdownOverlay_Close(false);
		return true;
	}
	if (key == CCKEY_ENTER || key == CCKEY_KP_ENTER) {
		MenuDropdownOverlay_Close(true);
		return true;
	}
	if (key == CCKEY_UP || key == CCWHEEL_UP) {
		if (s->currentValue > 0) {
			s->currentValue--;
			if (s->currentValue < s->scrollOffset)
				MenuDropdownOverlay_SetScrollOffset(s, s->scrollOffset - 1);
			else
				MenuDropdownOverlay_RedrawOptions(s);
			s->dirty = true;
		}
		return true;
	}
	if (key == CCKEY_DOWN || key == CCWHEEL_DOWN) {
		if (s->currentValue < s->totalCount - 1) {
			s->currentValue++;
			if (s->currentValue >= s->scrollOffset + s->visibleCount)
				MenuDropdownOverlay_SetScrollOffset(s, s->scrollOffset + 1);
			else
				MenuDropdownOverlay_RedrawOptions(s);
			s->dirty = true;
		}
		return true;
	}
	return Screen_InputDown(s, key, device);
}

static int MenuDropdownOverlay_PointerDown(void* screen, int id, int x, int y) {
	int result = Screen_DoPointerDown(screen, id, x, y);
	if (result < 0) {
		MenuDropdownOverlay_Close(false);
	}
	return true;
}

static int MenuDropdownOverlay_PointerMove(void* screen, int id, int x, int y) {
	Menu_DoPointerMove(screen, id, x, y);
	return true;
}

static int MenuDropdownOverlay_MouseScroll(void* screen, float delta) {
	struct MenuDropdownOverlay* s = (struct MenuDropdownOverlay*)screen;
	int steps;

	if (s->totalCount <= s->visibleCount) return true;

	steps = Utils_AccumulateWheelDelta(&s->scrollAcc, delta);
	if (steps != 0) {
		MenuDropdownOverlay_SetScrollOffset(s, s->scrollOffset - steps);
		s->dirty = true;
	}
	return true;
}

static void MenuDropdownOverlay_Render(void* screen, float delta) {
	MenuScreen_Render2(screen, delta);
}

static void MenuDropdownOverlay_Layout(void* screen) {
	struct MenuDropdownOverlay* s = (struct MenuDropdownOverlay*)screen;
	int i, startY, spacing;

	spacing = 50;
	startY = -(s->visibleCount * spacing) / 2;

	Widget_SetLocation(&s->title, ANCHOR_CENTRE, ANCHOR_CENTRE, 0, startY - 30);

	for (i = 0; i < s->visibleCount; i++) {
		Widget_SetLocation(&s->options[i], ANCHOR_CENTRE, ANCHOR_CENTRE,
			0, startY + i * spacing);
	}

	Screen_Layout(s);
}

static void MenuDropdownOverlay_ContextLost(void* screen) {
	struct MenuDropdownOverlay* s = (struct MenuDropdownOverlay*)screen;
	Font_Free(&s->titleFont);
	Font_Free(&s->textFont);
	Screen_ContextLost(s);
}

static void MenuDropdownOverlay_ContextRecreated(void* screen) {
	struct MenuDropdownOverlay* s = (struct MenuDropdownOverlay*)screen;
	cc_string titleStr; char titleBuffer[STRING_SIZE];

	Gui_MakeTitleFont(&s->titleFont);
	Gui_MakeBodyFont(&s->textFont);
	Screen_UpdateVb(s);

	String_InitArray(titleStr, titleBuffer);
	if (s->titleName) {
		String_AppendConst(&titleStr, s->titleName);
	}
	TextWidget_Set(&s->title, &titleStr, &s->titleFont);

	MenuDropdownOverlay_RedrawOptions(s);
}

static const struct ScreenVTABLE MenuDropdownOverlay_VTABLE = {
	MenuDropdownOverlay_Init,        Screen_NullUpdate,              Screen_NullFunc,
	MenuDropdownOverlay_Render,      Screen_BuildMesh,
	MenuDropdownOverlay_KeyDown,     Screen_InputUp,                 Screen_TKeyPress, Screen_TText,
	MenuDropdownOverlay_PointerDown, Screen_PointerUp,               MenuDropdownOverlay_PointerMove, MenuDropdownOverlay_MouseScroll,
	MenuDropdownOverlay_Layout,      MenuDropdownOverlay_ContextLost, MenuDropdownOverlay_ContextRecreated
};

void MenuDropdownOverlay_Show(const char* titleName,
	const char* const* names, int count,
	Button_GetEnum getValue, DropdownDone onDone) {
	struct MenuDropdownOverlay* s = &MenuDropdownOverlay_Instance;

	s->grabsInput  = true;
	s->closable    = false;
	s->titleName   = titleName;
	s->optionNames = names;
	s->totalCount  = count;
	s->getValue    = getValue;
	s->onDone      = onDone;
	s->VTABLE      = &MenuDropdownOverlay_VTABLE;

	Gui_Add((struct Screen*)s, GUI_PRIORITY_MENUINPUT);
}


/*########################################################################################################################*
*---------------------------------------------------ClassicOptionsScreen--------------------------------------------------*
*#########################################################################################################################*/
enum ViewDist { VIEW_TINY, VIEW_SHORT, VIEW_NORMAL, VIEW_FAR, VIEW_COUNT };
static const char* const viewDistNames[VIEW_COUNT] = { "TINY", "SHORT", "NORMAL", "FAR" };

static cc_bool ClO_GetMusic(void) { return Audio_MusicVolume > 0; }
static void    ClO_SetMusic(cc_bool v) {
	Audio_SetMusic(v ? 100 : 0);
	Options_SetInt(OPT_MUSIC_VOLUME, Audio_MusicVolume);
}

static cc_bool ClO_GetInvert(void) { return Camera.Invert; }
static void    ClO_SetInvert(cc_bool v) { 
	Camera.Invert = v;
	Options_SetBool(OPT_INVERT_MOUSE, v); 
}

static int  ClO_GetViewDist(void) {
	if (Game_ViewDistance >= 512) return VIEW_FAR;
	if (Game_ViewDistance >= 128) return VIEW_NORMAL;
	if (Game_ViewDistance >= 32)  return VIEW_SHORT;
	
	return VIEW_TINY;
}
static void ClO_SetViewDist(int v) {
	int dist = v == VIEW_FAR ? 512 : (v == VIEW_NORMAL ? 128 : (v == VIEW_SHORT ? 32 : 8));
	Game_UserSetViewDistance(dist);
}

static cc_bool ClO_GetAnaglyph(void) { return Game_Anaglyph3D; }
static void    ClO_SetAnaglyph(cc_bool v) {
	Game_Anaglyph3D = v;
	Options_SetBool(OPT_ANAGLYPH3D, v);
}

static cc_bool ClO_GetSounds(void) { return Audio_SoundsVolume > 0; }
static void    ClO_SetSounds(cc_bool v) {
	Audio_SetSounds(v ? 100 : 0);
	Options_SetInt(OPT_SOUND_VOLUME, Audio_SoundsVolume);
}

static cc_bool ClO_GetShowFPS(void) { return Gui.ShowFPS; }
static void    ClO_SetShowFPS(cc_bool v) { 
	Gui.ShowFPS = v;
	Options_SetBool(OPT_SHOW_FPS, v); 
}

static cc_bool ClO_GetViewBob(void) { return Game_ViewBobbing; }
static void    ClO_SetViewBob(cc_bool v) { 
	Game_ViewBobbing = v;
	Options_SetBool(OPT_VIEW_BOBBING, v); 
}

static cc_bool ClO_GetFPS(void) { return Game_FpsLimit == FPS_LIMIT_VSYNC; }
static void ClO_SetFPS(cc_bool v) {
	int method    = v ? FPS_LIMIT_VSYNC : FPS_LIMIT_NONE;
	cc_string str = String_FromReadonly(FpsLimit_Names[method]);
	Options_Set(OPT_FPS_LIMIT, &str);
	Game_SetFpsLimit(method);
}

static cc_bool ClO_GetHacks(void) { return Entities.CurPlayer->Hacks.Enabled; }
static void    ClO_SetHacks(cc_bool v) {
	Entities.CurPlayer->Hacks.Enabled = v;
	Options_SetBool(OPT_HACKS_ENABLED, v);
	HacksComp_Update(&Entities.CurPlayer->Hacks);
}

static void ClassicOptionsScreen_RecreateExtra(struct MenuOptionsScreen* s) {
	ButtonWidget_SetConst(&s->buttons[9], "Controls...", &s->titleFont);
}

static void ClassicOptionsScreen_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddBool(s, "Music",
			ClO_GetMusic,    ClO_SetMusic, NULL);
		MenuOptionsScreen_AddBool(s, "Invert mouse",
			ClO_GetInvert,   ClO_SetInvert, NULL);
		MenuOptionsScreen_AddEnum(s, "Render distance", viewDistNames, VIEW_COUNT,
			ClO_GetViewDist, ClO_SetViewDist, NULL);
		MenuOptionsScreen_AddBool(s, "3d anaglyph",
			ClO_GetAnaglyph, ClO_SetAnaglyph, NULL);
		
		MenuOptionsScreen_AddBool(s, "Sound",
			ClO_GetSounds,   ClO_SetSounds, NULL);
		MenuOptionsScreen_AddBool(s, "Show FPS",
			ClO_GetShowFPS,  ClO_SetShowFPS, NULL);
		MenuOptionsScreen_AddBool(s, "View bobbing",
			ClO_GetViewBob,  ClO_SetViewBob, NULL);
		MenuOptionsScreen_AddBool(s, "Limit framerate",
			ClO_GetFPS,      ClO_SetFPS, NULL);
		if (Game_ClassicHacks) {
			MenuOptionsScreen_AddBool(s, "Hacks enabled",
				ClO_GetHacks,ClO_SetHacks, NULL);
		}
	}
	MenuOptionsScreen_EndButtons(s, 4, Menu_SwitchPause);
	s->DoRecreateExtra = ClassicOptionsScreen_RecreateExtra;

	ButtonWidget_Add(s, &s->buttons[9], 400, Menu_SwitchBindsClassic);
	Widget_SetLocation(&s->buttons[9],  ANCHOR_CENTRE, ANCHOR_MAX, 0, 95);
}

void ClassicOptionsScreen_Show(void) {
	MenuOptionsScreen_Show(ClassicOptionsScreen_InitWidgets);
}


/*########################################################################################################################*
*----------------------------------------------------EnvSettingsScreen----------------------------------------------------*
*#########################################################################################################################*/
static PackedCol ES_GetCloudsColor(void) { return Env.CloudsCol; }
static void      ES_SetCloudsColor(PackedCol v) { Env_SetCloudsCol(v); }

static PackedCol ES_GetSkyColor(void) { return Env.SkyCol; }
static void      ES_SetSkyColor(PackedCol v) { Env_SetSkyCol(v); }

static PackedCol ES_GetFogColor(void) { return Env.FogCol; }
static void      ES_SetFogColor(PackedCol v) { Env_SetFogCol(v); }

static void ES_GetCloudsSpeed(cc_string* v) { String_AppendFloat(v, Env.CloudsSpeed, 2); }
static void ES_SetCloudsSpeed(const cc_string* v) { Env_SetCloudsSpeed(Menu_Float(v)); }

static int  ES_GetCloudsHeight(void)  { return Env.CloudsHeight; }
static void ES_SetCloudsHeight(int v) { Env_SetCloudsHeight(v);  }

static PackedCol ES_GetSunColor(void) { return Env.SunCol; }
static void      ES_SetSunColor(PackedCol v) { Env_SetSunCol(v); }

static PackedCol ES_GetShadowColor(void) { return Env.ShadowCol; }
static void      ES_SetShadowColor(PackedCol v) { Env_SetShadowCol(v); }

static int  ES_GetWeather(void)  { return Env.Weather; }
static void ES_SetWeather(int v) { Env_SetWeather(v); }

static void ES_GetWeatherSpeed(cc_string* v) { String_AppendFloat(v, Env.WeatherSpeed, 2); }
static void ES_SetWeatherSpeed(const cc_string* v) { Env_SetWeatherSpeed(Menu_Float(v)); }

static int  ES_GetEdgeHeight(void)  { return Env.EdgeHeight; }
static void ES_SetEdgeHeight(int v) { Env_SetEdgeHeight(v); }

static void EnvSettingsScreen_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddHex(s, "Clouds color", ENV_DEFAULT_CLOUDS_COLOR,
			ES_GetCloudsColor,  ES_SetCloudsColor, NULL);
		MenuOptionsScreen_AddHex(s, "Sky color",    ENV_DEFAULT_SKY_COLOR,
			ES_GetSkyColor,     ES_SetSkyColor, NULL);
		MenuOptionsScreen_AddHex(s, "Fog color",    ENV_DEFAULT_FOG_COLOR,
			ES_GetFogColor,     ES_SetFogColor, NULL);
		MenuOptionsScreen_AddNum(s, "Clouds speed",
			0,       1000,                1,
			ES_GetCloudsSpeed,  ES_SetCloudsSpeed, NULL);
		MenuOptionsScreen_AddInt(s, "Clouds height", 
			-10000, 10000, World.Height + 2,
			ES_GetCloudsHeight, ES_SetCloudsHeight, NULL);
		
		MenuOptionsScreen_AddHex(s, "Sunlight color", ENV_DEFAULT_SUN_COLOR,
			ES_GetSunColor,     ES_SetSunColor, NULL);
		MenuOptionsScreen_AddHex(s, "Shadow color",   ENV_DEFAULT_SHADOW_COLOR,
			ES_GetShadowColor,  ES_SetShadowColor, NULL);
		MenuOptionsScreen_AddEnum(s, "Weather", Weather_Names, Array_Elems(Weather_Names),
			ES_GetWeather,      ES_SetWeather, NULL);
		MenuOptionsScreen_AddNum(s, "Rain/Snow speed",
			 -100,  100,                1,
			ES_GetWeatherSpeed, ES_SetWeatherSpeed, NULL);
		MenuOptionsScreen_AddInt(s, "Water level",
			-2048, 2048, World.Height / 2,
			ES_GetEdgeHeight,   ES_SetEdgeHeight, NULL);
	}
	MenuOptionsScreen_EndButtons(s, -1, Menu_SwitchOptions);
}

void EnvSettingsScreen_Show(void) {
	MenuOptionsScreen_Show(EnvSettingsScreen_InitWidgets);
}


/*########################################################################################################################*
*--------------------------------------------------GraphicsOptionsScreen--------------------------------------------------*
*#########################################################################################################################*/

/* Fullscreen resolution options */
static const char* const GrO_ResNames[] = {
	"Desktop", "320x240", "400x300", "480x360", "512x384", "640x480",
	"800x600", "1024x768", "1280x720", "1366x768",
	"1600x900", "1920x1080", "2560x1080", "2560x1440",
	"3440x1440", "3840x2160", "5120x1440"
};
static const int GrO_ResWidths[]  = { 0, 320, 400, 480, 512, 640, 800, 1024, 1280, 1366, 1600, 1920, 2560, 2560, 3440, 3840, 5120 };
static const int GrO_ResHeights[] = { 0, 240, 300, 360, 384, 480, 600,  768,  720,  768,  900, 1080, 1080, 1440, 1440, 2160, 1440 };
#define GRO_RES_COUNT Array_Elems(GrO_ResNames)

static int GrO_GetRes(void) {
	cc_string str; char strBuf[STRING_SIZE];
	int i;
	String_InitArray(str, strBuf);
	Options_UNSAFE_Get(OPT_FULLSCREEN_RES, &str);
	for (i = 1; i < (int)GRO_RES_COUNT; i++) {
		if (String_CaselessEqualsConst(&str, GrO_ResNames[i])) return i;
	}
	return 0;
}

static int GrO_InitialResIdx; /* resolution index when screen was opened */

static void GrO_SetRes(int v) {
	cc_string str = String_FromReadonly(GrO_ResNames[v]);
	Options_Set(OPT_FULLSCREEN_RES, &str);
}

static void GrO_SwitchBack(void* a, void* b) {
	int curRes = GrO_GetRes();
	/* Apply resolution change on Done if in fullscreen and setting changed */
	if (Window_GetWindowState() == WINDOW_STATE_FULLSCREEN && curRes != GrO_InitialResIdx) {
		if (curRes > 0) {
			Window_SetDisplayResolution(GrO_ResWidths[curRes], GrO_ResHeights[curRes]);
			OptionsGroupScreen_Show();
			ResolutionConfirmOverlay_Show(GrO_ResWidths[curRes], GrO_ResHeights[curRes]);
			return;
		} else {
			Window_RestoreDisplayResolution();
		}
	}
	OptionsGroupScreen_Show();
}

static int  GrO_GetFPS(void) { return Game_FpsLimit; }
static void GrO_SetFPS(int v) {
	cc_string str = String_FromReadonly(FpsLimit_Names[v]);
	Options_Set(OPT_FPS_LIMIT, &str);
	Game_SetFpsLimit(v);
}

static int  GrO_GetViewDist(void) { return Game_ViewDistance; }
static void GrO_SetViewDist(int v) { Game_UserSetViewDistance(v); }

static cc_bool GrO_GetSmooth(void) { return Builder_SmoothLighting; }
static void    GrO_SetSmooth(cc_bool v) {
	Builder_SmoothLighting = v;
	Options_SetBool(OPT_SMOOTH_LIGHTING, v);
	Builder_ApplyActive();
	MapRenderer_Refresh();
}

static cc_bool GrO_GetSimpleFog(void) { return EnvRenderer_SimpleFog; }
static void    GrO_SetSimpleFog(cc_bool v) {
	EnvRenderer_SimpleFog = v;
	Options_SetBool(OPT_SIMPLE_FOG, v);
	EnvRenderer_UpdateFog();
}

static cc_bool GrO_GetClouds(void) { return EnvRenderer_CloudsEnabled; }
static void    GrO_SetClouds(cc_bool v) {
	EnvRenderer_CloudsEnabled = v;
	Options_SetBool(OPT_CLOUDS_ENABLED, v);
}

static int  GrO_GetChunkUpdates(void) { return MapRenderer_MaxChunkUpdates; }
static void GrO_SetChunkUpdates(int v) {
	MapRenderer_MaxChunkUpdates = v;
	Options_SetInt(OPT_MAX_CHUNK_UPDATES, v);
}

static cc_bool GrO_GetOcclusion(void) { return MapRenderer_OcclusionCulling; }
static void    GrO_SetOcclusion(cc_bool v) {
	MapRenderer_OcclusionCulling = v;
	Options_SetBool(OPT_OCCLUSION_CULLING, v);
	MapRenderer_InvalidateSortOrder();
}

static const char* const RenderMode_Names[] = { "Normal", "Legacy", "Fast", "LegacyFast" };
#define RENDER_MODE_COUNT 4

static int GrO_GetRenderMode(void) {
	if (EnvRenderer_Legacy && EnvRenderer_Minimal) return 3; /* LegacyFast */
	if (EnvRenderer_Minimal) return 2; /* Fast */
	if (EnvRenderer_Legacy)  return 1; /* Legacy */
	return 0; /* Normal */
}

static void GrO_SetRenderMode(int v) {
	int flags = 0;
	cc_string mode;
	if (v == 1) flags = ENV_LEGACY;
	else if (v == 2) flags = ENV_MINIMAL;
	else if (v == 3) flags = ENV_LEGACY | ENV_MINIMAL;
	
	EnvRenderer_SetMode(flags);
	mode = String_FromReadonly(RenderMode_Names[v]);
	Options_Set(OPT_RENDER_TYPE, &mode);
}

static int  GrO_GetNames(void) { return Entities.NamesMode; }
static void GrO_SetNames(int v) {
	cc_string str = String_FromReadonly(NameMode_Names[v]);
	Entities.NamesMode = v;
	Options_Set(OPT_NAMES_MODE, &str);
}

static int  GrO_GetShadows(void) { return Entities.ShadowsMode; }
static void GrO_SetShadows(int v) {
	cc_string str = String_FromReadonly(ShadowMode_Names[v]);
	Entities.ShadowsMode = v;
	Options_Set(OPT_ENTITY_SHADOW, &str);
}

static cc_bool GrO_GetMipmaps(void) { return Gfx.Mipmaps; }
static void    GrO_SetMipmaps(cc_bool v) {
	Gfx.Mipmaps = v;
	Options_SetBool(OPT_MIPMAPS, v);
	TexturePack_ExtractCurrent(true);
}

static void GraphicsOptionsScreen_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddEnum(s, "FPS mode", FpsLimit_Names, FPS_LIMIT_COUNT,
			GrO_GetFPS,        GrO_SetFPS,
			"&eVSync: &fLimits FPS to monitor refresh rate.\n" \
			"&e30/60/120/144 FPS: &fLimits to that rate.\n" \
			"&eNoLimit: &fRenders as fast as possible.\n" \
			"&cNoLimit wastes CPU on unseen frames!");
		MenuOptionsScreen_AddInt(s, "View distance",
			8, 4096, 512,
			GrO_GetViewDist,   GrO_SetViewDist, NULL);
		MenuOptionsScreen_AddEnum(s, "Render mode", RenderMode_Names, RENDER_MODE_COUNT,
			GrO_GetRenderMode, GrO_SetRenderMode,
			"&eNormal: &fAll effects enabled.\n" \
			"&eLegacy: &fMay fix cloud/edge issues.\n" \
			"&eFast: &fNo clouds/fog for speed.\n" \
			"&eLegacyFast: &fLegacy + Fast combined.");
		MenuOptionsScreen_AddInt(s, "Chunk updates",
			4, 1024, 30,
			GrO_GetChunkUpdates, GrO_SetChunkUpdates,
			"&eMax chunk updates per frame.\n" \
			"&fLower = better FPS, slower loading.\n" \
			"&cReduce on slow machines.");
		MenuOptionsScreen_AddBool(s, "Smooth lighting",
			GrO_GetSmooth,     GrO_SetSmooth,
			"&eSmoothes lighting, adds glow to blocks.\n" \
			"&cMay reduce performance.");
		MenuOptionsScreen_AddBool(s, "Occlusion cull",
			GrO_GetOcclusion,  GrO_SetOcclusion,
			"&eHides chunks behind solid chunks.\n" \
			"&fHelps in caves/tunnels.\n" \
			"&cMinor CPU cost per frame.");
		MenuOptionsScreen_AddEnum(s, "Names",   NameMode_Names,   NAME_MODE_COUNT,
			GrO_GetNames,      GrO_SetNames,
			"&eNone: &fNo names drawn.\n" \
			"&eHovered: &fTargeted name see-through.\n" \
			"&eAll: &fAll names drawn normally.\n" \
			"&eAllHovered: &fAll names see-through.\n" \
			"&eAllUnscaled: &fSee-through, no scaling.");
		MenuOptionsScreen_AddEnum(s, "Shadows", ShadowMode_Names, SHADOW_MODE_COUNT,
			GrO_GetShadows,    GrO_SetShadows,
			"&eNone: &fNo shadows drawn.\n" \
			"&eSnapToBlock: &fSquare shadow below you.\n" \
			"&eCircle: &fCircular shadow below you.\n" \
			"&eCircleAll: &fShadow under all entities.");
		MenuOptionsScreen_AddBool(s, "Simple fog",
			GrO_GetSimpleFog,  GrO_SetSimpleFog,
			"&eUses sky colour instead of GPU fog.\n" \
			"&fEnable if fog shows as white wall.\n" \
			"&fView edge blends into sky instead.");
		MenuOptionsScreen_AddBool(s, "Clouds",
			GrO_GetClouds,     GrO_SetClouds,
			"&eWhether clouds are rendered.\n" \
			"&fDisable for better performance.");

		if (!Gfx_GetUIOptions(s)) {
		MenuOptionsScreen_AddBool(s, "Mipmaps",
			GrO_GetMipmaps,    GrO_SetMipmaps, NULL);
		}

		MenuOptionsScreen_AddBool(s, "3D anaglyph",
			ClO_GetAnaglyph,   ClO_SetAnaglyph, NULL);

		MenuOptionsScreen_AddEnum(s, "Fullscreen res", GrO_ResNames, GRO_RES_COUNT,
			GrO_GetRes,        GrO_SetRes,
			"&eDesktop: &fUses desktop resolution.\n" \
			"&eOther: &fChanges display resolution.\n" \
			"&cLow res may break GUI display.");
	};
	MenuOptionsScreen_EndButtons(s, -1, GrO_SwitchBack);
	GrO_InitialResIdx = GrO_GetRes();
}

void GraphicsOptionsScreen_Show(void) {
	MenuOptionsScreen_Show(GraphicsOptionsScreen_InitWidgets);
}


/*########################################################################################################################*
*----------------------------------------------------ChatOptionsScreen-----------------------------------------------------*
*#########################################################################################################################*/
static void ChatOptionsScreen_SetScale(const cc_string* v, float* target, const char* optKey) {
	*target = Menu_Float(v);
	Options_Set(optKey, v);
	Gui_LayoutAll();
}

static cc_bool ChO_GetAutoScaleChat(void) { return Gui.AutoScaleChat; }
static void    ChO_SetAutoScaleChat(cc_bool v) {
	Gui.AutoScaleChat = v;
	Options_SetBool(OPT_CHAT_AUTO_SCALE, v);
	Gui_LayoutAll();
}

static void ChO_GetChatScale(cc_string* v) { String_AppendFloat(v, Gui.RawChatScale, 1); }
static void ChO_SetChatScale(const cc_string* v) { 
	ChatOptionsScreen_SetScale(v, &Gui.RawChatScale, OPT_CHAT_SCALE); 
}

static int  ChO_GetChatlines(void) { return Gui.Chatlines; }
static void ChO_SetChatlines(int v) {
	Gui.Chatlines = v;
	ChatScreen_SetChatlines(v);
	Options_SetInt(OPT_CHATLINES, v);
}

static cc_bool ChO_GetLogging(void) { return Chat_Logging; }
static void    ChO_SetLogging(cc_bool v) { 
	Chat_Logging = v;
	Options_SetBool(OPT_CHAT_LOGGING, v); 
	if (!Chat_Logging) Chat_DisableLogging();
}

static cc_bool ChO_GetClickable(void) { return Gui.ClickableChat; }
static void    ChO_SetClickable(cc_bool v) { 
	Gui.ClickableChat = v;
	Options_SetBool(OPT_CLICKABLE_CHAT, v); 
}

static void ChatOptionsScreen_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddBool(s, "Scale with window",
			ChO_GetAutoScaleChat, ChO_SetAutoScaleChat, NULL);
		MenuOptionsScreen_AddNum(s, "Chat scale",
			0.25f, 4.00f, 1,
			ChO_GetChatScale,     ChO_SetChatScale, NULL);
		MenuOptionsScreen_AddInt(s, "Chat lines",
			    0,    30, Gui.DefaultLines,
			ChO_GetChatlines,     ChO_SetChatlines, NULL);

		MenuOptionsScreen_AddBool(s, "Log to disk",
			ChO_GetLogging,       ChO_SetLogging, NULL);
		MenuOptionsScreen_AddBool(s, "Clickable chat",
			ChO_GetClickable,     ChO_SetClickable, NULL);
	}
	MenuOptionsScreen_EndButtons(s, -1, Menu_SwitchOptions);
}

void ChatOptionsScreen_Show(void) {
	MenuOptionsScreen_Show(ChatOptionsScreen_InitWidgets);
}


/*########################################################################################################################*
*----------------------------------------------------GuiOptionsScreen-----------------------------------------------------*
*#########################################################################################################################*/
static cc_bool GuO_GetShadows(void) { return Drawer2D.BlackTextShadows; }
static void    GuO_SetShadows(cc_bool v) {
	Drawer2D.BlackTextShadows = v;
	Options_SetBool(OPT_BLACK_TEXT, v);
	Event_RaiseVoid(&ChatEvents.FontChanged);
}

static cc_bool GuO_GetShowFPS(void) { return Gui.ShowFPS; }
static void    GuO_SetShowFPS(cc_bool v) { 
	Gui.ShowFPS = v;
	Options_SetBool(OPT_SHOW_FPS, v);
}

static void GuO_GetHotbar(cc_string* v) { String_AppendFloat(v, Gui.RawHotbarScale, 1); }
static void GuO_SetHotbar(const cc_string* v) { 
	ChatOptionsScreen_SetScale(v, &Gui.RawHotbarScale, OPT_HOTBAR_SCALE); 
}

static void GuO_GetInventory(cc_string* v) { String_AppendFloat(v, Gui.RawInventoryScale, 1); }
static void GuO_SetInventory(const cc_string* v) { 
	ChatOptionsScreen_SetScale(v, &Gui.RawInventoryScale, OPT_INVENTORY_SCALE); 
}

static void GuO_GetCrosshair(cc_string* v) { String_AppendFloat(v, Gui.RawCrosshairScale, 1); }
static void GuO_SetCrosshair(const cc_string* v) { 
	ChatOptionsScreen_SetScale(v, &Gui.RawCrosshairScale, OPT_CROSSHAIR_SCALE); 
}

static cc_bool GuO_GetTabAuto(void) { return Gui.TabAutocomplete; }
static void    GuO_SetTabAuto(cc_bool v) { 
	Gui.TabAutocomplete = v;
	Options_SetBool(OPT_TAB_AUTOCOMPLETE, v); 
}

static cc_bool GuO_GetUseFont(void) { return false; }
static void    GuO_SetUseFont(cc_bool v) {
	/* Always use built-in bitmap font */
	Drawer2D.BitmappedText = true;
}

static void GuiOptionsScreen_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddBool(s, "Show FPS",
			GuO_GetShowFPS,   GuO_SetShowFPS, NULL);
		MenuOptionsScreen_AddNum(s,  "Hotbar scale",
			0.25f, 4.00f, 1,
			GuO_GetHotbar,    GuO_SetHotbar, NULL);
		MenuOptionsScreen_AddNum(s,  "Inventory scale",
			0.25f, 4.00f, 1,
			GuO_GetInventory, GuO_SetInventory, NULL);
		MenuOptionsScreen_AddNum(s,  "Crosshair scale",
			0.25f, 4.00f, 1,
			GuO_GetCrosshair, GuO_SetCrosshair, NULL);
		
		MenuOptionsScreen_AddBool(s, "Black text shadows",
			GuO_GetShadows,   GuO_SetShadows, NULL);
		MenuOptionsScreen_AddBool(s, "Tab auto-complete",
			GuO_GetTabAuto,   GuO_SetTabAuto, NULL);
		MenuOptionsScreen_AddBool(s, "Use system font",
			GuO_GetUseFont,   GuO_SetUseFont, NULL);
		MenuOptionsScreen_AddButton(s, "Select system font", Menu_SwitchFont,
			NULL,             NULL, NULL);
	}
	MenuOptionsScreen_EndButtons(s, -1, Menu_SwitchOptions);
}

void GuiOptionsScreen_Show(void) {
	MenuOptionsScreen_Show(GuiOptionsScreen_InitWidgets);
}


/*########################################################################################################################*
*---------------------------------------------------HacksSettingsScreen---------------------------------------------------*
*#########################################################################################################################*/
static cc_bool HS_GetHacks(void) { return Entities.CurPlayer->Hacks.Enabled; }
static void    HS_SetHacks(cc_bool v) {
	Entities.CurPlayer->Hacks.Enabled = v;
	Options_SetBool(OPT_HACKS_ENABLED, v);
	HacksComp_Update(&Entities.CurPlayer->Hacks);
}

static void HS_GetSpeed(cc_string* v) { String_AppendFloat(v, Entities.CurPlayer->Hacks.SpeedMultiplier, 2); }
static void HS_SetSpeed(const cc_string* v) {
	Entities.CurPlayer->Hacks.SpeedMultiplier = Menu_Float(v);
	Options_Set(OPT_SPEED_FACTOR, v);
}

static cc_bool HS_GetClipping(void) { return Camera.Clipping; }
static void    HS_SetClipping(cc_bool v) {
	Camera.Clipping = v;
	Options_SetBool(OPT_CAMERA_CLIPPING, v);
}

static void HS_GetJump(cc_string* v) { 
	String_AppendFloat(v, LocalPlayer_JumpHeight(Entities.CurPlayer), 3); 
}

static void HS_SetJump(const cc_string* v) {
	cc_string str; char strBuffer[STRING_SIZE];
	struct PhysicsComp* physics;

	physics = &Entities.CurPlayer->Physics;
	physics->JumpVel     = PhysicsComp_CalcJumpVelocity(Menu_Float(v));
	physics->UserJumpVel = physics->JumpVel;
	
	String_InitArray(str, strBuffer);
	String_AppendFloat(&str, physics->JumpVel, 8);
	Options_Set(OPT_JUMP_VELOCITY, &str);
}

static cc_bool HS_GetWOMHacks(void) { return Entities.CurPlayer->Hacks.WOMStyleHacks; }
static void    HS_SetWOMHacks(cc_bool v) {
	Entities.CurPlayer->Hacks.WOMStyleHacks = v;
	Options_SetBool(OPT_WOM_STYLE_HACKS, v);
}

static cc_bool HS_GetFullStep(void) { return Entities.CurPlayer->Hacks.FullBlockStep; }
static void    HS_SetFullStep(cc_bool v) {
	Entities.CurPlayer->Hacks.FullBlockStep = v;
	Options_SetBool(OPT_FULL_BLOCK_STEP, v);
}

static cc_bool HS_GetPushback(void) { return Entities.CurPlayer->Hacks.PushbackPlacing; }
static void    HS_SetPushback(cc_bool v) {
	Entities.CurPlayer->Hacks.PushbackPlacing = v;
	Options_SetBool(OPT_PUSHBACK_PLACING, v);
}

static cc_bool HS_GetLiquids(void) { return Game_BreakableLiquids; }
static void    HS_SetLiquids(cc_bool v) {
	Game_BreakableLiquids = v;
	Options_SetBool(OPT_MODIFIABLE_LIQUIDS, v);
}

static cc_bool HS_GetSlide(void) { return Entities.CurPlayer->Hacks.NoclipSlide; }
static void    HS_SetSlide(cc_bool v) {
	Entities.CurPlayer->Hacks.NoclipSlide = v;
	Options_SetBool(OPT_NOCLIP_SLIDE, v);
}

static int  HS_GetFOV(void)    { return Camera.Fov; }
static void HS_SetFOV(int fov) {
	if (Camera.ZoomFov > fov) Camera.ZoomFov = fov;
	Camera.DefaultFov = fov;

	Options_SetInt(OPT_FIELD_OF_VIEW, fov);
	Camera_SetFov(fov);
}

static void HacksSettingsScreen_CheckHacksAllowed(struct MenuOptionsScreen* s) {
	struct Widget** widgets = s->widgets;
	struct LocalPlayer* p   = Entities.CurPlayer;
	cc_bool disabled        = !p->Hacks.Enabled;

	Widget_SetDisabled(widgets[3], disabled || !p->Hacks.CanSpeed);
	Widget_SetDisabled(widgets[4], disabled || !p->Hacks.CanSpeed);
	Widget_SetDisabled(widgets[5], disabled || !p->Hacks.CanSpeed);
	Widget_SetDisabled(widgets[7], disabled || !p->Hacks.CanPushbackBlocks);
	MenuInputOverlay_CheckStillValid(s);
}

static void HacksSettingsScreen_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddBool(s, "Hacks enabled",
			HS_GetHacks,    HS_SetHacks, NULL);
		MenuOptionsScreen_AddNum(s,  "Speed multiplier", 
			0.1f,   50, 10,
			HS_GetSpeed,    HS_SetSpeed, NULL);
		MenuOptionsScreen_AddBool(s, "Camera clipping",
			HS_GetClipping, HS_SetClipping,
			"&eIf &fON&e, then the third person cameras will limit\n" \
			"&etheir zoom distance if they hit a solid block.");
		MenuOptionsScreen_AddNum(s,  "Jump height",
			0.1f, 2048, 1.233f,
			HS_GetJump,     HS_SetJump,
			"&eSets how many blocks high you can jump up.\n" \
			"&eNote: You jump much higher when holding down the Speed key binding.");
		MenuOptionsScreen_AddBool(s, "WoM style hacks",
			HS_GetWOMHacks, HS_SetWOMHacks,
			"&eIf &fON&e, gives you a triple jump which increases speed massively,\n" \
			"&ealong with older noclip style. This is based on the \"World of Minecraft\"\n" \
			"&eclassic client mod, which popularized hacks conventions and controls\n" \
			"&ebefore ClassiCube was created.");
		
		MenuOptionsScreen_AddBool(s, "Full block stepping",
			HS_GetFullStep, HS_SetFullStep, NULL);
		MenuOptionsScreen_AddBool(s, "Breakable liquids",
			HS_GetLiquids,  HS_SetLiquids, NULL);
		MenuOptionsScreen_AddBool(s, "Pushback placing",
			HS_GetPushback, HS_SetPushback,
			"&eIf &fON&e, placing blocks that intersect your own position cause\n" \
			"&ethe block to be placed, and you to be moved out of the way.\n" \
			"&fThis is mainly useful for quick pillaring/towering.");
		MenuOptionsScreen_AddBool(s, "Noclip slide",
			HS_GetSlide,    HS_SetSlide,
			"&eIf &fOFF&e, you will immediately stop when in noclip\n&emode and no movement keys are held down.");
		MenuOptionsScreen_AddInt(s,  "Field of view", 
			1,  179, 70,
			HS_GetFOV,      HS_SetFOV, NULL);
	}
	MenuOptionsScreen_EndButtons(s, -1, Menu_SwitchOptions);

	s->OnHacksChanged = HacksSettingsScreen_CheckHacksAllowed;
	HacksSettingsScreen_CheckHacksAllowed(s);
}

void HacksSettingsScreen_Show(void) {
	MenuOptionsScreen_Show(HacksSettingsScreen_InitWidgets);
}


/*########################################################################################################################*
*----------------------------------------------------MiscOptionsScreen----------------------------------------------------*
*#########################################################################################################################*/
static void MiO_GetCameraMass(cc_string* v) { String_AppendFloat(v, Camera.Mass, 2); }
static void MiO_SetCameraMass(const cc_string* c) {
	Camera.Mass = Menu_Float(c);
	Options_Set(OPT_CAMERA_MASS, c);
}

static void MiO_GetReach(cc_string* v) { String_AppendFloat(v, Entities.CurPlayer->ReachDistance, 2); }
static void MiO_SetReach(const cc_string* v) { Entities.CurPlayer->ReachDistance = Menu_Float(v); }

static int  MiO_GetMusic(void)  { return Audio_MusicVolume; }
static void MiO_SetMusic(int v) {
	Options_SetInt(OPT_MUSIC_VOLUME, v);
	Audio_SetMusic(v);
}

static int  MiO_GetSounds(void)  { return Audio_SoundsVolume; }
static void MiO_SetSounds(int v) {
	Options_SetInt(OPT_SOUND_VOLUME, v);
	Audio_SetSounds(v);
}

static cc_bool MiO_GetViewBob(void) { return Game_ViewBobbing; }
static void    MiO_SetViewBob(cc_bool v) { 
	Game_ViewBobbing = v;
	Options_SetBool(OPT_VIEW_BOBBING, v); 
}

static cc_bool MiO_GetCamera(void) { return Camera.Smooth; }
static void    MiO_SetCamera(cc_bool v) { 
	Camera.Smooth = v;
	Options_SetBool(OPT_CAMERA_SMOOTH, v); 
}

static cc_bool MiO_GetPhysics(void) { return Physics.Enabled; }
static void    MiO_SetPhysics(cc_bool v) {
	Physics_SetEnabled(v);
	Options_SetBool(OPT_BLOCK_PHYSICS, v);
}

static cc_bool MiO_GetInvert(void) { return Camera.Invert; }
static void    MiO_SetInvert(cc_bool v) { 
	Camera.Invert = v;
	Options_SetBool(OPT_INVERT_MOUSE, v); 
}

static int  MiO_GetSensitivity(void)  { return Camera.Sensitivity; }
static void MiO_SetSensitivity(int v) {
	Camera.Sensitivity = v;
	Options_SetInt(OPT_SENSITIVITY, v);
}

static cc_bool MiO_GetDropdowns(void) { return Options_GetBool(OPT_USE_DROPDOWNS, true); }
static void    MiO_SetDropdowns(cc_bool v) { Options_SetBool(OPT_USE_DROPDOWNS, v); }

static void MiscSettingsScreen_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddNum(s,  "Reach distance",
			   1, 1024, 5,
			MiO_GetReach,    MiO_SetReach, NULL);
		MenuOptionsScreen_AddNum(s, "Camera Mass",
			1, 100, 20,
			MiO_GetCameraMass, MiO_SetCameraMass,
			"&eChange the smoothness of the smooth camera.");
		MenuOptionsScreen_AddInt(s,  "Music volume",
			   0, 100,  DEFAULT_MUSIC_VOLUME,
			MiO_GetMusic,     MiO_SetMusic, NULL);
		MenuOptionsScreen_AddInt(s,  "Sounds volume",
			   0, 100,  DEFAULT_SOUNDS_VOLUME,
			MiO_GetSounds,  MiO_SetSounds, NULL);

		MenuOptionsScreen_AddBool(s, "Block physics",
			MiO_GetPhysics, MiO_SetPhysics, NULL);
		MenuOptionsScreen_AddBool(s, "Smooth camera",
			MiO_GetCamera, MiO_SetCamera, NULL);
		MenuOptionsScreen_AddBool(s, "View bobbing",
			MiO_GetViewBob, MiO_SetViewBob, NULL);
		MenuOptionsScreen_AddBool(s, "Invert mouse",
			MiO_GetInvert,  MiO_SetInvert, NULL);
		MenuOptionsScreen_AddInt(s,  "Mouse sensitivity",
#ifdef CC_BUILD_WIN
			   1, 200, 40,
#else
			   1, 200, 30,
#endif
			MiO_GetSensitivity, MiO_SetSensitivity, NULL);
		MenuOptionsScreen_AddBool(s, "Dropdowns",
			MiO_GetDropdowns, MiO_SetDropdowns,
			"&eIf &fON&e, options with more than 2 choices\n"
			"&eopen a dropdown menu on click.\n"
			"&eIf &fOFF&e, left-click cycles forward and\n"
			"&eright-click cycles backward instead.");
	}
	MenuOptionsScreen_EndButtons(s, -1, Menu_SwitchOptions);

	/* Disable certain options */
	if (!Server.IsSinglePlayer) Menu_Remove(s, 0);
	if (!Server.IsSinglePlayer) Menu_Remove(s, 4);
}

void MiscOptionsScreen_Show(void) {
	MenuOptionsScreen_Show(MiscSettingsScreen_InitWidgets);
}


/*########################################################################################################################*
*-------------------------------------------------GameplayOptionsScreen---------------------------------------------------*
*#########################################################################################################################*/
static cc_bool GP_GetEnemySpawning(void) { return Game_EnemySpawning; }
static void    GP_SetEnemySpawning(cc_bool v) {
	Game_EnemySpawning = v;
	Options_SetBool(OPT_ENEMY_SPAWNING, v);
}

static cc_bool GP_GetPassiveSpawning(void) { return Game_PassiveSpawning; }
static void    GP_SetPassiveSpawning(cc_bool v) {
	Game_PassiveSpawning = v;
	Options_SetBool(OPT_PASSIVE_SPAWNING, v);
}

static cc_bool GP_GetSurvivalMode(void) { return Game_SurvivalMode; }
static void    GP_SetSurvivalMode(cc_bool v) {
	Game_SurvivalMode = v;
	Options_SetBool(OPT_SURVIVAL_MODE, v);
}

static const char* const CreeperBehavior_Names[CREEPER_BEHAVIOR_COUNT] = {
	"Don't explode", "Explosion attack", "Explode on death"
};
static int  GP_GetCreeperBehavior(void) { return Game_CreeperBehavior; }
static void GP_SetCreeperBehavior(int v) {
	Game_CreeperBehavior = v;
	Options_SetInt(OPT_CREEPER_BEHAVIOR, v);
}

static cc_bool GP_GetSpiderWallclimb(void) { return Game_SpiderWallclimb; }
static void    GP_SetSpiderWallclimb(cc_bool v) {
	Game_SpiderWallclimb = v;
	Options_SetBool(OPT_SPIDER_WALLCLIMB, v);
}

static cc_bool GP_GetSkeletonShoot(void) { return Game_SkeletonShoot; }
static void    GP_SetSkeletonShoot(cc_bool v) {
	Game_SkeletonShoot = v;
	Options_SetBool(OPT_SKELETON_SHOOT, v);
}

static const char* const SpiderLeapDist_Names[SPIDER_LEAP_DIST_COUNT] = {
	"Don't leap", "3", "4", "5", "6", "7", "8"
};
static int  GP_GetSpiderLeapDist(void) { return Game_SpiderLeapDist; }
static void GP_SetSpiderLeapDist(int v) {
	Game_SpiderLeapDist = v;
	Options_SetInt(OPT_SPIDER_LEAP_DIST, v);
}

static cc_bool GP_GetSpiderVariants(void) { return Game_SpiderVariants; }
static void    GP_SetSpiderVariants(cc_bool v) {
	Game_SpiderVariants = v;
	Options_SetBool(OPT_SPIDER_VARIANTS, v);
}

static cc_bool GP_GetCreeperVariants(void) { return Game_CreeperVariants; }
static void    GP_SetCreeperVariants(cc_bool v) {
	struct MenuOptionsScreen* s = &MenuOptionsScreen_Instance;
	Game_CreeperVariants = v;
	Options_SetBool(OPT_CREEPER_VARIANTS, v);

	if (v) {
		/* Force behavior to "Explosion attack" when variants are enabled */
		Game_CreeperBehavior = CREEPER_EXPLOSION_ATK;
		Options_SetInt(OPT_CREEPER_BEHAVIOR, CREEPER_EXPLOSION_ATK);
		MenuOptionsScreen_Update(s, &s->buttons[4]);
	}
	Widget_SetDisabled(&s->buttons[4], v);
}

static const char* const ZombieSpeed_Names[ZOMBIE_SPEED_COUNT] = {
	"25%", "50%", "75%", "100%"
};
static int  GP_GetZombieSpeed(void) { return Game_ZombieSpeed; }
static void GP_SetZombieSpeed(int v) {
	Game_ZombieSpeed = v;
	Options_SetInt(OPT_ZOMBIE_SPEED, v);
}

static const char* const MobSpawnRate_Names[MOB_SPAWN_RATE_COUNT] = {
	"Off", "Slow (60s)", "Normal (30s)", "Fast (15s)", "Very fast (5s)"
};
static int  GP_GetMobSpawnRate(void) { return Game_MobSpawnRate; }
static void GP_SetMobSpawnRate(int v) {
	Game_MobSpawnRate = v;
	Options_SetInt(OPT_MOB_SPAWN_RATE, v);
}

static cc_bool GP_GetLightRestrictSpawning(void) { return Game_LightRestrictSpawning; }
static void    GP_SetLightRestrictSpawning(cc_bool v) {
	Game_LightRestrictSpawning = v;
	Options_SetBool(OPT_LIGHT_RESTRICT_SPAWN, v);
}

static void Menu_SwitchMobBehaviors(void* a, void* b) { MobBehaviorsScreen_Show(); }

static const char* const MobLightSensitivity_Names[MOB_LIGHT_SENSITIVITY_COUNT] = {
	"None", "Undead", "All"
};
static int  GP_GetMobLightSensitivity(void) { return Game_MobLightSensitivity; }
static void GP_SetMobLightSensitivity(int v) {
	Game_MobLightSensitivity = v;
	Options_SetInt(OPT_MOB_LIGHT_SENSITIVITY, v);
}

static void GameplayScreen_SwitchBack(void* a, void* b) {
	if (Gui.ClassicMenu) { Menu_SwitchPause(a, b); } else { Menu_SwitchOptions(a, b); }
}

static void GameplayOptionsScreen_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddBool(s, "Enable enemy spawning",
			GP_GetEnemySpawning,   GP_SetEnemySpawning, NULL);
		MenuOptionsScreen_AddBool(s, "Enable passive spawning",
			GP_GetPassiveSpawning, GP_SetPassiveSpawning, NULL);
		MenuOptionsScreen_AddBool(s, "Enable survival mode",
			GP_GetSurvivalMode,    GP_SetSurvivalMode, NULL);
		MenuOptionsScreen_AddEnum(s, "Spawn rate",
			MobSpawnRate_Names, MOB_SPAWN_RATE_COUNT,
			GP_GetMobSpawnRate, GP_SetMobSpawnRate, NULL);
		MenuOptionsScreen_AddBool(s, "Light restrictive spawning",
			GP_GetLightRestrictSpawning, GP_SetLightRestrictSpawning, NULL);
		MenuOptionsScreen_AddEnum(s, "Mob light sensitivity",
			MobLightSensitivity_Names, MOB_LIGHT_SENSITIVITY_COUNT,
			GP_GetMobLightSensitivity, GP_SetMobLightSensitivity, NULL);
		MenuOptionsScreen_AddButton(s, "Mob behaviors...",
			Menu_SwitchMobBehaviors, NULL, NULL, NULL);
	}
	MenuOptionsScreen_EndButtons(s, -1, GameplayScreen_SwitchBack);
}

void GameplayOptionsScreen_Show(void) {
	MenuOptionsScreen_Show(GameplayOptionsScreen_InitWidgets);
}

static void MobBehaviorsScreen_SwitchBack(void* a, void* b) { GameplayOptionsScreen_Show(); }

static void MobBehaviorsScreen_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddBool(s, "Enable spider wallclimb",
			GP_GetSpiderWallclimb, GP_SetSpiderWallclimb, NULL);
		MenuOptionsScreen_AddBool(s, "Skeletons shoot",
			GP_GetSkeletonShoot,   GP_SetSkeletonShoot, NULL);
		MenuOptionsScreen_AddBool(s, "Enable creeper variants",
			GP_GetCreeperVariants, GP_SetCreeperVariants, NULL);
		MenuOptionsScreen_AddBool(s, "Enable spider variants",
			GP_GetSpiderVariants,  GP_SetSpiderVariants, NULL);
		MenuOptionsScreen_AddEnum(s, "Creeper behavior",
			CreeperBehavior_Names, CREEPER_BEHAVIOR_COUNT,
			GP_GetCreeperBehavior, GP_SetCreeperBehavior, NULL);
		MenuOptionsScreen_AddEnum(s, "Spider leap distance",
			SpiderLeapDist_Names, SPIDER_LEAP_DIST_COUNT,
			GP_GetSpiderLeapDist, GP_SetSpiderLeapDist, NULL);
		MenuOptionsScreen_AddEnum(s, "Zombie speed",
			ZombieSpeed_Names, ZOMBIE_SPEED_COUNT,
			GP_GetZombieSpeed, GP_SetZombieSpeed, NULL);
	}
	MenuOptionsScreen_EndButtons(s, -1, MobBehaviorsScreen_SwitchBack);

	/* Row 1: wallclimb (0) and shoot (1) side by side */
	Widget_SetLocation(&s->buttons[0], ANCHOR_CENTRE, ANCHOR_CENTRE, -160, -100);
	Widget_SetLocation(&s->buttons[1], ANCHOR_CENTRE, ANCHOR_CENTRE,  160, -100);
	/* Row 2: creeper variants (2) and spider variants (3) side by side */
	Widget_SetLocation(&s->buttons[2], ANCHOR_CENTRE, ANCHOR_CENTRE, -160, -50);
	Widget_SetLocation(&s->buttons[3], ANCHOR_CENTRE, ANCHOR_CENTRE,  160, -50);
	/* Row 3: creeper behavior (4) centered and wider */
	s->buttons[4].minWidth = (Window_Main.Width <= 320) ? 300 : 400;
	Widget_SetLocation(&s->buttons[4], ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 0);
	/* Row 4: spider leap distance (5) centered and wider */
	s->buttons[5].minWidth = (Window_Main.Width <= 320) ? 300 : 400;
	Widget_SetLocation(&s->buttons[5], ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 50);
	/* Row 5: zombie speed (6) centered and wider */
	s->buttons[6].minWidth = (Window_Main.Width <= 320) ? 300 : 400;
	Widget_SetLocation(&s->buttons[6], ANCHOR_CENTRE, ANCHOR_CENTRE, 0, 100);

	/* Disable creeper behavior when variants are enabled */
	Widget_SetDisabled(&s->buttons[4], Game_CreeperVariants);
}

void MobBehaviorsScreen_Show(void) {
	MenuOptionsScreen_Show(MobBehaviorsScreen_InitWidgets);
}


/*########################################################################################################################*
*-------------------------------------------------CustomThemeScreens------------------------------------------------------*
*#########################################################################################################################*/
static void Menu_SwitchCustomTheme(void* a, void* b) { CustomThemeGroupScreen_Show(); }
static void Menu_SwitchGenLevel(void* a, void* b)    { GenLevelScreen_Show(); }

/* Block names and IDs for dropdown selection in custom theme screens */
#define CT_BLOCK_COUNT 40
static const char* const CT_BlockNames[CT_BLOCK_COUNT] = {
	"Air", "Stone", "Grass", "Dirt", "Cobblestone", "Wood",
	"Sapling", "Bedrock", "Water", "Still Water", "Lava", "Still Lava",
	"Sand", "Gravel", "Gold Ore", "Iron Ore", "Coal Ore",
	"Log", "Leaves", "Sponge", "Glass",
	"Gold Block", "Iron Block", "Double Slab", "Slab",
	"Brick", "Mossy Cobble", "Obsidian",
	"Diamond Block", "Diamond Ore", "Red Ore",
	"Cactus", "Snow", "Ice", "Snow Block",
	"Brown Shroom", "Red Shroom",
	"Cobweb", "Bookshelf", "TNT"
};
static const BlockRaw CT_BlockIDs[CT_BLOCK_COUNT] = {
	BLOCK_AIR, BLOCK_STONE, BLOCK_GRASS, BLOCK_DIRT, BLOCK_COBBLE, BLOCK_WOOD,
	BLOCK_SAPLING, BLOCK_BEDROCK, BLOCK_WATER, BLOCK_STILL_WATER, BLOCK_LAVA, BLOCK_STILL_LAVA,
	BLOCK_SAND, BLOCK_GRAVEL, BLOCK_GOLD_ORE, BLOCK_IRON_ORE, BLOCK_COAL_ORE,
	BLOCK_LOG, BLOCK_LEAVES, BLOCK_SPONGE, BLOCK_GLASS,
	BLOCK_GOLD, BLOCK_IRON, BLOCK_DOUBLE_SLAB, BLOCK_SLAB,
	BLOCK_BRICK, BLOCK_MOSSY_ROCKS, BLOCK_OBSIDIAN,
	BLOCK_DIAMOND_BLOCK, BLOCK_DIAMOND_ORE, BLOCK_RED_ORE,
	BLOCK_CACTUS, BLOCK_SNOW, BLOCK_ICE, BLOCK_SNOW_BLOCK,
	BLOCK_BROWN_SHROOM, BLOCK_RED_SHROOM,
	BLOCK_COBWEB, BLOCK_BOOKSHELF, BLOCK_TNT
};

/* Helper: find index in CT_BlockIDs for a given block ID, returns 0 (Air) if not found */
static int CT_BlockToIndex(BlockRaw block) {
	int i;
	for (i = 0; i < CT_BLOCK_COUNT; i++) {
		if (CT_BlockIDs[i] == block) return i;
	}
	return 0;
}


/*----- Blocks sub-screen -----*/
static int  CT_GetSurface(void)  { return CT_BlockToIndex(Gen_CustomTheme.surfaceBlock); }
static void CT_SetSurface(int v) { Gen_CustomTheme.surfaceBlock = CT_BlockIDs[v]; CustomTheme_Save(); }
static int  CT_GetFill(void)     { return CT_BlockToIndex(Gen_CustomTheme.fillBlock); }
static void CT_SetFill(int v)    { Gen_CustomTheme.fillBlock = CT_BlockIDs[v]; CustomTheme_Save(); }
static int  CT_GetStone(void)    { return CT_BlockToIndex(Gen_CustomTheme.stoneBlock); }
static void CT_SetStone(int v)   { Gen_CustomTheme.stoneBlock = CT_BlockIDs[v]; CustomTheme_Save(); }
static int  CT_GetFluid(void)    { return CT_BlockToIndex(Gen_CustomTheme.fluidBlock); }
static void CT_SetFluid(int v)   { Gen_CustomTheme.fluidBlock = CT_BlockIDs[v]; CustomTheme_Save(); }
static int  CT_GetEdgeFluid(void)  { return CT_BlockToIndex(Gen_CustomTheme.edgeFluidBlock); }
static void CT_SetEdgeFluid(int v) { Gen_CustomTheme.edgeFluidBlock = CT_BlockIDs[v]; CustomTheme_Save(); }
static int  CT_GetUnderwater(void)  { return CT_BlockToIndex(Gen_CustomTheme.underwaterBlock); }
static void CT_SetUnderwater(int v) { Gen_CustomTheme.underwaterBlock = CT_BlockIDs[v]; CustomTheme_Save(); }
static int  CT_GetEdge(void)    { return CT_BlockToIndex(Gen_CustomTheme.edgeBlock); }
static void CT_SetEdge(int v)   { Gen_CustomTheme.edgeBlock = CT_BlockIDs[v]; CustomTheme_Save(); }
static int  CT_GetSides(void)   { return CT_BlockToIndex(Gen_CustomTheme.sidesBlock); }
static void CT_SetSides(int v)  { Gen_CustomTheme.sidesBlock = CT_BlockIDs[v]; CustomTheme_Save(); }
static int  CT_GetCaveFill(void)  { return CT_BlockToIndex(Gen_CustomTheme.caveFillBlock); }
static void CT_SetCaveFill(int v) { Gen_CustomTheme.caveFillBlock = CT_BlockIDs[v]; CustomTheme_Save(); }

static int  CT_GetEdgeOffset(void)  { return Gen_CustomTheme.edgeHeightOffset; }
static void CT_SetEdgeOffset(int v) { Gen_CustomTheme.edgeHeightOffset = v; CustomTheme_Save(); }

static void CTBlocks_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddEnum(s, "Surface block",
			CT_BlockNames, CT_BLOCK_COUNT, CT_GetSurface, CT_SetSurface,
			"Block used for the ground surface\n(e.g. Grass, Sand)");
		MenuOptionsScreen_AddEnum(s, "Fill block",
			CT_BlockNames, CT_BLOCK_COUNT, CT_GetFill, CT_SetFill,
			"Block used to fill below surface\n(e.g. Dirt, Sand)");
		MenuOptionsScreen_AddEnum(s, "Stone block",
			CT_BlockNames, CT_BLOCK_COUNT, CT_GetStone, CT_SetStone,
			"Block used instead of stone\nin terrain generation");
		MenuOptionsScreen_AddEnum(s, "Water top block",
			CT_BlockNames, CT_BLOCK_COUNT, CT_GetEdgeFluid, CT_SetEdgeFluid,
			"Block used for water surface\n(e.g. Ice in winter)");
		MenuOptionsScreen_AddEnum(s, "Water below block",
			CT_BlockNames, CT_BLOCK_COUNT, CT_GetFluid, CT_SetFluid,
			"Block used for water below surface\n(internal flood fill)");
		MenuOptionsScreen_AddEnum(s, "Underwater floor",
			CT_BlockNames, CT_BLOCK_COUNT, CT_GetUnderwater, CT_SetUnderwater,
			"Block placed on floor underwater\n(e.g. Gravel)");
		MenuOptionsScreen_AddEnum(s, "Edge block",
			CT_BlockNames, CT_BLOCK_COUNT, CT_GetEdge, CT_SetEdge,
			"Block shown at world border horizon\n(0=Air means use water top block)");
		MenuOptionsScreen_AddEnum(s, "Sides block",
			CT_BlockNames, CT_BLOCK_COUNT, CT_GetSides, CT_SetSides,
			"Block shown on sides below border\n(e.g. Bedrock, Obsidian)");
		MenuOptionsScreen_AddEnum(s, "Cave fill block",
			CT_BlockNames, CT_BLOCK_COUNT, CT_GetCaveFill, CT_SetCaveFill,
			"Block that fills cave worlds\n(e.g. Stone, Dirt)");
		MenuOptionsScreen_AddInt(s, "Edge height offset",
			-64, 64, 0, CT_GetEdgeOffset, CT_SetEdgeOffset,
			"Offset for edge water height\nfrom default (height/2)");
	}
	MenuOptionsScreen_EndButtons(s, -1, Menu_SwitchCustomTheme);
}

static void CTBlocks_Show(void) {
	MenuOptionsScreen_Show(CTBlocks_InitWidgets);
}


/*----- Colors sub-screen -----*/
static PackedCol CT_GetSky(void)     { return Gen_CustomTheme.skyCol; }
static void      CT_SetSky(PackedCol v) { Gen_CustomTheme.skyCol = v; CustomTheme_Save(); }
static PackedCol CT_GetFog(void)     { return Gen_CustomTheme.fogCol; }
static void      CT_SetFog(PackedCol v) { Gen_CustomTheme.fogCol = v; CustomTheme_Save(); }
static PackedCol CT_GetClouds(void)  { return Gen_CustomTheme.cloudsCol; }
static void      CT_SetClouds(PackedCol v) { Gen_CustomTheme.cloudsCol = v; CustomTheme_Save(); }
static PackedCol CT_GetShadow(void)  { return Gen_CustomTheme.shadowCol; }
static void      CT_SetShadow(PackedCol v) { Gen_CustomTheme.shadowCol = v; CustomTheme_Save(); }

static void CTColors_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddHex(s, "Sky color", ENV_DEFAULT_SKY_COLOR,
			CT_GetSky, CT_SetSky,
			"Sky color (set to default to\nuse engine default)");
		MenuOptionsScreen_AddHex(s, "Fog color", ENV_DEFAULT_FOG_COLOR,
			CT_GetFog, CT_SetFog,
			"Fog color (set to default to\nuse engine default)");
		MenuOptionsScreen_AddHex(s, "Clouds color", ENV_DEFAULT_CLOUDS_COLOR,
			CT_GetClouds, CT_SetClouds,
			"Cloud color (set to default to\nuse engine default)");
		MenuOptionsScreen_AddHex(s, "Shadow color", ENV_DEFAULT_SHADOW_COLOR,
			CT_GetShadow, CT_SetShadow,
			"Shadow color (set to default to\nuse engine default)");
	}
	MenuOptionsScreen_EndButtons(s, -1, Menu_SwitchCustomTheme);
}

static void CTColors_Show(void) {
	MenuOptionsScreen_Show(CTColors_InitWidgets);
}


/*----- Vegetation sub-screen -----*/
static int  CT_GetTreeMul(void)    { return Gen_CustomTheme.treePatchMul; }
static void CT_SetTreeMul(int v)   { Gen_CustomTheme.treePatchMul = v; CustomTheme_Save(); }
static int  CT_GetFlowerMul(void)  { return Gen_CustomTheme.flowerPatchMul; }
static void CT_SetFlowerMul(int v) { Gen_CustomTheme.flowerPatchMul = v; CustomTheme_Save(); }
static int  CT_GetMushroomMul(void)  { return Gen_CustomTheme.mushroomPatchMul; }
static void CT_SetMushroomMul(int v) { Gen_CustomTheme.mushroomPatchMul = v; CustomTheme_Save(); }

static cc_bool CT_GetGenFlowers(void)    { return Gen_CustomTheme.generateFlowers; }
static void    CT_SetGenFlowers(cc_bool v) { Gen_CustomTheme.generateFlowers = v; CustomTheme_Save(); }
static int  CT_GetCactiMul(void)  { return Gen_CustomTheme.cactiPatchMul; }
static void CT_SetCactiMul(int v) { Gen_CustomTheme.cactiPatchMul = v; CustomTheme_Save(); }
static cc_bool CT_GetJungleTrees(void)    { return Gen_CustomTheme.hasJungleTrees; }
static void    CT_SetJungleTrees(cc_bool v) { Gen_CustomTheme.hasJungleTrees = v; CustomTheme_Save(); }
static cc_bool CT_GetOases(void)    { return Gen_CustomTheme.hasOases; }
static void    CT_SetOases(cc_bool v) { Gen_CustomTheme.hasOases = v; CustomTheme_Save(); }

static void CTVegetation_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddInt(s, "Tree density",
			0, 20, 1, CT_GetTreeMul, CT_SetTreeMul,
			"Multiplier for tree generation\n0=no trees, 8=woods density");
		MenuOptionsScreen_AddInt(s, "Flower density",
			0, 20, 1, CT_GetFlowerMul, CT_SetFlowerMul,
			"Multiplier for flower generation\n0=no flowers, 3=paradise");
		MenuOptionsScreen_AddInt(s, "Mushroom density",
			0, 20, 1, CT_GetMushroomMul, CT_SetMushroomMul,
			"Multiplier for mushroom generation\n0=no mushrooms");
		MenuOptionsScreen_AddBool(s, "Generate flowers",
			CT_GetGenFlowers, CT_SetGenFlowers,
			"Whether flowers are generated");
		MenuOptionsScreen_AddInt(s, "Cacti density",
			0, 20, 0, CT_GetCactiMul, CT_SetCactiMul,
			"Multiplier for cacti generation\n0=no cacti, 1=normal");
		MenuOptionsScreen_AddBool(s, "Jungle trees",
			CT_GetJungleTrees, CT_SetJungleTrees,
			"Generate large 2x2 jungle trees");
		MenuOptionsScreen_AddBool(s, "Oasis patches",
			CT_GetOases, CT_SetOases,
			"Generate desert-style oases\nwith grass and trees");
	}
	MenuOptionsScreen_EndButtons(s, -1, Menu_SwitchCustomTheme);
}

static void CTVegetation_Show(void) {
	MenuOptionsScreen_Show(CTVegetation_InitWidgets);
}


/*----- Features sub-screen -----*/
static cc_bool CT_GetShadowCeil(void)    { return Gen_CustomTheme.hasShadowCeiling; }
static void    CT_SetShadowCeil(cc_bool v) { Gen_CustomTheme.hasShadowCeiling = v; CustomTheme_Save(); }
static cc_bool CT_GetSnowLayer(void)    { return Gen_CustomTheme.hasSnowLayer; }
static void    CT_SetSnowLayer(cc_bool v) { Gen_CustomTheme.hasSnowLayer = v; CustomTheme_Save(); }
static cc_bool CT_GetDirtToGrass(void)    { return Gen_CustomTheme.dirtToGrass; }
static void    CT_SetDirtToGrass(cc_bool v) { Gen_CustomTheme.dirtToGrass = v; CustomTheme_Save(); }
static cc_bool CT_GetCaveGardens(void)    { return Gen_CustomTheme.hasCaveGardens; }
static void    CT_SetCaveGardens(cc_bool v) { Gen_CustomTheme.hasCaveGardens = v; CustomTheme_Save(); }
static cc_bool CT_GetExtraCaveOres(void)    { return Gen_CustomTheme.hasExtraCaveOres; }
static void    CT_SetExtraCaveOres(cc_bool v) { Gen_CustomTheme.hasExtraCaveOres = v; CustomTheme_Save(); }
static cc_bool CT_GetTreesOnDirt(void)    { return Gen_CustomTheme.treesOnDirt; }
static void    CT_SetTreesOnDirt(cc_bool v) { Gen_CustomTheme.treesOnDirt = v; CustomTheme_Save(); }
static cc_bool CT_GetRaiseWater(void)    { return Gen_CustomTheme.raiseWaterLevel; }
static void    CT_SetRaiseWater(cc_bool v) { Gen_CustomTheme.raiseWaterLevel = v; CustomTheme_Save(); }

static void CTFeatures_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddBool(s, "Shadow ceiling",
			CT_GetShadowCeil, CT_SetShadowCeil,
			"Light-blocking ceiling at top\n(hell-style darkness)");
		MenuOptionsScreen_AddBool(s, "Snow layer",
			CT_GetSnowLayer, CT_SetSnowLayer,
			"Place snow on surfaces\nand tree leaves");
		MenuOptionsScreen_AddBool(s, "Dirt to grass",
			CT_GetDirtToGrass, CT_SetDirtToGrass,
			"Dirt converts to grass\nwhen exposed to light");
		MenuOptionsScreen_AddBool(s, "Cave gardens",
			CT_GetCaveGardens, CT_SetCaveGardens,
			"Generate garden areas\ninside caves");
		MenuOptionsScreen_AddBool(s, "Extra cave ores",
			CT_GetExtraCaveOres, CT_SetExtraCaveOres,
			"Generate cobble and mossy\ncobble in cave walls");
		MenuOptionsScreen_AddBool(s, "Trees on dirt",
			CT_GetTreesOnDirt, CT_SetTreesOnDirt,
			"Allow trees to grow on\ndirt blocks (not just grass)");
		MenuOptionsScreen_AddBool(s, "Raise water level",
			CT_GetRaiseWater, CT_SetRaiseWater,
			"Raise water level by height/8\n(paradise-style)");
	}
	MenuOptionsScreen_EndButtons(s, -1, Menu_SwitchCustomTheme);
}

static void CTFeatures_Show(void) {
	MenuOptionsScreen_Show(CTFeatures_InitWidgets);
}


/*----- Terrain sub-screen -----*/
static void CT_GetHeightScale(cc_string* v) { String_AppendFloat(v, Gen_CustomTheme.heightScale, 2); }
static void CT_SetHeightScale(const cc_string* v) { Gen_CustomTheme.heightScale = Menu_Float(v); CustomTheme_Save(); }
static void CT_GetCaveFreq(cc_string* v) { String_AppendFloat(v, Gen_CustomTheme.caveFreqScale, 2); }
static void CT_SetCaveFreq(const cc_string* v) { Gen_CustomTheme.caveFreqScale = Menu_Float(v); CustomTheme_Save(); }

static void CTTerrain_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddNum(s, "Height scale",
			0.1f, 5.0f, 1.0f, CT_GetHeightScale, CT_SetHeightScale,
			"Terrain height multiplier\n0.5=flat, 1.0=normal, 2.0+=extreme");
		MenuOptionsScreen_AddNum(s, "Cave frequency",
			0.0f, 10.0f, 1.0f, CT_GetCaveFreq, CT_SetCaveFreq,
			"Cave generation multiplier\n0=no caves, 1=normal, 5=very cavernous");
	}
	MenuOptionsScreen_EndButtons(s, -1, Menu_SwitchCustomTheme);
}

static void CTTerrain_Show(void) {
	MenuOptionsScreen_Show(CTTerrain_InitWidgets);
}


/*----- Ores sub-screen (edit individual ore) -----*/
static int ct_editingOre = 0;

static int  CTOreEdit_GetBlock(void)  { return CT_BlockToIndex(Gen_CustomOres[ct_editingOre].block); }
static void CTOreEdit_SetBlock(int v) { Gen_CustomOres[ct_editingOre].block = CT_BlockIDs[v]; CustomTheme_Save(); }
static void CTOreEdit_GetAbundance(cc_string* v) { String_AppendFloat(v, Gen_CustomOres[ct_editingOre].abundance, 2); }
static void CTOreEdit_SetAbundance(const cc_string* v) { Gen_CustomOres[ct_editingOre].abundance = Menu_Float(v); CustomTheme_Save(); }
static cc_bool CTOreEdit_GetEnabled(void) { return Gen_CustomOres[ct_editingOre].enabled; }
static void    CTOreEdit_SetEnabled(cc_bool v) { Gen_CustomOres[ct_editingOre].enabled = v; CustomTheme_Save(); }

static void CTOres_Show(void); /* forward declaration */
static void CTOreEdit_SwitchBack(void* a, void* b) { CTOres_Show(); }

static void CTOreEdit_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddBool(s, "Enabled",
			CTOreEdit_GetEnabled, CTOreEdit_SetEnabled, NULL);
		MenuOptionsScreen_AddEnum(s, "Ore block",
			CT_BlockNames, CT_BLOCK_COUNT, CTOreEdit_GetBlock, CTOreEdit_SetBlock,
			"Which block to generate as ore");
		MenuOptionsScreen_AddNum(s, "Abundance",
			0.0f, 2.0f, 0.5f, CTOreEdit_GetAbundance, CTOreEdit_SetAbundance,
			"How common this ore is\n0.4=rare, 0.7=medium, 0.9=common");
	}
	MenuOptionsScreen_EndButtons(s, -1, CTOreEdit_SwitchBack);
}

static void CTOreEdit_Show(void) {
	MenuOptionsScreen_Show(CTOreEdit_InitWidgets);
}

/*----- Ores list sub-screen -----*/
static void CTOres_GetOreLabel(struct ButtonWidget* btn, cc_string* v) {
	int i = (int)((union MenuOptionMeta*)btn->meta.ptr - menuOpts_meta);
	struct OreDefinition* ore = &Gen_CustomOres[i];
	if (ore->enabled) {
		String_AppendConst(v, CT_BlockNames[CT_BlockToIndex(ore->block)]);
	} else {
		String_AppendConst(v, "Disabled");
	}
}

static void CTOres_ClickOre(void* screen, void* widget) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	ct_editingOre = (int)((struct ButtonWidget*)widget - s->buttons);
	CTOreEdit_Show();
}

static void CTOres_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddButton(s, "Ore 1",  CTOres_ClickOre, CTOres_GetOreLabel, NULL, NULL);
		MenuOptionsScreen_AddButton(s, "Ore 2",  CTOres_ClickOre, CTOres_GetOreLabel, NULL, NULL);
		MenuOptionsScreen_AddButton(s, "Ore 3",  CTOres_ClickOre, CTOres_GetOreLabel, NULL, NULL);
		MenuOptionsScreen_AddButton(s, "Ore 4",  CTOres_ClickOre, CTOres_GetOreLabel, NULL, NULL);
		MenuOptionsScreen_AddButton(s, "Ore 5",  CTOres_ClickOre, CTOres_GetOreLabel, NULL, NULL);
		MenuOptionsScreen_AddButton(s, "Ore 6",  CTOres_ClickOre, CTOres_GetOreLabel, NULL, NULL);
		MenuOptionsScreen_AddButton(s, "Ore 7",  CTOres_ClickOre, CTOres_GetOreLabel, NULL, NULL);
		MenuOptionsScreen_AddButton(s, "Ore 8",  CTOres_ClickOre, CTOres_GetOreLabel, NULL, NULL);
		MenuOptionsScreen_AddButton(s, "Ore 9",  CTOres_ClickOre, CTOres_GetOreLabel, NULL, NULL);
		MenuOptionsScreen_AddButton(s, "Ore 10", CTOres_ClickOre, CTOres_GetOreLabel, NULL, NULL);
	}
	MenuOptionsScreen_EndButtons(s, -1, Menu_SwitchCustomTheme);
}

static void CTOres_Show(void) {
	MenuOptionsScreen_Show(CTOres_InitWidgets);
}


/*----- Save Preset sub-screen -----*/
static void CTPresetSave_Show(void);
static int ct_pendingSaveSlot;

static void CTPresetSave_GetLabel(struct ButtonWidget* btn, cc_string* v) {
	int i = (int)((union MenuOptionMeta*)btn->meta.ptr - menuOpts_meta);
	if (CustomTheme_HasPreset(i)) {
		CustomTheme_GetPresetName(i, v);
		if (!v->length) String_AppendConst(v, "Unnamed");
	} else {
		String_AppendConst(v, "Empty");
	}
}

static void CTPresetSave_NameDone(const cc_string* value, cc_bool valid) {
	if (!valid) return;
	CustomTheme_SavePreset(ct_pendingSaveSlot);
	CustomTheme_SetPresetName(ct_pendingSaveSlot, value);
	/* Re-show the save screen to update labels */
	CTPresetSave_Show();
}

static void CTPresetSave_Click(void* screen, void* widget) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	static struct MenuInputDesc desc;
	cc_string name; char nameBuf[STRING_SIZE];

	ct_pendingSaveSlot = (int)((struct ButtonWidget*)widget - s->buttons);

	/* Show text input to prompt for a name */
	String_InitArray(name, nameBuf);
	if (CustomTheme_HasPreset(ct_pendingSaveSlot)) {
		CustomTheme_GetPresetName(ct_pendingSaveSlot, &name);
	}
	MenuInput_String(desc);
	MenuInputOverlay_Show(&desc, &name, CTPresetSave_NameDone, false);
}

static void CTPresetSave_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddButton(s, "Slot 1",  CTPresetSave_Click, CTPresetSave_GetLabel, NULL,
			"Save current theme to slot 1");
		MenuOptionsScreen_AddButton(s, "Slot 2",  CTPresetSave_Click, CTPresetSave_GetLabel, NULL,
			"Save current theme to slot 2");
		MenuOptionsScreen_AddButton(s, "Slot 3",  CTPresetSave_Click, CTPresetSave_GetLabel, NULL,
			"Save current theme to slot 3");
		MenuOptionsScreen_AddButton(s, "Slot 4",  CTPresetSave_Click, CTPresetSave_GetLabel, NULL,
			"Save current theme to slot 4");
		MenuOptionsScreen_AddButton(s, "Slot 5",  CTPresetSave_Click, CTPresetSave_GetLabel, NULL,
			"Save current theme to slot 5");
		MenuOptionsScreen_AddButton(s, "Slot 6",  CTPresetSave_Click, CTPresetSave_GetLabel, NULL,
			"Save current theme to slot 6");
		MenuOptionsScreen_AddButton(s, "Slot 7",  CTPresetSave_Click, CTPresetSave_GetLabel, NULL,
			"Save current theme to slot 7");
		MenuOptionsScreen_AddButton(s, "Slot 8",  CTPresetSave_Click, CTPresetSave_GetLabel, NULL,
			"Save current theme to slot 8");
		MenuOptionsScreen_AddButton(s, "Slot 9",  CTPresetSave_Click, CTPresetSave_GetLabel, NULL,
			"Save current theme to slot 9");
		MenuOptionsScreen_AddButton(s, "Slot 10", CTPresetSave_Click, CTPresetSave_GetLabel, NULL,
			"Save current theme to slot 10");
	}
	MenuOptionsScreen_EndButtons(s, -1, Menu_SwitchCustomTheme);
}

static void CTPresetSave_Show(void) {
	MenuOptionsScreen_Show(CTPresetSave_InitWidgets);
}

/*----- Load Preset sub-screen -----*/
static void CTPresetLoad_GetLabel(struct ButtonWidget* btn, cc_string* v) {
	int i = (int)((union MenuOptionMeta*)btn->meta.ptr - menuOpts_meta);
	if (CustomTheme_HasPreset(i)) {
		CustomTheme_GetPresetName(i, v);
		if (!v->length) String_AppendConst(v, "Unnamed");
	} else {
		String_AppendConst(v, "Empty");
	}
}

static void CTPresetLoad_Click(void* screen, void* widget) {
	struct MenuOptionsScreen* s = (struct MenuOptionsScreen*)screen;
	int slot = (int)((struct ButtonWidget*)widget - s->buttons);
	if (!CustomTheme_HasPreset(slot)) return;
	CustomTheme_LoadPreset(slot);
	CustomThemeGroupScreen_Show();
}

static void CTPresetLoad_InitWidgets(struct MenuOptionsScreen* s) {
	int i;
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddButton(s, "Slot 1",  CTPresetLoad_Click, CTPresetLoad_GetLabel, NULL,
			"Load theme from slot 1");
		MenuOptionsScreen_AddButton(s, "Slot 2",  CTPresetLoad_Click, CTPresetLoad_GetLabel, NULL,
			"Load theme from slot 2");
		MenuOptionsScreen_AddButton(s, "Slot 3",  CTPresetLoad_Click, CTPresetLoad_GetLabel, NULL,
			"Load theme from slot 3");
		MenuOptionsScreen_AddButton(s, "Slot 4",  CTPresetLoad_Click, CTPresetLoad_GetLabel, NULL,
			"Load theme from slot 4");
		MenuOptionsScreen_AddButton(s, "Slot 5",  CTPresetLoad_Click, CTPresetLoad_GetLabel, NULL,
			"Load theme from slot 5");
		MenuOptionsScreen_AddButton(s, "Slot 6",  CTPresetLoad_Click, CTPresetLoad_GetLabel, NULL,
			"Load theme from slot 6");
		MenuOptionsScreen_AddButton(s, "Slot 7",  CTPresetLoad_Click, CTPresetLoad_GetLabel, NULL,
			"Load theme from slot 7");
		MenuOptionsScreen_AddButton(s, "Slot 8",  CTPresetLoad_Click, CTPresetLoad_GetLabel, NULL,
			"Load theme from slot 8");
		MenuOptionsScreen_AddButton(s, "Slot 9",  CTPresetLoad_Click, CTPresetLoad_GetLabel, NULL,
			"Load theme from slot 9");
		MenuOptionsScreen_AddButton(s, "Slot 10", CTPresetLoad_Click, CTPresetLoad_GetLabel, NULL,
			"Load theme from slot 10");
	}
	MenuOptionsScreen_EndButtons(s, -1, Menu_SwitchCustomTheme);

	/* Disable buttons for empty preset slots */
	for (i = 0; i < MAX_THEME_PRESETS; i++) {
		Widget_SetDisabled(&s->buttons[i], !CustomTheme_HasPreset(i));
	}
}

static void CTPresetLoad_Show(void) {
	MenuOptionsScreen_Show(CTPresetLoad_InitWidgets);
}


/*----- Custom Theme Group Screen (category hub) -----*/
static const char* const CT_BaseThemeNames[] = {
	"Normal", "Hell", "Paradise", "Woods", "Desert", "Winter", "Moon", "Jungle", "Plains"
};
#define CT_BASE_THEME_COUNT 9

static int  ct_baseTheme = 0;
static int  CT_GetBaseTheme(void) { return ct_baseTheme; }
static void CT_SetBaseTheme(int v) {
	ct_baseTheme = v;
	CustomTheme_CopyFrom(v);
	CustomTheme_Save();
}

static void Menu_SwitchCTBlocks(void* a, void* b)     { CTBlocks_Show(); }
static void Menu_SwitchCTColors(void* a, void* b)     { CTColors_Show(); }
static void Menu_SwitchCTVegetation(void* a, void* b) { CTVegetation_Show(); }
static void Menu_SwitchCTFeatures(void* a, void* b)   { CTFeatures_Show(); }
static void Menu_SwitchCTOres(void* a, void* b)       { CTOres_Show(); }
static void Menu_SwitchCTTerrain(void* a, void* b)    { CTTerrain_Show(); }
static void Menu_SwitchCTPresetSave(void* a, void* b) { CTPresetSave_Show(); }
static void Menu_SwitchCTPresetLoad(void* a, void* b) { CTPresetLoad_Show(); }

static void CTGroup_InitWidgets(struct MenuOptionsScreen* s) {
	MenuOptionsScreen_BeginButtons(s);
	{
		MenuOptionsScreen_AddEnum(s, "Base theme",
			CT_BaseThemeNames, CT_BASE_THEME_COUNT, CT_GetBaseTheme, CT_SetBaseTheme,
			"Copy settings from a built-in theme\nas a starting point");
		MenuOptionsScreen_AddButton(s, "Blocks...",
			Menu_SwitchCTBlocks, NULL, NULL,
			"Configure terrain blocks\n(surface, stone, water, borders)");
		MenuOptionsScreen_AddButton(s, "Colors...",
			Menu_SwitchCTColors, NULL, NULL,
			"Configure sky, fog, cloud,\nand shadow colors");
		MenuOptionsScreen_AddButton(s, "Vegetation...",
			Menu_SwitchCTVegetation, NULL, NULL,
			"Configure tree, flower, mushroom\ndensity and plant types");
		MenuOptionsScreen_AddButton(s, "Features...",
			Menu_SwitchCTFeatures, NULL, NULL,
			"Toggle shadow ceiling, snow,\ncave gardens, and more");
		MenuOptionsScreen_AddButton(s, "Ores...",
			Menu_SwitchCTOres, NULL, NULL,
			"Configure ore types and\ngeneration frequency");
		MenuOptionsScreen_AddButton(s, "Terrain...",
			Menu_SwitchCTTerrain, NULL, NULL,
			"Height scale and cave frequency\nfor terrain shape");
		MenuOptionsScreen_AddButton(s, "Save preset...",
			Menu_SwitchCTPresetSave, NULL, NULL,
			"Save current custom theme\nto a preset slot");
		MenuOptionsScreen_AddButton(s, "Load preset...",
			Menu_SwitchCTPresetLoad, NULL, NULL,
			"Load a custom theme\nfrom a preset slot");
	}
	MenuOptionsScreen_EndButtons(s, -1, Menu_SwitchGenLevel);
}

void CustomThemeGroupScreen_Show(void) {
	MenuOptionsScreen_Show(CTGroup_InitWidgets);
}

