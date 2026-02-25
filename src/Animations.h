#ifndef CC_ANIMATIONS_H
#define CC_ANIMATIONS_H
#include "Core.h"
/* 
Contains everything relating to texture animations (including default water/lava ones)
Copyright 2014-2025 ClassiCube | Licensed under BSD-3
*/
CC_BEGIN_HEADER

struct IGameComponent;
extern struct IGameComponent Animations_Component;

/* The decoded animations.png bitmap (used for fire overlay, etc.) */
extern struct Bitmap anims_bmp;

CC_END_HEADER
#endif
