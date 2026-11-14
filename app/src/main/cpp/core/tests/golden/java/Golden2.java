import itdelatrisu.opsu.GameData;
import itdelatrisu.opsu.GameMod;
import itdelatrisu.opsu.Utils;
import itdelatrisu.opsu.beatmap.Health;
import itdelatrisu.opsu.io.OsuReader;
import itdelatrisu.opsu.io.OsuWriter;
import itdelatrisu.opsu.ui.KineticScrolling;
import itdelatrisu.opsu.ui.animations.AnimatedValue;
import itdelatrisu.opsu.ui.animations.AnimationEquation;
import itdelatrisu.opsu.ui.animations.AnimationTrack;
import itdelatrisu.opsu.ui.animations.EasedSlide;
import itdelatrisu.opsu.ui.animations.LazerAnimations;
import itdelatrisu.opsu.user.User;
import itdelatrisu.opsu.user.UserStats;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

/** Second golden suite (see gen2.sh). Mirrored line for line by tests/test_golden2.cpp. */
public class Golden2 {
	static StringBuilder out = new StringBuilder();
	static long seed = 987654321L;
	static int rnd() { seed = seed * 6364136223846793005L + 1442695040888963407L; return (int) (seed >>> 33); }
	static int rnd(int n) { return rnd() % n; }
	static String hx(float f) { return String.format("%08x", Float.floatToIntBits(f)); }   // canonical NaN: payload is platform specific
	static String hx(double d) { return String.format("%016x", Double.doubleToLongBits(d)); }
	static void line(String s) { out.append(s).append('\n'); }
	static String bytesHex(byte[] b) { StringBuilder s = new StringBuilder(); for (byte x : b) s.append(String.format("%02x", x & 0xFF)); return s.toString(); }

	// ------------------------------------------------------------------ easing
	static void ease() {
		AnimationEquation[] eq = AnimationEquation.values();
		for (int i = 0; i < eq.length; i++) line("ENAME " + i + " " + eq[i].name());
		List<Float> ts = new ArrayList<Float>();
		float[] fixed = {0f, 1f, 0.5f, 0.25f, 0.75f, 1e-6f, 0.99999994f, 0.36363636f, 0.72727273f, 0.90909091f, 0.45f, 0.55f, -0.0f};
		for (float f : fixed) ts.add(f);
		for (int i = -16; i <= 80; i++) ts.add(i / 64f);
		for (int i = 0; i < 150; i++) ts.add((rnd(2000) - 500) / 1000f);
		for (int i = 0; i < eq.length; i++)
			for (float t : ts)
				line("E " + i + " " + hx(t) + " " + hx(eq[i].calc(t)));
	}

	// ------------------------------------------------------------------ AnimatedValue / EasedSlide
	static void animValue() {
		AnimationEquation[] eq = AnimationEquation.values();
		for (int round = 0; round < 40; round++) {
			int dur = 1 + rnd(2000);
			float min = (rnd(2000) - 1000) / 10f, max = (rnd(2000) - 1000) / 10f;
			AnimatedValue v = new AnimatedValue(dur, min, max, eq[rnd(eq.length)]);
			StringBuilder s = new StringBuilder("AV " + round + " " + dur + ":");
			for (int step = 0; step < 30; step++) {
				switch (rnd(6)) {
				case 0: v.setTime(rnd(dur * 2) - 100); break;
				case 1: v.setDuration(1 + rnd(2500)); break;
				case 2: v.setEquation(eq[rnd(eq.length)]); break;
				default: s.append(v.update(rnd(300) - 40) ? "T" : "F"); break;
				}
				s.append(' ').append(v.getTime()).append('/').append(v.getDuration()).append('/').append(hx(v.getValue())).append(v.isFinished() ? "!" : "");
			}
			line(s.toString());
		}
	}

	static void slide() {
		AnimationEquation[] eq = AnimationEquation.values();
		for (int round = 0; round < 30; round++) {
			EasedSlide s = new EasedSlide(rnd(600) - 50, rnd(600) - 50, eq[rnd(eq.length)]);
			StringBuilder b = new StringBuilder("SL " + round + ":");
			for (int step = 0; step < 40; step++) {
				switch (rnd(8)) {
				case 0: s.open(); break;
				case 1: s.close(); break;
				case 2: s.snap(rnd(2) == 0); break;
				default: b.append(s.update(rnd(120)) ? "T" : "F"); break;
				}
				b.append(' ').append(hx(s.get())).append(s.isOpening() ? 'o' : 'c').append(s.isVisible() ? 'v' : 'h').append(s.isIdle() ? 'i' : 'm');
			}
			line(b.toString());
		}
	}

	// ------------------------------------------------------------------ AnimationTrack / LazerAnimations
	static void track(String name, AnimationTrack t) {
		line("TD " + name + " " + hx(t.getDuration()) + " " + hx(t.getFinalValue()));
		for (float x = -10f; x < 2000f; x += 13.7f) line("TV " + name + " " + hx(x) + " " + hx(t.valueAt(x)));
		for (float x : new float[]{0f, 96f, 120f, 120.00001f, 1100f, 1400f}) line("TV " + name + " " + hx(x) + " " + hx(t.valueAt(x)));
	}

	static void lazer() {
		track("JA", LazerAnimations.JUDGEMENT_ALPHA);
		track("JH", LazerAnimations.JUDGEMENT_HIT_SCALE);
		track("JMS", LazerAnimations.JUDGEMENT_MISS_SCALE);
		track("JMY", LazerAnimations.JUDGEMENT_MISS_Y);
		track("JMR", LazerAnimations.JUDGEMENT_MISS_ROTATION);
		track("LS", LazerAnimations.LIGHTING_SCALE);
		track("LA", LazerAnimations.LIGHTING_ALPHA);
		AnimationEquation[] eq = AnimationEquation.values();
		AnimationTrack custom = new AnimationTrack(2f).to(5f, -30f, AnimationEquation.OUT_BOUNCE).delay(-1f).to(-3f, 50f, AnimationEquation.IN_OUT_ELASTIC).set(7f).to(1f, 0f, AnimationEquation.LINEAR);
		for (int i = 0; i < 9; i++) custom.to(i, 10f + i, eq[i * 3]);   // forces the Java arrays to grow
		track("CU", custom);
		for (int i = 0; i < 60; i++) {
			float t = (rnd(3000) - 500) / 7f, st = (rnd(1000) - 200) / 3f, ms = (rnd(5) == 0) ? 0f : rnd(900) / 3f;
			AnimationEquation e = eq[rnd(eq.length)];
			line("TP " + hx(t) + " " + hx(st) + " " + hx(ms) + " " + e.ordinal() + " " + hx(AnimationTrack.progress(t, st, ms, e)));
		}
		line("LC " + hx(LazerAnimations.SPINNER_SPIN_Y) + " " + hx(LazerAnimations.SPINNER_CLEAR_Y) + " " + hx(LazerAnimations.SPINNER_BONUS_Y)
			+ " " + hx(LazerAnimations.SPINNER_GLOW_R) + " " + hx(LazerAnimations.SPINNER_GLOW_G) + " " + hx(LazerAnimations.SPINNER_GLOW_B)
			+ " " + LazerAnimations.FOLLOW_INITIAL_TIME + " " + LazerAnimations.SPINNER_SPM_WINDOW + " " + LazerAnimations.SPINNER_METRE_BARS);
		for (int i = 0; i < 400; i++) {
			int ar = rnd(2500) - 100, td = rnd(3000) - 800;
			float t = (rnd(4000) - 500) / 5f, d = (rnd(2000) - 200) / 3f, prog = (rnd(1300) - 100) / 1000f;
			float base = rnd(1000) / 1000f, tgt = (rnd(2000) - 1000) / 10f, cur = (rnd(2000) - 1000) / 10f;
			line("LZ " + LazerAnimations.fadeInTime(ar) + " " + hx(LazerAnimations.approachScale(td, ar)) + " "
				+ hx(LazerAnimations.approachAlpha(td, ar, LazerAnimations.fadeInTime(ar))) + " "
				+ hx(LazerAnimations.approachAlpha(td, ar, rnd(500) - 50)) + " "
				+ hx(LazerAnimations.hitCircleProgress(t)) + " " + hx(LazerAnimations.hitCircleAlpha(t)) + " "
				+ hx(LazerAnimations.followEndScale(t)) + " " + hx(LazerAnimations.followEndAlpha(t)) + " "
				+ hx(LazerAnimations.reverseArrowScale(t, d)) + " " + hx(LazerAnimations.kiaiFlashAlpha(t, d)) + " "
				+ hx(LazerAnimations.damp(cur, tgt, base, d)) + " " + hx(LazerAnimations.spinnerAlpha(t, d, (float) ar)) + " "
				+ hx(LazerAnimations.spinnerApproachScale(t, d)) + " "
				+ LazerAnimations.spinnerMetreBars(prog, rnd(2) == 0, rnd(9) == 0, rnd(1000) / 1000f) + " "
				+ hx(LazerAnimations.spinnerNewStyleScale(prog)) + " " + hx(LazerAnimations.spinnerSpmHidden(t, d)) + " "
				+ hx(LazerAnimations.outQuad(t, d)));
		}
	}

	// ------------------------------------------------------------------ KineticScrolling
	static void kinetic() {
		for (int round = 0; round < 8; round++) {
			KineticScrolling k = new KineticScrolling();
			k.setAllowOverScroll(round % 2 == 1);
			if (round >= 2) k.setMinMax(-500f, 1500f);
			if (round >= 4) k.setSpeedMultiplier(2.5f);
			StringBuilder b = new StringBuilder("K " + round + ":");
			for (int step = 0; step < 250; step++) {
				switch (rnd(12)) {
				case 0: k.pressed(); break;
				case 1: k.released(); break;
				case 2: case 3: case 4: k.dragged((rnd(200) - 100) * 0.37f); break;
				case 5: k.scrollToPosition((rnd(3000) - 1000) * 1f); break;
				case 6: k.scrollOffset((rnd(400) - 200) / 3f); break;
				case 7: k.setPosition(rnd(2000) - 500); break;
				case 8: k.addOffset(rnd(100) / 4f); break;
				default: k.update(rnd(90) + 1); break;
				}
				b.append(' ').append(hx(k.getPosition())).append('/').append(hx(k.getTargetPosition())).append(k.isPressed() ? 'p' : 'r');
			}
			line(b.toString());
		}
		KineticScrolling k = new KineticScrolling();
		line("KD " + hx(k.getMin()) + " " + hx(k.getMax()) + " " + hx(k.getSpeedMultiplier()));
		for (float m : new float[]{0f, -1f, 0.5f}) {
			try { k.setSpeedMultiplier(m); line("KM " + hx(m) + " ok"); } catch (IllegalArgumentException e) { line("KM " + hx(m) + " IAE " + e.getMessage()); }
		}
		// .5 rounding of Math.round
		for (float p : new float[]{0.5f, 1.5f, 2.5f, -0.5f, -1.5f, -2.5f, 0.49999997f}) {
			KineticScrolling r = new KineticScrolling();
			r.setPosition(p);
			r.pressed(); r.released();
			line("KR " + hx(p) + " " + hx(r.getTargetPosition()));
		}
	}

	// ------------------------------------------------------------------ User / UserStats / grades
	static String u8(String s) { return bytesHex(s.getBytes(StandardCharsets.UTF_8)); }

	static void user() throws Exception {
		Method gsl = User.class.getDeclaredMethod("getScoreForLevel", int.class);
		gsl.setAccessible(true);
		for (int l = -2; l <= 125; l++) line("UL " + l + " " + gsl.invoke(null, l));
		line("UL 1000 " + gsl.invoke(null, 1000));
		long[] scores = {0, 1, 2, 4999, 5000, 123456789L, 26931190828L, 26931190829L, 26931190830L, 126931190829L, 5000000000000L, Long.MAX_VALUE / 4};
		for (long s : scores) {
			User u = new User("n", s, 98.5, 3, 4, 1);
			line("UU " + s + " " + u.getLevel() + " " + hx(u.getNextLevelProgress()));
		}
		for (int i = 0; i < 60; i++) {
			long s = (long) rnd() * rnd(100000) + rnd();
			User u = new User("x", s, rnd(10000) / 100.0, rnd(50), rnd(80), 3);
			line("UR " + s + " " + u.getLevel() + " " + hx(u.getNextLevelProgress()));
		}
		User u = new User("Player", 2);
		StringBuilder b = new StringBuilder("UA");
		for (int i = 0; i < 80; i++) {
			if (rnd(3) == 0) u.add(rnd(900000));
			else u.add((long) rnd(2000000) * 3, rnd(10001) / 100.0);
			b.append(' ').append(u.getScore()).append('/').append(hx(u.getAccuracy())).append('/').append(u.getPassedPlays()).append('/').append(u.getTotalPlays())
				.append('/').append(u.getLevel()).append('/').append(hx(u.getNextLevelProgress()));
		}
		line(b.toString());
		// customisation
		User c = new User("C", 5L, 1.0, 1, 1, 7, 3, null, "av.png", null);
		line("UC " + c.getBanner() + "|" + c.getBannerFile() + "|" + c.getAvatarFile() + "|" + c.getBio() + "|" + c.getIconId());
		c.setBannerFile("b.png"); line("UC " + c.getBanner() + "|" + c.getBannerFile());
		c.setBannerFile(null);    line("UC " + c.getBanner() + "|" + c.getBannerFile());
		c.setBanner(4);           line("UC " + c.getBanner() + "|" + c.getBannerFile());
		c.setAvatarFile(null);    line("UC [" + c.getAvatarFile() + "]");
		StringBuilder longBio = new StringBuilder();
		for (int i = 0; i < 100; i++) longBio.append((char) ('a' + i % 26));
		String[] bios = {null, "", "short", longBio.toString(), longBio.substring(0, 90), longBio.substring(0, 89),
			"ééé" + longBio.substring(0, 95), "中文" + longBio.substring(0, 95), "x😀y" + longBio.substring(0, 95)};
		for (String s : bios) { c.setBio(s); line("UB " + c.getBio().length() + " " + u8(c.getBio())); }
		String[] names = {"alice", "Alice", "ALICE", "bob", "Bob", "alicE2", "al", "", "Zed", "zed", "[x]", "_y", "a b"};
		for (String x : names) for (String y : names)
			line("UCMP " + Integer.signum(new User(x, 1).compareTo(new User(y, 1))));
	}

	static void grades() {
		line("GD " + GameData.HIT_MISS + " " + GameData.HIT_300 + " " + GameData.HIT_MU + " " + GameData.HIT_MAX);
		int[] cnt = {0, 1, 2, 3, 5, 9, 10, 19, 50, 99, 100, 101, 333, 1000};
		for (int a : cnt) for (int b : new int[]{0, 1, 7, 50}) for (int c : new int[]{0, 1, 3}) for (int d : new int[]{0, 1, 2})
			line("GR " + a + " " + b + " " + c + " " + d + " " + hx(GameData.getScorePercent(a, b, c, d)) + " "
				+ GameData.getGrade(a, b, c, d, false) + " " + GameData.getGrade(a, b, c, d, true));
		for (int i = 0; i < 300; i++) {
			int a = rnd(500), b = rnd(60), c = rnd(20), d = rnd(15) / 8;
			line("GRR " + a + " " + b + " " + c + " " + d + " " + hx(GameData.getScorePercent(a, b, c, d)) + " " + GameData.getGrade(a, b, c, d, rnd(2) == 0));
		}
	}

	static void stats() {
		int[] modBits = {0, GameMod.HIDDEN.getBit(), GameMod.FLASHLIGHT.getBit(), GameMod.HARD_ROCK.getBit(), GameMod.HIDDEN.getBit() | GameMod.FLASHLIGHT.getBit()};
		for (int round = 0; round < 12; round++) {
			UserStats s = new UserStats();
			int n = (round == 0) ? 0 : (round * 9) % 70;
			for (int i = 0; i < n; i++) {
				UserStats.Play p = new UserStats.Play();
				p.timestamp = (rnd(5) == 0) ? 0 : 1600000000L + rnd(10000000);
				p.title = "t" + i; p.artist = "a"; p.version = "v";
				p.hit300 = rnd(300); p.hit100 = rnd(40); p.hit50 = rnd(10); p.miss = rnd(4) == 0 ? rnd(5) : 0;
				p.score = (rnd(4) == 0) ? 100000L : (long) rnd(3000) * 1000;   // many ties -> checks sort stability
				p.combo = rnd(900); p.mods = modBits[rnd(modBits.length)]; p.perfect = rnd(3) == 0;
				if (rnd(6) == 0) { p.hit100 = p.hit50 = p.miss = 0; }
				s.add(p);
			}
			s.finish();
			StringBuilder b = new StringBuilder("US " + round + " " + s.plays + " " + s.hit300 + " " + s.hit100 + " " + s.hit50 + " " + s.miss + " "
				+ s.bestScore + " " + s.bestCombo + " " + s.fullCombos + " " + s.gradeSS + " " + s.gradeS + " " + s.gradeA + " " + s.gradeB + " " + s.gradeC + " " + s.gradeD
				+ " " + s.firstPlayed + " " + s.lastPlayed + " " + hx(s.averageAccuracy) + " " + s.getTotalHits() + " :");
			for (UserStats.Play p : s.topPlays) b.append(' ').append(p.title).append('/').append(p.score).append('/').append(hx(p.accuracy)).append('/').append(p.grade);
			line(b.toString());
		}
	}

	// ------------------------------------------------------------------ Health
	static void health() {
		int[] hits = {GameData.HIT_MISS, GameData.HIT_50, GameData.HIT_100, GameData.HIT_300, GameData.HIT_100K, GameData.HIT_300K, GameData.HIT_300G,
			GameData.HIT_SLIDER10, GameData.HIT_SLIDER30, GameData.HIT_MAX, GameData.HIT_SLIDER_REPEAT, GameData.HIT_SPINNERSPIN, GameData.HIT_SPINNERBONUS, GameData.HIT_MU};
		for (int round = 0; round < 16; round++) {
			Health h = new Health();
			h.setModifiers(round * 0.7f, 1f + round * 0.03f, 1f + round * 0.07f);
			StringBuilder b = new StringBuilder("HP " + round + ":");
			for (int step = 0; step < 120; step++) {
				switch (rnd(9)) {
				case 0: h.update(rnd(100)); break;
				case 1: h.setHealth(rnd(260) - 30); break;
				case 2: h.changeHealth((rnd(400) - 200) / 3f); break;
				case 3: if (rnd(10) == 0) h.reset(); break;
				default: h.changeHealthForHit(hits[rnd(hits.length)]); break;
				}
				b.append(' ').append(hx(h.getRawHealth())).append('/').append(hx(h.getUncappedRawHealth())).append('/').append(hx(h.getHealth())).append('/').append(hx(h.getHealthDisplay()));
			}
			line(b.toString());
		}
	}

	// ------------------------------------------------------------------ Utils
	static void utils() {
		for (int i = 0; i < 200; i++) {
			float a = (rnd(20000) - 10000) / 7f, b = (rnd(20000) - 10000) / 7f, c = (rnd(20000) - 10000) / 7f, d = (rnd(20000) - 10000) / 7f;
			line("UD " + hx(Utils.distance(a, b, c, d)) + " " + hx(Utils.lerp(a, b, rnd(1000) / 1000f)));
		}
		for (int i = -3; i <= 53; i++) {
			float d = i / 5f;
			line("MD " + hx(d) + " " + hx(Utils.mapDifficultyRange(d, 195, 160, 60)) + " " + hx(Utils.mapDifficultyRange(d, -6, -25, -40)) + " " + hx(Utils.mapDifficultyRange(d, 1800, 1200, 450)));
		}
		for (int round = 0; round < 25; round++) {
			List<Integer> l = new ArrayList<Integer>();
			int n = 1 + rnd(60);
			for (int i = 0; i < n; i++) l.add(rnd(400) - 200);
			line("SD " + n + " " + hx(Utils.standardDeviation(l)));
		}
		line("SD0 " + hx(Utils.standardDeviation(new ArrayList<Integer>())));
		int[] secs = {-5, 0, 1, 2, 59, 60, 61, 599, 3599, 3600, 3661, 86399, 100000, 360000};
		for (int s : secs) line("TS " + s + " " + Utils.getTimeString(s));
		long[] bytes = {0, 1, 1023, 1024, 1025, 1536, 1048575, 1048576, 123456789L, 1073741824L, 5368709120L, 1099511627776L, 1125899906842624L, 1152921504606846976L, Long.MAX_VALUE};
		for (long x : bytes) line("BY " + x + " " + Utils.bytesToString(x));
		for (int i = 0; i < 200; i++) { long x = 1024L + ((long) rnd() << rnd(30)) + rnd(); if (x < 0) x = -x; line("BYR " + x + " " + Utils.bytesToString(x)); }
		String[] names = {"normal.osu", "a<b>c", "he said \"hi\"", "p|q", "col:on", "q?m*s", "back\\slash/fwd", "tab\there", "é中文.osz", "ctrl\u0001\u001fx", "", "  sp  "};
		for (String s : names) {
			line("CF " + u8(s) + " " + u8(Utils.cleanFileName(s, (char) 0)) + " " + u8(Utils.cleanFileName(s, '_')) + " " + u8(Utils.cleanFileName(s, '*')) + " " + u8(Utils.cleanFileName(s, '-')));
		}
	}

	// ------------------------------------------------------------------ OsuReader / OsuWriter
	static void io() throws IOException {
		ByteArrayOutputStream bos = new ByteArrayOutputStream();
		OsuWriter w = new OsuWriter(bos);
		w.write((byte) -5); w.write((byte) 127);
		w.write((short) -2); w.write((short) 0x1234);
		w.write(0); w.write(-1); w.write(0x01020304); w.write(Integer.MIN_VALUE);
		w.write(0L); w.write(-2L); w.write(0x0102030405060708L); w.write(Long.MIN_VALUE);
		w.write(1.5f); w.write(-0f); w.write(Float.NaN); w.write(3.4028235E38f);
		w.write(Math.PI); w.write(-0.0); w.write(Double.NEGATIVE_INFINITY);
		w.write(true); w.write(false);
		for (int v : new int[]{0, 1, 127, 128, 300, 16383, 16384, 2097151, 268435455, 0x7FFFFFFF}) w.writeULEB128(v);
		w.write(""); w.write((String) null); w.write("hello"); w.write("Zenith (TV Size)");
		StringBuilder big = new StringBuilder();
		for (int i = 0; i < 200; i++) big.append((char) ('A' + i % 26));
		w.write(big.toString());
		w.write(new Date(0L)); w.write(new Date(1600000000123L)); w.write(new Date(-86400000L)); w.write(new Date(253402300799999L));
		w.write(new byte[]{1, 2, (byte) 0xFF});
		w.write("café"); w.write("中文abc"); w.write("x😀y");   // low-byte quirk
		w.close();
		byte[] bytes = bos.toByteArray();
		line("OW " + bytes.length + " " + bytesHex(bytes));

		OsuReader r = new OsuReader(new ByteArrayInputStream(bytes));
		line("OR " + r.readByte() + " " + r.readByte() + " " + r.readShort() + " " + r.readShort());
		line("OR " + r.readInt() + " " + r.readInt() + " " + r.readInt() + " " + r.readInt());
		line("OR " + r.readLong() + " " + r.readLong() + " " + r.readLong() + " " + r.readLong());
		// floats were written big endian but are read little endian: the reader sees the byte-swapped value
		line("OR " + hx(r.readSingle()) + " " + hx(r.readSingle()) + " " + hx(r.readSingle()) + " " + hx(r.readSingle()));
		line("OR " + hx(r.readDouble()) + " " + hx(r.readDouble()) + " " + hx(r.readDouble()));
		line("OR " + r.readBoolean() + " " + r.readBoolean());
		for (int i = 0; i < 10; i++) line("OR uleb " + r.readULEB128());
		line("OR str [" + r.readString() + "] [" + r.readString() + "] [" + r.readString() + "] [" + r.readString() + "]");
		line("OR str " + r.readString().length());
		for (int i = 0; i < 4; i++) line("OR date " + r.readDate().getTime());

		// hand-made byte streams, including malformed ones
		String[] cases = {"", "01", "0102", "010203", "01020304", "0b", "0b05", "0b0568656c6c", "0b0568656c6c6f", "00", "0a", "0b00", "0b8001",
			"ffffffff0f", "ffffffffff0f", "80808080", "0b03e4b8ad", "0b06e4b8ade69687", "0bff", "0b0368ff69", "0b81808080", "0bffffffff0f", "0b80808080",
			"0b02c328", "0b03e082a9", "0b03eda080", "0b03f09f98", "0b04f4908080", "0b02c080", "0b01c3", "0b03e4b8", "0b0480ffc2c2", "0b06f09f9880f09f", "0b04f09f9880"};
		for (String hex : cases) {
			byte[] b = new byte[hex.length() / 2];
			for (int i = 0; i < b.length; i++) b[i] = (byte) Integer.parseInt(hex.substring(2 * i, 2 * i + 2), 16);
			line("OC " + hex + " " + tryRead(b, 0) + " " + tryRead(b, 1) + " " + tryRead(b, 2) + " " + tryRead(b, 3) + " " + tryRead(b, 4) + " " + tryRead(b, 5));
		}
	}

	/** Random invalid-heavy byte strings through readString (the JDK's lenient UTF-8 decoder is the oracle). */
	static void utf8fuzz() {
		int[] pool = {0x41, 0x7f, 0x80, 0x8f, 0x9f, 0xa0, 0xbf, 0xc0, 0xc1, 0xc2, 0xdf, 0xe0, 0xe1, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf3, 0xf4, 0xf5, 0xff, 0x90, 0xa9};
		for (int i = 0; i < 6000; i++) {
			int n = 1 + rnd(9);
			byte[] b = new byte[n + 2];
			b[0] = 0x0b; b[1] = (byte) n;
			for (int k = 0; k < n; k++) b[2 + k] = (byte) pool[rnd(pool.length)];
			line("OF " + bytesHex(b) + " " + tryRead(b, 4));
		}
	}

	static String tryRead(byte[] b, int what) {
		try {
			OsuReader r = new OsuReader(new ByteArrayInputStream(b));
			switch (what) {
			case 0: return "i=" + r.readInt();
			case 1: return "s=" + r.readShort();
			case 2: return "l=" + r.readLong();
			case 3: return "u=" + r.readULEB128();
			case 4: { String s = r.readString(); return "str=" + u8(s); }
			default: return "d=" + r.readDate().getTime();
			}
		} catch (java.io.EOFException e) {
			return "EOF";
		} catch (IOException e) {
			return "IO:" + e.getMessage();
		} catch (RuntimeException e) {
			return "RT";
		}
	}

	public static void main(String[] args) throws Exception {
		ease(); animValue(); slide(); lazer(); kinetic(); user(); grades(); stats(); health(); utils(); io(); utf8fuzz();
		java.nio.file.Files.write(java.nio.file.Paths.get(args[0]), out.toString().getBytes(StandardCharsets.UTF_8));
		System.out.println("wrote " + args[0] + " (" + out.length() + " chars)");
	}
}
