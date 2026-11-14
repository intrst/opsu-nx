package itdelatrisu.opsu.downloads.servers;
import java.util.Map;
public abstract class DownloadServer {
	public abstract String getDownloadURL(int id); public abstract Map<String, String> getDownloadRequestHeaders(); public abstract boolean disableSSLInDownloads();
}
