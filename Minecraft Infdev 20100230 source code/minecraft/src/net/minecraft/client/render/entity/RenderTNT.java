package net.minecraft.client.render.entity;

import net.minecraft.client.render.RenderBlocks;
import net.minecraft.game.entity.Entity;
import net.minecraft.game.entity.misc.EntityTNT;
import net.minecraft.game.world.block.Block;
import org.lwjgl.opengl.GL11;

public final class RenderTNT extends Render {
	private RenderBlocks blockRenderer = new RenderBlocks();

	public RenderTNT() {
		this.shadowSize = 0.5F;
	}

	public final void doRender(Entity var1, double var2, double var4, double var6, float var8, float var9) {
		EntityTNT var10001 = (EntityTNT)var1;
		double var12 = var2;
		EntityTNT var18 = var10001;
		GL11.glPushMatrix();
		GL11.glTranslatef((float)var12, (float)var4, (float)var6);
		float var19;
		if((float)var18.fuse - var9 + 1.0F < 10.0F) {
			var19 = 1.0F - ((float)var18.fuse - var9 + 1.0F) / 10.0F;
			if(var19 < 0.0F) {
				var19 = 0.0F;
			}

			if(var19 > 1.0F) {
				var19 = 1.0F;
			}

			var19 *= var19;
			var19 *= var19;
			var19 = 1.0F + var19 * 0.3F;
			GL11.glScalef(var19, var19, var19);
		}

		var19 = (1.0F - ((float)var18.fuse - var9 + 1.0F) / 100.0F) * 0.8F;
		this.loadTexture("/terrain.png");
		this.blockRenderer.renderBlockOnInventory(Block.tnt);
		if(var18.fuse / 5 % 2 == 0) {
			GL11.glDisable(GL11.GL_TEXTURE_2D);
			GL11.glDisable(GL11.GL_LIGHTING);
			GL11.glEnable(GL11.GL_BLEND);
			GL11.glBlendFunc(GL11.GL_SRC_ALPHA, GL11.GL_DST_ALPHA);
			GL11.glColor4f(1.0F, 1.0F, 1.0F, var19);
			this.blockRenderer.renderBlockOnInventory(Block.tnt);
			GL11.glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
			GL11.glDisable(GL11.GL_BLEND);
			GL11.glEnable(GL11.GL_LIGHTING);
			GL11.glEnable(GL11.GL_TEXTURE_2D);
		}

		GL11.glPopMatrix();
	}
}
