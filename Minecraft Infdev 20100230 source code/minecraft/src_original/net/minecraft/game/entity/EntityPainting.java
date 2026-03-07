package net.minecraft.game.entity;

import com.mojang.nbt.NBTTagCompound;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.game.entity.misc.EntityItem;
import net.minecraft.game.item.Item;
import net.minecraft.game.item.ItemStack;
import net.minecraft.game.physics.AxisAlignedBB;
import net.minecraft.game.world.World;
import net.minecraft.game.world.material.Material;

public class EntityPainting extends Entity {
	private int tickCounter;
	public int direction;
	private int xPosition;
	private int yPosition;
	private int zPosition;
	public EnumArt art;

	private EntityPainting(World var1) {
		super(var1);
		this.tickCounter = 0;
		this.direction = 0;
		this.yOffset = 0.0F;
		this.setSize(0.5F, 0.5F);
	}

	public EntityPainting(World var1, int var2, int var3, int var4, int var5) {
		this(var1);
		this.xPosition = var2;
		this.yPosition = var3;
		this.zPosition = var4;
		ArrayList var7 = new ArrayList();
		EnumArt[] var8 = EnumArt.values();
		var3 = var8.length;

		for(var4 = 0; var4 < var3; ++var4) {
			EnumArt var6 = var8[var4];
			this.art = var6;
			this.setDirection(var5);
			if(this.onValidSurface()) {
				var7.add(var6);
			}
		}

		if(var7.size() > 0) {
			this.art = (EnumArt)var7.get(this.rand.nextInt(var7.size()));
		}

		this.setDirection(var5);
	}

	private void setDirection(int var1) {
		this.direction = var1;
		this.prevRotationYaw = this.rotationYaw = (float)(var1 * 90);
		float var2 = (float)this.art.sizeX;
		float var3 = (float)this.art.sizeY;
		float var4 = (float)this.art.sizeX;
		if(var1 != 0 && var1 != 2) {
			var2 = 0.5F;
		} else {
			var4 = 0.5F;
		}

		var2 /= 32.0F;
		var3 /= 32.0F;
		var4 /= 32.0F;
		float var5 = (float)this.xPosition + 0.5F;
		float var6 = (float)this.yPosition + 0.5F;
		float var7 = (float)this.zPosition + 0.5F;
		if(var1 == 0) {
			var7 -= 9.0F / 16.0F;
		}

		if(var1 == 1) {
			var5 -= 9.0F / 16.0F;
		}

		if(var1 == 2) {
			var7 += 9.0F / 16.0F;
		}

		if(var1 == 3) {
			var5 += 9.0F / 16.0F;
		}

		if(var1 == 0) {
			var5 -= getArtSize(this.art.sizeX);
		}

		if(var1 == 1) {
			var7 += getArtSize(this.art.sizeX);
		}

		if(var1 == 2) {
			var5 += getArtSize(this.art.sizeX);
		}

		if(var1 == 3) {
			var7 -= getArtSize(this.art.sizeX);
		}

		var6 += getArtSize(this.art.sizeY);
		this.setPosition((double)var5, (double)var6, (double)var7);
		this.boundingBox = new AxisAlignedBB((double)(var5 - var2), (double)(var6 - var3), (double)(var7 - var4), (double)(var5 + var2), (double)(var6 + var3), (double)(var7 + var4));
		double var13 = (double)0.00625F;
		AxisAlignedBB var27 = this.boundingBox;
		double var15 = var27.minX;
		double var17 = var27.minY;
		double var19 = var27.minZ;
		double var21 = var27.maxX;
		double var23 = var27.maxY;
		double var25 = var27.maxZ;
		var21 -= (double)0.00625F;
		var23 -= (double)0.00625F;
		var25 -= (double)0.00625F;
		this.boundingBox = new AxisAlignedBB(var15, var17, var19, var21, var23, var25);
	}

	private static float getArtSize(int var0) {
		return var0 == 32 ? 0.5F : (var0 == 64 ? 0.5F : 0.0F);
	}

	public final void onUpdate() {
		if(this.tickCounter++ == 100 && !this.onValidSurface()) {
			this.tickCounter = 0;
			this.setEntityDead();
			this.worldObj.spawnEntityInWorld(new EntityItem(this.worldObj, this.posX, this.posY, this.posZ, new ItemStack(Item.painting)));
		}

	}

	public final boolean onValidSurface() {
		if(this.worldObj.getCollidingBoundingBoxes(this.boundingBox).size() > 0) {
			return false;
		} else {
			int var1 = this.art.sizeX / 16;
			int var2 = this.art.sizeY / 16;
			int var3 = this.xPosition;
			int var5 = this.zPosition;
			if(this.direction == 0) {
				var3 = (int)(this.posX - (double)((float)this.art.sizeX / 32.0F));
			}

			if(this.direction == 1) {
				var5 = (int)(this.posZ - (double)((float)this.art.sizeX / 32.0F));
			}

			if(this.direction == 2) {
				var3 = (int)(this.posX - (double)((float)this.art.sizeX / 32.0F));
			}

			if(this.direction == 3) {
				var5 = (int)(this.posZ - (double)((float)this.art.sizeX / 32.0F));
			}

			int var4 = (int)(this.posY - (double)((float)this.art.sizeY / 32.0F));

			int var7;
			for(int var6 = 0; var6 < var1; ++var6) {
				for(var7 = 0; var7 < var2; ++var7) {
					Material var8;
					if(this.direction != 0 && this.direction != 2) {
						var8 = this.worldObj.getBlockMaterial(this.xPosition, var4 + var7, var5 + var6);
					} else {
						var8 = this.worldObj.getBlockMaterial(var3 + var6, var4 + var7, this.zPosition);
					}

					if(!var8.isSolid()) {
						return false;
					}
				}
			}

			List var9 = this.worldObj.a(this, this.boundingBox);

			for(var7 = 0; var7 < var9.size(); ++var7) {
				if(var9.get(var7) instanceof EntityPainting) {
					return false;
				}
			}

			return true;
		}
	}

	public final boolean canBeCollidedWith() {
		return true;
	}

	public final boolean attackEntityFrom(Entity var1, int var2) {
		this.setEntityDead();
		this.worldObj.spawnEntityInWorld(new EntityItem(this.worldObj, this.posX, this.posY, this.posZ, new ItemStack(Item.painting)));
		return true;
	}

	public final void writeEntityToNBT(NBTTagCompound var1) {
		var1.setByte("Dir", (byte)this.direction);
		var1.setString("Motive", this.art.title);
		var1.setInt("TileX", this.xPosition);
		var1.setInt("TileY", this.yPosition);
		var1.setInt("TileZ", this.zPosition);
	}

	public final String getEntityType() {
		return "Painting";
	}

	public final void readEntityFromNBT(NBTTagCompound var1) {
		this.direction = var1.getByte("Dir");
		this.xPosition = var1.getInt("TileX");
		this.yPosition = var1.getInt("TileY");
		this.zPosition = var1.getInt("TileZ");
		String var6 = var1.getString("Motive");
		EnumArt[] var2 = EnumArt.values();
		int var3 = var2.length;

		for(int var4 = 0; var4 < var3; ++var4) {
			EnumArt var5 = var2[var4];
			if(var5.title.equals(var6)) {
				this.art = var5;
			}
		}

		if(this.art == null) {
			this.art = EnumArt.Kebab;
		}

		this.setDirection(this.direction);
	}
}
