package net.minecraft.src;

import java.util.List;

public class EntityBoat extends Entity {
	public int damageTaken;
	public int timeSinceHit;
	public int forwardDirection;
	private boolean d;

	public EntityBoat(World var1) {
		super(var1);
		this.damageTaken = 0;
		this.timeSinceHit = 0;
		this.forwardDirection = 1;
		this.d = false;
		this.preventEntitySpawning = true;
		this.setSize(0.98F, 0.6F);
		this.at = this.width / 2.0F;
		this.ay = false;
	}

	public AxisAlignedBB getCollisionBox(Entity var1) {
		return var1.boundingBox;
	}

	public AxisAlignedBB e_() {
		return this.boundingBox;
	}

	public boolean canBePushed() {
		return true;
	}

	public EntityBoat(World var1, double var2, double var4, double var6) {
		this(var1);
		this.b(var2, var4 + (double)this.at, var6);
		this.motionX = 0.0D;
		this.motionY = 0.0D;
		this.motionZ = 0.0D;
		this.prevPosX = var2;
		this.prevPosY = var4;
		this.prevPosZ = var6;
	}

	public double g_() {
		return (double)this.width * 0.0D - (double)0.3F;
	}

	public boolean attackEntityFrom(Entity var1, int var2) {
		this.forwardDirection = -this.forwardDirection;
		this.timeSinceHit = 10;
		this.damageTaken += var2 * 10;
		if(this.damageTaken > 40) {
			int var3;
			for(var3 = 0; var3 < 3; ++var3) {
				this.entityDropItem(Block.planks.blockID, 1, 0.0F);
			}

			for(var3 = 0; var3 < 2; ++var3) {
				this.entityDropItem(Item.stick.shiftedIndex, 1, 0.0F);
			}

			this.setEntityDead();
		}

		return true;
	}

	public boolean canBeCollidedWith() {
		return !this.surfaceCollision;
	}

	public void i_() {
		super.i_();
		if(this.timeSinceHit > 0) {
			--this.timeSinceHit;
		}

		if(this.damageTaken > 0) {
			--this.damageTaken;
		}

		this.prevPosX = this.posX;
		this.prevPosY = this.posY;
		this.prevPosZ = this.posZ;
		byte var1 = 5;
		double var2 = 0.0D;

		for(int var4 = 0; var4 < var1; ++var4) {
			double var5 = this.boundingBox.minY + (this.boundingBox.maxY - this.boundingBox.minY) * (double)(var4 + 0) / (double)var1 - 0.125D;
			double var7 = this.boundingBox.minY + (this.boundingBox.maxY - this.boundingBox.minY) * (double)(var4 + 1) / (double)var1 - 0.125D;
			AxisAlignedBB var9 = AxisAlignedBB.getBoundingBoxFromPool(this.boundingBox.minX, var5, this.boundingBox.minZ, this.boundingBox.maxX, var7, this.boundingBox.maxZ);
			if(this.worldObj.isAABBInMaterial(var9, Material.water)) {
				var2 += 1.0D / (double)var1;
			}
		}

		double var23 = var2 * 2.0D - 1.0D;
		this.motionY += (double)0.04F * var23;
		if(this.riddenByEntity != null) {
			this.motionX += this.riddenByEntity.motionX * 0.2D;
			this.motionZ += this.riddenByEntity.motionZ * 0.2D;
		}

		double var6 = 0.4D;
		if(this.motionX < -var6) {
			this.motionX = -var6;
		}

		if(this.motionX > var6) {
			this.motionX = var6;
		}

		if(this.motionZ < -var6) {
			this.motionZ = -var6;
		}

		if(this.motionZ > var6) {
			this.motionZ = var6;
		}

		if(this.onGround) {
			this.motionX *= 0.5D;
			this.motionY *= 0.5D;
			this.motionZ *= 0.5D;
		}

		this.d(this.motionX, this.motionY, this.motionZ);
		double var8;
		double var10;
		double var12;
		if(this.motionX * this.motionX + this.motionZ * this.motionZ > 0.001D) {
			var8 = Math.sqrt(this.motionX * this.motionX + this.motionZ * this.motionZ);
			var10 = -this.motionX / var8;
			var12 = -this.motionZ / var8;

			for(int var14 = 0; (double)var14 < 1.0D + var8 * 60.0D; ++var14) {
				double var15 = (double)(this.aI.nextFloat() * 2.0F - 1.0F);
				double var17 = (double)(this.aI.nextInt(2) * 2 - 1) * 0.7D;
				double var19;
				double var21;
				if(this.aI.nextBoolean()) {
					var19 = this.posX - var10 * var15 * 0.8D + var12 * var17;
					var21 = this.posZ - var12 * var15 * 0.8D - var10 * var17;
					var19 -= var10 * 6.0D / 16.0D;
					var21 -= var12 * 6.0D / 16.0D;
					this.worldObj.spawnParticle("splash", var19, this.posY - 0.125D, var21, this.motionX, this.motionY, this.motionZ);
				} else {
					var19 = this.posX + var10 + var12 * var15 * 0.7D;
					var21 = this.posZ + var12 - var10 * var15 * 0.7D;
					var19 -= var10 * 6.0D / 16.0D;
					var21 -= var12 * 6.0D / 16.0D;
					this.worldObj.spawnParticle("splash", var19, this.posY - 0.125D, var21, this.motionX, this.motionY, this.motionZ);
				}
			}
		}

		if(!this.onGround && !this.isCollidedHorizontally) {
			this.motionX *= (double)0.99F;
			this.motionY *= (double)0.95F;
			this.motionZ *= (double)0.99F;
		} else {
			this.setEntityDead();

			int var24;
			for(var24 = 0; var24 < 3; ++var24) {
				this.entityDropItem(Block.planks.blockID, 1, 0.0F);
			}

			for(var24 = 0; var24 < 2; ++var24) {
				this.entityDropItem(Item.stick.shiftedIndex, 1, 0.0F);
			}
		}

		this.rotationPitch = 0.0F;
		var8 = this.prevPosX - this.posX;
		var10 = this.prevPosZ - this.posZ;
		if(var8 * var8 + var10 * var10 > 0.001D) {
			this.rotationYaw = (float)(Math.atan2(var10, var8) * 180.0D / Math.PI);
			if(this.d) {
				this.rotationYaw += 180.0F;
			}
		}

		for(var12 = (double)(this.rotationYaw - this.prevRotationYaw); var12 >= 180.0D; var12 -= 360.0D) {
		}

		while(var12 < -180.0D) {
			var12 += 360.0D;
		}

		if(var12 < -170.0D || var12 >= 170.0D) {
			this.rotationYaw += 180.0F;
			this.d = !this.d;
		}

		this.setRotation(this.rotationYaw, this.rotationPitch);
		List var25 = this.worldObj.getEntitiesWithinAABBExcludingEntity(this, this.boundingBox.expand((double)0.2F, 0.0D, (double)0.2F));
		if(var25 != null && var25.size() > 0) {
			for(int var26 = 0; var26 < var25.size(); ++var26) {
				Entity var16 = (Entity)var25.get(var26);
				if(var16 != this.riddenByEntity && var16.canBePushed() && var16 instanceof EntityBoat) {
					var16.applyEntityCollision(this);
				}
			}
		}

		if(this.riddenByEntity != null && this.riddenByEntity.surfaceCollision) {
			this.riddenByEntity = null;
		}

	}

	protected void writeEntityToNBT(NBTTagCompound var1) {
	}

	protected void readEntityFromNBT(NBTTagCompound var1) {
	}

	public boolean interact(EntityPlayer var1) {
		var1.mountEntity(this);
		return true;
	}
}
