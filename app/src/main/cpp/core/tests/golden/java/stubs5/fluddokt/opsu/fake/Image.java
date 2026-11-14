package fluddokt.opsu.fake;
/** Compile-only stub (drawResult is rendering and is not run). */
public class Image {
	public float getWidth() { return 10; } public float getHeight() { return 10; }
	public Image getScaledCopy(float s) { return this; } public Image getScaledCopy(int w, int h) { return this; }
	public void setAlpha(float a) {} public void setColor(Object... o) {}
	public void drawCentered(float x, float y) {} public void drawCentered(float x, float y, float s, float a, Color c) {} public void draw(float x, float y) {}
	public void draw(float x, float y, Color c) {} public void draw(float x, float y, float w, float h) {}
	public void draw(float x, float y, float w, float h, Color c) {}
	public void draw(float x, float y, float s, Color c) {}
	public void draw(float x, float y, float s) {}
}
