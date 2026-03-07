package net.minecraft.game.world.block;

import java.util.Random;
import net.minecraft.game.physics.AxisAlignedBB;
import net.minecraft.game.world.material.Material;

public final class BlockGears extends Block {
	protected BlockGears(int var1, int var2) {
		super(55, 62, Material.circuits);
	}

	public final AxisAlignedBB getCollisionBoundingBoxFromPool(int var1, int var2, int var3) {
		return null;
	}

	public final boolean isOpaqueCube() {
		return false;
	}

	public final boolean renderAsNormalBlock() {
		return false;
	}

	public final int getRenderType() {
		return 5;
	}

	public final int quantityDropped(Random var1) {
		return 1;
	}

	public final boolean isCollidable() {
		return false;
	}
}
