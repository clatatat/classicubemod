package net.minecraft.client.controller;

import net.minecraft.client.Minecraft;
import net.minecraft.client.Session;
import net.minecraft.game.entity.player.EntityPlayer;
import net.minecraft.game.item.ItemStack;
import net.minecraft.game.world.block.Block;

public final class PlayerControllerCreative extends PlayerController {
	private PlayerControllerCreative(Minecraft var1) {
		super(var1);
	}

	public final void onRespawn(EntityPlayer var1) {
		for(int var2 = 0; var2 < 9; ++var2) {
			if(var1.inventory.mainInventory[var2] == null) {
				this.mc.thePlayer.inventory.mainInventory[var2] = new ItemStack(((Block)Session.registeredBlocksList.get(var2)).blockID);
			} else {
				this.mc.thePlayer.inventory.mainInventory[var2].stackSize = 1;
			}
		}

	}

	public final boolean shouldDrawHUD() {
		return false;
	}

	public final void onUpdate() {
	}
}
