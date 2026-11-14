package fluddokt.opsu.fake;
/** Stub: Beatmap only uses the path (cache key) and isFile() (background loading, not ported). */
public class File {
	private final String path;
	public File(String path) { this.path = path; }
	public String getAbsolutePath() { return path; }
	public boolean isFile() { return true; }
}
