package net.minecraft.src;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public class Packet13PlayerLookMove extends Packet {
	public double a;
	public double b;
	public double c;
	public float d;
	public float e;
	public boolean f;

	public Packet13PlayerLookMove() {
	}

	public Packet13PlayerLookMove(double var1, double var3, double var5, float var7, float var8, boolean var9) {
		this.a = var1;
		this.b = var3;
		this.c = var5;
		this.d = var7;
		this.e = var8;
		this.f = var9;
	}

	public int getPacketId() {
		return 1;
	}

	public void processPacket(NetHandler var1) {
		var1.handleFlying(this);
	}

	public void readPacketData(DataInputStream var1) throws IOException {
		this.a = var1.readDouble();
		this.b = var1.readDouble();
		this.c = var1.readDouble();
		this.d = var1.readFloat();
		this.e = var1.readFloat();
		this.f = var1.read() != 0;
	}

	public void writePacket(DataOutputStream var1) throws IOException {
		var1.writeDouble(this.a);
		var1.writeDouble(this.b);
		var1.writeDouble(this.c);
		var1.writeFloat(this.d);
		var1.writeFloat(this.e);
		var1.write(this.f ? 1 : 0);
	}
}
