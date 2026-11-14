package itdelatrisu.opsu;
import fluddokt.opsu.fake.Image;
public enum GameImage {
	MOD_EASY, MOD_NO_FAIL, MOD_HALF_TIME, MOD_HARD_ROCK, MOD_SUDDEN_DEATH, MOD_DOUBLE_TIME, MOD_NIGHTCORE,
	MOD_HIDDEN, MOD_FLASHLIGHT, MOD_RELAX, MOD_AUTOPILOT, MOD_SPUN_OUT, MOD_AUTO;
	public static final float PARALLAX_SCALE = 1.1f;
	public Image getImage() { return new Image(); }
}
