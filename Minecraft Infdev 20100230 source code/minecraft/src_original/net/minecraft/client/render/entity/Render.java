package net.minecraft.client.render.entity;

import net.minecraft.client.model.ModelBiped;
import net.minecraft.client.render.RenderBlocks;
import net.minecraft.client.render.RenderEngine;
import net.minecraft.client.render.Tessellator;
import net.minecraft.game.entity.Entity;
import net.minecraft.game.world.World;
import net.minecraft.game.world.block.Block;
import org.lwjgl.opengl.GL11;
import util.MathHelper;

public abstract class Render {
	protected RenderManager renderManager;
	protected float shadowSize;
	protected float shadowOpaque;

	public Render() {
		new ModelBiped();
		new RenderBlocks();
		this.shadowSize = 0.0F;
		this.shadowOpaque = 1.0F;
	}

	public abstract void doRender(Entity var1, double var2, double var4, double var6, float var8, float var9);

	protected final void loadTexture(String var1) {
		RenderEngine var2 = this.renderManager.renderEngine;
		RenderEngine.bindTexture(var2.getTexture(var1));
	}

	protected final void loadDownloadableImageTexture(String var1, String var2) {
		RenderEngine var3 = this.renderManager.renderEngine;
		RenderEngine.bindTexture(var3.a(var1, var2));
	}

	public final void setRenderManager(RenderManager var1) {
		this.renderManager = var1;
	}

	public final void renderShadow(Entity var1, double var2, double var4, double var6, float var8) {
		int var68;
		float var11;
		if(this.shadowSize > 0.0F) {
			double var9 = this.renderManager.getDistanceToCamera(var2, var4, var6);
			var8 = (float)((1.0D - var9 / 256.0D) * (double)this.shadowOpaque);
			if(var8 > 0.0F) {
				float var19 = var8;
				double var17 = var6;
				double var15 = var4;
				double var13 = var2;
				Render var66 = this;
				GL11.glEnable(GL11.GL_BLEND);
				RenderEngine var67 = this.renderManager.renderEngine;
				RenderEngine.bindTexture(var67.getTexture("%%/shadow.png"));
				World var10 = this.renderManager.worldObj;
				GL11.glDepthMask(false);
				var11 = this.shadowSize;
				var68 = MathHelper.floor_double(var2 - (double)var11);
				int var12 = MathHelper.floor_double(var2 + (double)var11);
				int var20 = MathHelper.floor_double(var4 - (double)var11);
				int var21 = MathHelper.floor_double(var4);
				int var22 = MathHelper.floor_double(var6 - (double)var11);
				int var23 = MathHelper.floor_double(var6 + (double)var11);

				for(var68 = var68; var68 <= var12; ++var68) {
					for(int var24 = var20; var24 <= var21; ++var24) {
						for(int var25 = var22; var25 <= var23; ++var25) {
							int var26 = var10.getBlockId(var68, var24 - 1, var25);
							if(var26 > 0 && var10.f(var68, var24, var25) > 3) {
								Block var27 = Block.blocksList[var26];
								Tessellator var33 = Tessellator.instance;
								double var48 = ((double)var19 - (var15 - (double)var24) / 2.0D) * 0.5D * (double)var66.renderManager.worldObj.getBrightness(var68, var24, var25);
								if(var48 >= 0.0D) {
									GL11.glColor4f(1.0F, 1.0F, 1.0F, (float)var48);
									var33.startDrawingQuads();
									double var50 = (double)var68 + var27.minX;
									double var52 = (double)var68 + var27.maxX;
									double var54 = (double)var24 + var27.minY;
									double var56 = (double)var25 + var27.minZ;
									double var58 = (double)var25 + var27.maxZ;
									float var79 = (float)((var13 - var50) / 2.0D / (double)var11 + 0.5D);
									float var80 = (float)((var13 - var52) / 2.0D / (double)var11 + 0.5D);
									float var28 = (float)((var17 - var56) / 2.0D / (double)var11 + 0.5D);
									float var29 = (float)((var17 - var58) / 2.0D / (double)var11 + 0.5D);
									var33.addVertexWithUV(var50, var54, var56, (double)var79, (double)var28);
									var33.addVertexWithUV(var50, var54, var58, (double)var79, (double)var29);
									var33.addVertexWithUV(var52, var54, var58, (double)var80, (double)var29);
									var33.addVertexWithUV(var52, var54, var56, (double)var80, (double)var28);
									var33.draw();
								}
							}
						}
					}
				}

				GL11.glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
				GL11.glDisable(GL11.GL_BLEND);
				GL11.glDepthMask(true);
			}
		}

		if(var1.fire > 0) {
			GL11.glDisable(GL11.GL_LIGHTING);
			int var65 = Block.fire.blockIndexInTexture;
			var68 = (var65 & 15) << 4;
			int var69 = var65 & 240;
			var11 = (float)var68 / 256.0F;
			float var70 = ((float)var68 + 15.99F) / 256.0F;
			float var71 = (float)var69 / 256.0F;
			float var73 = ((float)var69 + 15.99F) / 256.0F;
			GL11.glPushMatrix();
			GL11.glTranslatef((float)var2, (float)var4, (float)var6);
			float var74 = var1.width * 1.4F;
			GL11.glScalef(var74, var74, var74);
			this.loadTexture("/terrain.png");
			Tessellator var75 = Tessellator.instance;
			float var76 = 1.0F;
			float var77 = 0.0F;
			float var78 = var1.height / var1.width;
			GL11.glRotatef(-this.renderManager.playerViewY, 0.0F, 1.0F, 0.0F);
			GL11.glTranslatef(0.0F, 0.0F, 0.4F + (float)((int)var78) * 0.02F);
			GL11.glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
			var75.startDrawingQuads();

			while(var78 > 0.0F) {
				var75.addVertexWithUV((double)(var76 - 0.5F), (double)(0.0F - var77), 0.0D, (double)var70, (double)var73);
				var75.addVertexWithUV(-0.5D, (double)(0.0F - var77), 0.0D, (double)var11, (double)var73);
				var75.addVertexWithUV(-0.5D, (double)(1.4F - var77), 0.0D, (double)var11, (double)var71);
				var75.addVertexWithUV((double)(var76 - 0.5F), (double)(1.4F - var77), 0.0D, (double)var70, (double)var71);
				--var78;
				--var77;
				var76 *= 0.9F;
				GL11.glTranslatef(0.0F, 0.0F, -0.04F);
			}

			var75.draw();
			GL11.glPopMatrix();
			GL11.glEnable(GL11.GL_LIGHTING);
		}

	}
}
