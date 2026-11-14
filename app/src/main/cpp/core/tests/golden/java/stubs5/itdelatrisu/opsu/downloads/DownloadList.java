package itdelatrisu.opsu.downloads;
import java.util.HashMap; import java.util.Map;
public class DownloadList {
	public static final Map<Integer, Download> map = new HashMap<Integer, Download>();
	private static final DownloadList list = new DownloadList();
	public static DownloadList get() { return list; }
	public Download getDownload(int id) { return map.get(id); }
}
