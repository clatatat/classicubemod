package net.minecraft.game.entity.monster;

import net.minecraft.game.item.Item;
import net.minecraft.game.world.World;

public class EntityZombie extends EntityMonster {
	public EntityZombie(World var1) {
		super(var1);
		this.texture = "/mob/zombie.png";
		this.moveSpeed = 0.5F;
		this.attackStrength = 5;
	}

	public final void onLivingUpdate() {
		if(this.worldObj.isDaytime()) {
			float var1 = this.getEntityBrightness(1.0F);
			if(var1 > 0.5F && this.worldObj.canBlockSeeTheSky((int)this.posX, (int)this.posY, (int)this.posZ) && this.rand.nextFloat() * 30.0F < (var1 - 0.4F) * 2.0F) {
				this.fire = 300;
			}
		}

		super.onLivingUpdate();
	}

	public final String getEntityType() {
		return "Zombie";
	}

	protected final int getDroppedItem() {
		return Item.feather.shiftedIndex;
	}
}
