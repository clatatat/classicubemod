package net.minecraft.client.render;

import net.minecraft.client.render.camera.Frustrum;
import net.minecraft.game.entity.Entity;
import net.minecraft.game.physics.AxisAlignedBB;
import net.minecraft.game.world.World;
import net.minecraft.game.world.block.Block;
import net.minecraft.game.world.chunk.Chunk;
import org.lwjgl.opengl.GL11;
import util.MathHelper;

public final class WorldRenderer {
	private World worldObj;
	private int glRenderList = -1;
	private static Tessellator tessellator = Tessellator.instance;
	public static int chunksUpdated = 0;
	private int posX;
	private int posY;
	private int posZ;
	private int sizeWidth;
	private int sizeHeight;
	private int sizeDepth;
	public int posXMinus;
	public int posYMinus;
	public int posZMinus;
	private int posXClip;
	private int posYClip;
	private int posZClip;
	public boolean isInFrustum = false;
	private boolean[] skipRenderPass = new boolean[2];
	private int posXPlus;
	private int posYPlus;
	private int posZPlus;
	public boolean needsUpdate;
	private AxisAlignedBB rendererBoundingBox;
	private RenderBlocks renderBlocks;
	public boolean isVisible = true;
	public boolean isWaitingOnOcclusionQuery;
	public int glOcclusionQuery;
	public boolean isChunkLit;

	public WorldRenderer(World var1, int var2, int var3, int var4, int var5, int var6) {
		this.renderBlocks = new RenderBlocks(var1);
		this.worldObj = var1;
		this.sizeWidth = this.sizeHeight = this.sizeDepth = 16;
		MathHelper.sqrt_float((float)(this.sizeWidth * this.sizeWidth + this.sizeHeight * this.sizeHeight + this.sizeDepth * this.sizeDepth));
		this.glRenderList = var6;
		this.posX = -999;
		this.setPosition(var2, var3, var4);
		this.needsUpdate = false;
	}

	public final void setPosition(int var1, int var2, int var3) {
		if(var1 != this.posX || var2 != this.posY || var3 != this.posZ) {
			this.setDontDraw();
			this.posX = var1;
			this.posY = var2;
			this.posZ = var3;
			this.posXPlus = var1 + this.sizeWidth / 2;
			this.posYPlus = var2 + this.sizeHeight / 2;
			this.posZPlus = var3 + this.sizeDepth / 2;
			this.posXClip = var1 & 511;
			this.posYClip = var2 & 511;
			this.posZClip = var3 & 511;
			this.posXMinus = var1 - this.posXClip;
			this.posYMinus = var2 - this.posYClip;
			this.posZMinus = var3 - this.posZClip;
			this.rendererBoundingBox = (new AxisAlignedBB((double)var1, (double)var2, (double)var3, (double)(var1 + this.sizeWidth), (double)(var2 + this.sizeHeight), (double)(var3 + this.sizeDepth))).expand(2.0D, 2.0D, 2.0D);
			GL11.glNewList(this.glRenderList + 2, GL11.GL_COMPILE);
			AxisAlignedBB var4 = new AxisAlignedBB((double)((float)this.posXClip - 2.0F), (double)((float)this.posYClip - 2.0F), (double)((float)this.posZClip - 2.0F), (double)((float)(this.posXClip + this.sizeWidth) + 2.0F), (double)((float)(this.posYClip + this.sizeHeight) + 2.0F), (double)((float)(this.posZClip + this.sizeDepth) + 2.0F));
			Tessellator var5 = Tessellator.instance;
			var5.startDrawingQuads();
			var5.addVertex(var4.minX, var4.maxY, var4.minZ);
			var5.addVertex(var4.maxX, var4.maxY, var4.minZ);
			var5.addVertex(var4.maxX, var4.minY, var4.minZ);
			var5.addVertex(var4.minX, var4.minY, var4.minZ);
			var5.addVertex(var4.minX, var4.minY, var4.maxZ);
			var5.addVertex(var4.maxX, var4.minY, var4.maxZ);
			var5.addVertex(var4.maxX, var4.maxY, var4.maxZ);
			var5.addVertex(var4.minX, var4.maxY, var4.maxZ);
			var5.addVertex(var4.minX, var4.minY, var4.minZ);
			var5.addVertex(var4.maxX, var4.minY, var4.minZ);
			var5.addVertex(var4.maxX, var4.minY, var4.maxZ);
			var5.addVertex(var4.minX, var4.minY, var4.maxZ);
			var5.addVertex(var4.minX, var4.maxY, var4.maxZ);
			var5.addVertex(var4.maxX, var4.maxY, var4.maxZ);
			var5.addVertex(var4.maxX, var4.maxY, var4.minZ);
			var5.addVertex(var4.minX, var4.maxY, var4.minZ);
			var5.addVertex(var4.minX, var4.minY, var4.maxZ);
			var5.addVertex(var4.minX, var4.maxY, var4.maxZ);
			var5.addVertex(var4.minX, var4.maxY, var4.minZ);
			var5.addVertex(var4.minX, var4.minY, var4.minZ);
			var5.addVertex(var4.maxX, var4.minY, var4.minZ);
			var5.addVertex(var4.maxX, var4.maxY, var4.minZ);
			var5.addVertex(var4.maxX, var4.maxY, var4.maxZ);
			var5.addVertex(var4.maxX, var4.minY, var4.maxZ);
			var5.draw();
			GL11.glEndList();
			this.needsUpdate = true;
		}
	}

	public final void updateRenderer() {
		if(this.needsUpdate) {
			++chunksUpdated;
			int var1 = this.posX;
			int var2 = this.posY;
			int var3 = this.posZ;
			int var4 = this.posX + this.sizeWidth;
			int var5 = this.posY + this.sizeHeight;
			int var6 = this.posZ + this.sizeDepth;

			int var7;
			for(var7 = 0; var7 < 2; ++var7) {
				this.skipRenderPass[var7] = true;
			}

			Chunk.isLit = false;

			for(var7 = 0; var7 < 2; ++var7) {
				boolean var8 = false;
				boolean var9 = false;
				GL11.glNewList(this.glRenderList + var7, GL11.GL_COMPILE);
				GL11.glPushMatrix();
				GL11.glTranslatef((float)this.posXClip, (float)this.posYClip, (float)this.posZClip);
				tessellator.startDrawingQuads();
				tessellator.setTranslationD((double)(-this.posX), (double)(-this.posY), (double)(-this.posZ));

				for(int var10 = var2; var10 < var5; ++var10) {
					for(int var11 = var3; var11 < var6; ++var11) {
						for(int var12 = var1; var12 < var4; ++var12) {
							int var13 = this.worldObj.getBlockId(var12, var10, var11);
							if(var13 > 0) {
								Block var14 = Block.blocksList[var13];
								if(var14.getRenderBlockPass() != var7) {
									var8 = true;
								} else {
									var9 |= this.renderBlocks.renderBlockByRenderType(var14, var12, var10, var11);
								}
							}
						}
					}
				}

				tessellator.draw();
				GL11.glPopMatrix();
				GL11.glEndList();
				tessellator.setTranslationD(0.0D, 0.0D, 0.0D);
				if(var9) {
					this.skipRenderPass[var7] = false;
				}

				if(!var8) {
					break;
				}
			}

			this.isChunkLit = Chunk.isLit;
		}
	}

	public final float distanceToEntitySquared(Entity var1) {
		float var2 = (float)(var1.posX - (double)this.posXPlus);
		float var3 = (float)(var1.posY - (double)this.posYPlus);
		float var4 = (float)(var1.posZ - (double)this.posZPlus);
		return var2 * var2 + var3 * var3 + var4 * var4;
	}

	private void setDontDraw() {
		for(int var1 = 0; var1 < 2; ++var1) {
			this.skipRenderPass[var1] = true;
		}

	}

	public final void stopRendering() {
		this.setDontDraw();
		this.worldObj = null;
	}

	public final int getGLCallListForPass(int var1) {
		return !this.isInFrustum ? -1 : (!this.skipRenderPass[var1] ? this.glRenderList + var1 : -1);
	}

	public final void updateInFrustrum(Frustrum var1) {
		this.isInFrustum = var1.isBoundingBoxInFrustrum(this.rendererBoundingBox);
	}

	public final void callOcclusionQueryList() {
		GL11.glCallList(this.glRenderList + 2);
	}

	public final boolean skipAllRenderPasses() {
		return this.skipRenderPass[0] && this.skipRenderPass[1];
	}
}
