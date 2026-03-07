package net.minecraft.game.entity;

import com.mojang.nbt.NBTTagCompound;
import java.util.List;
import net.minecraft.game.world.World;
import net.minecraft.game.world.block.Block;
import net.minecraft.game.world.block.StepSound;
import util.MathHelper;

public class EntityLiving extends Entity {
	public int heartsHalvesLife = 20;
	public float renderYawOffset = 0.0F;
	public float prevRenderYawOffset = 0.0F;
	private float rotationYawHead;
	private float prevRotationYawHead;
	protected String texture = "/char.png";
	private int scoreValue = 0;
	public int health;
	public int prevHealth;
	private int livingSoundTime;
	public int hurtTime;
	public int maxHurtTime;
	public float attackedAtYaw = 0.0F;
	public int deathTime = 0;
	public int attackTime = 0;
	public float prevCameraPitch;
	public float cameraPitch;
	public float newPosZ;
	public float newRotationYaw;
	public float newRotationPitch;
	protected int entityAge;
	protected float moveStrafing;
	protected float moveForward;
	private float randomYawVelocity;
	protected boolean isJumping;
	private float defaultPitch;
	protected float moveSpeed;

	public EntityLiving(World var1) {
		super(var1);
		Math.random();
		this.entityAge = 0;
		this.isJumping = false;
		this.defaultPitch = 0.0F;
		this.moveSpeed = 0.7F;
		this.health = 10;
		Math.random();
		this.setPosition(this.posX, this.posY, this.posZ);
		Math.random();
		this.rotationYaw = (float)(Math.random() * (double)((float)Math.PI) * 2.0D);
		this.stepHeight = 0.5F;
	}

	public final String getEntityTexture() {
		return this.texture;
	}

	public final boolean canBeCollidedWith() {
		return !this.isDead;
	}

	public final boolean canBePushed() {
		return !this.isDead;
	}

	protected float getEyeHeight() {
		return this.height * 0.85F;
	}

	public void onUpdate() {
		super.onUpdate();
		if(this.rand.nextInt(1000) < this.livingSoundTime++) {
			this.livingSoundTime = -80;
			String var1 = this.getLivingSound();
			if(var1 != null) {
				this.worldObj.playSoundAtEntity(this, var1, 1.0F, (this.rand.nextFloat() - this.rand.nextFloat()) * 0.2F + 1.0F);
			}
		}

		if(this.isEntityAlive() && this.isInsideOfMaterial()) {
			--this.air;
			if(this.air == -20) {
				this.air = 0;

				for(int var9 = 0; var9 < 8; ++var9) {
					float var2 = this.rand.nextFloat() - this.rand.nextFloat();
					float var3 = this.rand.nextFloat() - this.rand.nextFloat();
					float var4 = this.rand.nextFloat() - this.rand.nextFloat();
					this.worldObj.spawnParticle("bubble", this.posX + (double)var2, this.posY + (double)var3, this.posZ + (double)var4, this.motionX, this.motionY, this.motionZ);
				}

				this.attackEntityFrom((Entity)null, 2);
			}

			this.fire = 0;
		} else {
			this.air = this.maxAir;
		}

		this.prevCameraPitch = this.cameraPitch;
		if(this.attackTime > 0) {
			--this.attackTime;
		}

		if(this.hurtTime > 0) {
			--this.hurtTime;
		}

		if(this.heartsLife > 0) {
			--this.heartsLife;
		}

		if(this.health <= 0) {
			++this.deathTime;
			if(this.deathTime > 20) {
				this.setEntityDead();
			}
		}

		this.prevRenderYawOffset = this.renderYawOffset;
		this.prevRotationYaw = this.rotationYaw;
		this.prevRotationPitch = this.rotationPitch;
		this.onLivingUpdate();
		double var10 = this.posX - this.prevPosX;
		double var13 = this.posZ - this.prevPosZ;
		float var5 = MathHelper.sqrt_double(var10 * var10 + var13 * var13);
		float var6 = this.renderYawOffset;
		float var7 = 0.0F;
		float var8 = 0.0F;
		if(var5 > 0.05F) {
			var8 = 1.0F;
			var7 = var5 * 3.0F;
			var6 = (float)Math.atan2(var13, var10) * 180.0F / (float)Math.PI - 90.0F;
		}

		if(!this.onGround) {
			var8 = 0.0F;
		}

		this.rotationYawHead += (var8 - this.rotationYawHead) * 0.3F;

		float var11;
		for(var11 = var6 - this.renderYawOffset; var11 < -180.0F; var11 += 360.0F) {
		}

		while(var11 >= 180.0F) {
			var11 -= 360.0F;
		}

		this.renderYawOffset += var11 * 0.1F;

		for(var11 = this.rotationYaw - this.renderYawOffset; var11 < -180.0F; var11 += 360.0F) {
		}

		while(var11 >= 180.0F) {
			var11 -= 360.0F;
		}

		boolean var12 = var11 < -90.0F || var11 >= 90.0F;
		if(var11 < -75.0F) {
			var11 = -75.0F;
		}

		if(var11 >= 75.0F) {
			var11 = 75.0F;
		}

		this.renderYawOffset = this.rotationYaw - var11;
		this.renderYawOffset += var11 * 0.1F;
		if(var12) {
			var7 = -var7;
		}

		while(this.rotationYaw - this.prevRotationYaw < -180.0F) {
			this.prevRotationYaw -= 360.0F;
		}

		while(this.rotationYaw - this.prevRotationYaw >= 180.0F) {
			this.prevRotationYaw += 360.0F;
		}

		while(this.renderYawOffset - this.prevRenderYawOffset < -180.0F) {
			this.prevRenderYawOffset -= 360.0F;
		}

		while(this.renderYawOffset - this.prevRenderYawOffset >= 180.0F) {
			this.prevRenderYawOffset += 360.0F;
		}

		while(this.rotationPitch - this.prevRotationPitch < -180.0F) {
			this.prevRotationPitch -= 360.0F;
		}

		while(this.rotationPitch - this.prevRotationPitch >= 180.0F) {
			this.prevRotationPitch += 360.0F;
		}

		this.prevRotationYawHead += var7;
	}

	protected final void setSize(float var1, float var2) {
		super.setSize(var1, var2);
	}

	public final void heal(int var1) {
		if(this.health > 0) {
			this.health += var1;
			if(this.health > 20) {
				this.health = 20;
			}

			this.heartsLife = this.heartsHalvesLife / 2;
		}
	}

	public boolean attackEntityFrom(Entity var1, int var2) {
		this.entityAge = 0;
		if(this.health <= 0) {
			return false;
		} else {
			this.newRotationYaw = 1.5F;
			if((float)this.heartsLife > (float)this.heartsHalvesLife / 2.0F) {
				if(this.prevHealth - var2 >= this.health) {
					return false;
				}

				this.health = this.prevHealth - var2;
			} else {
				this.prevHealth = this.health;
				this.heartsLife = this.heartsHalvesLife;
				this.health -= var2;
				this.hurtTime = this.maxHurtTime = 10;
			}

			this.attackedAtYaw = 0.0F;
			if(var1 != null) {
				double var3 = var1.posX - this.posX;
				double var5 = var1.posZ - this.posZ;
				this.attackedAtYaw = (float)(Math.atan2(var5, var3) * 180.0D / (double)((float)Math.PI)) - this.rotationYaw;
				double var8 = var3;
				float var12 = MathHelper.sqrt_double(var3 * var3 + var5 * var5);
				this.motionX /= 2.0D;
				this.motionY /= 2.0D;
				this.motionZ /= 2.0D;
				this.motionX -= var8 / (double)var12 * (double)0.4F;
				this.motionY += (double)0.4F;
				this.motionZ -= var5 / (double)var12 * (double)0.4F;
				if(this.motionY > (double)0.4F) {
					this.motionY = (double)0.4F;
				}
			} else {
				this.attackedAtYaw = (float)((int)(Math.random() * 2.0D) * 180);
			}

			if(this.health <= 0) {
				this.worldObj.playSoundAtEntity(this, this.getDeathSound(), 1.0F, (this.rand.nextFloat() - this.rand.nextFloat()) * 0.2F + 1.0F);
				this.onDeath(var1);
			} else {
				this.worldObj.playSoundAtEntity(this, this.getHurtSound(), 1.0F, (this.rand.nextFloat() - this.rand.nextFloat()) * 0.2F + 1.0F);
			}

			return true;
		}
	}

	protected String getLivingSound() {
		return null;
	}

	protected String getHurtSound() {
		return "random.hurt";
	}

	protected String getDeathSound() {
		return "random.hurt";
	}

	public void onDeath(Entity var1) {
		int var4 = this.getDroppedItem();
		if(var4 > 0) {
			int var2 = this.rand.nextInt(3);

			for(int var3 = 0; var3 < var2; ++var3) {
				this.dropItemWithOffset(var4, 1);
			}
		}

	}

	protected int getDroppedItem() {
		return 0;
	}

	protected final void fall(float var1) {
		int var3 = (int)Math.ceil((double)(var1 - 3.0F));
		if(var3 > 0) {
			this.attackEntityFrom((Entity)null, var3);
			var3 = this.worldObj.getBlockId((int)this.posX, (int)(this.posY - (double)0.2F - (double)this.yOffset), (int)this.posZ);
			if(var3 > 0) {
				StepSound var4 = Block.blocksList[var3].stepSound;
				this.worldObj.playSoundAtEntity(this, var4.getStepSound(), var4.stepSoundVolume * 0.5F, var4.stepSoundPitch * (12.0F / 16.0F));
			}
		}

	}

	public void writeEntityToNBT(NBTTagCompound var1) {
		var1.setShort("Health", (short)this.health);
		var1.setShort("HurtTime", (short)this.hurtTime);
		var1.setShort("DeathTime", (short)this.deathTime);
		var1.setShort("AttackTime", (short)this.attackTime);
	}

	public void readEntityFromNBT(NBTTagCompound var1) {
		this.health = var1.getShort("Health");
		if(!var1.hasKey("Health")) {
			this.health = 10;
		}

		this.hurtTime = var1.getShort("HurtTime");
		this.deathTime = var1.getShort("DeathTime");
		this.attackTime = var1.getShort("AttackTime");
	}

	public String getEntityType() {
		return "Mob";
	}

	public final boolean isEntityAlive() {
		return !this.isDead && this.health > 0;
	}

	public void onLivingUpdate() {
		++this.entityAge;
		if(this.entityAge > 600 && this.rand.nextInt(800) == 0) {
			Entity var1 = this.worldObj.getPlayerEntity();
			if(var1 != null) {
				double var2 = var1.posX - this.posX;
				double var4 = var1.posY - this.posY;
				double var6 = var1.posZ - this.posZ;
				double var8 = var2 * var2 + var4 * var4 + var6 * var6;
				if(var8 < 1024.0D) {
					this.entityAge = 0;
				} else {
					this.setEntityDead();
				}
			}
		}

		if(this.health <= 0) {
			this.isJumping = false;
			this.moveStrafing = 0.0F;
			this.moveForward = 0.0F;
			this.randomYawVelocity = 0.0F;
		} else {
			this.updatePlayerActionState();
		}

		boolean var17 = this.handleWaterMovement();
		boolean var18 = this.handleLavaMovement();
		if(this.isJumping) {
			if(var17) {
				this.motionY += (double)0.04F;
			} else if(var18) {
				this.motionY += (double)0.04F;
			} else if(this.onGround) {
				this.motionY = (double)0.42F;
			}
		}

		this.moveStrafing *= 0.98F;
		this.moveForward *= 0.98F;
		this.randomYawVelocity *= 0.9F;
		float var3 = this.moveForward;
		float var19 = this.moveStrafing;
		double var13;
		if(this.handleWaterMovement()) {
			var13 = this.posY;
			this.moveFlying(var19, var3, 0.02F);
			this.moveEntity(this.motionX, this.motionY, this.motionZ);
			this.motionX *= (double)0.8F;
			this.motionY *= (double)0.8F;
			this.motionZ *= (double)0.8F;
			this.motionY -= 0.02D;
			if(this.isCollidedHorizontally && this.isOffsetPositionInLiquid(this.motionX, this.motionY + (double)0.6F - this.posY + var13, this.motionZ)) {
				this.motionY = (double)0.3F;
			}
		} else if(this.handleLavaMovement()) {
			var13 = this.posY;
			this.moveFlying(var19, var3, 0.02F);
			this.moveEntity(this.motionX, this.motionY, this.motionZ);
			this.motionX *= 0.5D;
			this.motionY *= 0.5D;
			this.motionZ *= 0.5D;
			this.motionY -= 0.02D;
			if(this.isCollidedHorizontally && this.isOffsetPositionInLiquid(this.motionX, this.motionY + (double)0.6F - this.posY + var13, this.motionZ)) {
				this.motionY = (double)0.3F;
			}
		} else {
			this.moveFlying(var19, var3, this.onGround ? 0.1F : 0.02F);
			this.moveEntity(this.motionX, this.motionY, this.motionZ);
			this.motionX *= (double)0.91F;
			this.motionY *= (double)0.98F;
			this.motionZ *= (double)0.91F;
			this.motionY -= 0.08D;
			if(this.onGround) {
				this.motionX *= (double)0.6F;
				this.motionZ *= (double)0.6F;
			}
		}

		this.newPosZ = this.newRotationYaw;
		var13 = this.posX - this.prevPosX;
		double var15 = this.posZ - this.prevPosZ;
		var19 = MathHelper.sqrt_double(var13 * var13 + var15 * var15) * 4.0F;
		if(var19 > 1.0F) {
			var19 = 1.0F;
		}

		this.newRotationYaw += (var19 - this.newRotationYaw) * 0.4F;
		this.newRotationPitch += this.newRotationYaw;
		List var20 = this.worldObj.a(this, this.boundingBox.expand((double)0.2F, 0.0D, (double)0.2F));
		if(var20 != null && var20.size() > 0) {
			for(int var21 = 0; var21 < var20.size(); ++var21) {
				Entity var5 = (Entity)var20.get(var21);
				if(var5.canBePushed()) {
					var5.applyEntityCollision(this);
				}
			}
		}

	}

	protected void updatePlayerActionState() {
		if(this.rand.nextFloat() < 0.07F) {
			this.moveStrafing = (this.rand.nextFloat() - 0.5F) * this.moveSpeed;
			this.moveForward = this.rand.nextFloat() * this.moveSpeed;
		}

		this.isJumping = this.rand.nextFloat() < 0.01F;
		if(this.rand.nextFloat() < 0.04F) {
			this.randomYawVelocity = (this.rand.nextFloat() - 0.5F) * 60.0F;
		}

		this.rotationYaw += this.randomYawVelocity;
		this.rotationPitch = 0.0F;
		boolean var1 = this.handleWaterMovement();
		boolean var2 = this.handleLavaMovement();
		if(var1 || var2) {
			this.isJumping = this.rand.nextFloat() < 0.8F;
		}

	}
}
