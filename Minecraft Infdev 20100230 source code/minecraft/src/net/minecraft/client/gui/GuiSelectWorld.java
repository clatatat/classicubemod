package net.minecraft.client.gui;

import com.mojang.nbt.NBTTagCompound;
import java.io.File;
import net.minecraft.game.world.World;

public class GuiSelectWorld extends GuiScreen {
	protected GuiScreen parentScreen;
	protected String screenTitle = "Select world";

	public GuiSelectWorld(GuiScreen var1) {
		this.parentScreen = var1;
	}

	public final void initGui() {
		File var1 = this.mc.getAppDir();

		for(int var2 = 0; var2 < 5; ++var2) {
			NBTTagCompound var3 = World.getWorldNBTTag(var1, "World" + (var2 + 1));
			if(var3 == null) {
				this.controlList.add(new GuiButton(var2, this.width / 2 - 100, this.height / 6 + var2 * 24, "- empty -"));
			} else {
				String var4 = "World " + (var2 + 1);
				long var5 = var3.getLong("SizeOnDisk");
				var4 = var4 + " (" + (float)(var5 / 1024L * 100L / 1024L) / 100.0F + " MB)";
				this.controlList.add(new GuiButton(var2, this.width / 2 - 100, this.height / 6 + var2 * 24, var4));
			}
		}

		this.initGui2();
	}

	protected final String getWorldName(int var1) {
		File var2 = this.mc.getAppDir();
		return World.getWorldNBTTag(var2, "World" + var1) != null ? "World" + var1 : null;
	}

	public void initGui2() {
		this.controlList.add(new GuiButton(5, this.width / 2 - 100, this.height / 6 + 120 + 12, "Delete world..."));
		this.controlList.add(new GuiButton(6, this.width / 2 - 100, this.height / 6 + 168, "Cancel"));
	}

	protected final void actionPerformed(GuiButton var1) {
		if(var1.enabled) {
			if(var1.id < 5) {
				this.selectWorld(var1.id + 1);
			} else if(var1.id == 5) {
				this.mc.displayGuiScreen(new GuiDeleteWorld(this));
			} else {
				if(var1.id == 6) {
					this.mc.displayGuiScreen(this.parentScreen);
				}

			}
		}
	}

	public void selectWorld(int var1) {
		this.mc.a("World" + var1);
		this.mc.displayGuiScreen((GuiScreen)null);
	}

	public final void drawScreen(int var1, int var2, float var3) {
		this.drawDefaultBackground();
		drawCenteredString(this.fontRenderer, this.screenTitle, this.width / 2, 20, 16777215);
		super.drawScreen(var1, var2, var3);
	}
}
