package net.minecraft.game.item;

import net.minecraft.game.world.World;
import net.minecraft.game.world.block.Block;

public final class ItemFlintAndSteel extends Item {
	public ItemFlintAndSteel(int var1) {
		super(3);
		this.maxStackSize = 1;
		this.maxDamage = 64;
	}

	public final boolean onItemUse(ItemStack var1, World var2, int var3, int var4, int var5, int var6) {
		if(var6 == 0) {
			--var4;
		}

		if(var6 == 1) {
			++var4;
		}

		if(var6 == 2) {
			--var5;
		}

		if(var6 == 3) {
			++var5;
		}

		if(var6 == 4) {
			--var3;
		}

		if(var6 == 5) {
			++var3;
		}

		var6 = var2.getBlockId(var3, var4, var5);
		if(var6 == 0) {
			var2.playSoundEffect((double)var3 + 0.5D, (double)var4 + 0.5D, (double)var5 + 0.5D, "fire.ignite", 1.0F, itemRand.nextFloat() * 0.4F + 0.8F);
			var2.setBlockWithNotify(var3, var4, var5, Block.fire.blockID);
		}

		var1.damageItem(1);
		return true;
	}
}
