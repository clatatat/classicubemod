package net.minecraft.client;

import java.awt.Canvas;
import java.awt.Component;
import java.io.File;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import javax.swing.JOptionPane;
import net.minecraft.client.controller.PlayerController;
import net.minecraft.client.controller.PlayerControllerCreative;
import net.minecraft.client.controller.PlayerControllerSP;
import net.minecraft.client.effect.EffectRenderer;
import net.minecraft.client.gui.FontRenderer;
import net.minecraft.client.gui.GuiErrorScreen;
import net.minecraft.client.gui.GuiGameOver;
import net.minecraft.client.gui.GuiIngame;
import net.minecraft.client.gui.GuiIngameMenu;
import net.minecraft.client.gui.GuiScreen;
import net.minecraft.client.gui.ScaledResolution;
import net.minecraft.client.gui.container.GuiInventory;
import net.minecraft.client.model.ModelBiped;
import net.minecraft.client.player.EntityPlayerSP;
import net.minecraft.client.player.MovementInputFromOptions;
import net.minecraft.client.render.EntityRenderer;
import net.minecraft.client.render.RenderEngine;
import net.minecraft.client.render.RenderGlobal;
import net.minecraft.client.render.WorldRenderer;
import net.minecraft.client.render.texture.TextureFlamesFX;
import net.minecraft.client.render.texture.TextureGearsFX;
import net.minecraft.client.render.texture.TextureLavaFX;
import net.minecraft.client.render.texture.TextureWaterFX;
import net.minecraft.client.render.texture.TextureWaterFlowFX;
import net.minecraft.client.sound.SoundManager;
import net.minecraft.game.entity.Entity;
import net.minecraft.game.entity.EntityLiving;
import net.minecraft.game.entity.player.InventoryPlayer;
import net.minecraft.game.item.Item;
import net.minecraft.game.item.ItemStack;
import net.minecraft.game.physics.MovingObjectPosition;
import net.minecraft.game.world.World;
import net.minecraft.game.world.block.Block;
import org.lwjgl.BufferUtils;
import org.lwjgl.LWJGLException;
import org.lwjgl.Sys;
import org.lwjgl.input.Controllers;
import org.lwjgl.input.Cursor;
import org.lwjgl.input.Keyboard;
import org.lwjgl.input.Mouse;
import org.lwjgl.opengl.ContextCapabilities;
import org.lwjgl.opengl.Display;
import org.lwjgl.opengl.DisplayMode;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.GLContext;
import util.MathHelper;

public final class Minecraft implements Runnable {
	public PlayerController playerController = new PlayerControllerSP(this);
	private boolean fullscreen = false;
	public int displayWidth;
	public int displayHeight;
	private OpenGlCapsChecker glCapabilities;
	private Timer timer = new Timer(20.0F);
	public World theWorld;
	public RenderGlobal renderGlobal;
	public EntityPlayerSP thePlayer;
	public EffectRenderer effectRenderer;
	public Session session = null;
	public String minecraftUri;
	private Canvas mcCanvas;
	public boolean appletMode = true;
	public volatile boolean isGamePaused = false;
	public RenderEngine renderEngine;
	public FontRenderer fontRenderer;
	public GuiScreen currentScreen = null;
	private LoadingScreenRenderer loadingScreen = new LoadingScreenRenderer(this);
	public EntityRenderer entityRenderer = new EntityRenderer(this);
	private ThreadDownloadResources downloadResourcesThread;
	private int ticksRan = 0;
	private int leftClickCounter = 0;
	private int tempDisplayWidth;
	private int tempDisplayHeight;
	public String loadMapUser = null;
	public int loadMapID = 0;
	public GuiIngame ingameGUI;
	public boolean skipRenderWorld = false;
	public MovingObjectPosition objectMouseOver;
	public GameSettings gameSettings;
	public SoundManager sndManager;
	public MouseHelper mouseHelper;
	private File mcDataDir;
	private String server;
	private TextureWaterFX textureWaterFX;
	private TextureLavaFX textureLavaFX;
	private File minecraftDir;
	volatile boolean running;
	public String debug;
	public boolean inventoryScreen;
	private int prevFrameTime;
	public boolean inGameHasFocus;

	public Minecraft(Canvas var1, MinecraftApplet var2, int var3, int var4, boolean var5) {
		new ModelBiped(0.0F);
		this.objectMouseOver = null;
		this.sndManager = new SoundManager();
		this.server = null;
		this.textureWaterFX = new TextureWaterFX();
		this.textureLavaFX = new TextureLavaFX();
		this.minecraftDir = null;
		this.running = false;
		this.debug = "";
		this.inventoryScreen = false;
		this.prevFrameTime = 0;
		this.inGameHasFocus = false;
		this.tempDisplayWidth = var3;
		this.tempDisplayHeight = var4;
		this.fullscreen = var5;
		new ThreadSleepForever(this, "Timer hack thread");
		this.mcCanvas = var1;
		this.displayWidth = var3;
		this.displayHeight = var4;
		this.fullscreen = var5;
	}

	public final void setServer(String var1, int var2) {
		this.server = var1;
	}

	public final File getAppDir() {
		if(this.minecraftDir == null) {
			String var2 = "minecraft";
			String var1 = System.getProperty("user.home", ".");
			int[] var10001 = OSMap.osValues;
			String var3 = System.getProperty("os.name").toLowerCase();
			File var4;
			switch(var10001[(var3.contains("win") ? EnumOS.windows : (var3.contains("mac") ? EnumOS.macos : (var3.contains("solaris") ? EnumOS.solaris : (var3.contains("sunos") ? EnumOS.solaris : (var3.contains("linux") ? EnumOS.linux : (var3.contains("unix") ? EnumOS.linux : EnumOS.unknown)))))).ordinal()]) {
			case 1:
			case 2:
				var4 = new File(var1, '.' + var2 + '/');
				break;
			case 3:
				var3 = System.getenv("APPDATA");
				if(var3 != null) {
					var4 = new File(var3, "." + var2 + '/');
				} else {
					var4 = new File(var1, '.' + var2 + '/');
				}
				break;
			case 4:
				var4 = new File(var1, "Library/Application Support/" + var2);
				break;
			default:
				var4 = new File(var1, var2 + '/');
			}

			if(!var4.exists() && !var4.mkdirs()) {
				throw new RuntimeException("The working directory could not be created: " + var4);
			}

			this.minecraftDir = var4;
		}

		return this.minecraftDir;
	}

	public final void displayGuiScreen(GuiScreen var1) {
		if(!(this.currentScreen instanceof GuiErrorScreen)) {
			if(this.currentScreen != null) {
				this.currentScreen.onGuiClosed();
			}

			if(var1 == null && this.theWorld == null) {
				var1 = new GuiMainMenu();
			} else if(var1 == null && this.thePlayer.health <= 0) {
				var1 = new GuiGameOver();
			}

			this.currentScreen = (GuiScreen)var1;
			if(var1 != null) {
				this.inputLock();
				ScaledResolution var2 = new ScaledResolution(this.displayWidth, this.displayHeight);
				int var3 = var2.getScaledWidth();
				int var4 = var2.getScaledHeight();
				((GuiScreen)var1).setWorldAndResolution(this, var3, var4);
				this.skipRenderWorld = false;
			} else {
				this.setIngameFocus();
			}
		}
	}

	public final void shutdownMinecraftApplet() {
		try {
			if(this.downloadResourcesThread != null) {
				this.downloadResourcesThread.closeMinecraft();
			}
		} catch (Exception var5) {
		}

		try {
			System.out.println("Stopping!");
			this.changeWorld2((World)null);
			this.sndManager.closeMinecraft();
			Mouse.destroy();
			Keyboard.destroy();
		} finally {
			Display.destroy();
		}

	}

	public final void run() {
		this.running = true;

		try {
			Minecraft var1 = this;
			if(this.mcCanvas != null) {
				Display.setParent(this.mcCanvas);
			} else if(this.fullscreen) {
				Display.setFullscreen(true);
				this.displayWidth = Display.getDisplayMode().getWidth();
				this.displayHeight = Display.getDisplayMode().getHeight();
			} else {
				Display.setDisplayMode(new DisplayMode(this.displayWidth, this.displayHeight));
			}

			Display.setTitle("Minecraft Minecraft Infdev");

			IntBuffer var24;
			try {
				Display.create();
				System.out.println("LWJGL version: " + Sys.getVersion());
				System.out.println("GL RENDERER: " + GL11.glGetString(GL11.GL_RENDERER));
				System.out.println("GL VENDOR: " + GL11.glGetString(GL11.GL_VENDOR));
				System.out.println("GL VERSION: " + GL11.glGetString(GL11.GL_VERSION));
				ContextCapabilities var2 = GLContext.getCapabilities();
				System.out.println("OpenGL 3.0: " + var2.OpenGL30);
				System.out.println("OpenGL 3.1: " + var2.OpenGL31);
				System.out.println("OpenGL 3.2: " + var2.OpenGL32);
				System.out.println("ARB_compatibility: " + var2.GL_ARB_compatibility);
				if(var2.OpenGL32) {
					var24 = ByteBuffer.allocateDirect(64).order(ByteOrder.nativeOrder()).asIntBuffer();
					GL11.glGetInteger('\u9126', var24);
					int var25 = var24.get(0);
					System.out.println("PROFILE MASK: " + Integer.toBinaryString(var25));
					System.out.println("CORE PROFILE: " + ((var25 & 1) != 0));
					System.out.println("COMPATIBILITY PROFILE: " + ((var25 & 2) != 0));
				}
			} catch (LWJGLException var17) {
				var17.printStackTrace();

				try {
					Thread.sleep(1000L);
				} catch (InterruptedException var16) {
				}

				Display.create();
			}

			Keyboard.create();
			Mouse.create();
			this.mouseHelper = new MouseHelper(this.mcCanvas);

			try {
				Controllers.create();
			} catch (Exception var15) {
				var15.printStackTrace();
			}

			GL11.glEnable(GL11.GL_TEXTURE_2D);
			GL11.glShadeModel(GL11.GL_SMOOTH);
			GL11.glClearDepth(1.0D);
			GL11.glEnable(GL11.GL_DEPTH_TEST);
			GL11.glDepthFunc(GL11.GL_LEQUAL);
			GL11.glEnable(GL11.GL_ALPHA_TEST);
			GL11.glAlphaFunc(GL11.GL_GREATER, 0.1F);
			GL11.glCullFace(GL11.GL_BACK);
			GL11.glMatrixMode(GL11.GL_PROJECTION);
			GL11.glLoadIdentity();
			GL11.glMatrixMode(GL11.GL_MODELVIEW);
			this.glCapabilities = new OpenGlCapsChecker();
			this.mcDataDir = this.getAppDir();
			this.gameSettings = new GameSettings(this, this.mcDataDir);
			this.sndManager.loadSoundSettings(this.gameSettings);
			this.renderEngine = new RenderEngine(this.gameSettings);
			this.renderEngine.registerTextureFX(this.textureLavaFX);
			this.renderEngine.registerTextureFX(this.textureWaterFX);
			this.renderEngine.registerTextureFX(new TextureWaterFlowFX());
			this.renderEngine.registerTextureFX(new TextureFlamesFX(0));
			this.renderEngine.registerTextureFX(new TextureFlamesFX(1));
			this.renderEngine.registerTextureFX(new TextureGearsFX(0));
			this.renderEngine.registerTextureFX(new TextureGearsFX(1));
			this.fontRenderer = new FontRenderer(this.gameSettings, "/default.png", this.renderEngine);
			var24 = BufferUtils.createIntBuffer(256);
			var24.clear().limit(256);
			this.renderGlobal = new RenderGlobal(this, this.renderEngine);
			GL11.glViewport(0, 0, this.displayWidth, this.displayHeight);
			if(this.server != null && this.session != null) {
				this.changeWorld2((World)null);
			} else if(this.theWorld == null) {
				this.displayGuiScreen(new GuiMainMenu());
			}

			this.effectRenderer = new EffectRenderer(this.theWorld, this.renderEngine);

			try {
				var1.downloadResourcesThread = new ThreadDownloadResources(var1.mcDataDir, var1);
				var1.downloadResourcesThread.start();
			} catch (Exception var14) {
			}

			this.ingameGUI = new GuiIngame(this);
		} catch (Exception var22) {
			var22.printStackTrace();
			JOptionPane.showMessageDialog((Component)null, var22.toString(), "Failed to start Minecraft", 0);
			return;
		}

		long var23 = System.currentTimeMillis();
		int var3 = 0;

		try {
			while(this.running) {
				if(this.mcCanvas == null && Display.isCloseRequested()) {
					this.running = false;
				}

				try {
					if(this.isGamePaused) {
						float var4 = this.timer.renderPartialTicks;
						this.timer.updateTimer();
						this.timer.renderPartialTicks = var4;
					} else {
						this.timer.updateTimer();
					}

					for(int var26 = 0; var26 < this.timer.elapsedTicks; ++var26) {
						++this.ticksRan;
						this.runTick();
					}

					if(this.isGamePaused) {
						this.timer.renderPartialTicks = 1.0F;
					}

					this.sndManager.setListener(this.thePlayer, this.timer.renderPartialTicks);
					GL11.glEnable(GL11.GL_TEXTURE_2D);
					if(this.theWorld != null) {
						this.theWorld.updatingLighting();
					}

					this.playerController.setPartialTime(this.timer.renderPartialTicks);
					this.entityRenderer.updateCameraAndRender(this.timer.renderPartialTicks);
					if(!Display.isActive()) {
						if(this.fullscreen) {
							this.toggleFullscreen();
						}

						Thread.sleep(10L);
					}

					if(this.mcCanvas != null && !this.fullscreen && (this.mcCanvas.getWidth() != this.displayWidth || this.mcCanvas.getHeight() != this.displayHeight)) {
						this.displayWidth = this.mcCanvas.getWidth();
						this.displayHeight = this.mcCanvas.getHeight();
						this.resize(this.displayWidth, this.displayHeight);
					}

					if(this.gameSettings.limitFramerate) {
						Thread.sleep(5L);
					}

					++var3;
					this.isGamePaused = this.currentScreen != null && this.currentScreen.doesGuiPauseGame();
				} catch (Exception var18) {
					this.displayGuiScreen(new GuiErrorScreen("Client error", "The game broke! [" + var18 + "]"));
					var18.printStackTrace();
					return;
				}

				while(System.currentTimeMillis() >= var23 + 1000L) {
					this.debug = var3 + " fps, " + WorldRenderer.chunksUpdated + " chunk updates";
					WorldRenderer.chunksUpdated = 0;
					var23 += 1000L;
					var3 = 0;
				}
			}

			return;
		} catch (MinecraftError var19) {
		} catch (Exception var20) {
			var20.printStackTrace();
			return;
		} finally {
			this.shutdownMinecraftApplet();
		}

	}

	public final void setIngameFocus() {
		if(Display.isActive()) {
			if(!this.inventoryScreen) {
				this.inventoryScreen = true;
				this.mouseHelper.grabMouseCursor();
				this.displayGuiScreen((GuiScreen)null);
				this.prevFrameTime = this.ticksRan + 10000;
			}
		}
	}

	private void inputLock() {
		if(this.inventoryScreen) {
			if(this.thePlayer != null) {
				EntityPlayerSP var1 = this.thePlayer;
				var1.movementInput.resetKeyState();
			}

			this.inventoryScreen = false;

			try {
				Mouse.setNativeCursor((Cursor)null);
			} catch (LWJGLException var2) {
				var2.printStackTrace();
			}
		}
	}

	public final void displayInGameMenu() {
		if(this.currentScreen == null) {
			this.displayGuiScreen(new GuiIngameMenu());
		}
	}

	private void clickMouse(int var1) {
		if(var1 != 0 || this.leftClickCounter <= 0) {
			if(var1 == 0) {
				this.entityRenderer.itemRenderer.equippedItemRender();
			}

			ItemStack var2;
			int var3;
			World var5;
			if(var1 == 1) {
				var2 = this.thePlayer.inventory.getCurrentItem();
				if(var2 != null) {
					var3 = var2.stackSize;
					EntityPlayerSP var7 = this.thePlayer;
					var5 = this.theWorld;
					ItemStack var4 = var2.getItem().onItemRightClick(var2, var5, var7);
					if(var4 != var2 || var4 != null && var4.stackSize != var3) {
						this.thePlayer.inventory.mainInventory[this.thePlayer.inventory.currentItem] = var4;
						this.entityRenderer.itemRenderer.resetEquippedProgress();
						if(var4.stackSize == 0) {
							this.thePlayer.inventory.mainInventory[this.thePlayer.inventory.currentItem] = null;
						}
					}
				}
			}

			if(this.objectMouseOver == null) {
				if(var1 == 0 && !(this.playerController instanceof PlayerControllerCreative)) {
					this.leftClickCounter = 10;
				}

			} else {
				ItemStack var9;
				if(this.objectMouseOver.typeOfHit == 1) {
					if(var1 == 0) {
						Entity var14 = this.objectMouseOver.entityHit;
						EntityPlayerSP var12 = this.thePlayer;
						InventoryPlayer var11 = var12.inventory;
						var9 = var11.getStackInSlot(var11.currentItem);
						int var17 = var9 != null ? Item.itemsList[var9.itemID].getDamageVsEntity() : 1;
						if(var17 > 0) {
							var14.attackEntityFrom(var12, var17);
							var2 = var12.inventory.getCurrentItem();
							if(var2 != null && var14 instanceof EntityLiving) {
								EntityLiving var8 = (EntityLiving)var14;
								Item.itemsList[var2.itemID].hitEntity(var2);
								if(var2.stackSize <= 0) {
									var12.displayInventoryGUI();
								}
							}
						}

						return;
					}
				} else if(this.objectMouseOver.typeOfHit == 0) {
					int var10 = this.objectMouseOver.blockX;
					var3 = this.objectMouseOver.blockY;
					int var13 = this.objectMouseOver.blockZ;
					int var15 = this.objectMouseOver.sideHit;
					Block var6 = Block.blocksList[this.theWorld.getBlockId(var10, var3, var13)];
					if(var1 == 0) {
						this.theWorld.extinguishFire(var10, var3, var13, this.objectMouseOver.sideHit);
						if(var6 != Block.bedrock) {
							this.playerController.clickBlock(var10, var3, var13);
							return;
						}
					} else {
						var9 = this.thePlayer.inventory.getCurrentItem();
						int var16 = this.theWorld.getBlockId(var10, var3, var13);
						if(var16 > 0 && Block.blocksList[var16].blockActivated(this.theWorld, var10, var3, var13, this.thePlayer)) {
							return;
						}

						if(var9 == null) {
							return;
						}

						var16 = var9.stackSize;
						int var18 = var15;
						var5 = this.theWorld;
						if(var9.getItem().onItemUse(var9, var5, var10, var3, var13, var18)) {
							this.entityRenderer.itemRenderer.equippedItemRender();
						}

						if(var9.stackSize == 0) {
							this.thePlayer.inventory.mainInventory[this.thePlayer.inventory.currentItem] = null;
							return;
						}

						if(var9.stackSize != var16) {
							this.entityRenderer.itemRenderer.resetEquippedProgress2();
						}
					}
				}

			}
		}
	}

	public final void toggleFullscreen() {
		try {
			this.fullscreen = !this.fullscreen;
			System.out.println("Toggle fullscreen!");
			if(this.fullscreen) {
				Display.setDisplayMode(Display.getDesktopDisplayMode());
				this.displayWidth = Display.getDisplayMode().getWidth();
				this.displayHeight = Display.getDisplayMode().getHeight();
			} else {
				if(this.mcCanvas != null) {
					this.displayWidth = this.mcCanvas.getWidth();
					this.displayHeight = this.mcCanvas.getHeight();
				} else {
					this.displayWidth = this.tempDisplayWidth;
					this.displayHeight = this.tempDisplayHeight;
				}

				Display.setDisplayMode(new DisplayMode(this.tempDisplayWidth, this.tempDisplayHeight));
			}

			this.inputLock();
			Display.setFullscreen(this.fullscreen);
			Display.update();
			Thread.sleep(1000L);
			if(this.fullscreen) {
				this.setIngameFocus();
			}

			if(this.currentScreen != null) {
				this.inputLock();
				this.resize(this.displayWidth, this.displayHeight);
			}

			System.out.println("Size: " + this.displayWidth + ", " + this.displayHeight);
		} catch (Exception var2) {
			var2.printStackTrace();
		}
	}

	private void resize(int var1, int var2) {
		this.displayWidth = var1;
		this.displayHeight = var2;
		if(this.currentScreen != null) {
			ScaledResolution var3 = new ScaledResolution(var1, var2);
			var2 = var3.getScaledWidth();
			var1 = var3.getScaledHeight();
			this.currentScreen.setWorldAndResolution(this, var2, var1);
		}

	}

	private void runTick() {
		this.ingameGUI.updateTick();
		if(!this.isGamePaused && this.theWorld != null) {
			this.playerController.onUpdate();
		}

		GL11.glBindTexture(GL11.GL_TEXTURE_2D, this.renderEngine.getTexture("/terrain.png"));
		if(!this.isGamePaused) {
			this.renderEngine.updateDynamicTextures();
		}

		if(this.currentScreen == null && this.thePlayer != null && this.thePlayer.health <= 0) {
			this.displayGuiScreen((GuiScreen)null);
		}

		if(this.currentScreen == null || this.currentScreen.allowUserInput) {
			label276:
			while(true) {
				while(true) {
					int var1;
					int var2;
					while(Mouse.next()) {
						var1 = Mouse.getEventDWheel();
						if(var1 != 0) {
							var2 = var1;
							InventoryPlayer var5 = this.thePlayer.inventory;
							if(var1 > 0) {
								var2 = 1;
							}

							if(var2 < 0) {
								var2 = -1;
							}

							for(var5.currentItem -= var2; var5.currentItem < 0; var5.currentItem += 9) {
							}

							while(var5.currentItem >= 9) {
								var5.currentItem -= 9;
							}
						}

						if(this.currentScreen == null) {
							if(!this.inventoryScreen && Mouse.getEventButtonState()) {
								this.setIngameFocus();
							} else {
								if(Mouse.getEventButton() == 0 && Mouse.getEventButtonState()) {
									this.clickMouse(0);
									this.prevFrameTime = this.ticksRan;
								}

								if(Mouse.getEventButton() == 1 && Mouse.getEventButtonState()) {
									this.clickMouse(1);
									this.prevFrameTime = this.ticksRan;
								}

								if(Mouse.getEventButton() == 2 && Mouse.getEventButtonState() && this.objectMouseOver != null) {
									var2 = this.theWorld.getBlockId(this.objectMouseOver.blockX, this.objectMouseOver.blockY, this.objectMouseOver.blockZ);
									if(var2 == Block.grass.blockID) {
										var2 = Block.dirt.blockID;
									}

									if(var2 == Block.stairDouble.blockID) {
										var2 = Block.stairSingle.blockID;
									}

									if(var2 == Block.bedrock.blockID) {
										var2 = Block.stone.blockID;
									}

									this.thePlayer.inventory.getFirstEmptyStack(var2);
								}
							}
						} else if(this.currentScreen != null) {
							this.currentScreen.handleMouseInput();
						}
					}

					if(this.leftClickCounter > 0) {
						--this.leftClickCounter;
					}

					while(true) {
						while(true) {
							do {
								boolean var3;
								if(!Keyboard.next()) {
									if(this.currentScreen == null) {
										if(Mouse.isButtonDown(0) && (float)(this.ticksRan - this.prevFrameTime) >= this.timer.ticksPerSecond / 4.0F && this.inventoryScreen) {
											this.clickMouse(0);
											this.prevFrameTime = this.ticksRan;
										}

										if(Mouse.isButtonDown(1) && (float)(this.ticksRan - this.prevFrameTime) >= this.timer.ticksPerSecond / 4.0F && this.inventoryScreen) {
											this.clickMouse(1);
											this.prevFrameTime = this.ticksRan;
										}
									}

									var3 = this.currentScreen == null && Mouse.isButtonDown(0) && this.inventoryScreen;
									boolean var9 = false;
									if(!this.playerController.isInTestMode && this.leftClickCounter <= 0) {
										if(var3 && this.objectMouseOver != null && this.objectMouseOver.typeOfHit == 0) {
											var2 = this.objectMouseOver.blockX;
											int var8 = this.objectMouseOver.blockY;
											int var4 = this.objectMouseOver.blockZ;
											this.playerController.sendBlockRemoving(var2, var8, var4, this.objectMouseOver.sideHit);
											this.effectRenderer.addBlockHitEffects(var2, var8, var4, this.objectMouseOver.sideHit);
										} else {
											this.playerController.resetBlockRemoving();
										}
									}
									break label276;
								}

								EntityPlayerSP var10000 = this.thePlayer;
								int var10001 = Keyboard.getEventKey();
								var3 = Keyboard.getEventKeyState();
								var2 = var10001;
								EntityPlayerSP var6 = var10000;
								var6.movementInput.checkKeyForMovementInput(var2, var3);
							} while(!Keyboard.getEventKeyState());

							if(Keyboard.getEventKey() == Keyboard.KEY_F11) {
								this.toggleFullscreen();
							} else {
								if(this.currentScreen != null) {
									this.currentScreen.handleKeyboardInput();
								} else {
									if(Keyboard.getEventKey() == Keyboard.KEY_ESCAPE) {
										this.displayInGameMenu();
									}

									if(this.playerController instanceof PlayerControllerCreative) {
										Keyboard.getEventKey();
										Keyboard.getEventKey();
									}

									if(Keyboard.getEventKey() == Keyboard.KEY_F5) {
										this.gameSettings.thirdPersonView = !this.gameSettings.thirdPersonView;
									}

									if(Keyboard.getEventKey() == this.gameSettings.keyBindInventory.keyCode) {
										this.displayGuiScreen(new GuiInventory(this.thePlayer.inventory));
									}

									if(Keyboard.getEventKey() == this.gameSettings.keyBindDrop.keyCode) {
										this.thePlayer.dropPlayerItemWithRandomChoice(this.thePlayer.inventory.decrStackSize(this.thePlayer.inventory.currentItem, 1), false);
									}
								}

								for(var1 = 0; var1 < 9; ++var1) {
									if(Keyboard.getEventKey() == var1 + 2) {
										this.thePlayer.inventory.currentItem = var1;
									}
								}

								if(Keyboard.getEventKey() == this.gameSettings.keyBindToggleFog.keyCode) {
									this.gameSettings.setOptionFloatValue(4, !Keyboard.isKeyDown(Keyboard.KEY_LSHIFT) && !Keyboard.isKeyDown(Keyboard.KEY_RSHIFT) ? 1 : -1);
								}
							}
						}
					}
				}
			}
		}

		if(this.currentScreen != null) {
			this.prevFrameTime = this.ticksRan + 10000;
		}

		if(this.currentScreen != null) {
			GuiScreen var7 = this.currentScreen;

			while(Mouse.next()) {
				var7.handleMouseInput();
			}

			while(Keyboard.next()) {
				var7.handleKeyboardInput();
			}

			if(this.currentScreen != null) {
				this.currentScreen.updateScreen();
			}
		}

		if(this.theWorld != null) {
			this.theWorld.difficultySetting = this.gameSettings.difficulty;
			if(!this.isGamePaused) {
				this.entityRenderer.updateRenderer();
			}

			if(!this.isGamePaused) {
				this.renderGlobal.updateClouds();
			}

			if(!this.isGamePaused) {
				this.theWorld.levelEntities();
			}

			if(!this.isGamePaused) {
				this.theWorld.restartTimeOfDay();
			}

			if(!this.isGamePaused) {
				this.theWorld.randomDisplayUpdates(MathHelper.floor_double(this.thePlayer.posX), MathHelper.floor_double(this.thePlayer.posY), MathHelper.floor_double(this.thePlayer.posZ));
			}

			if(!this.isGamePaused) {
				this.effectRenderer.updateEffects();
			}
		}

	}

	public final void a(String var1) {
		this.changeWorld2((World)null);
		System.gc();
		World var2 = new World(new File(this.getAppDir(), "saves"), var1);
		this.changeWorld2(var2);
	}

	public final void changeWorld2(World var1) {
		if(this.theWorld != null) {
			this.theWorld.saveWorldIndirectly();
		}

		this.theWorld = var1;
		if(var1 != null) {
			this.thePlayer = null;
			var1.playerEntity = this.thePlayer;
			if(this.thePlayer == null) {
				this.thePlayer = new EntityPlayerSP(this, var1, this.session);
				this.thePlayer.preparePlayerToSpawn();
				if(var1 != null) {
					var1.spawnEntityInWorld(this.thePlayer);
					var1.playerEntity = this.thePlayer;
					var1.spawnPlayer();
				}
			}

			this.thePlayer.movementInput = new MovementInputFromOptions(this.gameSettings);
			this.playerController.onRespawn(this.thePlayer);
			if(this.renderGlobal != null) {
				this.renderGlobal.changeWorld(var1);
			}

			if(this.effectRenderer != null) {
				this.effectRenderer.clearEffects(var1);
			}

			this.changeWorld1("Loading level");
		}

		System.gc();
	}

	private void changeWorld1(String var1) {
		this.loadingScreen.setTitle(var1);
		this.loadingScreen.setText("Loading chunks");

		for(int var5 = -256; var5 <= 256; var5 += 16) {
			this.loadingScreen.setProgress((var5 + 256) * 100 / 512);
			int var2 = MathHelper.floor_double(this.thePlayer.posX);
			int var3 = MathHelper.floor_double(this.thePlayer.posZ);

			for(int var4 = -256; var4 <= 256; var4 += 16) {
				this.theWorld.getBlockId(var2 + var5, 64, var3 + var4);
			}
		}

	}

	public final void respawn() {
		if(this.thePlayer != null && this.theWorld != null) {
			World.b(this.thePlayer);
		}

		this.thePlayer = new EntityPlayerSP(this, this.theWorld, this.session);
		this.thePlayer.preparePlayerToSpawn();
		if(this.theWorld != null) {
			this.theWorld.spawnEntityInWorld(this.thePlayer);
			this.theWorld.playerEntity = this.thePlayer;
			this.theWorld.spawnPlayer();
		}

		this.thePlayer.movementInput = new MovementInputFromOptions(this.gameSettings);
		this.playerController.onRespawn(this.thePlayer);
		this.changeWorld1("Respawning");
	}
}
