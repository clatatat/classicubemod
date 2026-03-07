package net.minecraft.game.world.block;

import java.util.Random;
import net.minecraft.game.entity.misc.EntityTNT;
import net.minecraft.game.world.World;
import net.minecraft.game.world.material.Material;

public final class BlockTNT extends Block {
	public BlockTNT(int var1, int var2) {
		super(46, 8, Material.tnt);
	}

	public final int getBlockTextureFromSide(int var1) {
		return var1 == 0 ? this.blockIndexInTexture + 2 : (var1 == 1 ? this.blockIndexInTexture + 1 : this.blockIndexInTexture);
	}

	public final int quantityDropped(Random var1) {
		return 0;
	}

	public final void onBlockDestroyedByExplosion(World var1, int var2, int var3, int var4) {
		EntityTNT var5 = new EntityTNT(var1, (float)var2 + 0.5F, (float)var3 + 0.5F, (float)var4 + 0.5F);
		var5.fuse = var1.rand.nextInt(var5.fuse / 4) + var5.fuse / 8;
		var1.spawnEntityInWorld(var5);
	}

	public final void onBlockDestroyedByPlayer(World var1, int var2, int var3, int var4, int var5) {
		EntityTNT var6 = new EntityTNT(var1, (float)var2 + 0.5F, (float)var3 + 0.5F, (float)var4 + 0.5F);
		var1.spawnEntityInWorld(var6);
		var1.playSoundAtEntity(var6, "random.fuse", 1.0F, 1.0F);
	}
}
