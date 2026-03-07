package net.minecraft.game.entity.animal;

import com.mojang.nbt.NBTTagCompound;
import net.minecraft.game.entity.EntityCreature;
import net.minecraft.game.world.World;
import net.minecraft.game.world.block.Block;

public abstract class EntityAnimal extends EntityCreature {
	public EntityAnimal(World var1) {
		super(var1);
	}

	protected final float getBlockPathWeight(int var1, int var2, int var3) {
		return this.worldObj.getBlockId(var1, var2 - 1, var3) == Block.grass.blockID ? 10.0F : this.worldObj.getBrightness(var1, var2, var3) - 0.5F;
	}

	public void writeEntityToNBT(NBTTagCompound var1) {
		super.writeEntityToNBT(var1);
	}

	public void readEntityFromNBT(NBTTagCompound var1) {
		super.readEntityFromNBT(var1);
	}
}
