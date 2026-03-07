package net.minecraft.client.render;

import java.nio.IntBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Random;
import net.minecraft.client.Minecraft;
import net.minecraft.client.effect.EntityBubbleFX;
import net.minecraft.client.effect.EntityExplodeFX;
import net.minecraft.client.effect.EntityFlameFX;
import net.minecraft.client.effect.EntityLavaFX;
import net.minecraft.client.effect.EntitySmokeFX;
import net.minecraft.client.effect.EntitySplashFX;
import net.minecraft.client.player.EntityPlayerSP;
import net.minecraft.client.render.camera.Frustrum;
import net.minecraft.client.render.entity.RenderManager;
import net.minecraft.game.entity.Entity;
import net.minecraft.game.entity.player.EntityPlayer;
import net.minecraft.game.item.ItemStack;
import net.minecraft.game.physics.AxisAlignedBB;
import net.minecraft.game.physics.MovingObjectPosition;
import net.minecraft.game.physics.Vec3D;
import net.minecraft.game.world.IWorldAccess;
import net.minecraft.game.world.World;
import net.minecraft.game.world.block.Block;
import org.lwjgl.BufferUtils;
import org.lwjgl.opengl.ARBOcclusionQuery;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.GL15;
import org.lwjgl.opengl.GLContext;
import util.MathHelper;

public final class RenderGlobal implements IWorldAccess {
	private World worldObj;
	private RenderEngine renderEngine;
	private List worlRenderersToUpdate = new ArrayList();
	private WorldRenderer[] sortedWorldRenderers;
	private WorldRenderer[] worldRenderers;
	private int renderChunksWide;
	private int renderChunksTall;
	private int renderChunksDeep;
	private int glRenderListBase;
	private Minecraft mc;
	private RenderBlocks globalRenderBlocks;
	private IntBuffer glOcclusionQueryBase;
	private boolean occlusionEnabled = false;
	private int cloudOffsetX = 0;
	private int glSkyList;
	private int glSkyList2;
	private int minBlockX;
	private int minBlockY;
	private int minBlockZ;
	private int maxBlockX;
	private int maxBlockY;
	private int maxBlockZ;
	private int renderDistance = -1;
	private int countEntitiesTotal;
	private int countEntitiesRendered;
	private int countEntitiesHidden;
	private IntBuffer occlusionResult = BufferUtils.createIntBuffer(64);
	private int renderersLoaded;
	private int renderersBeingClipped;
	private int renderersBeingOccluded;
	private int renderersBeingRendered;
	private List glRenderLists = new ArrayList();
	private double prevSortX = -9999.0D;
	private double prevSortY = -9999.0D;
	private double prevSortZ = -9999.0D;
	public float damagePartialTime;

	public RenderGlobal(Minecraft var1, RenderEngine var2) {
		this.mc = var1;
		this.renderEngine = var2;
		this.glRenderListBase = GL11.glGenLists(786432);
		this.occlusionEnabled = GLContext.getCapabilities().GL_ARB_occlusion_query;
		if(this.occlusionEnabled) {
			this.occlusionResult.clear();
			GL11.glGetInteger(GL15.GL_QUERY_COUNTER_BITS, this.occlusionResult);
			if(this.occlusionResult.get(0) == 0) {
				this.occlusionEnabled = false;
			} else {
				this.glOcclusionQueryBase = BufferUtils.createIntBuffer(262144);
				this.glOcclusionQueryBase.clear();
				this.glOcclusionQueryBase.position(0);
				this.glOcclusionQueryBase.limit(262144);
				ARBOcclusionQuery.glGenQueriesARB(this.glOcclusionQueryBase);
			}
		}

		this.glSkyList = GL11.glGenLists(1);
		GL11.glNewList(this.glSkyList, GL11.GL_COMPILE);
		Random var5 = new Random(10842L);

		Tessellator var3;
		int var7;
		for(var7 = 0; var7 < 500; ++var7) {
			GL11.glRotatef(var5.nextFloat() * 360.0F, 1.0F, 0.0F, 0.0F);
			GL11.glRotatef(var5.nextFloat() * 360.0F, 0.0F, 1.0F, 0.0F);
			GL11.glRotatef(var5.nextFloat() * 360.0F, 0.0F, 0.0F, 1.0F);
			var3 = Tessellator.instance;
			float var4 = 0.25F + var5.nextFloat() * 0.25F;
			var3.startDrawingQuads();
			var3.addVertexWithUV((double)(-var4), -100.0D, (double)var4, 1.0D, 1.0D);
			var3.addVertexWithUV((double)var4, -100.0D, (double)var4, 0.0D, 1.0D);
			var3.addVertexWithUV((double)var4, -100.0D, (double)(-var4), 0.0D, 0.0D);
			var3.addVertexWithUV((double)(-var4), -100.0D, (double)(-var4), 1.0D, 0.0D);
			var3.draw();
		}

		GL11.glEndList();
		this.glSkyList2 = GL11.glGenLists(1);
		GL11.glNewList(this.glSkyList2, GL11.GL_COMPILE);
		var3 = Tessellator.instance;
		var3.startDrawingQuads();

		for(int var6 = -256; var6 <= 256; var6 += 32) {
			for(var7 = -256; var7 <= 256; var7 += 32) {
				var3.addVertex((double)var6, 16.0D, (double)var7);
				var3.addVertex((double)(var6 + 32), 16.0D, (double)var7);
				var3.addVertex((double)(var6 + 32), 16.0D, (double)(var7 + 32));
				var3.addVertex((double)var6, 16.0D, (double)(var7 + 32));
			}
		}

		var3.draw();
		GL11.glEndList();
	}

	public final void changeWorld(World var1) {
		if(this.worldObj != null) {
			this.worldObj.removeWorldAccess(this);
		}

		this.prevSortX = -9999.0D;
		this.prevSortY = -9999.0D;
		this.prevSortZ = -9999.0D;
		RenderManager.instance.set(var1);
		this.worldObj = var1;
		this.globalRenderBlocks = new RenderBlocks(var1);
		if(var1 != null) {
			var1.addWorldAccess(this);
			this.loadRenderers();
		}

	}

	private void loadRenderers() {
		this.renderDistance = this.mc.gameSettings.renderDistance;
		int var1;
		if(this.worldRenderers != null) {
			for(var1 = 0; var1 < this.worldRenderers.length; ++var1) {
				this.worldRenderers[var1].stopRendering();
			}
		}

		var1 = 5 << 3 - this.renderDistance;
		if(var1 > 28) {
			var1 = 28;
		}

		this.renderChunksWide = var1;
		this.renderChunksTall = 8;
		this.renderChunksDeep = var1;
		this.worldRenderers = new WorldRenderer[this.renderChunksWide * this.renderChunksTall * this.renderChunksDeep];
		this.sortedWorldRenderers = new WorldRenderer[this.renderChunksWide * this.renderChunksTall * this.renderChunksDeep];
		var1 = 0;
		int var2 = 0;
		this.minBlockX = 0;
		this.minBlockY = 0;
		this.minBlockZ = 0;
		this.maxBlockX = this.renderChunksWide;
		this.maxBlockY = this.renderChunksTall;
		this.maxBlockZ = this.renderChunksDeep;

		int var3;
		for(var3 = 0; var3 < this.worlRenderersToUpdate.size(); ++var3) {
			((WorldRenderer)this.worlRenderersToUpdate.get(var3)).needsUpdate = false;
		}

		this.worlRenderersToUpdate.clear();

		for(var3 = 0; var3 < this.renderChunksWide; ++var3) {
			for(int var4 = 0; var4 < this.renderChunksTall; ++var4) {
				for(int var5 = 0; var5 < this.renderChunksDeep; ++var5) {
					this.worldRenderers[(var5 * this.renderChunksTall + var4) * this.renderChunksWide + var3] = new WorldRenderer(this.worldObj, var3 << 4, var4 << 4, var5 << 4, 16, this.glRenderListBase + var1);
					if(this.occlusionEnabled) {
						this.worldRenderers[(var5 * this.renderChunksTall + var4) * this.renderChunksWide + var3].glOcclusionQuery = this.glOcclusionQueryBase.get(var2);
					}

					this.worldRenderers[(var5 * this.renderChunksTall + var4) * this.renderChunksWide + var3].isWaitingOnOcclusionQuery = false;
					this.worldRenderers[(var5 * this.renderChunksTall + var4) * this.renderChunksWide + var3].isVisible = true;
					this.worldRenderers[(var5 * this.renderChunksTall + var4) * this.renderChunksWide + var3].isInFrustum = true;
					++var2;
					this.worldRenderers[(var5 * this.renderChunksTall + var4) * this.renderChunksWide + var3].needsUpdate = true;
					this.sortedWorldRenderers[(var5 * this.renderChunksTall + var4) * this.renderChunksWide + var3] = this.worldRenderers[(var5 * this.renderChunksTall + var4) * this.renderChunksWide + var3];
					this.worlRenderersToUpdate.add(this.worldRenderers[(var5 * this.renderChunksTall + var4) * this.renderChunksWide + var3]);
					var1 += 3;
				}
			}
		}

		Entity var6 = this.worldObj.playerEntity;
		this.markRenderersForNewPosition(MathHelper.floor_double(var6.posX), MathHelper.floor_double(var6.posY), MathHelper.floor_double(var6.posZ));
		Arrays.sort(this.sortedWorldRenderers, new EntitySorter(var6));
	}

	public final void renderEntities(Vec3D var1, Frustrum var2, float var3) {
		RenderManager.instance.cacheActiveRenderInfo(this.worldObj, this.renderEngine, this.mc.thePlayer, var3);
		this.countEntitiesTotal = 0;
		this.countEntitiesRendered = 0;
		this.countEntitiesHidden = 0;
		Entity var4 = this.worldObj.playerEntity;
		RenderManager.renderPosX = var4.lastTickPosX + (var4.posX - var4.lastTickPosX) * (double)var3;
		RenderManager.renderPosY = var4.lastTickPosY + (var4.posY - var4.lastTickPosY) * (double)var3;
		RenderManager.renderPosZ = var4.lastTickPosZ + (var4.posZ - var4.lastTickPosZ) * (double)var3;
		List var30 = this.worldObj.getLoadedEntityList();
		this.countEntitiesTotal = var30.size();

		for(int var5 = 0; var5 < var30.size(); ++var5) {
			Entity var6 = (Entity)var30.get(var5);
			double var10 = var6.posX - var1.xCoord;
			double var12 = var6.posY - var1.yCoord;
			double var14 = var6.posZ - var1.zCoord;
			double var16 = var10 * var10 + var12 * var12 + var14 * var14;
			AxisAlignedBB var7 = var6.boundingBox;
			double var24 = var7.maxX - var7.minX;
			double var26 = var7.maxY - var7.minY;
			double var28 = var7.maxZ - var7.minZ;
			double var21 = (var24 + var26 + var28) / 3.0D;
			var21 *= 64.0D;
			if(var16 < var21 * var21 && var2.isBoundingBoxInFrustrum(var6.boundingBox) && (var6 != this.worldObj.playerEntity || this.mc.gameSettings.thirdPersonView)) {
				++this.countEntitiesRendered;
				RenderManager.instance.renderEntity(var6, var3);
			}
		}

	}

	public final String getDebugInfoRenders() {
		return "C: " + this.renderersBeingRendered + "/" + this.renderersLoaded + ". F: " + this.renderersBeingClipped + ", O: " + this.renderersBeingOccluded;
	}

	public final String getDebugInfoEntities() {
		return "E: " + this.countEntitiesRendered + "/" + this.countEntitiesTotal + ". B: " + 0 + ", I: " + (this.countEntitiesTotal - this.countEntitiesRendered);
	}

	private void markRenderersForNewPosition(int var1, int var2, int var3) {
		var1 -= 8;
		var3 -= 8;
		this.minBlockX = Integer.MAX_VALUE;
		this.minBlockY = Integer.MAX_VALUE;
		this.minBlockZ = Integer.MAX_VALUE;
		this.maxBlockX = Integer.MIN_VALUE;
		this.maxBlockY = Integer.MIN_VALUE;
		this.maxBlockZ = Integer.MIN_VALUE;
		var2 = this.renderChunksWide << 4;
		int var4 = var2 / 2;

		for(int var5 = 0; var5 < this.renderChunksWide; ++var5) {
			int var6 = var5 << 4;
			int var7 = var6 + var4 - var1;
			if(var7 < 0) {
				var7 -= var2 - 1;
			}

			var7 /= var2;
			var6 -= var7 * var2;
			if(var6 < this.minBlockX) {
				this.minBlockX = var6;
			}

			if(var6 > this.maxBlockX) {
				this.maxBlockX = var6;
			}

			for(var7 = 0; var7 < this.renderChunksDeep; ++var7) {
				int var8 = var7 << 4;
				int var9 = var8 + var4 - var3;
				if(var9 < 0) {
					var9 -= var2 - 1;
				}

				var9 /= var2;
				var8 -= var9 * var2;
				if(var8 < this.minBlockZ) {
					this.minBlockZ = var8;
				}

				if(var8 > this.maxBlockZ) {
					this.maxBlockZ = var8;
				}

				for(var9 = 0; var9 < this.renderChunksTall; ++var9) {
					int var10 = var9 << 4;
					if(var10 < this.minBlockY) {
						this.minBlockY = var10;
					}

					if(var10 > this.maxBlockY) {
						this.maxBlockY = var10;
					}

					WorldRenderer var11 = this.worldRenderers[(var7 * this.renderChunksTall + var9) * this.renderChunksWide + var5];
					boolean var12 = var11.needsUpdate;
					var11.setPosition(var6, var10, var8);
					if(!var12 && var11.needsUpdate) {
						this.worlRenderersToUpdate.add(var11);
					}
				}
			}
		}

	}

	public final int sortAndRender(EntityPlayer var1, int var2, double var3) {
		if(this.mc.gameSettings.renderDistance != this.renderDistance) {
			this.loadRenderers();
		}

		if(var2 == 0) {
			this.renderersLoaded = 0;
			this.renderersBeingClipped = 0;
			this.renderersBeingOccluded = 0;
			this.renderersBeingRendered = 0;
		}

		double var5 = var1.lastTickPosX + (var1.posX - var1.lastTickPosX) * var3;
		double var7 = var1.lastTickPosY + (var1.posY - var1.lastTickPosY) * var3;
		double var9 = var1.lastTickPosZ + (var1.posZ - var1.lastTickPosZ) * var3;
		double var11 = var1.posX - this.prevSortX;
		double var13 = var1.posY - this.prevSortY;
		double var15 = var1.posZ - this.prevSortZ;
		if(var11 * var11 + var13 * var13 + var15 * var15 > 16.0D) {
			this.prevSortX = var1.posX;
			this.prevSortY = var1.posY;
			this.prevSortZ = var1.posZ;
			this.markRenderersForNewPosition(MathHelper.floor_double(var1.posX), MathHelper.floor_double(var1.posY), MathHelper.floor_double(var1.posZ));
			Arrays.sort(this.sortedWorldRenderers, new EntitySorter(var1));
		}

		int var21;
		if(this.occlusionEnabled && !this.mc.gameSettings.anaglyph && var2 == 0) {
			int var22 = 16;
			this.checkOcclusionQueryResult(0, 16);

			for(int var14 = 0; var14 < 16; ++var14) {
				this.sortedWorldRenderers[var14].isVisible = true;
			}

			var21 = 0 + this.renderSortedRenderers(0, 16, var2, var3);

			do {
				int var12 = var22;
				var22 <<= 1;
				if(var22 > this.sortedWorldRenderers.length) {
					var22 = this.sortedWorldRenderers.length;
				}

				GL11.glDisable(GL11.GL_TEXTURE_2D);
				GL11.glDisable(GL11.GL_LIGHTING);
				GL11.glDisable(GL11.GL_ALPHA_TEST);
				GL11.glDisable(GL11.GL_FOG);
				GL11.glColorMask(false, false, false, false);
				GL11.glDepthMask(false);
				this.checkOcclusionQueryResult(var12, var22);
				GL11.glPushMatrix();
				float var23 = 0.0F;
				float var24 = 0.0F;
				float var16 = 0.0F;

				for(int var17 = var12; var17 < var22; ++var17) {
					if(this.sortedWorldRenderers[var17].skipAllRenderPasses()) {
						this.sortedWorldRenderers[var17].isInFrustum = false;
					} else {
						if(!this.sortedWorldRenderers[var17].isInFrustum) {
							this.sortedWorldRenderers[var17].isVisible = true;
						}

						if(this.sortedWorldRenderers[var17].isInFrustum && !this.sortedWorldRenderers[var17].isWaitingOnOcclusionQuery) {
							float var18 = MathHelper.sqrt_float(this.sortedWorldRenderers[var17].distanceToEntitySquared(var1));
							int var25 = (int)(1.0F + var18 / 64.0F);
							if(this.cloudOffsetX % var25 == var17 % var25) {
								WorldRenderer var26 = this.sortedWorldRenderers[var17];
								float var19 = (float)((double)var26.posXMinus - var5);
								float var20 = (float)((double)var26.posYMinus - var7);
								var18 = (float)((double)var26.posZMinus - var9);
								var19 -= var23;
								var20 -= var24;
								var18 -= var16;
								if(var19 != 0.0F || var20 != 0.0F || var18 != 0.0F) {
									GL11.glTranslatef(var19, var20, var18);
									var23 += var19;
									var24 += var20;
									var16 += var18;
								}

								ARBOcclusionQuery.glBeginQueryARB(GL15.GL_SAMPLES_PASSED, this.sortedWorldRenderers[var17].glOcclusionQuery);
								this.sortedWorldRenderers[var17].callOcclusionQueryList();
								ARBOcclusionQuery.glEndQueryARB(GL15.GL_SAMPLES_PASSED);
								this.sortedWorldRenderers[var17].isWaitingOnOcclusionQuery = true;
							}
						}
					}
				}

				GL11.glPopMatrix();
				GL11.glColorMask(true, true, true, true);
				GL11.glDepthMask(true);
				GL11.glEnable(GL11.GL_TEXTURE_2D);
				GL11.glEnable(GL11.GL_ALPHA_TEST);
				GL11.glEnable(GL11.GL_FOG);
				var21 += this.renderSortedRenderers(var12, var22, var2, var3);
			} while(var22 < this.sortedWorldRenderers.length);
		} else {
			var21 = 0 + this.renderSortedRenderers(0, this.sortedWorldRenderers.length, var2, var3);
		}

		return var21;
	}

	private void checkOcclusionQueryResult(int var1, int var2) {
		for(var1 = var1; var1 < var2; ++var1) {
			if(this.sortedWorldRenderers[var1].isWaitingOnOcclusionQuery) {
				this.occlusionResult.clear();
				ARBOcclusionQuery.glGetQueryObjectuARB(this.sortedWorldRenderers[var1].glOcclusionQuery, GL15.GL_QUERY_RESULT_AVAILABLE, this.occlusionResult);
				if(this.occlusionResult.get(0) != 0) {
					this.sortedWorldRenderers[var1].isWaitingOnOcclusionQuery = false;
					this.occlusionResult.clear();
					ARBOcclusionQuery.glGetQueryObjectuARB(this.sortedWorldRenderers[var1].glOcclusionQuery, GL15.GL_QUERY_RESULT, this.occlusionResult);
					this.sortedWorldRenderers[var1].isVisible = this.occlusionResult.get(0) != 0;
				}
			}
		}

	}

	private int renderSortedRenderers(int var1, int var2, int var3, double var4) {
		this.glRenderLists.clear();
		int var6 = 0;

		for(var1 = var1; var1 < var2; ++var1) {
			if(var3 == 0) {
				++this.renderersLoaded;
				if(!this.sortedWorldRenderers[var1].isInFrustum) {
					++this.renderersBeingClipped;
				}

				if(this.sortedWorldRenderers[var1].isInFrustum && !this.sortedWorldRenderers[var1].isVisible) {
					++this.renderersBeingOccluded;
				}

				if(this.sortedWorldRenderers[var1].isInFrustum && this.sortedWorldRenderers[var1].isVisible) {
					++this.renderersBeingRendered;
				}
			}

			if(this.sortedWorldRenderers[var1].isInFrustum && this.sortedWorldRenderers[var1].isVisible) {
				int var7 = this.sortedWorldRenderers[var1].getGLCallListForPass(var3);
				if(var7 >= 0) {
					this.glRenderLists.add(this.sortedWorldRenderers[var1]);
					++var6;
				}
			}
		}

		this.renderAllRenderLists(var3, var4);
		return var6;
	}

	public final void renderAllRenderLists(int var1, double var2) {
		EntityPlayerSP var4 = this.mc.thePlayer;
		double var5 = var4.lastTickPosX + (var4.posX - var4.lastTickPosX) * var2;
		double var7 = var4.lastTickPosY + (var4.posY - var4.lastTickPosY) * var2;
		double var9 = var4.lastTickPosZ + (var4.posZ - var4.lastTickPosZ) * var2;
		GL11.glPushMatrix();
		float var16 = 0.0F;
		float var3 = 0.0F;
		float var17 = 0.0F;

		for(int var11 = 0; var11 < this.glRenderLists.size(); ++var11) {
			WorldRenderer var12 = (WorldRenderer)this.glRenderLists.get(var11);
			float var13 = (float)((double)var12.posXMinus - var5);
			float var14 = (float)((double)var12.posYMinus - var7);
			float var15 = (float)((double)var12.posZMinus - var9);
			var13 -= var16;
			var14 -= var3;
			var15 -= var17;
			if(var13 != 0.0F || var14 != 0.0F || var15 != 0.0F) {
				GL11.glTranslatef(var13, var14, var15);
				var16 += var13;
				var3 += var14;
				var17 += var15;
			}

			GL11.glCallList(var12.getGLCallListForPass(var1));
		}

		GL11.glPopMatrix();
	}

	public final void updateClouds() {
		++this.cloudOffsetX;
	}

	public final void renderSky(float var1) {
		GL11.glDisable(GL11.GL_TEXTURE_2D);
		float var2 = (float)(this.mc.thePlayer.lastTickPosY + (this.mc.thePlayer.posY - this.mc.thePlayer.lastTickPosY) * (double)var1);
		Vec3D var3 = this.worldObj.getSkyColor(var1);
		float var4 = (float)var3.xCoord;
		float var5 = (float)var3.yCoord;
		float var20 = (float)var3.zCoord;
		float var6;
		if(this.mc.gameSettings.anaglyph) {
			var6 = (var4 * 30.0F + var5 * 59.0F + var20 * 11.0F) / 100.0F;
			var5 = (var4 * 30.0F + var5 * 70.0F) / 100.0F;
			var20 = (var4 * 30.0F + var20 * 70.0F) / 100.0F;
			var4 = var6;
			var5 = var5;
			var20 = var20;
		}

		GL11.glColor3f(var4, var5, var20);
		Tessellator var21 = Tessellator.instance;
		GL11.glDepthMask(false);
		GL11.glCallList(this.glSkyList2);
		GL11.glEnable(GL11.GL_TEXTURE_2D);
		GL11.glDisable(GL11.GL_FOG);
		GL11.glDisable(GL11.GL_ALPHA_TEST);
		GL11.glEnable(GL11.GL_BLEND);
		GL11.glBlendFunc(GL11.GL_ONE, GL11.GL_ONE);
		GL11.glPushMatrix();
		GL11.glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
		GL11.glTranslatef(0.0F, 0.0F, 0.0F);
		GL11.glRotatef(0.0F, 0.0F, 0.0F, 1.0F);
		GL11.glRotatef(this.worldObj.getCelestialAngle(var1) * 360.0F, 1.0F, 0.0F, 0.0F);
		GL11.glBindTexture(GL11.GL_TEXTURE_2D, this.renderEngine.getTexture("/terrain/sun.png"));
		var21.startDrawingQuads();
		var21.addVertexWithUV(-30.0D, 100.0D, -30.0D, 0.0D, 0.0D);
		var21.addVertexWithUV(30.0D, 100.0D, -30.0D, 1.0D, 0.0D);
		var21.addVertexWithUV(30.0D, 100.0D, 30.0D, 1.0D, 1.0D);
		var21.addVertexWithUV(-30.0D, 100.0D, 30.0D, 0.0D, 1.0D);
		var21.draw();
		GL11.glBindTexture(GL11.GL_TEXTURE_2D, this.renderEngine.getTexture("/terrain/moon.png"));
		var21.startDrawingQuads();
		var21.addVertexWithUV(-20.0D, -100.0D, 20.0D, 1.0D, 1.0D);
		var21.addVertexWithUV(20.0D, -100.0D, 20.0D, 0.0D, 1.0D);
		var21.addVertexWithUV(20.0D, -100.0D, -20.0D, 0.0D, 0.0D);
		var21.addVertexWithUV(-20.0D, -100.0D, -20.0D, 1.0D, 0.0D);
		var21.draw();
		GL11.glDisable(GL11.GL_TEXTURE_2D);
		float var7 = this.worldObj.getStarBrightness(var1);
		GL11.glColor4f(var7, var7, var7, var7);
		GL11.glCallList(this.glSkyList);
		GL11.glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
		GL11.glEnable(GL11.GL_TEXTURE_2D);
		GL11.glDisable(GL11.GL_BLEND);
		GL11.glEnable(GL11.GL_ALPHA_TEST);
		GL11.glEnable(GL11.GL_FOG);
		GL11.glPopMatrix();
		GL11.glDepthMask(true);
		GL11.glBindTexture(GL11.GL_TEXTURE_2D, this.renderEngine.getTexture("/clouds.png"));
		GL11.glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
		Vec3D var22 = this.worldObj.getCloudColor(var1);
		var5 = (float)var22.xCoord;
		var6 = (float)var22.yCoord;
		var4 = (float)var22.zCoord;
		if(this.mc.gameSettings.anaglyph) {
			var7 = (var5 * 30.0F + var6 * 59.0F + var4 * 11.0F) / 100.0F;
			float var15 = (var5 * 30.0F + var6 * 70.0F) / 100.0F;
			float var16 = (var5 * 30.0F + var4 * 70.0F) / 100.0F;
			var5 = var7;
			var6 = var15;
			var4 = var16;
		}

		double var26 = this.worldObj.playerEntity.prevPosX + (this.worldObj.playerEntity.posX - this.worldObj.playerEntity.prevPosX) * (double)var1 + (double)(((float)this.cloudOffsetX + var1) * 0.03F);
		double var17 = this.worldObj.playerEntity.prevPosZ + (this.worldObj.playerEntity.posZ - this.worldObj.playerEntity.prevPosZ) * (double)var1;
		int var19 = MathHelper.floor_double(var26 / 2048.0D);
		int var25 = MathHelper.floor_double(var17 / 2048.0D);
		var26 -= (double)(var19 << 11);
		var17 -= (double)(var25 << 11);
		var1 = 120.0F - var2 + 0.33F;
		var2 = (float)(var26 * 4.8828125E-4D);
		var7 = (float)(var17 * 4.8828125E-4D);
		var21.startDrawingQuads();
		var21.setColorOpaque_F(var5, var6, var4);

		for(int var23 = -256; var23 < 256; var23 += 32) {
			for(int var24 = -256; var24 < 256; var24 += 32) {
				var21.addVertexWithUV((double)var23, (double)var1, (double)(var24 + 32), (double)((float)var23 * (0.5F / 1024.0F) + var2), (double)((float)(var24 + 32) * (0.5F / 1024.0F) + var7));
				var21.addVertexWithUV((double)(var23 + 32), (double)var1, (double)(var24 + 32), (double)((float)(var23 + 32) * (0.5F / 1024.0F) + var2), (double)((float)(var24 + 32) * (0.5F / 1024.0F) + var7));
				var21.addVertexWithUV((double)(var23 + 32), (double)var1, (double)var24, (double)((float)(var23 + 32) * (0.5F / 1024.0F) + var2), (double)((float)var24 * (0.5F / 1024.0F) + var7));
				var21.addVertexWithUV((double)var23, (double)var1, (double)var24, (double)((float)var23 * (0.5F / 1024.0F) + var2), (double)((float)var24 * (0.5F / 1024.0F) + var7));
				var21.addVertexWithUV((double)var23, (double)var1, (double)var24, (double)((float)var23 * (0.5F / 1024.0F) + var2), (double)((float)var24 * (0.5F / 1024.0F) + var7));
				var21.addVertexWithUV((double)(var23 + 32), (double)var1, (double)var24, (double)((float)(var23 + 32) * (0.5F / 1024.0F) + var2), (double)((float)var24 * (0.5F / 1024.0F) + var7));
				var21.addVertexWithUV((double)(var23 + 32), (double)var1, (double)(var24 + 32), (double)((float)(var23 + 32) * (0.5F / 1024.0F) + var2), (double)((float)(var24 + 32) * (0.5F / 1024.0F) + var7));
				var21.addVertexWithUV((double)var23, (double)var1, (double)(var24 + 32), (double)((float)var23 * (0.5F / 1024.0F) + var2), (double)((float)(var24 + 32) * (0.5F / 1024.0F) + var7));
			}
		}

		var21.draw();
	}

	public final void updateRenderers(EntityPlayer var1) {
		Collections.sort(this.worlRenderersToUpdate, new RenderSorter(var1));
		int var2 = this.worlRenderersToUpdate.size() - 1;
		int var3 = this.worlRenderersToUpdate.size();

		for(int var4 = 0; var4 < var3; ++var4) {
			WorldRenderer var5 = (WorldRenderer)this.worlRenderersToUpdate.get(var2 - var4);
			if(var5.distanceToEntitySquared(var1) > 2500.0F && var4 > 2) {
				return;
			}

			this.worlRenderersToUpdate.remove(var5);
			var5.updateRenderer();
			var5.needsUpdate = false;
		}

	}

	public final void drawBlockBreaking(EntityPlayer var1, MovingObjectPosition var2, int var3, ItemStack var4, float var5) {
		Tessellator var16 = Tessellator.instance;
		GL11.glEnable(GL11.GL_BLEND);
		GL11.glEnable(GL11.GL_ALPHA_TEST);
		GL11.glBlendFunc(GL11.GL_SRC_ALPHA, GL11.GL_ONE);
		GL11.glColor4f(1.0F, 1.0F, 1.0F, (MathHelper.sin((float)System.currentTimeMillis() / 100.0F) * 0.2F + 0.4F) * 0.5F);
		if(this.damagePartialTime > 0.0F) {
			GL11.glBlendFunc(GL11.GL_DST_COLOR, GL11.GL_SRC_COLOR);
			int var17 = this.renderEngine.getTexture("/terrain.png");
			GL11.glBindTexture(GL11.GL_TEXTURE_2D, var17);
			GL11.glColor4f(1.0F, 1.0F, 1.0F, 0.5F);
			GL11.glPushMatrix();
			var17 = this.worldObj.getBlockId(var2.blockX, var2.blockY, var2.blockZ);
			Block var18 = var17 > 0 ? Block.blocksList[var17] : null;
			GL11.glDisable(GL11.GL_ALPHA_TEST);
			GL11.glPolygonOffset(-1.0F, -1.0F);
			GL11.glEnable(GL11.GL_POLYGON_OFFSET_FILL);
			var16.startDrawingQuads();
			double var10 = var1.lastTickPosX + (var1.posX - var1.lastTickPosX) * (double)var5;
			double var12 = var1.lastTickPosY + (var1.posY - var1.lastTickPosY) * (double)var5;
			double var14 = var1.lastTickPosZ + (var1.posZ - var1.lastTickPosZ) * (double)var5;
			var16.setTranslationD(-var10, -var12, -var14);
			var16.disableColor();
			if(var18 == null) {
				var18 = Block.stone;
			}

			this.globalRenderBlocks.renderBlockUsingTexture(var18, var2.blockX, var2.blockY, var2.blockZ, 240 + (int)(this.damagePartialTime * 10.0F));
			var16.draw();
			var16.setTranslationD(0.0D, 0.0D, 0.0D);
			GL11.glPolygonOffset(0.0F, 0.0F);
			GL11.glDisable(GL11.GL_POLYGON_OFFSET_FILL);
			GL11.glEnable(GL11.GL_ALPHA_TEST);
			GL11.glDepthMask(true);
			GL11.glPopMatrix();
		}

		GL11.glDisable(GL11.GL_BLEND);
		GL11.glDisable(GL11.GL_ALPHA_TEST);
	}

	public final void drawSelectionBox(EntityPlayer var1, MovingObjectPosition var2, int var3, float var4) {
		GL11.glEnable(GL11.GL_BLEND);
		GL11.glBlendFunc(GL11.GL_SRC_ALPHA, GL11.GL_ONE_MINUS_SRC_ALPHA);
		GL11.glColor4f(0.0F, 0.0F, 0.0F, 0.4F);
		GL11.glLineWidth(2.0F);
		GL11.glDisable(GL11.GL_TEXTURE_2D);
		GL11.glDepthMask(false);
		var3 = this.worldObj.getBlockId(var2.blockX, var2.blockY, var2.blockZ);
		if(var3 > 0) {
			double var6 = var1.lastTickPosX + (var1.posX - var1.lastTickPosX) * (double)var4;
			double var8 = var1.lastTickPosY + (var1.posY - var1.lastTickPosY) * (double)var4;
			double var10 = var1.lastTickPosZ + (var1.posZ - var1.lastTickPosZ) * (double)var4;
			AxisAlignedBB var12 = Block.blocksList[var3].getSelectedBoundingBoxFromPool(var2.blockX, var2.blockY, var2.blockZ).expand((double)0.002F, (double)0.002F, (double)0.002F).offsetCopy(-var6, -var8, -var10);
			Tessellator var13 = Tessellator.instance;
			var13.startDrawing(3);
			var13.addVertex(var12.minX, var12.minY, var12.minZ);
			var13.addVertex(var12.maxX, var12.minY, var12.minZ);
			var13.addVertex(var12.maxX, var12.minY, var12.maxZ);
			var13.addVertex(var12.minX, var12.minY, var12.maxZ);
			var13.addVertex(var12.minX, var12.minY, var12.minZ);
			var13.draw();
			var13.startDrawing(3);
			var13.addVertex(var12.minX, var12.maxY, var12.minZ);
			var13.addVertex(var12.maxX, var12.maxY, var12.minZ);
			var13.addVertex(var12.maxX, var12.maxY, var12.maxZ);
			var13.addVertex(var12.minX, var12.maxY, var12.maxZ);
			var13.addVertex(var12.minX, var12.maxY, var12.minZ);
			var13.draw();
			var13.startDrawing(1);
			var13.addVertex(var12.minX, var12.minY, var12.minZ);
			var13.addVertex(var12.minX, var12.maxY, var12.minZ);
			var13.addVertex(var12.maxX, var12.minY, var12.minZ);
			var13.addVertex(var12.maxX, var12.maxY, var12.minZ);
			var13.addVertex(var12.maxX, var12.minY, var12.maxZ);
			var13.addVertex(var12.maxX, var12.maxY, var12.maxZ);
			var13.addVertex(var12.minX, var12.minY, var12.maxZ);
			var13.addVertex(var12.minX, var12.maxY, var12.maxZ);
			var13.draw();
		}

		GL11.glDepthMask(true);
		GL11.glEnable(GL11.GL_TEXTURE_2D);
		GL11.glDisable(GL11.GL_BLEND);
	}

	private void markBlocksForUpdate(int var1, int var2, int var3, int var4, int var5, int var6) {
		var1 >>= 4;
		var2 >>= 4;
		var3 >>= 4;
		var4 >>= 4;
		var5 >>= 4;
		var6 >>= 4;

		for(var1 = var1; var1 <= var4; ++var1) {
			int var7 = var1 % this.renderChunksWide;
			if(var7 < 0) {
				var7 += this.renderChunksWide;
			}

			for(int var8 = var2; var8 <= var5; ++var8) {
				int var9 = var8 % this.renderChunksTall;
				if(var9 < 0) {
					var9 += this.renderChunksTall;
				}

				for(int var10 = var3; var10 <= var6; ++var10) {
					int var11 = var10 % this.renderChunksDeep;
					if(var11 < 0) {
						var11 += this.renderChunksDeep;
					}

					var11 = (var11 * this.renderChunksTall + var9) * this.renderChunksWide + var7;
					WorldRenderer var12 = this.worldRenderers[var11];
					if(!var12.needsUpdate) {
						var12.needsUpdate = true;
						this.worlRenderersToUpdate.add(var12);
					}
				}
			}
		}

	}

	public final void markBlockAndNeighborsNeedsUpdate(int var1, int var2, int var3) {
		this.markBlocksForUpdate(var1 - 1, var2 - 1, var3 - 1, var1 + 1, var2 + 1, var3 + 1);
	}

	public final void markBlockRangeNeedsUpdate(int var1, int var2, int var3, int var4, int var5, int var6) {
		this.markBlocksForUpdate(var1 - 1, var2 - 1, var3 - 1, var4 + 1, var5 + 1, var6 + 1);
	}

	public final void clipRenderersByFrustrum(Frustrum var1) {
		for(int var2 = 0; var2 < this.worldRenderers.length; ++var2) {
			this.worldRenderers[var2].updateInFrustrum(var1);
		}

	}

	public final void playSound(String var1, double var2, double var4, double var6, float var8, float var9) {
		this.mc.sndManager.playSound(var1, (float)var2, (float)var4, (float)var6, var8, var9);
	}

	public final void spawnParticle(String var1, double var2, double var4, double var6, double var8, double var10, double var12) {
		double var14 = this.worldObj.playerEntity.posX - var2;
		double var16 = this.worldObj.playerEntity.posY - var4;
		double var18 = this.worldObj.playerEntity.posZ - var6;
		if(var14 * var14 + var16 * var16 + var18 * var18 <= 256.0D) {
			if(var1 == "bubble") {
				this.mc.effectRenderer.addEffect(new EntityBubbleFX(this.worldObj, var2, var4, var6, var8, var10, var12));
			} else if(var1 == "smoke") {
				this.mc.effectRenderer.addEffect(new EntitySmokeFX(this.worldObj, var2, var4, var6));
			} else if(var1 == "explode") {
				this.mc.effectRenderer.addEffect(new EntityExplodeFX(this.worldObj, var2, var4, var6, var8, var10, var12));
			} else if(var1 == "flame") {
				this.mc.effectRenderer.addEffect(new EntityFlameFX(this.worldObj, var2, var4, var6));
			} else if(var1 == "lava") {
				this.mc.effectRenderer.addEffect(new EntityLavaFX(this.worldObj, var2, var4, var6));
			} else if(var1 == "splash") {
				this.mc.effectRenderer.addEffect(new EntitySplashFX(this.worldObj, var2, var4, var6));
			} else {
				if(var1 == "largesmoke") {
					this.mc.effectRenderer.addEffect(new EntitySmokeFX(this.worldObj, var2, var4, var6, 2.5F));
				}

			}
		}
	}

	public final void updateAllRenderers() {
		for(int var1 = 0; var1 < this.worldRenderers.length; ++var1) {
			if(!this.worldRenderers[var1].needsUpdate && this.worldRenderers[var1].isChunkLit) {
				this.worldRenderers[var1].needsUpdate = true;
				this.worlRenderersToUpdate.add(this.worldRenderers[var1]);
			}
		}

	}
}
