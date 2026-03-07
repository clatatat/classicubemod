package net.minecraft.game.world.chunk;

import com.mojang.nbt.NBTTagCompound;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import net.minecraft.client.LoadingScreenRenderer;
import net.minecraft.game.world.World;

public final class ChunkProviderLoadOrGenerate implements IChunkProvider {
	private IChunkProvider chunkProvider;
	private Chunk[] chunks = new Chunk[1024];
	private File saveDirectory;
	private World worldObj;

	public ChunkProviderLoadOrGenerate(World var1, File var2, IChunkProvider var3) {
		this.worldObj = var1;
		this.chunkProvider = var3;
		this.saveDirectory = var2;
	}

	public final boolean chunkExists(int var1, int var2) {
		int var3 = var1 & 31 | (var2 & 31) << 5;
		if(this.chunks[var3] != null) {
			Chunk var10000 = this.chunks[var3];
			var3 = var2;
			var2 = var1;
			Chunk var4 = var10000;
			if(var2 == var4.xPosition && var3 == var4.zPosition) {
				return true;
			}
		}

		return false;
	}

	public final Chunk provideChunk(int var1, int var2) {
		int var3 = var1 & 31 | (var2 & 31) << 5;
		if(!this.chunkExists(var1, var2)) {
			if(this.chunks[var3] != null) {
				this.chunks[var3].unloadEntities();
				this.saveChunk(this.chunks[var3]);
			}

			Chunk var4 = this.loadChunk(var1, var2);
			if(var4 == null) {
				var4 = this.chunkProvider.provideChunk(var1, var2);
				this.saveChunk(var4);
			}

			this.chunks[var3] = var4;
			if(this.chunks[var3] != null) {
				this.chunks[var3].loadEntities();
			}

			if(this.chunkExists(var1 + 1, var2 + 1) && this.chunkExists(var1, var2 + 1) && this.chunkExists(var1 + 1, var2)) {
				this.populate(this, var1, var2);
			}

			if(this.chunkExists(var1 - 1, var2 + 1) && this.chunkExists(var1, var2 + 1) && this.chunkExists(var1 - 1, var2)) {
				this.populate(this, var1 - 1, var2);
			}

			if(this.chunkExists(var1 + 1, var2 - 1) && this.chunkExists(var1, var2 - 1) && this.chunkExists(var1 + 1, var2)) {
				this.populate(this, var1, var2 - 1);
			}

			if(this.chunkExists(var1 - 1, var2 - 1) && this.chunkExists(var1, var2 - 1) && this.chunkExists(var1 - 1, var2)) {
				this.populate(this, var1 - 1, var2 - 1);
			}
		}

		return this.chunks[var3];
	}

	private File chunkFileForXZ(int var1, int var2) {
		String var3 = "c." + Integer.toString(var1, 36) + "." + Integer.toString(var2, 36) + ".dat";
		String var4 = Integer.toString(var1 & 63, 36);
		String var6 = Integer.toString(var2 & 63, 36);
		File var5 = new File(this.saveDirectory, var4);
		var5.mkdirs();
		var5 = new File(var5, var6);
		var5.mkdirs();
		var5 = new File(var5, var3);
		return var5;
	}

	private Chunk loadChunk(int var1, int var2) {
		File var4 = this.chunkFileForXZ(var1, var2);
		if(var4.exists()) {
			try {
				FileInputStream var5 = new FileInputStream(var4);
				NBTTagCompound var6 = LoadingScreenRenderer.read(var5);
				return Chunk.readChunkNBTData(this.worldObj, var6.getCompoundTag("Level"));
			} catch (Exception var3) {
				var3.printStackTrace();
			}
		}

		return null;
	}

	private void saveChunk(Chunk var1) {
		File var2 = this.chunkFileForXZ(var1.xPosition, var1.zPosition);
		if(var2.exists()) {
			this.worldObj.sizeOnDisk -= var2.length();
		}

		try {
			FileOutputStream var3 = new FileOutputStream(var2);
			NBTTagCompound var4 = new NBTTagCompound();
			NBTTagCompound var5 = new NBTTagCompound();
			var4.setTag("Level", var5);
			var1.writeChunkNBTData(var5);
			LoadingScreenRenderer.write(var4, var3);
			this.worldObj.sizeOnDisk += var2.length();
		} catch (Exception var6) {
			var6.printStackTrace();
		}
	}

	public final void populate(IChunkProvider var1, int var2, int var3) {
		Chunk var4 = this.provideChunk(var2, var3);
		if(!var4.isTerrainPopulated) {
			var4.isTerrainPopulated = true;
			this.chunkProvider.populate(var1, var2, var3);
		}

	}

	public final void saveChunks(boolean var1) {
		int var2 = 0;

		for(int var3 = 0; var3 < this.chunks.length; ++var3) {
			if(this.chunks[var3] != null && this.chunks[var3].isModified) {
				this.saveChunk(this.chunks[var3]);
				this.chunks[var3].isModified = false;
				++var2;
				if(var2 == 10 && !var1) {
					return;
				}
			}
		}

	}
}
