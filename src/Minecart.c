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

/* Physics constants matching Minecraft Alpha 1.2.6 */
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

/* Minecart hitbox half-widths for block collision (Alpha: setSize(0.98, 0.7)) */
#define MC_HALF_WIDTH  0.49f   /* 0.98 / 2 */
#define MC_HEIGHT      0.7f
/* Max Y-distance for player push interaction (cart height + player height margin) */
#define MC_PUSH_Y_RANGE 2.0f

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
   Now simply reads the block ID and converts to meta 0-9. */
static int GetRailMeta(int x, int y, int z) {
	BlockID block;
	if (!World_Contains(x, y, z)) return -1;
	block = World_GetBlock(x, y, z);
	if (!IsRail(block)) return -1;
	return Rail_BlockToMeta(block);
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
	Minecarts[slot].prevPos.x   = x;
	Minecarts[slot].prevPos.y   = y;
	Minecarts[slot].prevPos.z   = z;
	Minecarts[slot].yaw         = 0.0f;
	Minecarts[slot].pitch       = 0.0f;
	Minecarts[slot].entityId    = eid;
	Minecarts[slot].riderId     = -1;
	Minecarts[slot].timeSinceHit = 0;
	Minecarts[slot].damageTaken  = 0.0f;
	Minecarts[slot].forwardDir   = 1;
	Minecarts[slot].isInReverse  = false;
	Minecarts[slot].onGround     = false;
	
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

/* Alpha's a() function - smooth rail position interpolation.
   Returns the smoothly interpolated position along the rail including Y for slopes.
   This is what makes going up/down hills smooth instead of teleporting at block boundaries. */
static cc_bool Minecart_GetSmoothPos(float inX, float inY, float inZ,
                                     float* outX, float* outY, float* outZ) {
	int bx, by, bz, meta;
	const int (*m)[3];
	float p0x, p0y, p0z, p1x, p1y, p1z;
	float dx, dy, dz, t;
	
	bx = (int)Math_Floor(inX);
	by = (int)Math_Floor(inY);
	bz = (int)Math_Floor(inZ);
	
	/* Alpha: check rail at y-1 */
	if (World_Contains(bx, by - 1, bz) && IsRail(World_GetBlock(bx, by - 1, bz)))
		by--;
	
	if (!World_Contains(bx, by, bz) || !IsRail(World_GetBlock(bx, by, bz)))
		return false;
	
	meta = GetRailMeta(bx, by, bz);
	if (meta < 0 || meta > 9) return false;
	
	m = railMatrix[meta];
	
	/* Endpoint positions INCLUDING Y from matrix (Alpha includes Y!) */
	p0x = bx + 0.5f + m[0][0] * 0.5f;
	p0y = by + 0.5f + m[0][1] * 0.5f;
	p0z = bz + 0.5f + m[0][2] * 0.5f;
	p1x = bx + 0.5f + m[1][0] * 0.5f;
	p1y = by + 0.5f + m[1][1] * 0.5f;
	p1z = bz + 0.5f + m[1][2] * 0.5f;
	
	dx = p1x - p0x;
	dy = (p1y - p0y) * 2.0f;  /* Alpha doubles the Y gradient */
	dz = p1z - p0z;
	
	/* Parametric projection onto rail line (Alpha's exact math) */
	if (dx == 0.0f) {
		t = inZ - (float)bz;
	} else if (dz == 0.0f) {
		t = inX - (float)bx;
	} else {
		float ox = inX - p0x;
		float oz = inZ - p0z;
		t = (ox * dx + oz * dz) * 2.0f;
	}
	
	*outX = p0x + dx * t;
	*outY = p0y + dy * t;
	*outZ = p0z + dz * t;
	
	/* Alpha's Y adjustments for slope direction */
	if (dy < 0.0f) *outY += 1.0f;
	if (dy > 0.0f) *outY += 0.5f;
	
	return true;
}

/* Offset from Alpha's smooth Y to CC's entity feet position.
   In Alpha: posY = smoothY (setPosition stores y directly), and feet = posY - yOffset.
   yOffset = height / 2 = 0.7 / 2 = 0.35. CC positions are at feet, so:
   CC_pos.y = smoothY - yOffset = smoothY - 0.35 */
#define MC_SMOOTH_Y_OFFSET 0.35f

/* Maximum block AABBs to consider in one moveEntity call.
   Sweep volume for a 0.98-wide cart moving 0.4/tick is at most ~3x2x3 blocks = 18.
   64 is more than enough headroom. */
#define MC_MAX_BLOCK_AABBS 64

/* Whether a block should provide collision for minecarts.
   In Alpha, any block that returns a non-null getCollisionBoundingBoxFromPool
   participates in entity collision. In CC, this includes SOLID-type blocks
   (COLLIDE_SOLID, COLLIDE_ICE, COLLIDE_SLIPPERY_ICE) and CLIMBABLE blocks
   (ladders). Liquids, air, and walk-through blocks (torches, flowers) do not. */
static cc_bool Minecart_BlockCollides(BlockID block) {
	cc_uint8 c = Blocks.Collide[block];
	return c >= COLLIDE_SOLID || c == COLLIDE_CLIMB;
}

/* Alpha-faithful moveEntity: resolve movement against block AABBs using the exact
   axis-by-axis clipping from Entity.java's moveEntity method.
   
   This is the key difference from the old collision code: instead of checking whether
   a block CELL is solid, we check actual AABB overlap with the block's MinBB/MaxBB
   collision bounds. This means thin blocks like ladders (2/16 thick) and doors (3/16
   thick) only block the minecart where their actual collision shape is, not the full
   block cell.
   
   Algorithm (from Alpha):
   1. Compute entity AABB from position and size
   2. Expand by movement vector to get sweep volume
   3. Collect all block AABBs in the sweep volume
   4. Clip movement per-axis: Y first, then X, then Z
   5. Update position by clipped movement
   6. Zero velocity on any axis that was clipped

   Returns: bit flags for which axes were blocked (1=X, 2=Y, 4=Z) */
static int Minecart_MoveEntity(struct Minecart* mc, float dx, float dy, float dz) {
	/* Entity AABB corners (feet-based position, same as Alpha after yOffset adjustment) */
	float eMinX, eMinY, eMinZ, eMaxX, eMaxY, eMaxZ;
	/* Sweep volume corners */
	float sMinX, sMinY, sMinZ, sMaxX, sMaxY, sMaxZ;
	/* Block iteration range */
	int bxMin, bxMax, byMin, byMax, bzMin, bzMax;
	int bx, by, bz, i, count;
	float origDx, origDy, origDz;
	int blocked;
	/* Block AABB storage */
	struct { float minX, minY, minZ, maxX, maxY, maxZ; } aabbs[MC_MAX_BLOCK_AABBS];
	
	origDx = dx; origDy = dy; origDz = dz;
	blocked = 0;
	count = 0;
	
	/* Entity AABB (Alpha: boundingBox from setSize(0.98, 0.7)) */
	eMinX = mc->pos.x - MC_HALF_WIDTH;
	eMaxX = mc->pos.x + MC_HALF_WIDTH;
	eMinY = mc->pos.y;
	eMaxY = mc->pos.y + MC_HEIGHT;
	eMinZ = mc->pos.z - MC_HALF_WIDTH;
	eMaxZ = mc->pos.z + MC_HALF_WIDTH;
	
	/* Sweep volume: expand entity AABB by movement (Alpha's addCoord) */
	sMinX = eMinX + (dx < 0 ? dx : 0);
	sMaxX = eMaxX + (dx > 0 ? dx : 0);
	sMinY = eMinY + (dy < 0 ? dy : 0);
	sMaxY = eMaxY + (dy > 0 ? dy : 0);
	sMinZ = eMinZ + (dz < 0 ? dz : 0);
	sMaxZ = eMaxZ + (dz > 0 ? dz : 0);
	
	/* Block range (Alpha: floor(min) to floor(max+1), y starts from min-1) */
	bxMin = (int)Math_Floor(sMinX);
	bxMax = (int)Math_Floor(sMaxX + 1.0f);
	byMin = (int)Math_Floor(sMinY) - 1;
	byMax = (int)Math_Floor(sMaxY + 1.0f);
	bzMin = (int)Math_Floor(sMinZ);
	bzMax = (int)Math_Floor(sMaxZ + 1.0f);
	
	/* Collect block AABBs (Alpha's getCollidingBoundingBoxes).
	   For each block in range, compute world-space collision AABB from MinBB/MaxBB
	   and check intersection with sweep volume. */
	for (bx = bxMin; bx < bxMax; bx++) {
		for (bz = bzMin; bz < bzMax; bz++) {
			for (by = byMin; by < byMax; by++) {
				BlockID block;
				float bbMinX, bbMinY, bbMinZ, bbMaxX, bbMaxY, bbMaxZ;
				
				if (!World_Contains(bx, by, bz)) continue;
				block = World_GetBlock(bx, by, bz);
				if (!Minecart_BlockCollides(block)) continue;
				
				/* World-space block collision AABB = block position + MinBB/MaxBB */
				bbMinX = (float)bx + Blocks.MinBB[block].x;
				bbMinY = (float)by + Blocks.MinBB[block].y;
				bbMinZ = (float)bz + Blocks.MinBB[block].z;
				bbMaxX = (float)bx + Blocks.MaxBB[block].x;
				bbMaxY = (float)by + Blocks.MaxBB[block].y;
				bbMaxZ = (float)bz + Blocks.MaxBB[block].z;
				
				/* Alpha's intersectsWith check against sweep volume */
				if (bbMaxX > sMinX && bbMinX < sMaxX &&
					bbMaxY > sMinY && bbMinY < sMaxY &&
					bbMaxZ > sMinZ && bbMinZ < sMaxZ) {
					if (count < MC_MAX_BLOCK_AABBS) {
						aabbs[count].minX = bbMinX; aabbs[count].minY = bbMinY; aabbs[count].minZ = bbMinZ;
						aabbs[count].maxX = bbMaxX; aabbs[count].maxY = bbMaxY; aabbs[count].maxZ = bbMaxZ;
						count++;
					}
				}
			}
		}
	}
	
	/* Y axis clipping first (Alpha's func_1172_b):
	   For each block AABB, if entity overlaps on X and Z, restrict Y movement. */
	for (i = 0; i < count; i++) {
		if (eMaxX > aabbs[i].minX && eMinX < aabbs[i].maxX &&
			eMaxZ > aabbs[i].minZ && eMinZ < aabbs[i].maxZ) {
			float clip;
			if (dy > 0.0f && eMaxY <= aabbs[i].minY) {
				clip = aabbs[i].minY - eMaxY;
				if (clip < dy) dy = clip;
			}
			if (dy < 0.0f && eMinY >= aabbs[i].maxY) {
				clip = aabbs[i].maxY - eMinY;
				if (clip > dy) dy = clip;
			}
		}
	}
	eMinY += dy; eMaxY += dy;
	
	/* X axis clipping (Alpha's func_1163_a):
	   For each block AABB, if entity overlaps on Y and Z, restrict X movement. */
	for (i = 0; i < count; i++) {
		if (eMaxY > aabbs[i].minY && eMinY < aabbs[i].maxY &&
			eMaxZ > aabbs[i].minZ && eMinZ < aabbs[i].maxZ) {
			float clip;
			if (dx > 0.0f && eMaxX <= aabbs[i].minX) {
				clip = aabbs[i].minX - eMaxX;
				if (clip < dx) dx = clip;
			}
			if (dx < 0.0f && eMinX >= aabbs[i].maxX) {
				clip = aabbs[i].maxX - eMinX;
				if (clip > dx) dx = clip;
			}
		}
	}
	eMinX += dx; eMaxX += dx;
	
	/* Z axis clipping (Alpha's func_1162_c):
	   For each block AABB, if entity overlaps on X and Y, restrict Z movement. */
	for (i = 0; i < count; i++) {
		if (eMaxX > aabbs[i].minX && eMinX < aabbs[i].maxX &&
			eMaxY > aabbs[i].minY && eMinY < aabbs[i].maxY) {
			float clip;
			if (dz > 0.0f && eMaxZ <= aabbs[i].minZ) {
				clip = aabbs[i].minZ - eMaxZ;
				if (clip < dz) dz = clip;
			}
			if (dz < 0.0f && eMinZ >= aabbs[i].maxZ) {
				clip = aabbs[i].maxZ - eMinZ;
				if (clip > dz) dz = clip;
			}
		}
	}
	
	/* Update position by clipped movement */
	mc->pos.x += dx;
	mc->pos.y += dy;
	mc->pos.z += dz;
	
	/* Alpha: zero velocity on blocked axes (from Entity.moveEntity) */
	if (origDx != dx) { mc->velocity.x = 0.0f; blocked |= 1; }
	if (origDy != dy) { mc->velocity.y = 0.0f; blocked |= 2; }
	if (origDz != dz) { mc->velocity.z = 0.0f; blocked |= 4; }
	
	return blocked;
}

/* On-rail physics - faithful translation of Alpha EntityMinecart.i_() on-rail section.
   Key differences from our old code:
   1. Uses a() smooth function for Y interpolation (no more teleporting on hills)
   2. Rider speed mult only affects movement distance, NOT stored velocity
   3. Energy conservation uses smooth Y difference, not block-level Y
   4. Snapping uses Alpha's exact parametric math (no clamping) */
static void Minecart_OnRailPhysics(struct Minecart* mc) {
	int blockX, blockY, blockZ, meta;
	const int (*m)[3];
	float smoothBeforeX, smoothBeforeY, smoothBeforeZ;
	float smoothAfterX, smoothAfterY, smoothAfterZ;
	cc_bool hadBefore, hadAfter;
	float dirX, dirZ, dirLen, dot, speed;
	float p0x, p0z, p1x, p1z, segDx, segDz, t;
	float moveX, moveZ;
	int newBX, newBZ;
	
	blockX = (int)Math_Floor(mc->pos.x);
	blockY = (int)Math_Floor(mc->pos.y);
	blockZ = (int)Math_Floor(mc->pos.z);
	
	/* Check for rail at y-1 (Alpha behavior) */
	if (World_Contains(blockX, blockY - 1, blockZ) && IsRail(World_GetBlock(blockX, blockY - 1, blockZ)))
		blockY--;
	
	if (!World_Contains(blockX, blockY, blockZ) || !IsRail(World_GetBlock(blockX, blockY, blockZ)))
		return;
	
	meta = GetRailMeta(blockX, blockY, blockZ);
	if (meta < 0 || meta > 9) return;
	
	/* Get smooth position BEFORE movement (Alpha's var8) */
	hadBefore = Minecart_GetSmoothPos(mc->pos.x, mc->pos.y, mc->pos.z,
	                                  &smoothBeforeX, &smoothBeforeY, &smoothBeforeZ);
	
	/* Alpha: posY = blockY (flat) or blockY+1 (slopes) */
	mc->pos.y = (float)blockY;
	if (meta >= 2 && meta <= 5)
		mc->pos.y = (float)(blockY + 1);
	
	/* Slope gravity push (Alpha's var6 = 1.0/128.0) */
	if (meta == 2) mc->velocity.x -= MC_SLOPE_PUSH;
	if (meta == 3) mc->velocity.x += MC_SLOPE_PUSH;
	if (meta == 4) mc->velocity.z += MC_SLOPE_PUSH;
	if (meta == 5) mc->velocity.z -= MC_SLOPE_PUSH;
	
	m = railMatrix[meta];
	
	/* Rail direction from matrix endpoints (Alpha's var11, var13) */
	dirX = (float)(m[1][0] - m[0][0]);
	dirZ = (float)(m[1][2] - m[0][2]);
	dirLen = Math_SqrtF(dirX * dirX + dirZ * dirZ);
	
	/* Project motion onto rail direction (Alpha: flip if dot < 0) */
	dot = mc->velocity.x * dirX + mc->velocity.z * dirZ;
	if (dot < 0.0f) { dirX = -dirX; dirZ = -dirZ; }
	
	/* Redistribute speed along rail (Alpha's var19) */
	speed = Math_SqrtF(mc->velocity.x * mc->velocity.x + mc->velocity.z * mc->velocity.z);
	mc->velocity.x = speed * dirX / dirLen;
	mc->velocity.z = speed * dirZ / dirLen;
	
	/* Snap position to rail line (Alpha's parametric projection - NO clamping) */
	p0x = (float)blockX + 0.5f + m[0][0] * 0.5f;
	p0z = (float)blockZ + 0.5f + m[0][2] * 0.5f;
	p1x = (float)blockX + 0.5f + m[1][0] * 0.5f;
	p1z = (float)blockZ + 0.5f + m[1][2] * 0.5f;
	segDx = p1x - p0x;
	segDz = p1z - p0z;
	
	if (segDx == 0.0f) {
		mc->pos.x = (float)blockX + 0.5f;
		t = mc->pos.z - (float)blockZ;
	} else if (segDz == 0.0f) {
		mc->pos.z = (float)blockZ + 0.5f;
		t = mc->pos.x - (float)blockX;
	} else {
		float ox = mc->pos.x - p0x;
		float oz = mc->pos.z - p0z;
		t = (ox * segDx + oz * segDz) * 2.0f;
	}
	
	mc->pos.x = p0x + segDx * t;
	mc->pos.z = p0z + segDz * t;
	
	/* Rider speed reduction: Alpha only affects movement distance, NOT stored velocity.
	   Alpha: var31 = motionX; if(riddenByEntity) var31 *= 0.75; then d(var31, 0, var33); */
	moveX = mc->velocity.x;
	moveZ = mc->velocity.z;
	if (mc->riderId >= 0) {
		moveX *= MC_RIDER_SPEED_MULT;
		moveZ *= MC_RIDER_SPEED_MULT;
	}
	
	/* Clamp movement to max speed */
	if (moveX < -MC_MAX_SPEED) moveX = -MC_MAX_SPEED;
	if (moveX >  MC_MAX_SPEED) moveX =  MC_MAX_SPEED;
	if (moveZ < -MC_MAX_SPEED) moveZ = -MC_MAX_SPEED;
	if (moveZ >  MC_MAX_SPEED) moveZ =  MC_MAX_SPEED;
	
	/* Alpha-faithful block collision via moveEntity (AABB vs actual block collision bounds).
	   On rail: moveEntity(moveX, 0, moveZ) - horizontal only, Y handled by smooth interp. */
	Minecart_MoveEntity(mc, moveX, 0.0f, moveZ);
	
	/* Slope height correction at block boundaries (Alpha's endpoint check) */
	newBX = (int)Math_Floor(mc->pos.x);
	newBZ = (int)Math_Floor(mc->pos.z);
	if (m[0][1] != 0 && newBX - blockX == m[0][0] && newBZ - blockZ == m[0][2])
		mc->pos.y += (float)m[0][1];
	else if (m[1][1] != 0 && newBX - blockX == m[1][0] && newBZ - blockZ == m[1][2])
		mc->pos.y += (float)m[1][1];
	
	/* Friction (Alpha applies to stored velocity, not movement amount) */
	if (mc->riderId >= 0) {
		mc->velocity.x *= MC_FRICTION_RIDDEN;
		mc->velocity.z *= MC_FRICTION_RIDDEN;
	} else {
		mc->velocity.x *= MC_FRICTION_EMPTY;
		mc->velocity.z *= MC_FRICTION_EMPTY;
	}
	mc->velocity.y = 0.0f;
	
	/* Smooth Y position from Alpha's a() function.
	   This is the key to smooth hill transitions. */
	hadAfter = Minecart_GetSmoothPos(mc->pos.x, mc->pos.y, mc->pos.z,
	                                 &smoothAfterX, &smoothAfterY, &smoothAfterZ);
	
	if (hadAfter && hadBefore) {
		/* Energy conservation: speed boost/loss from elevation change */
		float dY = (smoothBeforeY - smoothAfterY) * 0.05f;
		speed = Math_SqrtF(mc->velocity.x * mc->velocity.x + mc->velocity.z * mc->velocity.z);
		if (speed > 0.0f) {
			mc->velocity.x = mc->velocity.x / speed * (speed + dY);
			mc->velocity.z = mc->velocity.z / speed * (speed + dY);
		}
		/* Set Y to smoothly interpolated position (Alpha: b(posX, smoothY, posZ)) */
		mc->pos.y = smoothAfterY - MC_SMOOTH_Y_OFFSET;
	}
	
	/* Cross-block direction realignment (Alpha: straighten velocity when entering new block) */
	if (newBX != blockX || newBZ != blockZ) {
		speed = Math_SqrtF(mc->velocity.x * mc->velocity.x + mc->velocity.z * mc->velocity.z);
		mc->velocity.x = speed * (float)(newBX - blockX);
		mc->velocity.z = speed * (float)(newBZ - blockZ);
	}
}


/*########################################################################################################################*
*-------------------------------------------------Minecart-Minecart collision (booster bug)-------------------------------*
*#########################################################################################################################*/

/* Replicates Alpha 1.2.6's EntityMinecart.applyEntityCollision for minecart-minecart pairs.
   This is the famous booster bug: two carts on parallel tracks boost each other.
   
   The key difference from Alpha 1.0.6 (where boosters DON'T work) is the 0.2 damping:
   - 1.0.6: motionX = 0; addVelocity(avg - push) → result = avg - push
   - 1.2.6: motionX *= 0.2; addVelocity(avg - push) → result = 0.2*old + avg - push
   
   With 1.2.6's formula, each cart retains 20% of its original velocity plus the
   averaged velocity (which itself includes the original). The velocity coefficients
   sum to >1.0 (0.7*self + 0.5*other = 1.2), creating energy amplification each
   collision. This plus the perpendicular push (which gets re-projected onto the
   rail direction next tick via sqrt(vx²+vz²)) is what makes boosters work. */
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
	
	/* Average both carts' velocities (Alpha 1.2.6: var10/var12 after /= 2) */
	avgMX = (a->velocity.x + b->velocity.x) * 0.5f;
	avgMZ = (a->velocity.z + b->velocity.z) * 0.5f;
	
	/* Alpha 1.2.6 collision: dampen existing velocity to 20%, then add average +/- push.
	   The 0.2 retention means each cart keeps some of its original velocity on top of
	   the averaged velocity, causing total system energy to INCREASE with each collision.
	   The perpendicular push component gets converted to rail-aligned speed next tick
	   when OnRailPhysics re-projects velocity via sqrt(vx² + vz²). */
	a->velocity.x *= 0.2f;
	a->velocity.z *= 0.2f;
	a->velocity.x += avgMX - pushX;
	a->velocity.z += avgMZ - pushZ;
	
	b->velocity.x *= 0.2f;
	b->velocity.z *= 0.2f;
	b->velocity.x += avgMX + pushX;
	b->velocity.z += avgMZ + pushZ;
}


/*########################################################################################################################*
*-------------------------------------------------Player push / off-rail physics------------------------------------------*
*#########################################################################################################################*/
/* When a player walks into a minecart, push it */
static void Minecart_CheckPlayerPush(struct Minecart* mc) {
	struct LocalPlayer* p = Entities.CurPlayer;
	float dx, dy, dz, dist, pushStrength;
	
	if (!p) return;
	if (mc->riderId >= 0) return; /* Don't push occupied carts */
	
	/* Check Y proximity: player must be roughly at cart height.
	   Player feet Y vs cart Y: reject if too far above or below. */
	dy = p->Base.Position.y - mc->pos.y;
	if (dy < -MC_PUSH_Y_RANGE || dy > MC_PUSH_Y_RANGE) return;
	
	dx = mc->pos.x - p->Base.Position.x;
	dz = mc->pos.z - p->Base.Position.z;
	dist = Math_SqrtF(dx * dx + dz * dz);
	
	if (dist > 1.0f || dist < 0.01f) return;
	
	/* Push away from player */
	pushStrength = (1.0f - dist) * 0.1f;
	mc->velocity.x += (dx / dist) * pushStrength;
	mc->velocity.z += (dz / dist) * pushStrength;
}

/* Apply off-rail physics (Alpha behavior: clamp, ground friction, moveEntity, air friction).
   Note: gravity is applied in Minecart_TickOne before this is called.
   
   Uses the same Alpha-faithful moveEntity that handles all three axes with proper
   AABB collision against actual block bounds.
   
   Ground detection (Alpha): onGround is set inside moveEntity based on whether
   Y movement was clipped while moving downward. Ground friction uses the PREVIOUS
   tick's onGround, while air friction uses the CURRENT tick's (updated by moveEntity). */
static void Minecart_OffRailPhysics(struct Minecart* mc) {
	float origDy;
	int moveResult;
	cc_bool onGround;
	
	/* Clamp to max speed (Alpha does this first) */
	if (mc->velocity.x < -MC_MAX_SPEED) mc->velocity.x = -MC_MAX_SPEED;
	if (mc->velocity.x >  MC_MAX_SPEED) mc->velocity.x =  MC_MAX_SPEED;
	if (mc->velocity.z < -MC_MAX_SPEED) mc->velocity.z = -MC_MAX_SPEED;
	if (mc->velocity.z >  MC_MAX_SPEED) mc->velocity.z =  MC_MAX_SPEED;
	
	/* Alpha: ground friction BEFORE movement, based on PREVIOUS tick's onGround */
	if (mc->onGround) {
		mc->velocity.x *= MC_OFF_RAIL_GROUND;
		mc->velocity.y *= MC_OFF_RAIL_GROUND;
		mc->velocity.z *= MC_OFF_RAIL_GROUND;
	}
	
	/* Alpha-faithful moveEntity with all three axes.
	   This resolves Y (gravity/ground), X and Z (walls) using actual block AABBs.
	   Returns bit flags: 1=X blocked, 2=Y blocked, 4=Z blocked. */
	origDy = mc->velocity.y;
	moveResult = Minecart_MoveEntity(mc, mc->velocity.x, mc->velocity.y, mc->velocity.z);
	
	/* Alpha: onGround = Y was clipped AND original Y motion was downward */
	onGround = (moveResult & 2) && (origDy < 0.0f);
	mc->onGround = onGround;
	
	/* Alpha: air friction AFTER movement (only when NOT on ground) */
	if (!onGround) {
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
	float newYaw, yawDiff;
	
	/* Alpha: yaw is computed from actual displacement (prevPos - pos), NOT velocity.
	   Alpha uses: atan2(prevPosZ - posZ, prevPosX - posX) which gives the angle
	   pointing BACKWARD along the movement direction. The isInReverse flag then
	   flips it 180 degrees on the first tick, resulting in forward-facing yaw.
	   
	   We use the CC equivalent: the displacement direction itself via
	   atan2(pos - prevPos) to get the forward direction, then apply the same
	   reverse detection logic. */
	{
		float dX = mc->prevPos.x - mc->pos.x;
		float dZ = mc->prevPos.z - mc->pos.z;
		if (dX * dX + dZ * dZ > 0.001f) {
			/* Alpha: atan2(prevZ-posZ, prevX-posX) * 180/PI
			   CC's Math_Atan2f(x,y) = atan2(y,x), so Math_Atan2f(dX,dZ) = atan2(dZ,dX) */
			newYaw = Math_Atan2f(dX, dZ) * MATH_RAD2DEG;
			if (mc->isInReverse) {
				newYaw += 180.0f;
			}
			
			/* Alpha reverse detection: if yaw changed by >= 170 degrees, flip */
			yawDiff = newYaw - mc->yaw;
			while (yawDiff >= 180.0f) yawDiff -= 360.0f;
			while (yawDiff < -180.0f) yawDiff += 360.0f;
			
			if (yawDiff < -170.0f || yawDiff >= 170.0f) {
				newYaw += 180.0f;
				mc->isInReverse = !mc->isInReverse;
			}
			
			mc->yaw = newYaw;
		}
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

/* Main tick for one minecart - matches Alpha EntityMinecart.onUpdate() flow exactly */
static void Minecart_TickOne(struct Minecart* mc, float delta) {
	int blockX, blockY, blockZ;
	cc_bool onRail;
	
	/* Alpha: save previous position at start of tick (for yaw from displacement) */
	mc->prevPos = mc->pos;
	
	/* Alpha: gravity always applied BEFORE checking for rail */
	mc->velocity.y -= MC_GRAVITY;
	
	blockX = (int)Math_Floor(mc->pos.x);
	blockY = (int)Math_Floor(mc->pos.y);
	blockZ = (int)Math_Floor(mc->pos.z);
	
	/* Alpha: check rail at y-1 */
	if (World_Contains(blockX, blockY - 1, blockZ) && IsRail(World_GetBlock(blockX, blockY - 1, blockZ)))
		blockY--;
	
	onRail = (World_Contains(blockX, blockY, blockZ) && IsRail(World_GetBlock(blockX, blockY, blockZ)));
	
	if (onRail) {
		Minecart_OnRailPhysics(mc);
	} else {
		Minecart_OffRailPhysics(mc);
	}
	
	/* Player push (only when not riding) */
	if (mc->riderId < 0) {
		Minecart_CheckPlayerPush(mc);
	}
	
	/* Alpha does NOT have a velocity kill threshold.
	   Velocities decay naturally through friction (0.96 per tick). */
	
	Minecart_UpdateVisuals(mc);
	Minecart_UpdateEntity(mc);
	
	/* Alpha: check collisions with other minecarts (part of this entity's tick).
	   Called AFTER physics and visuals, BEFORE next cart ticks. */
	{
		int mySlot = (int)(mc - Minecarts);
		int j;
		for (j = 0; j < MAX_MINECARTS; j++) {
			if (j == mySlot || !Minecarts[j].active) continue;
			if (Minecarts[j].entityId == mc->riderId) continue; /* Don't collide with rider */
			
			{
				float dx = Minecarts[j].pos.x - mc->pos.x;
				float dy = Minecarts[j].pos.y - mc->pos.y;
				float dz = Minecarts[j].pos.z - mc->pos.z;
				float distSq;
				
				/* AABB overlap check matching Alpha's getEntitiesWithinAABBExcludingEntity.
				   X/Z: cart half-width 0.49 + 0.2 expansion + other cart's 0.49 = 1.18.
				   Y: cart height 0.7, no expansion. Two carts overlap when |dy| < 0.7. */
				if (dx > -1.18f && dx < 1.18f && dz > -1.18f && dz < 1.18f &&
					dy > -MC_HEIGHT && dy < MC_HEIGHT) {
					distSq = dx * dx + dz * dz;
					/* Alpha checks dist² >= 1.0E-4F before proceeding */
					if (distSq >= 1.0E-4f) {
						Minecart_ApplyBooster(mc, &Minecarts[j]);
					}
				}
			}
		}
	}
	
	/* Despawn if fallen into void */
	if (mc->pos.y < -64.0f) {
		int slot = (int)(mc - Minecarts);
		Minecart_Despawn(slot);
	}
}

/* Update player position to follow riding cart.
   This is a backup sync in case LocalPlayer_Tick's riding
   path doesn't fully cover all edge cases. */
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
	p->Base.Position.y = mc->pos.y + 0.0625f;
	p->Base.Position.z = mc->pos.z;
	
	/* Sync interpolation state so rendering is smooth */
	p->Base.prev.pos = p->Base.Position;
	p->Base.next.pos = p->Base.Position;
	
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

static void Minecart_OnNewMap(void) {
	int i;
	for (i = 0; i < MAX_MINECARTS; i++) {
		if (Minecarts[i].active) {
			Minecart_Despawn(i);
		}
	}
	playerRidingCart = -1;
}

struct IGameComponent Minecart_Component = {
	Minecart_OnInit,    /* Init  */
	NULL,               /* Free  */
	Minecart_OnReset,   /* Reset */
	Minecart_OnNewMap,  /* OnNewMap */
};
