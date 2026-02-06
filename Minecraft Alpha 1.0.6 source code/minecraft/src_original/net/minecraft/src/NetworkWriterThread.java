package net.minecraft.src;

class NetworkWriterThread extends Thread {
	final NetworkManager netManager;

	NetworkWriterThread(NetworkManager var1, String var2) {
		super(var2);
		this.netManager = var1;
	}

	public void run() {
		while(NetworkManager.isRunning(this.netManager)) {
			NetworkManager.sendNetworkPacket(this.netManager);
		}

	}
}
