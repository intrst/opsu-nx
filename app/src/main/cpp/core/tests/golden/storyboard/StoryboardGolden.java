// Twelfth golden suite: Storyboard (data model + evaluation). Lives in the Java package so it can use the package-private API the parser uses.
// Mirrored statement for statement by tests/test_golden12.cpp (Java evaluates arguments left to right, C++ does not: every rnd() is sequenced explicitly).
package itdelatrisu.opsu.storyboard;

import itdelatrisu.opsu.ui.animations.AnimationEquation;

import java.nio.charset.StandardCharsets;
import java.util.List;

public class StoryboardGolden {
	static StringBuilder out = new StringBuilder();
	static long seed = 20261113L * 7919L;
	static int rnd() { seed = seed * 6364136223846793005L + 1442695040888963407L; return (int) (seed >>> 33); }
	static int rnd(int n) { return rnd() % n; }
	static String hx(float f) { return String.format("%08x", Float.floatToIntBits(f)); }
	static String hx(double d) { return String.format("%016x", Double.doubleToLongBits(d)); }
	static void line(String s) { out.append(s).append('\n'); }
	static String b(boolean v) { return v ? "1" : "0"; }

	static final float[] ALPHAS = { 0f, 0.25f, 0.5f, 1f, 1.5f, 2.25f, 3.75f };

	static int width(int p) { return p == Storyboard.VECTOR_SCALE ? 2 : p == Storyboard.COLOUR ? 3 : 1; }

	static float val(int p) {
		int r;
		switch (p) {
		case Storyboard.X: case Storyboard.Y: r = rnd(1000); return (r - 200) / 2f;
		case Storyboard.SCALE: r = rnd(40); return (1 + r) / 10f;
		case Storyboard.VECTOR_SCALE: r = rnd(30); return (1 + r) / 10f;
		case Storyboard.ROTATION: r = rnd(720); return (r - 360) / 10f;
		case Storyboard.COLOUR: r = rnd(256); return r / 255f;
		case Storyboard.ALPHA: r = rnd(ALPHAS.length); return ALPHAS[r];
		default: r = rnd(2); return r;       // BLEND, FLIP_H, FLIP_V
		}
	}

	/** Adds one random command to the group. */
	static void addRandom(Storyboard.CommandGroup g) {
		int p = rnd(Storyboard.PROPERTY_COUNT);
		int eq = rnd(AnimationEquation.values().length);
		int s0 = rnd(6000);
		int half = rnd(4);
		double start = s0 + (half == 0 ? 0.5 : 0.0);
		int kind = rnd(5);
		double dur;
		if (kind == 0) { int d = rnd(300); dur = -d; }
		else if (kind == 1) dur = 0;
		else { int d = rnd(2500); dur = d; }
		double end = start + dur;
		int n = width(p);
		float[] v0 = new float[n], v1 = new float[n];
		for (int i = 0; i < n; i++) v0[i] = val(p);
		for (int i = 0; i < n; i++) v1[i] = val(p);
		g.add(p, AnimationEquation.values()[eq], start, end, v0, v1);
	}

	static void sprites() {
		for (int c = 0; c < 150; c++) {
			int ox = rnd(3), oy = rnd(3);
			int ix = rnd(640), iy = rnd(480);
			int ncmd = (rnd(10) == 0) ? 0 : rnd(13);
			int nloops = rnd(3);
			int ntrig = rnd(2);
			Storyboard.Sprite sp = new Storyboard.Sprite("sprite" + c + ".png", ox, oy, ix, iy, false, 0, 0, false);
			for (int i = 0; i < ncmd; i++) addRandom(sp.commands);
			int totalLoopCmds = 0;
			for (int l = 0; l < nloops; l++) {
				int ls = rnd(4000);
				int rc = rnd(8) - 2;
				Storyboard.LoopGroup g = sp.addLoopGroup(ls, rc);
				int k = rnd(5);
				for (int i = 0; i < k; i++) addRandom(g);
				totalLoopCmds += k;
			}
			for (int t = 0; t < ntrig; t++) {
				Storyboard.TriggerGroup g = sp.addTriggerGroup("Trigger" + t, 100, 200, t);
				int k = rnd(3);
				for (int i = 0; i < k; i++) addRandom(g);
			}
			sp.finish();
			line("SP " + c + " " + ncmd + " " + nloops + " " + totalLoopCmds + " " + ntrig + " " + b(sp.hasCommands()) + " " + b(sp.isDrawable()) + " "
					+ hx(sp.getStartTime()) + " " + hx(sp.getEndTimeForDisplay()) + " " + hx(sp.getEarliestTransformTime()));
			// evaluation times: random ones, plus the start / end of the first commands
			double[] times = new double[26];
			for (int i = 0; i < 20; i++) {
				int r = rnd(8000);
				int q = rnd(3);
				times[i] = (r - 800) + (q == 0 ? 0.25 : 0.0);
			}
			int ti = 20;
			for (int p = 0; p < Storyboard.PROPERTY_COUNT && ti < 26; p++)
				for (Storyboard.Command cm : sp.commands.lists[p]) {
					if (ti >= 26) break;
					times[ti++] = (ti % 2 == 0) ? cm.start : cm.end;
				}
			while (ti < 26) times[ti++] = 2500.5;
			Storyboard.State s = new Storyboard.State();
			for (int i = 0; i < times.length; i++) {
				double t = times[i];
				sp.evaluate(t, s);
				line("EV " + c + " " + hx(t) + " " + b(sp.isAlive(t)) + " " + hx(s.x) + " " + hx(s.y) + " " + hx(s.scale) + " " + hx(s.vsx) + " " + hx(s.vsy) + " "
						+ hx(s.rotation) + " " + hx(s.r) + " " + hx(s.g) + " " + hx(s.b) + " " + hx(s.alpha) + " " + b(s.additive) + " " + b(s.flipH) + " " + b(s.flipV));
			}
		}
	}

	/** Command clamping, loop groups with huge repeat counts, and hand-made cases around the loop / alpha rules. */
	static void special() {
		// end < start is clamped to start
		Storyboard.Sprite a = new Storyboard.Sprite("a.png", 1, 1, 10f, 20f, false, 0, 0, false);
		a.commands.add(Storyboard.X, AnimationEquation.LINEAR, 1000, 500, new float[] { 5f }, new float[] { 50f });
		a.finish();
		Storyboard.State s = new Storyboard.State();
		for (double t : new double[] { 0, 499, 500, 999, 1000, 1001, 5000 }) { a.evaluate(t, s); line("SPEC1 " + hx(t) + " " + hx(s.x) + " " + b(a.isAlive(t))); }
		line("SPEC1 life " + hx(a.getStartTime()) + " " + hx(a.getEndTimeForDisplay()));
		// loop: repeatCount INT_MAX wraps totalIterations to INT_MIN (Java), -5 clamps to 0
		Storyboard.LoopGroup big = a.addLoopGroup(0, Integer.MAX_VALUE);
		Storyboard.LoopGroup neg = a.addLoopGroup(0, -5);
		line("SPEC2 " + big.totalIterations + " " + neg.totalIterations);
		// looping alpha fade: loop starts at 1000, period 500, 3 iterations
		Storyboard.Sprite l = new Storyboard.Sprite("l.png", 1, 1, 0f, 0f, false, 0, 0, false);
		Storyboard.LoopGroup g = l.addLoopGroup(1000, 2);
		g.add(Storyboard.ALPHA, AnimationEquation.LINEAR, 0, 500, new float[] { 0f }, new float[] { 1f });
		g.add(Storyboard.X, AnimationEquation.OUT_QUAD, 0, 250, new float[] { 0f }, new float[] { 100f });
		l.finish();
		line("SPEC3 life " + hx(l.getStartTime()) + " " + hx(l.getEndTimeForDisplay()) + " " + hx(l.getEarliestTransformTime()));
		for (double t = 900; t <= 2700; t += 100) { l.evaluate(t, s); line("SPEC3 " + hx(t) + " " + b(l.isAlive(t)) + " " + hx(s.alpha) + " " + hx(s.x)); }
		// a sprite with no commands, one with only a zero-duration flip, blend held until the end
		Storyboard.Sprite e = new Storyboard.Sprite("e.png", 0, 0, 3f, 4f, false, 0, 0, false);
		e.finish();
		line("SPEC4 " + b(e.hasCommands()) + " " + hx(e.getStartTime()) + " " + hx(e.getEndTimeForDisplay()) + " " + b(e.isAlive(0)));
		e.evaluate(10, s);
		line("SPEC4 " + hx(s.x) + " " + hx(s.y) + " " + hx(s.scale) + " " + hx(s.alpha) + " " + b(s.additive));
		Storyboard.Sprite f = new Storyboard.Sprite("f.png", 0, 0, 0f, 0f, false, 0, 0, false);
		f.commands.add(Storyboard.FLIP_H, AnimationEquation.LINEAR, 100, 100, new float[] { 1f }, new float[] { 1f });
		f.commands.add(Storyboard.BLEND, AnimationEquation.LINEAR, 200, 400, new float[] { 1f }, new float[] { 0f });
		f.finish();
		for (double t : new double[] { 0, 99, 100, 150, 200, 399, 400, 500 }) { f.evaluate(t, s); line("SPEC5 " + hx(t) + " " + b(s.flipH) + " " + b(s.additive)); }
		// alpha flicker: > 1 is wrapped with %= 1
		Storyboard.Sprite al = new Storyboard.Sprite("al.png", 0, 0, 0f, 0f, false, 0, 0, false);
		al.commands.add(Storyboard.ALPHA, AnimationEquation.LINEAR, 0, 1000, new float[] { 0f }, new float[] { 3.5f });
		al.finish();
		for (double t : new double[] { 0, 100, 250, 300, 500, 700, 999, 1000, 2000 }) { al.evaluate(t, s); line("SPEC6 " + hx(t) + " " + hx(s.alpha)); }
		// two commands of one loop group with the same effective start: the LATER one wins (>=)
		Storyboard.Sprite tie = new Storyboard.Sprite("tie.png", 0, 0, 0f, 0f, false, 0, 0, false);
		Storyboard.LoopGroup tg = tie.addLoopGroup(500, 3);
		tg.add(Storyboard.X, AnimationEquation.LINEAR, 0, 100, new float[] { 1f }, new float[] { 2f });
		tg.add(Storyboard.X, AnimationEquation.LINEAR, 0, 100, new float[] { 10f }, new float[] { 20f });
		tg.add(Storyboard.Y, AnimationEquation.LINEAR, 0, 200, new float[] { 5f }, new float[] { 6f });
		tie.finish();
		for (double t : new double[] { 400, 500, 550, 600, 650, 700, 800, 1000, 1300 }) { tie.evaluate(t, s); line("SPEC7 " + hx(t) + " " + hx(s.x) + " " + hx(s.y)); }
		// 60 commands with the same start (more than std::sort's insertion-sort threshold): the sort must be stable, the last one added wins
		Storyboard.Sprite many = new Storyboard.Sprite("many.png", 0, 0, 0f, 0f, false, 0, 0, false);
		for (int i = 0; i < 60; i++) many.commands.add(Storyboard.X, AnimationEquation.LINEAR, 100, 100, new float[] { i }, new float[] { i });
		many.finish();
		for (double t : new double[] { 50, 100, 150 }) { many.evaluate(t, s); line("SPEC8 " + hx(t) + " " + hx(s.x)); }
	}

	static void layers() {
		Storyboard sb = new Storyboard();
		StringBuilder o = new StringBuilder();
		for (Storyboard.Layer l : sb.getDrawOrder()) o.append(l.name).append(':').append(l.depth).append(' ');
		line("LAY1 " + o);
		Storyboard.Layer x = sb.getLayer("Custom1"), y = sb.getLayer("Custom2"), x2 = sb.getLayer("Custom1");
		line("LAY2 " + x.depth + " " + y.depth + " " + (x == x2) + " " + x.masking + " " + x.visibleWhenPassing + x.visibleWhenFailing);
		StringBuilder o2 = new StringBuilder();
		for (Storyboard.Layer l : sb.getDrawOrder()) o2.append(l.name).append(':').append(l.depth).append(' ');
		line("LAY3 " + o2);      // cached order does not know Custom1/2 yet? (drawOrder computed before they were added)
		sb.finish();
		StringBuilder o3 = new StringBuilder();
		for (Storyboard.Layer l : sb.getDrawOrder()) o3.append(l.name).append(':').append(l.depth).append(' ');
		line("LAY4 " + o3);      // finish() resets the cache
		Storyboard.Layer fail = sb.getLayer("Fail"), pass = sb.getLayer("Pass");
		line("LAY5 " + fail.visibleWhenPassing + fail.visibleWhenFailing + " " + pass.visibleWhenPassing + pass.visibleWhenFailing);
		line("LAY6 " + sb.hasDrawable() + " " + sb.hasSamples() + " " + sb.widescreen + " " + sb.useSkinSprites);
		Storyboard.Sprite sp = new Storyboard.Sprite("BG/Back.PNG", 1, 1, 320f, 240f, false, 0, 0, false);
		sp.commands.add(Storyboard.ALPHA, AnimationEquation.LINEAR, 0, 10, new float[] { 0f }, new float[] { 1f });
		sb.getLayer("Background").sprites.add(sp);
		sb.getLayer("Foreground").samples.add(new Storyboard.Sample("hit.wav", 1000, 80));
		sb.finish();
		line("LAY7 " + sb.hasDrawable() + " " + sb.hasSamples());
		line("LAY8 " + sb.replacesBackground("bg/back.png") + " " + sb.replacesBackground("BG/BACK.PNG") + " " + sb.replacesBackground("bg/back.jpg") + " "
				+ sb.replacesBackground(null) + " " + sb.replacesBackground(""));
		Storyboard sb2 = new Storyboard();
		sb2.getLayer("A").sprites.add(new Storyboard.Sprite("x", 0, 0, 0f, 0f, false, 0, 0, false));   // no commands: not drawable
		line("LAY9 " + sb2.hasDrawable());
	}

	public static void main(String[] args) throws Exception {
		sprites(); special(); layers();
		java.nio.file.Files.write(java.nio.file.Paths.get(args[0]), out.toString().getBytes(StandardCharsets.UTF_8));
		System.out.println("wrote " + args[0] + " (" + out.length() + " chars)");
	}
}
