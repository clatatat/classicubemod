package net.minecraft.client.effect;

import net.minecraft.client.render.Tessellator;
import net.minecraft.game.world.World;

public final class EntityFlameFX extends EntityFX {
	private float flameScale;

	public EntityFlameFX(World var1, double var2, double var4, double var6) {
		super(var1, var2, var4, var6, 0.0D, 0.0D, 0.0D);
		this.motionX *= (double)0.01F;
		this.motionY *= (double)0.01F;
		this.motionZ *= (double)0.01F;
		this.rand.nextFloat();
		this.rand.nextFloat();
		this.rand.nextFloat();
		this.rand.nextFloat();
		this.rand.nextFloat();
		this.rand.nextFloat();
		this.flameScale = this.particleScale;
		this.particleRed = this.particleGreen = this.particleBlue = 1.0F;
		this.particleMaxAge = (int)(8.0D / (Math.random() * 0.8D + 0.2D)) + 4;
		this.noClip = true;
		this.particleTextureIndex = 48;
	}

	public final void renderParticle(Tessellator var1, float var2, float var3, float var4, float var5, float var6, float var7) {
		float var8 = ((float)this.particleAge + var2) / (float)this.particleMaxAge;
		this.particleScale = this.flameScale * (1.0F - var8 * var8 * 0.5F);
		super.renderParticle(var1, var2, var3, var4, var5, var6, var7);
	}

	public final float getEntityBrightness(float var1) {
		float var2 = ((float)this.particleAge + var1) / (float)this.particleMaxAge;
		if(var2 < 0.0F) {
			var2 = 0.0F;
		}

		if(var2 > 1.0F) {
			var2 = 1.0F;
		}

		var1 = super.getEntityBrightness(var1);
		return var1 * var2 + (1.0F - var2);
	}

	public final void onUpdate() {
		this.prevPosX = this.posX;
		this.prevPosY = this.posY;
		this.prevPosZ = this.posZ;
		if(this.particleAge++ >= this.particleMaxAge) {
			this.setEntityDead();
		}

		this.moveEntity(this.motionX, this.motionY, this.motionZ);
		this.motionX *= (double)0.96F;
		this.motionY *= (double)0.96F;
		this.motionZ *= (double)0.96F;
		if(this.onGround) {
			this.motionX *= (double)0.7F;
			this.motionZ *= (double)0.7F;
		}

	}
}
