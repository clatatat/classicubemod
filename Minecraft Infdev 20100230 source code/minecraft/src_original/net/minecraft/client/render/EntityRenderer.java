package net.minecraft.client.render;

import java.nio.FloatBuffer;
import java.util.List;
import java.util.Random;
import net.minecraft.client.Minecraft;
import net.minecraft.client.RenderHelper;
import net.minecraft.client.controller.PlayerControllerCreative;
import net.minecraft.client.effect.EffectRenderer;
import net.minecraft.client.gui.ScaledResolution;
import net.minecraft.client.player.EntityPlayerSP;
import net.minecraft.client.render.camera.ClippingHelperImplementation;
import net.minecraft.client.render.camera.Frustrum;
import net.minecraft.game.entity.Entity;
import net.minecraft.game.physics.AxisAlignedBB;
import net.minecraft.game.physics.MovingObjectPosition;
import net.minecraft.game.physics.Vec3D;
import net.minecraft.game.world.World;
import net.minecraft.game.world.block.Block;
import net.minecraft.game.world.material.Material;
import org.lwjgl.BufferUtils;
import org.lwjgl.input.Mouse;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.GLContext;
import org.lwjgl.opengl.NVFogDistance;
import org.lwjgl.util.glu.GLU;
import util.MathHelper;

public final class EntityRenderer {
	private Minecraft mc;
	private boolean anaglyphEnabled = false;
	private float farPlaneDistance = 0.0F;
	public ItemRenderer itemRenderer;
	private int rendererUpdateCount;
	private Entity pointedEntity = null;
	private int entityRendererInt1;
	private int entityRendererInt2;
	private Random random = new Random();
	private volatile int unusedVolatile1 = 0;
	private volatile int unusedVolatile2 = 0;
	private FloatBuffer fogColorBuffer = BufferUtils.createFloatBuffer(16);
	private float fogColorRed;
	private float fogColorGreen;
	private float fogColorBlue;
	private float fogColor2;
	private float fogColor1;

	public EntityRenderer(Minecraft var1) {
		this.mc = var1;
		this.itemRenderer = new ItemRenderer(var1);
	}

	public final void updateRenderer() {
		this.fogColor2 = this.fogColor1;
		float var1 = this.mc.theWorld.getBrightness(MathHelper.floor_double(this.mc.thePlayer.posX), MathHelper.floor_double(this.mc.thePlayer.posY), MathHelper.floor_double(this.mc.thePlayer.posZ));
		float var2 = (float)(3 - this.mc.gameSettings.renderDistance) / 3.0F;
		var1 = var1 * (1.0F - var2) + var2;
		this.fogColor1 += (var1 - this.fogColor1) * 0.1F;
		++this.rendererUpdateCount;
		this.itemRenderer.updateEquippedItem();
	}

	private Vec3D orientCamera(float var1) {
		EntityPlayerSP var2 = this.mc.thePlayer;
		double var3 = var2.prevPosX + (var2.posX - var2.prevPosX) * (double)var1;
		double var5 = var2.prevPosY + (var2.posY - var2.prevPosY) * (double)var1;
		double var7 = var2.prevPosZ + (var2.posZ - var2.prevPosZ) * (double)var1;
		return new Vec3D(var3, var5, var7);
	}

	private void hurtCameraEffect(float var1) {
		EntityPlayerSP var2 = this.mc.thePlayer;
		float var3 = (float)var2.hurtTime - var1;
		if(var2.health <= 0) {
			var1 += (float)var2.deathTime;
			GL11.glRotatef(40.0F - 8000.0F / (var1 + 200.0F), 0.0F, 0.0F, 1.0F);
		}

		if(var3 >= 0.0F) {
			var3 /= (float)var2.maxHurtTime;
			var3 = MathHelper.sin(var3 * var3 * var3 * var3 * (float)Math.PI);
			var1 = var2.attackedAtYaw;
			GL11.glRotatef(-var1, 0.0F, 1.0F, 0.0F);
			GL11.glRotatef(-var3 * 14.0F, 0.0F, 0.0F, 1.0F);
			GL11.glRotatef(var1, 0.0F, 1.0F, 0.0F);
		}
	}

	private void setupViewBobbing(float var1) {
		if(!this.mc.gameSettings.thirdPersonView) {
			EntityPlayerSP var2 = this.mc.thePlayer;
			float var3 = var2.distanceWalkedModified - var2.prevDistanceWalkedModified;
			var3 = var2.distanceWalkedModified + var3 * var1;
			float var4 = var2.prevCameraYaw + (var2.cameraYaw - var2.prevCameraYaw) * var1;
			var1 = var2.prevCameraPitch + (var2.cameraPitch - var2.prevCameraPitch) * var1;
			GL11.glTranslatef(MathHelper.sin(var3 * (float)Math.PI) * var4 * 0.5F, -Math.abs(MathHelper.cos(var3 * (float)Math.PI) * var4), 0.0F);
			GL11.glRotatef(MathHelper.sin(var3 * (float)Math.PI) * var4 * 3.0F, 0.0F, 0.0F, 1.0F);
			GL11.glRotatef(Math.abs(MathHelper.cos(var3 * (float)Math.PI + 0.2F) * var4) * 5.0F, 1.0F, 0.0F, 0.0F);
			GL11.glRotatef(var1, 1.0F, 0.0F, 0.0F);
		}
	}

	public final void updateCameraAndRender(float var1) {
		if(this.anaglyphEnabled && !Display.isActive()) {
			this.mc.displayInGameMenu();
		}

		this.anaglyphEnabled = Display.isActive();
		int var5;
		int var6;
		if(this.mc.inventoryScreen) {
			Mouse.getDX();
			byte var2 = 0;
			Mouse.getDY();
			byte var3 = 0;
			this.mc.mouseHelper.ungrabMouseCursor();
			byte var4 = 1;
			if(this.mc.gameSettings.invertMouse) {
				var4 = -1;
			}

			var5 = var2 + this.mc.mouseHelper.deltaX;
			var6 = var3 - this.mc.mouseHelper.deltaY;
			if(var2 != 0 || this.entityRendererInt1 != 0) {
				System.out.println("xxo: " + var2 + ", " + this.entityRendererInt1 + ": " + this.entityRendererInt1 + ", xo: " + var5);
			}

			if(this.entityRendererInt1 != 0) {
				this.entityRendererInt1 = 0;
			}

			if(this.entityRendererInt2 != 0) {
				this.entityRendererInt2 = 0;
			}

			if(var2 != 0) {
				this.entityRendererInt1 = var2;
			}

			if(var3 != 0) {
				this.entityRendererInt2 = var3;
			}

			float var10001 = (float)var5;
			float var11 = (float)(var6 * var4);
			float var9 = var10001;
			EntityPlayerSP var7 = this.mc.thePlayer;
			float var13 = var7.rotationPitch;
			float var14 = var7.rotationYaw;
			var7.rotationYaw = (float)((double)var7.rotationYaw + (double)var9 * 0.15D);
			var7.rotationPitch = (float)((double)var7.rotationPitch - (double)var11 * 0.15D);
			if(var7.rotationPitch < -90.0F) {
				var7.rotationPitch = -90.0F;
			}

			if(var7.rotationPitch > 90.0F) {
				var7.rotationPitch = 90.0F;
			}

			var7.prevRotationPitch += var7.rotationPitch - var13;
			var7.prevRotationYaw += var7.rotationYaw - var14;
		}

		ScaledResolution var8 = new ScaledResolution(this.mc.displayWidth, this.mc.displayHeight);
		int var10 = var8.getScaledWidth();
		int var12 = var8.getScaledHeight();
		var5 = Mouse.getX() * var10 / this.mc.displayWidth;
		var6 = var12 - Mouse.getY() * var12 / this.mc.displayHeight - 1;
		if(this.mc.theWorld != null) {
			this.getMouseOver(var1);
			this.mc.ingameGUI.renderGameOverlay(var1);
		} else {
			GL11.glViewport(0, 0, this.mc.displayWidth, this.mc.displayHeight);
			GL11.glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
			GL11.glClear(GL11.GL_DEPTH_BUFFER_BIT | GL11.GL_COLOR_BUFFER_BIT);
			GL11.glMatrixMode(GL11.GL_PROJECTION);
			GL11.glLoadIdentity();
			GL11.glMatrixMode(GL11.GL_MODELVIEW);
			GL11.glLoadIdentity();
			this.setupOverlayRendering();
		}

		if(this.mc.currentScreen != null) {
			GL11.glClear(GL11.GL_DEPTH_BUFFER_BIT);
			this.mc.currentScreen.drawScreen(var5, var6, var1);
		}

		Thread.yield();
		Display.update();
	}

	private void getMouseOver(float var1) {
		EntityRenderer var13 = this;
		EntityPlayerSP var15 = this.mc.thePlayer;
		float var16 = var15.prevRotationPitch + (var15.rotationPitch - var15.prevRotationPitch) * var1;
		float var17 = var15.prevRotationYaw + (var15.rotationYaw - var15.prevRotationYaw) * var1;
		Vec3D var18 = this.orientCamera(var1);
		float var19 = MathHelper.cos(-var17 * ((float)Math.PI / 180.0F) - (float)Math.PI);
		float var29 = MathHelper.sin(-var17 * ((float)Math.PI / 180.0F) - (float)Math.PI);
		float var30 = -MathHelper.cos(-var16 * ((float)Math.PI / 180.0F));
		float var31 = MathHelper.sin(-var16 * ((float)Math.PI / 180.0F));
		float var32 = var29 * var30;
		float var34 = var19 * var30;
		double var35 = (double)this.mc.playerController.getBlockReachDistance();
		Vec3D var37 = var18.addVector((double)var32 * var35, (double)var31 * var35, (double)var34 * var35);
		this.mc.objectMouseOver = this.mc.theWorld.rayTraceBlocks(var18, var37);
		double var38 = var35;
		var18 = this.orientCamera(var1);
		if(this.mc.objectMouseOver != null) {
			var38 = this.mc.objectMouseOver.hitVec.distance(var18);
		}

		if(this.mc.playerController instanceof PlayerControllerCreative) {
			var35 = 32.0D;
		} else {
			if(var38 > 3.0D) {
				var38 = 3.0D;
			}

			var35 = var38;
		}

		var37 = var18.addVector((double)var32 * var35, (double)var31 * var35, (double)var34 * var35);
		this.pointedEntity = null;
		List var40 = this.mc.theWorld.a(var15, var15.boundingBox.addCoord((double)var32 * var35, (double)var31 * var35, (double)var34 * var35));
		double var41 = 0.0D;

		int var14;
		double var48;
		MovingObjectPosition var57;
		for(var14 = 0; var14 < var40.size(); ++var14) {
			Entity var53 = (Entity)var40.get(var14);
			if(var53.canBeCollidedWith()) {
				AxisAlignedBB var56 = var53.boundingBox.expand((double)0.1F, (double)0.1F, (double)0.1F);
				var57 = var56.calculateIntercept(var18, var37);
				if(var57 != null) {
					var48 = var18.distance(var57.hitVec);
					if(var48 < var41 || var41 == 0.0D) {
						var13.pointedEntity = var53;
						var41 = var48;
					}
				}
			}
		}

		if(var13.pointedEntity != null && !(var13.mc.playerController instanceof PlayerControllerCreative)) {
			var13.mc.objectMouseOver = new MovingObjectPosition(var13.pointedEntity);
		}

		EntityPlayerSP var2 = this.mc.thePlayer;
		World var3 = this.mc.theWorld;
		RenderGlobal var4 = this.mc.renderGlobal;
		EffectRenderer var5 = this.mc.effectRenderer;
		double var6 = var2.lastTickPosX + (var2.posX - var2.lastTickPosX) * (double)var1;
		double var8 = var2.lastTickPosY + (var2.posY - var2.lastTickPosY) * (double)var1;
		double var10 = var2.lastTickPosZ + (var2.posZ - var2.lastTickPosZ) * (double)var1;

		for(int var12 = 0; var12 < 2; ++var12) {
			if(this.mc.gameSettings.anaglyph) {
				if(var12 == 0) {
					GL11.glColorMask(false, true, true, false);
				} else {
					GL11.glColorMask(true, false, false, false);
				}
			}

			GL11.glViewport(0, 0, this.mc.displayWidth, this.mc.displayHeight);
			World var54 = this.mc.theWorld;
			EntityPlayerSP var59 = this.mc.thePlayer;
			var17 = 1.0F / (float)(4 - this.mc.gameSettings.renderDistance);
			var17 = 1.0F - (float)Math.pow((double)var17, 0.25D);
			var18 = var54.getSkyColor(var1);
			var19 = (float)var18.xCoord;
			var29 = (float)var18.yCoord;
			var30 = (float)var18.zCoord;
			Vec3D var67 = var54.getFogColor(var1);
			this.fogColorRed = (float)var67.xCoord;
			this.fogColorGreen = (float)var67.yCoord;
			this.fogColorBlue = (float)var67.zCoord;
			this.fogColorRed += (var19 - this.fogColorRed) * var17;
			this.fogColorGreen += (var29 - this.fogColorGreen) * var17;
			this.fogColorBlue += (var30 - this.fogColorBlue) * var17;
			Block var70 = Block.blocksList[var54.getBlockId(MathHelper.floor_double(var59.posX), MathHelper.floor_double(var59.posY + (double)0.12F), MathHelper.floor_double(var59.posZ))];
			if(var70 != null && var70.blockMaterial != Material.air) {
				Material var33 = var70.blockMaterial;
				if(var33 == Material.water) {
					this.fogColorRed = 0.02F;
					this.fogColorGreen = 0.02F;
					this.fogColorBlue = 0.2F;
				} else if(var33 == Material.lava) {
					this.fogColorRed = 0.6F;
					this.fogColorGreen = 0.1F;
					this.fogColorBlue = 0.0F;
				}
			}

			float var71 = this.fogColor2 + (this.fogColor1 - this.fogColor2) * var1;
			this.fogColorRed *= var71;
			this.fogColorGreen *= var71;
			this.fogColorBlue *= var71;
			if(this.mc.gameSettings.anaglyph) {
				var34 = (this.fogColorRed * 30.0F + this.fogColorGreen * 59.0F + this.fogColorBlue * 11.0F) / 100.0F;
				float var73 = (this.fogColorRed * 30.0F + this.fogColorGreen * 70.0F) / 100.0F;
				float var36 = (this.fogColorRed * 30.0F + this.fogColorBlue * 70.0F) / 100.0F;
				this.fogColorRed = var34;
				this.fogColorGreen = var73;
				this.fogColorBlue = var36;
			}

			GL11.glClearColor(this.fogColorRed, this.fogColorGreen, this.fogColorBlue, 0.0F);
			GL11.glClear(GL11.GL_DEPTH_BUFFER_BIT | GL11.GL_COLOR_BUFFER_BIT);
			GL11.glEnable(GL11.GL_CULL_FACE);
			float var52 = var1;
			this.farPlaneDistance = (float)(256 >> this.mc.gameSettings.renderDistance);
			GL11.glMatrixMode(GL11.GL_PROJECTION);
			GL11.glLoadIdentity();
			if(this.mc.gameSettings.anaglyph) {
				GL11.glTranslatef((float)(-((var12 << 1) - 1)) * 0.07F, 0.0F, 0.0F);
			}

			EntityPlayerSP var65 = this.mc.thePlayer;
			var29 = 70.0F;
			if(var65.isInsideOfMaterial()) {
				var29 = 60.0F;
			}

			if(var65.health <= 0) {
				var30 = (float)var65.deathTime + var1;
				var29 /= (1.0F - 500.0F / (var30 + 500.0F)) * 2.0F + 1.0F;
			}

			GLU.gluPerspective(var29, (float)this.mc.displayWidth / (float)this.mc.displayHeight, 0.05F, this.farPlaneDistance);
			GL11.glMatrixMode(GL11.GL_MODELVIEW);
			GL11.glLoadIdentity();
			if(this.mc.gameSettings.anaglyph) {
				GL11.glTranslatef((float)((var12 << 1) - 1) * 0.1F, 0.0F, 0.0F);
			}

			this.hurtCameraEffect(var1);
			if(this.mc.gameSettings.fancyGraphics) {
				this.setupViewBobbing(var1);
			}

			EntityRenderer var60 = this;
			var65 = this.mc.thePlayer;
			double var68 = var65.prevPosX + (var65.posX - var65.prevPosX) * (double)var1;
			double var69 = var65.prevPosY + (var65.posY - var65.prevPosY) * (double)var1;
			double var72 = var65.prevPosZ + (var65.posZ - var65.prevPosZ) * (double)var1;
			if(!this.mc.gameSettings.thirdPersonView) {
				GL11.glTranslatef(0.0F, 0.0F, -0.1F);
			} else {
				var35 = 4.0D;
				double var74 = (double)(-MathHelper.sin(var65.rotationYaw / 180.0F * (float)Math.PI) * MathHelper.cos(var65.rotationPitch / 180.0F * (float)Math.PI)) * 4.0D;
				double var39 = (double)(MathHelper.cos(var65.rotationYaw / 180.0F * (float)Math.PI) * MathHelper.cos(var65.rotationPitch / 180.0F * (float)Math.PI)) * 4.0D;
				var41 = (double)(-MathHelper.sin(var65.rotationPitch / 180.0F * (float)Math.PI)) * 4.0D;

				for(int var53 = 0; var52 < 8; ++var53) {
					float var55 = (float)(((var53 & 1) << 1) - 1);
					float var50 = (float)(((var53 >> 1 & 1) << 1) - 1);
					var16 = (float)(((var53 >> 2 & 1) << 1) - 1);
					var55 *= 0.1F;
					var50 *= 0.1F;
					var16 *= 0.1F;
					var57 = var60.mc.theWorld.rayTraceBlocks(new Vec3D(var68 + (double)var55, var69 + (double)var50, var72 + (double)var16), new Vec3D(var68 - var74 + (double)var55 + (double)var16, var69 - var41 + (double)var50, var72 - var39 + (double)var16));
					if(var57 != null) {
						var48 = var57.hitVec.distance(new Vec3D(var68, var69, var72));
						if(var48 < var35) {
							var35 = var48;
						}
					}
				}

				GL11.glTranslatef(0.0F, 0.0F, (float)(-var35));
			}

			GL11.glRotatef(var65.prevRotationPitch + (var65.rotationPitch - var65.prevRotationPitch) * var52, 1.0F, 0.0F, 0.0F);
			GL11.glRotatef(var65.prevRotationYaw + (var65.rotationYaw - var65.prevRotationYaw) * var52 + 180.0F, 0.0F, 1.0F, 0.0F);
			ClippingHelperImplementation.init();
			this.setupFog();
			GL11.glEnable(GL11.GL_FOG);
			var4.renderSky(var1);
			this.setupFog();
			Frustrum var51 = new Frustrum();
			var51.setPosition(var6, var8, var10);
			this.mc.renderGlobal.clipRenderersByFrustrum(var51);
			this.mc.renderGlobal.updateRenderers(var2);
			this.setupFog();
			GL11.glEnable(GL11.GL_FOG);
			GL11.glBindTexture(GL11.GL_TEXTURE_2D, this.mc.renderEngine.getTexture("/terrain.png"));
			RenderHelper.disableStandardItemLighting();
			var4.sortAndRender(var2, 0, (double)var1);
			var14 = MathHelper.floor_double(var2.posX);
			int var58 = MathHelper.floor_double(var2.posY);
			int var61 = MathHelper.floor_double(var2.posZ);
			if(var3.isSolid(var14, var58, var61)) {
				RenderBlocks var62 = new RenderBlocks(var3);

				for(int var64 = var14 - 1; var64 <= var14 + 1; ++var64) {
					for(int var66 = var58 - 1; var66 <= var58 + 1; ++var66) {
						for(int var20 = var61 - 1; var20 <= var61 + 1; ++var20) {
							int var21 = var3.getBlockId(var64, var66, var20);
							if(var21 > 0) {
								var62.renderBlockAllFaces(Block.blocksList[var21], var64, var66, var20);
							}
						}
					}
				}
			}

			RenderHelper.enableStandardItemLighting();
			GL11.glPushMatrix();
			var4.renderEntities(this.orientCamera(var1), var51, var1);
			var5.renderLitParticles(var1);
			GL11.glPopMatrix();
			RenderHelper.disableStandardItemLighting();
			this.setupFog();
			var5.renderParticles(var2, var1);
			if(this.mc.objectMouseOver != null && var2.isInsideOfMaterial()) {
				GL11.glDisable(GL11.GL_ALPHA_TEST);
				var4.drawBlockBreaking(var2, this.mc.objectMouseOver, 0, var2.inventory.getCurrentItem(), var1);
				var4.drawSelectionBox(var2, this.mc.objectMouseOver, 0, var1);
				GL11.glEnable(GL11.GL_ALPHA_TEST);
			}

			GL11.glBlendFunc(GL11.GL_SRC_ALPHA, GL11.GL_ONE_MINUS_SRC_ALPHA);
			this.setupFog();
			GL11.glEnable(GL11.GL_BLEND);
			GL11.glDisable(GL11.GL_CULL_FACE);
			GL11.glColorMask(false, false, false, false);
			GL11.glBindTexture(GL11.GL_TEXTURE_2D, this.mc.renderEngine.getTexture("/terrain.png"));
			int var63 = var4.sortAndRender(var2, 1, (double)var1);
			GL11.glColorMask(true, true, true, true);
			if(this.mc.gameSettings.anaglyph) {
				if(var12 == 0) {
					GL11.glColorMask(false, true, true, false);
				} else {
					GL11.glColorMask(true, false, false, false);
				}
			}

			if(var63 > 0) {
				GL11.glBindTexture(GL11.GL_TEXTURE_2D, this.mc.renderEngine.getTexture("/terrain.png"));
				var4.renderAllRenderLists(1, (double)var1);
			}

			GL11.glDepthMask(true);
			GL11.glEnable(GL11.GL_CULL_FACE);
			GL11.glDisable(GL11.GL_BLEND);
			if(this.mc.objectMouseOver != null && !var2.isInsideOfMaterial()) {
				GL11.glDisable(GL11.GL_ALPHA_TEST);
				var4.drawBlockBreaking(var2, this.mc.objectMouseOver, 0, var2.inventory.getCurrentItem(), var1);
				var4.drawSelectionBox(var2, this.mc.objectMouseOver, 0, var1);
				GL11.glEnable(GL11.GL_ALPHA_TEST);
			}

			GL11.glDisable(GL11.GL_FOG);
			GL11.glClear(GL11.GL_DEPTH_BUFFER_BIT);
			GL11.glLoadIdentity();
			if(this.mc.gameSettings.anaglyph) {
				GL11.glTranslatef((float)((var12 << 1) - 1) * 0.1F, 0.0F, 0.0F);
			}

			GL11.glPushMatrix();
			this.hurtCameraEffect(var1);
			if(this.mc.gameSettings.fancyGraphics) {
				this.setupViewBobbing(var1);
			}

			if(!this.mc.gameSettings.thirdPersonView) {
				this.itemRenderer.renderItemInFirstPerson(var1);
			}

			GL11.glPopMatrix();
			if(!this.mc.gameSettings.thirdPersonView) {
				this.itemRenderer.renderOverlays(var1);
				this.hurtCameraEffect(var1);
			}

			if(this.mc.gameSettings.fancyGraphics) {
				this.setupViewBobbing(var1);
			}

			if(!this.mc.gameSettings.anaglyph) {
				return;
			}
		}

		GL11.glColorMask(true, true, true, false);
	}

	public final void setupOverlayRendering() {
		ScaledResolution var1 = new ScaledResolution(this.mc.displayWidth, this.mc.displayHeight);
		int var2 = var1.getScaledWidth();
		int var3 = var1.getScaledHeight();
		GL11.glClear(GL11.GL_DEPTH_BUFFER_BIT);
		GL11.glMatrixMode(GL11.GL_PROJECTION);
		GL11.glLoadIdentity();
		GL11.glOrtho(0.0D, (double)var2, (double)var3, 0.0D, 1000.0D, 3000.0D);
		GL11.glMatrixMode(GL11.GL_MODELVIEW);
		GL11.glLoadIdentity();
		GL11.glTranslatef(0.0F, 0.0F, -2000.0F);
	}

	private void setupFog() {
		World var1 = this.mc.theWorld;
		EntityPlayerSP var2 = this.mc.thePlayer;
		int var10000 = GL11.GL_FOG_COLOR;
		float var3 = 1.0F;
		float var6 = this.fogColorBlue;
		float var5 = this.fogColorGreen;
		float var4 = this.fogColorRed;
		this.fogColorBuffer.clear();
		this.fogColorBuffer.put(var4).put(var5).put(var6).put(1.0F);
		this.fogColorBuffer.flip();
		GL11.glFog(var10000, this.fogColorBuffer);
		GL11.glNormal3f(0.0F, -1.0F, 0.0F);
		GL11.glColor4f(1.0F, 1.0F, 1.0F, 1.0F);
		Block var7 = Block.blocksList[var1.getBlockId(MathHelper.floor_double(var2.posX), MathHelper.floor_double(var2.posY + (double)0.12F), MathHelper.floor_double(var2.posZ))];
		if(var7 != null && var7.blockMaterial.getIsLiquid()) {
			Material var8 = var7.blockMaterial;
			GL11.glFogi(GL11.GL_FOG_MODE, GL11.GL_EXP);
			if(var8 == Material.water) {
				GL11.glFogf(GL11.GL_FOG_DENSITY, 0.1F);
			} else if(var8 == Material.lava) {
				GL11.glFogf(GL11.GL_FOG_DENSITY, 2.0F);
			}
		} else {
			GL11.glFogi(GL11.GL_FOG_MODE, GL11.GL_LINEAR);
			GL11.glFogf(GL11.GL_FOG_START, this.farPlaneDistance * 0.25F);
			GL11.glFogf(GL11.GL_FOG_END, this.farPlaneDistance);
			if(GLContext.getCapabilities().GL_NV_fog_distance) {
				GL11.glFogi(NVFogDistance.GL_FOG_DISTANCE_MODE_NV, NVFogDistance.GL_EYE_RADIAL_NV);
			}
		}

		GL11.glEnable(GL11.GL_COLOR_MATERIAL);
		GL11.glColorMaterial(GL11.GL_FRONT, GL11.GL_AMBIENT);
	}
}
