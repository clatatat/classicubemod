package net.minecraft.src;

import java.io.File;

public class WorldClient extends World {
	private NetClientHandler sendQueue;
	private ChunkProviderClient clientChunkProvider;
	private MCHashTable entityHashTable = new MCHashTable();

	public WorldClient(NetClientHandler var1) {
		super("MpServer");
		this.sendQueue = var1;
		this.spawnX = 8;
		this.spawnY = 64;
		this.spawnZ = 8;
	}

	public void tick() {
		this.sendQueue.processReadPackets();
	}

	protected IChunkProvider getChunkProvider(File var1) {
		this.clientChunkProvider = new ChunkProviderClient(this);
		return this.clientChunkProvider;
	}

	public void setSpawnLocation() {
		this.spawnX = 8;
		this.spawnY = 64;
		this.spawnZ = 8;
	}

	protected void updateBlocksAndPlayCaveSounds() {
	}

	public void scheduleBlockUpdate(int var1, int var2, int var3, int var4) {
	}

	public boolean tickUpdates(boolean var1) {
		return false;
	}

	public void doPreChunk(int var1, int var2, boolean var3) {
		if(var3) {
			this.clientChunkProvider.loadChunk(var1, var2);
		} else {
			this.clientChunkProvider.unloadChunk(var1, var2);
		}

	}

	public void addEntityToWorld(int var1, Entity var2) {
		this.entityHashTable.addKey(var1, var2);
	}

	public Entity getEntityByID(int var1) {
		return (Entity)this.entityHashTable.lookup(var1);
	}

	public Entity removeEntityFromWorld(int var1) {
		return (Entity)this.entityHashTable.removeObject(var1);
	}
}
