import java.nio.charset.StandardCharsets;
import java.text.DateFormat;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;

/** Ninth golden suite (see gen9.sh): the JDK's SimpleDateFormat as the download servers use it (parse a fixed pattern, print "d MMM yyyy HH:mm:ss"),
 *  once per time zone. Checked by tests/test_golden9.cpp (util/JavaDate), which reads the lines. One-sided: no mirrored random generator.
 *  Lines: "ZONE|id", "PD|patternId|hex(input)|millis or ERR" (parse only), "RF|patternId|hex(input)|hex(output)" (the servers' formatDate(String): output is the raw
 *  input when the parse fails), "FM|millis|text" (format only). */
public class Golden9 {
	static StringBuilder out = new StringBuilder();
	static long seed = 9999999L;
	static long rnd64() { seed = seed * 6364136223846793005L + 1442695040888963407L; return seed; }
	static int rnd(int n) { return (int) ((rnd64() >>> 33) % n); }
	static String hex(String s) { StringBuilder sb = new StringBuilder(); for (byte x : s.getBytes(StandardCharsets.UTF_8)) sb.append(String.format("%02x", x & 0xff)); return sb.toString(); }
	static final String[] PAT = { "yyyy-MM-dd'T'HH:mm:ss", "yyyy-MM-dd HH:mm:ss.SSS", "yyyy-MM-dd'T'HH:mm:ss'Z'" };
	static final boolean[] UTC = { true, false, true };

	static DateFormat fmt(int id) {
		DateFormat f = new SimpleDateFormat(PAT[id], Locale.US);
		if (UTC[id]) f.setTimeZone(TimeZone.getTimeZone("UTC"));
		return f;
	}
	static void both(int id, String in) {
		// parse only
		String r;
		try { r = Long.toString(fmt(id).parse(in).getTime()); } catch (ParseException e) { r = "ERR"; }
		out.append("PD|").append(id).append('|').append(hex(in)).append('|').append(r).append('\n');
		// the server idiom: reformat or return the raw string (the Catboy / Hinamizawa / Nekoha servers cut to 19 chars first)
		String base = (id == 0 && in.length() >= 19) ? in.substring(0, 19) : in;
		String res;
		try { res = new SimpleDateFormat("d MMM yyyy HH:mm:ss", Locale.US).format(fmt(id).parse(base)); } catch (ParseException e) { res = in; }
		out.append("RF|").append(id).append('|').append(hex(in)).append('|').append(hex(res)).append('\n');
	}
	static String two(int v) { return String.format("%02d", v); }
	static String iso(int y, int mo, int d, int h, int mi, int s, char sep) { return String.format("%04d-%02d-%02d%c%02d:%02d:%02d", y, mo, d, sep, h, mi, s); }

	static void zone(String id) {
		TimeZone.setDefault(TimeZone.getTimeZone(id));
		out.append("ZONE|").append(id).append('\n');
		for (int id2 = 0; id2 < 3; id2++) {
			String sep = id2 == 1 ? " " : "T";
			String tail = id2 == 1 ? ".123" : (id2 == 2 ? "Z" : "");
			// random ordinary dates
			for (int i = 0; i < 350; i++) {
				int y = 1990 + rnd(70), mo = 1 + rnd(12), d = 1 + rnd(28), h = rnd(24), mi = rnd(60), s = rnd(60);
				both(id2, iso(y, mo, d, h, mi, s, sep.charAt(0)) + tail);
			}
			// DST transitions of several zones, wall times around them (gap / overlap)
			int[][] dst = { {2023, 3, 12}, {2023, 11, 5}, {2023, 3, 26}, {2023, 10, 29}, {2023, 10, 1}, {2023, 4, 2}, {2023, 10, 22}, {2023, 4, 1}, {2023, 3, 10}, {2023, 11, 3} };
			for (int[] t : dst)
				for (int h = 0; h < 5; h++)
					for (int mi : new int[] { 0, 15, 30, 59 })
						for (int dd = -1; dd <= 1; dd++)
							both(id2, iso(t[0], t[1], Math.max(1, t[2] + dd), h, mi, 30, sep.charAt(0)) + tail);
			// the lenient / malformed cases
			String[] bad = { "", " ", "x", "2023", "2023-05", "2023-05-14", "2023-05-14T", "2023-05-14T23", "2023-05-14T23:38", "2023-05-14T23:38:4", "2023-05-14T23:38:47",
				"2023-05-14 23:38:47", "2023-05-14T23:38:47Z", "2023-05-14T23:38:47.5", "2023-05-14T23:38:47.123Z", "2023-05-14T23:38:47+09:00", "2023-05-14T23:38:47 trailing",
				"2023-13-14T23:38:47", "2023-00-14T23:38:47", "2023-05-32T23:38:47", "2023-05-00T23:38:47", "2023-02-30T23:38:47", "2023-05-14T24:00:00", "2023-05-14T99:99:99",
				"2023-05-14T23:60:00", "2023-05-14T23:59:61", "2023-5-4T3:8:7", "2023-05-14T23:38:47.9999", "2023-05-14 23:38:47.1", "2023-05-14 23:38:47.12345", "2023-05-14 23:38:47.",
				"2023-05-14 23:38:47.-5", "2023-05-14 23:38:47", " 2023-05-14T23:38:47", "2023- 05-14T23:38: 47", "2023-\t05-14T23:38:47", "2023-05-14T 23:38:47", "-2023-05-14T23:38:47",
				"2023--5-14T23:38:47", "2023-05--3T23:38:47", "2023-05-14T-1:38:47", "2023-05-14T23:-5:47", "2023-05-14T23:38:-3", "+2023-05-14T23:38:47", "2023-+5-14T23:38:47",
				"2023-05-14T23:38:47e1", "2023-05-14T2e1:38:47", "2023-05-14T23:38:NaN", "2023-05-14T23:38:\u221e", "20230-05-14T23:38:47", "02023-05-14T23:38:47", "0000-01-01T00:00:00",
				"0001-01-01T00:00:00", "1582-10-04T12:00:00", "1582-10-05T12:00:00", "1582-10-15T12:00:00", "1583-01-01T00:00:00", "1600-02-29T12:00:00", "1900-01-01T00:00:00", "1969-12-31T23:59:59",
				"1970-01-01T00:00:00", "2038-01-19T03:14:08", "2100-02-29T12:00:00", "2400-02-29T12:00:00", "9999-12-31T23:59:59", "10000-01-01T00:00:00", "12345-06-07T08:09:10",
				"2023-05-14T23:38:47\n", "2023/05/14T23:38:47", "2023-05-14t23:38:47", "2023-05-14T23.38.47", "23-05-14T23:38:47", "2023-05-14T23:38:47Z", "2023-05-14T23:38:47z",
				"2023-05-14TZ", "2023-05-14T23:38:47Zjunk", "2023-12-31T23:59:59", "2023-01-01T00:00:00", "2024-02-29T00:00:00", "2023-05-14T00:00:00", "123456789-01-01T00:00:00",
				"2023-1234567890-01T00:00:00", "2023-05-14T23:38:47\u0663", "2023-05-14T23:38:4\u0667" };
			for (String b : bad) both(id2, b + (id2 == 2 && !b.endsWith("Z") ? "" : ""));
			// more random overflow / garbage mixes built from number fragments
			String[] frag = { "0", "1", "00", "07", "12", "13", "31", "32", "59", "60", "61", "99", "100", "-1", "-12", "-13", "999", "2023", "1970", "1999", "2000", "2100", "-5" };
			for (int i = 0; i < 400; i++) {
				String s = frag[rnd(frag.length)] + "-" + frag[rnd(frag.length)] + "-" + frag[rnd(frag.length)] + (id2 == 1 ? " " : "T") + frag[rnd(frag.length)] + ":" + frag[rnd(frag.length)] + ":" + frag[rnd(frag.length)] + tail;
				both(id2, s);
			}
		}
		// format only (also pre-1970 and far future)
		long[] ms = { 0, 1, -1, 999, -999, 1000, 86399999L, -86400000L, 951782400000L, 1678602000000L, 1699164000000L, 4102444800000L, 253402300799000L, -2208988800000L, -5000000000000L,
			-12219292800000L, 32503680000000L, 7258118400000L };
		for (long m : ms) out.append("FM|").append(m).append('|').append(hex(new SimpleDateFormat("d MMM yyyy HH:mm:ss", Locale.US).format(new Date(m)))).append('\n');
		for (int i = 0; i < 1200; i++) {
			long m = (long) (rnd64() >>> 20) % 4000000000000L - 700000000000L;   // roughly 1947..2096
			out.append("FM|").append(m).append('|').append(hex(new SimpleDateFormat("d MMM yyyy HH:mm:ss", Locale.US).format(new Date(m)))).append('\n');
		}
	}

	public static void main(String[] args) throws Exception {
		Locale.setDefault(Locale.US);
		for (String z : new String[] { "UTC", "Asia/Jakarta", "America/New_York", "Europe/London", "Australia/Lord_Howe", "Asia/Kolkata", "Pacific/Chatham", "America/Sao_Paulo", "Asia/Tehran" })
			zone(z);
		java.nio.file.Files.write(java.nio.file.Paths.get(args[0]), out.toString().getBytes(StandardCharsets.UTF_8));
	}
}
