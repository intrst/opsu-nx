package itdelatrisu.opsu.ui;
import fluddokt.opsu.fake.UnicodeFont;
import java.util.ArrayList;
import java.util.List;
public class Fonts {
	public static UnicodeFont DEFAULT, BOLD, LARGE, MEDIUM;
	public static final List<String> glyphLog = new ArrayList<String>();
	public static void setup(int large, int medium, int bold, int def) {
		LARGE = new UnicodeFont(large, 9.5f); MEDIUM = new UnicodeFont(medium, 7.25f);
		BOLD = new UnicodeFont(bold, 6.5f); DEFAULT = new UnicodeFont(def, 5.75f);
	}
	public static void loadGlyphs(UnicodeFont f, String s) {
		glyphLog.add((f == LARGE ? "LARGE" : f == MEDIUM ? "MEDIUM" : f == BOLD ? "BOLD" : "DEFAULT") + ":" + s);
	}
}
