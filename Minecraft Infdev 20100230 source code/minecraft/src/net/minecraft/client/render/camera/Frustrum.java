package net.minecraft.client.render.camera;

import net.minecraft.game.physics.AxisAlignedBB;

public class Frustrum {
	private ClippingHelper clippingHelper = ClippingHelperImplementation.init();
	private double xPosition;
	private double yPosition;
	private double zPosition;

	public boolean isBoundingBoxInFrustrum(AxisAlignedBB var1) {
		return this.isBoxInFrustum(var1.minX, var1.minY, var1.minZ, var1.maxX, var1.maxY, var1.maxZ);
	}

	public void setPosition(double var1, double var3, double var5) {
		this.xPosition = var1;
		this.yPosition = var3;
		this.zPosition = var5;
	}

	public boolean isBoxInFrustum(double var1, double var3, double var5, double var7, double var9, double var11) {
		double var10001 = var1 - this.xPosition;
		double var10002 = var3 - this.yPosition;
		double var10003 = var5 - this.zPosition;
		double var10004 = var7 - this.xPosition;
		double var10005 = var9 - this.yPosition;
		double var24 = var11 - this.zPosition;
		double var22 = var10005;
		double var20 = var10004;
		double var18 = var10003;
		double var16 = var10002;
		double var14 = var10001;
		ClippingHelper var26 = this.clippingHelper;

		for(int var2 = 0; var2 < 6; ++var2) {
			if((double)var26.frustrum[var2][0] * var14 + (double)var26.frustrum[var2][1] * var16 + (double)var26.frustrum[var2][2] * var18 + (double)var26.frustrum[var2][3] <= 0.0D && (double)var26.frustrum[var2][0] * var20 + (double)var26.frustrum[var2][1] * var16 + (double)var26.frustrum[var2][2] * var18 + (double)var26.frustrum[var2][3] <= 0.0D && (double)var26.frustrum[var2][0] * var14 + (double)var26.frustrum[var2][1] * var22 + (double)var26.frustrum[var2][2] * var18 + (double)var26.frustrum[var2][3] <= 0.0D && (double)var26.frustrum[var2][0] * var20 + (double)var26.frustrum[var2][1] * var22 + (double)var26.frustrum[var2][2] * var18 + (double)var26.frustrum[var2][3] <= 0.0D && (double)var26.frustrum[var2][0] * var14 + (double)var26.frustrum[var2][1] * var16 + (double)var26.frustrum[var2][2] * var24 + (double)var26.frustrum[var2][3] <= 0.0D && (double)var26.frustrum[var2][0] * var20 + (double)var26.frustrum[var2][1] * var16 + (double)var26.frustrum[var2][2] * var24 + (double)var26.frustrum[var2][3] <= 0.0D && (double)var26.frustrum[var2][0] * var14 + (double)var26.frustrum[var2][1] * var22 + (double)var26.frustrum[var2][2] * var24 + (double)var26.frustrum[var2][3] <= 0.0D && (double)var26.frustrum[var2][0] * var20 + (double)var26.frustrum[var2][1] * var22 + (double)var26.frustrum[var2][2] * var24 + (double)var26.frustrum[var2][3] <= 0.0D) {
				return false;
			}
		}

		return true;
	}
}
