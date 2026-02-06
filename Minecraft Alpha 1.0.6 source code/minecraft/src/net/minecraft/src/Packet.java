package net.minecraft.src;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

public abstract class Packet {
	public static Packet getNewPacket(int var0) {
		switch(var0) {
		case 0:
			return new Packet1Handshake();
		case 1:
			return new Packet13PlayerLookMove();
		case 2:
			return new Packet24NamedEntitySpawn();
		case 3:
			return new Packet25DestroyEntity();
		case 9:
			return new Packet50PreChunk();
		case 10:
			return new Packet51MapChunk();
		case 11:
			return new Packet52MultiBlockChange();
		case 12:
			return new Packet53BlockChange();
		case 50:
			return new Packet14BlockDig();
		case 51:
			return new Packet15Place();
		case 52:
			return new Packet16BlockItemSwitch();
		case 100:
			return new Packet23PickupSpawn();
		case 101:
			return new Packet21RelEntityMove();
		case 102:
			return new Packet22RelEntityMove();
		case 103:
			return new Packet20Entity();
		case 104:
			return new Packet26EntityTeleport();
		case 254:
			return new Packet0KeepAlive();
		case 255:
			return new Packet255KickDisconnect();
		default:
			return null;
		}
	}

	public static Packet readPacket(DataInputStream var0) throws IOException {
		int var1 = var0.read();
		if(var1 == -1) {
			return null;
		} else {
			Packet var2 = getNewPacket(var1);
			if(var2 == null) {
				throw new IOException("Bad packet id " + var1);
			} else {
				var2.readPacketData(var0);
				return var2;
			}
		}
	}

	public static void writePacket(Packet var0, DataOutputStream var1) throws IOException {
		var1.write(var0.getPacketId());
		var0.writePacket(var1);
	}

	public abstract void readPacketData(DataInputStream var1) throws IOException;

	public abstract void writePacket(DataOutputStream var1) throws IOException;

	public abstract int getPacketId() throws IOException;

	public abstract void processPacket(NetHandler var1);
}
