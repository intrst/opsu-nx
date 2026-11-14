package itdelatrisu.opsu.options;
public class Options {
	public static float playfieldScale = 1f;
	public static float getPlayfieldScale() { return playfieldScale; }
	public static class SkinStub { public boolean getSliderTrackOverride() { return false; } }
	public static SkinStub getSkin() { return new SkinStub(); }
}
