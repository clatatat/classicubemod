package net.minecraft.src;

public class GuiDownloadTerrain extends GuiScreen {
	private NetClientHandler netHandler;

	public GuiDownloadTerrain(NetClientHandler var1) {
		this.netHandler = var1;
	}

	protected void keyTyped(char var1, int var2) {
	}

	public void initGui() {
		this.controlList.clear();
	}

	public void updateScreen() {
		if(this.netHandler != null) {
			this.netHandler.processReadPackets();
		}

	}

	protected void actionPerformed(GuiButton var1) {
	}

	public void drawScreen(int var1, int var2, float var3) {
		this.drawBackground(0);
		this.drawCenteredString(this.fontRenderer, "Receiving level", this.width / 2, this.height / 2 - 50, 16777215);
		super.drawScreen(var1, var2, var3);
	}
}
