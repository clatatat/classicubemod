package net.minecraft.src;

class NetworkReaderThread extends Thread {
	final NetworkManager netManager;

	NetworkReaderThread(NetworkManager var1, String var2) {
		super(var2);
		this.netManager = var1;
	}

	public void run() {
		while(NetworkManager.isRunning(this.netManager) && !NetworkManager.isServerTerminating(this.netManager)) {
			NetworkManager.readNetworkPacket(this.netManager);
		}

	}
}
