package net.minecraft.src;

import org.lwjgl.opengl.GL11;

public class RenderPlayer extends RenderLiving {
	private ModelBiped modelBipedMain = (ModelBiped)this.mainModel;
	private ModelBiped modelArmorChestplate = new ModelBiped(1.0F);
	private ModelBiped modelArmor = new ModelBiped(0.5F);
	private static final String[] armorFilenamePrefix = new String[]{"cloth", "chain", "iron", "diamond", "gold"};

	public RenderPlayer() {
		super(new ModelBiped(0.0F), 0.5F);
	}

	protected boolean setArmorModel(EntityPlayer var1, int var2) {
		ItemStack var3 = var1.inventory.armorItemInSlot(3 - var2);
		if(var3 != null) {
			Item var4 = var3.getItem();
			if(var4 instanceof ItemArmor) {
				ItemArmor var5 = (ItemArmor)var4;
				this.loadTexture("/armor/" + armorFilenamePrefix[var5.renderIndex] + "_" + (var2 == 2 ? 2 : 1) + ".png");
				ModelBiped var6 = var2 == 2 ? this.modelArmor : this.modelArmorChestplate;
				var6.bipedHead.showModel = var2 == 0;
				var6.bipedHeadwear.showModel = var2 == 0;
				var6.bipedBody.showModel = var2 == 1 || var2 == 2;
				var6.bipedRightArm.showModel = var2 == 1;
				var6.bipedLeftArm.showModel = var2 == 1;
				var6.bipedRightLeg.showModel = var2 == 2 || var2 == 3;
				var6.bipedLeftLeg.showModel = var2 == 2 || var2 == 3;
				this.setRenderPassModel(var6);
				return true;
			}
		}

		return false;
	}

	public void renderPlayer(EntityPlayer var1, double var2, double var4, double var6, float var8, float var9) {
		super.doRenderLiving(var1, var2, var4 - (double)var1.at, var6, var8, var9);
	}

	protected void renderSpecials(EntityPlayer var1, float var2) {
		ItemStack var3 = var1.inventory.getCurrentItem();
		if(var3 != null) {
			GL11.glPushMatrix();
			this.modelBipedMain.bipedRightArm.renderWithRotation(1.0F / 16.0F);
			GL11.glTranslatef(-(1.0F / 16.0F), 7.0F / 16.0F, 1.0F / 16.0F);
			float var4 = 10.0F / 16.0F;
			if(var3.itemID < 256 && RenderBlocks.renderItemIn3d(Block.blocksList[var3.itemID].getRenderType())) {
				GL11.glTranslatef(0.0F, 3.0F / 16.0F, -(5.0F / 16.0F));
				var4 *= 12.0F / 16.0F;
				GL11.glRotatef(20.0F, 1.0F, 0.0F, 0.0F);
				GL11.glScalef(var4, -var4, var4);
			} else {
				GL11.glTranslatef(0.0F, 3.0F / 16.0F, 0.0F);
				GL11.glScalef(var4, -var4, var4);
				GL11.glRotatef(-120.0F, 1.0F, 0.0F, 0.0F);
				GL11.glRotatef(45.0F, 0.0F, 1.0F, 0.0F);
			}

			this.renderManager.itemRenderer.renderItem(var3);
			GL11.glPopMatrix();
		}

	}

	public void drawFirstPersonHand() {
		this.modelBipedMain.bipedRightArm.render(1.0F / 16.0F);
	}

	protected boolean shouldRenderPass(EntityLiving var1, int var2) {
		return this.setArmorModel((EntityPlayer)var1, var2);
	}

	protected void renderEquippedItems(EntityLiving var1, float var2) {
		this.renderSpecials((EntityPlayer)var1, var2);
	}

	public void doRenderLiving(EntityLiving var1, double var2, double var4, double var6, float var8, float var9) {
		this.renderPlayer((EntityPlayer)var1, var2, var4, var6, var8, var9);
	}

	public void doRender(Entity var1, double var2, double var4, double var6, float var8, float var9) {
		this.renderPlayer((EntityPlayer)var1, var2, var4, var6, var8, var9);
	}
}
