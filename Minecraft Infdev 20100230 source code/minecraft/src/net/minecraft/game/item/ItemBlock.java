package net.minecraft.game.item;

import net.minecraft.game.world.World;
import net.minecraft.game.world.block.Block;
import net.minecraft.game.world.block.StepSound;

public final class ItemBlock extends Item {
	private int blockID;

	public ItemBlock(int var1) {
		super(var1);
		this.blockID = var1 + 256;
		this.setIconIndex(Block.blocksList[var1 + 256].getBlockTextureFromSide(2));
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

		if(var1.stackSize == 0) {
			return false;
		} else {
			int var7 = var2.getBlockId(var3, var4, var5);
			Block var9 = Block.blocksList[var7];
			if(this.blockID > 0 && var9 == null || var9 == Block.waterMoving || var9 == Block.waterStill || var9 == Block.lavaMoving || var9 == Block.lavaStill || var9 == Block.fire) {
				var9 = Block.blocksList[this.blockID];
				var9.getCollisionBoundingBoxFromPool(var3, var4, var5);
				if(var9.canPlaceBlockAt(var2, var3, var4, var5) && var2.setBlockWithNotify(var3, var4, var5, this.blockID)) {
					Block.blocksList[this.blockID].onBlockPlaced(var2, var3, var4, var5, var6);
					double var10001 = (double)((float)var3 + 0.5F);
					double var10002 = (double)((float)var4 + 0.5F);
					double var10003 = (double)((float)var5 + 0.5F);
					String var10004 = var9.stepSound.getStepSound();
					StepSound var8 = var9.stepSound;
					float var10005 = (var8.stepSoundVolume + 1.0F) / 2.0F;
					var8 = var9.stepSound;
					var2.playSoundEffect(var10001, var10002, var10003, var10004, var10005, var8.stepSoundPitch * 0.8F);
					--var1.stackSize;
				}
			}

			return true;
		}
	}
}
