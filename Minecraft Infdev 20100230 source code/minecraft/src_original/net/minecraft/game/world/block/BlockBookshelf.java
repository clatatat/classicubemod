package net.minecraft.game.world.block;

import java.util.Random;
import net.minecraft.game.world.material.Material;

public final class BlockBookshelf extends Block {
	public BlockBookshelf(int var1, int var2) {
		super(47, 35, Material.wood);
	}

	public final int getBlockTextureFromSide(int var1) {
		return var1 <= 1 ? 4 : this.blockIndexInTexture;
	}

	public final int quantityDropped(Random var1) {
		return 0;
	}
}
