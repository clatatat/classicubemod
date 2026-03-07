package net.minecraft.game.world;

import com.mojang.nbt.NBTTagCompound;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Random;
import net.minecraft.client.LoadingScreenRenderer;
import net.minecraft.game.entity.Entity;
import net.minecraft.game.physics.AxisAlignedBB;
import net.minecraft.game.physics.MovingObjectPosition;
import net.minecraft.game.physics.Vec3D;
import net.minecraft.game.world.block.Block;
import net.minecraft.game.world.block.tileentity.TileEntity;
import net.minecraft.game.world.chunk.Chunk;
import net.minecraft.game.world.chunk.ChunkProviderLoadOrGenerate;
import net.minecraft.game.world.chunk.IChunkProvider;
import net.minecraft.game.world.material.Material;
import net.minecraft.game.world.path.Pathfinder;
import net.minecraft.game.world.terrain.ChunkProviderGenerate;
import util.MathHelper;

public final class World {
	private List lightingToUpdate = new ArrayList();
	private List loadedEntityList = new ArrayList();
	private List unloadedEntityList = new LinkedList();
	public List loadedTileEntityList = new ArrayList();
	public long worldTime = 0L;
	private long skyColor = 10079487L;
	private long fogColor = 11587839L;
	private long cloudColor = 16777215L;
	private int skylightSubtracted = 0;
	private int updateLCG = (new Random()).nextInt();
	private int DIST_HASH_MAGIC = 1013904223;
	private static float[] lightBrightnessTable = new float[16];
	public Entity playerEntity;
	public int difficultySetting;
	public final Pathfinder pathFinder = new Pathfinder(this);
	public Random rand = new Random();
	public int spawnX;
	public int spawnY;
	public int spawnZ;
	private List worldAccesses = new ArrayList();
	private IChunkProvider chunkProvider;
	private File saveDirectory;
	private long randomSeed = 0L;
	private NBTTagCompound nbtCompoundPlayer;
	public long sizeOnDisk = 0L;

	public static NBTTagCompound getWorldNBTTag(File var0, String var1) {
		var0 = new File(var0, "saves");
		var0 = new File(var0, var1);
		if(!var0.exists()) {
			return null;
		} else {
			var0 = new File(var0, "level.dat");
			if(var0.exists()) {
				try {
					NBTTagCompound var3 = LoadingScreenRenderer.read(new FileInputStream(var0));
					var3 = var3.getCompoundTag("Data");
					return var3;
				} catch (Exception var2) {
					var2.printStackTrace();
				}
			}

			return null;
		}
	}

	public static void deleteWorld(File var0, String var1) {
		var0 = new File(var0, "saves");
		var0 = new File(var0, var1);
		if(var0.exists()) {
			deleteFiles(var0.listFiles());
			var0.delete();
		}
	}

	private static void deleteFiles(File[] var0) {
		for(int var1 = 0; var1 < var0.length; ++var1) {
			if(var0[var1].isDirectory()) {
				deleteFiles(var0[var1].listFiles());
			}

			var0[var1].delete();
		}

	}

	public World(File var1, String var2) {
		var1.mkdirs();
		this.saveDirectory = new File(var1, var2);
		this.saveDirectory.mkdirs();
		var1 = new File(this.saveDirectory, "level.dat");
		if(var1.exists()) {
			try {
				NBTTagCompound var4 = LoadingScreenRenderer.read(new FileInputStream(var1));
				var4 = var4.getCompoundTag("Data");
				this.randomSeed = var4.getLong("RandomSeed");
				this.spawnX = var4.getInt("SpawnX");
				this.spawnY = var4.getInt("SpawnY");
				this.spawnZ = var4.getInt("SpawnZ");
				this.worldTime = var4.getLong("Time");
				this.sizeOnDisk = var4.getLong("SizeOnDisk");
				this.nbtCompoundPlayer = var4.getCompoundTag("Player");
			} catch (Exception var3) {
				var3.printStackTrace();
			}
		}

		while(this.randomSeed == 0L) {
			this.randomSeed = this.rand.nextLong();
			this.spawnX = 0;
			this.spawnY = 64;
			this.spawnZ = 0;
		}

		this.chunkProvider = new ChunkProviderLoadOrGenerate(this, this.saveDirectory, new ChunkProviderGenerate(this, this.randomSeed));
		this.saveWorld(false);
	}

	public final void spawnPlayer() {
		if(this.nbtCompoundPlayer != null) {
			this.playerEntity.readFromNBT(this.nbtCompoundPlayer);
			this.nbtCompoundPlayer = null;
		}

	}

	private void saveWorld(boolean var1) {
		File var2 = new File(this.saveDirectory, "level.dat");
		NBTTagCompound var3 = new NBTTagCompound();
		var3.setLong("RandomSeed", this.randomSeed);
		var3.setInt("SpawnX", this.spawnX);
		var3.setInt("SpawnY", this.spawnY);
		var3.setInt("SpawnZ", this.spawnZ);
		var3.setLong("Time", this.worldTime);
		var3.setLong("SizeOnDisk", this.sizeOnDisk);
		var3.setLong("LastPlayed", System.currentTimeMillis());
		NBTTagCompound var4;
		if(this.playerEntity != null) {
			var4 = new NBTTagCompound();
			this.playerEntity.writeToNBT(var4);
			var3.setCompoundTag("Player", var4);
		}

		var4 = new NBTTagCompound();
		var4.setTag("Data", var3);

		try {
			LoadingScreenRenderer.write(var4, new FileOutputStream(var2));
		} catch (Exception var5) {
			var5.printStackTrace();
		}

		this.chunkProvider.saveChunks(var1);
	}

	public final int getBlockId(int var1, int var2, int var3) {
		return var1 >= -32000000 && var3 >= -32000000 && var1 < 32000000 && var3 <= 32000000 ? (var2 <= 0 ? Block.lavaStill.blockID : (var2 >= 128 ? 0 : this.getChunkFromChunkCoords(var1 >> 4, var3 >> 4).getBlockID(var1 & 15, var2, var3 & 15))) : 0;
	}

	public final boolean blockExists(int var1, int var2, int var3) {
		return var2 >= 0 && var2 < 128 ? this.chunkExists(var1 >> 4, var3 >> 4) : false;
	}

	private boolean chunkExists(int var1, int var2) {
		return this.chunkProvider.chunkExists(var1, var2);
	}

	private Chunk getChunkFromChunkCoords(int var1, int var2) {
		return this.chunkProvider.provideChunk(var1, var2);
	}

	public final boolean setTileNoUpdate(int var1, int var2, int var3, int var4) {
		if(var1 >= -32000000 && var3 >= -32000000 && var1 < 32000000 && var3 <= 32000000) {
			if(var2 < 0) {
				return false;
			} else if(var2 >= 128) {
				return false;
			} else {
				Chunk var5 = this.getChunkFromChunkCoords(var1 >> 4, var3 >> 4);
				return var5.setBlockID(var1 & 15, var2, var3 & 15, var4);
			}
		} else {
			return false;
		}
	}

	public final Material getBlockMaterial(int var1, int var2, int var3) {
		var1 = this.getBlockId(var1, var2, var3);
		return var1 == 0 ? Material.air : Block.blocksList[var1].blockMaterial;
	}

	public final int getBlockMetadata(int var1, int var2, int var3) {
		if(var1 >= -32000000 && var3 >= -32000000 && var1 < 32000000 && var3 <= 32000000) {
			if(var2 < 0) {
				return 0;
			} else if(var2 >= 128) {
				return 0;
			} else {
				Chunk var4 = this.getChunkFromChunkCoords(var1 >> 4, var3 >> 4);
				var1 &= 15;
				var3 &= 15;
				return var4.getBlockMetadata(var1, var2, var3);
			}
		} else {
			return 0;
		}
	}

	public final void setBlockMetadataWithNotify(int var1, int var2, int var3, int var4) {
		this.setBlockMetadata(var1, var2, var3, var4);
	}

	private boolean setBlockMetadata(int var1, int var2, int var3, int var4) {
		if(var1 >= -32000000 && var3 >= -32000000 && var1 < 32000000 && var3 <= 32000000) {
			if(var2 < 0) {
				return false;
			} else if(var2 >= 128) {
				return false;
			} else {
				Chunk var5 = this.getChunkFromChunkCoords(var1 >> 4, var3 >> 4);
				var1 &= 15;
				var3 &= 15;
				var5.setBlockMetadata(var1, var2, var3, var4);
				return true;
			}
		} else {
			return false;
		}
	}

	public final boolean setBlockWithNotify(int var1, int var2, int var3, int var4) {
		if(!this.setTileNoUpdate(var1, var2, var3, var4)) {
			return false;
		} else {
			int var5 = var4;
			var4 = var3;
			var3 = var2;
			var2 = var1;
			World var7 = this;

			for(int var6 = 0; var6 < var7.worldAccesses.size(); ++var6) {
				((IWorldAccess)var7.worldAccesses.get(var6)).markBlockAndNeighborsNeedsUpdate(var2, var3, var4);
			}

			var7.notifyBlocksOfNeighborChange(var2, var3, var4, var5);
			return true;
		}
	}

	public final void markBlocksDirtyVertical(int var1, int var2, int var3, int var4) {
		int var5;
		if(var3 > var4) {
			var5 = var4;
			var4 = var3;
			var3 = var5;
		}

		int var7 = var2;
		int var6 = var4;
		var5 = var1;
		var4 = var2;
		var3 = var3;
		var2 = var1;
		World var9 = this;

		for(int var8 = 0; var8 < var9.worldAccesses.size(); ++var8) {
			((IWorldAccess)var9.worldAccesses.get(var8)).markBlockRangeNeedsUpdate(var2, var3, var4, var5, var6, var7);
		}

	}

	public final void swap(int var1, int var2, int var3, int var4, int var5, int var6) {
		int var7 = this.getBlockId(var1, var2, var3);
		int var8 = this.getBlockMetadata(var1, var2, var3);
		int var9 = this.getBlockId(var4, var5, var6);
		int var10 = this.getBlockMetadata(var4, var5, var6);
		this.setTileNoUpdate(var1, var2, var3, var9);
		this.setBlockMetadata(var1, var2, var3, var10);
		this.setTileNoUpdate(var4, var5, var6, var7);
		this.setBlockMetadata(var4, var5, var6, var8);
		this.notifyBlocksOfNeighborChange(var1, var2, var3, var9);
		this.notifyBlocksOfNeighborChange(var4, var5, var6, var7);
	}

	public final void notifyBlocksOfNeighborChange(int var1, int var2, int var3, int var4) {
		this.notifyBlockOfNeighborChange(var1 - 1, var2, var3, var4);
		this.notifyBlockOfNeighborChange(var1 + 1, var2, var3, var4);
		this.notifyBlockOfNeighborChange(var1, var2 - 1, var3, var4);
		this.notifyBlockOfNeighborChange(var1, var2 + 1, var3, var4);
		this.notifyBlockOfNeighborChange(var1, var2, var3 - 1, var4);
		this.notifyBlockOfNeighborChange(var1, var2, var3 + 1, var4);
	}

	private void notifyBlockOfNeighborChange(int var1, int var2, int var3, int var4) {
		Block var5 = Block.blocksList[this.getBlockId(var1, var2, var3)];
		if(var5 != null) {
			var5.onNeighborBlockChange(this, var1, var2, var3, var4);
		}

	}

	public final boolean canBlockSeeTheSky(int var1, int var2, int var3) {
		return this.getChunkFromChunkCoords(var1 >> 4, var3 >> 4).canBlockSeeTheSky(var1 & 15, var2, var3 & 15);
	}

	public final int f(int var1, int var2, int var3) {
		if(var1 >= -32000000 && var3 >= -32000000 && var1 < 32000000 && var3 <= 32000000) {
			int var4 = this.getBlockId(var1, var2, var3);
			if(var4 == Block.stairSingle.blockID || var4 == Block.tilledField.blockID) {
				++var2;
			}

			if(var2 < 0) {
				return 0;
			} else if(var2 >= 128) {
				var4 = 15 - this.skylightSubtracted;
				if(var4 < 0) {
					var4 = 0;
				}

				return var4;
			} else {
				Chunk var5 = this.getChunkFromChunkCoords(var1 >> 4, var3 >> 4);
				var1 &= 15;
				var3 &= 15;
				return var5.getBlockLightValue(var1, var2, var3, this.skylightSubtracted);
			}
		} else {
			return 15;
		}
	}

	public final int getHeightValue(int var1, int var2) {
		if(var1 >= -32000000 && var2 >= -32000000 && var1 < 32000000 && var2 <= 32000000) {
			if(!this.chunkExists(var1 >> 4, var2 >> 4)) {
				return 0;
			} else {
				Chunk var3 = this.getChunkFromChunkCoords(var1 >> 4, var2 >> 4);
				return var3.getHeightValue(var1 & 15, var2 & 15);
			}
		} else {
			return 0;
		}
	}

	public final void a(EnumSkyBlock var1, int var2, int var3, int var4, int var5) {
		if(this.blockExists(var2, var3, var4) && this.getSavedLightValue(var1, var2, var3, var4) != var5) {
			this.a(var1, var2, var3, var4, var2, var3, var4);
		}

	}

	public final int getSavedLightValue(EnumSkyBlock var1, int var2, int var3, int var4) {
		if(var1 == EnumSkyBlock.Sky) {
			if(var2 < -32000000 || var4 < -32000000 || var2 >= 32000000 || var4 > 32000000) {
				return 15;
			}

			if(var3 < 0) {
				return 15;
			}

			if(var3 >= 128) {
				return 15;
			}
		} else {
			if(var2 < -32000000 || var4 < -32000000 || var2 >= 32000000 || var4 > 32000000) {
				return 0;
			}

			if(var3 < 0) {
				return 0;
			}

			if(var3 >= 128) {
				return 0;
			}
		}

		if(!this.chunkExists(var2 >> 4, var4 >> 4)) {
			return 0;
		} else {
			Chunk var5 = this.getChunkFromChunkCoords(var2 >> 4, var4 >> 4);
			var2 &= 15;
			var4 &= 15;
			return var5.getSavedLightValue(var1, var2, var3, var4);
		}
	}

	public final float getBrightness(int var1, int var2, int var3) {
		return lightBrightnessTable[this.f(var1, var2, var3)];
	}

	public final boolean isDaytime() {
		return this.skylightSubtracted < 8;
	}

	public final MovingObjectPosition rayTraceBlocks(Vec3D var1, Vec3D var2) {
		if(!Double.isNaN(var1.xCoord) && !Double.isNaN(var1.yCoord) && !Double.isNaN(var1.zCoord)) {
			if(!Double.isNaN(var2.xCoord) && !Double.isNaN(var2.yCoord) && !Double.isNaN(var2.zCoord)) {
				int var3 = MathHelper.floor_double(var2.xCoord);
				int var4 = MathHelper.floor_double(var2.yCoord);
				int var5 = MathHelper.floor_double(var2.zCoord);
				int var6 = MathHelper.floor_double(var1.xCoord);
				int var7 = MathHelper.floor_double(var1.yCoord);
				int var8 = MathHelper.floor_double(var1.zCoord);
				int var9 = 20;

				while(var9-- >= 0) {
					if(Double.isNaN(var1.xCoord) || Double.isNaN(var1.yCoord) || Double.isNaN(var1.zCoord)) {
						return null;
					}

					if(var6 == var3 && var7 == var4 && var8 == var5) {
						return null;
					}

					double var10 = 999.0D;
					double var12 = 999.0D;
					double var14 = 999.0D;
					if(var3 > var6) {
						var10 = (double)var6 + 1.0D;
					}

					if(var3 < var6) {
						var10 = (double)var6;
					}

					if(var4 > var7) {
						var12 = (double)var7 + 1.0D;
					}

					if(var4 < var7) {
						var12 = (double)var7;
					}

					if(var5 > var8) {
						var14 = (double)var8 + 1.0D;
					}

					if(var5 < var8) {
						var14 = (double)var8;
					}

					double var16 = 999.0D;
					double var18 = 999.0D;
					double var20 = 999.0D;
					double var22 = var2.xCoord - var1.xCoord;
					double var24 = var2.yCoord - var1.yCoord;
					double var26 = var2.zCoord - var1.zCoord;
					if(var10 != 999.0D) {
						var16 = (var10 - var1.xCoord) / var22;
					}

					if(var12 != 999.0D) {
						var18 = (var12 - var1.yCoord) / var24;
					}

					if(var14 != 999.0D) {
						var20 = (var14 - var1.zCoord) / var26;
					}

					byte var28;
					if(var16 < var18 && var16 < var20) {
						if(var3 > var6) {
							var28 = 4;
						} else {
							var28 = 5;
						}

						var1.xCoord = var10;
						var1.yCoord += var24 * var16;
						var1.zCoord += var26 * var16;
					} else if(var18 < var20) {
						if(var4 > var7) {
							var28 = 0;
						} else {
							var28 = 1;
						}

						var1.xCoord += var22 * var18;
						var1.yCoord = var12;
						var1.zCoord += var26 * var18;
					} else {
						if(var5 > var8) {
							var28 = 2;
						} else {
							var28 = 3;
						}

						var1.xCoord += var22 * var20;
						var1.yCoord += var24 * var20;
						var1.zCoord = var14;
					}

					Vec3D var29 = new Vec3D(var1.xCoord, var1.yCoord, var1.zCoord);
					var6 = (int)(var29.xCoord = (double)MathHelper.floor_double(var1.xCoord));
					if(var28 == 5) {
						--var6;
						++var29.xCoord;
					}

					var7 = (int)(var29.yCoord = (double)MathHelper.floor_double(var1.yCoord));
					if(var28 == 1) {
						--var7;
						++var29.yCoord;
					}

					var8 = (int)(var29.zCoord = (double)MathHelper.floor_double(var1.zCoord));
					if(var28 == 3) {
						--var8;
						++var29.zCoord;
					}

					int var30 = this.getBlockId(var6, var7, var8);
					Block var11 = Block.blocksList[var30];
					if(var30 > 0 && var11.isCollidable()) {
						MovingObjectPosition var31 = var11.collisionRayTrace(this, var6, var7, var8, var1, var2);
						if(var31 != null) {
							return var31;
						}
					}
				}

				return null;
			} else {
				return null;
			}
		} else {
			return null;
		}
	}

	public final void playSoundAtEntity(Entity var1, String var2, float var3, float var4) {
		for(int var5 = 0; var5 < this.worldAccesses.size(); ++var5) {
			float var6 = 16.0F;
			if(var3 > 1.0F) {
				var6 = 16.0F * var3;
			}

			if(this.playerEntity.getDistanceSqToEntity(var1) < (double)(var6 * var6)) {
				((IWorldAccess)this.worldAccesses.get(var5)).playSound(var2, var1.posX, var1.posY - (double)var1.yOffset, var1.posZ, var3, var4);
			}
		}

	}

	public final void playSoundEffect(double var1, double var3, double var5, String var7, float var8, float var9) {
		try {
			for(int var10 = 0; var10 < this.worldAccesses.size(); ++var10) {
				float var11 = 16.0F;
				if(var8 > 1.0F) {
					var11 = 16.0F * var8;
				}

				double var12 = var1 - this.playerEntity.posX;
				double var14 = var3 - this.playerEntity.posY;
				double var16 = var5 - this.playerEntity.posZ;
				if(var12 * var12 + var14 * var14 + var16 * var16 < (double)(var11 * var11)) {
					((IWorldAccess)this.worldAccesses.get(var10)).playSound(var7, var1, var3, var5, var8, var9);
				}
			}

		} catch (Exception var18) {
			var18.printStackTrace();
		}
	}

	public final void spawnParticle(String var1, double var2, double var4, double var6, double var8, double var10, double var12) {
		for(int var14 = 0; var14 < this.worldAccesses.size(); ++var14) {
			((IWorldAccess)this.worldAccesses.get(var14)).spawnParticle(var1, var2, var4, var6, var8, var10, var12);
		}

	}

	public final void spawnEntityInWorld(Entity var1) {
		this.loadedEntityList.add(var1);
	}

	public static void b(Entity var0) {
		var0.setEntityDead();
	}

	public final void addWorldAccess(IWorldAccess var1) {
		this.worldAccesses.add(var1);
	}

	public final void removeWorldAccess(IWorldAccess var1) {
		this.worldAccesses.remove(var1);
	}

	public final List getCollidingBoundingBoxes(AxisAlignedBB var1) {
		ArrayList var2 = new ArrayList();
		int var3 = MathHelper.floor_double(var1.minX);
		int var4 = MathHelper.floor_double(var1.maxX + 1.0D);
		int var5 = MathHelper.floor_double(var1.minY);
		int var6 = MathHelper.floor_double(var1.maxY + 1.0D);
		int var7 = MathHelper.floor_double(var1.minZ);
		int var8 = MathHelper.floor_double(var1.maxZ + 1.0D);

		for(var3 = var3; var3 < var4; ++var3) {
			for(int var9 = var5; var9 < var6; ++var9) {
				for(int var10 = var7; var10 < var8; ++var10) {
					Block var11 = Block.blocksList[this.getBlockId(var3, var9, var10)];
					if(var11 != null) {
						AxisAlignedBB var12 = var11.getCollisionBoundingBoxFromPool(var3, var9, var10);
						if(var12 != null && var1.intersectsWith(var12)) {
							var2.add(var12);
						}
					}
				}
			}
		}

		return var2;
	}

	public final Vec3D getSkyColor(float var1) {
		var1 = this.getCelestialAngle(var1);
		var1 = MathHelper.cos(var1 * (float)Math.PI * 2.0F) * 2.0F + 0.5F;
		if(var1 < 0.0F) {
			var1 = 0.0F;
		}

		if(var1 > 1.0F) {
			var1 = 1.0F;
		}

		float var2 = (float)(this.skyColor >> 16 & 255L) / 255.0F;
		float var3 = (float)(this.skyColor >> 8 & 255L) / 255.0F;
		float var4 = (float)(this.skyColor & 255L) / 255.0F;
		var2 *= var1;
		var3 *= var1;
		var4 *= var1;
		return new Vec3D((double)var2, (double)var3, (double)var4);
	}

	public final float getCelestialAngle(float var1) {
		int var2 = (int)(this.worldTime % 24000L);
		var1 = ((float)var2 + var1) / 24000.0F - 0.15F;
		return var1;
	}

	public final Vec3D getCloudColor(float var1) {
		var1 = this.getCelestialAngle(var1);
		var1 = MathHelper.cos(var1 * (float)Math.PI * 2.0F) * 2.0F + 0.5F;
		if(var1 < 0.0F) {
			var1 = 0.0F;
		}

		if(var1 > 1.0F) {
			var1 = 1.0F;
		}

		float var2 = (float)(this.cloudColor >> 16 & 255L) / 255.0F;
		float var3 = (float)(this.cloudColor >> 8 & 255L) / 255.0F;
		float var4 = (float)(this.cloudColor & 255L) / 255.0F;
		var2 *= var1 * 0.9F + 0.1F;
		var3 *= var1 * 0.9F + 0.1F;
		var4 *= var1 * 0.85F + 0.15F;
		return new Vec3D((double)var2, (double)var3, (double)var4);
	}

	public final Vec3D getFogColor(float var1) {
		var1 = this.getCelestialAngle(var1);
		var1 = MathHelper.cos(var1 * (float)Math.PI * 2.0F) * 2.0F + 0.5F;
		if(var1 < 0.0F) {
			var1 = 0.0F;
		}

		if(var1 > 1.0F) {
			var1 = 1.0F;
		}

		float var2 = (float)(this.fogColor >> 16 & 255L) / 255.0F;
		float var3 = (float)(this.fogColor >> 8 & 255L) / 255.0F;
		float var4 = (float)(this.fogColor & 255L) / 255.0F;
		var2 *= var1 * 0.94F + 0.06F;
		var3 *= var1 * 0.94F + 0.06F;
		var4 *= var1 * 0.91F + 0.09F;
		return new Vec3D((double)var2, (double)var3, (double)var4);
	}

	public final float getStarBrightness(float var1) {
		var1 = this.getCelestialAngle(var1);
		var1 = 1.0F - (MathHelper.cos(var1 * (float)Math.PI * 2.0F) * 2.0F + 12.0F / 16.0F);
		if(var1 < 0.0F) {
			var1 = 0.0F;
		}

		if(var1 > 1.0F) {
			var1 = 1.0F;
		}

		return var1 * var1 * 0.5F;
	}

	public final void scheduleBlockUpdate(int var1, int var2, int var3, int var4) {
		NextTickListEntry var5 = new NextTickListEntry(var1, var2, var3, var4);
		if(var4 > 0) {
			var3 = Block.blocksList[var4].e();
			var5.scheduledTime = var3;
		}

		this.unloadedEntityList.add(var5);
	}

	public final void levelEntities() {
		int var1;
		for(var1 = 0; var1 < this.loadedEntityList.size(); ++var1) {
			Entity var2 = (Entity)this.loadedEntityList.get(var1);
			if(!var2.isDead) {
				var2.lastTickPosX = var2.posX;
				var2.lastTickPosY = var2.posY;
				var2.lastTickPosZ = var2.posZ;
				var2.prevRotationYaw = var2.rotationYaw;
				var2.prevRotationPitch = var2.rotationPitch;
				var2.onUpdate();
			}

			if(var2.isDead) {
				this.loadedEntityList.remove(var1--);
			}
		}

		for(var1 = 0; var1 < this.loadedTileEntityList.size(); ++var1) {
			TileEntity var3 = (TileEntity)this.loadedTileEntityList.get(var1);
			var3.updateEntity();
		}

	}

	public final boolean getIsAnyLiquid(AxisAlignedBB var1) {
		int var2 = MathHelper.floor_double(var1.minX);
		int var3 = MathHelper.floor_double(var1.maxX + 1.0D);
		int var4 = MathHelper.floor_double(var1.minY);
		int var5 = MathHelper.floor_double(var1.maxY + 1.0D);
		int var6 = MathHelper.floor_double(var1.minZ);
		int var7 = MathHelper.floor_double(var1.maxZ + 1.0D);
		if(var1.minX < 0.0D) {
			--var2;
		}

		if(var1.minY < 0.0D) {
			--var4;
		}

		if(var1.minZ < 0.0D) {
			--var6;
		}

		for(int var10 = var2; var10 < var3; ++var10) {
			for(var2 = var4; var2 < var5; ++var2) {
				for(int var8 = var6; var8 < var7; ++var8) {
					Block var9 = Block.blocksList[this.getBlockId(var10, var2, var8)];
					if(var9 != null && var9.blockMaterial.getIsLiquid()) {
						return true;
					}
				}
			}
		}

		return false;
	}

	public final boolean isBoundingBoxBurning(AxisAlignedBB var1) {
		int var2 = MathHelper.floor_double(var1.minX);
		int var3 = MathHelper.floor_double(var1.maxX + 1.0D);
		int var4 = MathHelper.floor_double(var1.minY);
		int var5 = MathHelper.floor_double(var1.maxY + 1.0D);
		int var6 = MathHelper.floor_double(var1.minZ);
		int var10 = MathHelper.floor_double(var1.maxZ + 1.0D);

		for(var2 = var2; var2 < var3; ++var2) {
			for(int var7 = var4; var7 < var5; ++var7) {
				for(int var8 = var6; var8 < var10; ++var8) {
					int var9 = this.getBlockId(var2, var7, var8);
					if(var9 == Block.fire.blockID || var9 == Block.lavaMoving.blockID || var9 == Block.lavaStill.blockID) {
						return true;
					}
				}
			}
		}

		return false;
	}

	public final boolean isMaterialInBB(AxisAlignedBB var1, Material var2) {
		int var3 = MathHelper.floor_double(var1.minX);
		int var4 = MathHelper.floor_double(var1.maxX + 1.0D);
		int var5 = MathHelper.floor_double(var1.minY);
		int var6 = MathHelper.floor_double(var1.maxY + 1.0D);
		int var7 = MathHelper.floor_double(var1.minZ);
		int var11 = MathHelper.floor_double(var1.maxZ + 1.0D);

		for(var3 = var3; var3 < var4; ++var3) {
			for(int var8 = var5; var8 < var6; ++var8) {
				for(int var9 = var7; var9 < var11; ++var9) {
					Block var10 = Block.blocksList[this.getBlockId(var3, var8, var9)];
					if(var10 != null && var10.blockMaterial == var2) {
						return true;
					}
				}
			}
		}

		return false;
	}

	public final void createExplosion(Entity var1, double var2, double var4, double var6, float var8) {
		new Explosion();
		float var3 = var8;
		double var15 = var6;
		double var13 = var4;
		double var11 = var2;
		Entity var67 = var1;
		World var66 = this;
		this.playSoundEffect(var2, var4, var6, "random.explode", 4.0F, (1.0F + (this.rand.nextFloat() - this.rand.nextFloat()) * 0.2F) * 0.7F);
		HashSet var68 = new HashSet();

		int var7;
		double var32;
		double var34;
		double var36;
		int var69;
		int var70;
		for(var69 = 0; var69 < 16; ++var69) {
			for(var7 = 0; var7 < 16; ++var7) {
				for(var70 = 0; var70 < 16; ++var70) {
					if(var69 == 0 || var69 == 15 || var7 == 0 || var7 == 15 || var70 == 0 || var70 == 15) {
						double var23 = (double)((float)var69 / 15.0F * 2.0F - 1.0F);
						double var25 = (double)((float)var7 / 15.0F * 2.0F - 1.0F);
						double var27 = (double)((float)var70 / 15.0F * 2.0F - 1.0F);
						double var29 = Math.sqrt(var23 * var23 + var25 * var25 + var27 * var27);
						var23 /= var29;
						var25 /= var29;
						var27 /= var29;
						float var31 = var3 * (0.7F + var66.rand.nextFloat() * 0.6F);
						var32 = var11;
						var34 = var13;

						for(var36 = var15; var31 > 0.0F; var31 -= 0.22500001F) {
							int var39 = MathHelper.floor_double(var32);
							int var40 = MathHelper.floor_double(var34);
							int var41 = MathHelper.floor_double(var36);
							int var42 = var66.getBlockId(var39, var40, var41);
							if(var42 > 0) {
								var31 -= (Block.blocksList[var42].getExplosionResistance() + 0.3F) * 0.3F;
							}

							if(var31 > 0.0F) {
								var68.add(new ChunkPosition(var39, var40, var41));
							}

							var32 += var23 * (double)0.3F;
							var34 += var25 * (double)0.3F;
							var36 += var27 * (double)0.3F;
						}
					}
				}
			}
		}

		var3 *= 2.0F;
		var69 = MathHelper.floor_double(var11 - (double)var3 - 1.0D);
		var7 = MathHelper.floor_double(var11 + (double)var3 + 1.0D);
		var70 = MathHelper.floor_double(var13 - (double)var3 - 1.0D);
		int var71 = MathHelper.floor_double(var13 + (double)var3 + 1.0D);
		int var24 = MathHelper.floor_double(var15 - (double)var3 - 1.0D);
		int var72 = MathHelper.floor_double(var15 + (double)var3 + 1.0D);
		List var26 = var66.a(var1, new AxisAlignedBB((double)var69, (double)var70, (double)var24, (double)var7, (double)var71, (double)var72));
		Vec3D var73 = new Vec3D(var11, var13, var15);

		double var38;
		double var65;
		double var81;
		for(int var28 = 0; var28 < var26.size(); ++var28) {
			Entity var75 = (Entity)var26.get(var28);
			double var59 = var75.posX - var11;
			double var61 = var75.posY - var13;
			double var63 = var75.posZ - var15;
			double var30 = (double)MathHelper.sqrt_double(var59 * var59 + var61 * var61 + var63 * var63) / (double)var3;
			if(var30 <= 1.0D) {
				var32 = var75.posX - var11;
				var34 = var75.posY - var13;
				var36 = var75.posZ - var15;
				var38 = (double)MathHelper.sqrt_double(var32 * var32 + var34 * var34 + var36 * var36);
				var32 /= var38;
				var34 /= var38;
				var36 /= var38;
				var81 = (double)var66.getBlockDensity(var73, var75.boundingBox);
				var65 = (1.0D - var30) * var81;
				var75.attackEntityFrom(var67, (int)((var65 * var65 + var65) / 2.0D * 8.0D * (double)var3 + 1.0D));
				var75.motionX += var32 * var65;
				var75.motionY += var34 * var65;
				var75.motionZ += var36 * var65;
			}
		}

		var3 = var8;
		ArrayList var74 = new ArrayList();
		var74.addAll(var68);

		for(int var76 = var74.size() - 1; var76 >= 0; --var76) {
			ChunkPosition var77 = (ChunkPosition)var74.get(var76);
			int var78 = var77.x;
			int var79 = var77.y;
			int var33 = var77.z;
			int var80 = var66.getBlockId(var78, var79, var33);

			for(int var35 = 0; var35 <= 0; ++var35) {
				var36 = (double)((float)var78 + var66.rand.nextFloat());
				var38 = (double)((float)var79 + var66.rand.nextFloat());
				var81 = (double)((float)var33 + var66.rand.nextFloat());
				var65 = var36 - var11;
				double var44 = var38 - var13;
				double var46 = var81 - var15;
				double var48 = (double)MathHelper.sqrt_double(var65 * var65 + var44 * var44 + var46 * var46);
				var65 /= var48;
				var44 /= var48;
				var46 /= var48;
				double var50 = 0.5D / (var48 / (double)var3 + 0.1D);
				var50 *= (double)(var66.rand.nextFloat() * var66.rand.nextFloat() + 0.3F);
				var65 *= var50;
				var44 *= var50;
				var46 *= var50;
				var66.spawnParticle("explode", (var36 + var11) / 2.0D, (var38 + var13) / 2.0D, (var81 + var15) / 2.0D, var65, var44, var46);
				var66.spawnParticle("smoke", var36, var38, var81, var65, var44, var46);
			}

			if(var80 > 0) {
				Block.blocksList[var80].dropBlockAsItemWithChance(var66, var78, var79, var33, var66.getBlockMetadata(var78, var79, var33), 0.3F);
				var66.setBlockWithNotify(var78, var79, var33, 0);
				Block.blocksList[var80].onBlockDestroyedByExplosion(var66, var78, var79, var33);
			}
		}

	}

	public final float getBlockDensity(Vec3D var1, AxisAlignedBB var2) {
		double var3 = 1.0D / ((var2.maxX - var2.minX) * 2.0D + 1.0D);
		double var5 = 1.0D / ((var2.maxY - var2.minY) * 2.0D + 1.0D);
		double var7 = 1.0D / ((var2.maxZ - var2.minZ) * 2.0D + 1.0D);
		int var9 = 0;
		int var10 = 0;

		for(float var11 = 0.0F; var11 <= 1.0F; var11 = (float)((double)var11 + var3)) {
			for(float var12 = 0.0F; var12 <= 1.0F; var12 = (float)((double)var12 + var5)) {
				for(float var13 = 0.0F; var13 <= 1.0F; var13 = (float)((double)var13 + var7)) {
					double var14 = var2.minX + (var2.maxX - var2.minX) * (double)var11;
					double var16 = var2.minY + (var2.maxY - var2.minY) * (double)var12;
					double var18 = var2.minZ + (var2.maxZ - var2.minZ) * (double)var13;
					if(this.rayTraceBlocks(new Vec3D(var14, var16, var18), var1) == null) {
						++var9;
					}

					++var10;
				}
			}
		}

		return (float)var9 / (float)var10;
	}

	public final void extinguishFire(int var1, int var2, int var3, int var4) {
		if(var4 == 0) {
			--var2;
		}

		if(var4 == 1) {
			++var2;
		}

		if(var4 == 2) {
			--var3;
		}

		if(var4 == 3) {
			++var3;
		}

		if(var4 == 4) {
			--var1;
		}

		if(var4 == 5) {
			++var1;
		}

		if(this.getBlockId(var1, var2, var3) == Block.fire.blockID) {
			this.playSoundEffect((double)((float)var1 + 0.5F), (double)((float)var2 + 0.5F), (double)((float)var3 + 0.5F), "random.fizz", 0.5F, 2.6F + (this.rand.nextFloat() - this.rand.nextFloat()) * 0.8F);
			this.setBlockWithNotify(var1, var2, var3, 0);
		}

	}

	public static String d() {
		return "";
	}

	public final Entity getPlayerEntity() {
		return this.playerEntity;
	}

	public final TileEntity getBlockTileEntity(int var1, int var2, int var3) {
		Chunk var4 = this.getChunkFromChunkCoords(var1 >> 4, var3 >> 4);
		return var4 != null ? var4.getChunkBlockTileEntity(var1 & 15, var2, var3 & 15) : null;
	}

	public final void setBlockTileEntity(int var1, int var2, int var3, TileEntity var4) {
		Chunk var5 = this.getChunkFromChunkCoords(var1 >> 4, var3 >> 4);
		if(var5 != null) {
			var5.setChunkBlockTileEntity(var1 & 15, var2, var3 & 15, var4);
		}

	}

	public final void removeBlockTileEntity(int var1, int var2, int var3) {
		Chunk var4 = this.getChunkFromChunkCoords(var1 >> 4, var3 >> 4);
		if(var4 != null) {
			var4.removeChunkBlockTileEntity(var1 & 15, var2, var3 & 15);
		}

	}

	public final boolean isSolid(int var1, int var2, int var3) {
		Block var4 = Block.blocksList[this.getBlockId(var1, var2, var3)];
		return var4 == null ? false : var4.isOpaqueCube();
	}

	public final void saveWorldIndirectly() {
		this.saveWorld(true);
	}

	public final void updatingLighting() {
		while(this.lightingToUpdate.size() > 0) {
			MetadataChunkBlock var10000 = (MetadataChunkBlock)this.lightingToUpdate.remove(this.lightingToUpdate.size() - 1);
			World var2 = this;
			MetadataChunkBlock var1 = var10000;

			for(int var3 = var1.b; var3 <= var1.e; ++var3) {
				for(int var4 = var1.d; var4 <= var1.g; ++var4) {
					if(var2.blockExists(var3, 0, var4)) {
						for(int var5 = var1.c; var5 <= var1.f; ++var5) {
							if(var5 >= 0 && var5 < 128) {
								int var6 = var2.getSavedLightValue(var1.skyBlock, var3, var5, var4);
								int var7 = var2.getBlockId(var3, var5, var4);
								int var8 = Block.lightOpacity[var7];
								if(var8 == 0) {
									var8 = 1;
								}

								int var9 = 0;
								int var11;
								int var13;
								if(var1.skyBlock == EnumSkyBlock.Sky) {
									boolean var19;
									if(var3 >= -32000000 && var4 >= -32000000 && var3 < 32000000 && var4 <= 32000000) {
										if(var5 < 0) {
											var19 = false;
										} else if(var5 >= 128) {
											var19 = true;
										} else if(!var2.chunkExists(var3 >> 4, var4 >> 4)) {
											var19 = false;
										} else {
											Chunk var14 = var2.getChunkFromChunkCoords(var3 >> 4, var4 >> 4);
											var11 = var3 & 15;
											var13 = var4 & 15;
											var19 = var14.canBlockSeeTheSky(var11, var5, var13);
										}
									} else {
										var19 = false;
									}

									if(var19) {
										var9 = 15;
									}
								} else if(var1.skyBlock == EnumSkyBlock.Block) {
									var9 = Block.lightValue[var7];
								}

								int var12;
								int var18;
								if(var8 >= 15 && var9 == 0) {
									var7 = 0;
								} else {
									var7 = var2.getSavedLightValue(var1.skyBlock, var3 - 1, var5, var4);
									int var10 = var2.getSavedLightValue(var1.skyBlock, var3 + 1, var5, var4);
									var11 = var2.getSavedLightValue(var1.skyBlock, var3, var5 - 1, var4);
									var12 = var2.getSavedLightValue(var1.skyBlock, var3, var5 + 1, var4);
									var13 = var2.getSavedLightValue(var1.skyBlock, var3, var5, var4 - 1);
									var18 = var2.getSavedLightValue(var1.skyBlock, var3, var5, var4 + 1);
									var7 = var7;
									if(var10 > var7) {
										var7 = var10;
									}

									if(var11 > var7) {
										var7 = var11;
									}

									if(var12 > var7) {
										var7 = var12;
									}

									if(var13 > var7) {
										var7 = var13;
									}

									if(var18 > var7) {
										var7 = var18;
									}

									var7 -= var8;
									if(var7 < 0) {
										var7 = 0;
									}

									if(var9 > var7) {
										var7 = var9;
									}
								}

								if(var6 != var7) {
									var18 = var4;
									var13 = var5;
									var12 = var3;
									EnumSkyBlock var17 = var1.skyBlock;
									World var16 = var2;
									if(var3 >= -32000000 && var4 >= -32000000 && var3 < 32000000 && var4 <= 32000000 && var5 >= 0 && var5 < 128 && var2.chunkExists(var3 >> 4, var4 >> 4)) {
										Chunk var15 = var2.getChunkFromChunkCoords(var3 >> 4, var4 >> 4);
										var15.setLightValue(var17, var3 & 15, var5, var4 & 15, var7);

										for(var6 = 0; var6 < var16.worldAccesses.size(); ++var6) {
											((IWorldAccess)var16.worldAccesses.get(var6)).markBlockAndNeighborsNeedsUpdate(var12, var13, var18);
										}
									}

									--var7;
									if(var7 < 0) {
										var7 = 0;
									}

									var2.a(var1.skyBlock, var3 - 1, var5, var4, var7);
									var2.a(var1.skyBlock, var3 + 1, var5, var4, var7);
									var2.a(var1.skyBlock, var3, var5 - 1, var4, var7);
									var2.a(var1.skyBlock, var3, var5 + 1, var4, var7);
									var2.a(var1.skyBlock, var3, var5, var4 - 1, var7);
									var2.a(var1.skyBlock, var3, var5, var4 + 1, var7);
								}
							}
						}
					}
				}
			}
		}

	}

	public final void a(EnumSkyBlock var1, int var2, int var3, int var4, int var5, int var6, int var7) {
		int var8 = this.lightingToUpdate.size();
		int var9 = 4;
		if(4 > var8) {
			var9 = var8;
		}

		for(var8 = 0; var8 < var9; ++var8) {
			MetadataChunkBlock var10 = (MetadataChunkBlock)this.lightingToUpdate.get(this.lightingToUpdate.size() - var8 - 1);
			if(var10.skyBlock == var1 && var2 >= var10.b && var3 >= var10.c && var4 >= var10.d && var5 <= var10.e && var6 <= var10.f && var7 <= var10.g) {
				return;
			}
		}

		this.lightingToUpdate.add(new MetadataChunkBlock(var1, var2, var3, var4, var5, var6, var7));
	}

	public final void restartTimeOfDay() {
		float var1 = 1.0F;
		var1 = this.getCelestialAngle(1.0F);
		var1 = 1.0F - (MathHelper.cos(var1 * (float)Math.PI * 2.0F) * 2.0F + 0.5F);
		if(var1 < 0.0F) {
			var1 = 0.0F;
		}

		if(var1 > 1.0F) {
			var1 = 1.0F;
		}

		int var8 = (int)(var1 * 13.0F);
		if(var8 != this.skylightSubtracted) {
			this.skylightSubtracted = var8;

			for(var8 = 0; var8 < this.worldAccesses.size(); ++var8) {
				((IWorldAccess)this.worldAccesses.get(var8)).updateAllRenderers();
			}
		}

		++this.worldTime;
		if(this.worldTime % 200L == 0L) {
			this.saveWorld(false);
		}

		var8 = this.unloadedEntityList.size();
		if(var8 > 200) {
			var8 = 200;
		}

		int var2;
		int var4;
		for(var2 = 0; var2 < var8; ++var2) {
			NextTickListEntry var3 = (NextTickListEntry)this.unloadedEntityList.remove(0);
			if(var3.scheduledTime > 0) {
				--var3.scheduledTime;
				this.unloadedEntityList.add(var3);
			} else if(this.blockExists(var3.xCoord, var3.yCoord, var3.zCoord)) {
				var4 = this.getBlockId(var3.xCoord, var3.yCoord, var3.zCoord);
				if(var4 == var3.blockID && var4 > 0) {
					Block.blocksList[var4].updateTick(this, var3.xCoord, var3.yCoord, var3.zCoord, this.rand);
				}
			}
		}

		var8 = MathHelper.floor_double(this.playerEntity.posX);
		var2 = MathHelper.floor_double(this.playerEntity.posZ);

		for(int var9 = 0; var9 < 32000; ++var9) {
			this.updateLCG = this.updateLCG * 3 + this.DIST_HASH_MAGIC;
			var4 = this.updateLCG >> 2;
			int var5 = (var4 & 255) - 128 + var8;
			int var6 = (var4 >> 8 & 255) - 128 + var2;
			var4 = var4 >> 16 & 127;
			int var7 = this.getBlockId(var5, var4, var6);
			if(Block.tickOnLoad[var7]) {
				Block.blocksList[var7].updateTick(this, var5, var4, var6, this.rand);
			}
		}

	}

	public final void randomDisplayUpdates(int var1, int var2, int var3) {
		Random var4 = new Random();

		for(int var5 = 0; var5 < 1000; ++var5) {
			int var6 = var1 + this.rand.nextInt(16) - this.rand.nextInt(16);
			int var7 = var2 + this.rand.nextInt(16) - this.rand.nextInt(16);
			int var8 = var3 + this.rand.nextInt(16) - this.rand.nextInt(16);
			int var9 = this.getBlockId(var6, var7, var8);
			if(var9 > 0) {
				Block.blocksList[var9].randomDisplayTick(this, var6, var7, var8, var4);
			}
		}

	}

	public final List a(Entity var1, AxisAlignedBB var2) {
		ArrayList var3 = new ArrayList();

		for(int var4 = 0; var4 < this.loadedEntityList.size(); ++var4) {
			Entity var5 = (Entity)this.loadedEntityList.get(var4);
			if(var5 != var1 && var5.boundingBox.intersectsWith(var2)) {
				var3.add(var5);
			}
		}

		return var3;
	}

	public final List getLoadedEntityList() {
		return this.loadedEntityList;
	}

	public final void updateTileEntityChunkAndDoNothing(int var1, int var2, int var3) {
		if(this.blockExists(var1, var2, var3)) {
			Chunk var4 = this.getChunkFromChunkCoords(var1 >> 4, var3 >> 4);
			var4.isModified = true;
		}

	}

	static {
		for(int var0 = 0; var0 <= 15; ++var0) {
			float var1 = 1.0F - (float)var0 / 15.0F;
			lightBrightnessTable[var0] = (1.0F - var1) / (var1 * 3.0F + 1.0F) * 0.95F + 0.05F;
		}

	}
}
