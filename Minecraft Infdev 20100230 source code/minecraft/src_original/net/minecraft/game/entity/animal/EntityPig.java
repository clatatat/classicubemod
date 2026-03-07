package net.minecraft.game.entity.animal;

import com.mojang.nbt.NBTTagCompound;
import net.minecraft.game.item.Item;
import net.minecraft.game.world.World;

public class EntityPig extends EntityAnimal {
	public EntityPig(World var1) {
		super(var1);
		this.texture = "/mob/pig.png";
		this.setSize(0.9F, 0.9F);
	}

	public final void writeEntityToNBT(NBTTagCompound var1) {
		super.writeEntityToNBT(var1);
	}

	public final void readEntityFromNBT(NBTTagCompound var1) {
		super.readEntityFromNBT(var1);
	}

	public final String getEntityType() {
		return "Pig";
	}

	protected final String getLivingSound() {
		return "mob.pig";
	}

	protected final String getHurtSound() {
		return "mob.pig";
	}

	protected final String getDeathSound() {
		return "mob.pigdeath";
	}

	protected final int getDroppedItem() {
		return Item.porkRaw.shiftedIndex;
	}
}
