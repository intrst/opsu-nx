import fluddokt.opsu.fake.Image;
import itdelatrisu.opsu.GameImage;
import itdelatrisu.opsu.beatmap.Beatmap;
import itdelatrisu.opsu.beatmap.BeatmapGroup;
import itdelatrisu.opsu.beatmap.BeatmapSet;
import itdelatrisu.opsu.beatmap.BeatmapSetNode;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

/** Sixth golden suite (see gen6.sh): the real BeatmapGroup. Mirrored line for line by tests/test_golden6.cpp. */
public class Golden6 {
	static StringBuilder out = new StringBuilder();
	static long seed = 20261113L;
	static int rnd() { seed = seed * 6364136223846793005L + 1442695040888963407L; return (int) (seed >>> 33); }
	static int rnd(int n) { return rnd() % n; }
	static String hx(float f) { return String.format("%08x", Float.floatToIntBits(f)); }
	static void line(String s) { out.append(s).append('\n'); }

	static void basics() {
		// tabs are not laid out yet: Java throws NullPointerException
		for (BeatmapGroup g : BeatmapGroup.values()) {
			String r;
			try { r = hx(g.getTabCenterX()); } catch (NullPointerException e) { r = "NPE"; }
			line("PREINIT " + g.name() + " " + r);
		}
		BeatmapGroup[] v = BeatmapGroup.values();
		StringBuilder sb = new StringBuilder("VALUES");
		for (BeatmapGroup g : v) sb.append(' ').append(g.name());
		sb.append(" REV");
		for (BeatmapGroup g : BeatmapGroup.VALUES_REVERSED) sb.append(' ').append(g.name());
		line(sb.toString());
		line("CURRENT0 " + BeatmapGroup.current().name());
		for (BeatmapGroup g : v) {
			BeatmapGroup.set(g);
			line("SET " + g.name() + " " + BeatmapGroup.current().name() + " " + g.getEmptyMessage());
		}
		BeatmapGroup.set(BeatmapGroup.ALL);
	}

	// one random library: sets of 0..4 beatmaps with assorted lastPlayed / favorite values
	static void filters() {
		for (int c = 0; c < 400; c++) {
			int style = c % 8;
			int n = (c % 10 == 0) ? rnd(3) : rnd(90);
			ArrayList<BeatmapSetNode> list = new ArrayList<BeatmapSetNode>();
			for (int i = 0; i < n; i++) {
				BeatmapSet set = new BeatmapSet();
				int nb = (style == 7 && rnd(5) == 0) ? 0 : 1 + rnd(4);
				for (int k = 0; k < nb; k++) {
					Beatmap b = new Beatmap();
					int r = rnd(10);
					long t;
					if (style == 0) t = r < 3 ? 0 : rnd(6);                           // many ties and zeros
					else if (style == 1) t = r < 2 ? 0 : 1000000000L + rnd(1000000);  // all within 2^31 of each other
					else if (style == 2) t = r < 2 ? 0 : 1700000000000L + (long) rnd(2000000000);  // < 2^31 apart
					else if (style == 3) t = r < 2 ? 0 : 1600000000000L + (long) rnd(2000000000) * (long) rnd(500);  // far apart: Long2.compare wraps
					else if (style == 4) t = r < 5 ? 0 : (long) rnd(2000000000) * 7L;
					else if (style == 5) t = 1500000000000L + (long) rnd(100) * 4294967296L;  // multiples of 2^32: compare() sees them as equal
					else if (style == 6) t = r < 1 ? 0 : 9000000000000000000L + rnd(1000);   // huge
					else t = r < 4 ? 0 : 1600000000000L + (long) rnd(90) * 86400000L;       // days apart
					b.lastPlayed = t;
					b.favorite = rnd(12) == 0;
					set.beatmaps.add(b);
				}
				list.add(new BeatmapSetNode(set, i));
			}
			StringBuilder sb = new StringBuilder("FILTER " + c + " " + n);
			for (BeatmapGroup g : BeatmapGroup.values()) {
				sb.append(" |").append(g.name());
				for (BeatmapSetNode node : g.filter(list)) sb.append(' ').append(node.idx);
			}
			line(sb.toString());
		}
	}

	static void tabs() {
		for (int c = 0; c < 300; c++) {
			int cw = 100 + rnd(3900);
			float bottomY = rnd(2000) / 4f - 50f;
			int tw, th;
			if (c % 3 == 0) { tw = 1 + rnd(60); th = 1 + rnd(60); }          // small tab: the "don't spread out" branch
			else if (c % 3 == 1) { tw = 1 + rnd(cw / 2 + 1); th = 1 + rnd(200); }
			else { tw = cw / 3 + rnd(300); th = 1 + rnd(500); }               // wide tab: tabOffset <= tabWidth
			GameImage.tab = new Image(tw, th);
			StringBuilder sb = new StringBuilder("TAB " + c + " " + cw + " " + hx(bottomY) + " " + tw + " " + th);
			for (BeatmapGroup g : BeatmapGroup.values()) {
				g.init(cw, bottomY);
				sb.append(' ').append(hx(g.getTabCenterX())).append(' ').append(hx(g.getTabCenterY())).append(' ').append(hx(g.getTabLeft()));
			}
			line(sb.toString());
		}
		// containerWidth = 6 * tabWidth makes tabOffset == tabWidth exactly (the `>` boundary), and +-1 either side
		for (int k = 10; k < 210; k += 10) {
			for (int d = -1; d <= 1; d++) {
				int tw = k + d, cw = 6 * k, th = 20 + k % 7;
				float bottomY = 1000f;
				GameImage.tab = new Image(tw, th);
				StringBuilder sb = new StringBuilder("TABEDGE " + cw + " " + tw + " " + th);
				for (BeatmapGroup g : BeatmapGroup.values()) {
					g.init(cw, bottomY);
					sb.append(' ').append(hx(g.getTabCenterX())).append(' ').append(hx(g.getTabCenterY())).append(' ').append(hx(g.getTabLeft()));
				}
				line(sb.toString());
			}
		}
	}

	public static void main(String[] args) throws Exception {
		basics(); filters(); tabs();
		java.nio.file.Files.write(java.nio.file.Paths.get(args[0]), out.toString().getBytes(StandardCharsets.UTF_8));
		System.out.println("wrote " + args[0] + " (" + out.length() + " chars)");
	}
}
