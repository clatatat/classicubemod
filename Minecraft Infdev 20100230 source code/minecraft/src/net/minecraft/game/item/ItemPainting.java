package net.minecraft.game.item;

import net.minecraft.game.entity.EntityPainting;
import net.minecraft.game.world.World;

public final class ItemPainting extends Item {
	public ItemPainting(int var1) {
		super(65);
		this.maxDamage = 64;
	}

	public final boolean onItemUse(ItemStack var1, World var2, int var3, int var4, int var5, int var6) {
		if(var6 == 0) {
			return false;
		} else if(var6 == 1) {
			return false;
		} else {
			byte var7 = 0;
			if(var6 == 4) {
				var7 = 1;
			}

			if(var6 == 3) {
				var7 = 2;
			}

			if(var6 == 5) {
				var7 = 3;
			}

			EntityPainting var8 = new EntityPainting(var2, var3, var4, var5, var7);
			if(var8.onValidSurface()) {
				var2.spawnEntityInWorld(var8);
				--var1.stackSize;
			}

			return true;
		}
	}
}
