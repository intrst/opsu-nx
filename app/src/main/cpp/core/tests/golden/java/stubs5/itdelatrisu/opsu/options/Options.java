package itdelatrisu.opsu.options;
import fluddokt.opsu.fake.File;
public class Options {
	public static boolean unicode = false;
	public static boolean useUnicodeMetadata() { return unicode; }
	public static File getImportDir() { return new File("/import"); }
}
