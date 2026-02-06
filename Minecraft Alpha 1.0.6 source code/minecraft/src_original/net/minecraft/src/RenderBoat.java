package net.minecraft.src;

import org.lwjgl.opengl.GL11;

public class RenderBoat extends Render {
	protected ModelBase modelBoat;

	public RenderBoat() {
		this.shadowSize = 0.5F;
		this.modelBoat = new ModelBoat();
	}

	public void renderBoat(EntityBoat var1, double var2, double var4, double var6, float var8, float var9) {
		GL11.glPushMatrix();
		double var10 = (double)0.3F;
		GL11.glTranslatef((float)var2, (float)var4, (float)var6);
		GL11.glRotatef(180.0F - var8, 0.0F, 1.0F, 0.0F);
		float var12 = (float)var1.timeSinceHit - var9;
		float var13 = (float)var1.damageTaken - var9;
		if(var13 < 0.0F) {
			var13 = 0.0F;
		}

		if(var12 > 0.0F) {
			GL11.glRotatef(MathHelper.sin(var12) * var12 * var13 / 10.0F * (float)var1.forwardDirection, 1.0F, 0.0F, 0.0F);
		}

		this.loadTexture("/terrain.png");
		float var14 = 12.0F / 16.0F;
		GL11.glScalef(var14, var14, var14);
		GL11.glScalef(1.0F / var14, 1.0F / var14, 1.0F / var14);
		this.loadTexture("/item/boat.png");
		GL11.glScalef(-1.0F, -1.0F, 1.0F);
		GL11.glTranslatef(-0.25F, 0.0F, 0.0F);
		this.modelBoat.render(0.0F, 0.0F, -0.1F, 0.0F, 0.0F, 1.0F / 16.0F);
		GL11.glPopMatrix();
	}

	public void doRender(Entity var1, double var2, double var4, double var6, float var8, float var9) {
		this.renderBoat((EntityBoat)var1, var2, var4, var6, var8, var9);
	}
}
