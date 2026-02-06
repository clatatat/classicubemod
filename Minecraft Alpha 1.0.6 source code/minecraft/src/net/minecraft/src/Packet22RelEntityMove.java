package net.minecraft.src;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

class Packet22RelEntityMove extends Packet20Entity {
	public int getPacketId() {
		return 102;
	}

	public void readPacketData(DataInputStream var1) throws IOException {
		super.readPacketData(var1);
		this.xPosition = var1.readByte();
		this.yPosition = var1.readByte();
		this.zPosition = var1.readByte();
	}

	public void writePacket(DataOutputStream var1) throws IOException {
		super.writePacket(var1);
		var1.writeByte(this.xPosition);
		var1.writeByte(this.yPosition);
		var1.writeByte(this.zPosition);
	}
}
