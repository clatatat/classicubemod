package net.minecraft.client.gui;

import java.io.File;
import net.minecraft.game.world.World;

public final class GuiDeleteWorld extends GuiSelectWorld {
	public GuiDeleteWorld(GuiScreen var1) {
		super(var1);
		this.screenTitle = "Delete world";
	}

	public final void initGui2() {
		this.controlList.add(new GuiButton(6, this.width / 2 - 100, this.height / 6 + 168, "Cancel"));
	}

	public final void selectWorld(int var1) {
		String var2 = this.getWorldName(var1);
		if(var2 != null) {
			this.mc.displayGuiScreen(new GuiYesNo(this, "Are you sure you want to delete this world?", "\'" + var2 + "\' will be lost forever!", var1));
		}

	}

	public final void deleteWorld(boolean var1, int var2) {
		if(var1) {
			File var3 = this.mc.getAppDir();
			World.deleteWorld(var3, this.getWorldName(var2));
		}

		this.mc.displayGuiScreen(this.parentScreen);
	}
}
