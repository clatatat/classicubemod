#ifndef CC_MINECART_H
#define CC_MINECART_H
#include "Core.h"
#include "Vectors.h"
CC_BEGIN_HEADER

/* Minecart entity system - simulates Minecraft Alpha 1.0.6 minecart behavior.
   Copyright 2014-2025 ClassiCube | Licensed under BSD-3 */

struct IGameComponent;
struct ScheduledTask;
struct Model;
struct ModelTex;

extern struct IGameComponent Minecart_Component;

/* Maximum number of active minecarts in the world */
#define MAX_MINECARTS 64

/* Minecart state */
struct Minecart {
	cc_bool active;
	Vec3 pos;            /* World position (center of cart) */
	Vec3 velocity;       /* Motion per tick */
	float yaw;           /* Visual rotation */
	float pitch;         /* Visual tilt (for slopes) */
	int entityId;        /* Entity slot in Entities.List for rendering */
	int riderId;         /* Entity ID of rider, or -1 if empty */
	int timeSinceHit;    /* Damage wobble timer */
	float damageTaken;   /* Accumulated damage for wobble amplitude */
	int forwardDir;      /* 1 or -1, flips on hit */
	cc_bool isInReverse; /* Whether cart is visually reversed */
};

extern struct Minecart Minecarts[MAX_MINECARTS];

/* Spawns a minecart at the given rail position. Returns slot index or -1 on failure. */
int Minecart_Spawn(float x, float y, float z);
/* Removes a minecart and its entity. */
void Minecart_Despawn(int slot);
/* Returns the index of the minecart closest to (x,y,z) within maxDist, or -1. */
int Minecart_FindClosest(Vec3 pos, float maxDist);
/* Attempts to have the current player ride the given minecart. */
void Minecart_RideCart(int slot);
/* Dismounts the current player from any minecart they're riding. */
void Minecart_DismountPlayer(void);
/* Returns the slot index the player is currently riding, or -1. */
int Minecart_GetPlayerCart(void);

/* Model/texture registration (called from Model.c) */
void MinecartModel_Register(void);
extern struct ModelTex minecart_tex;

CC_END_HEADER
#endif
