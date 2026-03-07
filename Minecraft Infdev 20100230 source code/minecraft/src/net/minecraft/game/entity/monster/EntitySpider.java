package net.minecraft.game.entity.monster;

import com.mojang.nbt.NBTTagCompound;
import net.minecraft.game.entity.Entity;
import net.minecraft.game.item.Item;
import net.minecraft.game.world.World;
import util.MathHelper;

public class EntitySpider extends EntityMonster {
	public EntitySpider(World var1) {
		super(var1);
		this.texture = "/mob/spider.png";
		this.setSize(1.4F, 0.9F);
		this.moveSpeed = 0.8F;
	}

	protected final Entity findPlayerToAttack() {
		float var1 = this.getEntityBrightness(1.0F);
		if(var1 < 0.5F) {
			double var2 = this.worldObj.playerEntity.getDistanceSqToEntity(this);
			if(var2 < 256.0D) {
				return this.worldObj.playerEntity;
			}
		}

		return null;
	}

	protected final void attackEntity(Entity var1, float var2) {
		float var3 = this.getEntityBrightness(1.0F);
		if(var3 > 0.5F && this.rand.nextInt(100) == 0) {
			this.playerToAttack = null;
		} else {
			if(var2 > 2.0F && var2 < 6.0F && this.rand.nextInt(10) == 0) {
				if(this.onGround) {
					double var4 = var1.posX - this.posX;
					double var6 = var1.posZ - this.posZ;
					float var8 = MathHelper.sqrt_double(var4 * var4 + var6 * var6);
					this.motionX = var4 / (double)var8 * 0.5D * (double)0.8F + this.motionX * (double)0.2F;
					this.motionZ = var6 / (double)var8 * 0.5D * (double)0.8F + this.motionZ * (double)0.2F;
					this.motionY = (double)0.4F;
					return;
				}
			} else {
				super.attackEntity(var1, var2);
			}

		}
	}

	public final void writeEntityToNBT(NBTTagCompound var1) {
		super.writeEntityToNBT(var1);
	}

	public final void readEntityFromNBT(NBTTagCompound var1) {
		super.readEntityFromNBT(var1);
	}

	public final String getEntityType() {
		return "Spider";
	}

	protected final int getDroppedItem() {
		return Item.silk.shiftedIndex;
	}
}
