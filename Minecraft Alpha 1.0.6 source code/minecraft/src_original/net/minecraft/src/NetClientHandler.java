package net.minecraft.src;

import java.io.IOException;
import java.net.InetAddress;
import java.net.Socket;
import java.util.Random;
import net.minecraft.client.Minecraft;

public class NetClientHandler extends NetHandler {
	private boolean disconnected = false;
	private NetworkManager netManager;
	public String loginProgress;
	private Minecraft mc;
	private WorldClient worldClient;
	private boolean posUpdated = false;
	Random rand = new Random();

	public NetClientHandler(Minecraft var1, String var2, int var3) throws IOException {
		this.mc = var1;
		Socket var4 = new Socket(InetAddress.getByName(var2), var3);
		this.netManager = new NetworkManager(var4, "Client", this);
	}

	public void processReadPackets() {
		if(!this.disconnected) {
			if(this.posUpdated && this.worldClient != null && this.mc.thePlayer != null) {
				EntityPlayerSP var1 = this.mc.thePlayer;
				this.netManager.addToSendQueue(new Packet13PlayerLookMove(var1.posX, var1.posY, var1.posZ, var1.rotationYaw, var1.rotationPitch, var1.onGround));
			}

			this.netManager.processReadPackets();
		}
	}

	public void handleLogin(Packet1Handshake var1) {
		this.mc.playerController = new PlayerControllerMP(this.mc, this);
		this.worldClient = new WorldClient(this);
		this.worldClient.multiplayerWorld = true;
		this.mc.changeWorld1(this.worldClient);
		this.mc.displayGuiScreen(new GuiDownloadTerrain(this));
	}

	public void handleNamedEntitySpawn(Packet24NamedEntitySpawn var1) {
		double var2 = (double)var1.xPosition / 32.0D;
		double var4 = (double)var1.yPosition / 32.0D;
		double var6 = (double)var1.zPosition / 32.0D;
		float var8 = (float)(var1.rotation * 360) / 256.0F;
		float var9 = (float)(var1.pitch * 360) / 256.0F;
		EntityOtherPlayerMP var10 = new EntityOtherPlayerMP(this.mc.theWorld, var1.name);
		int var11 = var1.currentItem;
		if(var11 == 0) {
			var10.inventory.mainInventory[var10.inventory.currentItem] = null;
		} else {
			var10.inventory.mainInventory[var10.inventory.currentItem] = new ItemStack(var11);
		}

		var10.setPositionAndRotation(var2, var4, var6, var8, var9);
		this.worldClient.spawnEntityInWorld(var10);
		this.worldClient.addEntityToWorld(var1.entityId, var10);
	}

	public void handleEntityTeleport(Packet26EntityTeleport var1) {
		Entity var2 = this.worldClient.getEntityByID(var1.entityId);
		if(var2 != null) {
			double var3 = (double)var1.xPosition / 32.0D;
			double var5 = (double)var1.yPosition / 32.0D;
			double var7 = (double)var1.zPosition / 32.0D;
			float var9 = (float)(var1.yaw * 360) / 256.0F;
			float var10 = (float)(var1.pitch * 360) / 256.0F;
			var2.setPositionAndRotation(var3, var5, var7, var9, var10);
		}
	}

	public void handleDestroyEntity(Packet25DestroyEntity var1) {
		Entity var2 = this.worldClient.getEntityByID(var1.entityId);
		if(var2 != null) {
			this.worldClient.removeEntityFromWorld(var1.entityId);
			this.worldClient.setEntityDead(var2);
		}
	}

	public void handleFlying(Packet13PlayerLookMove var1) {
		this.mc.thePlayer.setPositionAndRotation(var1.a, var1.b, var1.c, var1.d, var1.e);
		this.mc.thePlayer.aD = 0.0F;
		this.netManager.addToSendQueue(var1);
		if(!this.posUpdated) {
			this.posUpdated = true;
			this.mc.displayGuiScreen((GuiScreen)null);
		}

	}

	public void handlePreChunk(Packet50PreChunk var1) {
		this.worldClient.doPreChunk(var1.xPosition, var1.yPosition, var1.mode);
	}

	public void handleMultiBlockChange(Packet52MultiBlockChange var1) {
		Chunk var2 = this.worldClient.getChunkFromChunkCoords(var1.xPosition, var1.zPosition);
		int var3 = var1.xPosition * 16;
		int var4 = var1.zPosition * 16;

		for(int var5 = 0; var5 < var1.size; ++var5) {
			short var6 = var1.coordinateArray[var5];
			int var7 = var1.typeArray[var5] & 255;
			byte var8 = var1.metadataArray[var5];
			int var9 = var6 >> 12 & 15;
			int var10 = var6 >> 8 & 15;
			int var11 = var6 & 255;
			var2.setBlockIDWithMetadata(var9, var11, var10, var7, var8);
			this.worldClient.markBlocksDirty(var9 + var3, var11, var10 + var4, var9 + var3, var11, var10 + var4);
		}

	}

	public void handleMapChunk(Packet51MapChunk var1) {
		this.worldClient.setChunkData(var1.xPosition, var1.yPosition, var1.zPosition, var1.xSize, var1.ySize, var1.zSize, var1.chunkData);
	}

	public void handleBlockChange(Packet53BlockChange var1) {
		this.worldClient.setBlockAndMetadataWithNotify(var1.xPosition, var1.yPosition, var1.zPosition, var1.type, var1.metadata);
	}

	public void handleKickDisconnect(Packet255KickDisconnect var1) {
		this.netManager.networkShutdown("Got kicked");
		this.disconnected = true;
		this.mc.changeWorld1((World)null);
		this.mc.displayGuiScreen(new GuiConnectFailed("Disconnected by server", var1.reason));
	}

	public void handleErrorMessage(String var1) {
		if(!this.disconnected) {
			this.disconnected = true;
			this.mc.changeWorld1((World)null);
			this.mc.displayGuiScreen(new GuiConnectFailed("Connection lost", var1));
		}
	}

	public void addToSendQueue(Packet var1) {
		if(!this.disconnected) {
			this.netManager.addToSendQueue(var1);
		}
	}

	public void handleBlockItemSwitch(Packet16BlockItemSwitch var1) {
		Entity var2 = this.worldClient.getEntityByID(var1.entityId);
		if(var2 != null) {
			EntityPlayer var3 = (EntityPlayer)var2;
			int var4 = var1.id;
			if(var4 == 0) {
				var3.inventory.mainInventory[var3.inventory.currentItem] = null;
			} else {
				var3.inventory.mainInventory[var3.inventory.currentItem] = new ItemStack(var4);
			}

		}
	}
}
