package net.minecraft.src;

public class EntityOtherPlayerMP extends EntityPlayer {
	public EntityOtherPlayerMP(World var1, String var2) {
		super(var1);
		this.username = var2;
		this.at = 1.62F;
		if(var2 != null && var2.length() > 0) {
			this.aQ = "http://www.minecraft.net/skin/" + var2 + ".png";
			System.out.println("Loading texture " + this.aQ);
		}

		this.aF = true;
	}

	public void i_() {
		super.i_();
	}

	public void onLivingUpdate() {
	}
}
