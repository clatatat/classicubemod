#ifndef CC_SIGNS_H
#define CC_SIGNS_H
#include "Core.h"
CC_BEGIN_HEADER

/*
   World-space sign text renderer for survival mod.
   Copyright 2014-2025 ClassiCube | Licensed under BSD-3
*/

/* Renders all sign text as fixed-orientation world-space quads.
   Call this from Render3DFrame, after EntityNames_Render(). */
void Signs_RenderText(void);

/* Marks the cached texture for the sign at (x,y,z) as dirty so it is re-rasterized
   on the next Signs_RenderText call. Call this after editing sign text. */
void Signs_InvalidateAt(int x, int y, int z);

/* Frees all cached sign textures and the shared vertex buffer. */
void Signs_Free(void);

/* Registers event handlers (call once at startup). */
void Signs_Init(void);

CC_END_HEADER
#endif
