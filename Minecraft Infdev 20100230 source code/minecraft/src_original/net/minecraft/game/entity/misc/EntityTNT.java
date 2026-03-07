package net.minecraft.game.entity.misc;

import com.mojang.nbt.NBTTagCompound;
import net.minecraft.game.entity.Entity;
import net.minecraft.game.world.World;
import util.MathHelper;

public class EntityTNT extends Entity {
	public int fuse = 0;

	public EntityTNT(World var1, float var2, float var3, float var4) {
		super(var1);
		this.setSize(0.98F, 0.98F);
		this.yOffset = this.height / 2.0F;
		this.setPosition((double)var2, (double)var3, (double)var4);
		float var5 = (float)(Math.random() * (double)((float)Math.PI) * 2.0D);
		this.motionX = (double)(-MathHelper.sin(var5 * (float)Math.PI / 180.0F) * 0.02F);
		this.motionY = (double)0.2F;
		this.motionZ = (double)(-MathHelper.cos(var5 * (float)Math.PI / 180.0F) * 0.02F);
		this.entityWalks = false;
		this.fuse = 80;
		this.prevPosX = (double)var2;
		this.prevPosY = (double)var3;
		this.prevPosZ = (double)var4;
	}

	public final boolean canBeCollidedWith() {
		return !this.isDead;
	}

	public final void onUpdate() {
		this.prevPosX = this.posX;
		this.prevPosY = this.posY;
		this.prevPosZ = this.posZ;
		this.motionY -= (double)0.04F;
		this.moveEntity(this.motionX, this.motionY, this.motionZ);
		this.motionX *= (double)0.98F;
		this.motionY *= (double)0.98F;
		this.motionZ *= (double)0.98F;
		if(this.onGround) {
			this.motionX *= (double)0.7F;
			this.motionZ *= (double)0.7F;
			this.motionY *= -0.5D;
		}

		if(this.fuse-- <= 0) {
			this.setEntityDead();
			this.worldObj.createExplosion((Entity)null, this.posX, this.posY, this.posZ, 4.0F);
		} else {
			this.worldObj.spawnParticle("smoke", this.posX, this.posY + 0.5D, this.posZ, 0.0D, 0.0D, 0.0D);
		}
	}

	protected final void writeEntityToNBT(NBTTagCompound var1) {
		var1.setByte("Fuse", (byte)this.fuse);
	}

	protected final void readEntityFromNBT(NBTTagCompound var1) {
		this.fuse = var1.getByte("Fuse");
	}

	protected final String getEntityType() {
		return "PrimedTnt";
	}
}
