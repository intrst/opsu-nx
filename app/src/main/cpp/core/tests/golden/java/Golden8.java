import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;

/** Eighth golden suite (see gen8.sh): the JDK's URLEncoder.encode(s, "UTF-8"). Checked by tests/test_golden8.cpp, which reads the
 *  lines (input as UTF-8 hex | the JDK's answer) and compares util/JavaCompat's urlEncode. One-sided: no mirrored random generator. */
public class Golden8 {
	static StringBuilder out = new StringBuilder();
	static long seed = 8888888L;
	static long rnd64() { seed = seed * 6364136223846793005L + 1442695040888963407L; return seed; }
	static int rnd(int n) { return (int) ((rnd64() >>> 33) % n); }
	static String hex(byte[] b) { StringBuilder sb = new StringBuilder(); for (byte x : b) sb.append(String.format("%02x", x & 0xff)); return sb.toString(); }
	static void enc(String s) throws Exception {
		out.append("UE|").append(hex(s.getBytes(StandardCharsets.UTF_8))).append('|').append(URLEncoder.encode(s, "UTF-8")).append('\n');
	}
	static int randomCodePoint() {
		switch (rnd(6)) {
			case 0: return 0x20 + rnd(0x5f);                       // printable ASCII
			case 1: return rnd(0x80);                              // all ASCII incl. controls
			case 2: return 0x80 + rnd(0x780);                      // 2-byte
			case 3: { int c = 0x800 + rnd(0xf800); return (c >= 0xd800 && c <= 0xdfff) ? 0x4e00 + rnd(0x1000) : c; }  // 3-byte, no surrogates
			case 4: return 0x10000 + rnd(0x100000);                // 4-byte
			default: return "abcXYZ019 .-*_~!'()+&=%/?#:@,;$\"<>[]{}|\\^`".charAt(rnd(42));  // URL-significant chars
		}
	}
	public static void main(String[] args) throws Exception {
		String[] fixed = { "", " ", "  ", "a", "A", "Z", "0", "9", ".", "-", "*", "_", "~", "!", "'", "(", ")", "+", "%", "%20", "&", "=", "/", "?", "#", ":", "@", ",", ";", "$",
			"\"", "<", ">", "[", "]", "{", "}", "|", "\\", "^", "`", "\n", "\r\n", "\t", "\0", "\u007f", "hello world", "a b+c", "Camellia - Exit This Earth's Atomosphere (Camellia Mix)",
			"é", "ü", "日本語", "ゴッドスタイル", "😀", "a😀b", "\u00ff", "\u0100", "\u07ff", "\u0800", "\uffff", "\ufffd", "\ud83d\ude00", "\ud7ff", "\ue000", "\uf8ff",
			"A.B-C*D_E", "rank=ranked&q=a b", "100%", "50% off!", "line1\nline2", "tab\there", "emoji 🎵 song", "Привет мир", "مرحبا", "한국어 노래" };
		for (String s : fixed) enc(s);
		for (int i = 0; i < 6000; i++) {
			int n = rnd(24);
			StringBuilder sb = new StringBuilder();
			for (int j = 0; j < n; j++) sb.appendCodePoint(randomCodePoint());
			enc(sb.toString());
		}
		java.nio.file.Files.write(java.nio.file.Paths.get(args[0]), out.toString().getBytes(StandardCharsets.UTF_8));
	}
}
