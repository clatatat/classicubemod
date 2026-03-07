package net.minecraft.client.gui;

import net.minecraft.client.GuiMainMenu;
import net.minecraft.game.world.World;

public final class GuiIngameMenu extends GuiScreen {
	public final void initGui() {
		this.controlList.clear();
		this.controlList.add(new GuiButton(0, this.width / 2 - 100, this.height / 4, "Options..."));
		this.controlList.add(new GuiButton(1, this.width / 2 - 100, this.height / 4 + 24, "Change world..."));
		this.controlList.add(new GuiButton(2, this.width / 2 - 100, this.height / 4 + 48, "Quit game"));
		this.controlList.add(new GuiButton(4, this.width / 2 - 100, this.height / 4 + 120, "Back to game"));
	}

	protected final void actionPerformed(GuiButton var1) {
		if(var1.id == 0) {
			this.mc.displayGuiScreen(new GuiOptions(this, this.mc.gameSettings));
		}

		if(var1.id == 1) {
			this.mc.displayGuiScreen(new GuiSelectWorld(this));
		}

		if(var1.id == 2) {
			this.mc.changeWorld2((World)null);
			this.mc.displayGuiScreen(new GuiMainMenu());
		}

		if(var1.id == 4) {
			this.mc.displayGuiScreen((GuiScreen)null);
			this.mc.setIngameFocus();
		}

	}

	public final void drawScreen(int var1, int var2, float var3) {
		this.drawDefaultBackground();
		drawCenteredString(this.fontRenderer, "Game menu", this.width / 2, 40, 16777215);
		super.drawScreen(var1, var2, var3);
	}
}
