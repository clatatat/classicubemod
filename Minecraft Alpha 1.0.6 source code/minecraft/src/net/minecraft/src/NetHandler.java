package net.minecraft.src;

public class NetHandler {
	public void handleMapChunk(Packet51MapChunk var1) {
	}

	public void registerPacket(Packet var1) {
	}

	public void handleErrorMessage(String var1) {
	}

	public void handleKickDisconnect(Packet255KickDisconnect var1) {
		this.registerPacket(var1);
	}

	public void handleLogin(Packet1Handshake var1) {
		this.registerPacket(var1);
	}

	public void handleFlying(Packet13PlayerLookMove var1) {
		this.registerPacket(var1);
	}

	public void handleMultiBlockChange(Packet52MultiBlockChange var1) {
		this.registerPacket(var1);
	}

	public void handleBlockDig(Packet14BlockDig var1) {
		this.registerPacket(var1);
	}

	public void handleBlockChange(Packet53BlockChange var1) {
		this.registerPacket(var1);
	}

	public void handlePreChunk(Packet50PreChunk var1) {
		this.registerPacket(var1);
	}

	public void handleNamedEntitySpawn(Packet24NamedEntitySpawn var1) {
		this.registerPacket(var1);
	}

	public void handleEntity(Packet20Entity var1) {
		this.registerPacket(var1);
	}

	public void handleEntityTeleport(Packet26EntityTeleport var1) {
		this.registerPacket(var1);
	}

	public void handlePlace(Packet15Place var1) {
		this.registerPacket(var1);
	}

	public void handleBlockItemSwitch(Packet16BlockItemSwitch var1) {
		this.registerPacket(var1);
	}

	public void handleDestroyEntity(Packet25DestroyEntity var1) {
		this.registerPacket(var1);
	}
}
