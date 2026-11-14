package itdelatrisu.opsu;
/** Stub: only the pure helpers (bodies copied from the real Utils). */
public class Utils {
	public static int clamp(int val, int low, int high) { if (val < low) return low; if (val > high) return high; return val; }
	public static float clamp(float val, float low, float high) { if (val < low) return low; if (val > high) return high; return val; }
	public static double clamp(double val, double low, double high) { if (val < low) return low; if (val > high) return high; return val; }
	public static float lerp(float a, float b, float t) { return a * (1 - t) + b * t; }
	public static boolean parseBoolean(String s) { return (Integer.parseInt(s) == 1); }
}
