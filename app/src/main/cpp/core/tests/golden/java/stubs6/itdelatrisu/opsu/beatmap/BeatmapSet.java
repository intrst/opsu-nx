package itdelatrisu.opsu.beatmap;
import java.util.ArrayList;
import java.util.Iterator;
/** Test stub: a list of beatmaps (isFavorite is the real BeatmapSet.isFavorite). */
public class BeatmapSet implements Iterable<Beatmap> {
	public final ArrayList<Beatmap> beatmaps = new ArrayList<Beatmap>();
	public Iterator<Beatmap> iterator() { return beatmaps.iterator(); }
	public boolean isFavorite() {
		for (Beatmap map : beatmaps) {
			if (map.favorite)
				return true;
		}
		return false;
	}
}
