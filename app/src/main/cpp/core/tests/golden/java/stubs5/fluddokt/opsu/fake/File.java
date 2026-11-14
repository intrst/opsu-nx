package fluddokt.opsu.fake;
/** Stub: DownloadNode only builds a path with it. */
public class File {
	private final String path;
	public File(String path) { this.path = path; }
	public File(File dir, String name) { this.path = dir.path + "/" + name; }
	public String getAbsolutePath() { return path; }
}
