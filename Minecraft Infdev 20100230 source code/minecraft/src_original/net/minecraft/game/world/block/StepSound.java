package net.minecraft.game.world.block;

public class StepSound {
	private String stepSoundName;
	public final float stepSoundVolume;
	public final float stepSoundPitch;

	public StepSound(String var1, float var2, float var3) {
		this.stepSoundName = var1;
		this.stepSoundVolume = var2;
		this.stepSoundPitch = var3;
	}

	public String getBreakSound() {
		return "step." + this.stepSoundName;
	}

	public final String getStepSound() {
		return "step." + this.stepSoundName;
	}
}
