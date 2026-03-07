package net.minecraft.game.world.chunk;

import com.mojang.nbt.NBTTagCompound;
import com.mojang.nbt.NBTTagList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;
import net.minecraft.game.world.EnumSkyBlock;
import net.minecraft.game.world.World;
import net.minecraft.game.world.block.Block;
import net.minecraft.game.world.block.BlockContainer;
import net.minecraft.game.world.block.tileentity.TileEntity;

public final class Chunk {
	public static boolean isLit;
	private byte[] blocks;
	private World worldObj;
	private NibbleArray data;
	private NibbleArray skyLightMap;
	private NibbleArray blockLightMap;
	private byte[] heightMap;
	private int lowestBlockHeight;
	public final int xPosition;
	public final int zPosition;
	private Map chunkTileEntityMap;
	public boolean isTerrainPopulated;
	public boolean isModified;

	private Chunk(World var1, int var2, int var3) {
		this.chunkTileEntityMap = new HashMap();
		this.isTerrainPopulated = false;
		this.isModified = false;
		this.worldObj = var1;
		this.xPosition = var2;
		this.zPosition = var3;
		this.heightMap = new byte[256];
	}

	public Chunk(World var1, byte[] var2, int var3, int var4) {
		this(var1, var3, var4);
		this.blocks = var2;
		this.data = new NibbleArray(var2.length);
		this.skyLightMap = new NibbleArray(var2.length);
		this.blockLightMap = new NibbleArray(var2.length);
	}

	public final int getHeightValue(int var1, int var2) {
		return this.heightMap[var2 << 4 | var1] & 255;
	}

	public final void generateHeightMap() {
		int var1 = 127;

		int var2;
		int var3;
		for(var2 = 0; var2 < 16; ++var2) {
			for(var3 = 0; var3 < 16; ++var3) {
				this.heightMap[var3 << 4 | var2] = -128;
				this.relightBlock(var2, 127, var3);
				if((this.heightMap[var3 << 4 | var2] & 255) < var1) {
					var1 = this.heightMap[var3 << 4 | var2] & 255;
				}
			}
		}

		this.lowestBlockHeight = var1;

		for(var2 = 0; var2 < 16; ++var2) {
			for(var3 = 0; var3 < 16; ++var3) {
				this.updateSkylight_do(var2, var3);
			}
		}

		this.isModified = true;
	}

	private void updateSkylight_do(int var1, int var2) {
		int var3 = this.getHeightValue(var1, var2);
		var1 += this.xPosition << 4;
		var2 += this.zPosition << 4;
		this.checkSkylightNeighborHeight(var1 - 1, var2, var3);
		this.checkSkylightNeighborHeight(var1 + 1, var2, var3);
		this.checkSkylightNeighborHeight(var1, var2 - 1, var3);
		this.checkSkylightNeighborHeight(var1, var2 + 1, var3);
	}

	private void checkSkylightNeighborHeight(int var1, int var2, int var3) {
		int var4 = this.worldObj.getHeightValue(var1, var2);
		if(var4 > var3) {
			this.worldObj.a(EnumSkyBlock.Sky, var1, var3, var2, var1, var4, var2);
		} else if(var4 < var3) {
			this.worldObj.a(EnumSkyBlock.Sky, var1, var4, var2, var1, var3, var2);
		}

		this.isModified = true;
	}

	private void relightBlock(int var1, int var2, int var3) {
		int var4 = this.heightMap[var3 << 4 | var1] & 255;
		int var5 = var4;
		if(var2 > var4) {
			var5 = var2;
		}

		while(var5 > 0 && Block.lightOpacity[this.getBlockID(var1, var5 - 1, var3)] == 0) {
			--var5;
		}

		if(var5 != var4) {
			this.worldObj.markBlocksDirtyVertical(var1, var3, var5, var4);
			this.heightMap[var3 << 4 | var1] = (byte)var5;
			int var6;
			int var7;
			if(var5 < this.lowestBlockHeight) {
				this.lowestBlockHeight = var5;
			} else {
				var2 = 127;

				for(var6 = 0; var6 < 16; ++var6) {
					for(var7 = 0; var7 < 16; ++var7) {
						if((this.heightMap[var7 << 4 | var6] & 255) < var2) {
							var2 = this.heightMap[var7 << 4 | var6] & 255;
						}
					}
				}

				this.lowestBlockHeight = var2;
			}

			var2 = (this.xPosition << 4) + var1;
			var6 = (this.zPosition << 4) + var3;
			if(var5 < var4) {
				for(var7 = var5; var7 < var4; ++var7) {
					this.skyLightMap.set(var1, var7, var3, 15);
				}
			} else {
				this.worldObj.a(EnumSkyBlock.Sky, var2, var4, var6, var2, var5, var6);

				for(var7 = var4; var7 < var5; ++var7) {
					this.skyLightMap.set(var1, var7, var3, 0);
				}
			}

			var7 = 15;

			while(var5 > 0 && var7 > 0) {
				--var5;
				var4 = Block.lightOpacity[this.getBlockID(var1, var5, var3)];
				if(var4 == 0) {
					var4 = 1;
				}

				var7 -= var4;
				if(var7 < 0) {
					var7 = 0;
				}

				this.skyLightMap.set(var1, var5, var3, var7);
				this.worldObj.a(EnumSkyBlock.Sky, var2, var5, var6, -1);
			}

			this.isModified = true;
		}
	}

	public final int getBlockID(int var1, int var2, int var3) {
		return this.blocks[var1 << 11 | var3 << 7 | var2];
	}

	public final boolean setBlockID(int var1, int var2, int var3, int var4) {
		byte var5 = (byte)var4;
		int var6 = this.heightMap[var3 << 4 | var1] & 255;
		int var7 = this.blocks[var1 << 11 | var3 << 7 | var2] & 255;
		if(var7 == var4) {
			return false;
		} else {
			int var8 = (this.xPosition << 4) + var1;
			int var9 = (this.zPosition << 4) + var3;
			if(var7 != 0) {
				Block.blocksList[var7].onBlockRemoval(this.worldObj, var8, var2, var9);
			}

			this.blocks[var1 << 11 | var3 << 7 | var2] = var5;
			this.data.set(var1, var2, var3, 0);
			if(Block.lightOpacity[var5] != 0) {
				if(var2 >= var6) {
					this.relightBlock(var1, var2 + 1, var3);
				}
			} else if(var2 == var6 - 1) {
				this.relightBlock(var1, var2, var3);
			}

			this.worldObj.a(EnumSkyBlock.Sky, var8, var2, var9, var8, var2, var9);
			this.worldObj.a(EnumSkyBlock.Block, var8, var2, var9, var8, var2, var9);
			this.updateSkylight_do(var1, var3);
			if(var4 != 0) {
				Block.blocksList[var4].onBlockAdded(this.worldObj, var8, var2, var9);
			}

			this.isModified = true;
			return true;
		}
	}

	public final int getBlockMetadata(int var1, int var2, int var3) {
		return this.data.get(var1, var2, var3);
	}

	public final void setBlockMetadata(int var1, int var2, int var3, int var4) {
		this.isModified = true;
		this.data.set(var1, var2, var3, var4);
	}

	public final int getSavedLightValue(EnumSkyBlock var1, int var2, int var3, int var4) {
		return var1 == EnumSkyBlock.Sky ? this.skyLightMap.get(var2, var3, var4) : (var1 == EnumSkyBlock.Block ? this.blockLightMap.get(var2, var3, var4) : 0);
	}

	public final void setLightValue(EnumSkyBlock var1, int var2, int var3, int var4, int var5) {
		this.isModified = true;
		if(var1 == EnumSkyBlock.Sky) {
			this.skyLightMap.set(var2, var3, var4, var5);
		} else if(var1 == EnumSkyBlock.Block) {
			this.blockLightMap.set(var2, var3, var4, var5);
		}
	}

	public final int getBlockLightValue(int var1, int var2, int var3, int var4) {
		int var5 = this.skyLightMap.get(var1, var2, var3);
		if(var5 > 0) {
			isLit = true;
		}

		var5 -= var4;
		var1 = this.blockLightMap.get(var1, var2, var3);
		if(var1 > var5) {
			var5 = var1;
		}

		return var5;
	}

	public final void writeChunkNBTData(NBTTagCompound var1) {
		var1.setInt("xPos", this.xPosition);
		var1.setInt("zPos", this.zPosition);
		var1.setLong("LastUpdate", this.worldObj.worldTime);
		var1.setByteArray("Blocks", this.blocks);
		var1.setByteArray("Data", this.data.data);
		var1.setByteArray("SkyLight", this.skyLightMap.data);
		var1.setByteArray("BlockLight", this.blockLightMap.data);
		var1.setByteArray("HeightMap", this.heightMap);
		var1.setBoolean("TerrainPopulated", this.isTerrainPopulated);
		NBTTagList var2 = new NBTTagList();
		Iterator var3 = this.chunkTileEntityMap.values().iterator();

		while(var3.hasNext()) {
			TileEntity var4 = (TileEntity)var3.next();
			NBTTagCompound var5 = new NBTTagCompound();
			var4.writeToNBT(var5);
			var2.setTag(var5);
		}

		var1.setTag("TileEntities", var2);
	}

	public static Chunk readChunkNBTData(World var0, NBTTagCompound var1) {
		int var2 = var1.getInt("xPos");
		int var3 = var1.getInt("zPos");
		Chunk var8 = new Chunk(var0, var2, var3);
		var8.blocks = var1.getByteArray("Blocks");
		var8.data = new NibbleArray(var1.getByteArray("Data"));
		var8.skyLightMap = new NibbleArray(var1.getByteArray("SkyLight"));
		var8.blockLightMap = new NibbleArray(var1.getByteArray("BlockLight"));
		var8.heightMap = var1.getByteArray("HeightMap");
		var8.isTerrainPopulated = var1.getBoolean("TerrainPopulated");
		if(!var8.data.isValid()) {
			var8.data = new NibbleArray(var8.blocks.length);
		}

		if(var8.heightMap == null || !var8.skyLightMap.isValid()) {
			var8.heightMap = new byte[256];
			var8.skyLightMap = new NibbleArray(var8.blocks.length);
			var8.generateHeightMap();
		}

		if(!var8.blockLightMap.isValid()) {
			var8.blockLightMap = new NibbleArray(var8.blocks.length);
		}

		NBTTagList var9 = var1.getTagList("TileEntities");
		if(var9 != null) {
			for(var2 = 0; var2 < var9.tagCount(); ++var2) {
				NBTTagCompound var10 = (NBTTagCompound)var9.tagAt(var2);
				TileEntity var11 = TileEntity.createAndLoadEntity(var10);
				if(var11 != null) {
					int var5 = var11.xCoord - (var8.xPosition << 4);
					int var6 = var11.yCoord;
					int var7 = var11.zCoord - (var8.zPosition << 4);
					var8.setChunkBlockTileEntity(var5, var6, var7, var11);
				}
			}
		}

		return var8;
	}

	public final boolean canBlockSeeTheSky(int var1, int var2, int var3) {
		return var2 >= (this.heightMap[var3 << 4 | var1] & 255);
	}

	public final TileEntity getChunkBlockTileEntity(int var1, int var2, int var3) {
		int var4 = var1 + (var2 << 10) + (var3 << 10 << 10);
		TileEntity var5 = (TileEntity)this.chunkTileEntityMap.get(Integer.valueOf(var4));
		if(var5 == null) {
			int var6 = this.getBlockID(var1, var2, var3);
			BlockContainer var7 = (BlockContainer)Block.blocksList[var6];
			var7.onBlockAdded(this.worldObj, (this.xPosition << 4) + var1, var2, (this.zPosition << 4) + var3);
			var5 = (TileEntity)this.chunkTileEntityMap.get(Integer.valueOf(var4));
		}

		return var5;
	}

	public final void setChunkBlockTileEntity(int var1, int var2, int var3, TileEntity var4) {
		this.isModified = true;
		int var5 = var1 + (var2 << 10) + (var3 << 10 << 10);
		var4.worldObj = this.worldObj;
		var4.xCoord = (this.xPosition << 4) + var1;
		var4.yCoord = var2;
		var4.zCoord = (this.zPosition << 4) + var3;
		if(this.getBlockID(var1, var2, var3) != 0 && Block.blocksList[this.getBlockID(var1, var2, var3)] instanceof BlockContainer) {
			this.chunkTileEntityMap.put(Integer.valueOf(var5), var4);
			this.worldObj.loadedTileEntityList.add(var4);
		} else {
			System.out.println("Attempted to place a tile entity where there was no entity tile!");
		}
	}

	public final void removeChunkBlockTileEntity(int var1, int var2, int var3) {
		this.isModified = true;
		var1 = var1 + (var2 << 10) + (var3 << 10 << 10);
		this.worldObj.loadedTileEntityList.remove(this.chunkTileEntityMap.remove(Integer.valueOf(var1)));
	}

	public final void loadEntities() {
		this.worldObj.loadedTileEntityList.addAll(this.chunkTileEntityMap.values());
	}

	public final void unloadEntities() {
		this.worldObj.loadedTileEntityList.removeAll(this.chunkTileEntityMap.values());
	}
}
