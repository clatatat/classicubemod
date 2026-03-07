package net.minecraft.game.world.path;

public final class Path {
	private PathPoint[] pathpoints = new PathPoint[1024];
	private int count = 0;

	public final PathPoint addPoint(PathPoint var1) {
		if(var1.index >= 0) {
			throw new IllegalStateException("OW KNOWS!");
		} else {
			if(this.count == this.pathpoints.length) {
				PathPoint[] var2 = new PathPoint[this.count << 1];
				System.arraycopy(this.pathpoints, 0, var2, 0, this.count);
				this.pathpoints = var2;
			}

			this.pathpoints[this.count] = var1;
			var1.index = this.count;
			this.sortBack(this.count++);
			return var1;
		}
	}

	public final void clearPath() {
		this.count = 0;
	}

	public final PathPoint dequeue() {
		PathPoint var1 = this.pathpoints[0];
		this.pathpoints[0] = this.pathpoints[--this.count];
		this.pathpoints[this.count] = null;
		if(this.count > 0) {
			this.sortForward(0);
		}

		var1.index = -1;
		return var1;
	}

	public final void changeDistance(PathPoint var1, float var2) {
		float var3 = var1.distanceToTarget;
		var1.distanceToTarget = var2;
		if(var2 < var3) {
			this.sortBack(var1.index);
		} else {
			this.sortForward(var1.index);
		}
	}

	private void sortBack(int var1) {
		PathPoint var2 = this.pathpoints[var1];

		int var4;
		for(float var3 = var2.distanceToTarget; var1 > 0; var1 = var4) {
			var4 = var1 - 1 >> 1;
			PathPoint var5 = this.pathpoints[var4];
			if(var3 >= var5.distanceToTarget) {
				break;
			}

			this.pathpoints[var1] = var5;
			var5.index = var1;
		}

		this.pathpoints[var1] = var2;
		var2.index = var1;
	}

	private void sortForward(int var1) {
		PathPoint var2 = this.pathpoints[var1];
		float var3 = var2.distanceToTarget;

		while(true) {
			int var4 = 1 + (var1 << 1);
			int var5 = var4 + 1;
			if(var4 >= this.count) {
				break;
			}

			PathPoint var6 = this.pathpoints[var4];
			float var7 = var6.distanceToTarget;
			PathPoint var8;
			float var9;
			if(var5 >= this.count) {
				var8 = null;
				var9 = Float.POSITIVE_INFINITY;
			} else {
				var8 = this.pathpoints[var5];
				var9 = var8.distanceToTarget;
			}

			if(var7 < var9) {
				if(var7 >= var3) {
					break;
				}

				this.pathpoints[var1] = var6;
				var6.index = var1;
				var1 = var4;
			} else {
				if(var9 >= var3) {
					break;
				}

				this.pathpoints[var1] = var8;
				var8.index = var1;
				var1 = var5;
			}
		}

		this.pathpoints[var1] = var2;
		var2.index = var1;
	}

	public final boolean isPathEmpty() {
		return this.count == 0;
	}
}
