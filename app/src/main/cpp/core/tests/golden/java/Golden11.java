package fourresonace;

import fourresonace.PjskChart.BpmSegment;
import fourresonace.PjskChart.Note;
import fourresonace.PjskChart.TailMode;
import fourresonace.PjskChart.TimeScaleChange;

import java.io.StringReader;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/**
 * Golden suite 11 (see gen11.sh): the REAL OsuToPjsk / PjskChart. It lives in the Java package so it can reach the
 * package-private LaneLayout. Every case is written to a corpus file (cases11.txt) in the order it runs, next to the
 * expected output (expected11.txt); tests/test_golden11.cpp replays the corpus, so no generator has to be mirrored.
 */
public class Golden11 {
	static StringBuilder out = new StringBuilder(), cases = new StringBuilder();
	static long seed = 20261113L;
	static int rnd() { seed = seed * 6364136223846793005L + 1442695040888963407L; return (int) (seed >>> 33); }
	static int rnd(int n) { return rnd() % n; }
	static String hx(double d) { return String.format("%016x", Double.doubleToLongBits(d)); }
	static String hx(float f) { return String.format("%08x", Float.floatToIntBits(f)); }
	static void line(String s) { out.append(s).append('\n'); }
	static void emit(String s) { cases.append(s).append('\n'); }
	static int no = 0;

	static String hex(String s) {
		byte[] b = s.getBytes(StandardCharsets.UTF_8);
		if (b.length == 0) return "-";
		StringBuilder sb = new StringBuilder();
		for (byte x : b) sb.append(String.format("%02x", x & 0xFF));
		return sb.toString();
	}
	static String ex(Throwable t) {
		if (t instanceof NumberFormatException) return "NFE";
		if (t instanceof NegativeArraySizeException) return "NAS";
		if (t instanceof IndexOutOfBoundsException) return "OOB";
		return "EX";
	}
	static String pick(String... a) { return a[rnd(a.length)]; }

	// ------------------------------------------------------------------ String.trim / split(",", -1)
	static String show(String[] a) {
		StringBuilder sb = new StringBuilder();
		sb.append(a.length).append(':');
		for (int i = 0; i < a.length; i++) { if (i > 0) sb.append('|'); sb.append(hex(a[i])); }
		return sb.toString();
	}
	static void ts(String s) {
		int i = no++;
		emit("TS " + hex(s));
		line("TS " + i + " " + hex(s.trim()) + " " + show(s.split(",", -1)));
	}
	static void trimSplit() {
		String[] fixed = { "", ",", ",,", "a", "a,", ",a", "a,b", "a,b,", " a , b ,", "\u0000a\u0000", "\u00a0x", " \t\r\n", "x \u0001", "\u00e9,\u65e5\u672c,", "a,,b,,,", "  ,  " };
		for (String s : fixed) ts(s);
		String alpha = ", abc\t\u0001\u00e9";
		for (int i = 0; i < 200; i++) {
			int len = rnd(9);
			StringBuilder sb = new StringBuilder();
			for (int j = 0; j < len; j++) sb.append(alpha.charAt(rnd(alpha.length())));
			ts(sb.toString());
		}
	}

	// ------------------------------------------------------------------ beat / time conversion and snapping
	static String d(double v) { return hx(v); }
	static void beats() throws Exception {
		Method snap = OsuToPjsk.class.getDeclaredMethod("noteTimeMsToBeat", double.class, List.class);
		snap.setAccessible(true);
		for (int c = 0; c < 120; c++) {
			int n = rnd(5);
			if (c == 0) n = 0;
			List<BpmSegment> segs = new ArrayList<BpmSegment>();
			double start = rnd(5000) - 1000 + (rnd(3) == 0 ? 0.5 : 0.0);
			double beatAt = 0;
			double lastBl = 500;
			double lastStart = start;
			for (int i = 0; i < n; i++) {
				double bl = 250 + rnd(750) + rnd(4) / 4.0;
				if (rnd(40) == 0) bl = 0;
				if (i > 0) { beatAt += (start - lastStart) / lastBl; if (rnd(10) == 0) beatAt = rnd(100); }
				segs.add(new BpmSegment(start, beatAt, bl, bl > 0 ? 60000.0 / bl : 0));
				lastBl = bl; lastStart = start;
				start += 200 + rnd(30000);
			}
			int m = 8;
			double[] ts = new double[m];
			for (int i = 0; i < m; i++) {
				int k = rnd(8);
				if (k == 0 && !segs.isEmpty()) ts[i] = segs.get(rnd(segs.size())).startMs;
				else if (k == 1 && !segs.isEmpty()) ts[i] = segs.get(rnd(segs.size())).startMs - 1e-7;
				else if (k == 2 && !segs.isEmpty()) ts[i] = segs.get(rnd(segs.size())).startMs + 1e-7;
				else if (k == 3 && !segs.isEmpty()) { BpmSegment s = segs.get(rnd(segs.size())); ts[i] = s.startMs + s.beatLengthMs * rnd(64) / pickDiv(); }
				else if (k == 4) ts[i] = rnd(7) == 0 ? Double.NaN : (rnd(2) == 0 ? Double.POSITIVE_INFINITY : Double.NEGATIVE_INFINITY);
				else ts[i] = rnd(200000) - 2000 + rnd(1000) / 1000.0;
			}
			bt(snap, segs, ts);
		}
		// ties between equally good snap candidates need a grid of about 1 ms (very fast tempo); half-millisecond times sit exactly between two
		// grid points. The second group puts the snapping error exactly on, and one ulp either side of, the 1 ms + 1e-6 tolerance.
		for (double bl : new double[] { 4, 8, 16, 24, 32, 48 }) {
			List<BpmSegment> segs = new ArrayList<BpmSegment>();
			segs.add(new BpmSegment(0, 0, bl, 60000.0 / bl));
			double[] ts = new double[16];
			for (int i = 0; i < 16; i++) ts[i] = i * 0.5 + (bl > 30 ? 1000 : 0);
			bt(snap, segs, ts);
		}
		List<BpmSegment> edge = new ArrayList<BpmSegment>();
		edge.add(new BpmSegment(0, 0, 1000, 60));
		bt(snap, edge, new double[] { 1.0 + 1e-6, Math.nextUp(1.0 + 1e-6), Math.nextDown(1.0 + 1e-6), 1.0, 0.999999, 1.000001, 1.000002, 999.999, 998.999999, 998.99999 });
	}
	static void bt(Method snap, List<BpmSegment> segs, double[] ts) throws Exception {
		int n = segs.size();
		int m = ts.length;
		StringBuilder cs = new StringBuilder("BT " + n);
		for (BpmSegment s : segs) cs.append(' ').append(d(s.startMs)).append(' ').append(d(s.beatAtStart)).append(' ').append(d(s.beatLengthMs)).append(' ').append(d(s.bpm));
		cs.append(' ').append(m);
		for (double t : ts) cs.append(' ').append(d(t));
		emit(cs.toString());
		int i0 = no++;
		for (int i = 0; i < m; i++)
			line("BT " + i0 + " " + i + " " + d(ts[i]) + " " + d(OsuToPjsk.timeMsToBeat(ts[i], segs)) + " " + d(OsuToPjsk.beatToTimeMs(ts[i], segs))
				+ " " + d((Double) snap.invoke(null, ts[i], segs)));
	}
	static int pickDiv() { int[] a = { 1, 2, 3, 4, 6, 8, 12, 16 }; return a[rnd(a.length)]; }

	// ------------------------------------------------------------------ lane layout, columnToLaneSize, tinged columns
	static String ls(float[] f) { return hx(f[0]) + "/" + hx(f[1]); }
	static void lanes() {
		int[] keyList = { -3, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 16, 40 };
		for (int round = 0; round < 6; round++) {
			for (int keys : keyList) {
				int steps = 40;
				StringBuilder cs = new StringBuilder("LL " + keys + " " + steps);
				StringBuilder res = new StringBuilder();
				OsuToPjsk.LaneLayout layout = null;
				String ctor = "ok";
				try { layout = new OsuToPjsk.LaneLayout(keys); } catch (Throwable t) { ctor = ex(t); }
				int time = rnd(3000) - (round == 5 ? 2000000000 : 0);
				for (int s = 0; s < steps; s++) {
					int col = rnd(Math.max(keys, 1) + (rnd(30) == 0 ? 2 : 0)) - (rnd(40) == 0 ? 1 : 0);
					int jump = pick("0", "0", "5", "50", "340", "349", "350", "351", "700", "5000").length() == 0 ? 0 : Integer.parseInt(pick("0", "0", "5", "50", "340", "349", "350", "351", "700", "5000"));
					time += jump;
					int end = time + (rnd(3) == 0 ? rnd(2000) : 0);
					int special = rnd(60);
					if (special == 0) { time = Integer.MIN_VALUE / 2; end = time; }
					else if (special == 1) { time = Integer.MIN_VALUE; end = time; }
					else if (special == 2) { time = Integer.MAX_VALUE; end = time; }
					else if (special == 3) { time = Integer.MIN_VALUE / 4; end = time; }
					else if (special == 4) { time = Integer.MIN_VALUE / 4 + 1; end = time; }
					cs.append(' ').append(col).append(' ').append(time).append(' ').append(end);
					String r;
					if (layout == null) r = ctor;
					else { try { r = ls(layout.place(col, time, end)); } catch (Throwable t) { r = ex(t); } }
					res.append(' ').append(r);
				}
				emit(cs.toString());
				line("LL " + no++ + " keys=" + keys + " ctor=" + ctor + res);
			}
		}
		for (int keys = -2; keys <= 16; keys++) {
			for (int col = -1; col <= keys + 1; col++) {
				emit("CL " + keys + " " + col);
				String r;
				try { r = ls(OsuToPjsk.columnToLaneSize(keys, col)); } catch (Throwable t) { r = ex(t); }
				line("CL " + no++ + " " + r);
			}
		}
		for (int keys = -2; keys <= 12; keys++) {
			for (int col = -1; col <= 8; col++) {
				emit("TC " + keys + " " + col);
				line("TC " + no++ + " " + OsuToPjsk.isTingedColumn(keys, col));
			}
		}
	}

	// ------------------------------------------------------------------ the converter
	static String lineEnd(int kind) { return kind == 0 ? "\n" : kind == 1 ? "\r\n" : kind == 2 ? "\r" : pick("\n", "\r\n", "\r"); }

	/** Builds one fuzzed .osu file; `special` selects a few hand-made shapes, 0 = random. */
	static String genOsu() {
		List<String> L = new ArrayList<String>();
		int kChoice = rnd(100);
		int K = kChoice < 30 ? 4 : kChoice < 40 ? 5 : kChoice < 55 ? 6 : kChoice < 80 ? 7 : new int[] { 1, 2, 3, 8, 9, 10, 12, 16, 24 }[rnd(9)];
		L.add(pick("osu file format v14", "osu file format v12", "// header"));
		L.add("");
		L.add(pick("[General]", "[General]", "[ General ]"));
		L.add("AudioFilename: audio.mp3");
		int modeChoice = rnd(100);
		L.add(modeChoice < 88 ? pick("Mode: 3", "Mode:3", "mode : 3", "MODE:  3  ", "Mode: 3") : modeChoice < 92 ? pick("Mode: 0", "Mode: 1", "Mode: 2", "Mode: 4", "Mode: 7") : modeChoice < 95 ? "Mode:" : modeChoice < 97 ? "Mode: x" : "Mode: 03");
		L.add("");
		L.add("[Metadata]");
		L.add("Title:" + pick("Song", "\u30bf\u30a4\u30c8\u30eb", "T\u00e9st"));
		L.add("");
		L.add(pick("[Difficulty]", "[Difficulty]", "[Difficulty ]"));
		L.add("HPDrainRate:7");
		int cs = rnd(100);
		String csv = cs < 80 ? Integer.toString(K) : cs < 86 ? K + ".5" : cs < 89 ? K + ".999" : cs < 92 ? "" : cs < 94 ? "abc" : cs < 96 ? "NaN" : cs < 98 ? "-1" : "0";
		L.add(pick("CircleSize:", "CircleSize: ", "circlesize:", "CIRCLESIZE :") + csv);
		L.add("OverallDifficulty:" + pick("8", "8.5", "5", "", "x", "10", "7.25"));
		L.add("SliderMultiplier:1.4");
		L.add("");
		L.add("[Events]");
		L.add("//Background and Video events");
		L.add("0,0,\"bg\u00e9.jpg\",0,0");
		if (rnd(4) == 0) { L.add("[Editor]"); L.add("Bookmarks: 1,2,3"); }
		L.add("");

		// ---- timing points
		int T0 = 300 + rnd(3000);
		boolean dense = rnd(8) == 0;   // very fast tempo + notes a few ms apart: snapping collisions that cascade
		int reds = rnd(4);
		L.add("[TimingPoints]");
		List<String> tps = new ArrayList<String>();
		int tt = T0;
		double firstBl = 500;
		for (int i = 0; i < reds; i++) {
			int blw = dense ? 18 + rnd(60) : 250 + rnd(750);
			String bl = rnd(3) == 0 ? blw + "." + rnd(1000) : Integer.toString(blw);
			if (i == 0) firstBl = Double.parseDouble(bl);
			int bad = rnd(60);
			if (bad == 0) bl = "0"; else if (bad == 1) bl = "-" + bl; else if (bad == 2) bl = "abc"; else if (bad == 3) bl = "NaN"; else if (bad == 4) bl = "Infinity";
			String t = tt + (rnd(6) == 0 ? ".5" : "");
			if (rnd(70) == 0) t = pick("NaN", "-Infinity", "abc", "");
			tps.add(t + "," + bl + ",4,1,0,60," + pick("1", "1", "1", "1", " 1 ", "") + ",0");
			tt += 2000 + rnd(40000);
		}
		int greens = rnd(6);
		for (int i = 0; i < greens; i++) {
			int t = rnd(3) == 0 && reds > 0 ? T0 : rnd(3) == 0 ? tt - 2000 : T0 + rnd(60000);
			String sv = "-" + (25 + rnd(300)) + (rnd(3) == 0 ? "." + rnd(10) : "");
			if (rnd(50) == 0) sv = pick("0", "-0", "50", "-0.0");
			tps.add(t + "," + sv + ",4,1,0,60," + pick("0", "0", "0", "2", "x", "") + ",0");
		}
		if (rnd(3) == 0 && !tps.isEmpty()) { String x = tps.remove(0); tps.add(x); }   // not sorted
		if (rnd(5) == 0 && tps.size() > 1) { String x = tps.remove(tps.size() - 1); tps.add(0, x); }
		for (String s : tps) {
			int bad = rnd(40);
			L.add(bad == 0 ? "1,2,3" : bad == 1 ? "garbage" : bad == 2 ? "//comment" : bad == 3 ? "" : bad == 4 ? "1,2,3,4,5,6" : s);
		}
		L.add("");

		// ---- hit objects
		L.add(pick("[HitObjects]", "[HitObjects]", "[HitObjects]", "[Hitobjects]"));
		int count = rnd(100) < 3 ? 0 : rnd(100) < 5 ? 1 : 2 + rnd(45);
		int t = (rnd(4) == 0 ? T0 - 1000 - rnd(3000) : T0 + rnd(3000));
		int[] holdLens = { 1, 30, 49, 50, 51, 99, 100, 101, 150, 300, 1000, 3000 };
		int[] gaps = dense ? new int[] { 0, 1, 1, 2, 3, 4, 5, 7, 9 } : new int[] { 0, 0, 20, 50, 100, 150, 250, 333, 500, 700, 1000, 2500, 8000 };
		int made = 0;
		List<String> objs = new ArrayList<String>();
		List<Integer> pendingEnds = new ArrayList<Integer>();   // taps placed exactly where a hold ends (release <= vs <)
		while (made < count) {
			int g = rnd(100) < 60 ? 1 : rnd(100) < 60 ? 2 : 3 + rnd(4);
			for (int gi = 0; gi < g && made < count; gi++, made++) {
				int col = rnd(Math.max(K, 1));
				int x = (int) ((col + 0.5) * 512 / Math.max(K, 1)) + rnd(5) - 2;
				int xs = rnd(60);
				if (xs == 0) x = -50; else if (xs == 1) x = 700; else if (xs == 2) x = Integer.MAX_VALUE; else if (xs == 3) x = Integer.MIN_VALUE; else if (xs == 4) x = 512;
				int ht;
				if (rnd(2) == 0) { int div = pickDiv(); ht = T0 + (int) Math.round(firstBl * rnd(160) / div) + (rnd(3) == 0 ? rnd(5) - 2 : 0); }
				else ht = t + new int[] { 0, 0, 0, 4, 12, 13 }[rnd(6)];
				int ts = rnd(80);
				if (ts == 0) ht = Integer.MAX_VALUE; else if (ts == 1) ht = Integer.MIN_VALUE; else if (ts == 2) ht = -5;
				boolean hold = rnd(100) < 28;
				String ln;
				if (hold) {
					int len = holdLens[rnd(holdLens.length)];
					int et = ht + len;
					if (ts == 0) et = Integer.MIN_VALUE; else if (ts == 1) et = Integer.MAX_VALUE;
					String endTok = Integer.toString(et);
					int bad = rnd(40);
					if (bad == 0) endTok = "abc"; else if (bad == 1) endTok = ""; else if (bad == 2) endTok = " " + et + " ";
					String tail = pick(":0:0:0:0:", ":0:0:0:0:hit.wav", ":0:0:0:0:a,b", ":0", ":0:0:0:0:");
					if (bad == 3) { endTok = Integer.toString(et); tail = ""; }
					else if (bad == 4) tail = ",0:0:0:0:";   // the end time sits in field 6 without a colon: only joining with the comma keeps it a tap
					if (ts >= 3 && rnd(3) == 0) pendingEnds.add(et);
					ln = x + ",192," + ht + "," + pick("128", "128", "128", "129", "132", "160") + ",0," + endTok + tail;
				} else {
					ln = x + ",192," + ht + "," + pick("1", "1", "1", "5", "9", "8") + ",0," + pick("0:0:0:0:", "0:0:0:0:", "") ;
				}
				int fmt = rnd(30);
				if (fmt == 0) ln = " " + ln.replace(",", " , ") + " ";
				else if (fmt == 1) ln = "1,2,3,4,5";
				else if (fmt == 2) ln = x + ",y," + ht + ",1,0,0:0:0:0:";
				else if (fmt == 3) ln = "//" + ln;
				objs.add(ln);
			}
			t += gaps[rnd(gaps.length)];
		}
		for (int et : pendingEnds) objs.add(Math.max(1, K) == 0 ? "" : ((int) ((rnd(Math.max(K, 1)) + 0.5) * 512 / Math.max(K, 1))) + ",192," + et + ",1,0,0:0:0:0:");
		if (rnd(6) == 0 && !objs.isEmpty()) objs.add(rnd(objs.size()), pick("[x]y]", "[]", "[x] y]", "[ ]"));   // header look-alikes inside [HitObjects]
		if (rnd(4) == 0) { int a = rnd(Math.max(objs.size(), 1)), b = rnd(Math.max(objs.size(), 1)); if (!objs.isEmpty()) { String x = objs.get(a); objs.set(a, objs.get(b)); objs.set(b, x); } }
		if (rnd(8) == 0 && !objs.isEmpty()) objs.add(objs.get(rnd(objs.size())));   // duplicate line
		L.addAll(objs);
		if (rnd(4) == 0) { L.add(""); L.add("   "); L.add("// end"); }

		// ---- join with line ends, occasional stray whitespace
		int nlKind = rnd(4);
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < L.size(); i++) {
			String s = L.get(i);
			if (rnd(14) == 0) s = pick(" ", "\t", "  ") + s;
			if (rnd(14) == 0) s = s + pick(" ", "\t", "  ");
			sb.append(s);
			if (i + 1 < L.size() || rnd(2) == 0) sb.append(lineEnd(nlKind));
		}
		return sb.toString();
	}

	static void printChart(int i, PjskChart c) {
		if (c == null) { line("CV " + i + " null"); return; }
		line("CV " + i + " ok keys=" + c.keyCount + " od=" + hx(c.overallDifficulty) + " offset=" + hx(c.offsetMs) + " nseg=" + c.bpmSegments.size()
			+ " nts=" + c.timeScale.size() + " nn=" + c.notes.size());
		for (int j = 0; j < c.bpmSegments.size(); j++) {
			BpmSegment s = c.bpmSegments.get(j);
			line("SEG " + i + " " + j + " " + hx(s.startMs) + " " + hx(s.beatAtStart) + " " + hx(s.beatLengthMs) + " " + hx(s.bpm));
		}
		for (int j = 0; j < c.timeScale.size(); j++) {
			TimeScaleChange s = c.timeScale.get(j);
			line("TS " + i + " " + j + " " + hx(s.beat) + " " + hx(s.timeMs) + " " + hx(s.timeScale));
		}
		for (int j = 0; j < c.notes.size(); j++) {
			Note n = c.notes.get(j);
			line("NOTE " + i + " " + j + " " + n.column + " " + hx(n.lane) + " " + hx(n.size) + " " + hx(n.beat) + " " + hx(n.endBeat) + " " + n.timeMs + " " + n.endTimeMs
				+ " " + n.hold + " " + n.critical + " " + n.tail.ordinal());
		}
	}

	static void cv(String text, int tail, boolean tinged, int minHold, int maxSim) {
		emit("CV " + tail + " " + (tinged ? 1 : 0) + " " + minHold + " " + maxSim + " " + hex(text));
		int i = no++;
		PjskChart c;
		try {
			c = OsuToPjsk.convert(new StringReader(text), TailMode.values()[tail], tinged, minHold, maxSim);
		} catch (Throwable t) {
			line("CV " + i + " " + ex(t));
			return;
		}
		printChart(i, c);
	}

	static void converter() {
		// a handful of hand-made files first
		String simple = "osu file format v14\n[General]\nMode: 3\n[Difficulty]\nCircleSize:4\nOverallDifficulty:8\n[TimingPoints]\n0,500,4,1,0,60,1,0\n[HitObjects]\n"
			+ "64,192,1000,1,0,0:0:0:0:\n192,192,1500,128,0,2000:0:0:0:0:\n320,192,2500,1,0,0:0:0:0:\n448,192,3000,5,0,0:0:0:0:\n";
		cv("", 1, true, 0, 2);
		cv("osu file format v14\n", 1, true, 0, 2);
		cv(simple, 1, true, 0, 2);
		cv(simple, 0, false, 0, 0);
		cv(simple, 2, true, 600, 1);
		cv(simple.replace("Mode: 3", "Mode: 0"), 1, true, 0, 2);
		cv(simple.replace("CircleSize:4", "CircleSize:"), 1, true, 0, 2);
		cv(simple.replace("Mode: 3\n", ""), 1, true, 0, 2);
		cv(simple.replace("\n", "\r\n"), 1, true, 0, 2);
		cv(simple.replace("\n", "\r"), 1, true, 0, 2);
		cv(simple.replace("64,192,1000,1,0,0:0:0:0:\n", ""), 1, true, 0, 2);             // one object: not usable
		cv(simple.replace("0,500,4,1,0,60,1,0\n", ""), 1, true, 0, 2);                   // no timing points: default 120 BPM
		cv(simple.replace("0,500,4,1,0,60,1,0", "2000,500,4,1,0,60,1,0"), 1, true, 0, 2); // notes before the first red line
		cv("[General]\nMode: 3\n[Difficulty]\nCircleSize:1\n[HitObjects]\n0,0,100,1,0,0\n511,0,200,1,0,0\n", 1, true, 0, 2);
		cv("[General]\nMode: 3\n[Difficulty]\nCircleSize:24\n[TimingPoints]\n0,300,4,1,0,60,1,0\n[HitObjects]\n0,0,100,1,0,0\n511,0,100,1,0,0\n256,0,100,1,0,0\n", 2, true, 0, 0);
		cv("[General]\nMode: 3\n[Difficulty]\nCircleSize:4\n[HitObjects]\n10,0,2147483647,1,0,0\n10,0,-2147483648,128,0,2147483647:0\n300,0,5,128,0,-2147483648:0\n", 1, false, 100, 2);
		{
			StringBuilder sb = new StringBuilder("[General]\nMode: 3\n[Difficulty]\nCircleSize:4\n[TimingPoints]\n");
			for (int i = 0; i < 24; i++) sb.append((i % 3) * 5000).append(',').append(i % 5 == 4 ? "-" + (50 + i) : Integer.toString(300 + i * 7)).append(",4,1,0,60,").append(i % 5 == 4 ? "0" : "1").append(",0\n");
			sb.append("[HitObjects]\n64,0,100,1,0,0\n448,0,9000,1,0,0\n");
			cv(sb.toString(), 1, true, 0, 0);
			StringBuilder sb2 = new StringBuilder("[General]\nMode: 3\n[Difficulty]\nCircleSize:4\n[TimingPoints]\n0,500,4,1,0,60,1,0\n");
			for (int i = 0; i < 40; i++) sb2.append((i % 4) * 3000).append(",-").append(40 + i * 3).append(",4,1,0,60,0,0\n");
			sb2.append("[HitObjects]\n64,0,100,1,0,0\n448,0,9000,1,0,0\n");
			cv(sb2.toString(), 1, true, 0, 0);
		}
		// a hold that ends exactly when the next note starts is released in time for it (cap 1): the tap must survive
		cv("[General]\nMode: 3\n[Difficulty]\nCircleSize:4\n[HitObjects]\n64,0,1000,128,0,2000:0:0:0:0:\n448,0,2000,1,0,0:0:0:0:\n", 1, false, 0, 1);
		cv("[General]\nMode: 3\n[Difficulty]\nCircleSize:4\n[HitObjects]\n64,0,1000,128,0,2001:0:0:0:0:\n448,0,2000,1,0,0:0:0:0:\n", 1, false, 0, 1);
		cv(simple.replace("Mode: 3", "Mode: 4"), 1, true, 0, 2);
		cv(simple.replace("Mode: 3", "Mode: 2"), 1, true, 0, 2);
		cv(simple.replace("192,192,1500,128,0,2000:0:0:0:0:", "192,192,1500,128,0,2000,0:0:0:0:"), 1, true, 0, 2);
		cv(simple.replace("[HitObjects]\n", "[HitObjects]\n[x]y]\n"), 1, true, 0, 2);
		cv(simple.replace("[HitObjects]\n", "[HitObjects]\n[]\n"), 1, true, 0, 2);
		cv("\uFEFFosu file format v14\n[General]\nMode: 3\n[Difficulty]\nCircleSize:4\n[HitObjects]\n10,0,5,1,0,0\n300,0,50,1,0,0\n", 1, false, 0, 2);
		for (int i = 0; i < 260; i++) {
			String text = genOsu();
			int tail = rnd(3);
			boolean tinged = rnd(2) == 0;
			int minHold = new int[] { 0, 0, 0, 50, 100, 200, -5 }[rnd(7)];
			int maxSim = new int[] { 2, 2, 2, 0, -1, 1, 3, 4, 10 }[rnd(9)];
			cv(text, tail, tinged, minHold, maxSim);
		}
		// the same files again with every option combination of the tail / tinged / limit settings
		for (int i = 0; i < 12; i++) {
			String text = genOsu();
			for (int tail = 0; tail < 3; tail++)
				for (int sim : new int[] { 0, 1, 2, 3 })
					cv(text, tail, (tail + sim) % 2 == 0, tail == 1 ? 100 : 0, sim);
		}
	}

	public static void main(String[] args) throws Exception {
		trimSplit(); beats(); lanes(); converter();
		java.nio.file.Files.write(java.nio.file.Paths.get(args[0]), out.toString().getBytes(StandardCharsets.UTF_8));
		java.nio.file.Files.write(java.nio.file.Paths.get(args[1]), cases.toString().getBytes(StandardCharsets.UTF_8));
		System.out.println("wrote " + args[0] + " (" + out.length() + " chars) and " + args[1] + " (" + cases.length() + " chars)");
	}
}
