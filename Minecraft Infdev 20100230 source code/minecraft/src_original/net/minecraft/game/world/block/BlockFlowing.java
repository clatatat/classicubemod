package net.minecraft.game.world.block;

import java.util.Random;
import net.minecraft.game.physics.AxisAlignedBB;
import net.minecraft.game.world.World;
import net.minecraft.game.world.material.Material;

public final class BlockFlowing extends BlockFluid {
	private int stillId1;
	private int movingId1;

	protected BlockFlowing(int var1, Material var2) {
		super(var1, var2);
		new Random();
		int[] var10000 = new int[]{0, 1, 2, 3};
		this.blockIndexInTexture = 14;
		if(var2 == Material.lava) {
			this.blockIndexInTexture = 30;
		}

		Block.isBlockContainer[var1] = true;
		this.movingId1 = var1;
		this.stillId1 = var1 + 1;
		this.setBlockBounds(0.01F, -0.09F, 0.01F, 1.01F, 0.90999997F, 1.01F);
		this.setTickOnLoad(true);
	}

	public final void onBlockAdded(World var1, int var2, int var3, int var4) {
		var1.scheduleBlockUpdate(var2, var3, var4, this.movingId1);
	}

	public final void updateTick(World var1, int var2, int var3, int var4, Random var5) {
		boolean var6 = false;
		boolean var10000 = false;
	}

	public final boolean update(World var1, int var2, int var3, int var4, int var5) {
		return false;
	}

	public final boolean shouldSideBeRendered(World var1, int var2, int var3, int var4, int var5) {
		int var6 = var1.getBlockId(var2, var3, var4);
		return var6 != this.movingId1 && var6 != this.stillId1 ? (var5 != 1 || var1.getBlockId(var2 - 1, var3, var4) != 0 && var1.getBlockId(var2 + 1, var3, var4) != 0 && var1.getBlockId(var2, var3, var4 - 1) != 0 && var1.getBlockId(var2, var3, var4 + 1) != 0 ? super.shouldSideBeRendered(var1, var2, var3, var4, var5) : true) : false;
	}

	public final boolean isCollidable() {
		return false;
	}

	public final AxisAlignedBB getCollisionBoundingBoxFromPool(int var1, int var2, int var3) {
		return null;
	}

	public final boolean isOpaqueCube() {
		return false;
	}

	public final void onNeighborBlockChange(World var1, int var2, int var3, int var4, int var5) {
	}

	public final int e() {
		return this.blockMaterial == Material.lava ? 25 : 5;
	}

	public final int quantityDropped(Random var1) {
		return 0;
	}

	public final int getRenderBlockPass() {
		return this.blockMaterial == Material.water ? 1 : 0;
	}
}
