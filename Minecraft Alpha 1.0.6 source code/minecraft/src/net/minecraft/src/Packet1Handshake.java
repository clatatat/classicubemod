package net.minecraft.src;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public class Packet1Handshake extends Packet {
	public int protocol;
	public String username;
	public String password;

	public Packet1Handshake() {
	}

	public Packet1Handshake(String var1, String var2, int var3) {
		this.username = var1;
		this.password = var2;
		this.protocol = var3;
	}

	public int getPacketId() {
		return 0;
	}

	public void readPacketData(DataInputStream var1) throws IOException {
		this.protocol = var1.readInt();
		this.username = var1.readUTF();
		this.password = var1.readUTF();
	}

	public void writePacket(DataOutputStream var1) throws IOException {
		var1.writeInt(this.protocol);
		var1.writeUTF(this.username);
		var1.writeUTF(this.password);
	}

	public void processPacket(NetHandler var1) {
		var1.handleLogin(this);
	}
}
