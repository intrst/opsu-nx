import fluddokt.opsu.fake.Color;
import fluddokt.opsu.fake.Log;
import itdelatrisu.opsu.GameMod;
import itdelatrisu.opsu.beatmap.Beatmap;
import itdelatrisu.opsu.beatmap.BeatmapHPDropRateCalculator;
import itdelatrisu.opsu.beatmap.HitObject;
import itdelatrisu.opsu.beatmap.TimingPoint;
import itdelatrisu.opsu.options.Options;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;

/** Third golden suite (see gen3.sh): Beatmap + BeatmapHPDropRateCalculator. Mirrored line for line by tests/test_golden3.cpp. */
public class Golden3 {
	static StringBuilder out = new StringBuilder();
	static long seed = 424242424242L;
	static int rnd() { seed = seed * 6364136223846793005L + 1442695040888963407L; return (int) (seed >>> 33); }
	static int rnd(int n) { return rnd() % n; }
	static String hx(float f) { return String.format("%08x", Float.floatToIntBits(f)); }
	static String hx(double d) { return String.format("%016x", Double.doubleToLongBits(d)); }
	static void line(String s) { out.append(s).append('\n'); }
	static String ex(Throwable t) {
		if (t instanceof NumberFormatException) return "NFE";
		if (t instanceof IndexOutOfBoundsException) return "OOB";
		return "EX";
	}
	static Beatmap bm() { return new Beatmap(new fluddokt.opsu.fake.File("x.osu")); }
	static String nn(String s) { return s == null ? "null" : "'" + s + "'"; }

	// ------------------------------------------------------------------ Color (getRed truncates!)
	static void colors() {
		for (int v = 0; v < 256; v++) {
			Color c = new Color(v, 255 - v, (v * 7) % 256);
			line("COLOR " + v + " " + c.getRed() + " " + c.getGreen() + " " + c.getBlue() + " " + hx(c.r) + " " + hx(c.g) + " " + hx(c.b) + " " + hx(c.a));
		}
		for (int i = 0; i < 64; i++) {
			int r = rnd(256); int g = rnd(256); int b = rnd(256); int a = rnd(256);
			Color c = new Color(r, g, b, a);
			Color d = new Color(c);
			line("COLORA " + r + " " + g + " " + b + " " + a + " " + c.getRed() + " " + c.getGreen() + " " + c.getBlue() + " " + c.getAlpha() + " " + hx(d.a));
		}
		// colours built from arbitrary floats: this is where truncation ((int)(r * 255)) differs from rounding
		for (int i = 0; i < 64; i++) {
			int ri = rnd(1001); int gi = rnd(1001); int bi = rnd(1001);
			Color c = new Color(ri / 1000f, gi / 1000f, bi / 1000f);
			line("COLORF " + ri + " " + gi + " " + bi + " " + c.getRed() + " " + c.getGreen() + " " + c.getBlue() + " " + hx(c.a));
		}
	}

	// ------------------------------------------------------------------ breaks
	static void breaks() {
		String[] fixed = { null, "", "5", "1,2,3,4", "1,,2", " 7", "1,2,", ",", "a", "2147483648", "-5,6", "0", "00012,+3" };
		for (int i = 0; i < fixed.length; i++) {
			Beatmap b = bm();
			String r;
			try {
				b.breaksFromString(fixed[i]);
				r = "ok";
			} catch (Throwable t) { r = ex(t); }
			line("BRK " + i + " " + r + " " + nn(b.breaks == null ? null : b.breaksToString()));
		}
		for (int i = 0; i < 40; i++) {
			Beatmap b = bm();
			int n = rnd(9);
			b.breaks = new ArrayList<Integer>();
			for (int j = 0; j < n; j++) { int v = rnd(100000) - 20000; b.breaks.add(v); }
			String s = b.breaksToString();
			Beatmap b2 = bm();
			String r;
			try { b2.breaksFromString(s); r = "ok"; } catch (Throwable t) { r = ex(t); }
			line("BRKR " + i + " " + nn(s) + " " + r + " " + nn(b2.breaks == null ? null : b2.breaksToString()));
		}
		Beatmap n = bm();
		line("BRKNULL " + nn(n.breaksToString()));
	}

	// ------------------------------------------------------------------ timing points
	static void tpLine(String tag, int i, String s) {
		Beatmap b = bm();
		int w0 = Log.warnCount;
		String r;
		try { b.timingPointsFromString(s); r = "ok"; } catch (Throwable t) { r = ex(t); }
		StringBuilder sb = new StringBuilder();
		if (b.timingPoints != null)
			for (TimingPoint p : b.timingPoints) sb.append(p.toString()).append('#');
		line(tag + " " + i + " " + r + " warns=" + (Log.warnCount - w0) + " n=" + (b.timingPoints == null ? -1 : b.timingPoints.size()) + " " + sb + " " + nn(b.timingPointsToString()));
	}
	static void timing() {
		String[] fixed = { null, "", "0,1000.0,4,1,0,100,0,0", "70382,-66.6666666666667,4,2,1,70,0,1|1000,500,4,1,0,100,1,0",
			"abc|1,2,3", "5", "1.5,300,4,1,0,100,0,1|", "|", "0,500,4,1,0,100,0,0|garbage|10,-100,3,2,1,50,0,0", "12,0,4,1,0,100,0,0",
			"7,250.25,6,3,2,80,1,1" };
		for (int i = 0; i < fixed.length; i++) tpLine("TP", i, fixed[i]);
		for (int i = 0; i < 40; i++) {
			int k = 1 + rnd(4);
			StringBuilder sb = new StringBuilder();
			for (int j = 0; j < k; j++) {
				int t = rnd(300000);
				int neg = rnd(2);
				int whole = 20 + rnd(700);
				int frac = rnd(1000);
				int meter = 2 + rnd(6);
				int st = rnd(4);
				int sc = rnd(3);
				int vol = rnd(101);
				int kiai = rnd(2);
				if (j > 0) sb.append('|');
				sb.append(t).append(',').append(neg == 0 ? "" : "-").append(whole).append('.').append(frac).append(',')
					.append(meter).append(',').append(st).append(',').append(sc).append(',').append(vol).append(',').append(neg).append(',').append(kiai);
			}
			tpLine("TPR", i, sb.toString());
		}
	}

	// ------------------------------------------------------------------ combo / slider border colours
	static String colorBits(Color c) { return hx(c.r) + "/" + hx(c.g) + "/" + hx(c.b) + "/" + hx(c.a); }
	static void combo() {
		String[] fixed = { null, "255,0,0|0,255,0", "1,2", "", "1,2,3|", "a,b,c", " 1,2,3", "300,-5,7|1,2,3", "10,20,30|40,50", "1,2,3|a,b,c" };
		for (int i = 0; i < fixed.length; i++) {
			Beatmap b = bm();
			b.combo = new Color[] { new Color(11, 22, 33) };
			String r;
			try { b.comboFromString(fixed[i]); r = "ok"; } catch (Throwable t) { r = ex(t); }
			line("CMB " + i + " " + r + " " + b.combo.length + " " + nn(b.comboToString()) + " " + colorBits(b.combo[0]));
		}
		for (int i = 0; i < 60; i++) {
			int k = 1 + rnd(5);
			StringBuilder sb = new StringBuilder();
			for (int j = 0; j < k; j++) {
				int r = rnd(256); int g = rnd(256); int bl = rnd(256);
				if (j > 0) sb.append('|');
				sb.append(r).append(',').append(g).append(',').append(bl);
			}
			Beatmap b = bm();
			b.comboFromString(sb.toString());
			line("CMBR " + i + " " + sb + " " + b.comboToString() + " " + colorBits(b.combo[0]) + " " + colorBits(b.combo[b.combo.length - 1]));
		}
		Beatmap nul = bm();
		line("CMBNULL " + nn(nul.comboToString()) + " " + nn(nul.sliderBorderToString()));
	}
	static void sliderBorder() {
		String[] fixed = { null, "1,2,3", "1,2", "x", "", "255,255,255", "0,0,0", "300,-1,9" };
		for (int i = 0; i < fixed.length; i++) {
			Beatmap b = bm();
			b.sliderBorder = new Color(5, 6, 7);
			String r;
			try { b.sliderBorderFromString(fixed[i]); r = "ok"; } catch (Throwable t) { r = ex(t); }
			line("SB " + i + " " + r + " " + nn(b.sliderBorderToString()) + " " + colorBits(b.sliderBorder));
		}
		for (int i = 0; i < 40; i++) {
			int r = rnd(256); int g = rnd(256); int bl = rnd(256);
			Beatmap b = bm();
			b.sliderBorderFromString(r + "," + g + "," + bl);
			line("SBR " + i + " " + r + "," + g + "," + bl + " " + b.sliderBorderToString() + " " + colorBits(b.sliderBorder));
		}
	}

	// ------------------------------------------------------------------ ordering, text, accessors
	static void order() {
		float[] ods = { 0f, -0f, 5f, 7.5f, Float.NaN, 10f, 5f, -1f };
		int[] cnt = { 0, 1, 100, 2000000000, 2147483647, -5 };
		for (int i = 0; i < 300; i++) {
			Beatmap a = bm(); Beatmap b = bm();
			a.overallDifficulty = ods[rnd(ods.length)];
			b.overallDifficulty = ods[rnd(ods.length)];
			a.hitObjectCircle = cnt[rnd(cnt.length)]; a.hitObjectSlider = cnt[rnd(cnt.length)]; a.hitObjectSpinner = cnt[rnd(cnt.length)];
			b.hitObjectCircle = cnt[rnd(cnt.length)]; b.hitObjectSlider = cnt[rnd(cnt.length)]; b.hitObjectSpinner = cnt[rnd(cnt.length)];
			line("CMP " + i + " " + hx(a.overallDifficulty) + " " + hx(b.overallDifficulty) + " " + a.compareTo(b) + " " + b.compareTo(a) + " " + a.compareTo(a));
		}
		String[] ts = { "", "Title", "\u65e5\u672c\u8a9e", "t\u00e9st" };
		String[] srcs = { "", "Src" };
		for (int i = 0; i < 80; i++) {
			Beatmap b = bm();
			b.title = ts[rnd(ts.length)]; b.titleUnicode = ts[rnd(ts.length)];
			b.artist = ts[rnd(ts.length)]; b.artistUnicode = ts[rnd(ts.length)];
			b.source = srcs[rnd(srcs.length)];
			b.version = ts[rnd(ts.length)];
			Options.unicode = false;
			String s0 = b.toString(); String t0 = b.getTitle(); String a0 = b.getArtist();
			Options.unicode = true;
			String s1 = b.toString(); String t1 = b.getTitle(); String a1 = b.getArtist();
			Options.unicode = false;
			line("STR " + i + " " + nn(s0) + " " + nn(t0) + " " + nn(a0) + " " + nn(s1) + " " + nn(t1) + " " + nn(a1));
		}
		// skin fallbacks
		Beatmap b = bm();
		Color[] sc = b.getComboColors();
		line("SKIN " + sc.length + " " + colorBits(sc[0]) + " " + colorBits(sc[1]) + " " + colorBits(b.getSliderBorderColor()));
		b.combo = new Color[] { new Color(12, 34, 56) };
		b.sliderBorder = new Color(65, 43, 21);
		sc = b.getComboColors();
		line("OWN " + sc.length + " " + colorBits(sc[0]) + " " + colorBits(b.getSliderBorderColor()));
		// play counter / additional fields
		Beatmap src = bm();
		src.starRating = 4.25; src.dateAdded = 1234567890123L; src.favorite = true; src.playCount = 7; src.lastPlayed = 99; src.localMusicOffset = -12;
		Beatmap dst = bm();
		src.copyAdditionalFields(dst);
		line("COPY " + hx(dst.starRating) + " " + dst.dateAdded + " " + dst.favorite + " " + dst.playCount + " " + dst.lastPlayed + " " + dst.localMusicOffset);
		long before = System.currentTimeMillis();
		dst.incrementPlayCounter();
		long after = System.currentTimeMillis();
		line("PLAY " + dst.playCount + " " + (dst.lastPlayed >= before && dst.lastPlayed <= after));
	}

	// ------------------------------------------------------------------ mod params / mod stars
	static void mods() {
		float[] fs = { 0f, 0.5f, 1f, 1.25f, 1.5f, 2f, -1f };
		float[] cs = { 0f, 4.2f, 7f, 11f, -2f };
		float[] bcs = { 0f, 2.5f, 4f, 5f, 7.3f, 9.9f, 10f };
		for (int i = 0; i < 400; i++) {
			int st = rnd(1 << 13);
			GameMod.loadModState(st);
			Options.fixedSpeed = fs[rnd(fs.length)];
			Options.fixedCS = cs[rnd(cs.length)];
			Beatmap b = bm();
			b.circleSize = bcs[rnd(bcs.length)];
			int sr = rnd(1000);
			b.starRating = 1 + sr / 100.0;
			Beatmap.ModParams p = Beatmap.currentModParams();
			line("MOD " + i + " " + st + " " + p.key + " " + hx(p.clockRate) + " " + hx(p.circleSize(b)) + " " + p.isNoMod(b) + " " + b.needsModStars(p) + " " + hx(b.getStarRating()));
			b.setModStars(p, 6.25);
			line("MOD2 " + i + " " + b.needsModStars(p) + " " + hx(b.getStarRating()));
			int st2 = rnd(1 << 13);
			GameMod.loadModState(st2);
			Beatmap.ModParams q = Beatmap.currentModParams();
			b.setModStars(p, 7.5);   // stale unless the key is unchanged
			line("MOD3 " + i + " " + st2 + " " + (p.key == q.key) + " " + b.needsModStars(q) + " " + hx(b.getStarRating()));
			GameMod.loadModState(st);   // back to the first mods: was the stale result really dropped?
			line("MOD5 " + i + " " + b.needsModStars(p) + " " + hx(b.getStarRating()));
			GameMod.loadModState(st2);
			b.setModStars(q, 8.5);
			line("MOD4 " + i + " " + b.needsModStars(q) + " " + hx(b.getStarRating()));
		}
		GameMod.loadModState(0);
		Options.fixedSpeed = 0f;
		Options.fixedCS = 0f;
	}

	// ------------------------------------------------------------------ HP drop rate
	static void hpCase(String tag, int t, Beatmap b, float hpd, float od) {
		try {
			BeatmapHPDropRateCalculator c = new BeatmapHPDropRateCalculator(b, hpd, od);
			c.calculate();
			line(tag + " " + t + " ok " + hx(c.getHpDropRate()) + " " + hx(c.getHpMultiplierNormal()) + " " + hx(c.getHpMultiplierComboEnd()));
		} catch (Throwable e) {
			line(tag + " " + t + " " + ex(e));
		}
	}
	static void hp() {
		float[] ticks = { 1f, 2f, 0.5f, 3f, 4f };
		for (int t = 0; t < 150; t++) {
			int n = 5 + rnd(100);
			int time = 500 + rnd(4000);
			ArrayList<HitObject> list = new ArrayList<HitObject>();
			ArrayList<Integer> br = new ArrayList<Integer>();
			for (int i = 0; i < n; i++) {
				int kind = rnd(10);
				int r4 = rnd(4);
				int nc = (r4 == 0 || i == 0) ? 4 : 0;
				int x = rnd(512); int y = rnd(384);
				String ln; int end;
				if (kind < 7) {
					ln = x + "," + y + "," + time + "," + (1 | nc) + ",0";
					end = time;
				} else if (kind < 9) {
					int rep = 1 + rnd(3); int len = 50 + rnd(350); int cx = rnd(512); int cy = rnd(384);
					ln = x + "," + y + "," + time + "," + (2 | nc) + ",0,B|" + cx + ":" + cy + "," + rep + "," + len;
					end = time + rep * len * 3;
				} else {
					int dur = 800 + rnd(3000);
					ln = "256,192," + time + "," + (8 | nc) + ",0," + (time + dur);
					end = time + dur;
				}
				list.add(new HitObject(ln));
				int g1 = 80 + rnd(900);
				int r12 = rnd(12);
				if (r12 == 0) {
					int big = 3000 + rnd(4000);
					br.add(end + 500); br.add(end + big - 500);
					g1 = big;
				}
				time = end + g1;
			}
			int tpn = 1 + rnd(6);
			ArrayList<TimingPoint> tps = new ArrayList<TimingPoint>();
			int bw = 300 + rnd(500); int bf = rnd(10);
			tps.add(new TimingPoint("0," + bw + "." + bf + ",4,1,0,100,0,0"));
			int tt = 0;
			for (int j = 1; j < tpn; j++) {
				tt += 2000 + rnd(15000);
				int kindTp = rnd(2);
				if (kindTp == 0) {
					int w2 = 300 + rnd(500); int f2 = rnd(10);
					tps.add(new TimingPoint(tt + "," + w2 + "." + f2 + ",4,1,0,100,0,0"));
				} else {
					int v = 25 + rnd(200);
					tps.add(new TimingPoint(tt + ",-" + v + ".5,4,1,0,100,0,0"));
				}
			}
			Beatmap b = bm();
			b.HPDrainRate = rnd(101) / 10f;
			b.overallDifficulty = rnd(101) / 10f;
			b.sliderMultiplier = 1f + rnd(26) / 10f;
			b.sliderTickRate = ticks[rnd(ticks.length)];
			int rb = rnd(2);
			b.objects = list.toArray(new HitObject[0]);
			b.timingPoints = tps;
			b.breaks = (br.isEmpty() && rb == 0) ? null : br;
			float hpd = rnd(101) / 10f;
			float od = rnd(101) / 10f;
			hpCase("HP", t, b, hpd, od);
			line("HPIN " + t + " " + n + " " + tpn + " " + br.size() + " " + hx(hpd) + " " + hx(od) + " " + hx(b.sliderMultiplier) + " " + hx(b.sliderTickRate) + " " + hx(b.overallDifficulty));
		}
		// long runs of single-slider combos with high drain. A circle-only map can never reach the "3 combo ends too
		// low" branch (a combo-ending circle gains 20 HP, as much as the gap between the two thresholds at drain 10);
		// sliders can, because their duration drain is applied after the "ever" check and before the hit gains.
		for (int t = 0; t < 60; t++) {
			int n = 40 + rnd(60);
			int gap = 150 + rnd(600);
			int time = 1000;
			ArrayList<HitObject> list = new ArrayList<HitObject>();
			for (int i = 0; i < n; i++) {
				int x = rnd(512); int y = rnd(384); int cx = rnd(512); int cy = rnd(384);
				int rep = 1 + rnd(2); int len = 200 + rnd(150);
				list.add(new HitObject(x + "," + y + "," + time + ",6,0,B|" + cx + ":" + cy + "," + rep + "," + len));
				time += rep * len * 4 + gap;
			}
			Beatmap b = bm();
			b.objects = list.toArray(new HitObject[0]);
			b.timingPoints = new ArrayList<TimingPoint>();
			b.timingPoints.add(new TimingPoint("0,500.0,4,1,0,100,0,0"));
			int hd = rnd(21);
			int od = rnd(101);
			float hpd = 8f + hd / 10f;
			float odf = od / 10f;
			hpCase("HPC", t, b, hpd, odf);
			line("HPCIN " + t + " " + n + " " + gap + " " + hx(hpd) + " " + hx(odf));
		}
		// error handling, in the order the real code fails
		Beatmap e1 = bm();                                            // objects null
		e1.timingPoints = new ArrayList<TimingPoint>();
		hpCase("HPERR", 1, e1, 5f, 5f);
		Beatmap e2 = bm();                                            // objects empty, timing points null -> objects[0] fails first
		e2.objects = new HitObject[0];
		hpCase("HPERR", 2, e2, 5f, 5f);
		Beatmap e3 = bm();                                            // timing points null
		e3.objects = new HitObject[] { new HitObject("1,2,1000,1,0"), new HitObject("1,2,2000,1,0") };
		hpCase("HPERR", 3, e3, 5f, 5f);
		Beatmap e4 = bm();                                            // odd break list
		e4.objects = new HitObject[] { new HitObject("1,2,1000,5,0"), new HitObject("1,2,2000,1,0"), new HitObject("1,2,3000,5,0") };
		e4.timingPoints = new ArrayList<TimingPoint>();
		e4.timingPoints.add(new TimingPoint("0,500,4,1,0,100,0,0"));
		e4.breaks = new ArrayList<Integer>();
		e4.breaks.add(100000000); e4.breaks.add(200000000); e4.breaks.add(5);
		hpCase("HPERR", 4, e4, 5f, 5f);
		Beatmap e5 = bm();                                            // no timing points at all: beat length stays 1
		e5.objects = new HitObject[] { new HitObject("1,2,1000,5,0"), new HitObject("1,2,2000,1,0"), new HitObject("1,2,3000,5,0") };
		e5.timingPoints = new ArrayList<TimingPoint>();
		hpCase("HPERR", 5, e5, 5f, 5f);
	}

	public static void main(String[] args) throws Exception {
		colors(); breaks(); timing(); combo(); sliderBorder(); order(); mods(); hp();
		java.nio.file.Files.write(java.nio.file.Paths.get(args[0]), out.toString().getBytes(StandardCharsets.UTF_8));
		System.out.println("wrote " + args[0] + " (" + out.length() + " chars)");
	}
}
