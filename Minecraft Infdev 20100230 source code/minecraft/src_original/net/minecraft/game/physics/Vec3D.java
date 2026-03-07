package net.minecraft.game.physics;

import util.MathHelper;

public final class Vec3D {
	public double xCoord;
	public double yCoord;
	public double zCoord;

	public Vec3D(double var1, double var3, double var5) {
		this.xCoord = var1;
		this.yCoord = var3;
		this.zCoord = var5;
	}

	public final Vec3D subtract(Vec3D var1) {
		return new Vec3D(this.xCoord - var1.xCoord, this.yCoord - var1.yCoord, this.zCoord - var1.zCoord);
	}

	public final Vec3D normalize() {
		double var1 = (double)MathHelper.sqrt_double(this.xCoord * this.xCoord + this.yCoord * this.yCoord + this.zCoord * this.zCoord);
		return new Vec3D(this.xCoord / var1, this.yCoord / var1, this.zCoord / var1);
	}

	public final Vec3D addVector(double var1, double var3, double var5) {
		return new Vec3D(this.xCoord + var1, this.yCoord + var3, this.zCoord + var5);
	}

	public final double distance(Vec3D var1) {
		double var2 = var1.xCoord - this.xCoord;
		double var4 = var1.yCoord - this.yCoord;
		double var6 = var1.zCoord - this.zCoord;
		return (double)MathHelper.sqrt_double(var2 * var2 + var4 * var4 + var6 * var6);
	}

	public final double squareDistanceTo(Vec3D var1) {
		double var2 = var1.xCoord - this.xCoord;
		double var4 = var1.yCoord - this.yCoord;
		double var6 = var1.zCoord - this.zCoord;
		return var2 * var2 + var4 * var4 + var6 * var6;
	}

	public final Vec3D getIntermediateWithXValue(Vec3D var1, double var2) {
		double var4 = var1.xCoord - this.xCoord;
		double var6 = var1.yCoord - this.yCoord;
		double var8 = var1.zCoord - this.zCoord;
		if(var4 * var4 < (double)1.0E-7F) {
			return null;
		} else {
			double var10 = (var2 - this.xCoord) / var4;
			return var10 >= 0.0D && var10 <= 1.0D ? new Vec3D(this.xCoord + var4 * var10, this.yCoord + var6 * var10, this.zCoord + var8 * var10) : null;
		}
	}

	public final Vec3D getIntermediateWithYValue(Vec3D var1, double var2) {
		double var4 = var1.xCoord - this.xCoord;
		double var6 = var1.yCoord - this.yCoord;
		double var8 = var1.zCoord - this.zCoord;
		if(var6 * var6 < (double)1.0E-7F) {
			return null;
		} else {
			double var10 = (var2 - this.yCoord) / var6;
			return var10 >= 0.0D && var10 <= 1.0D ? new Vec3D(this.xCoord + var4 * var10, this.yCoord + var6 * var10, this.zCoord + var8 * var10) : null;
		}
	}

	public final Vec3D getIntermediateWithZValue(Vec3D var1, double var2) {
		double var4 = var1.xCoord - this.xCoord;
		double var6 = var1.yCoord - this.yCoord;
		double var8 = var1.zCoord - this.zCoord;
		if(var8 * var8 < (double)1.0E-7F) {
			return null;
		} else {
			double var10 = (var2 - this.zCoord) / var8;
			return var10 >= 0.0D && var10 <= 1.0D ? new Vec3D(this.xCoord + var4 * var10, this.yCoord + var6 * var10, this.zCoord + var8 * var10) : null;
		}
	}

	public final String toString() {
		return "(" + this.xCoord + ", " + this.yCoord + ", " + this.zCoord + ")";
	}
}
