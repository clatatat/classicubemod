#include "Minecart.h"
#include "Entity.h"
#include "Model.h"
#include "Block.h"
#include "World.h"
#include "Game.h"
#include "Event.h"
#include "ExtMath.h"
#include "Camera.h"
#include "Graphics.h"
#include "Funcs.h"
#include "Audio.h"
#include "Chat.h"
#include "Input.h"
#include "Lighting.h"
#include "MapRenderer.h"
#include "BlockID.h"
#include "Constants.h"
#include "Platform.h"

struct Minecart Minecarts[MAX_MINECARTS];

/* Physics constants matching Minecraft Alpha 1.0.6 */
#define MC_GRAVITY          0.04f
#define MC_MAX_SPEED        0.4f
#define MC_SLOPE_PUSH       (1.0f / 128.0f)
#define MC_FRICTION_RIDDEN  0.997f
#define MC_FRICTION_EMPTY   0.96f
#define MC_RIDER_SPEED_MULT 0.75f
#define MC_OFF_RAIL_GROUND  0.5f
#define MC_OFF_RAIL_AIR     0.95f
#define MC_PUSH_FACTOR      0.1f
#define MC_ENTITY_SCAN_PAD  0.2f

/* Ride height offset: rider sits 0.3 blocks below cart origin (from Alpha: 0.0 - 0.3) */
#define MC_RIDE_OFFSET     -0.3f

/* Rail direction endpoint matrix matching Alpha's matrix[10][2][3]
   We encode: meta 0=flat_NS, 1=flat_EW, 2=slope_asc_E, 3=slope_asc_W,
   4=slope_asc_N, 5=slope_asc_S, 6=curve_SE, 7=curve_SW, 8=curve_NW, 9=curve_NE
   Each entry: two endpoints as {dx, dy, dz} */
static const int railMatrix[10][2][3] = {
	/* 0: flat N-S */    {{ 0, 0,-1}, { 0, 0, 1}},
	/* 1: flat E-W */    {{-1, 0, 0}, { 1, 0, 0}},
	/* 2: slope asc E */ {{-1,-1, 0}, { 1, 0, 0}},
	/* 3: slope asc W */ {{-1, 0, 0}, { 1,-1, 0}},
	/* 4: slope asc N */ {{ 0, 0,-1}, { 0,-1, 1}},
	/* 5: slope asc S */ {{ 0,-1,-1}, { 0, 0, 1}},
	/* 6: curve SE */    {{ 0, 0, 1}, { 1, 0, 0}},
	/* 7: curve SW */    {{ 0, 0, 1}, {-1, 0, 0}},
	/* 8: curve NW */    {{ 0, 0,-1}, {-1, 0, 0}},
	/* 9: curve NE */    {{ 0, 0,-1}, { 1, 0, 0}},
};


/*########################################################################################################################*
*----------------------------------------------------Rail metadata--------------------------------------------------------*
*#########################################################################################################################*/
/* Determine the Alpha-style rail metadata for a given rail block position.
   This maps our connection-based rail system to Alpha's 0-9 metadata values. */
static int GetRailMeta(int x, int y, int z) {
	int encoded, rotation, sloped, highEnd;
	
	if (!World_Contains(x, y, z)) return -1;
	if (!IsRail(World_GetBlock(x, y, z))) return -1;
	
	encoded  = Rail_GetTextureAndRotation(x, y, z, FACE_YMAX);
	rotation = RAIL_DECODE_ROT(encoded);
	sloped   = RAIL_DECODE_SLOPED(encoded);
	highEnd  = RAIL_DECODE_HIGHEND(encoded);
	
	if (sloped) {
		if (rotation == 0) {
			/* N-S axis slope */
			return highEnd == 0 ? 4 : 5; /* 4=asc_N (north end high), 5=asc_S (south end high) */
		} else {
			/* E-W axis slope */
			return highEnd == 0 ? 2 : 3; /* 2=asc_E (east end high), 3=asc_W (west end high) */
		}
	}
	
	/* Flat or curve - determine from texture */
	{
		TextureLoc tex = RAIL_DECODE_TEX(encoded);
		if (tex == 112) {
			/* Curve texture */
			switch (rotation) {
				case 0: return 6; /* S-E curve */
				case 1: return 9; /* N-E curve */
				case 2: return 8; /* N-W curve */
				case 3: return 7; /* S-W curve */
			}
		}
		/* Straight */
		return rotation == 0 ? 0 : 1; /* 0=N-S, 1=E-W */
	}
}

/* Try to find a rail at (x,y,z) or (x,y-1,z). Returns the y level of the rail, or -1. */
static int FindRailY(int x, int y, int z) {
	if (World_Contains(x, y, z) && IsRail(World_GetBlock(x, y, z)))
		return y;
	if (World_Contains(x, y - 1, z) && IsRail(World_GetBlock(x, y - 1, z)))
		return y - 1;
	return -1;
}


/*########################################################################################################################*
*-------------------------------------------------Minecart model----------------------------------------------------------*
*#########################################################################################################################*/
/* Minecart model: 6 box parts (floor, interior floor, 4 walls)
   Texture: cart.png, 64x32
   
   Coordinates are in CC's native system (Y=0 at bottom, Y up).
   UV mapping preserved from Alpha's box definitions (same sizes → same texture layout).
   
   Floor/Interior: rotated around X to lay flat (preserves 20x16 / 18x14 UV faces)
   Side walls: rotated around Y to face left/right (preserves 16x8 UV faces)
   Back wall: rotated PI around Y (preserves 16x8 UV faces)
   Front wall: placed directly at final position (no rotation needed)
   
   Final geometry (model space, RotY = atan2(velZ,velX) aligns X with travel dir):
   - Floor:    X(-10..10), Y(0..2),  Z(-8..8)       [20 wide, 2 thick, 16 deep]
   - Interior: X(-9..9),   Y(1..2),  Z(-7..7)       [18 wide, 1 thick, 14 deep]  
   - Walls:    8 pixels tall (Y 2..10), 2 pixels thick
   - Left:     X(-10..-8), Z(-8..8)  (flush with floor edge)
   - Right:    X(8..10),   Z(-8..8)  (flush with floor edge)
   - Front:    Z(6..8),    X(-8..8)  (1px inward from floor edge)
   - Back:     Z(-8..-6),  X(-8..8)  (1px inward from floor edge) */

static struct ModelPart mc_floor, mc_interior, mc_wallL, mc_wallR, mc_wallB, mc_wallF;
/* 6 parts * 24 vertices each = 144 */
#define MC_MODEL_MAX_VERTICES (6 * MODEL_BOX_VERTICES)

static void MinecartModel_MakeParts(void) {
	/* Floor: UV(0,10), sizes 20x16x2 (for UV), laid flat via BuildRotatedBox.
	   BuildRotatedBox maps the body face (sizeX x sizeZ = 20x16) to the Y faces,
	   giving the correct floor texture on the horizontal surfaces. */
	static const struct BoxDesc floor_desc = {
		BoxDesc_Tex(0, 10),
		BoxDesc_Box(-10, 0, -8, 10, 2, 8),
		BoxDesc_Rot(0, 1, 0),
	};
	/* Interior floor: UV(44,10), sizes 18x14x1, laid flat via BuildRotatedBox. */
	static const struct BoxDesc interior_desc = {
		BoxDesc_Tex(44, 10),
		BoxDesc_Box(-9, 1, -7, 9, 2, 7),
		BoxDesc_Rot(0, 1, 0),
	};
	/* Left wall: UV(0,0), sizes 16x8x2.
	   Rotated -PI/2 around Y with pivot at X=-9. */
	static const struct BoxDesc wallL_desc = {
		BoxDesc_Tex(0, 0),
		BoxDesc_Box(-17, 2, -1, -1, 10, 1),
		BoxDesc_Rot(-9, 6, 0),
	};
	/* Right wall: UV(0,0), sizes 16x8x2.
	   Rotated PI/2 around Y with pivot at X=9. */
	static const struct BoxDesc wallR_desc = {
		BoxDesc_Tex(0, 0),
		BoxDesc_Box(1, 2, -1, 17, 10, 1),
		BoxDesc_Rot(9, 6, 0),
	};
	/* Back wall: UV(0,0), sizes 16x8x2.
	   Rotated PI around Y with pivot Z=-4. Box z shifted to (-2..0) so
	   final position is Z=-8..-6 (1px inward from original -9..-7). */
	static const struct BoxDesc wallB_desc = {
		BoxDesc_Tex(0, 0),
		BoxDesc_Box(-8, 2, -2, 8, 10, 0),
		BoxDesc_Rot(0, 6, -4),
	};
	/* Front wall: UV(0,0), sizes 16x8x2.
	   Placed directly at final position. Pushed 1px inward: Z=6..8 instead of 7..9. */
	static const struct BoxDesc wallF_desc = {
		BoxDesc_Tex(0, 0),
		BoxDesc_Box(-8, 2, 6, 8, 10, 8),
		BoxDesc_Rot(0, 6, 7),
	};
	
	BoxDesc_BuildRotatedBox(&mc_floor, &floor_desc);
	BoxDesc_BuildRotatedBox(&mc_interior, &interior_desc);
	BoxDesc_BuildBox(&mc_wallL, &wallL_desc);
	BoxDesc_BuildBox(&mc_wallR, &wallR_desc);
	BoxDesc_BuildBox(&mc_wallB, &wallB_desc);
	BoxDesc_BuildBox(&mc_wallF, &wallF_desc);
}

static void MinecartModel_Draw(struct Entity* e) {
	Model_ApplyTexture(e);
	Model_LockVB(e, MC_MODEL_MAX_VERTICES);
	
	/* Floor and interior: no rotation needed (BuildRotatedBox already laid them flat) */
	Model_DrawPart(&mc_floor);
	Model_DrawPart(&mc_interior);
	/* Left wall: rotate -PI/2 around Y */
	Model_DrawRotate(0, -MATH_PI / 2.0f, 0, &mc_wallL, false);
	/* Right wall: rotate PI/2 around Y */
	Model_DrawRotate(0, MATH_PI / 2.0f, 0, &mc_wallR, false);
	/* Back wall: rotate PI around Y */
	Model_DrawRotate(0, MATH_PI, 0, &mc_wallB, false);
	/* Front wall: no rotation (already at final position) */
	Model_DrawPart(&mc_wallF);
	
	Model_UnlockVB();
	Gfx_DrawVb_IndexedTris(MC_MODEL_MAX_VERTICES);
}

static float MinecartModel_GetNameY(struct Entity* e)  { return 0.8f; }
static float MinecartModel_GetEyeY(struct Entity* e)   { return 5.0f/16.0f; }
static void  MinecartModel_GetSize(struct Entity* e) {
	e->Size.x = 1.25f; e->Size.y = 0.625f; e->Size.z = 1.0f;
}
static void MinecartModel_GetBounds(struct Entity* e) {
	e->ModelAABB.Min.x = -10.0f/16.0f; e->ModelAABB.Min.y = 0.0f; e->ModelAABB.Min.z = -8.0f/16.0f;
	e->ModelAABB.Max.x =  10.0f/16.0f; e->ModelAABB.Max.y = 10.0f/16.0f; e->ModelAABB.Max.z =  8.0f/16.0f;
}

static void MinecartModel_GetTransform(struct Entity* e, Vec3 pos, struct Matrix* m) {
	Entity_GetTransform(e, pos, e->ModelScale, m);
}

static struct ModelVertex mc_vertices[MC_MODEL_MAX_VERTICES];
struct ModelTex minecart_tex = { "cart.png" };
static struct Model minecart_model = {
	"minecart", mc_vertices, &minecart_tex,
	MinecartModel_MakeParts, MinecartModel_Draw,
	MinecartModel_GetNameY,  MinecartModel_GetEyeY,
	MinecartModel_GetSize,   MinecartModel_GetBounds
};

void MinecartModel_Register(void) {
	Model_Init(&minecart_model);
	minecart_model.maxVertices  = MC_MODEL_MAX_VERTICES;
	minecart_model.bobbing      = false;
	minecart_model.pushes       = false;
	minecart_model.usesSkin     = false;
	minecart_model.GetTransform = MinecartModel_GetTransform;
	Model_Register(&minecart_model);
}


/*########################################################################################################################*
*-------------------------------------------------Minecart entity management----------------------------------------------*
*#########################################################################################################################*/
static int FindFreeEntitySlot(void) {
	int id;
	for (id = 0; id < MAX_NET_PLAYERS; id++) {
		if (!Entities.List[id]) return id;
	}
	return -1;
}

int Minecart_Spawn(float x, float y, float z) {
	struct NetPlayer* np;
	struct LocationUpdate update;
	cc_string model;
	int slot, eid;
	
	/* Find free minecart slot */
	for (slot = 0; slot < MAX_MINECARTS; slot++) {
		if (!Minecarts[slot].active) break;
	}
	if (slot == MAX_MINECARTS) return -1;
	
	/* Find free entity slot */
	eid = FindFreeEntitySlot();
	if (eid == -1) return -1;
	
	/* Create entity */
	np = &NetPlayers_List[eid];
	NetPlayer_Init(np);
	Entities.List[eid] = &np->Base;
	Event_RaiseInt(&EntityEvents.Added, eid);
	
	model = String_FromReadonly("minecart");
	Entity_SetModel(&np->Base, &model);
	
	/* Position the entity */
	update.flags = LU_HAS_POS;
	update.pos.x = x; update.pos.y = y; update.pos.z = z;
	np->Base.VTABLE->SetLocation(&np->Base, &update);
	np->Base.Position = update.pos;
	
	/* Initialize minecart state */
	Minecarts[slot].active      = true;
	Minecarts[slot].pos.x       = x;
	Minecarts[slot].pos.y       = y;
	Minecarts[slot].pos.z       = z;
	Minecarts[slot].velocity    = Vec3_Create3(0,0,0);
	Minecarts[slot].yaw         = 0.0f;
	Minecarts[slot].pitch       = 0.0f;
	Minecarts[slot].entityId    = eid;
	Minecarts[slot].riderId     = -1;
	Minecarts[slot].timeSinceHit = 0;
	Minecarts[slot].damageTaken  = 0.0f;
	Minecarts[slot].forwardDir   = 1;
	Minecarts[slot].isInReverse  = false;
	
	return slot;
}

void Minecart_Despawn(int slot) {
	int eid;
	if (slot < 0 || slot >= MAX_MINECARTS) return;
	if (!Minecarts[slot].active) return;
	
	/* Dismount rider */
	if (Minecarts[slot].riderId >= 0) {
		Minecarts[slot].riderId = -1;
	}
	
	/* Remove entity */
	eid = Minecarts[slot].entityId;
	if (eid >= 0 && Entities.List[eid]) {
		Entities_Remove(eid);
	}
	
	Minecarts[slot].active = false;
}

int Minecart_FindClosest(Vec3 pos, float maxDist) {
	float bestDist = maxDist * maxDist;
	int bestSlot = -1;
	int i;
	
	for (i = 0; i < MAX_MINECARTS; i++) {
		float dx, dy, dz, distSq;
		if (!Minecarts[i].active) continue;
		
		dx = Minecarts[i].pos.x - pos.x;
		dy = Minecarts[i].pos.y - pos.y;
		dz = Minecarts[i].pos.z - pos.z;
		distSq = dx * dx + dy * dy + dz * dz;
		
		if (distSq < bestDist) {
			bestDist = distSq;
			bestSlot = i;
		}
	}
	return bestSlot;
}

int Minecart_FindByEntityId(int entityId) {
	int i;
	if (entityId < 0) return -1;
	for (i = 0; i < MAX_MINECARTS; i++) {
		if (Minecarts[i].active && Minecarts[i].entityId == entityId) return i;
	}
	return -1;
}


/*########################################################################################################################*
*-------------------------------------------------Riding mechanics--------------------------------------------------------*
*#########################################################################################################################*/
static int playerRidingCart = -1;

int Minecart_GetPlayerCart(void) {
	return playerRidingCart;
}

void Minecart_RideCart(int slot) {
	if (slot < 0 || slot >= MAX_MINECARTS) return;
	if (!Minecarts[slot].active) return;
	if (Minecarts[slot].riderId >= 0) return; /* Already occupied */
	
	Minecarts[slot].riderId = ENTITIES_SELF_ID;
	playerRidingCart = slot;
}

void Minecart_DismountPlayer(void) {
	if (playerRidingCart < 0) return;
	if (playerRidingCart < MAX_MINECARTS && Minecarts[playerRidingCart].active) {
		Minecarts[playerRidingCart].riderId = -1;
	}
	playerRidingCart = -1;
}


/*########################################################################################################################*
*-------------------------------------------------Rail following physics--------------------------------------------------*
*#########################################################################################################################*/
/* Snap minecart position to rail line and apply rail-following motion.
   Closely mirrors Alpha EntityMinecart.i_() */
static void Minecart_FollowRail(struct Minecart* mc) {
	int blockX, blockY, blockZ, meta;
	float dx0, dy0, dz0, dx1, dy1, dz1;
	float railDx, railDz;
	float dot, speed;
	float var21, snapX, snapZ;
	float oldY;
	int newBlockX, newBlockZ;
	
	blockX = (int)Math_Floor(mc->pos.x);
	blockY = (int)Math_Floor(mc->pos.y);
	blockZ = (int)Math_Floor(mc->pos.z);
	
	/* Check for rail at current y or y-1 */
	{
		int railY = FindRailY(blockX, blockY, blockZ);
		if (railY < 0) return; /* Not on a rail */
		blockY = railY;
	}
	
	meta = GetRailMeta(blockX, blockY, blockZ);
	if (meta < 0 || meta > 9) return;
	
	/* Slopes: set Y to top of slope block */
	if (meta >= 2 && meta <= 5) {
		mc->pos.y = (float)(blockY + 1);
	}
	
	/* Get rail direction endpoints */
	dx0 = (float)railMatrix[meta][0][0];
	dy0 = (float)railMatrix[meta][0][1];
	dz0 = (float)railMatrix[meta][0][2];
	dx1 = (float)railMatrix[meta][1][0];
	dy1 = (float)railMatrix[meta][1][1];
	dz1 = (float)railMatrix[meta][1][2];
	
	/* Rail direction vector */
	railDx = dx1 - dx0;
	railDz = dz1 - dz0;
	
	/* Slope gravity push */
	if (meta == 2)      mc->velocity.x -= MC_SLOPE_PUSH; /* ascending east: push west */
	else if (meta == 3) mc->velocity.x += MC_SLOPE_PUSH; /* ascending west: push east */
	else if (meta == 4) mc->velocity.z += MC_SLOPE_PUSH; /* ascending north: push south */
	else if (meta == 5) mc->velocity.z -= MC_SLOPE_PUSH; /* ascending south: push north */
	
	/* Project motion onto rail direction */
	dot = mc->velocity.x * railDx + mc->velocity.z * railDz;
	speed = Math_SqrtF(mc->velocity.x * mc->velocity.x + mc->velocity.z * mc->velocity.z);
	
	if (speed > 0.001f) {
		/* Redistribute speed along rail direction */
		if (dot < 0) {
			railDx = -railDx;
			railDz = -railDz;
		}
		{
			float len = Math_SqrtF(railDx * railDx + railDz * railDz);
			if (len > 0.001f) {
				railDx /= len;
				railDz /= len;
			}
		}
		mc->velocity.x = railDx * speed;
		mc->velocity.z = railDz * speed;
	}
	
	/* Snap position to rail line */
	snapX = (float)blockX + 0.5f;
	snapZ = (float)blockZ + 0.5f;
	
	/* Calculate parametric position along the rail segment */
	{
		float p0x = snapX + dx0 * 0.5f;
		float p0z = snapZ + dz0 * 0.5f;
		float segDx = dx1 - dx0;
		float segDz = dz1 - dz0;
		float segLen = segDx * segDx + segDz * segDz;
		
		if (segLen > 0.001f) {
			var21 = ((mc->pos.x - p0x) * segDx + (mc->pos.z - p0z) * segDz) / segLen;
			if (var21 < 0.0f) var21 = 0.0f;
			if (var21 > 1.0f) var21 = 1.0f;
			
			mc->pos.x = p0x + segDx * var21;
			mc->pos.z = p0z + segDz * var21;
		} else {
			/* Purely N-S or E-W - snap the fixed axis */
			if (dx0 == 0 && dx1 == 0) mc->pos.x = snapX;
			if (dz0 == 0 && dz1 == 0) mc->pos.z = snapZ;
		}
	}
	
	/* Rider speed reduction */
	if (mc->riderId >= 0) {
		mc->velocity.x *= MC_RIDER_SPEED_MULT;
		mc->velocity.z *= MC_RIDER_SPEED_MULT;
	}
	
	/* Clamp to max speed */
	if (mc->velocity.x > MC_MAX_SPEED) mc->velocity.x = MC_MAX_SPEED;
	if (mc->velocity.x < -MC_MAX_SPEED) mc->velocity.x = -MC_MAX_SPEED;
	if (mc->velocity.z > MC_MAX_SPEED) mc->velocity.z = MC_MAX_SPEED;
	if (mc->velocity.z < -MC_MAX_SPEED) mc->velocity.z = -MC_MAX_SPEED;
	
	/* Move along rail */
	oldY = mc->pos.y;
	mc->pos.x += mc->velocity.x;
	mc->pos.z += mc->velocity.z;
	
	/* Check if we crossed into a new block - slope height correction */
	newBlockX = (int)Math_Floor(mc->pos.x);
	newBlockZ = (int)Math_Floor(mc->pos.z);
	
	if (newBlockX != blockX || newBlockZ != blockZ) {
		/* Check the rail endpoints for dy values */
		int endX0 = blockX + (int)dx0;
		int endZ0 = blockZ + (int)dz0;
		int endX1 = blockX + (int)dx1;
		int endZ1 = blockZ + (int)dz1;
		
		if (newBlockX == endX0 && newBlockZ == endZ0 && dy0 != 0) {
			mc->pos.y += dy0;
		} else if (newBlockX == endX1 && newBlockZ == endZ1 && dy1 != 0) {
			mc->pos.y += dy1;
		}
		
		/* Force direction toward new block */
		{
			float ddx = (float)(newBlockX - blockX);
			float ddz = (float)(newBlockZ - blockZ);
			float sp = Math_SqrtF(mc->velocity.x * mc->velocity.x + mc->velocity.z * mc->velocity.z);
			float len = Math_SqrtF(ddx * ddx + ddz * ddz);
			
			if (len > 0.001f && sp > 0.001f) {
				mc->velocity.x = (ddx / len) * sp;
				mc->velocity.z = (ddz / len) * sp;
			}
		}
	}
	
	/* Apply friction */
	if (mc->riderId >= 0) {
		mc->velocity.x *= MC_FRICTION_RIDDEN;
		mc->velocity.z *= MC_FRICTION_RIDDEN;
	} else {
		mc->velocity.x *= MC_FRICTION_EMPTY;
		mc->velocity.z *= MC_FRICTION_EMPTY;
	}
	
	/* Zero Y velocity on rail */
	mc->velocity.y = 0.0f;
	
	/* Energy conservation on slopes */
	{
		float dY = oldY - mc->pos.y;
		if (dY != 0.0f) {
			float sp = Math_SqrtF(mc->velocity.x * mc->velocity.x + mc->velocity.z * mc->velocity.z);
			sp += dY * 0.05f;
			if (sp > 0.001f && sp > 0.0f) {
				float len = Math_SqrtF(mc->velocity.x * mc->velocity.x + mc->velocity.z * mc->velocity.z);
				if (len > 0.001f) {
					mc->velocity.x = (mc->velocity.x / len) * sp;
					mc->velocity.z = (mc->velocity.z / len) * sp;
				}
			}
		}
	}
}


/*########################################################################################################################*
*-------------------------------------------------Minecart-Minecart collision (booster bug)-------------------------------*
*#########################################################################################################################*/
/* Replicates Alpha's EntityMinecart.applyEntityCollision for minecart-minecart pairs.
   This is the famous booster bug: two carts on parallel tracks boost each other. */
static void Minecart_ApplyBooster(struct Minecart* a, struct Minecart* b) {
	float dx, dz, dist, invDist, pushX, pushZ;
	float avgMX, avgMZ;
	
	dx = b->pos.x - a->pos.x;
	dz = b->pos.z - a->pos.z;
	dist = Math_SqrtF(dx * dx + dz * dz);
	
	if (dist < 0.001f) return;
	
	/* Normalize separation */
	dx /= dist;
	dz /= dist;
	
	/* Inverse distance, capped at 1.0 */
	invDist = 1.0f / dist;
	if (invDist > 1.0f) invDist = 1.0f;
	
	/* Scale push force: 0.1 * invDist * 0.5 */
	pushX = dx * invDist * MC_PUSH_FACTOR * 0.5f;
	pushZ = dz * invDist * MC_PUSH_FACTOR * 0.5f;
	
	/* Average both carts' velocities */
	avgMX = (a->velocity.x + b->velocity.x) * 0.5f;
	avgMZ = (a->velocity.z + b->velocity.z) * 0.5f;
	
	/* Apply: each cart gets average velocity plus/minus push from separation.
	   This is the booster bug: on parallel tracks, the push is perpendicular
	   to the rail and gets constrained, but the averaged velocity along the
	   rail accumulates, causing both carts to accelerate. */
	a->velocity.x = avgMX - pushX;
	a->velocity.z = avgMZ - pushZ;
	b->velocity.x = avgMX + pushX;
	b->velocity.z = avgMZ + pushZ;
}

/* Check all minecart pairs for collisions within expanded bounding boxes */
static void Minecart_CheckCollisions(void) {
	int i, j;
	float dx, dz, dist;
	
	for (i = 0; i < MAX_MINECARTS; i++) {
		if (!Minecarts[i].active) continue;
		
		for (j = i + 1; j < MAX_MINECARTS; j++) {
			if (!Minecarts[j].active) continue;
			
			dx = Minecarts[j].pos.x - Minecarts[i].pos.x;
			dz = Minecarts[j].pos.z - Minecarts[i].pos.z;
			dist = Math_SqrtF(dx * dx + dz * dz);
			
			/* Collision range: entity width (0.98) + padding (0.2) on each side */
			if (dist < 0.98f + MC_ENTITY_SCAN_PAD * 2.0f) {
				Minecart_ApplyBooster(&Minecarts[i], &Minecarts[j]);
			}
		}
	}
}


/*########################################################################################################################*
*-------------------------------------------------Player push / off-rail physics------------------------------------------*
*#########################################################################################################################*/
/* When a player walks into a minecart, push it */
static void Minecart_CheckPlayerPush(struct Minecart* mc) {
	struct LocalPlayer* p = Entities.CurPlayer;
	float dx, dz, dist, pushStrength;
	
	if (!p) return;
	if (mc->riderId >= 0) return; /* Don't push occupied carts */
	
	dx = mc->pos.x - p->Base.Position.x;
	dz = mc->pos.z - p->Base.Position.z;
	dist = Math_SqrtF(dx * dx + dz * dz);
	
	if (dist > 1.0f || dist < 0.01f) return;
	
	/* Push away from player */
	pushStrength = (1.0f - dist) * 0.1f;
	mc->velocity.x += (dx / dist) * pushStrength;
	mc->velocity.z += (dz / dist) * pushStrength;
}

/* Apply off-rail physics (gravity, friction) */
static void Minecart_OffRailPhysics(struct Minecart* mc) {
	int blockX, blockY, blockZ;
	cc_bool onGround;
	
	/* Apply gravity */
	mc->velocity.y -= MC_GRAVITY;
	
	/* Simple ground detection */
	blockX = (int)Math_Floor(mc->pos.x);
	blockY = (int)Math_Floor(mc->pos.y - 0.1f);
	blockZ = (int)Math_Floor(mc->pos.z);
	
	onGround = false;
	if (World_Contains(blockX, blockY, blockZ)) {
		BlockID below = World_GetBlock(blockX, blockY, blockZ);
		if (Blocks.Collide[below] == COLLIDE_SOLID) {
			onGround = true;
			if (mc->velocity.y < 0) {
				mc->pos.y = (float)(blockY + 1) + 0.0625f;
				mc->velocity.y = 0;
			}
		}
	}
	
	/* Move */
	mc->pos.x += mc->velocity.x;
	mc->pos.y += mc->velocity.y;
	mc->pos.z += mc->velocity.z;
	
	/* Friction */
	if (onGround) {
		mc->velocity.x *= MC_OFF_RAIL_GROUND;
		mc->velocity.y *= MC_OFF_RAIL_GROUND;
		mc->velocity.z *= MC_OFF_RAIL_GROUND;
	} else {
		mc->velocity.x *= MC_OFF_RAIL_AIR;
		mc->velocity.y *= MC_OFF_RAIL_AIR;
		mc->velocity.z *= MC_OFF_RAIL_AIR;
	}
}


/*########################################################################################################################*
*-------------------------------------------------Minecart tick-----------------------------------------------------------*
*#########################################################################################################################*/
static void Minecart_UpdateEntity(struct Minecart* mc) {
	struct Entity* e;
	struct LocationUpdate update;
	
	if (mc->entityId < 0) return;
	e = Entities.List[mc->entityId];
	if (!e) return;
	
	/* Update entity position and orientation */
	update.flags = LU_HAS_POS | LU_HAS_YAW | LU_HAS_PITCH;
	update.pos   = mc->pos;
	update.yaw   = mc->yaw;
	update.pitch  = mc->pitch;
	e->VTABLE->SetLocation(e, &update);
	e->Position = mc->pos;
	e->Yaw      = mc->yaw;
	e->RotY     = mc->yaw;
	e->Pitch    = mc->pitch;
}

static void Minecart_UpdateVisuals(struct Minecart* mc) {
	float speed, deltaX, deltaZ, newYaw, yawDiff;
	
	deltaX = mc->velocity.x;
	deltaZ = mc->velocity.z;
	speed = deltaX * deltaX + deltaZ * deltaZ;
	
	/* Update yaw based on motion direction.
	   NOTE: CC's Math_Atan2f(x,y) computes atan2(y,x) (params swapped vs standard C).
	   So Math_Atan2f(deltaX, deltaZ) = atan2(deltaZ, deltaX), which gives the standard
	   math angle from +X axis, correctly aligning model's X (long) axis with velocity. */
	if (speed > 0.001f) {
		newYaw = Math_Atan2f(deltaX, deltaZ) * MATH_RAD2DEG;
		
		/* Alpha reverse detection: if yaw changed by >= 170 degrees, flip */
		yawDiff = newYaw - mc->yaw;
		while (yawDiff >= 180.0f) yawDiff -= 360.0f;
		while (yawDiff < -180.0f) yawDiff += 360.0f;
		
		if (yawDiff >= 170.0f || yawDiff <= -170.0f) {
			newYaw += 180.0f;
			mc->isInReverse = !mc->isInReverse;
		}
		
		mc->yaw = newYaw;
	}
	
	/* Calculate pitch for slopes */
	{
		int bx = (int)Math_Floor(mc->pos.x);
		int by = (int)Math_Floor(mc->pos.y);
		int bz = (int)Math_Floor(mc->pos.z);
		int railY = FindRailY(bx, by, bz);
		int meta;
		
		mc->pitch = 0.0f;
		if (railY >= 0) {
			meta = GetRailMeta(bx, railY, bz);
			if (meta >= 2 && meta <= 5) {
				/* Slope: tilt the cart ~45 degrees (Alpha uses atan * 73.0 for exaggerated tilt) */
				mc->pitch = 45.0f;
				/* Flip sign based on direction */
				if (meta == 2 && mc->velocity.x < 0) mc->pitch = -45.0f;
				if (meta == 3 && mc->velocity.x > 0) mc->pitch = -45.0f;
				if (meta == 4 && mc->velocity.z < 0) mc->pitch = -45.0f;
				if (meta == 5 && mc->velocity.z > 0) mc->pitch = -45.0f;
			}
		}
	}
	
	/* Update damage wobble */
	if (mc->timeSinceHit > 0) mc->timeSinceHit--;
	if (mc->damageTaken > 0.0f) mc->damageTaken -= 1.0f;
	if (mc->damageTaken < 0.0f) mc->damageTaken = 0.0f;
}

/* Main tick for one minecart */
static void Minecart_TickOne(struct Minecart* mc, float delta) {
	int blockX, blockY, blockZ, railY;
	cc_bool onRail;
	
	blockX = (int)Math_Floor(mc->pos.x);
	blockY = (int)Math_Floor(mc->pos.y);
	blockZ = (int)Math_Floor(mc->pos.z);
	
	railY = FindRailY(blockX, blockY, blockZ);
	onRail = (railY >= 0);
	
	if (onRail) {
		Minecart_FollowRail(mc);
	} else {
		Minecart_OffRailPhysics(mc);
	}
	
	/* Player push (only when not riding) */
	if (mc->riderId < 0) {
		Minecart_CheckPlayerPush(mc);
	}
	
	/* Kill very small velocities */
	if (mc->velocity.x > -0.0001f && mc->velocity.x < 0.0001f) mc->velocity.x = 0;
	if (mc->velocity.z > -0.0001f && mc->velocity.z < 0.0001f) mc->velocity.z = 0;
	
	Minecart_UpdateVisuals(mc);
	Minecart_UpdateEntity(mc);
	
	/* Despawn if fallen into void */
	if (mc->pos.y < -64.0f) {
		int slot = (int)(mc - Minecarts);
		Minecart_Despawn(slot);
	}
}

/* Update player position to follow riding cart */
static void Minecart_UpdateRider(void) {
	struct LocalPlayer* p;
	struct Minecart* mc;
	
	if (playerRidingCart < 0 || playerRidingCart >= MAX_MINECARTS) return;
	mc = &Minecarts[playerRidingCart];
	if (!mc->active) {
		playerRidingCart = -1;
		return;
	}
	
	p = Entities.CurPlayer;
	if (!p) return;
	
	/* Position player at cart position + ride offset */
	p->Base.Position.x = mc->pos.x;
	p->Base.Position.y = mc->pos.y + MC_RIDE_OFFSET;
	p->Base.Position.z = mc->pos.z;
	
	/* Kill player velocity (cart controls movement) */
	p->Base.Velocity = Vec3_Create3(0,0,0);
	p->Base.OnGround = true;
}

/* Scheduled tick callback */
static void Minecart_ScheduledTick(struct ScheduledTask* task) {
	int i;
	float delta = (float)task->interval;
	
	for (i = 0; i < MAX_MINECARTS; i++) {
		if (!Minecarts[i].active) continue;
		Minecart_TickOne(&Minecarts[i], delta);
	}
	
	/* Check minecart-minecart collisions (booster bug) */
	Minecart_CheckCollisions();
	
	/* Update player riding position */
	Minecart_UpdateRider();
}


/*########################################################################################################################*
*-------------------------------------------------Component lifecycle-----------------------------------------------------*
*#########################################################################################################################*/
static void Minecart_OnInit(void) {
	int i;
	for (i = 0; i < MAX_MINECARTS; i++) {
		Minecarts[i].active = false;
	}
	playerRidingCart = -1;
	
	ScheduledTask_Add(GAME_DEF_TICKS, Minecart_ScheduledTick);
}

static void Minecart_OnReset(void) {
	int i;
	for (i = 0; i < MAX_MINECARTS; i++) {
		if (Minecarts[i].active) {
			Minecart_Despawn(i);
		}
	}
	playerRidingCart = -1;
}

struct IGameComponent Minecart_Component = {
	Minecart_OnInit,  /* Init  */
	NULL,             /* Free  */
	Minecart_OnReset, /* Reset */
};
