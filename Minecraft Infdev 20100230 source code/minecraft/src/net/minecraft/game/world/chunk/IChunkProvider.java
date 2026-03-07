package net.minecraft.game.world.chunk;

public interface IChunkProvider {
	boolean chunkExists(int var1, int var2);

	Chunk provideChunk(int var1, int var2);

	void populate(IChunkProvider var1, int var2, int var3);

	void saveChunks(boolean var1);
}
