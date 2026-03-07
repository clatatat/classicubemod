package net.minecraft.game.entity.player;

import com.mojang.nbt.NBTTagCompound;
import java.util.List;
import net.minecraft.game.IInventory;
import net.minecraft.game.entity.Entity;
import net.minecraft.game.entity.EntityLiving;
import net.minecraft.game.entity.misc.EntityItem;
import net.minecraft.game.entity.monster.EntityMonster;
import net.minecraft.game.entity.projectile.EntityArrow;
import net.minecraft.game.item.Item;
import net.minecraft.game.item.ItemArmor;
import net.minecraft.game.item.ItemStack;
import net.minecraft.game.world.World;
import net.minecraft.game.world.block.Block;
import net.minecraft.game.world.block.tileentity.TileEntityFurnace;
import net.minecraft.game.world.material.Material;
import util.MathHelper;

public class EntityPlayer extends EntityLiving {
	public InventoryPlayer inventory = new InventoryPlayer(this);
	public byte unusedMiningCooldown = 0;
	public int score = 0;
	public float prevCameraYaw;
	public float cameraYaw;
	protected String username;
	private int damageRemainder = 0;

	public EntityPlayer(World var1) {
		super(var1);
		if(var1 != null) {
			var1.playerEntity = this;
			World.b(this);
		}

		this.setLocationAndAngles((double)var1.spawnX + 0.5D, (double)var1.spawnY, (double)var1.spawnZ + 0.5D, 0.0F, 0.0F);
		this.yOffset = 1.62F;
		this.health = 20;
		this.fireResistance = 20;
		this.texture = "/char.png";
	}

	public final void preparePlayerToSpawn() {
		this.yOffset = 1.62F;
		this.setSize(0.6F, 1.8F);
		super.preparePlayerToSpawn();
		if(this.worldObj != null) {
			this.worldObj.playerEntity = this;
		}

		this.health = 20;
		this.deathTime = 0;
	}

	public void onLivingUpdate() {
		if(this.worldObj.difficultySetting == 0 && this.health < 20 && this.ticksExisted % 20 << 2 == 0) {
			this.heal(1);
		}

		InventoryPlayer var3 = this.inventory;

		for(int var4 = 0; var4 < var3.mainInventory.length; ++var4) {
			if(var3.mainInventory[var4] != null && var3.mainInventory[var4].animationsToGo > 0) {
				--var3.mainInventory[var4].animationsToGo;
			}
		}

		this.prevCameraYaw = this.cameraYaw;
		super.onLivingUpdate();
		float var1 = MathHelper.sqrt_double(this.motionX * this.motionX + this.motionZ * this.motionZ);
		float var2 = (float)Math.atan(-this.motionY * (double)0.2F) * 15.0F;
		if(var1 > 0.1F) {
			var1 = 0.1F;
		}

		if(!this.onGround || this.health <= 0) {
			var1 = 0.0F;
		}

		if(this.onGround || this.health <= 0) {
			var2 = 0.0F;
		}

		this.cameraYaw += (var1 - this.cameraYaw) * 0.4F;
		this.cameraPitch += (var2 - this.cameraPitch) * 0.8F;
		if(this.health > 0) {
			List var5 = this.worldObj.a(this, this.boundingBox.expand(1.0D, 0.0D, 1.0D));
			if(var5 != null) {
				for(int var6 = 0; var6 < var5.size(); ++var6) {
					Entity var7 = (Entity)var5.get(var6);
					var7.onCollideWithPlayer(this);
				}
			}
		}

	}

	public final void onDeath(Entity var1) {
		this.setSize(0.2F, 0.2F);
		this.setPosition(this.posX, this.posY, this.posZ);
		this.motionY = (double)0.1F;
		if(this.username.equals("Notch")) {
			this.dropPlayerItemWithRandomChoice(new ItemStack(Item.apple, 1), true);
		}

		this.inventory.dropAllItems();
		if(var1 != null) {
			this.motionX = (double)(-MathHelper.cos((this.attackedAtYaw + this.rotationYaw) * (float)Math.PI / 180.0F) * 0.1F);
			this.motionZ = (double)(-MathHelper.sin((this.attackedAtYaw + this.rotationYaw) * (float)Math.PI / 180.0F) * 0.1F);
		} else {
			this.motionX = this.motionZ = 0.0D;
		}

		this.yOffset = 0.1F;
	}

	public final void setEntityDead() {
	}

	public final void dropPlayerItem(ItemStack var1) {
		this.dropPlayerItemWithRandomChoice(var1, false);
	}

	public final void dropPlayerItemWithRandomChoice(ItemStack var1, boolean var2) {
		if(var1 != null) {
			EntityItem var4 = new EntityItem(this.worldObj, this.posX, this.posY - (double)0.3F, this.posZ, var1);
			var4.delayBeforeCanPickup = 40;
			float var3;
			float var5;
			if(var2) {
				var3 = this.rand.nextFloat() * 0.5F;
				var5 = this.rand.nextFloat() * (float)Math.PI * 2.0F;
				var4.motionX = (double)(-MathHelper.sin(var5) * var3);
				var4.motionZ = (double)(MathHelper.cos(var5) * var3);
				var4.motionY = (double)0.2F;
			} else {
				var4.motionX = (double)(-MathHelper.sin(this.rotationYaw / 180.0F * (float)Math.PI) * MathHelper.cos(this.rotationPitch / 180.0F * (float)Math.PI) * 0.3F);
				var4.motionZ = (double)(MathHelper.cos(this.rotationYaw / 180.0F * (float)Math.PI) * MathHelper.cos(this.rotationPitch / 180.0F * (float)Math.PI) * 0.3F);
				var4.motionY = (double)(-MathHelper.sin(this.rotationPitch / 180.0F * (float)Math.PI) * 0.3F + 0.1F);
				var3 = this.rand.nextFloat() * (float)Math.PI * 2.0F;
				var5 = 0.02F * this.rand.nextFloat();
				var4.motionX += Math.cos((double)var3) * (double)var5;
				var4.motionY += (double)((this.rand.nextFloat() - this.rand.nextFloat()) * 0.1F);
				var4.motionZ += Math.sin((double)var3) * (double)var5;
			}

			this.worldObj.spawnEntityInWorld(var4);
		}
	}

	public final boolean canHarvestBlock(Block var1) {
		Block var2 = var1;
		InventoryPlayer var3 = this.inventory;
		if(var2.blockMaterial != Material.rock && var2.blockMaterial != Material.iron) {
			return true;
		} else {
			ItemStack var4 = var3.getStackInSlot(var3.currentItem);
			return var4 != null ? Item.itemsList[var4.itemID].canHarvestBlock(var2) : false;
		}
	}

	public void readEntityFromNBT(NBTTagCompound var1) {
		super.readEntityFromNBT(var1);
	}

	public void writeEntityToNBT(NBTTagCompound var1) {
		super.writeEntityToNBT(var1);
	}

	public String getEntityType() {
		return null;
	}

	public void displayChestGUI(IInventory var1) {
	}

	public void displayWorkbenchGUI() {
	}

	public void onItemPickup(Entity var1) {
	}

	protected final float getEyeHeight() {
		return 0.12F;
	}

	public final boolean attackEntityFrom(Entity var1, int var2) {
		this.entityAge = 0;
		if(this.health <= 0) {
			return false;
		} else if((float)this.heartsLife > (float)this.heartsHalvesLife / 2.0F) {
			return false;
		} else {
			if(var1 instanceof EntityMonster || var1 instanceof EntityArrow) {
				if(this.worldObj.difficultySetting == 0) {
					var2 = 0;
				}

				if(this.worldObj.difficultySetting == 1) {
					var2 = var2 / 3 + 1;
				}

				if(this.worldObj.difficultySetting == 3) {
					var2 = var2 * 3 / 2;
				}
			}

			int var3 = 25 - this.inventory.getPlayerArmorValue();
			var3 = var2 * var3 + this.damageRemainder;
			int var4 = var2;
			InventoryPlayer var6 = this.inventory;

			for(int var5 = 0; var5 < var6.armorInventory.length; ++var5) {
				if(var6.armorInventory[var5] != null && var6.armorInventory[var5].getItem() instanceof ItemArmor) {
					var6.armorInventory[var5].damageItem(var4);
					if(var6.armorInventory[var5].stackSize == 0) {
						var6.armorInventory[var5] = null;
					}
				}
			}

			var2 = var3 / 25;
			this.damageRemainder = var3 % 25;
			if(var2 == 0) {
				return false;
			} else {
				return super.attackEntityFrom(var1, var2);
			}
		}
	}

	public void displayFurnaceGUI(TileEntityFurnace var1) {
	}
}
