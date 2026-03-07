package net.minecraft.game.item;

import net.minecraft.game.world.block.Block;

public final class ItemAxe extends ItemTool {
	private static Block[] damageReduceAmount = new Block[]{Block.planks, Block.bookshelf, Block.wood, Block.chest};

	public ItemAxe(int var1, int var2) {
		super(var1, 3, var2, damageReduceAmount);
	}
}
