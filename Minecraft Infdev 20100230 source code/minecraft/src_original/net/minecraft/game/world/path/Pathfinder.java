package net.minecraft.game.world.path;

import java.util.HashMap;
import java.util.Map;
import net.minecraft.game.entity.Entity;
import net.minecraft.game.world.World;
import net.minecraft.game.world.material.Material;

public final class Pathfinder {
	private World worldMap;
	private Path path = new Path();
	private Map pointMap = new HashMap();
	private PathPoint[] pathOptions = new PathPoint[32];

	public Pathfinder(World var1) {
		this.worldMap = var1;
	}

	public final PathEntity createEntityPathTo(Entity var1, Entity var2, float var3) {
		return this.addToPath(var1, var2.posX, var2.boundingBox.minY, var2.posZ, 16.0F);
	}

	public final PathEntity createEntityPathToXYZ(Entity var1, int var2, int var3, int var4, float var5) {
		return this.addToPath(var1, (double)((float)var2 + 0.5F), (double)((float)var3 + 0.5F), (double)((float)var4 + 0.5F), 16.0F);
	}

	private PathEntity addToPath(Entity var1, double var2, double var4, double var6, float var8) {
		this.path.clearPath();
		this.pointMap.clear();
		Object var9 = this.openPoint((int)var1.boundingBox.minX, (int)var1.boundingBox.minY, (int)var1.boundingBox.minZ);
		PathPoint var22 = this.openPoint((int)(var2 - (double)(var1.width / 2.0F)), (int)var4, (int)(var6 - (double)(var1.width / 2.0F)));
		PathPoint var3 = new PathPoint((int)(var1.width + 1.0F), (int)(var1.height + 1.0F), (int)(var1.width + 1.0F));
		float var25 = var8;
		PathPoint var5 = var3;
		PathPoint var24 = var22;
		Entity var23 = var1;
		Pathfinder var20 = this;
		((PathPoint)var9).totalPathDistance = 0.0F;
		((PathPoint)var9).distanceToNext = ((PathPoint)var9).distanceTo(var22);
		((PathPoint)var9).distanceToTarget = ((PathPoint)var9).distanceToNext;
		this.path.clearPath();
		this.path.addPoint((PathPoint)var9);
		Object var7 = var9;

		PathEntity var10000;
		while(true) {
			if(var20.path.isPathEmpty()) {
				var10000 = var7 == var9 ? null : createEntityPath((PathPoint)var7);
				break;
			}

			PathPoint var26 = var20.path.dequeue();
			if(var26.hash == var24.hash) {
				var10000 = createEntityPath(var24);
				break;
			}

			if(var26.distanceTo(var24) < ((PathPoint)var7).distanceTo(var24)) {
				var7 = var26;
			}

			var26.isFirst = true;
			int var15 = 0;
			byte var16 = 0;
			if(var20.getVerticalOffset(var26.xCoord, var26.yCoord + 1, var26.zCoord, var5) > 0) {
				var16 = 1;
			}

			PathPoint var17 = var20.getSafePoint(var23, var26.xCoord, var26.yCoord, var26.zCoord + 1, var5, var16);
			PathPoint var18 = var20.getSafePoint(var23, var26.xCoord - 1, var26.yCoord, var26.zCoord, var5, var16);
			PathPoint var19 = var20.getSafePoint(var23, var26.xCoord + 1, var26.yCoord, var26.zCoord, var5, var16);
			PathPoint var10 = var20.getSafePoint(var23, var26.xCoord, var26.yCoord, var26.zCoord - 1, var5, var16);
			if(var17 != null && !var17.isFirst && var17.distanceTo(var24) < var25) {
				++var15;
				var20.pathOptions[0] = var17;
			}

			if(var18 != null && !var18.isFirst && var18.distanceTo(var24) < var25) {
				var20.pathOptions[var15++] = var18;
			}

			if(var19 != null && !var19.isFirst && var19.distanceTo(var24) < var25) {
				var20.pathOptions[var15++] = var19;
			}

			if(var10 != null && !var10.isFirst && var10.distanceTo(var24) < var25) {
				var20.pathOptions[var15++] = var10;
			}

			for(int var27 = 0; var27 < var15; ++var27) {
				PathPoint var11 = var20.pathOptions[var27];
				float var12 = var26.totalPathDistance + var26.distanceTo(var11);
				if(!var11.isAssigned() || var12 < var11.totalPathDistance) {
					var11.previous = var26;
					var11.totalPathDistance = var12;
					var11.distanceToNext = var11.distanceTo(var24);
					if(var11.isAssigned()) {
						var20.path.changeDistance(var11, var11.totalPathDistance + var11.distanceToNext);
					} else {
						var11.distanceToTarget = var11.totalPathDistance + var11.distanceToNext;
						var20.path.addPoint(var11);
					}
				}
			}
		}

		PathEntity var21 = var10000;
		return var21;
	}

	private PathPoint getSafePoint(Entity var1, int var2, int var3, int var4, PathPoint var5, int var6) {
		PathPoint var8 = null;
		if(this.getVerticalOffset(var2, var3, var4, var5) > 0) {
			var8 = this.openPoint(var2, var3, var4);
		}

		if(var8 == null && this.getVerticalOffset(var2, var3 + var6, var4, var5) > 0) {
			var8 = this.openPoint(var2, var3 + var6, var4);
		}

		if(var8 != null) {
			var6 = 0;

			while(true) {
				if(var3 > 0) {
					int var7 = this.getVerticalOffset(var2, var3 - 1, var4, var5);
					if(var7 > 0) {
						if(var7 < 0) {
							return null;
						}

						++var6;
						if(var6 >= 4) {
							return null;
						}

						--var3;
						var8 = this.openPoint(var2, var3, var4);
						continue;
					}
				}

				Material var9 = this.worldMap.getBlockMaterial(var2, var3 - 1, var4);
				if(var9 == Material.water || var9 == Material.lava) {
					return null;
				}
				break;
			}
		}

		return var8;
	}

	private final PathPoint openPoint(int var1, int var2, int var3) {
		int var4 = var1 | var2 << 10 | var3 << 20;
		PathPoint var5 = (PathPoint)this.pointMap.get(Integer.valueOf(var4));
		if(var5 == null) {
			var5 = new PathPoint(var1, var2, var3);
			this.pointMap.put(Integer.valueOf(var4), var5);
		}

		return var5;
	}

	private int getVerticalOffset(int var1, int var2, int var3, PathPoint var4) {
		for(int var5 = var1; var5 < var1 + var4.xCoord; ++var5) {
			for(int var6 = var2; var6 < var2 + var4.yCoord; ++var6) {
				for(int var7 = var3; var7 < var3 + var4.zCoord; ++var7) {
					Material var8 = this.worldMap.getBlockMaterial(var1, var2, var3);
					if(var8.getIsSolid()) {
						return 0;
					}

					if(var8 == Material.water || var8 == Material.lava) {
						return -1;
					}
				}
			}
		}

		return 1;
	}

	private static PathEntity createEntityPath(PathPoint var0) {
		int var1 = 1;

		PathPoint var2;
		for(var2 = var0; var2.previous != null; var2 = var2.previous) {
			++var1;
		}

		PathPoint[] var3 = new PathPoint[var1];
		var2 = var0;
		--var1;

		for(var3[var1] = var0; var2.previous != null; var3[var1] = var2) {
			var2 = var2.previous;
			--var1;
		}

		return new PathEntity(var3);
	}
}
