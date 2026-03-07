package net.minecraft.game.physics;

public final class AxisAlignedBB {
	public double minX;
	public double minY;
	public double minZ;
	public double maxX;
	public double maxY;
	public double maxZ;

	public AxisAlignedBB(double var1, double var3, double var5, double var7, double var9, double var11) {
		this.minX = var1;
		this.minY = var3;
		this.minZ = var5;
		this.maxX = var7;
		this.maxY = var9;
		this.maxZ = var11;
	}

	public final AxisAlignedBB addCoord(double var1, double var3, double var5) {
		double var7 = this.minX;
		double var9 = this.minY;
		double var11 = this.minZ;
		double var13 = this.maxX;
		double var15 = this.maxY;
		double var17 = this.maxZ;
		if(var1 < 0.0D) {
			var7 += var1;
		}

		if(var1 > 0.0D) {
			var13 += var1;
		}

		if(var3 < 0.0D) {
			var9 += var3;
		}

		if(var3 > 0.0D) {
			var15 += var3;
		}

		if(var5 < 0.0D) {
			var11 += var5;
		}

		if(var5 > 0.0D) {
			var17 += var5;
		}

		return new AxisAlignedBB(var7, var9, var11, var13, var15, var17);
	}

	public final AxisAlignedBB expand(double var1, double var3, double var5) {
		if(this.minY > this.maxY) {
			throw new IllegalArgumentException("NOOOOOO!");
		} else {
			double var7 = this.minX - var1;
			double var9 = this.minY - var3;
			double var11 = this.minZ - var5;
			double var13 = this.maxX + var1;
			double var15 = this.maxY + var3;
			double var17 = this.maxZ + var5;
			return new AxisAlignedBB(var7, var9, var11, var13, var15, var17);
		}
	}

	public final AxisAlignedBB offsetCopy(double var1, double var3, double var5) {
		return new AxisAlignedBB(this.minX + var1, this.minY + var3, this.minZ + var5, this.maxX + var1, this.maxY + var3, this.maxZ + var5);
	}

	public final double calculateXOffset(AxisAlignedBB var1, double var2) {
		if(var1.maxY > this.minY && var1.minY < this.maxY) {
			if(var1.maxZ > this.minZ && var1.minZ < this.maxZ) {
				double var4;
				if(var2 > 0.0D && var1.maxX <= this.minX) {
					var4 = this.minX - var1.maxX;
					if(var4 < var2) {
						var2 = var4;
					}
				}

				if(var2 < 0.0D && var1.minX >= this.maxX) {
					var4 = this.maxX - var1.minX;
					if(var4 > var2) {
						var2 = var4;
					}
				}

				return var2;
			} else {
				return var2;
			}
		} else {
			return var2;
		}
	}

	public final double calculateYOffset(AxisAlignedBB var1, double var2) {
		if(var1.maxX > this.minX && var1.minX < this.maxX) {
			if(var1.maxZ > this.minZ && var1.minZ < this.maxZ) {
				double var4;
				if(var2 > 0.0D && var1.maxY <= this.minY) {
					var4 = this.minY - var1.maxY;
					if(var4 < var2) {
						var2 = var4;
					}
				}

				if(var2 < 0.0D && var1.minY >= this.maxY) {
					var4 = this.maxY - var1.minY;
					if(var4 > var2) {
						var2 = var4;
					}
				}

				return var2;
			} else {
				return var2;
			}
		} else {
			return var2;
		}
	}

	public final double calculateZOffset(AxisAlignedBB var1, double var2) {
		if(var1.maxX > this.minX && var1.minX < this.maxX) {
			if(var1.maxY > this.minY && var1.minY < this.maxY) {
				double var4;
				if(var2 > 0.0D && var1.maxZ <= this.minZ) {
					var4 = this.minZ - var1.maxZ;
					if(var4 < var2) {
						var2 = var4;
					}
				}

				if(var2 < 0.0D && var1.minZ >= this.maxZ) {
					var4 = this.maxZ - var1.minZ;
					if(var4 > var2) {
						var2 = var4;
					}
				}

				return var2;
			} else {
				return var2;
			}
		} else {
			return var2;
		}
	}

	public final boolean intersectsWith(AxisAlignedBB var1) {
		return var1.maxX > this.minX && var1.minX < this.maxX ? (var1.maxY > this.minY && var1.minY < this.maxY ? var1.maxZ > this.minZ && var1.minZ < this.maxZ : false) : false;
	}

	public final void offset(double var1, double var3, double var5) {
		this.minX += var1;
		this.minY += var3;
		this.minZ += var5;
		this.maxX += var1;
		this.maxY += var3;
		this.maxZ += var5;
	}

	public final AxisAlignedBB copy() {
		return new AxisAlignedBB(this.minX, this.minY, this.minZ, this.maxX, this.maxY, this.maxZ);
	}

	public final MovingObjectPosition calculateIntercept(Vec3D var1, Vec3D var2) {
		Vec3D var3 = var1.getIntermediateWithXValue(var2, this.minX);
		Vec3D var4 = var1.getIntermediateWithXValue(var2, this.maxX);
		Vec3D var5 = var1.getIntermediateWithYValue(var2, this.minY);
		Vec3D var6 = var1.getIntermediateWithYValue(var2, this.maxY);
		Vec3D var7 = var1.getIntermediateWithZValue(var2, this.minZ);
		var2 = var1.getIntermediateWithZValue(var2, this.maxZ);
		if(!this.isVecInYZ(var3)) {
			var3 = null;
		}

		if(!this.isVecInYZ(var4)) {
			var4 = null;
		}

		if(!this.isVecInXZ(var5)) {
			var5 = null;
		}

		if(!this.isVecInXZ(var6)) {
			var6 = null;
		}

		if(!this.isVecInXY(var7)) {
			var7 = null;
		}

		if(!this.isVecInXY(var2)) {
			var2 = null;
		}

		Vec3D var8 = null;
		if(var3 != null) {
			var8 = var3;
		}

		if(var4 != null && (var8 == null || var1.squareDistanceTo(var4) < var1.squareDistanceTo(var8))) {
			var8 = var4;
		}

		if(var5 != null && (var8 == null || var1.squareDistanceTo(var5) < var1.squareDistanceTo(var8))) {
			var8 = var5;
		}

		if(var6 != null && (var8 == null || var1.squareDistanceTo(var6) < var1.squareDistanceTo(var8))) {
			var8 = var6;
		}

		if(var7 != null && (var8 == null || var1.squareDistanceTo(var7) < var1.squareDistanceTo(var8))) {
			var8 = var7;
		}

		if(var2 != null && (var8 == null || var1.squareDistanceTo(var2) < var1.squareDistanceTo(var8))) {
			var8 = var2;
		}

		if(var8 == null) {
			return null;
		} else {
			byte var9 = -1;
			if(var8 == var3) {
				var9 = 4;
			}

			if(var8 == var4) {
				var9 = 5;
			}

			if(var8 == var5) {
				var9 = 0;
			}

			if(var8 == var6) {
				var9 = 1;
			}

			if(var8 == var7) {
				var9 = 2;
			}

			if(var8 == var2) {
				var9 = 3;
			}

			return new MovingObjectPosition(0, 0, 0, var9, var8);
		}
	}

	private boolean isVecInYZ(Vec3D var1) {
		return var1 == null ? false : var1.yCoord >= this.minY && var1.yCoord <= this.maxY && var1.zCoord >= this.minZ && var1.zCoord <= this.maxZ;
	}

	private boolean isVecInXZ(Vec3D var1) {
		return var1 == null ? false : var1.xCoord >= this.minX && var1.xCoord <= this.maxX && var1.zCoord >= this.minZ && var1.zCoord <= this.maxZ;
	}

	private boolean isVecInXY(Vec3D var1) {
		return var1 == null ? false : var1.xCoord >= this.minX && var1.xCoord <= this.maxX && var1.yCoord >= this.minY && var1.yCoord <= this.maxY;
	}
}
