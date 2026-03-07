package net.minecraft.game.world.block;

import net.minecraft.game.world.material.Material;

public final class BlockOreStorage extends Block {
	public BlockOreStorage(int var1, int var2) {
		super(var1, Material.iron);
		this.blockIndexInTexture = var2;
	}

	public final int getBlockTextureFromSide(int var1) {
		return var1 == 1 ? this.blockIndexInTexture - 16 : (var1 == 0 ? this.blockIndexInTexture + 16 : this.blockIndexInTexture);
	}
}
