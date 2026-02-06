package net.minecraft.src;

public class BlockCactus extends Block {
	protected BlockCactus(int var1, int var2) {
		super(var1, var2, Material.cactus);
	}

	public int getBlockTexture(IBlockAccess var1, int var2, int var3, int var4, int var5) {
		return var5 == 1 ? this.blockIndexInTexture - 1 : (var5 == 0 ? this.blockIndexInTexture + 1 : this.blockIndexInTexture);
	}

	public boolean canBlockStay(World var1, int var2, int var3, int var4) {
		if(var1.getBlockMaterial(var2 - 1, var3, var4).isSolid()) {
			return false;
		} else if(var1.getBlockMaterial(var2 + 1, var3, var4).isSolid()) {
			return false;
		} else if(var1.getBlockMaterial(var2, var3, var4 - 1).isSolid()) {
			return false;
		} else if(var1.getBlockMaterial(var2, var3, var4 + 1).isSolid()) {
			return false;
		} else {
			int var5 = var1.getBlockId(var2, var3 - 1, var4);
			return var5 == Block.cactus.blockID || var5 == Block.sand.blockID;
		}
	}

	public void onBlockClicked(World var1, int var2, int var3, int var4, EntityPlayer var5) {
		var5.attackEntityFrom((Entity)null, 1);
		super.onBlockClicked(var1, var2, var3, var4, var5);
	}

	public void onEntityWalking(World var1, int var2, int var3, int var4, Entity var5) {
		var5.attackEntityFrom((Entity)null, 1);
		super.onEntityWalking(var1, var2, var3, var4, var5);
	}

	public boolean blockActivated(World var1, int var2, int var3, int var4, EntityPlayer var5) {
		var5.attackEntityFrom((Entity)null, 1);
		return super.blockActivated(var1, var2, var3, var4, var5);
	}
}
