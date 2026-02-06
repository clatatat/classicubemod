package net.minecraft.src;

import java.util.List;
import java.util.Random;

public abstract class Entity {
	private static int nextEntityID = 0;
	public int entityID = nextEntityID++;
	public boolean preventEntitySpawning = false;
	public Entity riddenByEntity;
	public Entity ridingEntity;
	protected World worldObj;
	public double prevPosX;
	public double prevPosY;
	public double prevPosZ;
	public double posX;
	public double posY;
	public double posZ;
	public double motionX;
	public double motionY;
	public double motionZ;
	public float rotationYaw;
	public float rotationPitch;
	public float prevRotationYaw;
	public float prevRotationPitch;
	public final AxisAlignedBB boundingBox = AxisAlignedBB.getBoundingBox(0.0D, 0.0D, 0.0D, 0.0D, 0.0D, 0.0D);
	public boolean onGround = false;
	public boolean isCollidedHorizontally = false;
	public boolean isCollidedVertically = false;
	public boolean isCollided = true;
	public boolean surfaceCollision = false;
	public float at = 0.0F;
	public float yOffset = 0.6F;
	public float width = 1.8F;
	public float height = 0.0F;
	public float prevDistanceWalkedModified = 0.0F;
	protected boolean ay = true;
	protected float az = 0.0F;
	private int nextStepDistance = 1;
	public double aA;
	public double lastTickPosX;
	public double lastTickPosY;
	public float aD = 0.0F;
	public float ySize = 0.0F;
	public boolean aF = false;
	public float aG = 0.0F;
	public boolean aH = false;
	protected Random aI = new Random();
	public int aJ = 0;
	public int ticksExisted = 1;
	public int fireResistance = 0;
	protected int fire = 300;
	protected boolean aN = false;
	public int aO = 0;
	public int heartsLife = 300;
	private boolean firstUpdate = true;
	public String aQ;
	private double entityRiderPitchDelta;
	private double entityRiderYawDelta;

	public Entity(World var1) {
		this.worldObj = var1;
		this.b(0.0D, 0.0D, 0.0D);
	}

	protected void preparePlayerToSpawn() {
		if(this.worldObj != null) {
			while(this.posY > 0.0D) {
				this.b(this.posX, this.posY, this.posZ);
				if(this.worldObj.getCollidingBoundingBoxes(this, this.boundingBox).size() == 0) {
					break;
				}

				++this.posY;
			}

			this.motionX = this.motionY = this.motionZ = 0.0D;
			this.rotationPitch = 0.0F;
		}
	}

	public void setEntityDead() {
		this.surfaceCollision = true;
	}

	protected void setSize(float var1, float var2) {
		this.yOffset = var1;
		this.width = var2;
	}

	protected void setRotation(float var1, float var2) {
		this.rotationYaw = var1;
		this.rotationPitch = var2;
	}

	public void b(double var1, double var3, double var5) {
		this.posX = var1;
		this.posY = var3;
		this.posZ = var5;
		float var7 = this.yOffset / 2.0F;
		float var8 = this.width;
		this.boundingBox.setBounds(var1 - (double)var7, var3 - (double)this.at, var5 - (double)var7, var1 + (double)var7, var3 - (double)this.at + (double)var8, var5 + (double)var7);
	}

	public void setAngles(float var1, float var2) {
		float var3 = this.rotationPitch;
		float var4 = this.rotationYaw;
		this.rotationYaw = (float)((double)this.rotationYaw + (double)var1 * 0.15D);
		this.rotationPitch = (float)((double)this.rotationPitch - (double)var2 * 0.15D);
		if(this.rotationPitch < -90.0F) {
			this.rotationPitch = -90.0F;
		}

		if(this.rotationPitch > 90.0F) {
			this.rotationPitch = 90.0F;
		}

		this.prevRotationPitch += this.rotationPitch - var3;
		this.prevRotationYaw += this.rotationYaw - var4;
	}

	public void i_() {
		this.onEntityUpdate();
	}

	public void onEntityUpdate() {
		if(this.ridingEntity != null && this.ridingEntity.surfaceCollision) {
			this.ridingEntity = null;
		}

		++this.aJ;
		this.height = this.prevDistanceWalkedModified;
		this.prevPosX = this.posX;
		this.prevPosY = this.posY;
		this.prevPosZ = this.posZ;
		this.prevRotationPitch = this.rotationPitch;
		this.prevRotationYaw = this.rotationYaw;
		if(this.f_()) {
			if(!this.aN && !this.firstUpdate) {
				float var1 = MathHelper.sqrt_double(this.motionX * this.motionX * (double)0.2F + this.motionY * this.motionY + this.motionZ * this.motionZ * (double)0.2F) * 0.2F;
				if(var1 > 1.0F) {
					var1 = 1.0F;
				}

				this.worldObj.playSoundAtEntity(this, "random.splash", var1, 1.0F + (this.aI.nextFloat() - this.aI.nextFloat()) * 0.4F);
				float var2 = (float)MathHelper.floor_double(this.boundingBox.minY);

				int var3;
				float var4;
				float var5;
				for(var3 = 0; (float)var3 < 1.0F + this.yOffset * 20.0F; ++var3) {
					var4 = (this.aI.nextFloat() * 2.0F - 1.0F) * this.yOffset;
					var5 = (this.aI.nextFloat() * 2.0F - 1.0F) * this.yOffset;
					this.worldObj.spawnParticle("bubble", this.posX + (double)var4, (double)(var2 + 1.0F), this.posZ + (double)var5, this.motionX, this.motionY - (double)(this.aI.nextFloat() * 0.2F), this.motionZ);
				}

				for(var3 = 0; (float)var3 < 1.0F + this.yOffset * 20.0F; ++var3) {
					var4 = (this.aI.nextFloat() * 2.0F - 1.0F) * this.yOffset;
					var5 = (this.aI.nextFloat() * 2.0F - 1.0F) * this.yOffset;
					this.worldObj.spawnParticle("splash", this.posX + (double)var4, (double)(var2 + 1.0F), this.posZ + (double)var5, this.motionX, this.motionY, this.motionZ);
				}
			}

			this.az = 0.0F;
			this.aN = true;
			this.fireResistance = 0;
		} else {
			this.aN = false;
		}

		if(this.fireResistance > 0) {
			if(this.fireResistance % 20 == 0) {
				this.attackEntityFrom((Entity)null, 1);
			}

			--this.fireResistance;
		}

		if(this.handleLavaMovement()) {
			this.attackEntityFrom((Entity)null, 10);
			this.fireResistance = 600;
		}

		if(this.posY < -64.0D) {
			this.kill();
		}

		this.firstUpdate = false;
	}

	protected void kill() {
		this.setEntityDead();
	}

	public boolean c(double var1, double var3, double var5) {
		AxisAlignedBB var7 = this.boundingBox.getOffsetBoundingBox(var1, var3, var5);
		List var8 = this.worldObj.getCollidingBoundingBoxes(this, var7);
		return var8.size() > 0 ? false : !this.worldObj.getIsAnyLiquid(var7);
	}

	public void d(double var1, double var3, double var5) {
		if(this.aF) {
			this.boundingBox.offset(var1, var3, var5);
			this.posX = (this.boundingBox.minX + this.boundingBox.maxX) / 2.0D;
			this.posY = this.boundingBox.minY + (double)this.at - (double)this.aD;
			this.posZ = (this.boundingBox.minZ + this.boundingBox.maxZ) / 2.0D;
		} else {
			double var7 = this.posX;
			double var9 = this.posZ;
			double var11 = var1;
			double var13 = var3;
			double var15 = var5;
			AxisAlignedBB var17 = this.boundingBox.copy();
			List var18 = this.worldObj.getCollidingBoundingBoxes(this, this.boundingBox.addCoord(var1, var3, var5));

			for(int var19 = 0; var19 < var18.size(); ++var19) {
				var3 = ((AxisAlignedBB)var18.get(var19)).calculateYOffset(this.boundingBox, var3);
			}

			this.boundingBox.offset(0.0D, var3, 0.0D);
			if(!this.isCollided && var13 != var3) {
				var5 = 0.0D;
				var3 = var5;
				var1 = var5;
			}

			boolean var34 = this.onGround || var13 != var3 && var13 < 0.0D;

			int var20;
			for(var20 = 0; var20 < var18.size(); ++var20) {
				var1 = ((AxisAlignedBB)var18.get(var20)).calculateXOffset(this.boundingBox, var1);
			}

			this.boundingBox.offset(var1, 0.0D, 0.0D);
			if(!this.isCollided && var11 != var1) {
				var5 = 0.0D;
				var3 = var5;
				var1 = var5;
			}

			for(var20 = 0; var20 < var18.size(); ++var20) {
				var5 = ((AxisAlignedBB)var18.get(var20)).calculateZOffset(this.boundingBox, var5);
			}

			this.boundingBox.offset(0.0D, 0.0D, var5);
			if(!this.isCollided && var15 != var5) {
				var5 = 0.0D;
				var3 = var5;
				var1 = var5;
			}

			double var22;
			int var27;
			double var35;
			if(this.ySize > 0.0F && var34 && this.aD < 0.05F && (var11 != var1 || var15 != var5)) {
				var35 = var1;
				var22 = var3;
				double var24 = var5;
				var1 = var11;
				var3 = (double)this.ySize;
				var5 = var15;
				AxisAlignedBB var26 = this.boundingBox.copy();
				this.boundingBox.setBB(var17);
				var18 = this.worldObj.getCollidingBoundingBoxes(this, this.boundingBox.addCoord(var11, var3, var15));

				for(var27 = 0; var27 < var18.size(); ++var27) {
					var3 = ((AxisAlignedBB)var18.get(var27)).calculateYOffset(this.boundingBox, var3);
				}

				this.boundingBox.offset(0.0D, var3, 0.0D);
				if(!this.isCollided && var13 != var3) {
					var5 = 0.0D;
					var3 = var5;
					var1 = var5;
				}

				for(var27 = 0; var27 < var18.size(); ++var27) {
					var1 = ((AxisAlignedBB)var18.get(var27)).calculateXOffset(this.boundingBox, var1);
				}

				this.boundingBox.offset(var1, 0.0D, 0.0D);
				if(!this.isCollided && var11 != var1) {
					var5 = 0.0D;
					var3 = var5;
					var1 = var5;
				}

				for(var27 = 0; var27 < var18.size(); ++var27) {
					var5 = ((AxisAlignedBB)var18.get(var27)).calculateZOffset(this.boundingBox, var5);
				}

				this.boundingBox.offset(0.0D, 0.0D, var5);
				if(!this.isCollided && var15 != var5) {
					var5 = 0.0D;
					var3 = var5;
					var1 = var5;
				}

				if(var35 * var35 + var24 * var24 >= var1 * var1 + var5 * var5) {
					var1 = var35;
					var3 = var22;
					var5 = var24;
					this.boundingBox.setBB(var26);
				} else {
					this.aD = (float)((double)this.aD + 0.5D);
				}
			}

			this.posX = (this.boundingBox.minX + this.boundingBox.maxX) / 2.0D;
			this.posY = this.boundingBox.minY + (double)this.at - (double)this.aD;
			this.posZ = (this.boundingBox.minZ + this.boundingBox.maxZ) / 2.0D;
			this.isCollidedHorizontally = var11 != var1 || var15 != var5;
			this.onGround = var13 != var3 && var13 < 0.0D;
			this.isCollidedVertically = this.isCollidedHorizontally || var13 != var3;
			if(this.onGround) {
				if(this.az > 0.0F) {
					this.fall(this.az);
					this.az = 0.0F;
				}
			} else if(var3 < 0.0D) {
				this.az = (float)((double)this.az - var3);
			}

			if(var11 != var1) {
				this.motionX = 0.0D;
			}

			if(var13 != var3) {
				this.motionY = 0.0D;
			}

			if(var15 != var5) {
				this.motionZ = 0.0D;
			}

			var35 = this.posX - var7;
			var22 = this.posZ - var9;
			this.prevDistanceWalkedModified = (float)((double)this.prevDistanceWalkedModified + (double)MathHelper.sqrt_double(var35 * var35 + var22 * var22) * 0.6D);
			int var25;
			int var36;
			int var38;
			if(this.ay) {
				var36 = MathHelper.floor_double(this.posX);
				var25 = MathHelper.floor_double(this.posY - (double)0.2F - (double)this.at);
				var38 = MathHelper.floor_double(this.posZ);
				var27 = this.worldObj.getBlockId(var36, var25, var38);
				if(this.prevDistanceWalkedModified > (float)this.nextStepDistance && var27 > 0) {
					++this.nextStepDistance;
					StepSound var28 = Block.blocksList[var27].stepSound;
					if(this.worldObj.getBlockId(var36, var25 + 1, var38) == Block.snow.blockID) {
						var28 = Block.snow.stepSound;
						this.worldObj.playSoundAtEntity(this, var28.getStepSound(), var28.getVolume() * 0.15F, var28.getPitch());
					} else if(!Block.blocksList[var27].material.getIsLiquid()) {
						this.worldObj.playSoundAtEntity(this, var28.getStepSound(), var28.getVolume() * 0.15F, var28.getPitch());
					}

					Block.blocksList[var27].onEntityWalking(this.worldObj, var36, var25, var38, this);
				}
			}

			var36 = MathHelper.floor_double(this.boundingBox.minX);
			var25 = MathHelper.floor_double(this.boundingBox.minY);
			var38 = MathHelper.floor_double(this.boundingBox.minZ);
			var27 = MathHelper.floor_double(this.boundingBox.maxX);
			int var39 = MathHelper.floor_double(this.boundingBox.maxY);
			int var29 = MathHelper.floor_double(this.boundingBox.maxZ);

			for(int var30 = var36; var30 <= var27; ++var30) {
				for(int var31 = var25; var31 <= var39; ++var31) {
					for(int var32 = var38; var32 <= var29; ++var32) {
						int var33 = this.worldObj.getBlockId(var30, var31, var32);
						if(var33 > 0) {
							Block.blocksList[var33].onEntityCollidedWithBlock(this.worldObj, var30, var31, var32, this);
						}
					}
				}
			}

			this.aD *= 0.4F;
			boolean var37 = this.f_();
			if(this.worldObj.isBoundingBoxBurning(this.boundingBox)) {
				this.dealFireDamage(1);
				if(!var37) {
					++this.fireResistance;
					if(this.fireResistance == 0) {
						this.fireResistance = 300;
					}
				}
			} else if(this.fireResistance <= 0) {
				this.fireResistance = -this.ticksExisted;
			}

			if(var37 && this.fireResistance > 0) {
				this.worldObj.playSoundAtEntity(this, "random.fizz", 0.7F, 1.6F + (this.aI.nextFloat() - this.aI.nextFloat()) * 0.4F);
				this.fireResistance = -this.ticksExisted;
			}

		}
	}

	public AxisAlignedBB e_() {
		return null;
	}

	protected void dealFireDamage(int var1) {
		this.attackEntityFrom((Entity)null, var1);
	}

	protected void fall(float var1) {
	}

	public boolean f_() {
		return this.worldObj.handleMaterialAcceleration(this.boundingBox.expand(0.0D, (double)-0.4F, 0.0D), Material.water, this);
	}

	public boolean isInsideOfMaterial(Material var1) {
		double var2 = this.posY + (double)this.getEyeHeight();
		int var4 = MathHelper.floor_double(this.posX);
		int var5 = MathHelper.floor_float((float)MathHelper.floor_double(var2));
		int var6 = MathHelper.floor_double(this.posZ);
		int var7 = this.worldObj.getBlockId(var4, var5, var6);
		if(var7 != 0 && Block.blocksList[var7].material == var1) {
			float var8 = BlockFluid.getFluidHeightPercent(this.worldObj.getBlockMetadata(var4, var5, var6)) - 1.0F / 9.0F;
			float var9 = (float)(var5 + 1) - var8;
			return var2 < (double)var9;
		} else {
			return false;
		}
	}

	protected float getEyeHeight() {
		return 0.0F;
	}

	public boolean handleLavaMovement() {
		return this.worldObj.isMaterialInBB(this.boundingBox.expand(0.0D, (double)-0.4F, 0.0D), Material.lava);
	}

	public void moveFlying(float var1, float var2, float var3) {
		float var4 = MathHelper.sqrt_float(var1 * var1 + var2 * var2);
		if(var4 >= 0.01F) {
			if(var4 < 1.0F) {
				var4 = 1.0F;
			}

			var4 = var3 / var4;
			var1 *= var4;
			var2 *= var4;
			float var5 = MathHelper.sin(this.rotationYaw * (float)Math.PI / 180.0F);
			float var6 = MathHelper.cos(this.rotationYaw * (float)Math.PI / 180.0F);
			this.motionX += (double)(var1 * var6 - var2 * var5);
			this.motionZ += (double)(var2 * var6 + var1 * var5);
		}
	}

	public float getBrightness(float var1) {
		int var2 = MathHelper.floor_double(this.posX);
		double var3 = (this.boundingBox.maxY - this.boundingBox.minY) * 0.66D;
		int var5 = MathHelper.floor_double(this.posY - (double)this.at + var3);
		int var6 = MathHelper.floor_double(this.posZ);
		return this.worldObj.getBrightness(var2, var5, var6);
	}

	public void setWorld(World var1) {
		this.worldObj = var1;
	}

	public void setPositionAndRotation(double var1, double var3, double var5, float var7, float var8) {
		this.prevPosX = this.posX = var1;
		this.prevPosY = this.posY = var3;
		this.prevPosZ = this.posZ = var5;
		this.rotationYaw = var7;
		this.rotationPitch = var8;
		double var9 = (double)(this.prevRotationYaw - var7);
		if(var9 < -180.0D) {
			this.prevRotationYaw += 360.0F;
		}

		if(var9 >= 180.0D) {
			this.prevRotationYaw -= 360.0F;
		}

		this.b(this.posX, this.posY, this.posZ);
	}

	public void setLocationAndAngles(double var1, double var3, double var5, float var7, float var8) {
		this.prevPosX = this.posX = var1;
		this.prevPosY = this.posY = var3 + (double)this.at;
		this.prevPosZ = this.posZ = var5;
		this.rotationYaw = var7;
		this.rotationPitch = var8;
		this.b(this.posX, this.posY, this.posZ);
	}

	public float getDistanceToEntity(Entity var1) {
		float var2 = (float)(this.posX - var1.posX);
		float var3 = (float)(this.posY - var1.posY);
		float var4 = (float)(this.posZ - var1.posZ);
		return MathHelper.sqrt_float(var2 * var2 + var3 * var3 + var4 * var4);
	}

	public double getDistance(double var1, double var3, double var5) {
		double var7 = this.posX - var1;
		double var9 = this.posY - var3;
		double var11 = this.posZ - var5;
		return var7 * var7 + var9 * var9 + var11 * var11;
	}

	public double f(double var1, double var3, double var5) {
		double var7 = this.posX - var1;
		double var9 = this.posY - var3;
		double var11 = this.posZ - var5;
		return (double)MathHelper.sqrt_double(var7 * var7 + var9 * var9 + var11 * var11);
	}

	public double getDistanceSqToEntity(Entity var1) {
		double var2 = this.posX - var1.posX;
		double var4 = this.posY - var1.posY;
		double var6 = this.posZ - var1.posZ;
		return var2 * var2 + var4 * var4 + var6 * var6;
	}

	public void onCollideWithPlayer(EntityPlayer var1) {
	}

	public void applyEntityCollision(Entity var1) {
		if(var1.riddenByEntity != this && var1.ridingEntity != this) {
			double var2 = var1.posX - this.posX;
			double var4 = var1.posZ - this.posZ;
			double var6 = MathHelper.abs_max(var2, var4);
			if(var6 >= (double)0.01F) {
				var6 = (double)MathHelper.sqrt_double(var6);
				var2 /= var6;
				var4 /= var6;
				double var8 = 1.0D / var6;
				if(var8 > 1.0D) {
					var8 = 1.0D;
				}

				var2 *= var8;
				var4 *= var8;
				var2 *= (double)0.05F;
				var4 *= (double)0.05F;
				var2 *= (double)(1.0F - this.aG);
				var4 *= (double)(1.0F - this.aG);
				this.g(-var2, 0.0D, -var4);
				var1.g(var2, 0.0D, var4);
			}

		}
	}

	public void g(double var1, double var3, double var5) {
		this.motionX += var1;
		this.motionY += var3;
		this.motionZ += var5;
	}

	public boolean attackEntityFrom(Entity var1, int var2) {
		return false;
	}

	public boolean canBeCollidedWith() {
		return false;
	}

	public boolean canBePushed() {
		return false;
	}

	public void addToPlayerScore(Entity var1, int var2) {
	}

	public boolean isInRangeToRenderVec3D(Vec3D var1) {
		double var2 = this.posX - var1.xCoord;
		double var4 = this.posY - var1.yCoord;
		double var6 = this.posZ - var1.zCoord;
		double var8 = var2 * var2 + var4 * var4 + var6 * var6;
		return this.isInRangeToRenderDist(var8);
	}

	public boolean isInRangeToRenderDist(double var1) {
		double var3 = this.boundingBox.getAverageEdgeLength();
		var3 *= 64.0D;
		return var1 < var3 * var3;
	}

	public String getTexture() {
		return null;
	}

	public boolean addEntityID(NBTTagCompound var1) {
		String var2 = this.getEntityString();
		if(!this.surfaceCollision && var2 != null) {
			var1.setString("id", var2);
			this.writeToNBT(var1);
			return true;
		} else {
			return false;
		}
	}

	public void writeToNBT(NBTTagCompound var1) {
		var1.setTag("Pos", this.newDoubleNBTList(new double[]{this.posX, this.posY, this.posZ}));
		var1.setTag("Motion", this.newDoubleNBTList(new double[]{this.motionX, this.motionY, this.motionZ}));
		var1.setTag("Rotation", this.newFloatNBTList(new float[]{this.rotationYaw, this.rotationPitch}));
		var1.setFloat("FallDistance", this.az);
		var1.setShort("Fire", (short)this.fireResistance);
		var1.setShort("Air", (short)this.heartsLife);
		var1.setBoolean("OnGround", this.onGround);
		this.writeEntityToNBT(var1);
	}

	public void readFromNBT(NBTTagCompound var1) {
		NBTTagList var2 = var1.getTagList("Pos");
		NBTTagList var3 = var1.getTagList("Motion");
		NBTTagList var4 = var1.getTagList("Rotation");
		this.b(0.0D, 0.0D, 0.0D);
		this.motionX = ((NBTTagDouble)var3.tagAt(0)).doubleValue;
		this.motionY = ((NBTTagDouble)var3.tagAt(1)).doubleValue;
		this.motionZ = ((NBTTagDouble)var3.tagAt(2)).doubleValue;
		this.prevPosX = this.aA = this.posX = ((NBTTagDouble)var2.tagAt(0)).doubleValue;
		this.prevPosY = this.lastTickPosX = this.posY = ((NBTTagDouble)var2.tagAt(1)).doubleValue;
		this.prevPosZ = this.lastTickPosY = this.posZ = ((NBTTagDouble)var2.tagAt(2)).doubleValue;
		this.prevRotationYaw = this.rotationYaw = ((NBTTagFloat)var4.tagAt(0)).floatValue;
		this.prevRotationPitch = this.rotationPitch = ((NBTTagFloat)var4.tagAt(1)).floatValue;
		this.az = var1.getFloat("FallDistance");
		this.fireResistance = var1.getShort("Fire");
		this.heartsLife = var1.getShort("Air");
		this.onGround = var1.getBoolean("OnGround");
		this.b(this.posX, this.posY, this.posZ);
		this.readEntityFromNBT(var1);
	}

	protected final String getEntityString() {
		return EntityList.getEntityString(this);
	}

	protected abstract void readEntityFromNBT(NBTTagCompound var1);

	protected abstract void writeEntityToNBT(NBTTagCompound var1);

	protected NBTTagList newDoubleNBTList(double... var1) {
		NBTTagList var2 = new NBTTagList();
		double[] var3 = var1;
		int var4 = var1.length;

		for(int var5 = 0; var5 < var4; ++var5) {
			double var6 = var3[var5];
			var2.setTag(new NBTTagDouble(var6));
		}

		return var2;
	}

	protected NBTTagList newFloatNBTList(float... var1) {
		NBTTagList var2 = new NBTTagList();
		float[] var3 = var1;
		int var4 = var1.length;

		for(int var5 = 0; var5 < var4; ++var5) {
			float var6 = var3[var5];
			var2.setTag(new NBTTagFloat(var6));
		}

		return var2;
	}

	public EntityItem dropItem(int var1, int var2) {
		return this.entityDropItem(var1, var2, 0.0F);
	}

	public EntityItem entityDropItem(int var1, int var2, float var3) {
		EntityItem var4 = new EntityItem(this.worldObj, this.posX, this.posY + (double)var3, this.posZ, new ItemStack(var1, var2));
		var4.delayBeforeCanPickup = 10;
		this.worldObj.spawnEntityInWorld(var4);
		return var4;
	}

	public boolean isEntityAlive() {
		return !this.surfaceCollision;
	}

	public boolean isEntityInsideOpaqueBlock() {
		int var1 = MathHelper.floor_double(this.posX);
		int var2 = MathHelper.floor_double(this.posY + (double)this.getEyeHeight());
		int var3 = MathHelper.floor_double(this.posZ);
		return this.worldObj.isBlockNormalCube(var1, var2, var3);
	}

	public boolean interact(EntityPlayer var1) {
		return false;
	}

	public AxisAlignedBB getCollisionBox(Entity var1) {
		return null;
	}

	public void updateRidden() {
		if(this.ridingEntity.surfaceCollision) {
			this.ridingEntity = null;
		} else {
			this.motionX = 0.0D;
			this.motionY = 0.0D;
			this.motionZ = 0.0D;
			this.i_();
			this.b(this.ridingEntity.posX, this.ridingEntity.posY + this.ridingEntity.g_() + this.getYOffset(), this.ridingEntity.posZ);
			this.entityRiderYawDelta += (double)(this.ridingEntity.rotationYaw - this.ridingEntity.prevRotationYaw);

			for(this.entityRiderPitchDelta += (double)(this.ridingEntity.rotationPitch - this.ridingEntity.prevRotationPitch); this.entityRiderYawDelta >= 180.0D; this.entityRiderYawDelta -= 360.0D) {
			}

			while(this.entityRiderYawDelta < -180.0D) {
				this.entityRiderYawDelta += 360.0D;
			}

			while(this.entityRiderPitchDelta >= 180.0D) {
				this.entityRiderPitchDelta -= 360.0D;
			}

			while(this.entityRiderPitchDelta < -180.0D) {
				this.entityRiderPitchDelta += 360.0D;
			}

			double var1 = this.entityRiderYawDelta * 0.5D;
			double var3 = this.entityRiderPitchDelta * 0.5D;
			float var5 = 10.0F;
			if(var1 > (double)var5) {
				var1 = (double)var5;
			}

			if(var1 < (double)(-var5)) {
				var1 = (double)(-var5);
			}

			if(var3 > (double)var5) {
				var3 = (double)var5;
			}

			if(var3 < (double)(-var5)) {
				var3 = (double)(-var5);
			}

			this.entityRiderYawDelta -= var1;
			this.entityRiderPitchDelta -= var3;
			this.rotationYaw = (float)((double)this.rotationYaw + var1);
			this.rotationPitch = (float)((double)this.rotationPitch + var3);
		}
	}

	protected double getYOffset() {
		return (double)this.at;
	}

	public double g_() {
		return (double)this.width * 0.75D;
	}

	public void mountEntity(Entity var1) {
		this.entityRiderPitchDelta = 0.0D;
		this.entityRiderYawDelta = 0.0D;
		if(this.ridingEntity == var1) {
			this.ridingEntity.riddenByEntity = null;
			this.ridingEntity = null;
			this.setLocationAndAngles(var1.posX, var1.boundingBox.minY + (double)var1.width, var1.posZ, this.rotationYaw, this.rotationPitch);
		} else {
			if(this.ridingEntity != null) {
				this.ridingEntity.riddenByEntity = null;
			}

			if(var1.riddenByEntity != null) {
				var1.riddenByEntity.ridingEntity = null;
			}

			this.ridingEntity = var1;
			var1.riddenByEntity = this;
		}
	}
}
