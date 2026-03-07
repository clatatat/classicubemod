package net.minecraft.game.entity.animal;

import com.mojang.nbt.NBTTagCompound;
import net.minecraft.game.entity.Entity;
import net.minecraft.game.entity.EntityLiving;
import net.minecraft.game.entity.misc.EntityItem;
import net.minecraft.game.world.World;
import net.minecraft.game.world.block.Block;

public class EntitySheep extends EntityAnimal {
	public boolean sheared = false;

	public EntitySheep(World var1) {
		super(var1);
		this.texture = "/mob/sheep.png";
		this.setSize(0.9F, 1.3F);
	}

	public final boolean attackEntityFrom(Entity var1, int var2) {
		if(!this.sheared && var1 instanceof EntityLiving) {
			this.sheared = true;
			int var3 = 1 + this.rand.nextInt(3);

			for(int var4 = 0; var4 < var3; ++var4) {
				EntityItem var5 = this.entityDropItem(Block.clothGray.blockID, 1, 1.0F);
				var5.motionY += (double)(this.rand.nextFloat() * 0.05F);
				var5.motionX += (double)((this.rand.nextFloat() - this.rand.nextFloat()) * 0.1F);
				var5.motionZ += (double)((this.rand.nextFloat() - this.rand.nextFloat()) * 0.1F);
			}
		}

		return super.attackEntityFrom(var1, var2);
	}

	public final void writeEntityToNBT(NBTTagCompound var1) {
		super.writeEntityToNBT(var1);
		var1.setBoolean("Sheared", this.sheared);
	}

	public final void readEntityFromNBT(NBTTagCompound var1) {
		super.readEntityFromNBT(var1);
		this.sheared = var1.getBoolean("Sheared");
	}

	public final String getEntityType() {
		return "Sheep";
	}

	protected final String getLivingSound() {
		return "mob.sheep";
	}

	protected final String getHurtSound() {
		return "mob.sheep";
	}

	protected final String getDeathSound() {
		return "mob.sheep";
	}
}
