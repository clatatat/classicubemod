#ifndef CC_BLOCKPHYSICS_H
#define CC_BLOCKPHYSICS_H
#include "Core.h"
CC_BEGIN_HEADER

/* Implements simple block physics.
   Copyright 2014-2025 ClassiCube | Licensed under BSD-3
*/
typedef void (*PhysicsHandler)(int index, BlockID block);

CC_VAR extern struct Physics_ {
	/* Whether block physics are enabled at all. */
	cc_bool Enabled;
	/* Called when block is activated by a neighbouring block change. */
	/* e.g. trigger sand falling, water flooding */
	PhysicsHandler OnActivate[256];
	/* Called when this block is randomly activated. */
	/* e.g. grass eventually fading to dirt in darkness */
	PhysicsHandler OnRandomTick[256];
	/* Called when user manually places a block. */
	PhysicsHandler OnPlace[256];
	/* Called when user manually deletes a block. */
	PhysicsHandler OnDelete[256];
} Physics;

void Physics_SetEnabled(cc_bool enabled);
void Physics_OnBlockChanged(int x, int y, int z, BlockID old, BlockID now);
void Physics_Init(void);
void Physics_Free(void);
void Physics_Tick(void);
/* Schedule a TNT block to explode after a 5-second fuse */
void TNT_ScheduleFuse(int x, int y, int z);
/* Immediately explode at the given world position (no fuse) */
void TNT_Explode(int x, int y, int z);
/* Immediately explode at the given world position with custom radius */
void TNT_ExplodeRadius(int x, int y, int z, int power);

CC_END_HEADER
#endif
