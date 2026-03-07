package net.minecraft.client.render;

import net.minecraft.game.world.World;
import net.minecraft.game.world.block.Block;
import net.minecraft.game.world.material.Material;
import org.lwjgl.opengl.GL11;

public final class RenderBlocks {
	private World blockAccess;
	private int overrideBlockTexture = -1;
	private boolean flipTexture = false;

	public RenderBlocks(World var1) {
		this.blockAccess = var1;
	}

	public RenderBlocks() {
	}

	public final void renderBlockUsingTexture(Block var1, int var2, int var3, int var4, int var5) {
		this.overrideBlockTexture = var5;
		this.renderBlockByRenderType(var1, var2, var3, var4);
		this.overrideBlockTexture = -1;
	}

	public final void renderBlockAllFaces(Block var1, int var2, int var3, int var4) {
		this.flipTexture = true;
		this.renderBlockByRenderType(var1, var2, var3, var4);
		this.flipTexture = false;
	}

	public final boolean renderBlockByRenderType(Block var1, int var2, int var3, int var4) {
		int var5 = var1.getRenderType();
		Tessellator var6;
		boolean var45;
		if(var5 == 0) {
			var6 = Tessellator.instance;
			var45 = false;
			float var47;
			if(this.flipTexture || var1.shouldSideBeRendered(this.blockAccess, var2, var3 - 1, var4, 0)) {
				var47 = var1.getBlockBrightness(this.blockAccess, var2, var3 - 1, var4);
				if(Block.lightValue[var1.blockID] > 0) {
					var47 = 1.0F;
				}

				var6.setColorOpaque_F(0.5F * var47, 0.5F * var47, 0.5F * var47);
				this.renderBlockBottom(var1, (double)var2, (double)var3, (double)var4, var1.getBlockTexture(this.blockAccess, var2, var3, var4, 0));
				var45 = true;
			}

			if(this.flipTexture || var1.shouldSideBeRendered(this.blockAccess, var2, var3 + 1, var4, 1)) {
				var47 = var1.getBlockBrightness(this.blockAccess, var2, var3 + 1, var4);
				if(Block.lightValue[var1.blockID] > 0) {
					var47 = 1.0F;
				}

				var6.setColorOpaque_F(var47 * 1.0F, var47 * 1.0F, var47 * 1.0F);
				this.renderBlockTop(var1, (double)var2, (double)var3, (double)var4, var1.getBlockTexture(this.blockAccess, var2, var3, var4, 1));
				var45 = true;
			}

			if(this.flipTexture || var1.shouldSideBeRendered(this.blockAccess, var2, var3, var4 - 1, 2)) {
				var47 = var1.getBlockBrightness(this.blockAccess, var2, var3, var4 - 1);
				if(Block.lightValue[var1.blockID] > 0) {
					var47 = 1.0F;
				}

				var6.setColorOpaque_F(0.8F * var47, 0.8F * var47, 0.8F * var47);
				this.renderBlockNorth(var1, (double)var2, (double)var3, (double)var4, var1.getBlockTexture(this.blockAccess, var2, var3, var4, 2));
				var45 = true;
			}

			if(this.flipTexture || var1.shouldSideBeRendered(this.blockAccess, var2, var3, var4 + 1, 3)) {
				var47 = var1.getBlockBrightness(this.blockAccess, var2, var3, var4 + 1);
				if(Block.lightValue[var1.blockID] > 0) {
					var47 = 1.0F;
				}

				var6.setColorOpaque_F(0.8F * var47, 0.8F * var47, 0.8F * var47);
				this.renderBlockSouth(var1, (double)var2, (double)var3, (double)var4, var1.getBlockTexture(this.blockAccess, var2, var3, var4, 3));
				var45 = true;
			}

			if(this.flipTexture || var1.shouldSideBeRendered(this.blockAccess, var2 - 1, var3, var4, 4)) {
				var47 = var1.getBlockBrightness(this.blockAccess, var2 - 1, var3, var4);
				if(Block.lightValue[var1.blockID] > 0) {
					var47 = 1.0F;
				}

				var6.setColorOpaque_F(0.6F * var47, 0.6F * var47, 0.6F * var47);
				this.renderBlockWest(var1, (double)var2, (double)var3, (double)var4, var1.getBlockTexture(this.blockAccess, var2, var3, var4, 4));
				var45 = true;
			}

			if(this.flipTexture || var1.shouldSideBeRendered(this.blockAccess, var2 + 1, var3, var4, 5)) {
				var47 = var1.getBlockBrightness(this.blockAccess, var2 + 1, var3, var4);
				if(Block.lightValue[var1.blockID] > 0) {
					var47 = 1.0F;
				}

				var6.setColorOpaque_F(0.6F * var47, 0.6F * var47, 0.6F * var47);
				this.renderBlockEast(var1, (double)var2, (double)var3, (double)var4, var1.getBlockTexture(this.blockAccess, var2, var3, var4, 5));
				var45 = true;
			}

			return var45;
		} else {
			double var17;
			double var19;
			if(var5 == 4) {
				var6 = Tessellator.instance;
				var45 = false;
				var17 = var1.minY;
				var19 = var1.maxY;
				var1.maxY = var19 - (double)this.materialNotWater(var2, var3, var4);
				float var48;
				if(this.flipTexture || var1.shouldSideBeRendered(this.blockAccess, var2, var3 - 1, var4, 0)) {
					var48 = var1.getBlockBrightness(this.blockAccess, var2, var3 - 1, var4);
					var6.setColorOpaque_F(0.5F * var48, 0.5F * var48, 0.5F * var48);
					this.renderBlockBottom(var1, (double)var2, (double)var3, (double)var4, var1.getBlockTextureFromSide(0));
					var45 = true;
				}

				if(this.flipTexture || var1.shouldSideBeRendered(this.blockAccess, var2, var3 + 1, var4, 1)) {
					var48 = var1.getBlockBrightness(this.blockAccess, var2, var3 + 1, var4);
					var6.setColorOpaque_F(var48 * 1.0F, var48 * 1.0F, var48 * 1.0F);
					this.renderBlockTop(var1, (double)var2, (double)var3, (double)var4, var1.getBlockTextureFromSide(1));
					var45 = true;
				}

				var1.minY = var19 - (double)this.materialNotWater(var2, var3, var4 - 1);
				if(this.flipTexture || var1.maxY > var1.minY || var1.shouldSideBeRendered(this.blockAccess, var2, var3, var4 - 1, 2)) {
					var48 = var1.getBlockBrightness(this.blockAccess, var2, var3, var4 - 1);
					var6.setColorOpaque_F(0.8F * var48, 0.8F * var48, 0.8F * var48);
					this.renderBlockNorth(var1, (double)var2, (double)var3, (double)var4, var1.getBlockTextureFromSide(2));
					var45 = true;
				}

				var1.minY = var19 - (double)this.materialNotWater(var2, var3, var4 + 1);
				if(this.flipTexture || var1.maxY > var1.minY || var1.shouldSideBeRendered(this.blockAccess, var2, var3, var4 + 1, 3)) {
					var48 = var1.getBlockBrightness(this.blockAccess, var2, var3, var4 + 1);
					var6.setColorOpaque_F(0.8F * var48, 0.8F * var48, 0.8F * var48);
					this.renderBlockSouth(var1, (double)var2, (double)var3, (double)var4, var1.getBlockTextureFromSide(3));
					var45 = true;
				}

				var1.minY = var19 - (double)this.materialNotWater(var2 - 1, var3, var4);
				if(this.flipTexture || var1.maxY > var1.minY || var1.shouldSideBeRendered(this.blockAccess, var2 - 1, var3, var4, 4)) {
					var48 = var1.getBlockBrightness(this.blockAccess, var2 - 1, var3, var4);
					var6.setColorOpaque_F(0.6F * var48, 0.6F * var48, 0.6F * var48);
					this.renderBlockWest(var1, (double)var2, (double)var3, (double)var4, var1.getBlockTextureFromSide(4));
					var45 = true;
				}

				var1.minY = var19 - (double)this.materialNotWater(var2 + 1, var3, var4);
				if(this.flipTexture || var1.maxY > var1.minY || var1.shouldSideBeRendered(this.blockAccess, var2 + 1, var3, var4, 5)) {
					var48 = var1.getBlockBrightness(this.blockAccess, var2 + 1, var3, var4);
					var6.setColorOpaque_F(0.6F * var48, 0.6F * var48, 0.6F * var48);
					this.renderBlockEast(var1, (double)var2, (double)var3, (double)var4, var1.getBlockTextureFromSide(5));
					var45 = true;
				}

				var1.minY = var17;
				var1.maxY = var19;
				return var45;
			} else {
				float var44;
				if(var5 == 1) {
					var6 = Tessellator.instance;
					var44 = var1.getBlockBrightness(this.blockAccess, var2, var3, var4);
					var6.setColorOpaque_F(var44, var44, var44);
					this.renderBlockPlant(var1, this.blockAccess.getBlockMetadata(var2, var3, var4), (double)var2, (double)var3, (double)var4);
					return true;
				} else if(var5 == 6) {
					var6 = Tessellator.instance;
					var44 = var1.getBlockBrightness(this.blockAccess, var2, var3, var4);
					var6.setColorOpaque_F(var44, var44, var44);
					this.renderBlockCrops(var1, this.blockAccess.getBlockMetadata(var2, var3, var4), (double)var2, (double)((float)var3 - 1.0F / 16.0F), (double)var4);
					return true;
				} else {
					float var8;
					if(var5 == 2) {
						int var41 = this.blockAccess.getBlockMetadata(var2, var3, var4);
						Tessellator var43 = Tessellator.instance;
						var8 = var1.getBlockBrightness(this.blockAccess, var2, var3, var4);
						if(Block.lightValue[var1.blockID] > 0) {
							var8 = 1.0F;
						}

						var43.setColorOpaque_F(var8, var8, var8);
						if(var41 == 1) {
							this.renderBlockTorch(var1, (double)var2 - (double)0.099999994F, (double)var3 + (double)0.2F, (double)var4, (double)-0.4F, 0.0D);
						} else if(var41 == 2) {
							this.renderBlockTorch(var1, (double)var2 + (double)0.099999994F, (double)var3 + (double)0.2F, (double)var4, (double)0.4F, 0.0D);
						} else if(var41 == 3) {
							this.renderBlockTorch(var1, (double)var2, (double)var3 + (double)0.2F, (double)var4 - (double)0.099999994F, 0.0D, (double)-0.4F);
						} else if(var41 == 4) {
							this.renderBlockTorch(var1, (double)var2, (double)var3 + (double)0.2F, (double)var4 + (double)0.099999994F, 0.0D, (double)0.4F);
						} else {
							this.renderBlockTorch(var1, (double)var2, (double)var3, (double)var4, 0.0D, 0.0D);
						}

						return true;
					} else {
						int var7;
						double var25;
						double var27;
						double var29;
						double var31;
						int var42;
						if(var5 == 3) {
							var5 = var4;
							var4 = var3;
							var3 = var2;
							var6 = Tessellator.instance;
							var7 = var1.getBlockTextureFromSide(0);
							if(this.overrideBlockTexture >= 0) {
								var7 = this.overrideBlockTexture;
							}

							var8 = var1.getBlockBrightness(this.blockAccess, var2, var4, var5);
							var6.setColorOpaque_F(var8, var8, var8);
							var2 = (var7 & 15) << 4;
							var42 = var7 & 240;
							double var46 = (double)((float)var2 / 256.0F);
							double var18 = (double)(((float)var2 + 15.99F) / 256.0F);
							double var20 = (double)((float)var42 / 256.0F);
							double var22 = (double)(((float)var42 + 15.99F) / 256.0F);
							double var33;
							if(!this.blockAccess.isSolid(var3, var4 - 1, var5) && !Block.fire.canBlockCatchFire(this.blockAccess, var3, var4 - 1, var5)) {
								if((var3 + var4 + var5 & 1) == 1) {
									var46 = (double)((float)var2 / 256.0F);
									var18 = (double)(((float)var2 + 15.99F) / 256.0F);
									var20 = (double)((float)(var42 + 16) / 256.0F);
									var22 = (double)(((float)var42 + 15.99F + 16.0F) / 256.0F);
								}

								if((var3 / 2 + var4 / 2 + var5 / 2 & 1) == 1) {
									var27 = var18;
									var18 = var46;
									var46 = var27;
								}

								if(Block.fire.canBlockCatchFire(this.blockAccess, var3 - 1, var4, var5)) {
									var6.addVertexWithUV((double)((float)var3 + 0.2F), (double)((float)var4 + 1.4F + 1.0F / 16.0F), (double)(var5 + 1), var18, var20);
									var6.addVertexWithUV((double)var3, (double)((float)var4 + 1.0F / 16.0F), (double)(var5 + 1), var18, var22);
									var6.addVertexWithUV((double)var3, (double)((float)var4 + 1.0F / 16.0F), (double)var5, var46, var22);
									var6.addVertexWithUV((double)((float)var3 + 0.2F), (double)((float)var4 + 1.4F + 1.0F / 16.0F), (double)var5, var46, var20);
									var6.addVertexWithUV((double)((float)var3 + 0.2F), (double)((float)var4 + 1.4F + 1.0F / 16.0F), (double)var5, var46, var20);
									var6.addVertexWithUV((double)var3, (double)((float)var4 + 1.0F / 16.0F), (double)var5, var46, var22);
									var6.addVertexWithUV((double)var3, (double)((float)var4 + 1.0F / 16.0F), (double)(var5 + 1), var18, var22);
									var6.addVertexWithUV((double)((float)var3 + 0.2F), (double)((float)var4 + 1.4F + 1.0F / 16.0F), (double)(var5 + 1), var18, var20);
								}

								if(Block.fire.canBlockCatchFire(this.blockAccess, var3 + 1, var4, var5)) {
									var6.addVertexWithUV((double)((float)(var3 + 1) - 0.2F), (double)((float)var4 + 1.4F + 1.0F / 16.0F), (double)var5, var46, var20);
									var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + 1.0F / 16.0F), (double)var5, var46, var22);
									var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + 1.0F / 16.0F), (double)(var5 + 1), var18, var22);
									var6.addVertexWithUV((double)((float)(var3 + 1) - 0.2F), (double)((float)var4 + 1.4F + 1.0F / 16.0F), (double)(var5 + 1), var18, var20);
									var6.addVertexWithUV((double)((float)(var3 + 1) - 0.2F), (double)((float)var4 + 1.4F + 1.0F / 16.0F), (double)(var5 + 1), var18, var20);
									var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + 1.0F / 16.0F), (double)(var5 + 1), var18, var22);
									var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + 1.0F / 16.0F), (double)var5, var46, var22);
									var6.addVertexWithUV((double)((float)(var3 + 1) - 0.2F), (double)((float)var4 + 1.4F + 1.0F / 16.0F), (double)var5, var46, var20);
								}

								if(Block.fire.canBlockCatchFire(this.blockAccess, var3, var4, var5 - 1)) {
									var6.addVertexWithUV((double)var3, (double)((float)var4 + 1.4F + 1.0F / 16.0F), (double)((float)var5 + 0.2F), var18, var20);
									var6.addVertexWithUV((double)var3, (double)((float)var4 + 1.0F / 16.0F), (double)var5, var18, var22);
									var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + 1.0F / 16.0F), (double)var5, var46, var22);
									var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + 1.4F + 1.0F / 16.0F), (double)((float)var5 + 0.2F), var46, var20);
									var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + 1.4F + 1.0F / 16.0F), (double)((float)var5 + 0.2F), var46, var20);
									var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + 1.0F / 16.0F), (double)var5, var46, var22);
									var6.addVertexWithUV((double)var3, (double)((float)var4 + 1.0F / 16.0F), (double)var5, var18, var22);
									var6.addVertexWithUV((double)var3, (double)((float)var4 + 1.4F + 1.0F / 16.0F), (double)((float)var5 + 0.2F), var18, var20);
								}

								if(Block.fire.canBlockCatchFire(this.blockAccess, var3, var4, var5 + 1)) {
									var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + 1.4F + 1.0F / 16.0F), (double)((float)(var5 + 1) - 0.2F), var46, var20);
									var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + 1.0F / 16.0F), (double)(var5 + 1), var46, var22);
									var6.addVertexWithUV((double)var3, (double)((float)var4 + 1.0F / 16.0F), (double)(var5 + 1), var18, var22);
									var6.addVertexWithUV((double)var3, (double)((float)var4 + 1.4F + 1.0F / 16.0F), (double)((float)(var5 + 1) - 0.2F), var18, var20);
									var6.addVertexWithUV((double)var3, (double)((float)var4 + 1.4F + 1.0F / 16.0F), (double)((float)(var5 + 1) - 0.2F), var18, var20);
									var6.addVertexWithUV((double)var3, (double)((float)var4 + 1.0F / 16.0F), (double)(var5 + 1), var18, var22);
									var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + 1.0F / 16.0F), (double)(var5 + 1), var46, var22);
									var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + 1.4F + 1.0F / 16.0F), (double)((float)(var5 + 1) - 0.2F), var46, var20);
								}

								if(Block.fire.canBlockCatchFire(this.blockAccess, var3, var4 + 1, var5)) {
									var27 = (double)var3 + 0.5D + 0.5D;
									var29 = (double)var3 + 0.5D - 0.5D;
									var31 = (double)var5 + 0.5D + 0.5D;
									var33 = (double)var5 + 0.5D - 0.5D;
									var46 = (double)((float)var2 / 256.0F);
									var18 = (double)(((float)var2 + 15.99F) / 256.0F);
									var20 = (double)((float)var42 / 256.0F);
									var22 = (double)(((float)var42 + 15.99F) / 256.0F);
									++var4;
									if((var3 + var4 + var5 & 1) == 0) {
										var6.addVertexWithUV(var29, (double)((float)var4 + -0.2F), (double)var5, var18, var20);
										var6.addVertexWithUV(var27, (double)var4, (double)var5, var18, var22);
										var6.addVertexWithUV(var27, (double)var4, (double)(var5 + 1), var46, var22);
										var6.addVertexWithUV(var29, (double)((float)var4 + -0.2F), (double)(var5 + 1), var46, var20);
										var46 = (double)((float)var2 / 256.0F);
										var18 = (double)(((float)var2 + 15.99F) / 256.0F);
										var20 = (double)((float)(var42 + 16) / 256.0F);
										var22 = (double)(((float)var42 + 15.99F + 16.0F) / 256.0F);
										var6.addVertexWithUV(var27, (double)((float)var4 + -0.2F), (double)(var5 + 1), var18, var20);
										var6.addVertexWithUV(var29, (double)var4, (double)(var5 + 1), var18, var22);
										var6.addVertexWithUV(var29, (double)var4, (double)var5, var46, var22);
										var6.addVertexWithUV(var27, (double)((float)var4 + -0.2F), (double)var5, var46, var20);
									} else {
										var6.addVertexWithUV((double)var3, (double)((float)var4 + -0.2F), var31, var18, var20);
										var6.addVertexWithUV((double)var3, (double)var4, var33, var18, var22);
										var6.addVertexWithUV((double)(var3 + 1), (double)var4, var33, var46, var22);
										var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + -0.2F), var31, var46, var20);
										var46 = (double)((float)var2 / 256.0F);
										var18 = (double)(((float)var2 + 15.99F) / 256.0F);
										var20 = (double)((float)(var42 + 16) / 256.0F);
										var22 = (double)(((float)var42 + 15.99F + 16.0F) / 256.0F);
										var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + -0.2F), var33, var18, var20);
										var6.addVertexWithUV((double)(var3 + 1), (double)var4, var31, var18, var22);
										var6.addVertexWithUV((double)var3, (double)var4, var31, var46, var22);
										var6.addVertexWithUV((double)var3, (double)((float)var4 + -0.2F), var33, var46, var20);
									}
								}
							} else {
								var25 = (double)var3 + 0.5D + 0.2D;
								var27 = (double)var3 + 0.5D - 0.2D;
								var29 = (double)var5 + 0.5D + 0.2D;
								var31 = (double)var5 + 0.5D - 0.2D;
								var33 = (double)var3 + 0.5D - 0.3D;
								double var35 = (double)var3 + 0.5D + 0.3D;
								double var37 = (double)var5 + 0.5D - 0.3D;
								double var39 = (double)var5 + 0.5D + 0.3D;
								var6.addVertexWithUV(var33, (double)((float)var4 + 1.4F), (double)(var5 + 1), var18, var20);
								var6.addVertexWithUV(var25, (double)var4, (double)(var5 + 1), var18, var22);
								var6.addVertexWithUV(var25, (double)var4, (double)var5, var46, var22);
								var6.addVertexWithUV(var33, (double)((float)var4 + 1.4F), (double)var5, var46, var20);
								var6.addVertexWithUV(var35, (double)((float)var4 + 1.4F), (double)var5, var18, var20);
								var6.addVertexWithUV(var27, (double)var4, (double)var5, var18, var22);
								var6.addVertexWithUV(var27, (double)var4, (double)(var5 + 1), var46, var22);
								var6.addVertexWithUV(var35, (double)((float)var4 + 1.4F), (double)(var5 + 1), var46, var20);
								var46 = (double)((float)var2 / 256.0F);
								var18 = (double)(((float)var2 + 15.99F) / 256.0F);
								var20 = (double)((float)(var42 + 16) / 256.0F);
								var22 = (double)(((float)var42 + 15.99F + 16.0F) / 256.0F);
								var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + 1.4F), var39, var18, var20);
								var6.addVertexWithUV((double)(var3 + 1), (double)var4, var31, var18, var22);
								var6.addVertexWithUV((double)var3, (double)var4, var31, var46, var22);
								var6.addVertexWithUV((double)var3, (double)((float)var4 + 1.4F), var39, var46, var20);
								var6.addVertexWithUV((double)var3, (double)((float)var4 + 1.4F), var37, var18, var20);
								var6.addVertexWithUV((double)var3, (double)var4, var29, var18, var22);
								var6.addVertexWithUV((double)(var3 + 1), (double)var4, var29, var46, var22);
								var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + 1.4F), var37, var46, var20);
								var25 = (double)var3 + 0.5D - 0.5D;
								var27 = (double)var3 + 0.5D + 0.5D;
								var29 = (double)var5 + 0.5D - 0.5D;
								var31 = (double)var5 + 0.5D + 0.5D;
								var33 = (double)var3 + 0.5D - 0.4D;
								var35 = (double)var3 + 0.5D + 0.4D;
								var37 = (double)var5 + 0.5D - 0.4D;
								var39 = (double)var5 + 0.5D + 0.4D;
								var6.addVertexWithUV(var33, (double)((float)var4 + 1.4F), (double)var5, var46, var20);
								var6.addVertexWithUV(var25, (double)var4, (double)var5, var46, var22);
								var6.addVertexWithUV(var25, (double)var4, (double)(var5 + 1), var18, var22);
								var6.addVertexWithUV(var33, (double)((float)var4 + 1.4F), (double)(var5 + 1), var18, var20);
								var6.addVertexWithUV(var35, (double)((float)var4 + 1.4F), (double)(var5 + 1), var46, var20);
								var6.addVertexWithUV(var27, (double)var4, (double)(var5 + 1), var46, var22);
								var6.addVertexWithUV(var27, (double)var4, (double)var5, var18, var22);
								var6.addVertexWithUV(var35, (double)((float)var4 + 1.4F), (double)var5, var18, var20);
								var46 = (double)((float)var2 / 256.0F);
								var18 = (double)(((float)var2 + 15.99F) / 256.0F);
								var20 = (double)((float)var42 / 256.0F);
								var22 = (double)(((float)var42 + 15.99F) / 256.0F);
								var6.addVertexWithUV((double)var3, (double)((float)var4 + 1.4F), var39, var46, var20);
								var6.addVertexWithUV((double)var3, (double)var4, var31, var46, var22);
								var6.addVertexWithUV((double)(var3 + 1), (double)var4, var31, var18, var22);
								var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + 1.4F), var39, var18, var20);
								var6.addVertexWithUV((double)(var3 + 1), (double)((float)var4 + 1.4F), var37, var46, var20);
								var6.addVertexWithUV((double)(var3 + 1), (double)var4, var29, var46, var22);
								var6.addVertexWithUV((double)var3, (double)var4, var29, var18, var22);
								var6.addVertexWithUV((double)var3, (double)((float)var4 + 1.4F), var37, var18, var20);
							}

							return true;
						} else if(var5 == 5) {
							var5 = var4;
							var4 = var3;
							var3 = var2;
							var6 = Tessellator.instance;
							var7 = var1.getBlockTextureFromSide(0);
							if(this.overrideBlockTexture >= 0) {
								var7 = this.overrideBlockTexture;
							}

							var8 = var1.getBlockBrightness(this.blockAccess, var2, var4, var5);
							var6.setColorOpaque_F(var8, var8, var8);
							var2 = ((var7 & 15) << 4) + 16;
							var42 = (var7 & 15) << 4;
							int var16 = var7 & 240;
							if((var3 + var4 + var5 & 1) == 1) {
								var2 = (var7 & 15) << 4;
								var42 = ((var7 & 15) << 4) + 16;
							}

							var17 = (double)((float)var2 / 256.0F);
							var19 = (double)(((float)var2 + 15.99F) / 256.0F);
							double var21 = (double)((float)var16 / 256.0F);
							double var23 = (double)(((float)var16 + 15.99F) / 256.0F);
							var25 = (double)((float)var42 / 256.0F);
							var27 = (double)(((float)var42 + 15.99F) / 256.0F);
							var29 = (double)((float)var16 / 256.0F);
							var31 = (double)(((float)var16 + 15.99F) / 256.0F);
							if(this.blockAccess.isSolid(var3 - 1, var4, var5)) {
								var6.addVertexWithUV((double)((float)var3 + 0.05F), (double)((float)(var4 + 1) + 2.0F / 16.0F), (double)((float)(var5 + 1) + 2.0F / 16.0F), var17, var21);
								var6.addVertexWithUV((double)((float)var3 + 0.05F), (double)((float)var4 - 2.0F / 16.0F), (double)((float)(var5 + 1) + 2.0F / 16.0F), var17, var23);
								var6.addVertexWithUV((double)((float)var3 + 0.05F), (double)((float)var4 - 2.0F / 16.0F), (double)((float)var5 - 2.0F / 16.0F), var19, var23);
								var6.addVertexWithUV((double)((float)var3 + 0.05F), (double)((float)(var4 + 1) + 2.0F / 16.0F), (double)((float)var5 - 2.0F / 16.0F), var19, var21);
							}

							if(this.blockAccess.isSolid(var3 + 1, var4, var5)) {
								var6.addVertexWithUV((double)((float)(var3 + 1) - 0.05F), (double)((float)var4 - 2.0F / 16.0F), (double)((float)(var5 + 1) + 2.0F / 16.0F), var19, var23);
								var6.addVertexWithUV((double)((float)(var3 + 1) - 0.05F), (double)((float)(var4 + 1) + 2.0F / 16.0F), (double)((float)(var5 + 1) + 2.0F / 16.0F), var19, var21);
								var6.addVertexWithUV((double)((float)(var3 + 1) - 0.05F), (double)((float)(var4 + 1) + 2.0F / 16.0F), (double)((float)var5 - 2.0F / 16.0F), var17, var21);
								var6.addVertexWithUV((double)((float)(var3 + 1) - 0.05F), (double)((float)var4 - 2.0F / 16.0F), (double)((float)var5 - 2.0F / 16.0F), var17, var23);
							}

							if(this.blockAccess.isSolid(var3, var4, var5 - 1)) {
								var6.addVertexWithUV((double)((float)(var3 + 1) + 2.0F / 16.0F), (double)((float)var4 - 2.0F / 16.0F), (double)((float)var5 + 0.05F), var27, var31);
								var6.addVertexWithUV((double)((float)(var3 + 1) + 2.0F / 16.0F), (double)((float)(var4 + 1) + 2.0F / 16.0F), (double)((float)var5 + 0.05F), var27, var29);
								var6.addVertexWithUV((double)((float)var3 - 2.0F / 16.0F), (double)((float)(var4 + 1) + 2.0F / 16.0F), (double)((float)var5 + 0.05F), var25, var29);
								var6.addVertexWithUV((double)((float)var3 - 2.0F / 16.0F), (double)((float)var4 - 2.0F / 16.0F), (double)((float)var5 + 0.05F), var25, var31);
							}

							if(this.blockAccess.isSolid(var3, var4, var5 + 1)) {
								var6.addVertexWithUV((double)((float)(var3 + 1) + 2.0F / 16.0F), (double)((float)(var4 + 1) + 2.0F / 16.0F), (double)((float)(var5 + 1) - 0.05F), var25, var29);
								var6.addVertexWithUV((double)((float)(var3 + 1) + 2.0F / 16.0F), (double)((float)var4 - 2.0F / 16.0F), (double)((float)(var5 + 1) - 0.05F), var25, var31);
								var6.addVertexWithUV((double)((float)var3 - 2.0F / 16.0F), (double)((float)var4 - 2.0F / 16.0F), (double)((float)(var5 + 1) - 0.05F), var27, var31);
								var6.addVertexWithUV((double)((float)var3 - 2.0F / 16.0F), (double)((float)(var4 + 1) + 2.0F / 16.0F), (double)((float)(var5 + 1) - 0.05F), var27, var29);
							}

							return true;
						} else {
							return false;
						}
					}
				}
			}
		}
	}

	private void renderBlockTorch(Block var1, double var2, double var4, double var6, double var8, double var10) {
		Tessellator var12 = Tessellator.instance;
		int var36 = var1.getBlockTextureFromSide(0);
		if(this.overrideBlockTexture >= 0) {
			var36 = this.overrideBlockTexture;
		}

		int var13 = (var36 & 15) << 4;
		var36 &= 240;
		float var14 = (float)var13 / 256.0F;
		float var38 = ((float)var13 + 15.99F) / 256.0F;
		float var15 = (float)var36 / 256.0F;
		float var37 = ((float)var36 + 15.99F) / 256.0F;
		double var20 = (double)var14 + 1.75D / 64.0D;
		double var22 = (double)var15 + 6.0D / 256.0D;
		double var24 = (double)var14 + 9.0D / 256.0D;
		double var26 = (double)var15 + 1.0D / 32.0D;
		var2 += 0.5D;
		var6 += 0.5D;
		double var28 = var2 - 0.5D;
		double var30 = var2 + 0.5D;
		double var32 = var6 - 0.5D;
		double var34 = var6 + 0.5D;
		var12.addVertexWithUV(var2 + var8 * 0.375D - 1.0D / 16.0D, var4 + 0.625D, var6 + var10 * 0.375D - 1.0D / 16.0D, var20, var22);
		var12.addVertexWithUV(var2 + var8 * 0.375D - 1.0D / 16.0D, var4 + 0.625D, var6 + var10 * 0.375D + 1.0D / 16.0D, var20, var26);
		var12.addVertexWithUV(var2 + var8 * 0.375D + 1.0D / 16.0D, var4 + 0.625D, var6 + var10 * 0.375D + 1.0D / 16.0D, var24, var26);
		var12.addVertexWithUV(var2 + var8 * 0.375D + 1.0D / 16.0D, var4 + 0.625D, var6 + var10 * 0.375D - 1.0D / 16.0D, var24, var22);
		var12.addVertexWithUV(var2 - 1.0D / 16.0D, var4 + 1.0D, var32, (double)var14, (double)var15);
		var12.addVertexWithUV(var2 - 1.0D / 16.0D + var8, var4, var32 + var10, (double)var14, (double)var37);
		var12.addVertexWithUV(var2 - 1.0D / 16.0D + var8, var4, var34 + var10, (double)var38, (double)var37);
		var12.addVertexWithUV(var2 - 1.0D / 16.0D, var4 + 1.0D, var34, (double)var38, (double)var15);
		var12.addVertexWithUV(var2 + 1.0D / 16.0D, var4 + 1.0D, var34, (double)var14, (double)var15);
		var12.addVertexWithUV(var2 + var8 + 1.0D / 16.0D, var4, var34 + var10, (double)var14, (double)var37);
		var12.addVertexWithUV(var2 + var8 + 1.0D / 16.0D, var4, var32 + var10, (double)var38, (double)var37);
		var12.addVertexWithUV(var2 + 1.0D / 16.0D, var4 + 1.0D, var32, (double)var38, (double)var15);
		var12.addVertexWithUV(var28, var4 + 1.0D, var6 + 1.0D / 16.0D, (double)var14, (double)var15);
		var12.addVertexWithUV(var28 + var8, var4, var6 + 1.0D / 16.0D + var10, (double)var14, (double)var37);
		var12.addVertexWithUV(var30 + var8, var4, var6 + 1.0D / 16.0D + var10, (double)var38, (double)var37);
		var12.addVertexWithUV(var30, var4 + 1.0D, var6 + 1.0D / 16.0D, (double)var38, (double)var15);
		var12.addVertexWithUV(var30, var4 + 1.0D, var6 - 1.0D / 16.0D, (double)var14, (double)var15);
		var12.addVertexWithUV(var30 + var8, var4, var6 - 1.0D / 16.0D + var10, (double)var14, (double)var37);
		var12.addVertexWithUV(var28 + var8, var4, var6 - 1.0D / 16.0D + var10, (double)var38, (double)var37);
		var12.addVertexWithUV(var28, var4 + 1.0D, var6 - 1.0D / 16.0D, (double)var38, (double)var15);
	}

	private void renderBlockPlant(Block var1, int var2, double var3, double var5, double var7) {
		Tessellator var9 = Tessellator.instance;
		int var29 = var1.getBlockTextureFromSideAndMetadata(0, var2);
		if(this.overrideBlockTexture >= 0) {
			var29 = this.overrideBlockTexture;
		}

		var2 = (var29 & 15) << 4;
		var29 &= 240;
		double var13 = (double)((float)var2 / 256.0F);
		double var15 = (double)(((float)var2 + 15.99F) / 256.0F);
		double var17 = (double)((float)var29 / 256.0F);
		double var19 = (double)(((float)var29 + 15.99F) / 256.0F);
		double var21 = var3 + 0.5D - (double)0.45F;
		double var23 = var3 + 0.5D + (double)0.45F;
		double var25 = var7 + 0.5D - (double)0.45F;
		double var27 = var7 + 0.5D + (double)0.45F;
		var9.addVertexWithUV(var21, var5 + 1.0D, var25, var13, var17);
		var9.addVertexWithUV(var21, var5, var25, var13, var19);
		var9.addVertexWithUV(var23, var5, var27, var15, var19);
		var9.addVertexWithUV(var23, var5 + 1.0D, var27, var15, var17);
		var9.addVertexWithUV(var23, var5 + 1.0D, var27, var13, var17);
		var9.addVertexWithUV(var23, var5, var27, var13, var19);
		var9.addVertexWithUV(var21, var5, var25, var15, var19);
		var9.addVertexWithUV(var21, var5 + 1.0D, var25, var15, var17);
		var9.addVertexWithUV(var21, var5 + 1.0D, var27, var13, var17);
		var9.addVertexWithUV(var21, var5, var27, var13, var19);
		var9.addVertexWithUV(var23, var5, var25, var15, var19);
		var9.addVertexWithUV(var23, var5 + 1.0D, var25, var15, var17);
		var9.addVertexWithUV(var23, var5 + 1.0D, var25, var13, var17);
		var9.addVertexWithUV(var23, var5, var25, var13, var19);
		var9.addVertexWithUV(var21, var5, var27, var15, var19);
		var9.addVertexWithUV(var21, var5 + 1.0D, var27, var15, var17);
	}

	private void renderBlockCrops(Block var1, int var2, double var3, double var5, double var7) {
		Tessellator var9 = Tessellator.instance;
		int var29 = var1.getBlockTextureFromSideAndMetadata(0, var2);
		if(this.overrideBlockTexture >= 0) {
			var29 = this.overrideBlockTexture;
		}

		var2 = (var29 & 15) << 4;
		var29 &= 240;
		double var13 = (double)((float)var2 / 256.0F);
		double var15 = (double)(((float)var2 + 15.99F) / 256.0F);
		double var17 = (double)((float)var29 / 256.0F);
		double var19 = (double)(((float)var29 + 15.99F) / 256.0F);
		double var21 = var3 + 0.5D - 0.25D;
		double var23 = var3 + 0.5D + 0.25D;
		double var25 = var7 + 0.5D - 0.5D;
		double var27 = var7 + 0.5D + 0.5D;
		var9.addVertexWithUV(var21, var5 + 1.0D, var25, var13, var17);
		var9.addVertexWithUV(var21, var5, var25, var13, var19);
		var9.addVertexWithUV(var21, var5, var27, var15, var19);
		var9.addVertexWithUV(var21, var5 + 1.0D, var27, var15, var17);
		var9.addVertexWithUV(var21, var5 + 1.0D, var27, var13, var17);
		var9.addVertexWithUV(var21, var5, var27, var13, var19);
		var9.addVertexWithUV(var21, var5, var25, var15, var19);
		var9.addVertexWithUV(var21, var5 + 1.0D, var25, var15, var17);
		var9.addVertexWithUV(var23, var5 + 1.0D, var27, var13, var17);
		var9.addVertexWithUV(var23, var5, var27, var13, var19);
		var9.addVertexWithUV(var23, var5, var25, var15, var19);
		var9.addVertexWithUV(var23, var5 + 1.0D, var25, var15, var17);
		var9.addVertexWithUV(var23, var5 + 1.0D, var25, var13, var17);
		var9.addVertexWithUV(var23, var5, var25, var13, var19);
		var9.addVertexWithUV(var23, var5, var27, var15, var19);
		var9.addVertexWithUV(var23, var5 + 1.0D, var27, var15, var17);
		var21 = var3 + 0.5D - 0.5D;
		var23 = var3 + 0.5D + 0.5D;
		var25 = var7 + 0.5D - 0.25D;
		var27 = var7 + 0.5D + 0.25D;
		var9.addVertexWithUV(var21, var5 + 1.0D, var25, var13, var17);
		var9.addVertexWithUV(var21, var5, var25, var13, var19);
		var9.addVertexWithUV(var23, var5, var25, var15, var19);
		var9.addVertexWithUV(var23, var5 + 1.0D, var25, var15, var17);
		var9.addVertexWithUV(var23, var5 + 1.0D, var25, var13, var17);
		var9.addVertexWithUV(var23, var5, var25, var13, var19);
		var9.addVertexWithUV(var21, var5, var25, var15, var19);
		var9.addVertexWithUV(var21, var5 + 1.0D, var25, var15, var17);
		var9.addVertexWithUV(var23, var5 + 1.0D, var27, var13, var17);
		var9.addVertexWithUV(var23, var5, var27, var13, var19);
		var9.addVertexWithUV(var21, var5, var27, var15, var19);
		var9.addVertexWithUV(var21, var5 + 1.0D, var27, var15, var17);
		var9.addVertexWithUV(var21, var5 + 1.0D, var27, var13, var17);
		var9.addVertexWithUV(var21, var5, var27, var13, var19);
		var9.addVertexWithUV(var23, var5, var27, var15, var19);
		var9.addVertexWithUV(var23, var5 + 1.0D, var27, var15, var17);
	}

	private float materialNotWater(int var1, int var2, int var3) {
		return this.blockAccess.getBlockMaterial(var1, var2, var3) != Material.water ? 1.0F : (float)this.blockAccess.getBlockMetadata(var1, var2, var3) / 9.0F;
	}

	private void renderBlockBottom(Block var1, double var2, double var4, double var6, int var8) {
		Tessellator var9 = Tessellator.instance;
		if(this.overrideBlockTexture >= 0) {
			var8 = this.overrideBlockTexture;
		}

		int var10 = (var8 & 15) << 4;
		var8 &= 240;
		double var12 = (double)((float)var10 / 256.0F);
		double var14 = (double)(((float)var10 + 15.99F) / 256.0F);
		double var16 = (double)((float)var8 / 256.0F);
		double var18 = (double)(((float)var8 + 15.99F) / 256.0F);
		double var20 = var2 + var1.minX;
		double var22 = var2 + var1.maxX;
		double var24 = var4 + var1.minY;
		double var26 = var6 + var1.minZ;
		double var28 = var6 + var1.maxZ;
		var9.addVertexWithUV(var20, var24, var28, var12, var18);
		var9.addVertexWithUV(var20, var24, var26, var12, var16);
		var9.addVertexWithUV(var22, var24, var26, var14, var16);
		var9.addVertexWithUV(var22, var24, var28, var14, var18);
	}

	private void renderBlockTop(Block var1, double var2, double var4, double var6, int var8) {
		Tessellator var9 = Tessellator.instance;
		if(this.overrideBlockTexture >= 0) {
			var8 = this.overrideBlockTexture;
		}

		int var10 = (var8 & 15) << 4;
		var8 &= 240;
		double var12 = (double)((float)var10 / 256.0F);
		double var14 = (double)(((float)var10 + 15.99F) / 256.0F);
		double var16 = (double)((float)var8 / 256.0F);
		double var18 = (double)(((float)var8 + 15.99F) / 256.0F);
		double var20 = var2 + var1.minX;
		double var22 = var2 + var1.maxX;
		double var24 = var4 + var1.maxY;
		double var26 = var6 + var1.minZ;
		double var28 = var6 + var1.maxZ;
		var9.addVertexWithUV(var22, var24, var28, var14, var18);
		var9.addVertexWithUV(var22, var24, var26, var14, var16);
		var9.addVertexWithUV(var20, var24, var26, var12, var16);
		var9.addVertexWithUV(var20, var24, var28, var12, var18);
	}

	private void renderBlockNorth(Block var1, double var2, double var4, double var6, int var8) {
		Tessellator var9 = Tessellator.instance;
		if(this.overrideBlockTexture >= 0) {
			var8 = this.overrideBlockTexture;
		}

		int var10 = (var8 & 15) << 4;
		var8 &= 240;
		double var12 = (double)((float)var10 / 256.0F);
		double var14 = ((double)var10 + 15.99D) / 256.0D;
		double var16;
		double var18;
		if(var1.minY >= 0.0D && var1.maxY <= 1.0D) {
			var16 = ((double)var8 + var1.minY * (double)15.99F) / 256.0D;
			var18 = ((double)var8 + var1.maxY * (double)15.99F) / 256.0D;
		} else {
			var16 = (double)((float)var8 / 256.0F);
			var18 = (double)(((float)var8 + 15.99F) / 256.0F);
		}

		double var20 = var2 + var1.minX;
		double var22 = var2 + var1.maxX;
		double var24 = var4 + var1.minY;
		double var26 = var4 + var1.maxY;
		double var28 = var6 + var1.minZ;
		var9.addVertexWithUV(var20, var26, var28, var14, var16);
		var9.addVertexWithUV(var22, var26, var28, var12, var16);
		var9.addVertexWithUV(var22, var24, var28, var12, var18);
		var9.addVertexWithUV(var20, var24, var28, var14, var18);
	}

	private void renderBlockSouth(Block var1, double var2, double var4, double var6, int var8) {
		Tessellator var9 = Tessellator.instance;
		if(this.overrideBlockTexture >= 0) {
			var8 = this.overrideBlockTexture;
		}

		int var10 = (var8 & 15) << 4;
		var8 &= 240;
		double var12 = (double)((float)var10 / 256.0F);
		double var14 = (double)(((float)var10 + 15.99F) / 256.0F);
		double var16;
		double var18;
		if(var1.minY >= 0.0D && var1.maxY <= 1.0D) {
			var16 = ((double)var8 + var1.minY * (double)15.99F) / 256.0D;
			var18 = ((double)var8 + var1.maxY * (double)15.99F) / 256.0D;
		} else {
			var16 = (double)((float)var8 / 256.0F);
			var18 = (double)(((float)var8 + 15.99F) / 256.0F);
		}

		double var20 = var2 + var1.minX;
		double var22 = var2 + var1.maxX;
		double var24 = var4 + var1.minY;
		double var26 = var4 + var1.maxY;
		double var28 = var6 + var1.maxZ;
		var9.addVertexWithUV(var20, var26, var28, var12, var16);
		var9.addVertexWithUV(var20, var24, var28, var12, var18);
		var9.addVertexWithUV(var22, var24, var28, var14, var18);
		var9.addVertexWithUV(var22, var26, var28, var14, var16);
	}

	private void renderBlockWest(Block var1, double var2, double var4, double var6, int var8) {
		Tessellator var9 = Tessellator.instance;
		if(this.overrideBlockTexture >= 0) {
			var8 = this.overrideBlockTexture;
		}

		int var10 = (var8 & 15) << 4;
		var8 &= 240;
		double var12 = (double)((float)var10 / 256.0F);
		double var14 = (double)(((float)var10 + 15.99F) / 256.0F);
		double var16;
		double var18;
		if(var1.minY >= 0.0D && var1.maxY <= 1.0D) {
			var16 = ((double)var8 + var1.minY * (double)15.99F) / 256.0D;
			var18 = ((double)var8 + var1.maxY * (double)15.99F) / 256.0D;
		} else {
			var16 = (double)((float)var8 / 256.0F);
			var18 = (double)(((float)var8 + 15.99F) / 256.0F);
		}

		double var20 = var2 + var1.minX;
		double var22 = var4 + var1.minY;
		double var24 = var4 + var1.maxY;
		double var26 = var6 + var1.minZ;
		double var28 = var6 + var1.maxZ;
		var9.addVertexWithUV(var20, var24, var28, var14, var16);
		var9.addVertexWithUV(var20, var24, var26, var12, var16);
		var9.addVertexWithUV(var20, var22, var26, var12, var18);
		var9.addVertexWithUV(var20, var22, var28, var14, var18);
	}

	private void renderBlockEast(Block var1, double var2, double var4, double var6, int var8) {
		Tessellator var9 = Tessellator.instance;
		if(this.overrideBlockTexture >= 0) {
			var8 = this.overrideBlockTexture;
		}

		int var10 = (var8 & 15) << 4;
		var8 &= 240;
		double var12 = (double)((float)var10 / 256.0F);
		double var14 = (double)(((float)var10 + 15.99F) / 256.0F);
		double var16;
		double var18;
		if(var1.minY >= 0.0D && var1.maxY <= 1.0D) {
			var16 = ((double)var8 + var1.minY * (double)15.99F) / 256.0D;
			var18 = ((double)var8 + var1.maxY * (double)15.99F) / 256.0D;
		} else {
			var16 = (double)((float)var8 / 256.0F);
			var18 = (double)(((float)var8 + 15.99F) / 256.0F);
		}

		double var20 = var2 + var1.maxX;
		double var22 = var4 + var1.minY;
		double var24 = var4 + var1.maxY;
		double var26 = var6 + var1.minZ;
		double var28 = var6 + var1.maxZ;
		var9.addVertexWithUV(var20, var22, var28, var12, var18);
		var9.addVertexWithUV(var20, var22, var26, var14, var18);
		var9.addVertexWithUV(var20, var24, var26, var14, var16);
		var9.addVertexWithUV(var20, var24, var28, var12, var16);
	}

	public final void renderBlockOnInventory(Block var1) {
		Tessellator var2 = Tessellator.instance;
		int var3 = var1.getRenderType();
		if(var3 == 0) {
			GL11.glTranslatef(-0.5F, -0.5F, -0.5F);
			var2.startDrawingQuads();
			Tessellator.setNormal(0.0F, -1.0F, 0.0F);
			this.renderBlockBottom(var1, 0.0D, 0.0D, 0.0D, var1.getBlockTextureFromSide(0));
			var2.draw();
			var2.startDrawingQuads();
			Tessellator.setNormal(0.0F, 1.0F, 0.0F);
			this.renderBlockTop(var1, 0.0D, 0.0D, 0.0D, var1.getBlockTextureFromSide(1));
			var2.draw();
			var2.startDrawingQuads();
			Tessellator.setNormal(0.0F, 0.0F, -1.0F);
			this.renderBlockNorth(var1, 0.0D, 0.0D, 0.0D, var1.getBlockTextureFromSide(2));
			var2.draw();
			var2.startDrawingQuads();
			Tessellator.setNormal(0.0F, 0.0F, 1.0F);
			this.renderBlockSouth(var1, 0.0D, 0.0D, 0.0D, var1.getBlockTextureFromSide(3));
			var2.draw();
			var2.startDrawingQuads();
			Tessellator.setNormal(-1.0F, 0.0F, 0.0F);
			this.renderBlockWest(var1, 0.0D, 0.0D, 0.0D, var1.getBlockTextureFromSide(4));
			var2.draw();
			var2.startDrawingQuads();
			Tessellator.setNormal(1.0F, 0.0F, 0.0F);
			this.renderBlockEast(var1, 0.0D, 0.0D, 0.0D, var1.getBlockTextureFromSide(5));
			var2.draw();
			GL11.glTranslatef(0.5F, 0.5F, 0.5F);
		} else if(var3 == 1) {
			var2.startDrawingQuads();
			Tessellator.setNormal(0.0F, -1.0F, 0.0F);
			this.renderBlockPlant(var1, -1, -0.5D, -0.5D, -0.5D);
			var2.draw();
		} else if(var3 == 6) {
			var2.startDrawingQuads();
			Tessellator.setNormal(0.0F, -1.0F, 0.0F);
			this.renderBlockCrops(var1, -1, -0.5D, -0.5D, -0.5D);
			var2.draw();
		} else {
			if(var3 == 2) {
				var2.startDrawingQuads();
				Tessellator.setNormal(0.0F, -1.0F, 0.0F);
				this.renderBlockTorch(var1, -0.5D, -0.5D, -0.5D, 0.0D, 0.0D);
				var2.draw();
			}

		}
	}
}
