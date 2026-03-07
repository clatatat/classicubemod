package net.minecraft.client.render.entity;

import net.minecraft.client.render.Tessellator;
import net.minecraft.game.entity.Entity;
import net.minecraft.game.physics.AxisAlignedBB;
import org.lwjgl.opengl.GL11;

public final class RenderEntity extends Render {
	public final void doRender(Entity var1, double var2, double var4, double var6, float var8, float var9) {
		GL11.glPushMatrix();
		GL11.glTranslatef((float)(var2 - var1.lastTickPosX), (float)(var4 - var1.lastTickPosY), (float)(var6 - var1.lastTickPosZ));
		AxisAlignedBB var10 = var1.boundingBox;
		GL11.glDisable(GL11.GL_TEXTURE_2D);
		Tessellator var11 = Tessellator.instance;
		GL11.glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
		var11.startDrawingQuads();
		Tessellator.setNormal(0.0F, 0.0F, -1.0F);
		var11.addVertex(var10.minX, var10.maxY, var10.minZ);
		var11.addVertex(var10.maxX, var10.maxY, var10.minZ);
		var11.addVertex(var10.maxX, var10.minY, var10.minZ);
		var11.addVertex(var10.minX, var10.minY, var10.minZ);
		Tessellator.setNormal(0.0F, 0.0F, 1.0F);
		var11.addVertex(var10.minX, var10.minY, var10.maxZ);
		var11.addVertex(var10.maxX, var10.minY, var10.maxZ);
		var11.addVertex(var10.maxX, var10.maxY, var10.maxZ);
		var11.addVertex(var10.minX, var10.maxY, var10.maxZ);
		Tessellator.setNormal(0.0F, -1.0F, 0.0F);
		var11.addVertex(var10.minX, var10.minY, var10.minZ);
		var11.addVertex(var10.maxX, var10.minY, var10.minZ);
		var11.addVertex(var10.maxX, var10.minY, var10.maxZ);
		var11.addVertex(var10.minX, var10.minY, var10.maxZ);
		Tessellator.setNormal(0.0F, 1.0F, 0.0F);
		var11.addVertex(var10.minX, var10.maxY, var10.maxZ);
		var11.addVertex(var10.maxX, var10.maxY, var10.maxZ);
		var11.addVertex(var10.maxX, var10.maxY, var10.minZ);
		var11.addVertex(var10.minX, var10.maxY, var10.minZ);
		Tessellator.setNormal(-1.0F, 0.0F, 0.0F);
		var11.addVertex(var10.minX, var10.minY, var10.maxZ);
		var11.addVertex(var10.minX, var10.maxY, var10.maxZ);
		var11.addVertex(var10.minX, var10.maxY, var10.minZ);
		var11.addVertex(var10.minX, var10.minY, var10.minZ);
		Tessellator.setNormal(1.0F, 0.0F, 0.0F);
		var11.addVertex(var10.maxX, var10.minY, var10.minZ);
		var11.addVertex(var10.maxX, var10.maxY, var10.minZ);
		var11.addVertex(var10.maxX, var10.maxY, var10.maxZ);
		var11.addVertex(var10.maxX, var10.minY, var10.maxZ);
		var11.draw();
		GL11.glEnable(GL11.GL_TEXTURE_2D);
		GL11.glPopMatrix();
	}
}
