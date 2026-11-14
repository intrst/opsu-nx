package itdelatrisu.opsu.ui;
import fluddokt.opsu.fake.Image;
/** Test stub: remembers its centre. */
public class MenuButton {
	private final float x, y;
	public MenuButton(Image i, float x, float y) { this.x = x; this.y = y; }
	public float getX() { return x; }
	public float getY() { return y; }
	public boolean contains(float x, float y) { return false; }
}
