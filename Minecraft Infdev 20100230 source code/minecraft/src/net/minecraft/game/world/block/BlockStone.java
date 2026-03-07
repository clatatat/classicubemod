package net.minecraft.game.world.block;

import java.util.Random;
import net.minecraft.game.world.material.Material;

public final class BlockStone extends Block {
	public BlockStone(int var1, int var2) {
		super(var1, var2, Material.rock);
	}

	public final int idDropped(int var1, Random var2) {
		return Block.cobblestone.blockID;
	}
}
