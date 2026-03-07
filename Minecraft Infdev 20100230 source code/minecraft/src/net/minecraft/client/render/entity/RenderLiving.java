package net.minecraft.client.render.entity;

import net.minecraft.client.model.ModelBase;
import net.minecraft.game.entity.Entity;
import net.minecraft.game.entity.EntityLiving;
import org.lwjgl.opengl.GL11;
import util.MathHelper;

public class RenderLiving extends Render {
	protected ModelBase mainModel;
	private ModelBase renderPassModel;

	public RenderLiving(ModelBase var1, float var2) {
		this.mainModel = var1;
		this.shadowSize = var2;
	}

	public final void setRenderPassModel(ModelBase var1) {
		this.renderPassModel = var1;
	}

	public void a(EntityLiving var1, double var2, double var4, double var6, float var8, float var9) {
		GL11.glPushMatrix();
		GL11.glDisable(GL11.GL_CULL_FACE);

		try {
			var8 = var1.prevRenderYawOffset + (var1.renderYawOffset - var1.prevRenderYawOffset) * var9;
			float var10 = var1.prevRotationYaw + (var1.rotationYaw - var1.prevRotationYaw) * var9;
			float var11 = var1.prevRotationPitch + (var1.rotationPitch - var1.prevRotationPitch) * var9;
			GL11.glTranslatef((float)var2, (float)var4, (float)var6);
			float var14 = (float)var1.ticksExisted + var9;
			GL11.glRotatef(180.0F - var8, 0.0F, 1.0F, 0.0F);
			float var3;
			if(var1.deathTime > 0) {
				var3 = ((float)var1.deathTime + var9 - 1.0F) / 20.0F * 1.6F;
				var3 = MathHelper.sqrt_float(var3);
				if(var3 > 1.0F) {
					var3 = 1.0F;
				}

				GL11.glRotatef(var3 * this.getDeathMaxRotation(var1), 0.0F, 0.0F, 1.0F);
			}

			GL11.glScalef(-(1.0F / 16.0F), -(1.0F / 16.0F), 1.0F / 16.0F);
			this.preRenderCallback(var1, var9);
			GL11.glTranslatef(0.0F, -24.0F, 0.0F);
			GL11.glEnable(GL11.GL_NORMALIZE);
			var3 = var1.newPosZ + (var1.newRotationYaw - var1.newPosZ) * var9;
			float var15 = var1.newRotationPitch - var1.newRotationYaw * (1.0F - var9);
			if(var3 > 1.0F) {
				var3 = 1.0F;
			}

			this.loadDownloadableImageTexture(var1.skinUrl, var1.getEntityTexture());
			GL11.glEnable(GL11.GL_ALPHA_TEST);
			this.mainModel.render(var15, var3, var14, var10 - var8, var11, 1.0F);

			for(int var5 = 0; var5 < 4; ++var5) {
				if(this.shouldRenderPass(var1, var5)) {
					this.renderPassModel.render(var15, var3, var14, var10 - var8, var11, 1.0F);
					GL11.glDisable(GL11.GL_BLEND);
					GL11.glEnable(GL11.GL_ALPHA_TEST);
				}
			}

			float var16 = var1.getEntityBrightness(var9);
			int var17 = this.getColorMultiplier(var1, var16, var9);
			if(var17 >>> 24 > 0 || var1.hurtTime > 0 || var1.deathTime > 0) {
				GL11.glDisable(GL11.GL_TEXTURE_2D);
				GL11.glDisable(GL11.GL_ALPHA_TEST);
				GL11.glEnable(GL11.GL_BLEND);
				GL11.glBlendFunc(GL11.GL_SRC_ALPHA, GL11.GL_ONE_MINUS_SRC_ALPHA);
				GL11.glDepthFunc(GL11.GL_EQUAL);
				if(var1.hurtTime > 0 || var1.deathTime > 0) {
					GL11.glColor4f(var16, 0.0F, 0.0F, 0.4F);
					this.mainModel.render(var15, var3, var14, var10 - var8, var11, 1.0F);

					for(int var7 = 0; var7 < 4; ++var7) {
						if(this.shouldRenderPass(var1, var7)) {
							GL11.glColor4f(var16, 0.0F, 0.0F, 0.4F);
							this.renderPassModel.render(var15, var3, var14, var10 - var8, var11, 1.0F);
						}
					}
				}

				if(var17 >>> 24 > 0) {
					float var19 = (float)(var17 >> 16 & 255) / 255.0F;
					var16 = (float)(var17 >> 8 & 255) / 255.0F;
					var9 = (float)(var17 & 255) / 255.0F;
					float var18 = (float)(var17 >>> 24) / 255.0F;
					GL11.glColor4f(var19, var16, var9, var18);
					this.mainModel.render(var15, var3, var14, var10 - var8, var11, 1.0F);

					for(int var12 = 0; var12 < 4; ++var12) {
						if(this.shouldRenderPass(var1, var12)) {
							GL11.glColor4f(var19, var16, var9, var18);
							this.renderPassModel.render(var15, var3, var14, var10 - var8, var11, 1.0F);
						}
					}
				}

				GL11.glDepthFunc(GL11.GL_LEQUAL);
				GL11.glDisable(GL11.GL_BLEND);
				GL11.glEnable(GL11.GL_ALPHA_TEST);
				GL11.glEnable(GL11.GL_TEXTURE_2D);
			}

			GL11.glDisable(GL11.GL_NORMALIZE);
		} catch (Exception var13) {
			var13.printStackTrace();
		}

		GL11.glEnable(GL11.GL_CULL_FACE);
		GL11.glPopMatrix();
	}

	protected boolean shouldRenderPass(EntityLiving var1, int var2) {
		return false;
	}

	protected float getDeathMaxRotation(EntityLiving var1) {
		return 90.0F;
	}

	protected int getColorMultiplier(EntityLiving var1, float var2, float var3) {
		return 0;
	}

	protected void preRenderCallback(EntityLiving var1, float var2) {
	}

	public void doRender(Entity var1, double var2, double var4, double var6, float var8, float var9) {
		this.a((EntityLiving)var1, var2, var4, var6, var8, var9);
	}
}
