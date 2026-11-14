package fluddokt.opsu.fake;
public class Image {
	public float getWidth() { return 10; } public float getHeight() { return 10; }
	public Image getScaledCopy(int w, int h) { return this; }
	public void setAlpha(float a) {}
	public void drawCentered(float x, float y) {}
}
