#include "BlockPhysics.h"
#include "World.h"
#include "Constants.h"
#include "Funcs.h"
#include "Event.h"
#include "ExtMath.h"
#include "Block.h"
#include "Lighting.h"
#include "Options.h"
#include "Generator.h"
#include "Platform.h"
#include "Game.h"
#include "Logger.h"
#include "Vectors.h"
#include "Chat.h"
#include "Audio.h"
#include "Entity.h"
#include "Particle.h"
#include "InputHandler.h"
#include "Gui.h"

/* Data for a resizable queue, used for liquid physic tick entries. */
struct TickQueue {
	cc_uint32* entries; /* Buffer holding the items in the tick queue */
	int capacity; /* Max number of elements in the buffer */
	int mask;     /* capacity - 1, as capacity is always a power of two */
	int count;    /* Number of used elements */
	int head;     /* Head index into the buffer */
	int tail;     /* Tail index into the buffer */
};

static void TickQueue_Init(struct TickQueue* queue) {
	queue->entries  = NULL;
	queue->capacity = 0;
	queue->mask  = 0;
	queue->count = 0;
	queue->head  = 0;
	queue->tail  = 0;
}

static void TickQueue_Clear(struct TickQueue* queue) {
	if (!queue->entries) return;
	Mem_Free(queue->entries);
	TickQueue_Init(queue);
}

static void TickQueue_Resize(struct TickQueue* queue) {
	cc_uint32* entries;
	int i, idx, capacity;

	if (queue->capacity >= (Int32_MaxValue / 4)) {
		Chat_AddRaw("&cToo many physics entries, clearing");
		TickQueue_Clear(queue);
	}

	capacity = queue->capacity * 2;
	if (capacity < 32) capacity = 32;
	entries = (cc_uint32*)Mem_Alloc(capacity, 4, "physics tick queue");

	/* Elements must be readjusted to avoid index wrapping issues */
	/* https://stackoverflow.com/questions/55343683/resizing-of-the-circular-queue-using-dynamic-array */
	for (i = 0; i < queue->count; i++) {
		idx = (queue->head + i) & queue->mask;
		entries[i] = queue->entries[idx];
	}
	Mem_Free(queue->entries);

	queue->entries  = entries;
	queue->capacity = capacity;
	queue->mask     = capacity - 1; /* capacity is power of two */
	queue->head = 0;
	queue->tail = queue->count;
}

/* Appends an entry to the end of the queue, resizing if necessary. */
static void TickQueue_Enqueue(struct TickQueue* queue, cc_uint32 item) {
	if (queue->count == queue->capacity)
		TickQueue_Resize(queue);

	queue->entries[queue->tail] = item;
	queue->tail = (queue->tail + 1) & queue->mask;
	queue->count++;
}

/* Retrieves the entry from the front of the queue. */
static cc_uint32 TickQueue_Dequeue(struct TickQueue* queue) {
	cc_uint32 result = queue->entries[queue->head];
	queue->head = (queue->head + 1) & queue->mask;
	queue->count--;
	return result;
}


struct Physics_ Physics;
static RNGState physics_rnd;
static int physics_tickCount;
static int physics_maxWaterX, physics_maxWaterY, physics_maxWaterZ;
static struct TickQueue lavaQ, waterQ, fireQ, wheatQ;

/* Flow level metadata array - parallels World.Blocks */
/* 0 = source, 1-7 = flow distance, 8+ = falling */
static cc_uint8* physics_flowLevels;
static int physics_flowLevelsSize;

#define PHYSICS_DELAY_MASK 0xF8000000UL
#define PHYSICS_POS_MASK   0x07FFFFFFUL
#define PHYSICS_DELAY_SHIFT 27
#define PHYSICS_ONE_DELAY   (1U << PHYSICS_DELAY_SHIFT)
#define PHYSICS_LAVA_DELAY (30U << PHYSICS_DELAY_SHIFT)
#define PHYSICS_WATER_DELAY (5U << PHYSICS_DELAY_SHIFT)

static void Redstone_Reset(void); /* forward declaration */
static void IronDoor_ScanWorld(void); /* forward declaration */
static void TNT_ScanWorld(void); /* forward declaration */
static void Wheat_ScanWorld(void);  /* forward declaration */

static void Physics_OnNewMapLoaded(void* obj) {
	TickQueue_Clear(&lavaQ);
	TickQueue_Clear(&waterQ);
	TickQueue_Clear(&fireQ);
	TickQueue_Clear(&wheatQ);

	physics_maxWaterX = World.MaxX - 2;
	physics_maxWaterY = World.MaxY - 2;
	physics_maxWaterZ = World.MaxZ - 2;

	Tree_Blocks = World.Blocks;
	Random_SeedFromCurrentTime(&physics_rnd);
	Tree_Rnd = &physics_rnd;
	
	/* Allocate/reallocate flow level metadata */
	if (physics_flowLevels) Mem_Free(physics_flowLevels);
	physics_flowLevels = NULL;
	physics_flowLevelsSize = 0;
	if (World.Volume > 0) {
		physics_flowLevels = (cc_uint8*)Mem_AllocCleared(World.Volume, 1, "flow levels");
		physics_flowLevelsSize = World.Volume;
	}
	Physics.FlowLevels = physics_flowLevels;

	Redstone_Reset();
}

void Physics_SetEnabled(cc_bool enabled) {
	Physics.Enabled = enabled;
	Physics_OnNewMapLoaded(NULL);
}

/* Forward declarations for finite liquid handlers */
static void FiniteLiquid_PlaceLava(int index, BlockID block);
static void FiniteLiquid_PlaceWater(int index, BlockID block);
static void FiniteLiquid_DeleteWater(int index, BlockID block);
static void FiniteLiquid_DeleteLava(int index, BlockID block);
static void FiniteLiquid_ActivateWater(int index, BlockID block);
static void FiniteLiquid_ActivateLava(int index, BlockID block);
static void FiniteLiquid_TickWater(void);
static void FiniteLiquid_TickLava(void);
/* Forward declarations for classic liquid handlers */
static void Physics_PlaceLava(int index, BlockID block);
static void Physics_PlaceWater(int index, BlockID block);
static void Physics_ActivateWater(int index, BlockID block);
static void Physics_ActivateLava(int index, BlockID block);
static void Physics_PlaceSponge(int index, BlockID block);
static void Physics_DeleteSponge(int index, BlockID block);

/* Helper to swap liquid handler registrations based on FiniteLiquid flag */
static void Physics_RegisterLiquidHandlers(void) {
	if (Physics.FiniteLiquid) {
		Physics.OnPlace[BLOCK_LAVA]    = FiniteLiquid_PlaceLava;
		Physics.OnPlace[BLOCK_WATER]   = FiniteLiquid_PlaceWater;
		Physics.OnDelete[BLOCK_WATER]       = FiniteLiquid_DeleteWater;
		Physics.OnDelete[BLOCK_STILL_WATER] = FiniteLiquid_DeleteWater;
		Physics.OnDelete[BLOCK_LAVA]        = FiniteLiquid_DeleteLava;
		Physics.OnDelete[BLOCK_STILL_LAVA]  = FiniteLiquid_DeleteLava;
	} else {
		Physics.OnPlace[BLOCK_LAVA]    = Physics_PlaceLava;
		Physics.OnPlace[BLOCK_WATER]   = Physics_PlaceWater;
		Physics.OnDelete[BLOCK_WATER]       = NULL;
		Physics.OnDelete[BLOCK_STILL_WATER] = NULL;
		Physics.OnDelete[BLOCK_LAVA]        = NULL;
		Physics.OnDelete[BLOCK_STILL_LAVA]  = NULL;
	}
	Physics.OnPlace[BLOCK_SPONGE]  = Physics_PlaceSponge;
	Physics.OnDelete[BLOCK_SPONGE] = Physics_DeleteSponge;

	if (Physics.FiniteLiquid) {
		Physics.OnActivate[BLOCK_WATER]       = FiniteLiquid_ActivateWater;
		Physics.OnActivate[BLOCK_STILL_WATER] = FiniteLiquid_ActivateWater;
		Physics.OnActivate[BLOCK_LAVA]        = FiniteLiquid_ActivateLava;
		Physics.OnActivate[BLOCK_STILL_LAVA]  = FiniteLiquid_ActivateLava;

		/* Finite liquid does NOT use random ticks - only scheduled ticks via queue.
		   Using random ticks would cause stable water to repeatedly re-spread. */
		Physics.OnRandomTick[BLOCK_WATER]       = NULL;
		Physics.OnRandomTick[BLOCK_STILL_WATER] = NULL;
		Physics.OnRandomTick[BLOCK_LAVA]        = NULL;
		Physics.OnRandomTick[BLOCK_STILL_LAVA]  = NULL;
	} else {
		Physics.OnActivate[BLOCK_WATER]       = Physics.OnPlace[BLOCK_WATER];
		Physics.OnActivate[BLOCK_STILL_WATER] = Physics.OnPlace[BLOCK_WATER];
		Physics.OnActivate[BLOCK_LAVA]        = Physics.OnPlace[BLOCK_LAVA];
		Physics.OnActivate[BLOCK_STILL_LAVA]  = Physics.OnPlace[BLOCK_LAVA];

		Physics.OnRandomTick[BLOCK_WATER]       = Physics_ActivateWater;
		Physics.OnRandomTick[BLOCK_STILL_WATER] = Physics_ActivateWater;
		Physics.OnRandomTick[BLOCK_LAVA]        = Physics_ActivateLava;
		Physics.OnRandomTick[BLOCK_STILL_LAVA]  = Physics_ActivateLava;
	}
}

void Physics_SetFiniteLiquid(cc_bool enabled) {
	Physics.FiniteLiquid = enabled;
	/* Ensure flow levels array is allocated if enabling mid-game */
	if (enabled && !physics_flowLevels && World.Volume > 0) {
		physics_flowLevels = (cc_uint8*)Mem_AllocCleared(World.Volume, 1, "flow levels");
		physics_flowLevelsSize = World.Volume;
	}
	Physics.FlowLevels = physics_flowLevels;
	/* Clear liquid queues to avoid stale entries */
	TickQueue_Clear(&lavaQ);
	TickQueue_Clear(&waterQ);
	TickQueue_Init(&lavaQ);
	TickQueue_Init(&waterQ);
	/* Swap handlers */
	Physics_RegisterLiquidHandlers();
}

static void Physics_Activate(int index) {
	BlockID block = World.Blocks[index];
	PhysicsHandler activate = Physics.OnActivate[block];
	if (activate) activate(index, block);
}

static void Physics_ActivateNeighbours(int x, int y, int z, int index) {
	if (x > 0)          Physics_Activate(index - 1);
	if (x < World.MaxX) Physics_Activate(index + 1);
	if (z > 0)          Physics_Activate(index - World.Width);
	if (z < World.MaxZ) Physics_Activate(index + World.Width);
	if (y > 0)          Physics_Activate(index - World.OneY);
	if (y < World.MaxY) Physics_Activate(index + World.OneY);
}

static cc_bool Physics_IsEdgeWater(int x, int y, int z) {
	return
		(Env.EdgeBlock == BLOCK_WATER || Env.EdgeBlock == BLOCK_STILL_WATER)
		&& (y >= Env_SidesHeight && y < Env.EdgeHeight)
		&& (x == 0 || z == 0 || x == World.MaxX || z == World.MaxZ);
}

static cc_bool Physics_IsEdgeLava(int x, int y, int z) {
	return
		(Env.EdgeBlock == BLOCK_LAVA || Env.EdgeBlock == BLOCK_STILL_LAVA)
		&& (y >= Env_SidesHeight && y < Env.EdgeHeight)
		&& (x == 0 || z == 0 || x == World.MaxX || z == World.MaxZ);
}


void Physics_OnBlockChanged(int x, int y, int z, BlockID old, BlockID now) {
	PhysicsHandler handler;
	int index;
	if (!Physics.Enabled) return;

	if (now == BLOCK_AIR && Physics_IsEdgeWater(x, y, z)) {
		now = BLOCK_STILL_WATER;
		Game_UpdateBlock(x, y, z, BLOCK_STILL_WATER);
	}
	if (now == BLOCK_AIR && Physics_IsEdgeLava(x, y, z)) {
		now = BLOCK_STILL_LAVA;
		Game_UpdateBlock(x, y, z, BLOCK_STILL_LAVA);
	}
	index = World_Pack(x, y, z);

	/* User can place/delete blocks over ID 256 */
	if (now == BLOCK_AIR) {
		handler = Physics.OnDelete[(BlockRaw)old];
		if (handler) handler(index, old);
	} else {
		handler = Physics.OnPlace[(BlockRaw)now];
		if (handler) handler(index, now);
	}
	Physics_ActivateNeighbours(x, y, z, index);
}

static void Physics_TickRandomBlocks(void) {
	int lo, hi, index;
	BlockID block;
	PhysicsHandler tick;
	int x, y, z, x2, y2, z2;

	for (y = 0; y < World.Height; y += CHUNK_SIZE) {
		y2 = min(y + CHUNK_MAX, World.MaxY);
		for (z = 0; z < World.Length; z += CHUNK_SIZE) {
			z2 = min(z + CHUNK_MAX, World.MaxZ);
			for (x = 0; x < World.Width; x += CHUNK_SIZE) {
				x2 = min(x + CHUNK_MAX, World.MaxX);

				/* Inlined 3 random ticks for this chunk */
				lo = World_Pack( x,  y,  z);
				hi = World_Pack(x2, y2, z2);
				
				index = Random_Range(&physics_rnd, lo, hi);
				block = World.Blocks[index];
				tick = Physics.OnRandomTick[block];
				if (tick) tick(index, block);

				index = Random_Range(&physics_rnd, lo, hi);
				block = World.Blocks[index];
				tick = Physics.OnRandomTick[block];
				if (tick) tick(index, block);

				index = Random_Range(&physics_rnd, lo, hi);
				block = World.Blocks[index];
				tick = Physics.OnRandomTick[block];
				if (tick) tick(index, block);
			}
		}
	}
}


static void Physics_DoFalling(int index, BlockID block) {
	int found = -1, start = index;
	BlockID other;
	int x, y, z;

	/* Find lowest block can fall into */
	while (index >= World.OneY) {
		index -= World.OneY;
		other  = World.Blocks[index];

		if (other == BLOCK_AIR || (other >= BLOCK_WATER && other <= BLOCK_STILL_LAVA))
			found = index;
		else
			break;
	}

	if (found == -1) return;
	World_Unpack(found, x, y, z);
	Game_UpdateBlock(x, y, z, block);

	World_Unpack(start, x, y, z);
	Game_UpdateBlock(x, y, z, BLOCK_AIR);
	Physics_ActivateNeighbours(x, y, z, start);
}

static void Physics_HandleLadder(int index, BlockID block) {
	BlockID neighbor;
	cc_bool hasSupport = false;
	int x, y, z;
	
	World_Unpack(index, x, y, z);
	
	/* Check all 4 horizontal neighbors for opaque blocks */
	if (x > 0) {
		neighbor = World_GetBlock(x - 1, y, z);
		if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
	}
	if (x < World.MaxX) {
		neighbor = World_GetBlock(x + 1, y, z);
		if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
	}
	if (z > 0) {
		neighbor = World_GetBlock(x, y, z - 1);
		if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
	}
	if (z < World.MaxZ) {
		neighbor = World_GetBlock(x, y, z + 1);
		if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
	}
	
	/* No adjacent support block - break the ladder */
	if (!hasSupport) {
		Physics_DropBlock(x, y, z, BLOCK_LADDER);
		Game_UpdateBlock(x, y, z, BLOCK_AIR);
		Physics_ActivateNeighbours(x, y, z, index);
	}
}

static void Physics_HandleTorch(int index, BlockID block) {
	BlockID neighbor;
	cc_bool hasSupport = false;
	int x, y, z;
	
	World_Unpack(index, x, y, z);
	
	/* Check block below (ground support) */
	if (y > 0) {
		neighbor = World_GetBlock(x, y - 1, z);
		if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
	}
	
	/* Check 4 horizontal neighbors for opaque blocks (wall support) */
	if (!hasSupport && x > 0) {
		neighbor = World_GetBlock(x - 1, y, z);
		if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
	}
	if (!hasSupport && x < World.MaxX) {
		neighbor = World_GetBlock(x + 1, y, z);
		if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
	}
	if (!hasSupport && z > 0) {
		neighbor = World_GetBlock(x, y, z - 1);
		if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
	}
	if (!hasSupport && z < World.MaxZ) {
		neighbor = World_GetBlock(x, y, z + 1);
		if (Blocks.Draw[neighbor] == DRAW_OPAQUE) hasSupport = true;
	}
	
	/* No support at all - break the torch */
	if (!hasSupport) {
		Physics_DropBlock(x, y, z, BLOCK_TORCH);
		Game_UpdateBlock(x, y, z, BLOCK_AIR);
		Physics_ActivateNeighbours(x, y, z, index);
	}
}

static void Physics_DeleteIce(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);
	/* Replace ice with water when broken */
	Game_UpdateBlock(x, y, z, BLOCK_STILL_WATER);
}

static void Physics_HandleSnow(int index, BlockID block) {
	BlockID neighbor;
	int x, y, z;
	
	World_Unpack(index, x, y, z);
	
	/* Check block below - snow needs solid support */
	if (y > 0) {
		neighbor = World_GetBlock(x, y - 1, z);
		if (neighbor != BLOCK_AIR) return; /* Has support */
	}
	
	/* No support - snow disappears without dropping (only drops when broken by shovel) */
	Game_UpdateBlock(x, y, z, BLOCK_AIR);
	Physics_ActivateNeighbours(x, y, z, index);
}

/*########################################################################################################################*
*-------------------------------------------------Redstone shared data----------------------------------------------------*
*#########################################################################################################################*/
/* BFS queue size for redstone power propagation */
#define REDSTONE_MAX_QUEUE 4096
#define REDSTONE_MAX_POWER 15

/* Shared constant offset arrays used by multiple redstone functions */
static const int adjOffsets6[6][3] = {{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
static const int hOffsets4[4][2] = {{-1,0},{1,0},{0,-1},{0,1}};

/* Static buffers for Redstone_PropagatePower - avoids ~88KB+ of stack allocations */
/* Safe because redstone_propagating flag prevents concurrent use */
static int pp_dustPositions[REDSTONE_MAX_QUEUE][3];
static int pp_dustPower[REDSTONE_MAX_QUEUE];
/* Visited bitset for dust discovery - indexed by World_Pack position */
/* Allocated once when world loads, avoids O(n^2) linear visited checks */
static cc_uint8* pp_visitedBits = NULL;
static int pp_visitedCapacity = 0; /* in bytes */
/* Positions touched in visited bitset, for fast clearing */
static int pp_visitedPositions[REDSTONE_MAX_QUEUE];
static int pp_visitedCount = 0;

static void Redstone_AllocVisited(void) {
	int worldVolume = World.Width * World.Height * World.Length;
	int needed = (worldVolume + 7) / 8;
	if (pp_visitedBits && pp_visitedCapacity >= needed) return;
	if (pp_visitedBits) Mem_Free(pp_visitedBits);
	pp_visitedBits = (cc_uint8*)Mem_AllocCleared(needed, 1, "redstone visited");
	pp_visitedCapacity = needed;
}

static void Redstone_FreeVisited(void) {
	if (pp_visitedBits) { Mem_Free(pp_visitedBits); pp_visitedBits = NULL; }
	pp_visitedCapacity = 0;
}

#define PP_VISITED_SET(idx) pp_visitedBits[(idx) >> 3] |= (1 << ((idx) & 7))
#define PP_VISITED_GET(idx) (pp_visitedBits[(idx) >> 3] & (1 << ((idx) & 7)))

static void PP_ClearVisited(void) {
	int i;
	for (i = 0; i < pp_visitedCount; i++) {
		int idx = pp_visitedPositions[i];
		pp_visitedBits[idx >> 3] = 0;
	}
	pp_visitedCount = 0;
}

/*########################################################################################################################*
*-------------------------------------------------Redstone power system---------------------------------------------------*
*#########################################################################################################################*/
/* Check if a block is any form of redstone dust */
static cc_bool Redstone_IsDust(BlockID block) {
	return block == BLOCK_RED_ORE_DUST || block == BLOCK_LIT_RED_ORE_DUST;
}

/* Check if a block is a lit (active) redstone torch */
static cc_bool Redstone_IsTorchOn(BlockID block) {
	return block == BLOCK_RED_ORE_TORCH
		|| block == BLOCK_RED_TORCH_ON_S || block == BLOCK_RED_TORCH_ON_N
		|| block == BLOCK_RED_TORCH_ON_E || block == BLOCK_RED_TORCH_ON_W
		|| block == BLOCK_RED_TORCH_UNMOUNTED;
}

/* Check if a block is an unlit (inactive) redstone torch */
static cc_bool Redstone_IsTorchOff(BlockID block) {
	return block == BLOCK_RED_ORE_TORCH_OFF
		|| block == BLOCK_RED_TORCH_OFF_S || block == BLOCK_RED_TORCH_OFF_N
		|| block == BLOCK_RED_TORCH_OFF_E || block == BLOCK_RED_TORCH_OFF_W
		|| block == BLOCK_RED_TORCH_UNMOUNTED_OFF;
}

/* Check if a block is any redstone torch */
static cc_bool Redstone_IsTorch(BlockID block) {
	return Redstone_IsTorchOn(block) || Redstone_IsTorchOff(block);
}

/* Check if dust at (dx,dy,dz) visually reaches toward direction (dirX,dirZ) */
/* Must match the texture shape from RedOreDust_GetTexture in Block.c: */
/* - No connections = cross texture = reaches all 4 directions */
/* - Single connection = line = reaches both ends (e.g. only north → N-S line) */
/* - Corner/T/cross = explicit connections only */
static cc_bool Redstone_DustConnectsToward(int dx, int dy, int dz, int dirX, int dirZ) {
	cc_bool north, south, east, west;
	
	/* Calculate dust connections at (dx,dy,dz) - mirrors Block.c logic */
	/* Connects to dust (including pour-over) and to redstone torches at same level */
	north = south = east = west = false;
	
	/* North (z-1) */
	if (Redstone_IsDust(World_GetBlock(dx, dy, dz - 1)) || Redstone_IsTorch(World_GetBlock(dx, dy, dz - 1))) {
		north = true;
	} else {
		if (dy > 0 && World_GetBlock(dx, dy, dz - 1) == BLOCK_AIR && Redstone_IsDust(World_GetBlock(dx, dy - 1, dz - 1)))
			north = true;
		if (dy < World.MaxY && Redstone_IsDust(World_GetBlock(dx, dy + 1, dz - 1)) && World_GetBlock(dx, dy + 1, dz) == BLOCK_AIR)
			north = true;
	}
	/* South (z+1) */
	if (Redstone_IsDust(World_GetBlock(dx, dy, dz + 1)) || Redstone_IsTorch(World_GetBlock(dx, dy, dz + 1))) {
		south = true;
	} else {
		if (dy > 0 && World_GetBlock(dx, dy, dz + 1) == BLOCK_AIR && Redstone_IsDust(World_GetBlock(dx, dy - 1, dz + 1)))
			south = true;
		if (dy < World.MaxY && Redstone_IsDust(World_GetBlock(dx, dy + 1, dz + 1)) && World_GetBlock(dx, dy + 1, dz) == BLOCK_AIR)
			south = true;
	}
	/* East (x+1) */
	if (Redstone_IsDust(World_GetBlock(dx + 1, dy, dz)) || Redstone_IsTorch(World_GetBlock(dx + 1, dy, dz))) {
		east = true;
	} else {
		if (dy > 0 && World_GetBlock(dx + 1, dy, dz) == BLOCK_AIR && Redstone_IsDust(World_GetBlock(dx + 1, dy - 1, dz)))
			east = true;
		if (dy < World.MaxY && Redstone_IsDust(World_GetBlock(dx + 1, dy + 1, dz)) && World_GetBlock(dx, dy + 1, dz) == BLOCK_AIR)
			east = true;
	}
	/* West (x-1) */
	if (Redstone_IsDust(World_GetBlock(dx - 1, dy, dz)) || Redstone_IsTorch(World_GetBlock(dx - 1, dy, dz))) {
		west = true;
	} else {
		if (dy > 0 && World_GetBlock(dx - 1, dy, dz) == BLOCK_AIR && Redstone_IsDust(World_GetBlock(dx - 1, dy - 1, dz)))
			west = true;
		if (dy < World.MaxY && Redstone_IsDust(World_GetBlock(dx - 1, dy + 1, dz)) && World_GetBlock(dx, dy + 1, dz) == BLOCK_AIR)
			west = true;
	}
	
	/* Now determine which directions the dust VISUALLY reaches based on its texture shape */
	/* This must mirror the texture selection in RedOreDust_GetTexture */
	
	/* No connections = cross/dot texture = reaches all 4 directions */
	if (!north && !south && !east && !west) return true;
	
	/* All 4 connected = cross = reaches all 4 */
	if (north && south && east && west) return true;
	
	/* Line shapes: a single axis connection shows as a full line reaching BOTH ends */
	/* e.g. only north, or only south, or north+south all show vertical line */
	if ((north || south) && !east && !west) {
		return (dirZ == -1 || dirZ == 1); /* vertical line reaches N and S */
	}
	if ((east || west) && !north && !south) {
		return (dirX == -1 || dirX == 1); /* horizontal line reaches E and W */
	}
	
	/* Corner and T-junction shapes: only the explicitly connected directions */
	if (dirZ == -1) return north;
	if (dirZ == 1)  return south;
	if (dirX == 1)  return east;
	if (dirX == -1) return west;
	return false;
}

/* Get the corresponding OFF variant for an ON torch */
static BlockID Redstone_GetTorchOffVariant(BlockID on) {
	switch (on) {
		case BLOCK_RED_ORE_TORCH:  return BLOCK_RED_ORE_TORCH_OFF;
		case BLOCK_RED_TORCH_ON_S: return BLOCK_RED_TORCH_OFF_S;
		case BLOCK_RED_TORCH_ON_N: return BLOCK_RED_TORCH_OFF_N;
		case BLOCK_RED_TORCH_ON_E: return BLOCK_RED_TORCH_OFF_E;
		case BLOCK_RED_TORCH_ON_W: return BLOCK_RED_TORCH_OFF_W;
		case BLOCK_RED_TORCH_UNMOUNTED: return BLOCK_RED_TORCH_UNMOUNTED_OFF;
		default: return BLOCK_RED_ORE_TORCH_OFF;
	}
}

/* Get the corresponding ON variant for an OFF torch */
static BlockID Redstone_GetTorchOnVariant(BlockID off) {
	switch (off) {
		case BLOCK_RED_ORE_TORCH_OFF:  return BLOCK_RED_ORE_TORCH;
		case BLOCK_RED_TORCH_OFF_S: return BLOCK_RED_TORCH_ON_S;
		case BLOCK_RED_TORCH_OFF_N: return BLOCK_RED_TORCH_ON_N;
		case BLOCK_RED_TORCH_OFF_E: return BLOCK_RED_TORCH_ON_E;
		case BLOCK_RED_TORCH_OFF_W: return BLOCK_RED_TORCH_ON_W;
		case BLOCK_RED_TORCH_UNMOUNTED_OFF: return BLOCK_RED_TORCH_UNMOUNTED;
		default: return BLOCK_RED_ORE_TORCH;
	}
}

/* Get the block a torch is attached to (returns coords via pointers, returns true if found) */
/* Uses the block ID to determine attachment direction (no directional cache dependency) */
static cc_bool Redstone_GetTorchAttachBlock(int tx, int ty, int tz, int* ax, int* ay, int* az) {
	BlockID block = World_GetBlock(tx, ty, tz);
	
	/* Wall-mounted variants encode direction in block ID */
	if (block == BLOCK_RED_TORCH_ON_S || block == BLOCK_RED_TORCH_OFF_S) {
		/* Attached to z+1 (south) */
		if (tz < World.MaxZ) { *ax = tx; *ay = ty; *az = tz + 1; return true; }
	} else if (block == BLOCK_RED_TORCH_ON_N || block == BLOCK_RED_TORCH_OFF_N) {
		/* Attached to z-1 (north) */
		if (tz > 0) { *ax = tx; *ay = ty; *az = tz - 1; return true; }
	} else if (block == BLOCK_RED_TORCH_ON_E || block == BLOCK_RED_TORCH_OFF_E) {
		/* Attached to x+1 (east) */
		if (tx < World.MaxX) { *ax = tx + 1; *ay = ty; *az = tz; return true; }
	} else if (block == BLOCK_RED_TORCH_ON_W || block == BLOCK_RED_TORCH_OFF_W) {
		/* Attached to x-1 (west) */
		if (tx > 0) { *ax = tx - 1; *ay = ty; *az = tz; return true; }
	} else if (block == BLOCK_RED_ORE_TORCH || block == BLOCK_RED_ORE_TORCH_OFF
	        || block == BLOCK_RED_TORCH_UNMOUNTED || block == BLOCK_RED_TORCH_UNMOUNTED_OFF) {
		/* Ground / unmounted variant - attached to y-1 */
		if (ty > 0) { *ax = tx; *ay = ty - 1; *az = tz; return true; }
	}
	
	return false;
}

/* ---- Button helpers ---- */
static cc_bool Redstone_IsButton(BlockID block) {
	return block == BLOCK_BUTTON || block == BLOCK_BUTTON_PRESSED;
}

static cc_bool Redstone_IsButtonPressed(BlockID block) {
	return block == BLOCK_BUTTON_PRESSED;
}

/* ---- Lever helpers ---- */
static cc_bool Redstone_IsLever(BlockID block) {
	return block == BLOCK_LEVER || block == BLOCK_LEVER_ON;
}

static cc_bool Redstone_IsLeverOn(BlockID block) {
	return block == BLOCK_LEVER_ON;
}

/* ---- Pressure plate helpers ---- */
static cc_bool Redstone_IsPressurePlate(BlockID block) {
	return block == BLOCK_PRESSURE_PLATE || block == BLOCK_PRESSURE_PLATE_PRESSED
		|| block == BLOCK_STONE_PLATE || block == BLOCK_STONE_PLATE_PRESSED;
}

static cc_bool Redstone_IsPressurePlatePressed(BlockID block) {
	return block == BLOCK_PRESSURE_PLATE_PRESSED || block == BLOCK_STONE_PLATE_PRESSED;
}

static cc_bool Redstone_IsStonePlate(BlockID block) {
	return block == BLOCK_STONE_PLATE || block == BLOCK_STONE_PLATE_PRESSED;
}

/* Get the attach block position for a button using directional cache */
static cc_bool Redstone_GetButtonAttachBlock(int bx, int by, int bz, int* ax, int* ay, int* az) {
	cc_uint8 facing = DirectionalBlock_GetFacing(bx, by, bz);
	/* facing: 0=attached to z+1, 1=attached to z-1, 2=attached to x+1, 3=attached to x-1 */
	switch (facing) {
		case 0: if (bz < World.MaxZ) { *ax = bx; *ay = by; *az = bz + 1; return true; } break;
		case 1: if (bz > 0)          { *ax = bx; *ay = by; *az = bz - 1; return true; } break;
		case 2: if (bx < World.MaxX) { *ax = bx + 1; *ay = by; *az = bz; return true; } break;
		case 3: if (bx > 0)          { *ax = bx - 1; *ay = by; *az = bz; return true; } break;
	}
	return false;
}

/* Button release queue - buttons auto-release after 1 second (20 ticks at 20Hz) */
#define BUTTON_QUEUE_MAX 64
#define BUTTON_RELEASE_TICKS 20
typedef struct ButtonQueueEntry_ {
	int x, y, z;
	int ticksLeft;
} ButtonQueueEntry;

static ButtonQueueEntry button_queue[BUTTON_QUEUE_MAX];
static int button_queueCount = 0;

static void Button_ScheduleRelease(int x, int y, int z) {
	int i;
	/* Check if already scheduled */
	for (i = 0; i < button_queueCount; i++) {
		if (button_queue[i].x == x && button_queue[i].y == y && button_queue[i].z == z) {
			button_queue[i].ticksLeft = BUTTON_RELEASE_TICKS;
			return;
		}
	}
	if (button_queueCount >= BUTTON_QUEUE_MAX) return;
	button_queue[button_queueCount].x = x;
	button_queue[button_queueCount].y = y;
	button_queue[button_queueCount].z = z;
	button_queue[button_queueCount].ticksLeft = BUTTON_RELEASE_TICKS;
	button_queueCount++;
}

/* Pressure plate release queue - plates auto-release after 1 second (20 ticks at 20Hz) */
#define PLATE_QUEUE_MAX 64
#define PLATE_RELEASE_TICKS 20
typedef struct PlateQueueEntry_ {
	int x, y, z;
	int ticksLeft;
} PlateQueueEntry;

static PlateQueueEntry plate_queue[PLATE_QUEUE_MAX];
static int plate_queueCount = 0;

static void Plate_ScheduleRelease(int x, int y, int z) {
	int i;
	/* Check if already scheduled */
	for (i = 0; i < plate_queueCount; i++) {
		if (plate_queue[i].x == x && plate_queue[i].y == y && plate_queue[i].z == z) {
			plate_queue[i].ticksLeft = PLATE_RELEASE_TICKS;
			return;
		}
	}
	if (plate_queueCount >= PLATE_QUEUE_MAX) return;
	plate_queue[plate_queueCount].x = x;
	plate_queue[plate_queueCount].y = y;
	plate_queue[plate_queueCount].z = z;
	plate_queue[plate_queueCount].ticksLeft = PLATE_RELEASE_TICKS;
	plate_queueCount++;
}

static void Plate_CancelRelease(int x, int y, int z) {
	int i;
	for (i = 0; i < plate_queueCount; i++) {
		if (plate_queue[i].x == x && plate_queue[i].y == y && plate_queue[i].z == z) {
			plate_queueCount--;
			plate_queue[i] = plate_queue[plate_queueCount];
			return;
		}
	}
}

/* BFS queue for redstone power propagation */
typedef struct RedstoneNode_ {
	int x, y, z;
	int power; /* Signal strength 0-15 */
} RedstoneNode;

static RedstoneNode redstone_queue[REDSTONE_MAX_QUEUE];
static int redstone_powerMap[REDSTONE_MAX_QUEUE]; /* Sparse power tracking */
static int redstone_queueHead, redstone_queueTail;
static int redstone_depth = 0; /* Recursion depth guard for power propagation */
static cc_bool redstone_propagating = false; /* Flag to prevent re-entrant propagation during third/fourth pass */

/* Forward declaration - defined later, needed by Redstone_ApplyTorchToggle */
static void Redstone_PropagatePower(int startX, int startY, int startZ);
static void Redstone_PropagateTorchPower(int x, int y, int z);
static void Redstone_EvalNearbyTorches(int cx, int cy, int cz);

/* Get the relative offset to a torch's attach block from its block ID */
/* Works even after the torch block has been removed from the world */
static cc_bool Redstone_GetTorchAttachDir(BlockID block, int* dx, int* dy, int* dz) {
	*dx = 0; *dy = 0; *dz = 0;
	if (block == BLOCK_RED_TORCH_ON_S || block == BLOCK_RED_TORCH_OFF_S) { *dz = 1; return true; }
	if (block == BLOCK_RED_TORCH_ON_N || block == BLOCK_RED_TORCH_OFF_N) { *dz = -1; return true; }
	if (block == BLOCK_RED_TORCH_ON_E || block == BLOCK_RED_TORCH_OFF_E) { *dx = 1; return true; }
	if (block == BLOCK_RED_TORCH_ON_W || block == BLOCK_RED_TORCH_OFF_W) { *dx = -1; return true; }
	if (block == BLOCK_RED_ORE_TORCH || block == BLOCK_RED_ORE_TORCH_OFF) { *dy = -1; return true; }
	if (block == BLOCK_RED_TORCH_UNMOUNTED || block == BLOCK_RED_TORCH_UNMOUNTED_OFF) { *dy = -1; return true; }
	return false;
}

/* Delayed torch toggle queue - torches switch after a 1-tick (1/20s) delay */
#define REDSTONE_TORCH_QUEUE_MAX 256
typedef struct RedstoneTorchEntry_ {
	int x, y, z;
	BlockID targetBlock; /* BLOCK_RED_ORE_TORCH or BLOCK_RED_ORE_TORCH_OFF */
	int ticksLeft;       /* Number of physics ticks until the toggle fires */
} RedstoneTorchEntry;

static RedstoneTorchEntry redstone_torchQueue[REDSTONE_TORCH_QUEUE_MAX];
static int redstone_torchQueueCount = 0;

/* Torch burn-out: prevent rapid oscillation (clock circuits) */
/* Track recent toggles per position; if too many, torch stays OFF */
#define REDSTONE_BURNOUT_MAX 256
#define REDSTONE_BURNOUT_THRESHOLD 8
#define REDSTONE_BURNOUT_WINDOW 100  /* ticks */
typedef struct RedstoneBurnout_ {
	int x, y, z;
	int toggleCount;
	int firstToggleTick;
} RedstoneBurnout;

static RedstoneBurnout redstone_burnouts[REDSTONE_BURNOUT_MAX];
static int redstone_burnoutCount = 0;

static cc_bool Redstone_CheckBurnout(int x, int y, int z) {
	int i;
	for (i = 0; i < redstone_burnoutCount; i++) {
		if (redstone_burnouts[i].x == x && redstone_burnouts[i].y == y && redstone_burnouts[i].z == z) {
			/* Reset if outside the time window */
			if (physics_tickCount - redstone_burnouts[i].firstToggleTick > REDSTONE_BURNOUT_WINDOW) {
				redstone_burnouts[i].toggleCount = 1;
				redstone_burnouts[i].firstToggleTick = physics_tickCount;
				return false;
			}
			redstone_burnouts[i].toggleCount++;
			if (redstone_burnouts[i].toggleCount >= REDSTONE_BURNOUT_THRESHOLD) {
				return true; /* burned out! */
			}
			return false;
		}
	}
	/* New entry */
	if (redstone_burnoutCount < REDSTONE_BURNOUT_MAX) {
		redstone_burnouts[redstone_burnoutCount].x = x;
		redstone_burnouts[redstone_burnoutCount].y = y;
		redstone_burnouts[redstone_burnoutCount].z = z;
		redstone_burnouts[redstone_burnoutCount].toggleCount = 1;
		redstone_burnouts[redstone_burnoutCount].firstToggleTick = physics_tickCount;
		redstone_burnoutCount++;
	}
	return false;
}

/* Reset all redstone state (called on map load) */
static void Redstone_Reset(void) {
	redstone_torchQueueCount = 0;
	redstone_burnoutCount = 0;
	button_queueCount = 0;
	redstone_depth = 0;
	redstone_propagating = false;
	Redstone_AllocVisited();
	IronDoor_ScanWorld();
	TNT_ScanWorld();
	Wheat_ScanWorld();
}

/* Schedule a torch to toggle after a 1-tick delay.
   If a toggle for the same position is already pending, update it instead. */
static void Redstone_ScheduleTorchToggle(int x, int y, int z, BlockID targetBlock) {
	int i;
	/* Check if there's already a pending toggle for this position */
	for (i = 0; i < redstone_torchQueueCount; i++) {
		if (redstone_torchQueue[i].x == x && redstone_torchQueue[i].y == y && redstone_torchQueue[i].z == z) {
			redstone_torchQueue[i].targetBlock = targetBlock;
			redstone_torchQueue[i].ticksLeft = 2;
			return;
		}
	}
	/* Add new entry */
	if (redstone_torchQueueCount >= REDSTONE_TORCH_QUEUE_MAX) return;
	redstone_torchQueue[redstone_torchQueueCount].x = x;
	redstone_torchQueue[redstone_torchQueueCount].y = y;
	redstone_torchQueue[redstone_torchQueueCount].z = z;
	redstone_torchQueue[redstone_torchQueueCount].targetBlock = targetBlock;
	redstone_torchQueue[redstone_torchQueueCount].ticksLeft = 2;
	redstone_torchQueueCount++;
}

/* Propagate power from a torch at (x,y,z) to all dust it powers.
   Torches directly power all 5 adjacent positions except the attach direction.
   They also strongly power the block above, which emits to all 6 sides. */
static void Redstone_PropagateTorchPower(int x, int y, int z) {
	int adx, ady, adz, s;
	BlockID block = World_GetBlock(x, y, z);

	if (!Redstone_GetTorchAttachDir(block, &adx, &ady, &adz)) return;

	/* Direct power: dust in all 5 adjacent positions except attach direction */
	for (s = 0; s < 6; s++) {
		int sx = x + adjOffsets6[s][0];
		int sy = y + adjOffsets6[s][1];
		int sz = z + adjOffsets6[s][2];
		if (adjOffsets6[s][0] == adx && adjOffsets6[s][1] == ady && adjOffsets6[s][2] == adz)
			continue; /* skip attach direction */
		if (World_Contains(sx, sy, sz) && Redstone_IsDust(World_GetBlock(sx, sy, sz)))
			Redstone_PropagatePower(sx, sy, sz);
	}

	/* Strong power: if block above is opaque, propagate to dust on all 6 sides */
	if (World_Contains(x, y + 1, z)) {
		BlockID above = World_GetBlock(x, y + 1, z);
		if (Blocks.Draw[above] == DRAW_OPAQUE) {
			for (s = 0; s < 6; s++) {
				int sx = x + adjOffsets6[s][0];
				int sy = (y + 1) + adjOffsets6[s][1];
				int sz = z + adjOffsets6[s][2];
				if (World_Contains(sx, sy, sz) && Redstone_IsDust(World_GetBlock(sx, sy, sz)))
					Redstone_PropagatePower(sx, sy, sz);
			}
		}
	}
}

/* Apply a single torch toggle and propagate power to adjacent dust */
static void Redstone_ApplyTorchToggle(int x, int y, int z, BlockID targetBlock) {
	Game_UpdateBlock(x, y, z, targetBlock);
	Redstone_PropagateTorchPower(x, y, z);
	Redstone_EvalNearbyTorches(x, y, z);
}

/* Process pending torch toggles - called once per physics tick */
static void Redstone_TickTorchQueue(void) {
	int i, toggledCount = 0;
	int toggledPositions[REDSTONE_TORCH_QUEUE_MAX][3];
	
	/* Phase 1: Decrement ALL timers first (before any processing) */
	/* This prevents new entries added during processing from getting extra decrements */
	for (i = 0; i < redstone_torchQueueCount; i++) {
		redstone_torchQueue[i].ticksLeft--;
	}
	
	/* Phase 2: Process all entries that are ready (ticksLeft <= 0) */
	i = 0;
	while (i < redstone_torchQueueCount) {
		if (redstone_torchQueue[i].ticksLeft <= 0) {
			int tx = redstone_torchQueue[i].x;
			int ty = redstone_torchQueue[i].y;
			int tz = redstone_torchQueue[i].z;
			BlockID target = redstone_torchQueue[i].targetBlock;
			BlockID current = World_GetBlock(tx, ty, tz);
			
			/* Only apply if the block is still a torch (hasn't been removed) */
			if (Redstone_IsTorch(current)) {
				/* Check for burn-out (rapid oscillation / clock circuits) */
				if (Redstone_CheckBurnout(tx, ty, tz)) {
					/* Torch has burned out - force it OFF and stop toggling */
					if (Redstone_IsTorchOn(current)) {
						Game_UpdateBlock(tx, ty, tz, Redstone_GetTorchOffVariant(current));
						Audio_PlayDigSoundRate(SOUND_FIZZ, 80 + (physics_tickCount % 41));
					}
				} else {
					Redstone_ApplyTorchToggle(tx, ty, tz, target);
				}
				/* Record position for post-processing re-evaluation */
				if (toggledCount < REDSTONE_TORCH_QUEUE_MAX) {
					toggledPositions[toggledCount][0] = tx;
					toggledPositions[toggledCount][1] = ty;
					toggledPositions[toggledCount][2] = tz;
					toggledCount++;
				}
			}
			/* Remove entry by swapping with last */
			redstone_torchQueueCount--;
			redstone_torchQueue[i] = redstone_torchQueue[redstone_torchQueueCount];
			/* Don't increment i - recheck swapped-in entry */
		} else {
			i++;
		}
	}
	
	/* Phase 3: After all toggles have settled, re-evaluate torches near toggled positions.
	   This catches cascades missed when earlier toggles evaluated during intermediate state. */
	for (i = 0; i < toggledCount; i++) {
		Redstone_EvalNearbyTorches(toggledPositions[i][0], toggledPositions[i][1], toggledPositions[i][2]);
	}
	/* Also re-propagate dust adjacent to toggled torches, since multiple toggle
	   interactions may have left some dust networks with stale power. */
	for (i = 0; i < toggledCount; i++) {
		Redstone_PropagateTorchPower(
			toggledPositions[i][0], toggledPositions[i][1], toggledPositions[i][2]);
	}
}

/* Propagate power from a pressed button through its attached block to adjacent dust.
   Pressed buttons strongly power the attached block, which emits to all 6 sides. */
static void Redstone_PropagateButtonPower(int bx, int by, int bz) {
	int ax, ay, az, s;

	if (!Redstone_GetButtonAttachBlock(bx, by, bz, &ax, &ay, &az)) return;

	/* Strong power: if attached block is opaque, propagate to dust on all 6 sides */
	if (World_Contains(ax, ay, az)) {
		BlockID attachBlock = World_GetBlock(ax, ay, az);
		if (Blocks.Draw[attachBlock] == DRAW_OPAQUE) {
			for (s = 0; s < 6; s++) {
				int sx = ax + adjOffsets6[s][0];
				int sy = ay + adjOffsets6[s][1];
				int sz = az + adjOffsets6[s][2];
				if (World_Contains(sx, sy, sz) && Redstone_IsDust(World_GetBlock(sx, sy, sz)))
					Redstone_PropagatePower(sx, sy, sz);
			}
		}
	}
}

/* Process pending button releases - called once per physics tick */
static void Redstone_TickButtonQueue(void) {
	int i;
	
	/* Decrement all timers */
	for (i = 0; i < button_queueCount; i++) {
		button_queue[i].ticksLeft--;
	}
	
	/* Process entries that are ready */
	i = 0;
	while (i < button_queueCount) {
		if (button_queue[i].ticksLeft <= 0) {
			int bx = button_queue[i].x;
			int by = button_queue[i].y;
			int bz = button_queue[i].z;
			BlockID current = World_GetBlock(bx, by, bz);
			
			/* Only release if still a pressed button */
			if (Redstone_IsButtonPressed(current)) {
				Game_UpdateBlock(bx, by, bz, BLOCK_BUTTON);
				Audio_PlayDigSound(SOUND_BUTTON_OFF);
				/* Re-propagate power (now de-powered) through attached block */
				Redstone_PropagateButtonPower(bx, by, bz);
				Redstone_EvalNearbyTorches(bx, by, bz);
			}
			/* Remove entry */
			button_queueCount--;
			button_queue[i] = button_queue[button_queueCount];
		} else {
			i++;
		}
	}
}

/* Propagate power from a pressed pressure plate through the block below and to sides.
   Pressed plates strongly power the block directly below them,
   and emit redstone power to all 4 horizontal neighbors at the same level. */
static void Redstone_PropagatePlatePower(int bx, int by, int bz) {
	int ax, ay, az, s;

	/* Block below the pressure plate */
	ax = bx; ay = by - 1; az = bz;

	/* Strong power the block below: if opaque, propagate to dust on all 6 sides */
	if (World_Contains(ax, ay, az)) {
		BlockID belowBlock = World_GetBlock(ax, ay, az);
		if (Blocks.Draw[belowBlock] == DRAW_OPAQUE) {
			for (s = 0; s < 6; s++) {
				int sx = ax + adjOffsets6[s][0];
				int sy = ay + adjOffsets6[s][1];
				int sz = az + adjOffsets6[s][2];
				if (World_Contains(sx, sy, sz) && Redstone_IsDust(World_GetBlock(sx, sy, sz)))
					Redstone_PropagatePower(sx, sy, sz);
			}
		}
	}

	/* Also emit power to all 4 horizontal neighbors at plate level */
	for (s = 0; s < 4; s++) {
		int nx = bx + hOffsets4[s][0];
		int nz = bz + hOffsets4[s][1];
		if (!World_Contains(nx, by, nz)) continue;

		/* Direct dust neighbors */
		if (Redstone_IsDust(World_GetBlock(nx, by, nz))) {
			Redstone_PropagatePower(nx, by, nz);
		}

		/* Strongly power opaque blocks to the side (emit redstone signal from sides) */
		{
			BlockID sideBlock = World_GetBlock(nx, by, nz);
			if (Blocks.Draw[sideBlock] == DRAW_OPAQUE) {
				int t;
				for (t = 0; t < 6; t++) {
					int sx = nx + adjOffsets6[t][0];
					int sy = by + adjOffsets6[t][1];
					int sz = nz + adjOffsets6[t][2];
					if (World_Contains(sx, sy, sz) && Redstone_IsDust(World_GetBlock(sx, sy, sz)))
						Redstone_PropagatePower(sx, sy, sz);
				}
				/* Also evaluate torches attached to the powered side block */
				Redstone_EvalNearbyTorches(nx, by, nz);
			}
		}
	}
}

/* Process pending pressure plate releases */
static void Redstone_TickPlateQueue(void) {
	int i;
	
	for (i = 0; i < plate_queueCount; i++) {
		plate_queue[i].ticksLeft--;
	}
	
	i = 0;
	while (i < plate_queueCount) {
		if (plate_queue[i].ticksLeft <= 0) {
			int bx = plate_queue[i].x;
			int by = plate_queue[i].y;
			int bz = plate_queue[i].z;
			BlockID current = World_GetBlock(bx, by, bz);
			
			if (Redstone_IsPressurePlatePressed(current)) {
				BlockID unpressed = (current == BLOCK_STONE_PLATE_PRESSED) ? BLOCK_STONE_PLATE : BLOCK_PRESSURE_PLATE;
				Game_UpdateBlock(bx, by, bz, unpressed);
				Audio_PlayDigSound(SOUND_BUTTON_OFF);
				Redstone_PropagatePlatePower(bx, by, bz);
				Redstone_EvalNearbyTorches(bx, by, bz);
			}
			plate_queueCount--;
			plate_queue[i] = plate_queue[plate_queueCount];
		} else {
			i++;
		}
	}
}

/* Check all entities and activate/deactivate pressure plates they stand on.
   Approach: when entity steps on unpressed plate, press it and add to release queue.
   Every tick, entities standing on a pressed plate reset its release timer.
   When no entity is on the plate, the timer counts down and releases. */
static void Redstone_TickPressurePlates(void) {
	int i, px, py, pz;
	cc_bool isMobOrPlayer;

	for (i = 0; i < ENTITIES_MAX_COUNT; i++) {
		struct Entity* e = Entities.List[i];
		if (!e) continue;
		
		/* Entity feet position - check the block at their feet level */
		px = (int)Math_Floor(e->Position.x);
		py = (int)Math_Floor(e->Position.y);
		pz = (int)Math_Floor(e->Position.z);
		
		if (!World_Contains(px, py, pz)) continue;
		
		{
			BlockID block = World_GetBlock(px, py, pz);
			
			/* Stone plates only respond to the player and mobs, not items/arrows */
			if (Redstone_IsStonePlate(block)) {
				isMobOrPlayer = (i == ENTITIES_SELF_ID) || Mob_IsMob(i);
				if (!isMobOrPlayer) continue;
			}
			
			if (block == BLOCK_PRESSURE_PLATE || block == BLOCK_STONE_PLATE) {
				/* Press the plate */
				BlockID pressed = (block == BLOCK_STONE_PLATE) ? BLOCK_STONE_PLATE_PRESSED : BLOCK_PRESSURE_PLATE_PRESSED;
				Game_UpdateBlock(px, py, pz, pressed);
				Audio_PlayDigSound(SOUND_BUTTON_ON);
				Redstone_PropagatePlatePower(px, py, pz);
				Redstone_EvalNearbyTorches(px, py, pz);
				/* Schedule release (timer will be reset each tick entity stays) */
				Plate_ScheduleRelease(px, py, pz);
			} else if (block == BLOCK_PRESSURE_PLATE_PRESSED || block == BLOCK_STONE_PLATE_PRESSED) {
				/* Entity still on plate - reset release timer */
				Plate_ScheduleRelease(px, py, pz);
			}
		}
	}
}

/* Temporary per-block power level storage for BFS propagation */
/* We use a simple approach: scan reachable dust from sources */

/* Check if a position has connected redstone dust (including pour-over) */
/* Adds neighbors to BFS queue with reduced power */
static void Redstone_AddDustNeighbor(int nx, int ny, int nz, int power) {
	BlockID block;
	if (!World_Contains(nx, ny, nz)) return;
	if (power <= 0) return;
	
	block = World_GetBlock(nx, ny, nz);
	if (Redstone_IsDust(block)) {
		/* Direct horizontal connection */
		if (redstone_queueTail < REDSTONE_MAX_QUEUE) {
			redstone_queue[redstone_queueTail].x = nx;
			redstone_queue[redstone_queueTail].y = ny;
			redstone_queue[redstone_queueTail].z = nz;
			redstone_queue[redstone_queueTail].power = power;
			redstone_queueTail++;
		}
		return;
	}
	
	/* Pour-over: check one block down (dust going over an edge) */
	if (block == BLOCK_AIR && ny > 0) {
		BlockID below = World_GetBlock(nx, ny - 1, nz);
		if (Redstone_IsDust(below)) {
			if (redstone_queueTail < REDSTONE_MAX_QUEUE) {
				redstone_queue[redstone_queueTail].x = nx;
				redstone_queue[redstone_queueTail].y = ny - 1;
				redstone_queue[redstone_queueTail].z = nz;
				redstone_queue[redstone_queueTail].power = power;
				redstone_queueTail++;
			}
		}
	}
	
	/* Pour-over: check one block up (dust climbing a wall) */
	/* Only if the block above the current dust position is air (not blocked) */
	if (Blocks.Draw[block] == DRAW_OPAQUE && ny < World.MaxY) {
		BlockID above = World_GetBlock(nx, ny + 1, nz);
		if (Redstone_IsDust(above)) {
			if (redstone_queueTail < REDSTONE_MAX_QUEUE) {
				redstone_queue[redstone_queueTail].x = nx;
				redstone_queue[redstone_queueTail].y = ny + 1;
				redstone_queue[redstone_queueTail].z = nz;
				redstone_queue[redstone_queueTail].power = power;
				redstone_queueTail++;
			}
		}
	}
}

/* Check if a specific block position receives redstone power from any source */
/* Used to determine if a torch's attached block is powered */
/* Lit redstone dust powers adjacent opaque blocks */
/* An ON torch powers the opaque block directly above it */
static cc_bool Redstone_BlockReceivesPower(int bx, int by, int bz) {
	int i;
	/* Lit dust directly on TOP of the block always powers it */
	if (World_Contains(bx, by + 1, bz) && World_GetBlock(bx, by + 1, bz) == BLOCK_LIT_RED_ORE_DUST)
		return true;

	/* Lit dust on the same Y to a horizontal side only powers if the dust connects toward this block */
	for (i = 0; i < 4; i++) {
		int dx = bx + hOffsets4[i][0];
		int dz = bz + hOffsets4[i][1];
		BlockID neighbor;

		if (!World_Contains(dx, by, dz)) continue;
		neighbor = World_GetBlock(dx, by, dz);

		if (neighbor == BLOCK_LIT_RED_ORE_DUST) {
			/* Direction FROM dust TOWARD the block being tested */
			int towardX = -hOffsets4[i][0];
			int towardZ = -hOffsets4[i][1];
			if (Redstone_DustConnectsToward(dx, by, dz, towardX, towardZ))
				return true;
		}
	}
	
	/* Any ON torch directly below powers the block above it */
	if (World_Contains(bx, by - 1, bz)) {
		BlockID below = World_GetBlock(bx, by - 1, bz);
		if (Redstone_IsTorchOn(below)) {
			/* But NOT if this torch is attached TO this block (feedback loop) */
			int tax, tay, taz;
			if (Redstone_GetTorchAttachBlock(bx, by - 1, bz, &tax, &tay, &taz)) {
				if (tax == bx && tay == by && taz == bz)
					return false; /* skip - this is the attach block */
			}
			return true;
		}
	}
	
	/* A pressed button attached to this block powers it */
	{
		int s;
		for (s = 0; s < 6; s++) {
			int nx = bx + adjOffsets6[s][0];
			int ny = by + adjOffsets6[s][1];
			int nz = bz + adjOffsets6[s][2];
			if (World_Contains(nx, ny, nz)) {
				BlockID neighbor = World_GetBlock(nx, ny, nz);
				if (Redstone_IsButtonPressed(neighbor)) {
					int abx, aby, abz;
					if (Redstone_GetButtonAttachBlock(nx, ny, nz, &abx, &aby, &abz)) {
						if (abx == bx && aby == by && abz == bz)
							return true;
					}
				}
				/* A lever ON attached to this block powers it */
				if (Redstone_IsLeverOn(neighbor)) {
					int abx, aby, abz;
					if (Redstone_GetButtonAttachBlock(nx, ny, nz, &abx, &aby, &abz)) {
						if (abx == bx && aby == by && abz == bz)
							return true;
					}
				}
			}
		}
	}

	/* A pressed pressure plate directly above this block powers it */
	if (World_Contains(bx, by + 1, bz)) {
		BlockID above = World_GetBlock(bx, by + 1, bz);
		if (Redstone_IsPressurePlatePressed(above))
			return true;
	}

	/* A pressed pressure plate horizontally adjacent at the same level powers this block */
	for (i = 0; i < 4; i++) {
		int px = bx + hOffsets4[i][0];
		int pz = bz + hOffsets4[i][1];
		if (World_Contains(px, by, pz)) {
			BlockID plateNeighbor = World_GetBlock(px, by, pz);
			if (Redstone_IsPressurePlatePressed(plateNeighbor))
				return true;
		}
	}
	
	return false;
}

/* Check if an opaque block is "strongly" powered (only by redstone torches).
   Strong power passes through blocks to adjacent dust. Unlike BlockReceivesPower,
   this ignores dust (weak power) to prevent feedback loops during propagation.
   In Minecraft, dust only weakly powers blocks - weak power affects torch state
   but does NOT re-emit to other dust through opaque blocks. */
static cc_bool Redstone_BlockStronglyPowered(int bx, int by, int bz) {
	/* Any ON torch directly below strongly powers the block above it */
	if (World_Contains(bx, by - 1, bz)) {
		BlockID below = World_GetBlock(bx, by - 1, bz);
		if (Redstone_IsTorchOn(below)) {
			/* But NOT if this torch is attached TO this block (feedback loop) */
			int tax, tay, taz;
			if (Redstone_GetTorchAttachBlock(bx, by - 1, bz, &tax, &tay, &taz)) {
				if (tax == bx && tay == by && taz == bz)
					return false;
			}
			return true;
		}
	}
	
	/* A pressed button attached to this block strongly powers it */
	{
		int s;
		for (s = 0; s < 6; s++) {
			int nx = bx + adjOffsets6[s][0];
			int ny = by + adjOffsets6[s][1];
			int nz = bz + adjOffsets6[s][2];
			if (World_Contains(nx, ny, nz)) {
				BlockID neighbor = World_GetBlock(nx, ny, nz);
				if (Redstone_IsButtonPressed(neighbor)) {
					int abx, aby, abz;
					if (Redstone_GetButtonAttachBlock(nx, ny, nz, &abx, &aby, &abz)) {
						if (abx == bx && aby == by && abz == bz)
							return true;
					}
				}
				/* A lever ON attached to this block strongly powers it */
				if (Redstone_IsLeverOn(neighbor)) {
					int abx, aby, abz;
					if (Redstone_GetButtonAttachBlock(nx, ny, nz, &abx, &aby, &abz)) {
						if (abx == bx && aby == by && abz == bz)
							return true;
					}
				}
			}
		}
	}
	
	/* A pressed pressure plate directly above strongly powers this block */
	if (World_Contains(bx, by + 1, bz)) {
		BlockID above = World_GetBlock(bx, by + 1, bz);
		if (Redstone_IsPressurePlatePressed(above))
			return true;
	}
	
	return false;
}

/* Propagate redstone power through the world from all power sources */
/* This recalculates the entire connected redstone network */
static void Redstone_PropagatePower(int startX, int startY, int startZ) {
	int i, x, y, z, dustCount = 0;
	BlockID block, startBlock;

	/* Recursion depth guard to prevent infinite loops from torch cascading */
	if (redstone_depth >= 3) return;
	redstone_depth++;

	startBlock = World_GetBlock(startX, startY, startZ);

	/* Also check if the start position is near a torch */
	if (Redstone_IsTorch(startBlock)) {
		Redstone_PropagateTorchPower(startX, startY, startZ);
		redstone_depth--;
		return;
	}

	if (!Redstone_IsDust(startBlock)) { redstone_depth--; return; }
	if (!pp_visitedBits) { redstone_depth--; return; }

	/* First pass: BFS to find all connected dust using bitset for O(1) visited checks */
	redstone_queueHead = 0;
	redstone_queueTail = 0;
	pp_visitedCount = 0;

	{
		int idx = World_Pack(startX, startY, startZ);
		redstone_queue[redstone_queueTail].x = startX;
		redstone_queue[redstone_queueTail].y = startY;
		redstone_queue[redstone_queueTail].z = startZ;
		redstone_queue[redstone_queueTail].power = 0;
		redstone_queueTail++;
		PP_VISITED_SET(idx);
		pp_visitedPositions[pp_visitedCount++] = idx;
	}

	while (redstone_queueHead < redstone_queueTail) {
		x = redstone_queue[redstone_queueHead].x;
		y = redstone_queue[redstone_queueHead].y;
		z = redstone_queue[redstone_queueHead].z;
		redstone_queueHead++;

		block = World_GetBlock(x, y, z);
		if (!Redstone_IsDust(block)) continue;
		if (dustCount >= REDSTONE_MAX_QUEUE) break;

		pp_dustPositions[dustCount][0] = x;
		pp_dustPositions[dustCount][1] = y;
		pp_dustPositions[dustCount][2] = z;
		pp_dustPower[dustCount] = 0;
		dustCount++;

		/* Add all connected positions using Redstone_AddDustNeighbor-style checks */
		/* but with O(1) visited bitset instead of O(n) linear scan */
		{
			int d;
			for (d = 0; d < 4; d++) {
				int nx2 = x + hOffsets4[d][0];
				int nz2 = z + hOffsets4[d][1];
				BlockID nb;
				int nIdx;
				if (!World_Contains(nx2, y, nz2)) continue;

				nIdx = World_Pack(nx2, y, nz2);
				nb = World_GetBlock(nx2, y, nz2);

				if (Redstone_IsDust(nb)) {
					if (!PP_VISITED_GET(nIdx) && redstone_queueTail < REDSTONE_MAX_QUEUE) {
						PP_VISITED_SET(nIdx);
						if (pp_visitedCount < REDSTONE_MAX_QUEUE) pp_visitedPositions[pp_visitedCount++] = nIdx;
						redstone_queue[redstone_queueTail].x = nx2;
						redstone_queue[redstone_queueTail].y = y;
						redstone_queue[redstone_queueTail].z = nz2;
						redstone_queue[redstone_queueTail].power = 0;
						redstone_queueTail++;
					}
				} else if (nb == BLOCK_AIR && y > 0) {
					/* Pour-over down */
					int belowIdx = World_Pack(nx2, y - 1, nz2);
					if (Redstone_IsDust(World_GetBlock(nx2, y - 1, nz2))) {
						if (!PP_VISITED_GET(belowIdx) && redstone_queueTail < REDSTONE_MAX_QUEUE) {
							PP_VISITED_SET(belowIdx);
							if (pp_visitedCount < REDSTONE_MAX_QUEUE) pp_visitedPositions[pp_visitedCount++] = belowIdx;
							redstone_queue[redstone_queueTail].x = nx2;
							redstone_queue[redstone_queueTail].y = y - 1;
							redstone_queue[redstone_queueTail].z = nz2;
							redstone_queue[redstone_queueTail].power = 0;
							redstone_queueTail++;
						}
					}
				}
				if (Blocks.Draw[nb] == DRAW_OPAQUE && y < World.MaxY) {
					/* Pour-over up */
					int aboveIdx = World_Pack(nx2, y + 1, nz2);
					if (Redstone_IsDust(World_GetBlock(nx2, y + 1, nz2))) {
						if (!PP_VISITED_GET(aboveIdx) && redstone_queueTail < REDSTONE_MAX_QUEUE) {
							PP_VISITED_SET(aboveIdx);
							if (pp_visitedCount < REDSTONE_MAX_QUEUE) pp_visitedPositions[pp_visitedCount++] = aboveIdx;
							redstone_queue[redstone_queueTail].x = nx2;
							redstone_queue[redstone_queueTail].y = y + 1;
							redstone_queue[redstone_queueTail].z = nz2;
							redstone_queue[redstone_queueTail].power = 0;
							redstone_queueTail++;
						}
					}
				}
			}
		}
	}

	PP_ClearVisited();

	if (dustCount == 0) { redstone_depth--; return; }

	/* Second pass: find power sources for each dust position */
	for (i = 0; i < dustCount; i++) {
		int j;
		x = pp_dustPositions[i][0];
		y = pp_dustPositions[i][1];
		z = pp_dustPositions[i][2];

		/* Check all 6 adjacent positions for power sources */
		for (j = 0; j < 6; j++) {
			int tx = x + adjOffsets6[j][0];
			int ty = y + adjOffsets6[j][1];
			int tz = z + adjOffsets6[j][2];
			BlockID tBlock;
			int adx, ady, adz;

			if (!World_Contains(tx, ty, tz)) continue;
			tBlock = World_GetBlock(tx, ty, tz);

			/* Direct torch power */
			if (Redstone_IsTorchOn(tBlock)) {
				if (Redstone_GetTorchAttachDir(tBlock, &adx, &ady, &adz)) {
					int dirX = -adjOffsets6[j][0];
					int dirY = -adjOffsets6[j][1];
					int dirZ = -adjOffsets6[j][2];
					if (dirX != adx || dirY != ady || dirZ != adz) {
						pp_dustPower[i] = REDSTONE_MAX_POWER;
						continue;
					}
				}
			}

			/* Strong power through opaque blocks */
			if (Blocks.Draw[tBlock] == DRAW_OPAQUE && Redstone_BlockStronglyPowered(tx, ty, tz)) {
				pp_dustPower[i] = REDSTONE_MAX_POWER;
			}

			if (Redstone_IsPressurePlatePressed(tBlock)) {
				pp_dustPower[i] = REDSTONE_MAX_POWER;
			}

			if (Redstone_IsButtonPressed(tBlock)) {
				pp_dustPower[i] = REDSTONE_MAX_POWER;
			}

			if (Redstone_IsLeverOn(tBlock)) {
				pp_dustPower[i] = REDSTONE_MAX_POWER;
			}
		}
	}

	/* Iterative power decay: propagate power from sources through connected dust */
	/* Uses convergent relaxation to correctly handle complex multi-source networks */
	{
		int changed = 1;
		int iter;
		for (iter = 0; iter < REDSTONE_MAX_POWER && changed; iter++) {
			changed = 0;
			for (i = 0; i < dustCount; i++) {
				int j;
				if (pp_dustPower[i] <= 1) continue;

				x = pp_dustPositions[i][0];
				y = pp_dustPositions[i][1];
				z = pp_dustPositions[i][2];

				for (j = 0; j < dustCount; j++) {
					int jx, jy, jz, dist;
					cc_bool connected = false;
					if (i == j) continue;

					jx = pp_dustPositions[j][0];
					jy = pp_dustPositions[j][1];
					jz = pp_dustPositions[j][2];

					dist = Math_AbsI(x - jx) + Math_AbsI(y - jy) + Math_AbsI(z - jz);

					if (dist == 1) {
						connected = true;
					} else if (dist == 2 && Math_AbsI(y - jy) == 1) {
						if (jy < y) {
							if (World_Contains(jx, y, jz) && World_GetBlock(jx, y, jz) == BLOCK_AIR)
								connected = true;
						} else {
							if (World_Contains(x, jy, z) && World_GetBlock(x, jy, z) == BLOCK_AIR)
								connected = true;
						}
					}

					if (connected && pp_dustPower[j] < pp_dustPower[i] - 1) {
						pp_dustPower[j] = pp_dustPower[i] - 1;
						changed = 1;
					}
				}
			}
		}
	}

	/* Update pass: set all dust blocks based on calculated power */
	{
		cc_bool wasPropagating = redstone_propagating;
		redstone_propagating = true;

		for (i = 0; i < dustCount; i++) {
			x = pp_dustPositions[i][0];
			y = pp_dustPositions[i][1];
			z = pp_dustPositions[i][2];
			block = World_GetBlock(x, y, z);

			if (pp_dustPower[i] > 0) {
				if (block != BLOCK_LIT_RED_ORE_DUST) {
					Game_UpdateBlock(x, y, z, BLOCK_LIT_RED_ORE_DUST);
				}
			} else {
				if (block != BLOCK_RED_ORE_DUST) {
					Game_UpdateBlock(x, y, z, BLOCK_RED_ORE_DUST);
				}
			}
		}

		redstone_propagating = wasPropagating;
	}

	/* Torch update pass: find opaque blocks adjacent to dust, update attached torches */
	/* Uses visited bitset for O(1) dedup instead of O(n) linear scan */
	{
		int opaquePositions[2048][3];
		int opaqueCount = 0;

		for (i = 0; i < dustCount; i++) {
			int j;
			x = pp_dustPositions[i][0];
			y = pp_dustPositions[i][1];
			z = pp_dustPositions[i][2];

			for (j = 0; j < 6; j++) {
				int ox = x + adjOffsets6[j][0];
				int oy = y + adjOffsets6[j][1];
				int oz = z + adjOffsets6[j][2];
				BlockID oBlock;
				int oIdx;

				if (!World_Contains(ox, oy, oz)) continue;
				oBlock = World_GetBlock(ox, oy, oz);
				if (Blocks.Draw[oBlock] != DRAW_OPAQUE) continue;

				oIdx = World_Pack(ox, oy, oz);
				if (PP_VISITED_GET(oIdx)) continue;
				PP_VISITED_SET(oIdx);
				if (pp_visitedCount < REDSTONE_MAX_QUEUE) pp_visitedPositions[pp_visitedCount++] = oIdx;

				if (opaqueCount >= 2048) continue;
				opaquePositions[opaqueCount][0] = ox;
				opaquePositions[opaqueCount][1] = oy;
				opaquePositions[opaqueCount][2] = oz;
				opaqueCount++;
			}
		}

		PP_ClearVisited();

		for (i = 0; i < opaqueCount; i++) {
			int j;
			int ox = opaquePositions[i][0];
			int oy = opaquePositions[i][1];
			int oz = opaquePositions[i][2];
			cc_bool opaquePowered = Redstone_BlockReceivesPower(ox, oy, oz);

			for (j = 0; j < 6; j++) {
				int tx = ox + adjOffsets6[j][0];
				int ty = oy + adjOffsets6[j][1];
				int tz = oz + adjOffsets6[j][2];
				int ax, ay, az;
				BlockID tBlock;

				if (!World_Contains(tx, ty, tz)) continue;
				tBlock = World_GetBlock(tx, ty, tz);

				if (!Redstone_IsTorch(tBlock)) continue;

				if (!Redstone_GetTorchAttachBlock(tx, ty, tz, &ax, &ay, &az)) continue;
				if (ax != ox || ay != oy || az != oz) continue;

				if (opaquePowered && Redstone_IsTorchOn(tBlock)) {
					Redstone_ScheduleTorchToggle(tx, ty, tz, Redstone_GetTorchOffVariant(tBlock));
				} else if (!opaquePowered && Redstone_IsTorchOff(tBlock)) {
					Redstone_ScheduleTorchToggle(tx, ty, tz, Redstone_GetTorchOnVariant(tBlock));
				}
			}
		}
	}

	redstone_depth--;
}

/* After dust propagation completes, explicitly check nearby torches
   for state changes. This is a safety net for cases where the fourth
   pass inside Redstone_PropagatePower might not fire at the right depth. */
static void Redstone_EvalNearbyTorches(int cx, int cy, int cz) {
	/* Scan 2-block radius for opaque blocks, then check their adjacent torches */
	int d, t;

	for (d = 0; d < 6; d++) {
		int ox = cx + adjOffsets6[d][0];
		int oy = cy + adjOffsets6[d][1];
		int oz = cz + adjOffsets6[d][2];

		if (!World_Contains(ox, oy, oz)) continue;
		if (Blocks.Draw[World_GetBlock(ox, oy, oz)] != DRAW_OPAQUE) continue;

		/* Found an opaque block - check all 6 neighbors for torches */
		{
			cc_bool powered = Redstone_BlockReceivesPower(ox, oy, oz);
			for (t = 0; t < 6; t++) {
				int tx = ox + adjOffsets6[t][0];
				int ty = oy + adjOffsets6[t][1];
				int tz = oz + adjOffsets6[t][2];
				int tax, tay, taz;
				BlockID tBlock;
				
				if (!World_Contains(tx, ty, tz)) continue;
				tBlock = World_GetBlock(tx, ty, tz);
				if (!Redstone_IsTorch(tBlock)) continue;
				if (!Redstone_GetTorchAttachBlock(tx, ty, tz, &tax, &tay, &taz)) continue;
				if (tax != ox || tay != oy || taz != oz) continue;
				
				if (powered && Redstone_IsTorchOn(tBlock)) {
					Redstone_ScheduleTorchToggle(tx, ty, tz, Redstone_GetTorchOffVariant(tBlock));
				} else if (!powered && Redstone_IsTorchOff(tBlock)) {
					Redstone_ScheduleTorchToggle(tx, ty, tz, Redstone_GetTorchOnVariant(tBlock));
				}
			}
		}
	}
}

static void Physics_HandleRedOreDust(int index, BlockID block) {
	BlockID below;
	int x, y, z;
	
	World_Unpack(index, x, y, z);
	
	/* Check if block below is solid */
	if (y > 0) {
		below = World_GetBlock(x, y - 1, z);
		if (Blocks.Draw[below] == DRAW_OPAQUE) {
			/* Still has support - recalculate power (unless parent propagation is already handling it) */
			if (!redstone_propagating) Redstone_PropagatePower(x, y, z);
			return;
		}
	}
	
	/* No solid block below - break the red ore dust */
	Physics_DropBlock(x, y, z, BLOCK_RED_ORE_DUST);
	Game_UpdateBlock(x, y, z, BLOCK_AIR);
	Physics_ActivateNeighbours(x, y, z, index);
}

static void Physics_HandleLitRedOreDust(int index, BlockID block) {
	BlockID below;
	int x, y, z;
	
	World_Unpack(index, x, y, z);
	
	/* Check if block below is solid */
	if (y > 0) {
		below = World_GetBlock(x, y - 1, z);
		if (Blocks.Draw[below] == DRAW_OPAQUE) {
			/* Still has support - recalculate power (unless parent propagation is already handling it) */
			if (!redstone_propagating) Redstone_PropagatePower(x, y, z);
			return;
		}
	}
	
	/* No solid block below - break the lit red ore dust */
	Physics_DropBlock(x, y, z, BLOCK_RED_ORE_DUST);
	Game_UpdateBlock(x, y, z, BLOCK_AIR);
	Physics_ActivateNeighbours(x, y, z, index);
}

static void Physics_PlaceRedOreDust(int index, BlockID block) {
	int x, y, z;
	BlockID neighbor;
	World_Unpack(index, x, y, z);
	
	/* Trigger visual updates on neighboring redstone dust blocks */
	/* This causes their textures to recalculate based on new connections */
	if (x > 0) {
		neighbor = World_GetBlock(x - 1, y, z);
		if (Redstone_IsDust(neighbor))
			Game_UpdateBlock(x - 1, y, z, neighbor);
	}
	if (x < World.MaxX) {
		neighbor = World_GetBlock(x + 1, y, z);
		if (Redstone_IsDust(neighbor))
			Game_UpdateBlock(x + 1, y, z, neighbor);
	}
	if (z > 0) {
		neighbor = World_GetBlock(x, y, z - 1);
		if (Redstone_IsDust(neighbor))
			Game_UpdateBlock(x, y, z - 1, neighbor);
	}
	if (z < World.MaxZ) {
		neighbor = World_GetBlock(x, y, z + 1);
		if (Redstone_IsDust(neighbor))
			Game_UpdateBlock(x, y, z + 1, neighbor);
	}
	
	/* Pour-over visual updates */
	if (y > 0) {
		if (x > 0 && World_GetBlock(x - 1, y, z) == BLOCK_AIR) {
			neighbor = World_GetBlock(x - 1, y - 1, z);
			if (Redstone_IsDust(neighbor)) Game_UpdateBlock(x - 1, y - 1, z, neighbor);
		}
		if (x < World.MaxX && World_GetBlock(x + 1, y, z) == BLOCK_AIR) {
			neighbor = World_GetBlock(x + 1, y - 1, z);
			if (Redstone_IsDust(neighbor)) Game_UpdateBlock(x + 1, y - 1, z, neighbor);
		}
		if (z > 0 && World_GetBlock(x, y, z - 1) == BLOCK_AIR) {
			neighbor = World_GetBlock(x, y - 1, z - 1);
			if (Redstone_IsDust(neighbor)) Game_UpdateBlock(x, y - 1, z - 1, neighbor);
		}
		if (z < World.MaxZ && World_GetBlock(x, y, z + 1) == BLOCK_AIR) {
			neighbor = World_GetBlock(x, y - 1, z + 1);
			if (Redstone_IsDust(neighbor)) Game_UpdateBlock(x, y - 1, z + 1, neighbor);
		}
	}
	if (y < World.MaxY) {
		if (x > 0) {
			neighbor = World_GetBlock(x - 1, y + 1, z);
			if (Redstone_IsDust(neighbor)) Game_UpdateBlock(x - 1, y + 1, z, neighbor);
		}
		if (x < World.MaxX) {
			neighbor = World_GetBlock(x + 1, y + 1, z);
			if (Redstone_IsDust(neighbor)) Game_UpdateBlock(x + 1, y + 1, z, neighbor);
		}
		if (z > 0) {
			neighbor = World_GetBlock(x, y + 1, z - 1);
			if (Redstone_IsDust(neighbor)) Game_UpdateBlock(x, y + 1, z - 1, neighbor);
		}
		if (z < World.MaxZ) {
			neighbor = World_GetBlock(x, y + 1, z + 1);
			if (Redstone_IsDust(neighbor)) Game_UpdateBlock(x, y + 1, z + 1, neighbor);
		}
	}
	
	/* Propagate redstone power through the network */
	if (!redstone_propagating) Redstone_PropagatePower(x, y, z);
}

static void Physics_DeleteRedOreDust(int index, BlockID block) {
	int x, y, z;
	BlockID neighbor;
	World_Unpack(index, x, y, z);
	
	/* Trigger visual updates on neighboring redstone dust blocks */
	if (x > 0) {
		neighbor = World_GetBlock(x - 1, y, z);
		if (Redstone_IsDust(neighbor))
			Game_UpdateBlock(x - 1, y, z, neighbor);
	}
	if (x < World.MaxX) {
		neighbor = World_GetBlock(x + 1, y, z);
		if (Redstone_IsDust(neighbor))
			Game_UpdateBlock(x + 1, y, z, neighbor);
	}
	if (z > 0) {
		neighbor = World_GetBlock(x, y, z - 1);
		if (Redstone_IsDust(neighbor))
			Game_UpdateBlock(x, y, z - 1, neighbor);
	}
	if (z < World.MaxZ) {
		neighbor = World_GetBlock(x, y, z + 1);
		if (Redstone_IsDust(neighbor))
			Game_UpdateBlock(x, y, z + 1, neighbor);
	}
	
	/* Pour-over visual updates */
	if (y > 0) {
		if (x > 0 && World_GetBlock(x - 1, y, z) == BLOCK_AIR) {
			neighbor = World_GetBlock(x - 1, y - 1, z);
			if (Redstone_IsDust(neighbor)) Game_UpdateBlock(x - 1, y - 1, z, neighbor);
		}
		if (x < World.MaxX && World_GetBlock(x + 1, y, z) == BLOCK_AIR) {
			neighbor = World_GetBlock(x + 1, y - 1, z);
			if (Redstone_IsDust(neighbor)) Game_UpdateBlock(x + 1, y - 1, z, neighbor);
		}
		if (z > 0 && World_GetBlock(x, y, z - 1) == BLOCK_AIR) {
			neighbor = World_GetBlock(x, y - 1, z - 1);
			if (Redstone_IsDust(neighbor)) Game_UpdateBlock(x, y - 1, z - 1, neighbor);
		}
		if (z < World.MaxZ && World_GetBlock(x, y, z + 1) == BLOCK_AIR) {
			neighbor = World_GetBlock(x, y - 1, z + 1);
			if (Redstone_IsDust(neighbor)) Game_UpdateBlock(x, y - 1, z + 1, neighbor);
		}
	}
	if (y < World.MaxY) {
		if (x > 0) {
			neighbor = World_GetBlock(x - 1, y + 1, z);
			if (Redstone_IsDust(neighbor)) Game_UpdateBlock(x - 1, y + 1, z, neighbor);
		}
		if (x < World.MaxX) {
			neighbor = World_GetBlock(x + 1, y + 1, z);
			if (Redstone_IsDust(neighbor)) Game_UpdateBlock(x + 1, y + 1, z, neighbor);
		}
		if (z > 0) {
			neighbor = World_GetBlock(x, y + 1, z - 1);
			if (Redstone_IsDust(neighbor)) Game_UpdateBlock(x, y + 1, z - 1, neighbor);
		}
		if (z < World.MaxZ) {
			neighbor = World_GetBlock(x, y + 1, z + 1);
			if (Redstone_IsDust(neighbor)) Game_UpdateBlock(x, y + 1, z + 1, neighbor);
		}
	}
	
	/* Recalculate power for the network (the removed dust may have been carrying power) */
	/* Find any adjacent dust and propagate from there (including pour-over connections) */
	if (!redstone_propagating) {
	if (x > 0 && Redstone_IsDust(World_GetBlock(x - 1, y, z)))
		Redstone_PropagatePower(x - 1, y, z);
	if (x < World.MaxX && Redstone_IsDust(World_GetBlock(x + 1, y, z)))
		Redstone_PropagatePower(x + 1, y, z);
	if (z > 0 && Redstone_IsDust(World_GetBlock(x, y, z - 1)))
		Redstone_PropagatePower(x, y, z - 1);
	if (z < World.MaxZ && Redstone_IsDust(World_GetBlock(x, y, z + 1)))
		Redstone_PropagatePower(x, y, z + 1);
	
	/* Pour-over: check dust below via air (edge drop-off) */
	if (y > 0) {
		if (x > 0 && World_GetBlock(x - 1, y, z) == BLOCK_AIR && Redstone_IsDust(World_GetBlock(x - 1, y - 1, z)))
			Redstone_PropagatePower(x - 1, y - 1, z);
		if (x < World.MaxX && World_GetBlock(x + 1, y, z) == BLOCK_AIR && Redstone_IsDust(World_GetBlock(x + 1, y - 1, z)))
			Redstone_PropagatePower(x + 1, y - 1, z);
		if (z > 0 && World_GetBlock(x, y, z - 1) == BLOCK_AIR && Redstone_IsDust(World_GetBlock(x, y - 1, z - 1)))
			Redstone_PropagatePower(x, y - 1, z - 1);
		if (z < World.MaxZ && World_GetBlock(x, y, z + 1) == BLOCK_AIR && Redstone_IsDust(World_GetBlock(x, y - 1, z + 1)))
			Redstone_PropagatePower(x, y - 1, z + 1);
	}
	/* Pour-over: check dust above via opaque block (climbing wall) */
	if (y < World.MaxY) {
		if (x > 0 && Blocks.Draw[World_GetBlock(x - 1, y, z)] == DRAW_OPAQUE && Redstone_IsDust(World_GetBlock(x - 1, y + 1, z)))
			Redstone_PropagatePower(x - 1, y + 1, z);
		if (x < World.MaxX && Blocks.Draw[World_GetBlock(x + 1, y, z)] == DRAW_OPAQUE && Redstone_IsDust(World_GetBlock(x + 1, y + 1, z)))
			Redstone_PropagatePower(x + 1, y + 1, z);
		if (z > 0 && Blocks.Draw[World_GetBlock(x, y, z - 1)] == DRAW_OPAQUE && Redstone_IsDust(World_GetBlock(x, y + 1, z - 1)))
			Redstone_PropagatePower(x, y + 1, z - 1);
		if (z < World.MaxZ && Blocks.Draw[World_GetBlock(x, y, z + 1)] == DRAW_OPAQUE && Redstone_IsDust(World_GetBlock(x, y + 1, z + 1)))
			Redstone_PropagatePower(x, y + 1, z + 1);
	}
	} /* end !redstone_propagating */
	
	/* Safety net: explicitly check torches near the break point */
	Redstone_EvalNearbyTorches(x, y, z);
}

/* ---- Button physics handlers ---- */

/* Handle button placement (unpressed variant placed by player) */
static void Physics_PlaceButton(int index, BlockID block) {
	/* Nothing special for unpressed button placement - just exists on wall */
	(void)index; (void)block;
}

/* Handle pressed button placement (triggered by right-click converting unpressed→pressed) */
static void Physics_PlaceButtonPressed(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);
	
	/* Schedule auto-release after 2 seconds */
	Button_ScheduleRelease(x, y, z);
	
	/* Propagate power through attached block */
	Redstone_PropagateButtonPower(x, y, z);
	
	/* Evaluate torches near the button (pressed button powers attached block, may affect torches) */
	Redstone_EvalNearbyTorches(x, y, z);
}

/* Handle button deletion (player breaks it) */
static void Physics_DeleteButton(int index, BlockID block) {
	int x, y, z, i;
	World_Unpack(index, x, y, z);
	
	/* Remove from release queue if scheduled */
	for (i = 0; i < button_queueCount; i++) {
		if (button_queue[i].x == x && button_queue[i].y == y && button_queue[i].z == z) {
			button_queueCount--;
			button_queue[i] = button_queue[button_queueCount];
			break;
		}
	}
	
	/* If it was pressed, need to de-power the attached block */
	if (Redstone_IsButtonPressed(block)) {
		Redstone_PropagateButtonPower(x, y, z);
		Redstone_EvalNearbyTorches(x, y, z);
	}
}

/* Handle button activation (called when neighbor changes) - support check */
static void Physics_HandleButton(int index, BlockID block) {
	int x, y, z;
	int ax, ay, az;
	World_Unpack(index, x, y, z);
	
	/* Check if button still has support from attached block */
	if (Redstone_GetButtonAttachBlock(x, y, z, &ax, &ay, &az)) {
		if (!World_Contains(ax, ay, az) || Blocks.Draw[World_GetBlock(ax, ay, az)] != DRAW_OPAQUE) {
			/* Support gone - delete button */
			Physics_DropBlock(x, y, z, BLOCK_BUTTON);
			Game_UpdateBlock(x, y, z, BLOCK_AIR);
			Physics_ActivateNeighbours(x, y, z, index);
			return;
		}
	} else {
		/* Couldn't determine attach block - delete for safety */
		Physics_DropBlock(x, y, z, BLOCK_BUTTON);
		Game_UpdateBlock(x, y, z, BLOCK_AIR);
		Physics_ActivateNeighbours(x, y, z, index);
		return;
	}
}

/* ---- Lever physics handlers ---- */

/* Handle lever placement (off variant placed by player, or toggled off) */
static void Physics_PlaceLever(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);
	
	/* Re-propagate to depower the attached block when turned off */
	Redstone_PropagateButtonPower(x, y, z);
	Redstone_EvalNearbyTorches(x, y, z);
}

/* Handle lever ON placement (toggled by right-click) */
static void Physics_PlaceLeverOn(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);
	
	/* Propagate power through attached block */
	Redstone_PropagateButtonPower(x, y, z);
	Redstone_EvalNearbyTorches(x, y, z);
}

/* Handle lever deletion (player breaks it) */
static void Physics_DeleteLever(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);
	
	/* If it was on, need to de-power the attached block */
	if (Redstone_IsLeverOn(block)) {
		Redstone_PropagateButtonPower(x, y, z);
		Redstone_EvalNearbyTorches(x, y, z);
	}
}

/* Handle lever activation (called when neighbor changes) - support check */
static void Physics_HandleLever(int index, BlockID block) {
	int x, y, z;
	int ax, ay, az;
	World_Unpack(index, x, y, z);
	
	/* Check if lever still has support from attached block */
	if (Redstone_GetButtonAttachBlock(x, y, z, &ax, &ay, &az)) {
		if (!World_Contains(ax, ay, az) || Blocks.Draw[World_GetBlock(ax, ay, az)] != DRAW_OPAQUE) {
			Physics_DropBlock(x, y, z, BLOCK_LEVER);
			Game_UpdateBlock(x, y, z, BLOCK_AIR);
			Physics_ActivateNeighbours(x, y, z, index);
			return;
		}
	} else {
		Physics_DropBlock(x, y, z, BLOCK_LEVER);
		Game_UpdateBlock(x, y, z, BLOCK_AIR);
		Physics_ActivateNeighbours(x, y, z, index);
		return;
	}
}

/* ---- Pressure plate physics handlers ---- */

/* Handle unpressed pressure plate placement */
static void Physics_PlacePlate(int index, BlockID block) {
	(void)index; (void)block;
}

/* Handle pressed pressure plate placement (triggered by entity stepping on) */
static void Physics_PlacePlatePressed(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);
	
	/* Propagate power through block below */
	Redstone_PropagatePlatePower(x, y, z);
	Redstone_EvalNearbyTorches(x, y, z);
}

/* Handle pressure plate deletion */
static void Physics_DeletePlate(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);
	
	/* Remove from release queue if scheduled */
	Plate_CancelRelease(x, y, z);
	
	/* If it was pressed, de-power the block below */
	if (Redstone_IsPressurePlatePressed(block)) {
		Redstone_PropagatePlatePower(x, y, z);
		Redstone_EvalNearbyTorches(x, y, z);
	}
}

/* Handle pressure plate activation (called when neighbor changes) - support check */
static void Physics_HandlePlate(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);
	
	/* Check if plate still has support from block below */
	if (y > 0) {
		BlockID below = World_GetBlock(x, y - 1, z);
		if (Blocks.Draw[below] != DRAW_OPAQUE) {
			Plate_CancelRelease(x, y, z);
			Physics_DropBlock(x, y, z, BLOCK_PRESSURE_PLATE);
			Game_UpdateBlock(x, y, z, BLOCK_AIR);
			Physics_ActivateNeighbours(x, y, z, index);
			return;
		}
	} else {
		Plate_CancelRelease(x, y, z);
		Physics_DropBlock(x, y, z, BLOCK_PRESSURE_PLATE);
		Game_UpdateBlock(x, y, z, BLOCK_AIR);
		Physics_ActivateNeighbours(x, y, z, index);
		return;
	}
}

/* Handle stone pressure plate activation (called when neighbor changes) - support check */
static void Physics_HandleStonePlate(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);
	
	/* Check if plate still has support from block below */
	if (y > 0) {
		BlockID below = World_GetBlock(x, y - 1, z);
		if (Blocks.Draw[below] != DRAW_OPAQUE) {
			Plate_CancelRelease(x, y, z);
			Physics_DropBlock(x, y, z, BLOCK_STONE_PLATE);
			Game_UpdateBlock(x, y, z, BLOCK_AIR);
			Physics_ActivateNeighbours(x, y, z, index);
			return;
		}
	} else {
		Plate_CancelRelease(x, y, z);
		Physics_DropBlock(x, y, z, BLOCK_STONE_PLATE);
		Game_UpdateBlock(x, y, z, BLOCK_AIR);
		Physics_ActivateNeighbours(x, y, z, index);
		return;
	}
}

/*########################################################################################################################*
*-------------------------------------------------Iron door physics-------------------------------------------------------*
*#########################################################################################################################*/
/* Iron doors are opened/closed by redstone power. They track their starting
   direction (NS or EW) and toggle between closed/open states based on power. */

#define IRON_DOOR_MAX 64

static struct { int x, y, z; } ironDoor_positions[IRON_DOOR_MAX];
static int ironDoor_count = 0;

static cc_bool Redstone_IsIronDoor(BlockID b) {
	return b == BLOCK_IRON_DOOR || b == BLOCK_IRON_DOOR_NS_TOP
		|| b == BLOCK_IRON_DOOR_EW_BOTTOM || b == BLOCK_IRON_DOOR_EW_TOP
		|| b == BLOCK_IRON_DOOR_NS_OPEN_BOTTOM || b == BLOCK_IRON_DOOR_NS_OPEN_TOP
		|| b == BLOCK_IRON_DOOR_EW_OPEN_BOTTOM || b == BLOCK_IRON_DOOR_EW_OPEN_TOP;
}

static cc_bool Redstone_IsIronDoorBottom(BlockID b) {
	return b == BLOCK_IRON_DOOR || b == BLOCK_IRON_DOOR_EW_BOTTOM
		|| b == BLOCK_IRON_DOOR_NS_OPEN_BOTTOM || b == BLOCK_IRON_DOOR_EW_OPEN_BOTTOM;
}

static void IronDoor_Register(int x, int y, int z) {
	int i;
	/* Check for duplicate */
	for (i = 0; i < ironDoor_count; i++) {
		if (ironDoor_positions[i].x == x && ironDoor_positions[i].y == y && ironDoor_positions[i].z == z)
			return;
	}
	if (ironDoor_count >= IRON_DOOR_MAX) return;
	ironDoor_positions[ironDoor_count].x = x;
	ironDoor_positions[ironDoor_count].y = y;
	ironDoor_positions[ironDoor_count].z = z;
	ironDoor_count++;
}

static void IronDoor_Unregister(int x, int y, int z) {
	int i;
	for (i = 0; i < ironDoor_count; i++) {
		if (ironDoor_positions[i].x == x && ironDoor_positions[i].y == y && ironDoor_positions[i].z == z) {
			ironDoor_positions[i] = ironDoor_positions[ironDoor_count - 1];
			ironDoor_count--;
			return;
		}
	}
}

/* Scan the world for iron door bottoms and register them */
static void IronDoor_ScanWorld(void) {
	int x, y, z;
	ironDoor_count = 0;
	if (!World.Blocks) return;
	
	for (y = 0; y < World.Height; y++) {
		for (z = 0; z < World.Length; z++) {
			for (x = 0; x < World.Width; x++) {
				BlockID block = World_GetBlock(x, y, z);
				if (Redstone_IsIronDoorBottom(block)) {
					IronDoor_Register(x, y, z);
				}
			}
		}
	}
}

/* Check if an iron door at (x,y,z) should be powered.
   Powered if: the door block itself receives power, OR the block below it receives power. */
static cc_bool IronDoor_IsPowered(int x, int y, int z) {
	if (Redstone_BlockReceivesPower(x, y, z)) return true;
	if (y > 0 && Redstone_BlockReceivesPower(x, y - 1, z)) return true;
	return false;
}

/* Each tick, iterate all tracked iron door bottoms and toggle open/closed based on power */
static void Redstone_TickIronDoors(void) {
	int i;
	for (i = 0; i < ironDoor_count; i++) {
		int dx = ironDoor_positions[i].x;
		int dy = ironDoor_positions[i].y;
		int dz = ironDoor_positions[i].z;
		BlockID bottom;
		cc_bool powered;
		int index;
		
		if (!World_Contains(dx, dy, dz)) continue;
		bottom = World_GetBlock(dx, dy, dz);
		
		/* If this position no longer holds an iron door bottom, skip */
		if (!Redstone_IsIronDoorBottom(bottom)) continue;
		
		powered = IronDoor_IsPowered(dx, dy, dz);
		index = World_Pack(dx, dy, dz);
		
		if (bottom == BLOCK_IRON_DOOR && powered) {
			/* NS closed → NS open (EW geometry) */
			Game_UpdateBlock(dx, dy, dz, BLOCK_IRON_DOOR_NS_OPEN_BOTTOM);
			if (World_Contains(dx, dy + 1, dz))
				Game_UpdateBlock(dx, dy + 1, dz, BLOCK_IRON_DOOR_NS_OPEN_TOP);
			Audio_PlayDigSound(SOUND_DOOR);
			Physics_ActivateNeighbours(dx, dy, dz, index);
		} else if (bottom == BLOCK_IRON_DOOR_EW_BOTTOM && powered) {
			/* EW closed → EW open (NS geometry) */
			Game_UpdateBlock(dx, dy, dz, BLOCK_IRON_DOOR_EW_OPEN_BOTTOM);
			if (World_Contains(dx, dy + 1, dz))
				Game_UpdateBlock(dx, dy + 1, dz, BLOCK_IRON_DOOR_EW_OPEN_TOP);
			Audio_PlayDigSound(SOUND_DOOR);
			Physics_ActivateNeighbours(dx, dy, dz, index);
		} else if (bottom == BLOCK_IRON_DOOR_NS_OPEN_BOTTOM && !powered) {
			/* NS open → NS closed */
			Game_UpdateBlock(dx, dy, dz, BLOCK_IRON_DOOR);
			if (World_Contains(dx, dy + 1, dz))
				Game_UpdateBlock(dx, dy + 1, dz, BLOCK_IRON_DOOR_NS_TOP);
			Audio_PlayDigSound(SOUND_DOOR);
			Physics_ActivateNeighbours(dx, dy, dz, index);
		} else if (bottom == BLOCK_IRON_DOOR_EW_OPEN_BOTTOM && !powered) {
			/* EW open → EW closed */
			Game_UpdateBlock(dx, dy, dz, BLOCK_IRON_DOOR_EW_BOTTOM);
			if (World_Contains(dx, dy + 1, dz))
				Game_UpdateBlock(dx, dy + 1, dz, BLOCK_IRON_DOOR_EW_TOP);
			Audio_PlayDigSound(SOUND_DOOR);
			Physics_ActivateNeighbours(dx, dy, dz, index);
		}
	}
}

/* Physics handlers for iron door placement/deletion */
static void Physics_PlaceIronDoor(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);
	IronDoor_Register(x, y, z);
}

static void Physics_DeleteIronDoor(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);
	IronDoor_Unregister(x, y, z);
	Physics_ActivateNeighbours(x, y, z, index);
}

/* Handle redstone torch placement - propagate power to adjacent dust */
static void Physics_PlaceRedstoneTorch(int index, BlockID block) {
	int x, y, z;
	int ax, ay, az;
	World_Unpack(index, x, y, z);
	
	/* Block ID now encodes attach direction - use GetTorchAttachBlock directly */
	if (Redstone_GetTorchAttachBlock(x, y, z, &ax, &ay, &az)) {
		/* If this is a lit torch, check if its attached block is powered */
		/* If so, immediately turn it off */
		if (Redstone_IsTorchOn(block)) {
			cc_bool powered;
			/* Temporarily hide the torch from the world so Redstone_BlockReceivesPower
			   doesn't see it as a power source */
			World_SetBlock(x, y, z, BLOCK_AIR);
			powered = Redstone_BlockReceivesPower(ax, ay, az);
			if (powered) {
				/* Attach block IS powered by external sources - place as OFF variant */
				Game_UpdateBlock(x, y, z, Redstone_GetTorchOffVariant(block));
				return;
			}
			/* Restore the torch in the world */
			World_SetBlock(x, y, z, block);
		}
	}
	
	/* Propagate to directly adjacent dust + through block above */
	Redstone_PropagateTorchPower(x, y, z);
}

/* Handle redstone torch deletion - remove power from adjacent dust */
static void Physics_DeleteRedstoneTorch(int index, BlockID block) {
	int x, y, z, s;
	int adx, ady, adz;
	World_Unpack(index, x, y, z);

	/* Clear burn-out for this position so player can re-place the torch */
	{
		int b;
		for (b = 0; b < redstone_burnoutCount; b++) {
			if (redstone_burnouts[b].x == x && redstone_burnouts[b].y == y && redstone_burnouts[b].z == z) {
				redstone_burnoutCount--;
				redstone_burnouts[b] = redstone_burnouts[redstone_burnoutCount];
				break;
			}
		}
	}

	/* Recalculate power for all dust this torch was powering:
	   direct neighbors (5 directions except attach) + through opaque block above */
	if (Redstone_GetTorchAttachDir(block, &adx, &ady, &adz)) {
		for (s = 0; s < 6; s++) {
			int sx = x + adjOffsets6[s][0];
			int sy = y + adjOffsets6[s][1];
			int sz = z + adjOffsets6[s][2];
			if (adjOffsets6[s][0] == adx && adjOffsets6[s][1] == ady && adjOffsets6[s][2] == adz)
				continue;
			if (World_Contains(sx, sy, sz) && Redstone_IsDust(World_GetBlock(sx, sy, sz)))
				Redstone_PropagatePower(sx, sy, sz);
		}
	}
	if (World_Contains(x, y + 1, z)) {
		BlockID above = World_GetBlock(x, y + 1, z);
		if (Blocks.Draw[above] == DRAW_OPAQUE) {
			for (s = 0; s < 6; s++) {
				int sx = x + adjOffsets6[s][0];
				int sy = (y + 1) + adjOffsets6[s][1];
				int sz = z + adjOffsets6[s][2];
				if (World_Contains(sx, sy, sz) && Redstone_IsDust(World_GetBlock(sx, sy, sz)))
					Redstone_PropagatePower(sx, sy, sz);
			}
		}
	}
	
	Redstone_EvalNearbyTorches(x, y, z);
}

/* Handle redstone torch activation (called when neighbor changes) */
static void Physics_HandleRedstoneTorch(int index, BlockID block) {
	int x, y, z;
	int ax, ay, az;
	World_Unpack(index, x, y, z);
	
	/* Check if torch still has physical support from its attach block */
	/* If not, delete it (no re-rotation - attached torches are destroyed) */
	if (Redstone_GetTorchAttachBlock(x, y, z, &ax, &ay, &az)) {
		if (!World_Contains(ax, ay, az) || Blocks.Draw[World_GetBlock(ax, ay, az)] != DRAW_OPAQUE) {
			Physics_DropBlock(x, y, z, BLOCK_RED_ORE_TORCH);
			Game_UpdateBlock(x, y, z, BLOCK_AIR);
			Physics_ActivateNeighbours(x, y, z, index);
			return;
		}
	} else {
		/* Couldn't determine attach block - delete for safety */
		Physics_DropBlock(x, y, z, BLOCK_RED_ORE_TORCH);
		Game_UpdateBlock(x, y, z, BLOCK_AIR);
		Physics_ActivateNeighbours(x, y, z, index);
		return;
	}
	
	/* Check if the torch's attached block is powered - if so, torch should be off */
	{
		cc_bool attachedPowered = Redstone_BlockReceivesPower(ax, ay, az);
		
		if (attachedPowered && Redstone_IsTorchOn(block)) {
			Redstone_ScheduleTorchToggle(x, y, z, Redstone_GetTorchOffVariant(block));
		} else if (!attachedPowered && Redstone_IsTorchOff(block)) {
			Redstone_ScheduleTorchToggle(x, y, z, Redstone_GetTorchOnVariant(block));
		}
	}
}

static cc_bool Physics_CheckItem(struct TickQueue* queue, int* posIndex) {
	cc_uint32 item = TickQueue_Dequeue(queue);
	*posIndex     = (int)(item & PHYSICS_POS_MASK);

	if (item >= PHYSICS_ONE_DELAY) {
		item -= PHYSICS_ONE_DELAY;
		TickQueue_Enqueue(queue, item);
		return false;
	}
	return true;
}


static void Physics_HandleSapling(int index, BlockID block) {
	IVec3 coords[TREE_MAX_COUNT];
	BlockRaw blocks[TREE_MAX_COUNT];
	int i, count, height;

	BlockID below;
	int x, y, z;
	World_Unpack(index, x, y, z);

	below = BLOCK_AIR;
	if (y > 0) below = World.Blocks[index - World.OneY];
	/* Saplings stay alive on dirt */
	if (below == BLOCK_DIRT) return;

	/* Saplings grow if on grass in light, otherwise turn to air */
	Game_UpdateBlock(x, y, z, BLOCK_AIR);
	if (below != BLOCK_GRASS || !Lighting.IsLit(x, y, z)) return;

	height = 5 + Random_Next(&physics_rnd, 3);

	if (TreeGen_CanGrow(x, y, z, height)) {
		count = TreeGen_Grow(x, y, z, height, coords, blocks);

		for (i = 0; i < count; i++) 
		{
			Game_UpdateBlock(coords[i].x, coords[i].y, coords[i].z, blocks[i]);
		}
	}
}

static void Physics_HandleDirt(int index, BlockID block) {
	int x, y, z;
	BlockID above;
	World_Unpack(index, x, y, z);

	if (!Gen_Themes[Gen_Theme].dirtToGrass) return;

	/* Don't grow grass if water, lava, ice, or glass is above */
	if (y + 1 < World.Height) {
		above = World_GetBlock(x, y + 1, z);
		if (above == BLOCK_WATER || above == BLOCK_STILL_WATER) return;
		if (above == BLOCK_LAVA  || above == BLOCK_STILL_LAVA)  return;
		if (above == BLOCK_ICE || above == BLOCK_GLASS) return;
	}

	if (Lighting.IsLit(x, y, z)) {
		Game_UpdateBlock(x, y, z, BLOCK_GRASS);
	}
}

static void Physics_HandleGrass(int index, BlockID block) {
	int x, y, z;
	BlockID above;
	World_Unpack(index, x, y, z);

	/* Convert grass to dirt when water, lava, or a solid block is directly above */
	if (y + 1 < World.Height) {
		above = World_GetBlock(x, y + 1, z);

		/* Water, lava, and ice kill grass immediately */
		if (above == BLOCK_WATER || above == BLOCK_STILL_WATER ||
		    above == BLOCK_LAVA  || above == BLOCK_STILL_LAVA ||
		    above == BLOCK_ICE) {
			Game_UpdateBlock(x, y, z, BLOCK_DIRT);
			return;
		}

		/* Don't decay under glass (transparent but blocks light growth) */
		if (above == BLOCK_GLASS) return;

		/* Don't decay under snow, snow blocks, or doors (they're transparent/decorative) */
		if (above == BLOCK_SNOW || above == BLOCK_SNOW_BLOCK) return;

		/* Don't decay under regular doors */
		if (above == BLOCK_DOOR_NS_BOTTOM || above == BLOCK_DOOR_NS_TOP ||
		    above == BLOCK_DOOR_EW_BOTTOM || above == BLOCK_DOOR_EW_TOP) return;

		/* Don't decay under iron doors */
		if (above == BLOCK_IRON_DOOR || above == BLOCK_IRON_DOOR_NS_TOP ||
		    above == BLOCK_IRON_DOOR_EW_BOTTOM || above == BLOCK_IRON_DOOR_EW_TOP ||
		    above == BLOCK_IRON_DOOR_NS_OPEN_BOTTOM || above == BLOCK_IRON_DOOR_NS_OPEN_TOP ||
		    above == BLOCK_IRON_DOOR_EW_OPEN_BOTTOM || above == BLOCK_IRON_DOOR_EW_OPEN_TOP) return;

		if (Blocks.Collide[above] == COLLIDE_SOLID) {
			Game_UpdateBlock(x, y, z, BLOCK_DIRT);
		}
	}
}

static void Physics_HandleGrassActivate(int index, BlockID block) {
	int x, y, z;
	BlockID above;
	World_Unpack(index, x, y, z);
	if (y >= World.MaxY) return;
	above = World_GetBlock(x, y + 1, z);
	if (above == BLOCK_SNOW || above == BLOCK_SNOW_BLOCK) {
		Game_UpdateBlock(x, y, z, BLOCK_SNOWY_GRASS);
	}
}

static void Physics_HandleSnowyGrassActivate(int index, BlockID block) {
	int x, y, z;
	BlockID above;
	World_Unpack(index, x, y, z);
	if (y >= World.MaxY) {
		Game_UpdateBlock(x, y, z, BLOCK_GRASS);
		return;
	}
	above = World_GetBlock(x, y + 1, z);
	if (above != BLOCK_SNOW && above != BLOCK_SNOW_BLOCK) {
		Game_UpdateBlock(x, y, z, BLOCK_GRASS);
	}
}

static void Physics_HandleSnowyGrass(int index, BlockID block) {
	int x, y, z;
	BlockID above;
	World_Unpack(index, x, y, z);
	if (y + 1 >= World.Height) return;
	above = World_GetBlock(x, y + 1, z);
	/* Water, lava, and ice kill snowy grass to dirt */
	if (above == BLOCK_WATER || above == BLOCK_STILL_WATER ||
	    above == BLOCK_LAVA  || above == BLOCK_STILL_LAVA ||
	    above == BLOCK_ICE) {
		Game_UpdateBlock(x, y, z, BLOCK_DIRT);
		return;
	}
	if (above == BLOCK_SNOW || above == BLOCK_SNOW_BLOCK) return;
	/* Snow was removed but activation hasn't fired yet - convert back */
	Game_UpdateBlock(x, y, z, BLOCK_GRASS);
}

/* Dry farmland: check for water within 4 blocks horizontally (same Y or Y-1) */
static void Physics_HandleFarmlandDry(int index, BlockID block) {
	int x, y, z, dx, dz, cy;
	BlockID b;
	World_Unpack(index, x, y, z);

	for (dx = -4; dx <= 4; dx++) {
		for (dz = -4; dz <= 4; dz++) {
			for (cy = y - 1; cy <= y; cy++) {
				if (!World_Contains(x + dx, cy, z + dz)) continue;
				b = World_GetBlock(x + dx, cy, z + dz);
				if (b == BLOCK_WATER || b == BLOCK_STILL_WATER) {
					Game_UpdateBlock(x, y, z, BLOCK_FARMLAND_WET);
					return;
				}
			}
		}
	}
}

/* Wet farmland: if no water nearby, revert to dry */
static void Physics_HandleFarmlandWet(int index, BlockID block) {
	int x, y, z, dx, dz, cy;
	BlockID b;
	World_Unpack(index, x, y, z);

	for (dx = -4; dx <= 4; dx++) {
		for (dz = -4; dz <= 4; dz++) {
			for (cy = y - 1; cy <= y; cy++) {
				if (!World_Contains(x + dx, cy, z + dz)) continue;
				b = World_GetBlock(x + dx, cy, z + dz);
				if (b == BLOCK_WATER || b == BLOCK_STILL_WATER) {
					return; /* Still has water nearby, stay wet */
				}
			}
		}
	}
	/* No water found, revert to dry farmland */
	Game_UpdateBlock(x, y, z, BLOCK_FARMLAND_DRY);
}

/* Wheat growth delay: ~20 ticks (~1 second between growth checks) */
#define WHEAT_GROWTH_DELAY 20

/* Enqueue a wheat block for future growth ticking */
static void Physics_PlaceWheat(int index, BlockID block) {
	cc_uint32 item = (cc_uint32)index;
	item |= ((cc_uint32)WHEAT_GROWTH_DELAY << 27);
	TickQueue_Enqueue(&wheatQ, item);
}

/* Process the wheat growth queue each tick (like fire queue) */
static void Physics_TickWheat(void) {
	cc_uint32 item;
	int index, count;
	BlockID block;
	cc_uint32 delay;

	count = wheatQ.count;
	while (count > 0) {
		item = TickQueue_Dequeue(&wheatQ);
		count--;

		delay = (item & PHYSICS_DELAY_MASK) >> 27;
		index = (int)(item & ~PHYSICS_DELAY_MASK);

		if (index < 0 || index >= World.Volume) continue;

		block = World_GetRawBlock(index);
		if (block < BLOCK_WHEAT_0 || block > BLOCK_WHEAT_7) continue;

		if (delay > 0) {
			item = (cc_uint32)index | ((delay - 1) << 27);
			TickQueue_Enqueue(&wheatQ, item);
			continue;
		}

		/* Delay expired: attempt growth */
		{
			int x, y, z, stage;
			BlockID below;
			World_Unpack(index, x, y, z);

			/* Crops must be on farmland */
			if (y <= 0) { Game_UpdateBlock(x, y, z, BLOCK_AIR); continue; }
			below = World_GetBlock(x, y - 1, z);
			if (below != BLOCK_FARMLAND_WET && below != BLOCK_FARMLAND_DRY) {
				Game_UpdateBlock(x, y, z, BLOCK_AIR);
				continue;
			}

			stage = block - BLOCK_WHEAT_0;

			/* Only grow on wet farmland, and not already fully grown */
			if (below == BLOCK_FARMLAND_WET && stage < 7) {
				/* ~8% chance to advance per check */
				if (Random_Next(&physics_rnd, 32) == 0) {
					Game_UpdateBlock(x, y, z, (BlockID)(BLOCK_WHEAT_0 + stage + 1));
				}
			}

			/* Re-enqueue for continued growth checking */
			if (stage < 7) {
				cc_uint32 reitem = (cc_uint32)index;
				reitem |= ((cc_uint32)WHEAT_GROWTH_DELAY << 27);
				TickQueue_Enqueue(&wheatQ, reitem);
			}
		}
	}
}

/* Wheat activation: if block below is no longer farmland, break the crop */
static void Physics_HandleWheatActivate(int index, BlockID block) {
	int x, y, z;
	BlockID below;
	World_Unpack(index, x, y, z);

	if (y <= 0) { Game_UpdateBlock(x, y, z, BLOCK_AIR); return; }
	below = World_GetBlock(x, y - 1, z);
	if (below != BLOCK_FARMLAND_WET && below != BLOCK_FARMLAND_DRY) {
		Game_UpdateBlock(x, y, z, BLOCK_AIR);
	}
}

/* Scan world for existing wheat blocks and enqueue them for growth */
static void Wheat_ScanWorld(void) {
	int i;
	BlockID b;
	if (!World.Blocks) return;
	for (i = 0; i < World.Volume; i++) {
		b = World_GetRawBlock(i);
		if (b >= BLOCK_WHEAT_0 && b <= BLOCK_WHEAT_7) {
			int stage = b - BLOCK_WHEAT_0;
			if (stage < 7) {
				cc_uint32 item = (cc_uint32)i;
				item |= ((cc_uint32)WHEAT_GROWTH_DELAY << 27);
				TickQueue_Enqueue(&wheatQ, item);
			}
		}
	}
}

static void Physics_HandleFlower(int index, BlockID block) {
	BlockID below;
	int x, y, z;
	World_Unpack(index, x, y, z);

	if (!Lighting.IsLit(x, y, z)) {
		Game_UpdateBlock(x, y, z, BLOCK_AIR);
		Physics_ActivateNeighbours(x, y, z, index);
		return;
	}

	below = BLOCK_DIRT;
	if (y > 0) below = World.Blocks[index - World.OneY];
	if (!(below == BLOCK_DIRT || below == BLOCK_GRASS)) {
		Game_UpdateBlock(x, y, z, BLOCK_AIR);
		Physics_ActivateNeighbours(x, y, z, index);
	}
}

static void Physics_HandleMushroom(int index, BlockID block) {
	BlockID below;
	int x, y, z;
	World_Unpack(index, x, y, z);

	if (Lighting.IsLit(x, y, z)) {
		Game_UpdateBlock(x, y, z, BLOCK_AIR);
		Physics_ActivateNeighbours(x, y, z, index);
		return;
	}

	below = BLOCK_STONE;
	if (y > 0) below = World.Blocks[index - World.OneY];
	if (!(below == BLOCK_STONE || below == BLOCK_COBBLE)) {
		Game_UpdateBlock(x, y, z, BLOCK_AIR);
		Physics_ActivateNeighbours(x, y, z, index);
	}
}

/* OnActivate handler for flowers (dandelion, rose) - check ground support */
static void Physics_HandleFlowerActivate(int index, BlockID block) {
	BlockID below;
	int x, y, z;
	World_Unpack(index, x, y, z);

	below = BLOCK_DIRT;
	if (y > 0) below = World.Blocks[index - World.OneY];
	if (below == BLOCK_DIRT || below == BLOCK_GRASS || below == BLOCK_SNOWY_GRASS || below == BLOCK_FARMLAND_DRY || below == BLOCK_FARMLAND_WET) return;

	/* No support - break and drop */
	Physics_DropBlock(x, y, z, block);
	Game_UpdateBlock(x, y, z, BLOCK_AIR);
	Physics_ActivateNeighbours(x, y, z, index);
}

/* OnActivate handler for mushrooms (red, brown) - check ground support */
static void Physics_HandleMushroomActivate(int index, BlockID block) {
	BlockID below;
	int x, y, z;
	World_Unpack(index, x, y, z);

	below = BLOCK_STONE;
	if (y > 0) below = World.Blocks[index - World.OneY];
	if (below == BLOCK_STONE || below == BLOCK_COBBLE) return;

	/* No support - break and drop */
	Physics_DropBlock(x, y, z, block);
	Game_UpdateBlock(x, y, z, BLOCK_AIR);
	Physics_ActivateNeighbours(x, y, z, index);
}

/* OnActivate handler for saplings - check ground support */
static void Physics_HandleSaplingActivate(int index, BlockID block) {
	BlockID below;
	int x, y, z;
	World_Unpack(index, x, y, z);

	below = BLOCK_AIR;
	if (y > 0) below = World.Blocks[index - World.OneY];
	if (below == BLOCK_DIRT || below == BLOCK_GRASS || below == BLOCK_SNOWY_GRASS) return;

	/* No support - break and drop */
	Physics_DropBlock(x, y, z, block);
	Game_UpdateBlock(x, y, z, BLOCK_AIR);
	Physics_ActivateNeighbours(x, y, z, index);
}


/*########################################################################################################################*
*-----------------------------------------------Finite Liquid Physics-----------------------------------------------------*
*#########################################################################################################################*/
/* Minecraft Alpha-style finite water/lava spread.
   Faithfully replicates BlockFlowing.updateTick from MC Alpha 1.0.6.
   Water: flows 7 blocks horizontally (fluidType=1), lava: flows 3 blocks (fluidType=2).
   Water has infinite source reproduction (2+ adjacent sources + solid below = new source).
   Liquids prefer flowing toward nearby drops (optimal flow direction).
   Stable blocks convert to "still" variant; neighbor changes reactivate them. */

static cc_bool FiniteLiquid_IsWater(BlockID b) {
	return b == BLOCK_WATER || b == BLOCK_STILL_WATER;
}

static cc_bool FiniteLiquid_IsLava(BlockID b) {
	return b == BLOCK_LAVA || b == BLOCK_STILL_LAVA;
}

/* MC Alpha: getFlowDecay - returns metadata if same material, else -1 */
static int FiniteLiquid_GetFlowDecay(int x, int y, int z, cc_bool isWater) {
	BlockID b;
	int idx;
	if (!World_Contains(x, y, z)) return -1;
	b = World_GetBlock(x, y, z);
	if (isWater ? !FiniteLiquid_IsWater(b) : !FiniteLiquid_IsLava(b)) return -1;
	idx = World_Pack(x, y, z);
	return physics_flowLevels[idx];
}

/* MC Alpha: blockBlocksFlow - doors, signs, ladders always block; otherwise solid material blocks */
static cc_bool FiniteLiquid_BlocksFlow(int x, int y, int z) {
	BlockID b;
	if (!World_Contains(x, y, z)) return true;
	b = World_GetBlock(x, y, z);
	if (b == BLOCK_AIR) return false;
	/* Doors, signs, ladders block flow (MC Alpha hardcoded special cases) */
	if (b == BLOCK_DOOR_NS_BOTTOM || b == BLOCK_DOOR_NS_TOP ||
		b == BLOCK_DOOR_EW_BOTTOM || b == BLOCK_DOOR_EW_TOP ||
		b == BLOCK_IRON_DOOR      || b == BLOCK_IRON_DOOR_NS_TOP ||
		b == BLOCK_IRON_DOOR_EW_BOTTOM || b == BLOCK_IRON_DOOR_EW_TOP ||
		b == BLOCK_LADDER ||
		b == BLOCK_SIGN_WALL || b == BLOCK_SIGN_FLOOR) return true;
	/* MC Alpha: material.isSolid() - in CC, solid collide blocks */
	return Blocks.Collide[b] >= COLLIDE_SOLID;
}

/* MC Alpha: liquidCanDisplaceBlock
   - same material: NO
   - target is lava: NO (even water can't displace lava!)
   - target blocks flow: NO
   - otherwise: YES */
static cc_bool FiniteLiquid_CanDisplace(int x, int y, int z, cc_bool isWater) {
	BlockID b;
	if (!World_Contains(x, y, z)) return false;
	b = World_GetBlock(x, y, z);
	/* Can't displace same liquid type */
	if (isWater ? FiniteLiquid_IsWater(b) : FiniteLiquid_IsLava(b)) return false;
	/* Can't displace lava (even water can't - MC Alpha behavior) */
	if (FiniteLiquid_IsLava(b)) return false;
	/* Can't displace flow-blocking blocks */
	return !FiniteLiquid_BlocksFlow(x, y, z);
}

/* MC Alpha: getSmallestFlowDecay - accumulative; call once per cardinal neighbor.
   Returns the smallest effective flow decay seen so far. Counts source blocks. */
static int FiniteLiquid_GetSmallestFlowDecay(int x, int y, int z,
	int currentSmallest, cc_bool isWater, int* numSources)
{
	int decay = FiniteLiquid_GetFlowDecay(x, y, z, isWater);
	if (decay < 0) return currentSmallest;
	if (decay == 0) (*numSources)++;
	if (decay >= 8) decay = 0;
	return (currentSmallest >= 0 && decay >= currentSmallest) ? currentSmallest : decay;
}

/* MC Alpha: calculateFlowCost - find shortest path to a drop within 4 blocks */
static int FiniteLiquid_CalcFlowCost(int x, int y, int z, int depth, int fromDir, cc_bool isWater) {
	int best = 1000;
	int dir, nx, nz, cost;

	for (dir = 0; dir < 4; dir++) {
		/* Don't go back the way we came */
		if ((dir == 0 && fromDir == 1) || (dir == 1 && fromDir == 0) ||
			(dir == 2 && fromDir == 3) || (dir == 3 && fromDir == 2)) continue;

		nx = x; nz = z;
		if (dir == 0) nx--;
		if (dir == 1) nx++;
		if (dir == 2) nz--;
		if (dir == 3) nz++;

		if (FiniteLiquid_BlocksFlow(nx, y, nz)) continue;

		/* Can't flow through source blocks of same type */
		if (World_Contains(nx, y, nz)) {
			BlockID nb = World_GetBlock(nx, y, nz);
			if ((isWater ? FiniteLiquid_IsWater(nb) : FiniteLiquid_IsLava(nb)) &&
				physics_flowLevels[World_Pack(nx, y, nz)] == 0) continue;
		}

		/* Found a drop - return current depth as cost */
		if (!FiniteLiquid_BlocksFlow(nx, y - 1, nz)) return depth;

		if (depth < 4) {
			cost = FiniteLiquid_CalcFlowCost(nx, y, nz, depth + 1, dir, isWater);
			if (cost < best) best = cost;
		}
	}
	return best;
}

/* MC Alpha: getOptimalFlowDirections - find directions toward nearest drop */
static void FiniteLiquid_GetOptimalDirs(int x, int y, int z, cc_bool isWater, cc_bool* dirs) {
	int flowCost[4];
	int dir, nx, nz, best;

	for (dir = 0; dir < 4; dir++) {
		flowCost[dir] = 1000;
		nx = x; nz = z;
		if (dir == 0) nx--;
		if (dir == 1) nx++;
		if (dir == 2) nz--;
		if (dir == 3) nz++;

		if (FiniteLiquid_BlocksFlow(nx, y, nz)) continue;

		/* Don't flow toward same-type source blocks */
		if (World_Contains(nx, y, nz)) {
			BlockID nb = World_GetBlock(nx, y, nz);
			if ((isWater ? FiniteLiquid_IsWater(nb) : FiniteLiquid_IsLava(nb)) &&
				physics_flowLevels[World_Pack(nx, y, nz)] == 0) continue;
		}

		if (!FiniteLiquid_BlocksFlow(nx, y - 1, nz)) {
			flowCost[dir] = 0;
		} else {
			flowCost[dir] = FiniteLiquid_CalcFlowCost(nx, y, nz, 1, dir, isWater);
		}
	}

	best = flowCost[0];
	for (dir = 1; dir < 4; dir++) {
		if (flowCost[dir] < best) best = flowCost[dir];
	}

	for (dir = 0; dir < 4; dir++) {
		dirs[dir] = (flowCost[dir] == best);
	}
}

/* Check if position is near a sponge (within 2 blocks) */
static cc_bool FiniteLiquid_NearSponge(int x, int y, int z) {
	int xx, yy, zz;
	for (yy = (y < 2 ? 0 : y - 2); yy <= (y > physics_maxWaterY ? World.MaxY : y + 2); yy++) {
		for (zz = (z < 2 ? 0 : z - 2); zz <= (z > physics_maxWaterZ ? World.MaxZ : z + 2); zz++) {
			for (xx = (x < 2 ? 0 : x - 2); xx <= (x > physics_maxWaterX ? World.MaxX : x + 2); xx++) {
				if (World_GetBlock(xx, yy, zz) == BLOCK_SPONGE) return true;
			}
		}
	}
	return false;
}

/* MC Alpha: updateFlow - convert flowing liquid to still (stabilized) variant */
static void FiniteLiquid_UpdateFlow(int x, int y, int z, cc_bool isWater) {
	/* Convert flowing -> still, preserving flow level metadata */
	BlockID stillBlock = isWater ? BLOCK_STILL_WATER : BLOCK_STILL_LAVA;
	Game_UpdateBlock(x, y, z, stillBlock);
}

/* MC Alpha: flowIntoBlock - flow liquid into a neighboring position */
static void FiniteLiquid_FlowInto(int x, int y, int z, int newLevel, cc_bool isWater) {
	BlockID existing;
	int idx;
	if (!World_Contains(x, y, z)) return;
	if (!FiniteLiquid_CanDisplace(x, y, z, isWater)) return;

	existing = World_GetBlock(x, y, z);
	idx = World_Pack(x, y, z);

	if (existing != BLOCK_AIR) {
		if (!isWater) {
			/* Lava: trigger mix effects (fizz) when displacing non-air blocks */
			Audio_PlayDigSoundRate(SOUND_FIZZ, 80 + (x * 7 + z * 13) % 41);
		}
		/* Non-lava liquids: in MC Alpha, the displaced block drops as item.
		   CC doesn't have item drops, so just replace. */
	}

	/* Check sponge before placing water */
	if (isWater && FiniteLiquid_NearSponge(x, y, z)) return;

	physics_flowLevels[idx] = (cc_uint8)newLevel;
	Game_UpdateBlock(x, y, z, isWater ? BLOCK_WATER : BLOCK_LAVA);

	/* Schedule this new block for a future tick (MC Alpha: onBlockAdded -> scheduleBlockUpdate) */
	if (isWater)
		TickQueue_Enqueue(&waterQ, PHYSICS_WATER_DELAY | idx);
	else
		TickQueue_Enqueue(&lavaQ, PHYSICS_LAVA_DELAY | idx);
}

/* MC Alpha: checkForHarden - when lava is placed, check for adjacent water and harden */
static void FiniteLiquid_CheckLavaHarden(int x, int y, int z) {
	cc_bool touchesWater = false;
	int idx = World_Pack(x, y, z);
	int level = physics_flowLevels[idx];

	if (World_Contains(x, y, z - 1) && FiniteLiquid_IsWater(World_GetBlock(x, y, z - 1))) touchesWater = true;
	if (World_Contains(x, y, z + 1) && FiniteLiquid_IsWater(World_GetBlock(x, y, z + 1))) touchesWater = true;
	if (World_Contains(x - 1, y, z) && FiniteLiquid_IsWater(World_GetBlock(x - 1, y, z))) touchesWater = true;
	if (World_Contains(x + 1, y, z) && FiniteLiquid_IsWater(World_GetBlock(x + 1, y, z))) touchesWater = true;
	if (World_Contains(x, y + 1, z) && FiniteLiquid_IsWater(World_GetBlock(x, y + 1, z))) touchesWater = true;

	if (touchesWater) {
		if (level == 0) {
			Game_UpdateBlock(x, y, z, BLOCK_OBSIDIAN);
		} else {
			Game_UpdateBlock(x, y, z, BLOCK_COBBLE);
		}
		physics_flowLevels[idx] = 0;
		Audio_PlayDigSoundRate(SOUND_FIZZ, 80 + (x * 7 + z * 13) % 41);
	}
}

/* When water touches lava, harden the lava into cobblestone or obsidian.
   Only water on TOP of a lava source -> obsidian; all other contacts -> cobblestone. */
static void FiniteLiquid_CheckWaterHarden(int x, int y, int z) {
	int dirs[6][3] = { {0,0,-1}, {0,0,1}, {-1,0,0}, {1,0,0}, {0,1,0}, {0,-1,0} };
	int i;
	for (i = 0; i < 6; i++) {
		int nx = x + dirs[i][0], ny = y + dirs[i][1], nz = z + dirs[i][2];
		BlockID nb;
		int nIdx, nLevel;
		cc_bool waterAboveLava;
		if (!World_Contains(nx, ny, nz)) continue;
		nb = World_GetBlock(nx, ny, nz);
		if (!FiniteLiquid_IsLava(nb)) continue;
		nIdx = World_Pack(nx, ny, nz);
		nLevel = physics_flowLevels[nIdx];
		/* Obsidian only when water is directly on top of a lava source */
		waterAboveLava = (dirs[i][1] == -1);
		if (nLevel == 0 && waterAboveLava) {
			Game_UpdateBlock(nx, ny, nz, BLOCK_OBSIDIAN);
		} else {
			Game_UpdateBlock(nx, ny, nz, BLOCK_COBBLE);
		}
		physics_flowLevels[nIdx] = 0;
		Audio_PlayDigSoundRate(SOUND_FIZZ, 80 + (nx * 7 + nz * 13) % 41);
	}
}

/* Main finite liquid update for a single block - MC Alpha: BlockFlowing.updateTick */
static void FiniteLiquid_UpdateBlock(int index, cc_bool isWater) {
	int x, y, z;
	int curLevel, newLevel, numSources;
	int fluidType = isWater ? 1 : 2;
	cc_bool shouldUpdateFlow = true;

	World_Unpack(index, x, y, z);
	curLevel = physics_flowLevels[index];

	/* Non-source blocks: recalculate what our level should be */
	if (curLevel > 0) {
		int smallestNeighbor = -100;
		numSources = 0;

		/* MC Alpha: getSmallestFlowDecay for each of 4 cardinal neighbors */
		smallestNeighbor = FiniteLiquid_GetSmallestFlowDecay(x - 1, y, z, smallestNeighbor, isWater, &numSources);
		smallestNeighbor = FiniteLiquid_GetSmallestFlowDecay(x + 1, y, z, smallestNeighbor, isWater, &numSources);
		smallestNeighbor = FiniteLiquid_GetSmallestFlowDecay(x, y, z - 1, smallestNeighbor, isWater, &numSources);
		smallestNeighbor = FiniteLiquid_GetSmallestFlowDecay(x, y, z + 1, smallestNeighbor, isWater, &numSources);

		newLevel = smallestNeighbor + fluidType;
		if (newLevel >= 8 || smallestNeighbor < 0) {
			newLevel = -1;
		}

		/* Check for liquid above - falling liquid overrides level */
		{
			int aboveDecay = FiniteLiquid_GetFlowDecay(x, y + 1, z, isWater);
			if (aboveDecay >= 0) {
				newLevel = (aboveDecay >= 8) ? aboveDecay : aboveDecay + 8;
			}
		}

		/* Water source reproduction: 2+ adjacent sources + solid block below = new source */
		if (numSources >= 2 && isWater) {
			if (y > 0) {
				BlockID below = World_GetBlock(x, y - 1, z);
				/* MC Alpha: isBlockNormalCube - solid opaque cube below */
				if (Blocks.Collide[below] >= COLLIDE_SOLID) {
					newLevel = 0;
				}
			}
		}

		/* Lava resists increasing levels (75% chance to keep current level) */
		if (!isWater && curLevel < 8 && newLevel < 8 && newLevel > curLevel) {
			if (Random_Range(&physics_rnd, 0, 4) != 0) {
				newLevel = curLevel;
				shouldUpdateFlow = false;
			}
		}

		if (newLevel != curLevel) {
			curLevel = newLevel;
			if (newLevel < 0) {
				/* Block should disappear */
				Game_UpdateBlock(x, y, z, BLOCK_AIR);
				physics_flowLevels[index] = 0;
				Physics_ActivateNeighbours(x, y, z, index);
				return;
			} else {
				/* Level changed - update metadata, re-schedule, notify neighbors */
				physics_flowLevels[index] = (cc_uint8)newLevel;
				/* Trigger mesh rebuild so flow-level visual height updates */
				Game_UpdateBlock(x, y, z, isWater ? BLOCK_WATER : BLOCK_LAVA);
				if (isWater)
					TickQueue_Enqueue(&waterQ, PHYSICS_WATER_DELAY | index);
				else
					TickQueue_Enqueue(&lavaQ, PHYSICS_LAVA_DELAY | index);
				Physics_ActivateNeighbours(x, y, z, index);
			}
		} else if (shouldUpdateFlow) {
			/* Level unchanged and stable - convert to still variant */
			FiniteLiquid_UpdateFlow(x, y, z, isWater);
		}
	} else {
		/* Source block (level 0) - always convert to still */
		FiniteLiquid_UpdateFlow(x, y, z, isWater);
	}

	/* Check lava/water hardening interactions */
	if (!isWater) {
		FiniteLiquid_CheckLavaHarden(x, y, z);
		/* If lava was hardened (turned to obsidian/cobble), stop spreading */
		if (!FiniteLiquid_IsLava(World_GetBlock(x, y, z))) return;
	} else {
		FiniteLiquid_CheckWaterHarden(x, y, z);
	}

	/* --- Spreading phase (runs for both source and non-source blocks) --- */

	/* Spread downward first */
	if (FiniteLiquid_CanDisplace(x, y - 1, z, isWater)) {
		int downLevel = (curLevel >= 8) ? curLevel : curLevel + 8;
		FiniteLiquid_FlowInto(x, y - 1, z, downLevel, isWater);
	} else if (curLevel >= 0 && (curLevel == 0 || FiniteLiquid_BlocksFlow(x, y - 1, z))) {
		/* Can't flow down (or below blocks flow): spread horizontally */
		cc_bool dirs[4];
		int spreadLevel = curLevel + fluidType;
		int nx, nz, dir;

		/* Falling liquid spreads horizontally at level 1 */
		if (curLevel >= 8) spreadLevel = 1;
		/* Can't spread further if level would be >= 8 */
		if (spreadLevel >= 8) return;

		FiniteLiquid_GetOptimalDirs(x, y, z, isWater, dirs);

		for (dir = 0; dir < 4; dir++) {
			if (!dirs[dir]) continue;
			nx = x; nz = z;
			if (dir == 0) nx--;
			if (dir == 1) nx++;
			if (dir == 2) nz--;
			if (dir == 3) nz++;
			FiniteLiquid_FlowInto(nx, y, nz, spreadLevel, isWater);
		}
	}
}

/* Place handler for finite water */
static void FiniteLiquid_PlaceWater(int index, BlockID block) {
	physics_flowLevels[index] = 0; /* Source block */
	TickQueue_Enqueue(&waterQ, PHYSICS_WATER_DELAY | index);
}

/* Place handler for finite lava */
static void FiniteLiquid_PlaceLava(int index, BlockID block) {
	physics_flowLevels[index] = 0; /* Source block */
	TickQueue_Enqueue(&lavaQ, PHYSICS_LAVA_DELAY | index);
}

/* Delete handler for finite water - clears flow level metadata */
static void FiniteLiquid_DeleteWater(int index, BlockID block) {
	physics_flowLevels[index] = 0;
}

/* Delete handler for finite lava - clears flow level metadata */
static void FiniteLiquid_DeleteLava(int index, BlockID block) {
	physics_flowLevels[index] = 0;
}

/* Activate handler for finite water.
   When a still water block is activated by a neighbor change,
   convert it back to flowing (MC Alpha: BlockStationary.onNeighborBlockChange) */
static void FiniteLiquid_ActivateWater(int index, BlockID block) {
	if (block == BLOCK_STILL_WATER) {
		int x, y, z;
		World_Unpack(index, x, y, z);
		Game_UpdateBlock(x, y, z, BLOCK_WATER);
	}
	TickQueue_Enqueue(&waterQ, PHYSICS_WATER_DELAY | index);
}

/* Activate handler for finite lava.
   When a still lava block is activated by a neighbor change,
   convert it back to flowing (MC Alpha: BlockStationary.onNeighborBlockChange) */
static void FiniteLiquid_ActivateLava(int index, BlockID block) {
	if (block == BLOCK_STILL_LAVA) {
		int x, y, z;
		World_Unpack(index, x, y, z);
		Game_UpdateBlock(x, y, z, BLOCK_LAVA);
	}
	TickQueue_Enqueue(&lavaQ, PHYSICS_LAVA_DELAY | index);
}

/* Tick all queued finite water blocks */
static void FiniteLiquid_TickWater(void) {
	int i, count = waterQ.count;
	for (i = 0; i < count; i++) {
		int index;
		if (Physics_CheckItem(&waterQ, &index)) {
			BlockID block = World.Blocks[index];
			if (block != BLOCK_WATER) continue; /* Only tick flowing, not still */
			FiniteLiquid_UpdateBlock(index, true);
		}
	}
}

/* Tick all queued finite lava blocks */
static void FiniteLiquid_TickLava(void) {
	int i, count = lavaQ.count;
	for (i = 0; i < count; i++) {
		int index;
		if (Physics_CheckItem(&lavaQ, &index)) {
			BlockID block = World.Blocks[index];
			if (block != BLOCK_LAVA) continue; /* Only tick flowing, not still */
			FiniteLiquid_UpdateBlock(index, false);
		}
	}
}


static void Physics_PlaceLava(int index, BlockID block) {
	TickQueue_Enqueue(&lavaQ, PHYSICS_LAVA_DELAY | index);
}

static void Physics_PropagateLava(int posIndex, int x, int y, int z) {
	BlockID block = World.Blocks[posIndex];

	if (block >= BLOCK_WATER && block <= BLOCK_STILL_LAVA) {
		/* Lava spreading into water turns the water solid */
		if (block == BLOCK_WATER || block == BLOCK_STILL_WATER) {
			Game_UpdateBlock(x, y, z, BLOCK_STONE);
			Audio_PlayDigSoundRate(SOUND_FIZZ, 80 + (x * 7 + z * 13) % 41);
		}
	} else if (Blocks.Collide[block] == COLLIDE_NONE) {
		TickQueue_Enqueue(&lavaQ, PHYSICS_LAVA_DELAY | posIndex);
		Game_UpdateBlock(x, y, z, BLOCK_LAVA);
	}
}

static void Physics_ActivateLava(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);

	if (x > 0)          Physics_PropagateLava(index - 1, x - 1, y, z);
	if (x < World.MaxX) Physics_PropagateLava(index + 1, x + 1, y, z);
	if (z > 0)          Physics_PropagateLava(index - World.Width, x, y, z - 1);
	if (z < World.MaxZ) Physics_PropagateLava(index + World.Width, x, y, z + 1);
	if (y > 0)          Physics_PropagateLava(index - World.OneY, x, y - 1, z);
}

static void Physics_TickLava(void) {
	int i, count = lavaQ.count;
	for (i = 0; i < count; i++) {
		int index;
		if (Physics_CheckItem(&lavaQ, &index)) {
			BlockID block = World.Blocks[index];
			if (!(block == BLOCK_LAVA || block == BLOCK_STILL_LAVA)) continue;
			Physics_ActivateLava(index, block);
		}
	}
}


static void Physics_PlaceWater(int index, BlockID block) {
	TickQueue_Enqueue(&waterQ, PHYSICS_WATER_DELAY | index);
}

static void Physics_PropagateWater(int posIndex, int x, int y, int z) {
	BlockID block = World.Blocks[posIndex];
	int xx, yy, zz;

	if (block >= BLOCK_WATER && block <= BLOCK_STILL_LAVA) {
		/* Water spreading into lava turns the lava solid */
		if (block == BLOCK_LAVA || block == BLOCK_STILL_LAVA) {
			Game_UpdateBlock(x, y, z, BLOCK_OBSIDIAN);
			Audio_PlayDigSoundRate(SOUND_FIZZ, 80 + (x * 7 + z * 13) % 41);
		}
	} else if (Blocks.Collide[block] == COLLIDE_NONE) {
		/* Sponge check */		
		for (yy = (y < 2 ? 0 : y - 2); yy <= (y > physics_maxWaterY ? World.MaxY : y + 2); yy++) {
			for (zz = (z < 2 ? 0 : z - 2); zz <= (z > physics_maxWaterZ ? World.MaxZ : z + 2); zz++) {
				for (xx = (x < 2 ? 0 : x - 2); xx <= (x > physics_maxWaterX ? World.MaxX : x + 2); xx++) {
					block = World_GetBlock(xx, yy, zz);
					if (block == BLOCK_SPONGE) return;
				}
			}
		}

		TickQueue_Enqueue(&waterQ, PHYSICS_WATER_DELAY | posIndex);
		Game_UpdateBlock(x, y, z, BLOCK_WATER);
	}
}

static void Physics_ActivateWater(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);

	if (x > 0)          Physics_PropagateWater(index - 1,           x - 1, y,     z);
	if (x < World.MaxX) Physics_PropagateWater(index + 1,           x + 1, y,     z);
	if (z > 0)          Physics_PropagateWater(index - World.Width, x,     y,     z - 1);
	if (z < World.MaxZ) Physics_PropagateWater(index + World.Width, x,     y,     z + 1);
	if (y > 0)          Physics_PropagateWater(index - World.OneY,  x,     y - 1, z);
}

static void Physics_TickWater(void) {
	int i, count = waterQ.count;
	for (i = 0; i < count; i++) {
		int index;
		if (Physics_CheckItem(&waterQ, &index)) {
			BlockID block = World.Blocks[index];
			if (!(block == BLOCK_WATER || block == BLOCK_STILL_WATER)) continue;
			Physics_ActivateWater(index, block);
		}
	}
}


static void Physics_PlaceSponge(int index, BlockID block) {
	int x, y, z, xx, yy, zz;
	World_Unpack(index, x, y, z);

	for (yy = y - 2; yy <= y + 2; yy++) {
		for (zz = z - 2; zz <= z + 2; zz++) {
			for (xx = x - 2; xx <= x + 2; xx++) {
				if (!World_Contains(xx, yy, zz)) continue;

				block = World_GetBlock(xx, yy, zz);
				if (block == BLOCK_WATER || block == BLOCK_STILL_WATER) {
					Game_UpdateBlock(xx, yy, zz, BLOCK_AIR);
				}
			}
		}
	}
}

static void Physics_DeleteSponge(int index, BlockID block) {
	int x, y, z, xx, yy, zz;
	World_Unpack(index, x, y, z);

	for (yy = y - 3; yy <= y + 3; yy++) {
		for (zz = z - 3; zz <= z + 3; zz++) {
			for (xx = x - 3; xx <= x + 3; xx++) {
				if (Math_AbsI(yy - y) == 3 || Math_AbsI(zz - z) == 3 || Math_AbsI(xx - x) == 3) {
					if (!World_Contains(xx, yy, zz)) continue;

					index = World_Pack(xx, yy, zz);
					block = World.Blocks[index];
					if (block == BLOCK_WATER || block == BLOCK_STILL_WATER) {
						TickQueue_Enqueue(&waterQ, index | PHYSICS_ONE_DELAY);
					}
				}
			}
		}
	}
}


static void Physics_HandleSlab(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);
	if (index < World.OneY) return;

	if (World.Blocks[index - World.OneY] != BLOCK_SLAB) return;
	Game_UpdateBlock(x, y,     z, BLOCK_AIR);
	Game_UpdateBlock(x, y - 1, z, BLOCK_DOUBLE_SLAB);
}

/* Cobblestone slab physics removed - block is now invalid */


/* ---- TNT system: placed normally, explodes on redstone power or right-click fuse ---- */

/* Blocks that resist TNT explosions (water, lava, metal, stone) */
static cc_bool BlocksTNT(BlockID b) {
	return (b >= BLOCK_WATER && b <= BLOCK_STILL_LAVA) || 
		(Blocks.ExtendedCollide[b] == COLLIDE_SOLID && (Blocks.DigSounds[b] == SOUND_METAL || Blocks.DigSounds[b] == SOUND_STONE));
}

#define TNT_POWER 4
#define TNT_POWER_SQUARED (TNT_POWER * TNT_POWER)

/* Explode TNT at the given position - removes TNT block and breaks radius */
/* Max chain TNT detonations per explosion */
#define TNT_CHAIN_MAX 32
#define TNT_CHAIN_FUSE_TICKS 4  /* very short fuse for chain reactions (~0.2s) */

static void TNT_ScheduleFuseShort(int x, int y, int z, int ticks);

void TNT_ExplodeRadius(int x, int y, int z, int power) {
	int dx, dy, dz, xx, yy, zz, index, i;
	int powerSq = power * power;
	BlockID block;
	float cx, cy, cz, ex, ey, ez, distSq, dist, maxDistSq, strength;
	int damage;
	struct Entity* player;
	/* Chain TNT positions to schedule */
	struct { int x, y, z; } chainTNT[TNT_CHAIN_MAX];
	int chainCount = 0;
	
	Particles_SmokeEffect((float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f, (float)power);
	
	/* Remove the TNT block itself */
	if (World_Contains(x, y, z)) {
		index = World_Pack(x, y, z);
		Game_UpdateBlock(x, y, z, BLOCK_AIR);
		Physics_ActivateNeighbours(x, y, z, index);
	}
	
	/* Break blocks in a sphere, collect TNT blocks for chain detonation */
	for (dy = -power; dy <= power; dy++) {
		for (dz = -power; dz <= power; dz++) {
			for (dx = -power; dx <= power; dx++) {
				if (dx * dx + dy * dy + dz * dz > powerSq) continue;

				xx = x + dx; yy = y + dy; zz = z + dz;
				if (!World_Contains(xx, yy, zz)) continue;
				index = World_Pack(xx, yy, zz);

				block = World.Blocks[index];
				if (BlocksTNT(block)) continue;

				/* If this is a TNT block, schedule it for chain detonation */
				if (block == BLOCK_TNT && chainCount < TNT_CHAIN_MAX) {
					chainTNT[chainCount].x = xx;
					chainTNT[chainCount].y = yy;
					chainTNT[chainCount].z = zz;
					chainCount++;
					continue; /* don't break the TNT block - let its own explosion do that */
				}

				Game_UpdateBlock(xx, yy, zz, BLOCK_AIR);
				Physics_ActivateNeighbours(xx, yy, zz, index);
			}
		}
	}
	
	/* Schedule chain TNT explosions with short fuse */
	for (i = 0; i < chainCount; i++) {
		TNT_ScheduleFuseShort(chainTNT[i].x, chainTNT[i].y, chainTNT[i].z, TNT_CHAIN_FUSE_TICKS);
	}
	
	/* Damage/knockback for mobs and player within explosion radius */
	cx = (float)x + 0.5f;
	cy = (float)y + 0.5f;
	cz = (float)z + 0.5f;
	maxDistSq = (float)(powerSq + 4);
	
	/* Damage and push mobs */
	for (i = 0; i < MAX_NET_PLAYERS; i++) {
		struct Entity* e = Entities.List[i];
		if (!e || !Mob_IsMob(i)) continue;
		
		ex = e->Position.x - cx;
		ey = e->Position.y - cy;
		ez = e->Position.z - cz;
		distSq = ex * ex + ey * ey + ez * ez;
		
		if (distSq <= maxDistSq) {
			dist = Math_SqrtF(distSq);
			if (dist < 0.5f) dist = 0.5f;
			/* Damage: 20 at center (dist<=1), 5 at edge */
			strength = 1.0f - (dist / (Math_SqrtF(maxDistSq) + 0.01f));
			if (strength < 0.0f) strength = 0.0f;
			damage = 5 + (int)(15.0f * strength);
			Mob_DamageMob(i, damage, false);
			
			/* Chain-explode creepers caught in TNT blast */
			if (Mob_IsCreeper(i)) {
				Mob_TriggerCreeperChainExplosion(i);
			}
			
			/* Push mob away from explosion */
			e->Velocity.x += (ex / dist) * 1.5f * strength + (ex / dist) * 0.3f;
			e->Velocity.y += 0.6f * strength + 0.2f;
			e->Velocity.z += (ez / dist) * 1.5f * strength + (ez / dist) * 0.3f;
		}
	}
	
	/* Push the player away from the explosion */
	player = &Entities.CurPlayer->Base;
	ex = player->Position.x - cx;
	ey = player->Position.y - cy;
	ez = player->Position.z - cz;
	distSq = ex * ex + ey * ey + ez * ez;
	
	if (distSq <= maxDistSq) {
		dist = Math_SqrtF(distSq);
		if (dist < 0.5f) dist = 0.5f;
		strength = 1.0f - (dist / (Math_SqrtF(maxDistSq) + 0.01f));
		if (strength < 0.0f) strength = 0.0f;
		
		/* Deal same damage to player as mobs take from explosions */
		{
			int damage = 5 + (int)(15.0f * strength);
			Player_Damage(damage);
		}
		
		player->Velocity.x += (ex / dist) * 2.0f * strength + (ex / dist) * 0.5f;
		player->Velocity.y += 1.2f * strength + 0.4f;
		player->Velocity.z += (ez / dist) * 2.0f * strength + (ez / dist) * 0.5f;
	}
}

void TNT_Explode(int x, int y, int z) {
	Audio_PlayDigSound(SOUND_EXPLODE);
	TNT_ExplodeRadius(x, y, z, TNT_POWER);
}

/* TNT tracking array - for checking redstone power each tick */
#define TNT_TRACK_MAX 2048
static struct { int x, y, z; } tnt_positions[TNT_TRACK_MAX];
static int tnt_count = 0;

static void TNT_Register(int x, int y, int z) {
	int i;
	for (i = 0; i < tnt_count; i++) {
		if (tnt_positions[i].x == x && tnt_positions[i].y == y && tnt_positions[i].z == z)
			return; /* already tracked */
	}
	if (tnt_count >= TNT_TRACK_MAX) return;
	tnt_positions[tnt_count].x = x;
	tnt_positions[tnt_count].y = y;
	tnt_positions[tnt_count].z = z;
	tnt_count++;
}

static void TNT_Unregister(int x, int y, int z) {
	int i;
	for (i = 0; i < tnt_count; i++) {
		if (tnt_positions[i].x == x && tnt_positions[i].y == y && tnt_positions[i].z == z) {
			tnt_count--;
			tnt_positions[i] = tnt_positions[tnt_count];
			return;
		}
	}
}

/* TNT fuse queue - right-clicked TNT waits 5 seconds (100 ticks) before exploding */
#define TNT_FUSE_MAX 2048
#define TNT_FUSE_TICKS 60  /* 3 seconds at 20Hz */
typedef struct TNTFuseEntry_ {
	int x, y, z;
	int ticksLeft;
} TNTFuseEntry;

static TNTFuseEntry tnt_fuseQueue[TNT_FUSE_MAX];
static int tnt_fuseCount = 0;

static void TNT_ScanWorld(void) {
	int x, y, z;
	tnt_count = 0;
	tnt_fuseCount = 0;
	if (!World.Blocks) return;
	
	for (y = 0; y < World.Height; y++) {
		for (z = 0; z < World.Length; z++) {
			for (x = 0; x < World.Width; x++) {
				if (World_GetBlock(x, y, z) == BLOCK_TNT)
					TNT_Register(x, y, z);
			}
		}
	}
}

void TNT_ScheduleFuse(int x, int y, int z) {
	int i;
	/* Check if already scheduled */
	for (i = 0; i < tnt_fuseCount; i++) {
		if (tnt_fuseQueue[i].x == x && tnt_fuseQueue[i].y == y && tnt_fuseQueue[i].z == z)
			return; /* already has a fuse lit */
	}
	if (tnt_fuseCount >= TNT_FUSE_MAX) return;
	tnt_fuseQueue[tnt_fuseCount].x = x;
	tnt_fuseQueue[tnt_fuseCount].y = y;
	tnt_fuseQueue[tnt_fuseCount].z = z;
	tnt_fuseQueue[tnt_fuseCount].ticksLeft = TNT_FUSE_TICKS;
	tnt_fuseCount++;
	Audio_PlayDigSound(SOUND_FUSE);
}

/* Schedule a TNT fuse with custom tick count (for chain reactions) */
static void TNT_ScheduleFuseShort(int x, int y, int z, int ticks) {
	int i;
	for (i = 0; i < tnt_fuseCount; i++) {
		if (tnt_fuseQueue[i].x == x && tnt_fuseQueue[i].y == y && tnt_fuseQueue[i].z == z)
			return;
	}
	if (tnt_fuseCount >= TNT_FUSE_MAX) return;
	tnt_fuseQueue[tnt_fuseCount].x = x;
	tnt_fuseQueue[tnt_fuseCount].y = y;
	tnt_fuseQueue[tnt_fuseCount].z = z;
	tnt_fuseQueue[tnt_fuseCount].ticksLeft = ticks;
	tnt_fuseCount++;
}

/* Check all tracked TNT blocks for redstone power - explode immediately if powered */
static void Redstone_TickTNT(void) {
	int i = 0;
	while (i < tnt_count) {
		int tx = tnt_positions[i].x;
		int ty = tnt_positions[i].y;
		int tz = tnt_positions[i].z;
		
		if (!World_Contains(tx, ty, tz) || World_GetBlock(tx, ty, tz) != BLOCK_TNT) {
			/* TNT no longer here, remove from tracking */
			tnt_count--;
			tnt_positions[i] = tnt_positions[tnt_count];
			continue;
		}
		
		if (Redstone_BlockReceivesPower(tx, ty, tz)) {
			/* Remove from tracking before exploding */
			tnt_count--;
			tnt_positions[i] = tnt_positions[tnt_count];
			TNT_Explode(tx, ty, tz);
			continue; /* don't increment - recheck swapped entry */
		}
		
		/* Also check if any adjacent block is strongly powered */
		{
			int s;
			cc_bool adjPowered = false;
			for (s = 0; s < 6; s++) {
				int nx = tx + adjOffsets6[s][0];
				int ny = ty + adjOffsets6[s][1];
				int nz = tz + adjOffsets6[s][2];
				if (World_Contains(nx, ny, nz) && Redstone_BlockStronglyPowered(nx, ny, nz)) {
					adjPowered = true;
					break;
				}
			}
			if (adjPowered) {
				tnt_count--;
				tnt_positions[i] = tnt_positions[tnt_count];
				TNT_Explode(tx, ty, tz);
				continue;
			}
		}
		i++;
	}
}

/* Process TNT fuse timers - called once per physics tick */
static void Redstone_TickTNTFuse(void) {
	int i;
	
	/* Decrement all timers */
	for (i = 0; i < tnt_fuseCount; i++) {
		tnt_fuseQueue[i].ticksLeft--;
	}
	
	/* Process entries that are ready */
	i = 0;
	while (i < tnt_fuseCount) {
		if (tnt_fuseQueue[i].ticksLeft <= 0) {
			int fx = tnt_fuseQueue[i].x;
			int fy = tnt_fuseQueue[i].y;
			int fz = tnt_fuseQueue[i].z;
			
			/* Remove fuse entry */
			tnt_fuseCount--;
			tnt_fuseQueue[i] = tnt_fuseQueue[tnt_fuseCount];
			
			/* Explode (TNT_Explode handles removing the block) */
			TNT_Explode(fx, fy, fz);
			/* don't increment - recheck swapped entry */
		} else {
			i++;
		}
	}
}

/* Physics handlers for TNT placement/deletion */
static void Physics_PlaceTnt(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);
	TNT_Register(x, y, z);
}

static void Physics_DeleteTnt(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);
	TNT_Unregister(x, y, z);
	Physics_ActivateNeighbours(x, y, z, index);
}


/*########################################################################################################################*
*----------------------------------------------------Double Chest Physics-------------------------------------------------*
*#########################################################################################################################*/
/* Given a double chest block, find partner offset and revert partner to single chest */
static void Physics_DeleteDoubleChest(int index, BlockID block) {
	int x, y, z;
	int px, pz;
	BlockID partner;
	World_Unpack(index, x, y, z);
	
	px = x; pz = z;
	/* Determine partner position based on variant */
	switch (block) {
		case BLOCK_DCHEST_S_L: px = x - 1; break; /* Left(+X), partner is Right(-X) => -1 */
		case BLOCK_DCHEST_S_R: px = x + 1; break; /* Right(-X), partner is Left(+X) => +1 */
		case BLOCK_DCHEST_N_L: px = x + 1; break; /* Left(-X), partner is Right(+X) => +1 */
		case BLOCK_DCHEST_N_R: px = x - 1; break; /* Right(+X), partner is Left(-X) => -1 */
		case BLOCK_DCHEST_E_L: pz = z + 1; break; /* Left(-Z), partner is Right(+Z) => +1 */
		case BLOCK_DCHEST_E_R: pz = z - 1; break; /* Right(+Z), partner is Left(-Z) => -1 */
		case BLOCK_DCHEST_W_L: pz = z - 1; break; /* Left(+Z), partner is Right(-Z) => -1 */
		case BLOCK_DCHEST_W_R: pz = z + 1; break; /* Right(-Z), partner is Left(+Z) => +1 */
	}
	
	if (World_Contains(px, y, pz)) {
		partner = World_GetBlock(px, y, pz);
		if (partner >= BLOCK_DCHEST_S_L && partner <= BLOCK_DCHEST_W_R) {
			Game_UpdateBlock(px, y, pz, BLOCK_CHEST);
		}
	}
}


/*########################################################################################################################*
*---------------------------------------------------------Fire------------------------------------------------------------*
*#########################################################################################################################*/

/* Fire spread delay: ~40 ticks (2 seconds) encoded in upper bits */
#define FIRE_SPREAD_DELAY 40

static cc_bool Block_IsFlammable(BlockID b) {
	return b == BLOCK_WOOD || b == BLOCK_LOG || b == BLOCK_LEAVES ||
	       b == BLOCK_BOOKSHELF || b == BLOCK_COBWEB ||
	       (b >= BLOCK_RED && b <= BLOCK_WHITE); /* All wool colors */
}

static cc_bool Fire_HasFlammableNeighbor(int x, int y, int z) {
	if (x > 0          && Block_IsFlammable(World_GetBlock(x - 1, y, z))) return true;
	if (x < World.MaxX && Block_IsFlammable(World_GetBlock(x + 1, y, z))) return true;
	if (z > 0          && Block_IsFlammable(World_GetBlock(x, y, z - 1))) return true;
	if (z < World.MaxZ && Block_IsFlammable(World_GetBlock(x, y, z + 1))) return true;
	if (y > 0          && Block_IsFlammable(World_GetBlock(x, y - 1, z))) return true;
	if (y < World.MaxY && Block_IsFlammable(World_GetBlock(x, y + 1, z))) return true;
	return false;
}

/* Try to place fire in an air block adjacent to a flammable surface */
static void Fire_TrySpread(int x, int y, int z) {
	BlockID b;
	if (!World_Contains(x, y, z)) return;
	b = World_GetBlock(x, y, z);
	if (b != BLOCK_AIR) return;

	/* Only spread here if there's a flammable block next to this air space */
	if (!Fire_HasFlammableNeighbor(x, y, z)) return;

	Game_UpdateBlock(x, y, z, BLOCK_FIRE);
	/* Enqueue the new fire for future ticking */
	{
		cc_uint32 item = (cc_uint32)World_Pack(x, y, z);
		item |= ((cc_uint32)FIRE_SPREAD_DELAY << 27);
		TickQueue_Enqueue(&fireQ, item);
	}
}

/* Try to consume (destroy) a flammable neighbor */
static void Fire_TryConsume(int x, int y, int z) {
	BlockID b;
	if (!World_Contains(x, y, z)) return;
	b = World_GetBlock(x, y, z);

	if (b == BLOCK_TNT) {
		/* Ignite TNT instead of destroying it */
		TNT_ScheduleFuse(x, y, z);
		return;
	}

	if (Block_IsFlammable(b)) {
		/* Replace flammable block with fire (which will eventually burn out) */
		Game_UpdateBlock(x, y, z, BLOCK_FIRE);
	}
}

static void Physics_HandleFire(int index, BlockID block) {
	int x, y, z;
	BlockID below;
	cc_bool hasFuel, onSolid;
	int spreadDir;

	World_Unpack(index, x, y, z);

	below  = (y > 0) ? World_GetBlock(x, y - 1, z) : BLOCK_AIR;
	onSolid = Blocks.Collide[below] == COLLIDE_SOLID;
	hasFuel = Fire_HasFlammableNeighbor(x, y, z);

	/* Fire with no fuel and not on a solid block burns out quickly */
	if (!hasFuel && !onSolid) {
		Game_UpdateBlock(x, y, z, BLOCK_AIR);
		return;
	}

	/* Fire with no fuel but on solid ground burns out fairly fast */
	if (!hasFuel) {
		if (Random_Next(&physics_rnd, 4) == 0) {
			Game_UpdateBlock(x, y, z, BLOCK_AIR);
		}
		return;
	}

	/* Fire with fuel: small chance to burn out */
	if (Random_Next(&physics_rnd, 50) == 0) {
		Game_UpdateBlock(x, y, z, BLOCK_AIR);
		return;
	}

	/* Try to spread fire to adjacent air blocks */
	spreadDir = Random_Next(&physics_rnd, 6);
	switch (spreadDir) {
		case 0: Fire_TrySpread(x - 1, y, z); break;
		case 1: Fire_TrySpread(x + 1, y, z); break;
		case 2: Fire_TrySpread(x, y - 1, z); break;
		case 3: Fire_TrySpread(x, y + 1, z); break;
		case 4: Fire_TrySpread(x, y, z - 1); break;
		case 5: Fire_TrySpread(x, y, z + 1); break;
	}

	/* Try to consume an adjacent flammable block */
	if (Random_Next(&physics_rnd, 3) == 0) {
		spreadDir = Random_Next(&physics_rnd, 6);
		switch (spreadDir) {
			case 0: Fire_TryConsume(x - 1, y, z); break;
			case 1: Fire_TryConsume(x + 1, y, z); break;
			case 2: Fire_TryConsume(x, y - 1, z); break;
			case 3: Fire_TryConsume(x, y + 1, z); break;
			case 4: Fire_TryConsume(x, y, z - 1); break;
			case 5: Fire_TryConsume(x, y, z + 1); break;
		}
	}

	/* Re-enqueue this fire block for continued ticking */
	{
		cc_uint32 item = (cc_uint32)index;
		item |= ((cc_uint32)FIRE_SPREAD_DELAY << 27);
		TickQueue_Enqueue(&fireQ, item);
	}
}

/* When fire is placed, enqueue it for ticking */
static void Physics_PlaceFire(int index, BlockID block) {
	int x, y, z;
	cc_uint32 item;
	World_Unpack(index, x, y, z);
	Physics_ActivateNeighbours(x, y, z, index);

	item = (cc_uint32)index;
	item |= ((cc_uint32)FIRE_SPREAD_DELAY << 27);
	TickQueue_Enqueue(&fireQ, item);
}

/* When fire is deleted, activate neighbors */
static void Physics_DeleteFire(int index, BlockID block) {
	int x, y, z;
	World_Unpack(index, x, y, z);
	Physics_ActivateNeighbours(x, y, z, index);
}

/* Fire breaks when the block below it is removed (if floating) */
static void Physics_ActivateFire(int index, BlockID block) {
	int x, y, z;
	BlockID below;

	World_Unpack(index, x, y, z);
	below = (y > 0) ? World_GetBlock(x, y - 1, z) : BLOCK_AIR;

	/* Fire stays if it has a solid block below or a flammable neighbor */
	if (Blocks.Collide[below] == COLLIDE_SOLID) return;
	if (Fire_HasFlammableNeighbor(x, y, z)) return;

	/* No support - fire goes out */
	Game_UpdateBlock(x, y, z, BLOCK_AIR);
}

static void Physics_TickFire(void) {
	cc_uint32 item;
	int index, count;
	BlockID block;
	cc_uint32 delay;

	count = fireQ.count;
	while (count > 0) {
		item = TickQueue_Dequeue(&fireQ);
		count--;

		delay = (item & PHYSICS_DELAY_MASK) >> 27;
		index = (int)(item & ~PHYSICS_DELAY_MASK);

		if (index < 0 || index >= World.Volume) continue;

		block = World.Blocks[index];
		if (block != BLOCK_FIRE) continue;

		if (delay > 0) {
			/* Re-enqueue with reduced delay */
			item = (cc_uint32)index | ((delay - 1) << 27);
			TickQueue_Enqueue(&fireQ, item);
			continue;
		}

		Physics_HandleFire(index, block);
	}
}


/*########################################################################################################################*
*-------------------------------------------------Flow vector (MC Alpha)--------------------------------------------------*
*#########################################################################################################################*/
/* Get the effective flow decay level for flow vector calculation.
   Same liquid: if rawLevel >= 8 (falling), treat as 0 (source). Otherwise rawLevel.
   Not same liquid: returns -1. */
static int Physics_GetEffectiveFlowDecay(int x, int y, int z, cc_bool isWater) {
	BlockID b;
	cc_bool sameLiquid;
	int rawLevel;
	if (!World_Contains(x, y, z)) return -1;
	b = World_GetBlock(x, y, z);
	sameLiquid = isWater ? (b == BLOCK_WATER || b == BLOCK_STILL_WATER)
	                     : (b == BLOCK_LAVA  || b == BLOCK_STILL_LAVA);
	if (!sameLiquid) return -1;
	rawLevel = Physics.FlowLevels ? Physics.FlowLevels[World_Pack(x, y, z)] : 0;
	return (rawLevel >= 8) ? 0 : rawLevel;
}

void Physics_GetFlowVector(int x, int y, int z, cc_bool isWater, Vec3* result) {
	int myDecay, adjDecay;
	float len;
	BlockID belowBlock;
	cc_bool belowSame;
	result->x = 0; result->y = 0; result->z = 0;
	if (!Physics.FiniteLiquid || !Physics.FlowLevels) return;

	myDecay = Physics_GetEffectiveFlowDecay(x, y, z, isWater);
	if (myDecay < 0) return;

	/* Check 4 cardinal neighbors */
	/* -X direction */
	adjDecay = Physics_GetEffectiveFlowDecay(x - 1, y, z, isWater);
	if (adjDecay >= 0) {
		result->x -= (float)(adjDecay - myDecay);
	} else if (World_Contains(x - 1, y, z) && Blocks.Collide[World_GetBlock(x - 1, y, z)] < COLLIDE_SOLID) {
		adjDecay = Physics_GetEffectiveFlowDecay(x - 1, y - 1, z, isWater);
		if (adjDecay >= 0) result->x -= (float)(adjDecay - (myDecay - 8));
	}
	/* +X direction */
	adjDecay = Physics_GetEffectiveFlowDecay(x + 1, y, z, isWater);
	if (adjDecay >= 0) {
		result->x += (float)(adjDecay - myDecay);
	} else if (World_Contains(x + 1, y, z) && Blocks.Collide[World_GetBlock(x + 1, y, z)] < COLLIDE_SOLID) {
		adjDecay = Physics_GetEffectiveFlowDecay(x + 1, y - 1, z, isWater);
		if (adjDecay >= 0) result->x += (float)(adjDecay - (myDecay - 8));
	}
	/* -Z direction */
	adjDecay = Physics_GetEffectiveFlowDecay(x, y, z - 1, isWater);
	if (adjDecay >= 0) {
		result->z -= (float)(adjDecay - myDecay);
	} else if (World_Contains(x, y, z - 1) && Blocks.Collide[World_GetBlock(x, y, z - 1)] < COLLIDE_SOLID) {
		adjDecay = Physics_GetEffectiveFlowDecay(x, y - 1, z - 1, isWater);
		if (adjDecay >= 0) result->z -= (float)(adjDecay - (myDecay - 8));
	}
	/* +Z direction */
	adjDecay = Physics_GetEffectiveFlowDecay(x, y, z + 1, isWater);
	if (adjDecay >= 0) {
		result->z += (float)(adjDecay - myDecay);
	} else if (World_Contains(x, y, z + 1) && Blocks.Collide[World_GetBlock(x, y, z + 1)] < COLLIDE_SOLID) {
		adjDecay = Physics_GetEffectiveFlowDecay(x, y - 1, z + 1, isWater);
		if (adjDecay >= 0) result->z += (float)(adjDecay - (myDecay - 8));
	}

	/* Falling liquid gets downward push */
	if (Physics.FlowLevels[World_Pack(x, y, z)] >= 8) {
		/* Check if any horizontal neighbor is blocking (MC Alpha: checks for solid face) */
		/* Simplified: if falling liquid, bias downward */
		result->y -= 6.0f;
	}

	/* Normalize */
	len = Math_SqrtF(result->x * result->x + result->y * result->y + result->z * result->z);
	if (len > 0.0001f) {
		result->x /= len;
		result->y /= len;
		result->z /= len;
	}
}


void Physics_Init(void) {
	Event_Register_(&WorldEvents.MapLoaded,    NULL, Physics_OnNewMapLoaded);
	Physics.Enabled = Options_GetBool(OPT_BLOCK_PHYSICS, true);
	Physics.FiniteLiquid = Options_GetBool(OPT_FINITE_LIQUID, false);
	TickQueue_Init(&lavaQ);
	TickQueue_Init(&waterQ);
	TickQueue_Init(&fireQ);
	TickQueue_Init(&wheatQ);

	Physics.OnPlace[BLOCK_SAND]        = Physics_DoFalling;
	Physics.OnPlace[BLOCK_GRAVEL]      = Physics_DoFalling;
	Physics.OnActivate[BLOCK_SAND]     = Physics_DoFalling;
	Physics.OnActivate[BLOCK_GRAVEL]   = Physics_DoFalling;
	Physics.OnRandomTick[BLOCK_SAND]   = Physics_DoFalling;
	Physics.OnRandomTick[BLOCK_GRAVEL] = Physics_DoFalling;

	Physics.OnRandomTick[BLOCK_SAPLING] = Physics_HandleSapling;
	Physics.OnRandomTick[BLOCK_DIRT]    = Physics_HandleDirt;
	Physics.OnRandomTick[BLOCK_GRASS]   = Physics_HandleGrass;
	Physics.OnActivate[BLOCK_GRASS]     = Physics_HandleGrassActivate;
	Physics.OnActivate[BLOCK_SNOWY_GRASS]     = Physics_HandleSnowyGrassActivate;
	Physics.OnRandomTick[BLOCK_SNOWY_GRASS]   = Physics_HandleSnowyGrass;

	/* Farmland: hydration checks */
	Physics.OnRandomTick[BLOCK_FARMLAND_DRY]  = Physics_HandleFarmlandDry;
	Physics.OnRandomTick[BLOCK_FARMLAND_WET]  = Physics_HandleFarmlandWet;

	/* Wheat crops: queue-based growth, break if unsupported */
	{ int wi;
	  for (wi = BLOCK_WHEAT_0; wi <= BLOCK_WHEAT_7; wi++) {
		Physics.OnPlace[wi]    = Physics_PlaceWheat;
		Physics.OnActivate[wi] = Physics_HandleWheatActivate;
	  }
	}

	/* Fire: spread, burn out, support checks */
	Physics.OnRandomTick[BLOCK_FIRE]   = Physics_HandleFire;
	Physics.OnPlace[BLOCK_FIRE]        = Physics_PlaceFire;
	Physics.OnDelete[BLOCK_FIRE]       = Physics_DeleteFire;
	Physics.OnActivate[BLOCK_FIRE]     = Physics_ActivateFire;

	Physics.OnRandomTick[BLOCK_DANDELION]    = Physics_HandleFlower;
	Physics.OnRandomTick[BLOCK_ROSE]         = Physics_HandleFlower;
	Physics.OnRandomTick[BLOCK_RED_SHROOM]   = Physics_HandleMushroom;
	Physics.OnRandomTick[BLOCK_BROWN_SHROOM] = Physics_HandleMushroom;

	/* Flowers, mushrooms, saplings: check ground support on neighbor changes */
	Physics.OnActivate[BLOCK_DANDELION]    = Physics_HandleFlowerActivate;
	Physics.OnActivate[BLOCK_ROSE]         = Physics_HandleFlowerActivate;
	Physics.OnActivate[BLOCK_RED_SHROOM]   = Physics_HandleMushroomActivate;
	Physics.OnActivate[BLOCK_BROWN_SHROOM] = Physics_HandleMushroomActivate;
	Physics.OnActivate[BLOCK_SAPLING]      = Physics_HandleSaplingActivate;

	Physics_RegisterLiquidHandlers();

	Physics.OnPlace[BLOCK_SLAB]        = Physics_HandleSlab;
	if (Game_ClassicMode) return;
	/* BLOCK_COBBLE_SLAB physics removed - block is now invalid */
	Physics.OnPlace[BLOCK_TNT]         = Physics_PlaceTnt;
	Physics.OnDelete[BLOCK_TNT]        = Physics_DeleteTnt;
	
	/* Ladder breaks when support block is removed */
	Physics.OnActivate[BLOCK_LADDER]   = Physics_HandleLadder;
	
	/* Torch breaks when all support blocks are removed */
	Physics.OnActivate[BLOCK_TORCH]    = Physics_HandleTorch;
	
	/* Snow breaks when block below is removed */
	Physics.OnActivate[BLOCK_SNOW]     = Physics_HandleSnow;
	
	/* Ice turns into water when broken */
	Physics.OnDelete[BLOCK_ICE]        = Physics_DeleteIce;
	
	/* Red Ore Torch: support check + redstone power logic */
	Physics.OnActivate[BLOCK_RED_ORE_TORCH]     = Physics_HandleRedstoneTorch;
	Physics.OnPlace[BLOCK_RED_ORE_TORCH]         = Physics_PlaceRedstoneTorch;
	Physics.OnDelete[BLOCK_RED_ORE_TORCH]        = Physics_DeleteRedstoneTorch;
	
	/* Unlit Red Ore Torch: same handlers */
	Physics.OnActivate[BLOCK_RED_ORE_TORCH_OFF]  = Physics_HandleRedstoneTorch;
	Physics.OnPlace[BLOCK_RED_ORE_TORCH_OFF]     = Physics_PlaceRedstoneTorch;
	Physics.OnDelete[BLOCK_RED_ORE_TORCH_OFF]    = Physics_DeleteRedstoneTorch;
	
	/* Wall-mounted redstone torch variants (ON) */
	Physics.OnActivate[BLOCK_RED_TORCH_ON_S]  = Physics_HandleRedstoneTorch;
	Physics.OnPlace[BLOCK_RED_TORCH_ON_S]     = Physics_PlaceRedstoneTorch;
	Physics.OnDelete[BLOCK_RED_TORCH_ON_S]    = Physics_DeleteRedstoneTorch;
	Physics.OnActivate[BLOCK_RED_TORCH_ON_N]  = Physics_HandleRedstoneTorch;
	Physics.OnPlace[BLOCK_RED_TORCH_ON_N]     = Physics_PlaceRedstoneTorch;
	Physics.OnDelete[BLOCK_RED_TORCH_ON_N]    = Physics_DeleteRedstoneTorch;
	Physics.OnActivate[BLOCK_RED_TORCH_ON_E]  = Physics_HandleRedstoneTorch;
	Physics.OnPlace[BLOCK_RED_TORCH_ON_E]     = Physics_PlaceRedstoneTorch;
	Physics.OnDelete[BLOCK_RED_TORCH_ON_E]    = Physics_DeleteRedstoneTorch;
	Physics.OnActivate[BLOCK_RED_TORCH_ON_W]  = Physics_HandleRedstoneTorch;
	Physics.OnPlace[BLOCK_RED_TORCH_ON_W]     = Physics_PlaceRedstoneTorch;
	Physics.OnDelete[BLOCK_RED_TORCH_ON_W]    = Physics_DeleteRedstoneTorch;
	
	/* Wall-mounted redstone torch variants (OFF) */
	Physics.OnActivate[BLOCK_RED_TORCH_OFF_S]  = Physics_HandleRedstoneTorch;
	Physics.OnPlace[BLOCK_RED_TORCH_OFF_S]     = Physics_PlaceRedstoneTorch;
	Physics.OnDelete[BLOCK_RED_TORCH_OFF_S]    = Physics_DeleteRedstoneTorch;
	Physics.OnActivate[BLOCK_RED_TORCH_OFF_N]  = Physics_HandleRedstoneTorch;
	Physics.OnPlace[BLOCK_RED_TORCH_OFF_N]     = Physics_PlaceRedstoneTorch;
	Physics.OnDelete[BLOCK_RED_TORCH_OFF_N]    = Physics_DeleteRedstoneTorch;
	Physics.OnActivate[BLOCK_RED_TORCH_OFF_E]  = Physics_HandleRedstoneTorch;
	Physics.OnPlace[BLOCK_RED_TORCH_OFF_E]     = Physics_PlaceRedstoneTorch;
	Physics.OnDelete[BLOCK_RED_TORCH_OFF_E]    = Physics_DeleteRedstoneTorch;
	Physics.OnActivate[BLOCK_RED_TORCH_OFF_W]  = Physics_HandleRedstoneTorch;
	Physics.OnPlace[BLOCK_RED_TORCH_OFF_W]     = Physics_PlaceRedstoneTorch;
	Physics.OnDelete[BLOCK_RED_TORCH_OFF_W]    = Physics_DeleteRedstoneTorch;
	
	/* Unmounted (free-standing) redstone torch variants */
	Physics.OnActivate[BLOCK_RED_TORCH_UNMOUNTED]      = Physics_HandleRedstoneTorch;
	Physics.OnPlace[BLOCK_RED_TORCH_UNMOUNTED]          = Physics_PlaceRedstoneTorch;
	Physics.OnDelete[BLOCK_RED_TORCH_UNMOUNTED]         = Physics_DeleteRedstoneTorch;
	Physics.OnActivate[BLOCK_RED_TORCH_UNMOUNTED_OFF]   = Physics_HandleRedstoneTorch;
	Physics.OnPlace[BLOCK_RED_TORCH_UNMOUNTED_OFF]      = Physics_PlaceRedstoneTorch;
	Physics.OnDelete[BLOCK_RED_TORCH_UNMOUNTED_OFF]     = Physics_DeleteRedstoneTorch;
	
	/* Red ore dust: support check + power propagation */
	Physics.OnActivate[BLOCK_RED_ORE_DUST] = Physics_HandleRedOreDust;
	Physics.OnPlace[BLOCK_RED_ORE_DUST]    = Physics_PlaceRedOreDust;
	Physics.OnDelete[BLOCK_RED_ORE_DUST]   = Physics_DeleteRedOreDust;
	
	/* Lit red ore dust: same handlers */
	Physics.OnActivate[BLOCK_LIT_RED_ORE_DUST] = Physics_HandleLitRedOreDust;
	Physics.OnPlace[BLOCK_LIT_RED_ORE_DUST]    = Physics_PlaceRedOreDust;
	Physics.OnDelete[BLOCK_LIT_RED_ORE_DUST]   = Physics_DeleteRedOreDust;
	
	/* Button variants (unpressed) */
	Physics.OnActivate[BLOCK_BUTTON]  = Physics_HandleButton;
	Physics.OnPlace[BLOCK_BUTTON]     = Physics_PlaceButton;
	Physics.OnDelete[BLOCK_BUTTON]    = Physics_DeleteButton;
	
	/* Button variants (pressed) */
	Physics.OnActivate[BLOCK_BUTTON_PRESSED]  = Physics_HandleButton;
	Physics.OnPlace[BLOCK_BUTTON_PRESSED]     = Physics_PlaceButtonPressed;
	Physics.OnDelete[BLOCK_BUTTON_PRESSED]    = Physics_DeleteButton;
	
	/* Lever variants (off) */
	Physics.OnActivate[BLOCK_LEVER]  = Physics_HandleLever;
	Physics.OnPlace[BLOCK_LEVER]     = Physics_PlaceLever;
	Physics.OnDelete[BLOCK_LEVER]    = Physics_DeleteLever;
	
	/* Lever variants (on) */
	Physics.OnActivate[BLOCK_LEVER_ON]  = Physics_HandleLever;
	Physics.OnPlace[BLOCK_LEVER_ON]     = Physics_PlaceLeverOn;
	Physics.OnDelete[BLOCK_LEVER_ON]    = Physics_DeleteLever;
	
	/* Pressure plate variants (unpressed) */
	Physics.OnActivate[BLOCK_PRESSURE_PLATE]  = Physics_HandlePlate;
	Physics.OnPlace[BLOCK_PRESSURE_PLATE]     = Physics_PlacePlate;
	Physics.OnDelete[BLOCK_PRESSURE_PLATE]    = Physics_DeletePlate;
	
	/* Pressure plate variants (pressed) */
	Physics.OnActivate[BLOCK_PRESSURE_PLATE_PRESSED]  = Physics_HandlePlate;
	Physics.OnPlace[BLOCK_PRESSURE_PLATE_PRESSED]     = Physics_PlacePlatePressed;
	Physics.OnDelete[BLOCK_PRESSURE_PLATE_PRESSED]    = Physics_DeletePlate;
	
	/* Stone pressure plate variants (unpressed) */
	Physics.OnActivate[BLOCK_STONE_PLATE]  = Physics_HandleStonePlate;
	Physics.OnPlace[BLOCK_STONE_PLATE]     = Physics_PlacePlate;
	Physics.OnDelete[BLOCK_STONE_PLATE]    = Physics_DeletePlate;
	
	/* Stone pressure plate variants (pressed) */
	Physics.OnActivate[BLOCK_STONE_PLATE_PRESSED]  = Physics_HandleStonePlate;
	Physics.OnPlace[BLOCK_STONE_PLATE_PRESSED]     = Physics_PlacePlatePressed;
	Physics.OnDelete[BLOCK_STONE_PLATE_PRESSED]    = Physics_DeletePlate;
	
	/* Iron door variants - register/unregister tracking on place/delete */
	Physics.OnPlace[BLOCK_IRON_DOOR]                  = Physics_PlaceIronDoor;
	Physics.OnDelete[BLOCK_IRON_DOOR]                 = Physics_DeleteIronDoor;
	Physics.OnPlace[BLOCK_IRON_DOOR_EW_BOTTOM]        = Physics_PlaceIronDoor;
	Physics.OnDelete[BLOCK_IRON_DOOR_EW_BOTTOM]       = Physics_DeleteIronDoor;
	Physics.OnDelete[BLOCK_IRON_DOOR_NS_OPEN_BOTTOM]  = Physics_DeleteIronDoor;
	Physics.OnDelete[BLOCK_IRON_DOOR_EW_OPEN_BOTTOM]  = Physics_DeleteIronDoor;
	/* Top halves just need delete to activate neighbors */
	Physics.OnDelete[BLOCK_IRON_DOOR_NS_TOP]          = Physics_DeleteIronDoor;
	Physics.OnDelete[BLOCK_IRON_DOOR_EW_TOP]          = Physics_DeleteIronDoor;
	Physics.OnDelete[BLOCK_IRON_DOOR_NS_OPEN_TOP]     = Physics_DeleteIronDoor;
	Physics.OnDelete[BLOCK_IRON_DOOR_EW_OPEN_TOP]     = Physics_DeleteIronDoor;
	
	/* Double chest deletion - revert partner to single chest */
	Physics.OnDelete[BLOCK_DCHEST_S_L] = Physics_DeleteDoubleChest;
	Physics.OnDelete[BLOCK_DCHEST_S_R] = Physics_DeleteDoubleChest;
	Physics.OnDelete[BLOCK_DCHEST_N_L] = Physics_DeleteDoubleChest;
	Physics.OnDelete[BLOCK_DCHEST_N_R] = Physics_DeleteDoubleChest;
	Physics.OnDelete[BLOCK_DCHEST_E_L] = Physics_DeleteDoubleChest;
	Physics.OnDelete[BLOCK_DCHEST_E_R] = Physics_DeleteDoubleChest;
	Physics.OnDelete[BLOCK_DCHEST_W_L] = Physics_DeleteDoubleChest;
	Physics.OnDelete[BLOCK_DCHEST_W_R] = Physics_DeleteDoubleChest;
}

void Physics_Free(void) {
	Event_Unregister_(&WorldEvents.MapLoaded,    NULL, Physics_OnNewMapLoaded);
	Redstone_FreeVisited();
	if (physics_flowLevels) { Mem_Free(physics_flowLevels); physics_flowLevels = NULL; }
	physics_flowLevelsSize = 0;
	Physics.FlowLevels = NULL;
}

void Physics_Tick(void) {
	if (!Physics.Enabled || !World.Blocks) return;
	/* Pause physics when a menu is open */
	if (Gui_GetInputGrab()) return;

	/*if ((tickCount % 5) == 0) {*/
	if (Physics.FiniteLiquid) {
		FiniteLiquid_TickLava();
		FiniteLiquid_TickWater();
	} else {
		Physics_TickLava();
		Physics_TickWater();
	}
	/*}*/
	Redstone_TickTorchQueue();
	Redstone_TickButtonQueue();
	Redstone_TickPressurePlates();
	Redstone_TickPlateQueue();
	Redstone_TickIronDoors();
	Redstone_TickTNT();
	Redstone_TickTNTFuse();
	Physics_TickFire();
	Physics_TickWheat();
	physics_tickCount++;
	Physics_TickRandomBlocks();
}
