package net.minecraft.client.render.entity;

import java.util.Random;
import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.render.RenderBlocks;
import net.minecraft.client.render.RenderEngine;
import net.minecraft.client.render.Tessellator;
import net.minecraft.game.entity.Entity;
import net.minecraft.game.entity.misc.EntityItem;
import net.minecraft.game.item.ItemStack;
import net.minecraft.game.world.block.Block;
import org.lwjgl.opengl.GL11;
import util.MathHelper;

public final class RenderItem extends Render {
	private RenderBlocks renderBlocks = new RenderBlocks();
	private Random random = new Random();

	public RenderItem() {
		this.shadowSize = 0.15F;
		this.shadowOpaque = 12.0F / 16.0F;
	}

	public final void renderItemIntoGUI(RenderEngine var1, ItemStack var2, int var3, int var4) {
		if(var2 != null) {
			int var9;
			if(var2.itemID < 256 && Block.blocksList[var2.itemID].getRenderType() == 0) {
				var9 = var2.itemID;
				RenderEngine.bindTexture(var1.getTexture("/terrain.png"));
				Block var8 = Block.blocksList[var9];
				GL11.glPushMatrix();
				GL11.glTranslatef((float)(var3 - 2), (float)(var4 + 3), 0.0F);
				GL11.glScalef(10.0F, 10.0F, 10.0F);
				GL11.glTranslatef(1.0F, 0.5F, 8.0F);
				GL11.glRotatef(210.0F, 1.0F, 0.0F, 0.0F);
				GL11.glRotatef(45.0F, 0.0F, 1.0F, 0.0F);
				GL11.glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
				this.renderBlocks.renderBlockOnInventory(var8);
				GL11.glPopMatrix();
			} else {
				if(var2.getItem().getIconFromDamage() >= 0) {
					GL11.glDisable(GL11.GL_LIGHTING);
					if(var2.itemID < 256) {
						RenderEngine.bindTexture(var1.getTexture("/terrain.png"));
					} else {
						RenderEngine.bindTexture(var1.getTexture("/gui/items.png"));
					}

					int var10000 = var3;
					int var10001 = var4;
					int var10002 = var2.getItem().getIconFromDamage() % 16 << 4;
					int var10003 = var2.getItem().getIconFromDamage() / 16 << 4;
					boolean var6 = true;
					var6 = true;
					var4 = var10003;
					var3 = var10002;
					var9 = var10001;
					int var7 = var10000;
					Tessellator var5 = Tessellator.instance;
					var5.startDrawingQuads();
					var5.addVertexWithUV((double)var7, (double)(var9 + 16), 0.0D, (double)((float)var3 * 0.00390625F), (double)((float)(var4 + 16) * 0.00390625F));
					var5.addVertexWithUV((double)(var7 + 16), (double)(var9 + 16), 0.0D, (double)((float)(var3 + 16) * 0.00390625F), (double)((float)(var4 + 16) * 0.00390625F));
					var5.addVertexWithUV((double)(var7 + 16), (double)var9, 0.0D, (double)((float)(var3 + 16) * 0.00390625F), (double)((float)var4 * 0.00390625F));
					var5.addVertexWithUV((double)var7, (double)var9, 0.0D, (double)((float)var3 * 0.00390625F), (double)((float)var4 * 0.00390625F));
					var5.draw();
					GL11.glEnable(GL11.GL_LIGHTING);
				}

			}
		}
	}

	public final void renderItemOverlayIntoGUI(FontRenderer var1, ItemStack var2, int var3, int var4) {
		if(var2 != null) {
			if(var2.stackSize > 1) {
				String var5 = "" + var2.stackSize;
				GL11.glDisable(GL11.GL_LIGHTING);
				GL11.glDisable(GL11.GL_DEPTH_TEST);
				var1.drawStringWithShadow(var5, var3 + 19 - 2 - var1.getStringWidth(var5), var4 + 6 + 3, 16777215);
				GL11.glEnable(GL11.GL_LIGHTING);
				GL11.glEnable(GL11.GL_DEPTH_TEST);
			}

			if(var2.itemDamage > 0) {
				int var9 = 13 - var2.itemDamage * 13 / var2.getMaxDamage();
				int var7 = 255 - var2.itemDamage * 255 / var2.getMaxDamage();
				GL11.glDisable(GL11.GL_LIGHTING);
				GL11.glDisable(GL11.GL_DEPTH_TEST);
				GL11.glDisable(GL11.GL_TEXTURE_2D);
				Tessellator var8 = Tessellator.instance;
				int var6 = 255 - var7 << 16 | var7 << 8;
				var7 = (255 - var7) / 4 << 16 | 16128;
				renderQuad(var8, var3 + 2, var4 + 13, 13, 2, 0);
				renderQuad(var8, var3 + 2, var4 + 13, 12, 1, var7);
				renderQuad(var8, var3 + 2, var4 + 13, var9, 1, var6);
				GL11.glEnable(GL11.GL_TEXTURE_2D);
				GL11.glEnable(GL11.GL_LIGHTING);
				GL11.glEnable(GL11.GL_DEPTH_TEST);
				GL11.glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
			}

		}
	}

	private static void renderQuad(Tessellator var0, int var1, int var2, int var3, int var4, int var5) {
		var0.startDrawingQuads();
		var0.setColorOpaque_I(var5);
		var0.addVertex((double)var1, (double)var2, 0.0D);
		var0.addVertex((double)var1, (double)(var2 + var4), 0.0D);
		var0.addVertex((double)(var1 + var3), (double)(var2 + var4), 0.0D);
		var0.addVertex((double)(var1 + var3), (double)var2, 0.0D);
		var0.draw();
	}

	public final void doRender(Entity var1, double var2, double var4, double var6, float var8, float var9) {
		EntityItem var19 = (EntityItem)var1;
		RenderItem var18 = this;
		this.random.setSeed(187L);
		ItemStack var24 = var19.item;
		GL11.glPushMatrix();
		float var5 = MathHelper.sin(((float)var19.age + var9) / 10.0F + var19.hoverStart) * 0.1F + 0.1F;
		float var3 = (((float)var19.age + var9) / 20.0F + var19.hoverStart) * (180.0F / (float)Math.PI);
		byte var26 = 1;
		if(var19.item.stackSize > 1) {
			var26 = 2;
		}

		if(var19.item.stackSize > 5) {
			var26 = 3;
		}

		if(var19.item.stackSize > 20) {
			var26 = 4;
		}

		GL11.glTranslatef((float)var2, (float)var4 + var5, (float)var6);
		GL11.glEnable(GL11.GL_NORMALIZE);
		float var7;
		float var21;
		if(var24.itemID < 256 && Block.blocksList[var24.itemID].getRenderType() == 0) {
			GL11.glRotatef(var3, 0.0F, 1.0F, 0.0F);
			this.loadTexture("/terrain.png");
			var21 = 0.25F;
			if(!Block.blocksList[var24.itemID].renderAsNormalBlock() && var24.itemID != Block.stairSingle.blockID) {
				var21 = 0.5F;
			}

			GL11.glScalef(var21, var21, var21);

			for(int var23 = 0; var23 < var26; ++var23) {
				GL11.glPushMatrix();
				if(var23 > 0) {
					var5 = (var18.random.nextFloat() * 2.0F - 1.0F) * 0.2F / var21;
					var7 = (var18.random.nextFloat() * 2.0F - 1.0F) * 0.2F / var21;
					var8 = (var18.random.nextFloat() * 2.0F - 1.0F) * 0.2F / var21;
					GL11.glTranslatef(var5, var7, var8);
				}

				var18.renderBlocks.renderBlockOnInventory(Block.blocksList[var24.itemID]);
				GL11.glPopMatrix();
			}
		} else {
			GL11.glScalef(0.5F, 0.5F, 0.5F);
			int var20 = var24.getItem().getIconFromDamage();
			if(var24.itemID < 256) {
				this.loadTexture("/terrain.png");
			} else {
				this.loadTexture("/gui/items.png");
			}

			Tessellator var22 = Tessellator.instance;
			var5 = (float)(var20 % 16 << 4) / 256.0F;
			var7 = (float)((var20 % 16 << 4) + 16) / 256.0F;
			var8 = (float)(var20 / 16 << 4) / 256.0F;
			var21 = (float)((var20 / 16 << 4) + 16) / 256.0F;

			for(int var25 = 0; var25 < var26; ++var25) {
				GL11.glPushMatrix();
				if(var25 > 0) {
					var9 = (var18.random.nextFloat() * 2.0F - 1.0F) * 0.3F;
					float var10 = (var18.random.nextFloat() * 2.0F - 1.0F) * 0.3F;
					float var11 = (var18.random.nextFloat() * 2.0F - 1.0F) * 0.3F;
					GL11.glTranslatef(var9, var10, var11);
				}

				GL11.glRotatef(180.0F - var18.renderManager.playerViewY, 0.0F, 1.0F, 0.0F);
				var22.startDrawingQuads();
				Tessellator.setNormal(0.0F, 1.0F, 0.0F);
				var22.addVertexWithUV(-0.5D, -0.25D, 0.0D, (double)var5, (double)var21);
				var22.addVertexWithUV(0.5D, -0.25D, 0.0D, (double)var7, (double)var21);
				var22.addVertexWithUV(0.5D, 0.75D, 0.0D, (double)var7, (double)var8);
				var22.addVertexWithUV(-0.5D, 0.75D, 0.0D, (double)var5, (double)var8);
				var22.draw();
				GL11.glPopMatrix();
			}
		}

		GL11.glDisable(GL11.GL_NORMALIZE);
		GL11.glPopMatrix();
	}
}
