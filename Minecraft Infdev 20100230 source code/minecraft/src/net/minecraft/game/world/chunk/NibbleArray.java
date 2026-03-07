package net.minecraft.game.world.chunk;

final class NibbleArray {
	public final byte[] data;

	public NibbleArray(int var1) {
		this.data = new byte[var1 >> 1];
	}

	public NibbleArray(byte[] var1) {
		this.data = var1;
	}

	public final int get(int var1, int var2, int var3) {
		var1 = var1 << 11 | var3 << 7 | var2;
		var2 = var1 >> 1;
		var1 &= 1;
		return var1 == 0 ? this.data[var2] & 15 : this.data[var2] >> 4 & 15;
	}

	public final void set(int var1, int var2, int var3, int var4) {
		var1 = var1 << 11 | var3 << 7 | var2;
		var2 = var1 >> 1;
		var1 &= 1;
		if(var1 == 0) {
			this.data[var2] = (byte)(this.data[var2] & 240 | var4 & 15);
		} else {
			this.data[var2] = (byte)(this.data[var2] & 15 | (var4 & 15) << 4);
		}
	}

	public final boolean isValid() {
		return this.data != null;
	}
}
