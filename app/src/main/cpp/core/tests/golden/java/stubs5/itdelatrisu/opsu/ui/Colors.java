package itdelatrisu.opsu.ui;
import fluddokt.opsu.fake.Color;
public class Colors {
	public static final Color BLACK_BG_NORMAL = new Color(0f, 0f, 0f, 0.5f), GREEN = new Color(0.1f, 0.8f, 0.3f), LAZER_UNKNOWN_STAR = new Color(0.5f, 0.5f, 0.5f), LAZER_STAR_GOLD = new Color(1f, 0.8f, 0.2f);
	public static Color getDifficultyColor(double stars) { return new Color((float) (stars / 10.0), 0.5f, 0.5f); }
}
