package net.minecraft.game.world.block;

final class StepSoundSand extends StepSound {
	StepSoundSand(String var1, float var2, float var3) {
		super(var1, 1.0F, 1.0F);
	}

	public final String getBreakSound() {
		return "step.gravel";
	}
}
