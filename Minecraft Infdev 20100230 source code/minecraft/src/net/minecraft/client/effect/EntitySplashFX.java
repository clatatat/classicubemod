package net.minecraft.client.effect;

import net.minecraft.game.world.World;

public final class EntitySplashFX extends EntityRainFX {
	public EntitySplashFX(World var1, double var2, double var4, double var6) {
		super(var1, var2, var4, var6);
		this.particleGravity = 0.04F;
		++this.particleTextureIndex;
	}
}
