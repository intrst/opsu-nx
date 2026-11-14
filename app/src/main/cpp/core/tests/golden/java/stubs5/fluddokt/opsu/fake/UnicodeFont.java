package fluddokt.opsu.fake;
/**
 * Stub font with a deterministic, content dependent width so text fitting is observable:
 * width = (int) sum of cw * (1 + (c % 7) * 0.1f) over the UTF-16 units (float accumulation, like the C++ measurer).
 */
public class UnicodeFont {
	public int lineHeight; public float cw;
	public UnicodeFont(int lineHeight, float cw) { this.lineHeight = lineHeight; this.cw = cw; }
	public int getLineHeight() { return lineHeight; }
	public int getWidth(String s) {
		float w = 0f;
		for (int i = 0; i < s.length(); i++) w += cw * (1f + (s.charAt(i) % 7) * 0.1f);
		return (int) w;
	}
	public void drawString(float x, float y, String s) {}
	public void drawString(float x, float y, String s, Color c) {}
}
