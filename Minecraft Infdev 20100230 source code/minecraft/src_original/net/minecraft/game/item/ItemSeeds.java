package net.minecraft.game.item;

import net.minecraft.game.world.World;
import net.minecraft.game.world.block.Block;

public final class ItemSeeds extends Item {
	private int blockType;

	public ItemSeeds(int var1, int var2) {
		super(39);
		this.blockType = var2;
	}

	public final boolean onItemUse(ItemStack var1, World var2, int var3, int var4, int var5, int var6) {
		if(var6 != 1) {
			return false;
		} else {
			var6 = var2.getBlockId(var3, var4, var5);
			if(var6 == Block.tilledField.blockID) {
				var2.setBlockWithNotify(var3, var4 + 1, var5, this.blockType);
				--var1.stackSize;
				return true;
			} else {
				return false;
			}
		}
	}
}
