import java.nio.charset.StandardCharsets;

/** Seventh golden suite (see gen7.sh): the JDK's Double.toString / Double.parseDouble. Checked by tests/test_golden7.cpp, which reads
 *  the lines and compares util/JavaCompat's doubleToString / parseDouble (one-sided: no mirrored random generator). */
public class Golden7 {
	static StringBuilder out = new StringBuilder();
	static long seed = 7777777L;
	static long rnd64() { seed = seed * 6364136223846793005L + 1442695040888963407L; return seed; }
	static int rnd(int n) { return (int) ((rnd64() >>> 33) % n); }
	static String hx(double d) { return String.format("%016x", Double.doubleToLongBits(d)); }
	static void dts(double d) { out.append("DTS|").append(hx(d)).append('|').append(Double.toString(d)).append('\n'); }
	static void dp(String in) {
		String r;
		try { r = hx(Double.parseDouble(in)); } catch (NumberFormatException e) { r = "NFE"; }
		out.append("DP|").append(in).append('|').append(r).append('\n');
	}

	public static void main(String[] args) throws Exception {
		double[] special = { 0.0, -0.0, 1.0, -1.0, 0.1, 0.5, 1e-3, 9.999999999999999e-4, 1.0000000000000002e-3, 1e7, 9999999.999999998, 1.0000000000000002e7,
			1e21, 1e22, 1e23, 123456789.0, 1.0E-5, 100.0, 1e100, 1e-100, Double.MIN_VALUE, Double.MAX_VALUE, Double.MIN_NORMAL, Double.NaN,
			Double.POSITIVE_INFINITY, Double.NEGATIVE_INFINITY, 2e-3, 3.0, 12.0, 2.0e22, 4.35, 0.3, 0.30000000000000004, 5e-324, 1.0E23, 8.41E21, 2.2250738585072014E-308 };
		for (double d : special) dts(d);
		for (int i = 0; i < 4000; i++) dts(Double.longBitsToDouble(rnd64()));                               // any bit pattern
		for (int i = 0; i < 3000; i++) dts(rnd(100000000) / 1000.0);                                        // "nice" decimals up to 1e5
		for (int i = 0; i < 2000; i++) dts((rnd(2000001) - 1000000) * Math.pow(10, rnd(40) - 20));          // short mantissas, wide exponents
		for (int i = 0; i < 1000; i++) dts(rnd(9) + 1 + 0.0);                                                // one-digit values (JDK19+ two-digit rule)
		String[] ins = { "1", "-1", "+1", "1.", "  2.5  ", ".5", "5.", "1e3", "1E-3", "1e+3", "1e", "e1", "", "  ", "NaN", "-NaN", "+NaN", "Infinity", "-Infinity",
			"+Infinity", "inf", "nan", "infinity", "1d", "1D", "1f", "1F", "1.5f", "1.5d", "1_0", "1e400", "-1e400", "1e-400", "-1e-400", "4.9e-324", "2.4e-324", "2.5e-324",
			"2.2250738585072014E-308", "1.7976931348623157E308", "1.7976931348623159E308", "0.1", "0.30000000000000004", "123456789012345678901234567890",
			"1,5", "1 2", "--1", "+-1", "1e1e1", "1..2", "0", "-0", "+0", "00012", "0.000000000000000000000000000001", "9007199254740993", "9007199254740992.5",
			"1.00000000000000011102230246251565404236316680908203125", "1.00000000000000011102230246251565404236316680908203126", "\t3\n", "1.e2", ".e2", "-.5", "+.5e1" };
		for (String s : ins) dp(s);
		for (int i = 0; i < 3000; i++) {
			double d = Double.longBitsToDouble(rnd64());
			if (Double.isNaN(d) || Double.isInfinite(d)) continue;
			dp(Double.toString(d));
			dp(String.format("%.17e", d));
			dp(String.format("%.3f", d % 1000.0));
		}
		java.nio.file.Files.write(java.nio.file.Paths.get(args[0]), out.toString().getBytes(StandardCharsets.UTF_8));
		System.out.println("wrote " + args[0] + " (" + out.length() + " chars)");
	}
}
