package net.minecraft.client.render.entity;

import net.minecraft.client.render.Tessellator;
import net.minecraft.game.entity.Entity;
import net.minecraft.game.entity.projectile.EntityArrow;
import org.lwjgl.opengl.GL11;
import util.MathHelper;

public final class RenderArrow extends Render {
	public final void doRender(Entity var1, double var2, double var4, double var6, float var8, float var9) {
		EntityArrow var19 = (EntityArrow)var1;
		this.loadTexture("/item/arrows.png");
		GL11.glPushMatrix();
		GL11.glTranslatef((float)var2, (float)var4, (float)var6);
		GL11.glRotatef(var19.prevRotationYaw + (var19.rotationYaw - var19.prevRotationYaw) * var9 - 90.0F, 0.0F, 1.0F, 0.0F);
		GL11.glRotatef(var19.prevRotationPitch + (var19.rotationPitch - var19.prevRotationPitch) * var9, 0.0F, 0.0F, 1.0F);
		Tessellator var18 = Tessellator.instance;
		GL11.glEnable(GL11.GL_NORMALIZE);
		float var20 = (float)var19.arrowShake - var9;
		if(var20 > 0.0F) {
			var20 = -MathHelper.sin(var20 * 3.0F) * var20;
			GL11.glRotatef(var20, 0.0F, 0.0F, 1.0F);
		}

		GL11.glRotatef(45.0F, 1.0F, 0.0F, 0.0F);
		GL11.glScalef(0.05625F, 0.05625F, 0.05625F);
		GL11.glTranslatef(-4.0F, 0.0F, 0.0F);
		GL11.glNormal3f(0.05625F, 0.0F, 0.0F);
		var18.startDrawingQuads();
		var18.addVertexWithUV(-7.0D, -2.0D, -2.0D, 0.0D, 0.15625D);
		var18.addVertexWithUV(-7.0D, -2.0D, 2.0D, 0.15625D, 0.15625D);
		var18.addVertexWithUV(-7.0D, 2.0D, 2.0D, 0.15625D, 0.3125D);
		var18.addVertexWithUV(-7.0D, 2.0D, -2.0D, 0.0D, 0.3125D);
		var18.draw();
		GL11.glNormal3f(-0.05625F, 0.0F, 0.0F);
		var18.startDrawingQuads();
		var18.addVertexWithUV(-7.0D, 2.0D, -2.0D, 0.0D, 0.15625D);
		var18.addVertexWithUV(-7.0D, 2.0D, 2.0D, 0.15625D, 0.15625D);
		var18.addVertexWithUV(-7.0D, -2.0D, 2.0D, 0.15625D, 0.3125D);
		var18.addVertexWithUV(-7.0D, -2.0D, -2.0D, 0.0D, 0.3125D);
		var18.draw();

		for(int var21 = 0; var21 < 4; ++var21) {
			GL11.glRotatef(90.0F, 1.0F, 0.0F, 0.0F);
			GL11.glNormal3f(0.0F, 0.0F, 0.05625F);
			var18.startDrawingQuads();
			var18.addVertexWithUV(-8.0D, -2.0D, 0.0D, 0.0D, 0.0D);
			var18.addVertexWithUV(8.0D, -2.0D, 0.0D, 0.5D, 0.0D);
			var18.addVertexWithUV(8.0D, 2.0D, 0.0D, 0.5D, 0.15625D);
			var18.addVertexWithUV(-8.0D, 2.0D, 0.0D, 0.0D, 0.15625D);
			var18.draw();
		}

		GL11.glDisable(GL11.GL_NORMALIZE);
		GL11.glPopMatrix();
	}
}
