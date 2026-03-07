package net.minecraft.game.world.terrain.generate;

import java.util.Random;
import net.minecraft.game.world.World;

public abstract class WorldGenerator {
	public abstract boolean generate(World var1, Random var2, int var3, int var4, int var5);
}
