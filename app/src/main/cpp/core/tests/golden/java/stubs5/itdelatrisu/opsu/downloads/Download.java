package itdelatrisu.opsu.downloads;
import fluddokt.opsu.fake.File;
import java.util.Map;
/** Stub with settable status / progress. */
public class Download {
	public enum Status { WAITING, DOWNLOADING, COMPLETE, CANCELLED, ERROR }
	public interface DownloadListener { void completed(); void error(); }
	public Status status = Status.WAITING; public float progress = 0f;
	public Download(String u, File p, String r) {}
	public void setListener(DownloadListener l) {} public void setRequestHeaders(Map<String, String> h) {}
	public void setSSLCertValidation(boolean b) {}
	public Status getStatus() { return status; }
	public float getProgress() { return progress; }
}
