package net.minecraft.game.world.block.tileentity;

import com.mojang.nbt.NBTTagCompound;
import java.util.HashMap;
import java.util.Map;
import net.minecraft.game.world.World;

public class TileEntity {
	private static Map nameToClassMap = new HashMap();
	private static Map classToNameMap = new HashMap();
	public World worldObj;
	public int xCoord;
	public int yCoord;
	public int zCoord;

	private static void addMapping(Class var0, String var1) {
		nameToClassMap.put(var1, var0);
		classToNameMap.put(var0, var1);
	}

	public void readFromNBT(NBTTagCompound var1) {
		this.xCoord = var1.getInt("x");
		this.yCoord = var1.getInt("y");
		this.zCoord = var1.getInt("z");
	}

	public void writeToNBT(NBTTagCompound var1) {
		var1.setString("id", (String)classToNameMap.get(this.getClass()));
		var1.setInt("x", this.xCoord);
		var1.setInt("y", this.yCoord);
		var1.setInt("z", this.zCoord);
	}

	public void updateEntity() {
	}

	public static TileEntity createAndLoadEntity(NBTTagCompound var0) {
		TileEntity var1 = null;

		try {
			Class var2 = (Class)nameToClassMap.get(var0.getString("id"));
			if(var2 != null) {
				var1 = (TileEntity)var2.newInstance();
			}
		} catch (Exception var3) {
			var3.printStackTrace();
		}

		if(var1 != null) {
			var1.readFromNBT(var0);
		} else {
			System.out.println("Skipping TileEntity with id " + var0.getString("id"));
		}

		return var1;
	}

	static {
		addMapping(TileEntityFurnace.class, "Furnace");
		addMapping(TileEntityChest.class, "Chest");
	}
}
