package net.minecraft.client.render;

import java.util.Comparator;
import net.minecraft.game.entity.Entity;

public final class EntitySorter implements Comparator {
	private Entity entity;

	public EntitySorter(Entity var1) {
		this.entity = var1;
	}

	public final int compare(Object var1, Object var2) {
		WorldRenderer var10001 = (WorldRenderer)var1;
		WorldRenderer var3 = (WorldRenderer)var2;
		WorldRenderer var4 = var10001;
		return var4.distanceToEntitySquared(this.entity) < var3.distanceToEntitySquared(this.entity) ? -1 : 1;
	}
}
