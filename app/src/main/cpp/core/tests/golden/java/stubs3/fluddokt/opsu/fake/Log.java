package fluddokt.opsu.fake;
/** Stub that counts calls (the golden driver prints the counters). */
public class Log {
	public static int debugCount = 0, warnCount = 0, errorCount = 0;
	public static String lastWarn = "";
	public static void debug(String s) { debugCount++; }
	public static void warn(String s) { warnCount++; lastWarn = s; }
	public static void warn(String s, Throwable t) { warnCount++; lastWarn = s; }
	public static void error(String s) { errorCount++; }
	public static void error(String s, Throwable t) { errorCount++; }
}
