package net.minecraft.src;

public class EntityMob extends EntityCreature {
	protected int attackStrength = 2;

	public EntityMob(World var1) {
		super(var1);
		this.health = 20;
	}

	public void onLivingUpdate() {
		float var1 = this.getBrightness(1.0F);
		if(var1 > 0.5F) {
			this.entityAge += 2;
		}

		super.onLivingUpdate();
	}

	public void i_() {
		super.i_();
		if(this.worldObj.difficultySetting == 0) {
			this.setEntityDead();
		}

	}

	protected Entity findPlayerToAttack() {
		EntityPlayer var1 = this.worldObj.getClosestPlayerToEntity(this, 16.0D);
		return var1 != null && this.b(var1) ? var1 : null;
	}

	public boolean attackEntityFrom(Entity var1, int var2) {
		if(super.attackEntityFrom(var1, var2)) {
			if(var1 != this) {
				this.entityToAttack = var1;
			}

			return true;
		} else {
			return false;
		}
	}

	protected void attackEntity(Entity var1, float var2) {
		if((double)var2 < 2.5D && var1.boundingBox.maxY > this.boundingBox.minY && var1.boundingBox.minY < this.boundingBox.maxY) {
			this.attackTime = 20;
			var1.attackEntityFrom(this, this.attackStrength);
		}

	}

	protected float getBlockPathWeight(int var1, int var2, int var3) {
		return 0.5F - this.worldObj.getBrightness(var1, var2, var3);
	}

	public void writeEntityToNBT(NBTTagCompound var1) {
		super.writeEntityToNBT(var1);
	}

	public void readEntityFromNBT(NBTTagCompound var1) {
		super.readEntityFromNBT(var1);
	}

	public boolean a(double var1, double var3, double var5) {
		int var7 = MathHelper.floor_double(var1);
		int var8 = MathHelper.floor_double(var3);
		int var9 = MathHelper.floor_double(var5);
		if(this.worldObj.getSavedLightValue(EnumSkyBlock.Sky, var7, var8, var9) > this.aI.nextInt(16)) {
			return false;
		} else {
			int var10 = this.worldObj.getBlockLightValue(var7, var8, var9);
			return var10 <= this.aI.nextInt(8) && super.a(var1, var3, var5);
		}
	}
}
