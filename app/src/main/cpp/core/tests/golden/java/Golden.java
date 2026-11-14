import itdelatrisu.opsu.GameMod;
import itdelatrisu.opsu.beatmap.HitObject;
import itdelatrisu.opsu.beatmap.TimingPoint;
import itdelatrisu.opsu.objects.curves.Curve;
import itdelatrisu.opsu.objects.curves.Vec2f;
import itdelatrisu.opsu.options.Options;
import itdelatrisu.opsu.replay.ReplayFrame;

import java.text.DecimalFormat;
import java.util.ArrayList;
import java.util.List;

/** Generates tests/golden/expected.txt from the REAL Java classes. Mirrored by tests/test_golden.cpp. */
public class Golden {
	static StringBuilder out = new StringBuilder();
	static long seed = 12345;
	static int rnd() { seed = seed * 6364136223846793005L + 1442695040888963407L; return (int) (seed >>> 33); }
	static int rnd(int n) { return rnd() % n; }
	static String hx(float f) { return String.format("%08x", Float.floatToRawIntBits(f)); }
	static void line(String s) { out.append(s).append('\n'); }
	static String hexList(float[] a) { if (a == null) return "-"; StringBuilder s = new StringBuilder(); for (float f : a) s.append(hx(f)).append(','); return s.toString(); }

	static void fmt() {
		List<Float> v = new ArrayList<Float>();
		float[] cur = {0f, -0f, 0.125f, 0.375f, 0.625f, 0.875f, 1.005f, 2.5f, 0.5f, 1e7f, 9999999f, 1e-3f, 0.00099999f,
			100000f, 999999.5f, 1e-5f, 1234567f, 123456f, 1000f, 60000f, 333.33334f, 428.57143f, 0.0625f, 1.0E10f,
			3.4028235E38f, 1.4E-45f, 256.125f, 0.045f, 99.995f, 0.9999995f, 5e-6f, -7.5f, 12345.678f, 180f, 123.456f, 0.1f, 1f/3f};
		for (float f : cur) v.add(f);
		for (int i = -24; i <= 24; i++) v.add(i / 8f);
		for (int i = 0; i <= 200; i += 7) v.add(i * 0.01f);
		for (int i = 0; i < 400; i++) {
			int bits = (rnd(2) << 31) | ((100 + rnd(60)) << 23) | (rnd() & 0x7fffff);
			v.add(Float.intBitsToFloat(bits));
		}
		DecimalFormat nf = new DecimalFormat("###.#####");
		for (float f : v) {
			double d = f;
			line("FMT " + hx(f) + " " + String.format("%.2f", d) + "|" + String.format("%.3f", d) + "|" + String.format("%g", d)
				+ "|" + Float.toString(f) + "|" + nf.format(d));
		}
		for (float f : new float[]{Float.NaN, Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY})
			line("FMTSPECIAL " + String.format("%.2f", (double) f) + "|" + String.format("%g", (double) f) + "|" + Float.toString(f));
	}

	static void parse() {
		String[] fl = {" 1.5 ", "1e3", "1.5f", ".5", "5.", "+3", "-0", "NaN", "-Infinity", "abc", "", "1e", "1.5.2", "  ", "1e40", "1e-50",
			"3.4028236e38", "0.1", "16777217", "1,5", "1_0", "-.5e2", "7d", "+", "e5", "1 2"};
		for (String s : fl) { String r; try { r = hx(Float.parseFloat(s)); } catch (Exception e) { r = "ERR"; } line("PF [" + s + "] " + r); }
		String[] in = {"12", "-0", "+5", "", "1 ", " 1", "2147483648", "-2147483648", "2147483647", "-", "+", "0x10", "1.0", "007", "99999999999"};
		for (String s : in) {
			String r1, r2, r3;
			try { r1 = "" + Integer.parseInt(s); } catch (Exception e) { r1 = "ERR"; }
			try { r2 = "" + Byte.parseByte(s); } catch (Exception e) { r2 = "ERR"; }
			try { r3 = "" + Short.parseShort(s); } catch (Exception e) { r3 = "ERR"; }
			line("PI [" + s + "] " + r1 + " " + r2 + " " + r3);
		}
		String[] sp = {"", ",", ",,", "a", "a,b", "a,,b", ",a", "a,", "a,,", ",,a,,", "a,b,c,,,"};
		for (String s : sp) {
			String[] p = s.split(",");
			StringBuilder b = new StringBuilder();
			for (String x : p) b.append('<').append(x).append('>');
			line("SPLIT [" + s + "] " + p.length + " " + b);
		}
		String[] sp2 = {"B|1:2|3:4", "B", "|", "||a", "0:0:0:0:", ":"};
		for (String s : sp2) { String[] p = s.split("\\|"); String[] q = s.split(":"); line("SPLIT2 [" + s + "] " + p.length + " " + q.length); }
	}

	static void tp() {
		String[] lines = {"0,1000.0,4,1,0,100,0,0", "70382,\t-66.6666666666667,4,2,1,70,0,1", "1000,500,4,2,1,70", "1000,500,4,2,1,70,1", "1000,500", "1000",
			"1000.7,333.33334,3,3,2,50,1,1", "-5,-100,4,1,0,100,1,0", "0,-5000,4,1,0,100,1,0", "0,-1,4,1,0,100,1,0", "0,1e-3,4,1,0,100,1,0",
			"0,1000000,4,1,0,100,1,0", "0,123456.789,4,1,0,100,1,1", "0,60000,4,1,0,100", "0,0,4,1,0,100", "abc,1,2,3,4,5", "0,1,x,3,4,5", "0,1,2,300,4,5",
			"0,428.571428,4,1,0,100,1,2", "0,-100.5,4,1,0,100,0,0", "0,0.1,4,1,0,100,0,0", "0,99999.9,4,1,0,100,0,0"};
		for (String l : lines) {
			String r;
			try {
				TimingPoint t = new TimingPoint(l);
				r = t.toString() + " | " + t.getTime() + " " + hx(t.getBeatLength()) + " " + hx(t.getSliderMultiplier()) + " " + t.getMeter() + " "
					+ t.getSampleType() + " " + t.getSampleTypeCustom() + " " + hx(t.getSampleVolume()) + " " + t.isInherited() + " " + t.isKiaiTimeActive();
			} catch (Exception e) { r = "ERR"; }
			line("TP [" + l + "] " + r);
		}
		TimingPoint d = TimingPoint.DEFAULT;
		line("TPDEFAULT " + d.toString());
	}

	static final String[] HO = {
		"256,148,9466,1,2,0:0:0:0:", "256,148,9466,5,2", "256,148,9466,1,0", "100.5,50.25,1000,1,0,3:2:1:80:hit.wav", "100,100,1,1,0,1:2",
		"100,100,1,1,0,1:2:3:4:a:b", "100,100,1,1,0,1", "100,100,1,1,0,1:2:3:4:", "100,100,1,1,0,1:2:3:4::x",
		"256,192,654,12,0,4029,0:0:0:0:", "256,192,654,12,0,4029:1:2", "256,192,654,12,0,4029", "256,192,654,8,0,4029,1:2", "256,192,654,8,0,4029,",
		"300,68,4591,2,0,B|372:100|332:172|420:192,2,180,2|2|2,0:0|0:0|0:0,0:0:0:0:", "300,68,4591,6,0,B|372:100|332:172|420:192,2,180.5",
		"300,68,4591,2,4,L|372:100|332:172,1,100,2|8,1:2|3:4,1:2:3:50:", "100,100,500,2,0,P|150:150|200:100,1,150.75",
		"100,100,500,2,0,C|150:150|200:100|250:150,1,200", "100,100,500,2,0,B|150:150|150:150|200:200|300:100,1,300,0|0,0:0|0:0",
		"100,100,500,2,0,P|150:150|200:200,1,150", "100,100,500,2,0,P|200:100|300:100,1,150", "100,100,500,2,0,P|100:100|300:100,1,150",
		"100,100,500,2,0,B|200:100,1,2", "100,100,500,2,0,B,1,100", "100,100,500,2,0,C|150:150,1,100", "100,100,500,2,0,L|1:1,1,1e3",
		"100,100,500,2,0,B|200:100|200:100|300:50|300:50|400:100,1,250", "100,100,500,2,0,B|100:100|200:100|200:100,1,90",
		"100,100,500,2,0,B|100:100|100:100,1,90", "100,100,500,2,0,L|100:100|100:100|100:100,1,90",
		"0,0,0,1,2", "bad", "", "1,2", "1,2,3,4", "1,2,3,4,300", "1,2,3,4,-3", "1,2,3,16,0", "1,2,3,2,0,B,1,100",
		"1.5f,2,3,1,0", "x,2,3,1,0", " 1 ,2,3,1,0", "0.00001,0.5,3,1,0", "-0.125,-3.5,3,1,0", "1234567.123456,0.000004,3,1,0", "100,100,500,2,0,B|1:1,1,1e10", "100,100,500,2,0,B|1:1,1,12345.678"
	};

	static int[][] CFG = {{1920, 1080, 10}, {1280, 720, 16}, {800, 600, 10}, {1080, 2400, 10}, {640, 480, 20}, {512, 384, 13}};

	static void setHR(boolean on) { if (GameMod.HARD_ROCK.isActive() != on) GameMod.HARD_ROCK.toggle(false); }
	static void init(int[] c) { Options.playfieldScale = c[2] / 10f; HitObject.init(c[0], c[1]); }

	static void ho() {
		GameMod.loadModState(0);
		for (String l : HO) {
			HitObject h;
			try { h = new HitObject(l); } catch (Exception e) { line("HO [" + l + "] ERR"); continue; }
			line("HO [" + l + "] " + h.toString() + " | " + h.getTypeName() + " t=" + h.getTime() + " ty=" + h.getType() + " hs=" + h.getHitSoundType()
				+ " e=" + h.getEdgeHitSoundType(-1) + "," + h.getEdgeHitSoundType(0) + "," + h.getEdgeHitSoundType(7)
				+ " ss=" + h.getSampleSet(0) + "," + h.getSampleSet(3) + " as=" + h.getAdditionSampleSet(0) + "," + h.getAdditionSampleSet(9)
				+ " os=" + h.getObjectSampleSet() + "," + h.getObjectAdditionSampleSet() + " ci=" + h.getCustomSampleIndex() + " vol=" + h.getHitSoundVolume()
				+ " file=" + (h.getHitSoundFile() == null ? "" : h.getHitSoundFile()) + " rep=" + h.getRepeatCount() + " pl=" + hx(h.getPixelLength())
				+ " et=" + h.getEndTime() + " skip=" + h.getComboSkip() + " nc=" + h.isNewCombo() + " st=" + (int) h.getSliderType()
				+ " sx=" + hexList(h.getSliderX()) + " sy=" + hexList(h.getSliderY()) + " x=" + hx(h.getX()) + " y=" + hx(h.getY())
				+ " time=" + hx(h.getSliderTime(1.4f, 333.33f)));
			for (int[] c : CFG) {
				init(c);
				line("HOINIT " + c[0] + "x" + c[1] + "@" + c[2] + " " + hx(HitObject.getXMultiplier()) + " " + hx(HitObject.getYMultiplier()) + " "
					+ HitObject.getXOffset() + " " + HitObject.getYOffset());
				for (int mode = 0; mode < 3; mode++) {
					setHR(mode == 1 || mode == 2);
					HitObject.setStackOffset(mode == 2 ? 3.2f : 0f);
					h.setStack(mode == 2 ? 3 : 0);
					line("HOSCALE " + mode + " " + hx(h.getScaledX()) + " " + hx(h.getScaledY()) + " " + hexList(h.getScaledSliderX()) + " " + hexList(h.getScaledSliderY()));
				}
				setHR(false); HitObject.setStackOffset(0f); h.setStack(0);
			}
		}
	}

	static int curated;
	static String fnv(String s) { long h = 0xcbf29ce484222325L; for (int i = 0; i < s.length(); i++) { h ^= (s.charAt(i) & 0xff); h *= 0x100000001b3L; } return String.format("%016x", h); }

	static List<String> sliders() {
		List<String> l = new ArrayList<String>();
		for (String s : HO) if (s.split(",").length > 7 && s.indexOf('|') > 0 && !s.contains("1e10")) l.add(s);
		curated = l.size();
		String types = "BLPC";
		for (int i = 0; i < 300; i++) {
			char t = types.charAt(rnd(4));
			int n = 1 + rnd(6);
			if (t == 'P' && rnd(4) != 0) n = 2;
			StringBuilder b = new StringBuilder();
			b.append(rnd(513)).append(',').append(rnd(385)).append(",1000,2,0,").append(t);
			for (int k = 0; k < n; k++) {
				if (rnd(6) == 0 && k > 0) { String last = b.substring(b.lastIndexOf("|") + 1); b.append('|').append(last); } // repeat last point (red point)
				else b.append('|').append(rnd(513)).append(':').append(rnd(385));
			}
			int len = 500 + rnd(80000);
			b.append(',').append(1 + rnd(3)).append(',').append(len / 100).append('.').append(String.format("%02d", len % 100));
			l.add(b.toString());
		}
		return l;
	}

	static void curves() {
		GameMod.loadModState(0);
		List<String> sl = sliders();
		int idx = 0;
		for (String l : sl) {
			idx++;
			HitObject h;
			try { h = new HitObject(l); } catch (Exception e) { continue; }
			for (int cfg = 0; cfg < 3; cfg++) {
				init(CFG[cfg == 2 ? 1 : 0]);
				boolean scaled = cfg != 1;
				String r;
				try {
					Curve c = h.getSliderCurve(scaled);
					StringBuilder b = new StringBuilder();
					Vec2f[] p = c.getCurvePoints();
					b.append("n=").append(p.length).append(" sa=").append(hx(c.getStartAngle())).append(" ea=").append(hx(c.getEndAngle())).append(" pts=");
					for (Vec2f v : p) b.append(hx(v.x)).append(':').append(hx(v.y)).append(' ');
					b.append("at=");
					for (int k = 0; k <= 10; k++) { Vec2f v = c.pointAt(k / 10f); b.append(hx(v.x)).append(':').append(hx(v.y)).append(' '); }
					b.append("ix=");
					for (float t : new float[]{-0.5f, 0f, 0.33f, 1f, 1.5f}) b.append(hx(c.indexAt(t))).append(' ');
					b.append("x0=").append(hx(c.getX(0))).append(" y1=").append(hx(c.getY(1)));
					r = b.toString();
					if (idx > curated) r = "n=" + p.length + " hash=" + fnv(r);   // random sliders: compact
				} catch (Exception e) { r = "ERR"; }
				line("CURVE " + idx + " cfg=" + cfg + " " + r);
			}
		}
	}

	static void replay() {
		init(CFG[0]);
		for (int i = 0; i < 300; i++) {
			float x = (i % 3 == 0) ? (rnd(8000) - 200) / 8f : rnd(51200) / 100f;
			float y = (i % 3 == 1) ? (rnd(8000) - 200) / 8f : rnd(38400) / 100f;
			ReplayFrame f = new ReplayFrame(rnd(100), rnd(100000), x, y, rnd(4096));
			line("RF " + f.toString() + " " + f.getScaledX() + " " + f.getScaledY() + " " + f.isKeyPressed() + " " + f.getKeys());
		}
		ReplayFrame s = ReplayFrame.getStartFrame(77);
		line("RFSTART " + s.toString() + " " + s.getTimeDiff());
	}

	static void mods() {
		GameMod[] vals = GameMod.values();
		for (GameMod m : vals)
			line("MODINFO " + m.getName() + " [" + m.getAbbreviation() + "] " + m.getBit() + " " + m.getKey() + " " + hx(m.getMultiplier()) + " "
				+ m.getDescription().replace("\n", "\\n"));
		for (int a = 0; a < vals.length; a++)
			for (int b = 0; b < vals.length; b++)
				for (int inv = 0; inv < 2; inv++) {
					GameMod.loadModState(0);
					vals[a].toggle(inv == 1); vals[b].toggle(inv == 1);
					line("MODPAIR " + a + " " + b + " " + inv + " " + GameMod.getModState());
				}
		GameMod.loadModState(0);
		for (int s = 0; s < 3000; s++) {
			if (s % 5 == 0) GameMod.loadModState(0);
			GameMod m = vals[rnd(vals.length)];
			m.toggle(rnd(5) != 0);
			line("MODSEQ " + s + " " + GameMod.getModState() + " " + hx(GameMod.getScoreMultiplier()) + " " + hx(GameMod.getSpeedMultiplier()) + " " + hx(GameMod.getDifficultyMultiplier()));
		}
		for (int st = 0; st < 16384; st += 31) {
			GameMod.loadModState(st);
			line("MODLOAD " + st + " " + GameMod.getModState() + " " + hx(GameMod.getScoreMultiplier()) + " " + hx(GameMod.getSpeedMultiplier())
				+ " " + hx(GameMod.getDifficultyMultiplier()) + " " + GameMod.getModString(st));
		}
		line("MODSTRING0 " + GameMod.getModString(0));
	}

	public static void main(String[] args) throws Exception {
		fmt(); parse(); tp(); ho(); curves(); replay(); mods();
		java.nio.file.Files.write(java.nio.file.Paths.get(args[0]), out.toString().getBytes("UTF-8"));
	}
}
