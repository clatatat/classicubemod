package net.minecraft.game.entity;

import net.minecraft.game.physics.Vec3D;
import net.minecraft.game.world.World;
import net.minecraft.game.world.path.PathEntity;
import util.MathHelper;

public class EntityCreature extends EntityLiving {
	private PathEntity pathToEntity;
	protected Entity playerToAttack;
	protected boolean hasAttacked = false;

	public EntityCreature(World var1) {
		super(var1);
	}

	protected final boolean canEntityBeSeen(Entity var1) {
		return this.worldObj.rayTraceBlocks(new Vec3D(this.posX, this.posY + (double)this.getEyeHeight(), this.posZ), new Vec3D(var1.posX, var1.posY + (double)var1.getEyeHeight(), var1.posZ)) == null;
	}

	protected void updatePlayerActionState() {
		this.hasAttacked = false;
		if(this.playerToAttack == null) {
			this.playerToAttack = this.findPlayerToAttack();
			if(this.playerToAttack != null) {
				this.pathToEntity = this.worldObj.pathFinder.createEntityPathTo(this, this.playerToAttack, 16.0F);
			}
		} else if(!this.playerToAttack.isEntityAlive()) {
			this.playerToAttack = null;
		} else {
			Entity var5 = this.playerToAttack;
			float var13 = (float)(var5.posX - super.posX);
			float var14 = (float)(var5.posY - super.posY);
			float var15 = (float)(var5.posZ - super.posZ);
			float var1 = MathHelper.sqrt_float(var13 * var13 + var14 * var14 + var15 * var15);
			if(this.canEntityBeSeen(this.playerToAttack)) {
				this.attackEntity(this.playerToAttack, var1);
			}
		}

		if(this.hasAttacked) {
			this.moveStrafing = 0.0F;
			this.moveForward = 0.0F;
			this.isJumping = false;
		} else {
			float var4;
			if(this.playerToAttack == null || this.pathToEntity != null && this.rand.nextInt(20) != 0) {
				if(this.pathToEntity == null || this.rand.nextInt(100) == 0) {
					int var24 = -1;
					int var2 = -1;
					int var3 = -1;
					var4 = -99999.0F;

					for(int var28 = 0; var28 < 200; ++var28) {
						int var6 = (int)(this.posX + (double)this.rand.nextInt(21) - 10.0D);
						int var7 = (int)(this.posY + (double)this.rand.nextInt(9) - 4.0D);
						int var8 = (int)(this.posZ + (double)this.rand.nextInt(21) - 10.0D);
						float var9 = this.getBlockPathWeight(var6, var7, var8);
						if(var9 > var4) {
							var4 = var9;
							var24 = var6;
							var2 = var7;
							var3 = var8;
						}
					}

					if(var24 > 0) {
						this.pathToEntity = this.worldObj.pathFinder.createEntityPathToXYZ(this, var24, var2, var3, 16.0F);
					}
				}
			} else {
				this.pathToEntity = this.worldObj.pathFinder.createEntityPathTo(this, this.playerToAttack, 16.0F);
			}

			boolean var25 = this.handleWaterMovement();
			boolean var26 = this.handleLavaMovement();
			if(this.pathToEntity != null && this.rand.nextInt(100) != 0) {
				Vec3D var27 = this.pathToEntity.getPosition(this);
				var4 = this.width * 2.0F;

				while(var27 != null) {
					double var16 = this.posZ;
					double var32 = this.posY;
					double var12 = this.posX;
					double var18 = var12 - var27.xCoord;
					double var20 = var32 - var27.yCoord;
					double var22 = var16 - var27.zCoord;
					if(var18 * var18 + var20 * var20 + var22 * var22 >= (double)(var4 * var4) || var27.yCoord > this.posY) {
						break;
					}

					this.pathToEntity.incrementPathIndex();
					if(this.pathToEntity.isFinished()) {
						var27 = null;
						this.pathToEntity = null;
					} else {
						var27 = this.pathToEntity.getPosition(this);
					}
				}

				this.isJumping = false;
				if(var27 != null) {
					double var29 = var27.xCoord - this.posX;
					double var30 = var27.zCoord - this.posZ;
					double var31 = var27.yCoord - this.posY;
					this.rotationYaw = (float)(Math.atan2(var30, var29) * 180.0D / (double)((float)Math.PI)) - 90.0F;
					this.moveForward = this.moveSpeed;
					if(var31 > 0.0D) {
						this.isJumping = true;
					}
				}

				if(this.rand.nextFloat() < 0.8F && (var25 || var26)) {
					this.isJumping = true;
				}

			} else {
				super.updatePlayerActionState();
				this.pathToEntity = null;
			}
		}
	}

	protected void attackEntity(Entity var1, float var2) {
	}

	protected float getBlockPathWeight(int var1, int var2, int var3) {
		return 0.0F;
	}

	protected Entity findPlayerToAttack() {
		return null;
	}
}
