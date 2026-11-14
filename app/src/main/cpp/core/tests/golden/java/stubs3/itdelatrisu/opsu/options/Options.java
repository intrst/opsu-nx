package itdelatrisu.opsu.options;
import fluddokt.opsu.fake.Color;
/** Stub with settable values; the C++ test passes the same values explicitly. */
public class Options {
	public static float playfieldScale = 1f, fixedSpeed = 0f, fixedCS = 0f;
	public static boolean unicode = false;
	public static float getPlayfieldScale() { return playfieldScale; }
	public static float getFixedSpeed() { return fixedSpeed; }
	public static float getFixedCS() { return fixedCS; }
	public static boolean useUnicodeMetadata() { return unicode; }
	public static boolean isParallaxEnabled() { return false; }
	public static final Color[] SKIN_COMBO = { new Color(1, 2, 3), new Color(250, 251, 252) };
	public static final Color SKIN_BORDER = new Color(9, 8, 7);
	public static class SkinStub {
		public boolean getSliderTrackOverride() { return false; }
		public Color[] getComboColors() { return SKIN_COMBO; }
		public Color getSliderBorderColor() { return SKIN_BORDER; }
	}
	public static SkinStub getSkin() { return new SkinStub(); }
}
