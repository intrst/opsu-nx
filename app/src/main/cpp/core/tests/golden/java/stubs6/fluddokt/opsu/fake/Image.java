package fluddokt.opsu.fake;
/** Test stub: an image that only knows its size. */
public class Image {
	public int w, h;
	public Image(int w, int h) { this.w = w; this.h = h; }
	public int getWidth() { return w; }
	public int getHeight() { return h; }
}
