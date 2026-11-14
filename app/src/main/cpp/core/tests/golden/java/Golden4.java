import fluddokt.opsu.fake.Log;
import itdelatrisu.opsu.beatmap.Beatmap;
import itdelatrisu.opsu.beatmap.BeatmapDifficultyCalculator;
import itdelatrisu.opsu.beatmap.HitObject;
import itdelatrisu.opsu.beatmap.PerformanceCalculator;
import itdelatrisu.opsu.beatmap.TimingPoint;
import itdelatrisu.opsu.options.Options;

import java.util.ArrayList;

/** Fourth golden suite (see gen4.sh): BeatmapDifficultyCalculator + PerformanceCalculator. Mirrored line for line by tests/test_golden4.cpp. */
public class Golden4 {
	static StringBuilder out = new StringBuilder();
	static long seed = 777000777L;
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
	static String logs() { return "E" + Log.errorCount + " W" + Log.warnCount; }

	static final int[][] CFG = { { 640, 480, 10 }, { 1920, 1080, 10 }, { 1280, 720, 16 }, { 800, 600, 10 }, { 1080, 2400, 10 }, { 512, 384, 13 } };
	static void initCfg(int[] c) { Options.playfieldScale = c[2] / 10f; HitObject.init(c[0], c[1]); }

	/** kind: 0 mixed, 1 streams (dense circles), 2 slider heavy. */
	static Beatmap genMap(int kind) {
		int n = 2 + rnd(kind == 2 ? 40 : 70);
		int time = 200 + rnd(3000);
		int sliderPct = (kind == 2) ? 70 : (kind == 1 ? 5 : 30);
		int spinnerPct = (kind == 1) ? 0 : 6;
		String types = "BLPC";
		ArrayList<HitObject> list = new ArrayList<HitObject>();
		for (int i = 0; i < n; i++) {
			int k = rnd(100);
			int x = rnd(513); int y = rnd(385);
			String ln; int end;
			if (k < sliderPct) {
				char t = types.charAt(rnd(4));
				int pts = (t == 'P') ? 2 : 1 + rnd(4);
				int rep = 1 + rnd(4);
				int len = 20 + rnd(480); int dec = rnd(10);
				StringBuilder sb = new StringBuilder();
				sb.append(x).append(',').append(y).append(',').append(time).append(",2,0,").append(t);
				for (int p = 0; p < pts; p++) { int cx = rnd(513); int cy = rnd(385); sb.append('|').append(cx).append(':').append(cy); }
				sb.append(',').append(rep).append(',').append(len).append('.').append(dec);
				ln = sb.toString();
				end = time + rep * len * 2;
			} else if (k < sliderPct + spinnerPct) {
				int dur = 500 + rnd(2500);
				ln = "256,192," + time + ",8,0," + (time + dur);
				end = time + dur;
			} else {
				ln = x + "," + y + "," + time + ",1,0";
				end = time;
			}
			list.add(new HitObject(ln));
			int gap;
			if (kind == 1) gap = 40 + rnd(120);
			else if (rnd(10) == 0) gap = rnd(60);
			else gap = 100 + rnd(900);
			time = end + gap;
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
		b.circleSize = rnd(101) / 10f;
		b.sliderMultiplier = 1f + rnd(26) / 10f;
		b.overallDifficulty = rnd(101) / 10f;
		int arSel = rnd(3);
		int arVal = rnd(101);
		b.approachRate = (arSel == 0) ? -1f : arVal / 10f;
		b.objects = list.toArray(new HitObject[0]);
		b.timingPoints = tps;
		return b;
	}

	static void diffCase(String tag, int t, Beatmap b, boolean two, float cs, float clock) {
		try {
			BeatmapDifficultyCalculator c = two ? new BeatmapDifficultyCalculator(b, cs, clock) : new BeatmapDifficultyCalculator(b);
			c.calculate();
			line(tag + " " + t + " ok " + hx(c.getStarRating()) + " " + hx(c.getDifficulty(0)) + " " + hx(c.getDifficulty(1)) + " "
				+ hx(c.getStars(0)) + " " + hx(c.getStars(1)) + " " + logs());
		} catch (Throwable e) {
			line(tag + " " + t + " " + ex(e) + " " + logs());
		}
	}

	static void diff() {
		float[] css = { 0f, 2.5f, 4f, 5f, 7f, 9.5f, 10f, 12.142858f, -3f };
		float[] clocks = { 1f, 1.5f, 0.75f, 0f, -1f, 2f, 1.3333334f, Float.NaN };
		// before any HitObject.init(): the static multipliers are still 0
		for (int t = 0; t < 6; t++) {
			Beatmap b = genMap(t % 3);
			diffCase("DIFF0", t, b, false, 0f, 0f);
		}
		for (int t = 0; t < 180; t++) {
			int kind = t % 3;
			int[] cfg = CFG[rnd(CFG.length)];
			initCfg(cfg);
			Beatmap b = genMap(kind);
			line("DIFFIN " + t + " " + b.objects.length + " " + b.timingPoints.size() + " " + cfg[0] + "x" + cfg[1] + "@" + cfg[2] + " "
				+ hx(b.circleSize) + " " + hx(b.sliderMultiplier));
			diffCase("DIFF", t, b, false, 0f, 0f);
			int ci = rnd(css.length); int ki = rnd(clocks.length);
			diffCase("DIFF2", t, b, true, css[ci], clocks[ki]);
		}
		// getDifficulty / getStars with a bad type
		initCfg(CFG[1]);
		Beatmap g = genMap(0);
		try {
			BeatmapDifficultyCalculator c = new BeatmapDifficultyCalculator(g);
			c.calculate();
			String r = "";
			try { c.getStars(2); r = "noex"; } catch (Throwable e) { r = ex(e); }
			String r2 = "";
			try { c.getDifficulty(-1); r2 = "noex"; } catch (Throwable e) { r2 = ex(e); }
			line("DIFFTYPE " + r + " " + r2);
		} catch (Throwable e) { line("DIFFTYPE " + ex(e)); }

		// error / edge cases
		Beatmap e1 = bm(); e1.timingPoints = new ArrayList<TimingPoint>(); e1.timingPoints.add(new TimingPoint("0,500,4,1,0,100,0,0"));
		diffCase("DIFFERR", 1, e1, false, 0f, 0f);                         // objects not loaded
		Beatmap e2 = bm(); e2.objects = new HitObject[] { new HitObject("10,10,100,1,0") };
		diffCase("DIFFERR", 2, e2, false, 0f, 0f);                         // timing points not loaded
		Beatmap e3 = bm(); e3.objects = new HitObject[0]; e3.timingPoints = new ArrayList<TimingPoint>();
		diffCase("DIFFERR", 3, e3, false, 0f, 0f);                         // empty map
		Beatmap e4 = bm(); e4.objects = new HitObject[] { new HitObject("10,10,100,1,0") }; e4.timingPoints = new ArrayList<TimingPoint>();
		diffCase("DIFFERR", 4, e4, false, 0f, 0f);                         // single object, no timing points
		Beatmap e5 = bm(); e5.objects = new HitObject[] { new HitObject("10,10,100,1,0"), new HitObject("200,100,100,1,0"), new HitObject("300,300,100,1,0") };
		e5.timingPoints = new ArrayList<TimingPoint>(); e5.timingPoints.add(new TimingPoint("0,-100,4,1,0,100,1,0"));
		diffCase("DIFFERR", 5, e5, false, 0f, 0f);                         // all at the same time, first timing point inherited
		Beatmap e6 = bm(); e6.objects = new HitObject[] { new HitObject("10,10,100,1,0"), new HitObject("100,100,300,2,0,B,1,100") };
		e6.timingPoints = new ArrayList<TimingPoint>(); e6.timingPoints.add(new TimingPoint("0,500,4,1,0,100,0,0"));
		diffCase("DIFFERR", 6, e6, false, 0f, 0f);                         // slider without control points: curve construction fails
		Beatmap e7 = bm(); e7.objects = new HitObject[] { new HitObject("10,10,100,1,0"), new HitObject("100,100,300,2,0,L|150:150,1,0") };
		e7.timingPoints = new ArrayList<TimingPoint>(); e7.timingPoints.add(new TimingPoint("0,500,4,1,0,100,0,0"));
		diffCase("DIFFERR", 7, e7, false, 0f, 0f);                         // zero-length slider
		Beatmap e8 = bm(); e8.objects = new HitObject[] { new HitObject("10,10,100,1,0"), new HitObject("100,100,300,2,0,L|150:150,1,-50") };
		e8.timingPoints = new ArrayList<TimingPoint>(); e8.timingPoints.add(new TimingPoint("0,500,4,1,0,100,0,0"));
		diffCase("DIFFERR", 8, e8, false, 0f, 0f);                         // negative slider length
		// timing point after all objects / exactly at an object time / inherited before any uninherited one
		Beatmap e9 = bm();
		e9.objects = new HitObject[] { new HitObject("10,10,100,2,0,B|60:60|90:20,2,150.5"), new HitObject("200,100,1000,2,0,L|250:100,3,120"), new HitObject("300,300,2000,1,0") };
		e9.timingPoints = new ArrayList<TimingPoint>();
		e9.timingPoints.add(new TimingPoint("0,-50,4,1,0,100,1,0")); e9.timingPoints.add(new TimingPoint("100,400,4,1,0,100,0,0"));
		e9.timingPoints.add(new TimingPoint("1000,-200,4,1,0,100,1,0")); e9.timingPoints.add(new TimingPoint("9000,300,4,1,0,100,0,0"));
		diffCase("DIFFERR", 9, e9, false, 0f, 0f);

		// timing-point boundary: an inherited x10 point exactly at (or just after) a slider's time moves sliderTime across the
		// "at least 2 ms" threshold that makes the second-segment lazy movement run; the trailing circle puts the effect in a
		// completed strain interval (the last interval is never added to highestStrains)
		initCfg(CFG[1]);
		for (int v = 0; v < 4; v++) {
			int inhTime = (v % 2 == 0) ? 1000 : 1001;
			int rep = (v < 2) ? 2 : 3;
			Beatmap e = bm();
			e.objects = new HitObject[] { new HitObject("10,10,100,1,0"), new HitObject("100,100,1000,2,0,L|400:100," + rep + ",50"),
				new HitObject("300,300,1500,1,0"), new HitObject("50,50,2500,1,0") };
			e.timingPoints = new ArrayList<TimingPoint>();
			e.timingPoints.add(new TimingPoint("0,10,4,1,0,100,0,0"));
			e.timingPoints.add(new TimingPoint(inhTime + ",-1000,4,1,0,100,1,0"));
			e.sliderMultiplier = 3.5f;
			diffCase("DIFFTP", v, e, false, 0f, 0f);
		}
	}

	static void perf() {
		for (int t = 0; t < 160; t++) {
			int kind = t % 3;
			int[] cfg = CFG[rnd(CFG.length)];
			initCfg(cfg);
			Beatmap b = genMap(kind);
			int h300 = rnd(3000); int h100 = rnd(300); int h50 = rnd(60); int miss = rnd(40);
			int zero = rnd(15);
			if (zero == 0) { h300 = 0; h100 = 0; h50 = 0; miss = 0; }
			int combo = rnd(h300 + h100 + h50 + miss + 1);
			int mcSel = rnd(4); int mcAdd = rnd(200);
			int maxCombo = (mcSel == 0) ? 0 : combo + mcAdd;
			int mods = rnd(16384);
			PerformanceCalculator.Result r = PerformanceCalculator.calculate(b, h300, h100, h50, miss, combo, maxCombo, mods);
			line("PERF " + t + " " + b.objects.length + " " + h300 + " " + h100 + " " + h50 + " " + miss + " " + combo + " " + maxCombo + " " + mods + " "
				+ hx(b.overallDifficulty) + " " + hx(b.approachRate) + " | "
				+ hx(r.aim) + " " + hx(r.speed) + " " + hx(r.accuracy) + " " + hx(r.reading) + " " + hx(r.total) + " "
				+ hx(r.maxAim) + " " + hx(r.maxSpeed) + " " + hx(r.maxAccuracy) + " " + hx(r.maxReading) + " " + hx(r.maxTotal) + " "
				+ hx(r.aimRatio()) + " " + hx(r.speedRatio()) + " " + hx(r.accuracyRatio()) + " " + hx(r.readingRatio()) + " " + logs());
		}
		// ratio() corner cases
		double[][] rc = { { 1, 2 }, { 3, 2 }, { -1, 2 }, { 0, 0 }, { 5, -1 }, { Double.NaN, 1 }, { 1, Double.NaN }, { Double.POSITIVE_INFINITY, 1 }, { 0, 1e-300 }, { 0.5, 1 } };
		for (int i = 0; i < rc.length; i++) line("RATIO " + i + " " + hx(PerformanceCalculator.Result.ratio(rc[i][0], rc[i][1])));
		// error / edge cases
		Beatmap e1 = bm(); e1.timingPoints = new ArrayList<TimingPoint>(); e1.timingPoints.add(new TimingPoint("0,500,4,1,0,100,0,0"));
		PerformanceCalculator.Result r1 = PerformanceCalculator.calculate(e1, 100, 0, 0, 0, 100, 100, 0);
		line("PERFERR 1 " + hx(r1.total) + " " + hx(r1.maxTotal) + " " + logs());   // objects not loaded
		Beatmap e2 = bm(); e2.objects = new HitObject[] { new HitObject("10,10,100,1,0"), new HitObject("100,100,300,2,0,B,1,100") };
		e2.timingPoints = new ArrayList<TimingPoint>(); e2.timingPoints.add(new TimingPoint("0,500,4,1,0,100,0,0"));
		PerformanceCalculator.Result r2 = PerformanceCalculator.calculate(e2, 100, 0, 0, 0, 100, 100, 0);
		line("PERFERR 2 " + hx(r2.total) + " " + hx(r2.maxTotal) + " " + logs());   // curve failure is caught
		Beatmap e3 = genMap(0);
		PerformanceCalculator.Result r3 = PerformanceCalculator.calculate(e3, 0, 0, 0, 0, 0, 0, 0);
		line("PERFERR 3 " + hx(r3.total) + " " + hx(r3.maxTotal) + " " + logs());   // no hits at all
		Beatmap e4 = genMap(0);
		PerformanceCalculator.Result r4 = PerformanceCalculator.calculate(e4, Integer.MAX_VALUE, 5, 0, 0, 1, 1, 0);
		line("PERFERR 4 " + hx(r4.total) + " " + hx(r4.maxTotal) + " " + logs());   // totalHits overflows to a negative int
	}

	public static void main(String[] args) throws Exception {
		diff(); perf();
		java.nio.file.Files.write(java.nio.file.Paths.get(args[0]), out.toString().getBytes("UTF-8"));
	}
}
