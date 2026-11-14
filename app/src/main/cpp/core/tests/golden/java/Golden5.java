import com.badlogic.gdx.graphics.g2d.TextureRegion;
import fluddokt.opsu.fake.Graphics;
import fluddokt.opsu.fake.UnicodeFont;
import itdelatrisu.opsu.beatmap.BeatmapSetList;
import itdelatrisu.opsu.downloads.Download;
import itdelatrisu.opsu.downloads.DownloadList;
import itdelatrisu.opsu.downloads.DownloadNode;
import itdelatrisu.opsu.options.Options;
import itdelatrisu.opsu.ui.Fonts;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;

/** Fifth golden suite (see gen5.sh): the layout / hit-test / text-fit / animate half of the REAL DownloadNode.java. */
public class Golden5 {
	static StringBuilder out = new StringBuilder();
	static long seed = 20261113L;
	static int rnd() { seed = seed * 6364136223846793005L + 1442695040888963407L; return (int) (seed >>> 33); }
	static int rnd(int n) { return rnd() % n; }
	static String hx(float f) { return String.format("%08x", Float.floatToIntBits(f)); }
	static void line(String s) { out.append(s).append('\n'); }
	static String u8(String s) { StringBuilder b = new StringBuilder(); for (byte x : s.getBytes(StandardCharsets.UTF_8)) b.append(String.format("%02x", x & 0xFF)); return b.toString(); }

	static Object field(Object o, String name) throws Exception {
		Field f = DownloadNode.class.getDeclaredField(name); f.setAccessible(true); return f.get(o);
	}
	static Object call(String name, Class<?>[] types, Object... args) throws Exception {
		Method m = DownloadNode.class.getDeclaredMethod(name, types); m.setAccessible(true); return m.invoke(null, args);
	}
	static final Class<?>[] I = {int.class}, NONE = {};

	// ------------------------------------------------------------------ layout + hit tests
	static void layout() throws Exception {
		int[] ws = {320, 800, 1080, 1280, 1920, 2400, 3000, 123, 640, 1600};
		int[] hs = {240, 480, 720, 1080, 1440, 2160, 99, 50, 360, 900};
		for (int round = 0; round < 40; round++) {
			int w = ws[rnd(ws.length)], h = hs[rnd(hs.length)];
			int large = 20 + rnd(60), medium = 14 + rnd(46), bold = 10 + rnd(40), def = 8 + rnd(32);
			Fonts.setup(large, medium, bold, def);
			DownloadNode.init(w, h);
			line("LY " + round + " " + w + " " + h + " " + large + " " + medium + " " + bold + " " + def + " " + hx(DownloadNode.getListX()) + " " + hx(DownloadNode.getListWidth())
				+ " " + hx(DownloadNode.getListY()) + " " + hx(DownloadNode.getListHeight()) + " " + hx(DownloadNode.getBottomReserve()) + " " + DownloadNode.maxResultsShown()
				+ " " + hx(DownloadNode.getButtonOffset()) + " " + hx((Float) field(null, "cardWidth")) + " " + hx((Float) field(null, "cardGap"))
				+ " " + hx((Float) call("margin", NONE)) + " " + hx((Float) call("thumbW", NONE)) + " " + hx((Float) call("btnH", NONE)));
			for (int col = 0; col < 3; col++) {
				line("LC " + round + " " + col + " " + hx((Float) call("cardX", I, col)) + " " + hx((Float) call("textLeft", I, col)) + " " + hx((Float) call("textRight", I, col)));
				for (int zone = 0; zone < 4; zone++) {
					float[] r = new float[4];
					DownloadNode.getCardRect(zone, col, r);
					line("LR " + round + " " + col + " " + zone + " " + hx(r[0]) + " " + hx(r[1]) + " " + hx(r[2]) + " " + hx(r[3]));
				}
			}
			for (int row = 0; row < 5; row++) line("LB " + round + " " + row + " " + hx((Float) call("btnY", I, row)));
			Graphics.calls.clear();
			DownloadNode.clipToResultArea(new Graphics());
			line("LK " + round + " " + Graphics.calls.get(0));
			// hit tests: random points over the list, plus exact edges
			float lx = DownloadNode.getListX(), lw = DownloadNode.getListWidth(), ly = DownloadNode.getListY(), lh = DownloadNode.getListHeight();
			for (int i = 0; i < 60; i++) {
				float cx = lx + (rnd(1100) - 50) / 1000f * lw, cy = ly + (rnd(1100) - 50) / 1000f * lh;
				int row = rnd(4), col = rnd(3);
				line("HT " + round + " " + (DownloadNode.resultContains(cx, cy, row, col) ? 1 : 0) + (DownloadNode.resultThumbContains(cx, cy, row, col) ? 1 : 0)
					+ (DownloadNode.resultDownloadContains(cx, cy, row, col) ? 1 : 0) + (DownloadNode.resultAreaContains(cx, cy) ? 1 : 0));
			}
			for (int col = 0; col < 3; col++) for (int row = 0; row < 3; row++) {
				float cx0 = (Float) call("cardX", I, col), cw = (Float) field(null, "cardWidth"), by = ly + row * DownloadNode.getButtonOffset();
				float tl = (Float) call("textLeft", I, col), tr = (Float) call("textRight", I, col), bY = (Float) call("btnY", I, row), bH = (Float) call("btnH", NONE);
				float[] xs = {cx0, cx0 + cw, tl, tr, cx0 + (Float) call("thumbW", NONE), Math.nextUp(cx0), Math.nextDown(cx0 + cw), Math.nextUp(tl), Math.nextDown(tr), (tl + tr) / 2};
				float[] ys = {by, by + (Float) field(null, "buttonHeight"), bY, bY + bH, Math.nextUp(by), Math.nextDown(bY + bH), bY + bH / 2};
				StringBuilder b = new StringBuilder("HE " + round + " " + row + " " + col + ":");
				for (float x : xs) for (float y : ys)
					b.append(' ').append(DownloadNode.resultContains(x, y, row, col) ? 1 : 0).append(DownloadNode.resultThumbContains(x, y, row, col) ? 1 : 0).append(DownloadNode.resultDownloadContains(x, y, row, col) ? 1 : 0);
				line(b.toString());
			}
			for (float x : new float[]{lx, lx + lw, Math.nextUp(lx), Math.nextDown(lx + lw), lx + lw / 2})
				for (float y : new float[]{ly, ly + lh, Math.nextUp(ly), Math.nextDown(ly + lh), ly + lh / 2})
					line("HA " + round + " " + (DownloadNode.resultAreaContains(x, y) ? 1 : 0));
		}
	}

	// ------------------------------------------------------------------ fit / icon
	static void fit() throws Exception {
		Fonts.setup(40, 30, 24, 20);
		String[] strs = {"", "a", "ab", "Hello World", "Mapped by someone with a really long name", "The Quick Brown Fox Jumps Over The Lazy Dog 0123456789",
			"日本語のタイトル とても長いタイトル", "café ééé", "......", "WWWWWWWWWWWWWWWWWWWW"};
		UnicodeFont[] fonts = {Fonts.DEFAULT, Fonts.BOLD, Fonts.LARGE};
		for (int fi = 0; fi < fonts.length; fi++) for (String s : strs) {
			float[] ws = {-5f, 0f, 1f, 10f, 20f, 33.5f, 60f, 100f, 150f, 250f, 400f, 1000f, 100000f};
			for (float w : ws)
				line("FT " + fi + " " + u8(s) + " " + hx(w) + " " + u8((String) call("fit", new Class<?>[]{UnicodeFont.class, String.class, float.class}, fonts[fi], s, w)));
			for (int i = 0; i < 12; i++) {
				float w = rnd(30000) / 100f;
				line("FT " + fi + " " + u8(s) + " " + hx(w) + " " + u8((String) call("fit", new Class<?>[]{UnicodeFont.class, String.class, float.class}, fonts[fi], s, w)));
			}
		}
	}

	static void icon() throws Exception {
		Class<?>[] t = {Graphics.class, TextureRegion.class, float.class, float.class, float.class, float.class};
		float[] bhs = {-10f, -1f, 0f, 0.5f, 1f, 5f, 11.5f, 12f, 24f, 37.7f, 64f, 100f, 250.25f};
		for (float bh : bhs) for (int i = 0; i < 4; i++) {
			float cx = (rnd(4000) - 500) / 7f, cy = (rnd(4000) - 500) / 7f;
			Graphics.calls.clear();
			call("drawDownloadIcon", t, new Graphics(), new TextureRegion(), cx, cy, bh, 0.5f);
			StringBuilder b = new StringBuilder("IC " + hx(cx) + " " + hx(cy) + " " + hx(bh) + " " + Graphics.calls.size() + ":");
			for (String c : Graphics.calls) b.append(" [").append(c).append(']');
			line(b.toString());
		}
	}

	// ------------------------------------------------------------------ animate
	static void animate() throws Exception {
		Download.Status[] sts = Download.Status.values();
		for (int round = 0; round < 60; round++) {
			int id = 1000 + round;
			DownloadNode n = new DownloadNode(id, "d", "t", "", "a", "", "c");
			StringBuilder b = new StringBuilder("AN " + round + ":");
			for (int step = 0; step < 50; step++) {
				switch (rnd(7)) {
				case 0: BeatmapSetList.ids.add(id); break;
				case 1: BeatmapSetList.ids.remove(id); break;
				case 2: DownloadList.map.remove(id); break;
				case 3: case 4: {
					Download d = new Download(null, null, null);
					d.status = sts[rnd(sts.length)];
					d.progress = (rnd(5) == 0) ? -(rnd(500) / 10f) : rnd(10001) / 100f;
					DownloadList.map.put(id, d);
					break;
				}
				default: break;
				}
				float k = (rnd(4) == 0) ? 1f : DownloadNode.smoothing(rnd(200));
				float gt = rnd(3) * 0.5f, pt = rnd(2);
				n.animate(k, gt, pt);
				b.append(' ').append(hx((Float) field(n, "glow"))).append('/').append(hx((Float) field(n, "previewMix"))).append('/').append(hx(n.getShownProgress()))
					.append('/').append(n.isLoadedInLibrary() ? 'L' : '-').append(n.isDownloadActive() ? 'A' : '-').append('/').append(n.getDownloadStatus()).append('/').append(n.getProgressLabel());
			}
			line(b.toString());
			DownloadList.map.remove(id); BeatmapSetList.ids.remove(id);
		}
		for (float v : new float[]{0f, 1f, 0.001f, -0.001f, 0.0019999f, 0.002f, 0.5f})
			for (float tg : new float[]{0f, 0.001f, 0.003f, 1f, 100f}) line("EA " + hx(v) + " " + hx(tg) + " " + hx((Float) call("ease", new Class<?>[]{float.class, float.class, float.class}, v, tg, 0.3f)));
		for (int d = -50; d <= 400; d += 7) line("SM " + d + " " + hx(DownloadNode.smoothing(d)));
	}

	// ------------------------------------------------------------------ updateFit
	static void updateFit() throws Exception {
		String[][] meta = {
			{"Short Title", "", "Artist", "", "mapper"},
			{"A Very Long Song Title That Needs To Be Shortened Somewhere", "", "An Artist With A Long Name Too", "", "someone with a very long username"},
			{"Romaji Title", "日本語のタイトルとても長いタイトル", "Romaji Artist", "アーティスト", "m"},
			{"", "", "", "", ""},
			{"x", "", "café éé", "", "é"}};
		double[][] stars = {null, {}, {1.5}, {1.5, 2.25, 6.789}, {0.005, 10.0}};
		for (int round = 0; round < 30; round++) {
			String[] m = meta[rnd(meta.length)];
			DownloadNode n = new DownloadNode(round, "d", m[0], m[1], m[2], m[3], m[4]);
			Method uf = DownloadNode.class.getDeclaredMethod("updateFit", float.class); uf.setAccessible(true);
			StringBuilder b = new StringBuilder("UF " + round + ":");
			for (int step = 0; step < 14; step++) {
				switch (rnd(5)) {
				case 0: Options.unicode = !Options.unicode; break;
				case 1: n.setStars(stars[rnd(stars.length)]); break;
				default: break;
				}
				Fonts.glyphLog.clear();
				float w = (rnd(6) == 0) ? 150f : (rnd(900) + 20) / 2f;
				uf.invoke(n, w);
				uf.invoke(n, w);   // second call: cached (no glyph load, same result)
				b.append(" w=").append(hx(w)).append(" ").append(Options.unicode ? 'U' : 'n').append(' ').append(u8((String) field(n, "fTitle"))).append('|').append(u8((String) field(n, "fArtist")))
					.append('|').append(u8((String) field(n, "fMapper"))).append('|').append(hx((Float) field(n, "chipW"))).append('|').append(Fonts.glyphLog.size());
				for (String g : Fonts.glyphLog) b.append('|').append(u8(g));
			}
			line(b.toString());
			Options.unicode = false;
		}
	}

	public static void main(String[] args) throws Exception {
		layout(); fit(); icon(); animate(); updateFit();
		java.nio.file.Files.write(java.nio.file.Paths.get(args[0]), out.toString().getBytes(StandardCharsets.UTF_8));
		System.out.println("wrote " + args[0] + " (" + out.length() + " chars)");
	}
}
