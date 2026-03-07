package net.minecraft.game.world.block;

import java.util.Random;
import net.minecraft.game.item.Item;
import net.minecraft.game.world.material.Material;

public final class BlockOre extends Block {
	public BlockOre(int var1, int var2) {
		super(var1, var2, Material.rock);
	}

	public final int idDropped(int var1, Random var2) {
		return this.blockID == Block.oreCoal.blockID ? Item.coal.shiftedIndex : (this.blockID == Block.oreDiamond.blockID ? Item.diamod.shiftedIndex : this.blockID);
	}

	public final int quantityDropped(Random var1) {
		return 1;
	}
}
