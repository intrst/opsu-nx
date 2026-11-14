package fluddokt.opsu.fake;
import com.badlogic.gdx.graphics.g2d.TextureRegion;
import java.util.ArrayList;
import java.util.List;
/** Stub that records the texture draws. */
public class Graphics {
	public static final List<String> calls = new ArrayList<String>();
	public void setColor(Color c) {}
	public void drawTexture(TextureRegion t, float x, float y, float w, float h, float rot) {
		calls.add(Float.floatToIntBits(x) + " " + Float.floatToIntBits(y) + " " + Float.floatToIntBits(w) + " " + Float.floatToIntBits(h));
	}
	public void setClip(int x, int y, int w, int h) { calls.add("clip " + x + " " + y + " " + w + " " + h); }
	public void clearClip() {}
	public void fillRect(float x, float y, float w, float h) {}
}
