package net.minecraft.client.render;

import java.util.Comparator;
import net.minecraft.game.entity.player.EntityPlayer;

public final class RenderSorter implements Comparator {
	private EntityPlayer entityPlayer;

	public RenderSorter(EntityPlayer var1) {
		this.entityPlayer = var1;
	}

	public final int compare(Object var1, Object var2) {
		WorldRenderer var10001 = (WorldRenderer)var1;
		WorldRenderer var3 = (WorldRenderer)var2;
		WorldRenderer var6 = var10001;
		boolean var4 = var6.isInFrustum;
		boolean var5 = var3.isInFrustum;
		return var4 && !var5 ? 1 : ((!var5 || var4) && var6.distanceToEntitySquared(this.entityPlayer) < var3.distanceToEntitySquared(this.entityPlayer) ? 1 : -1);
	}
}
