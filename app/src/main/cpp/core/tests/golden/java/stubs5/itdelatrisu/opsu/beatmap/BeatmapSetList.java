package itdelatrisu.opsu.beatmap;
import java.util.HashSet; import java.util.Set;
public class BeatmapSetList {
	public static final Set<Integer> ids = new HashSet<Integer>();
	private static final BeatmapSetList list = new BeatmapSetList();
	public static BeatmapSetList get() { return list; }
	public boolean containsBeatmapSetID(int id) { return ids.contains(id); }
}
