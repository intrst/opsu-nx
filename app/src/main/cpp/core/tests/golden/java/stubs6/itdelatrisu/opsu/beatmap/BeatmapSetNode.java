package itdelatrisu.opsu.beatmap;
/** Test stub: a node that also remembers its index in the input list. */
public class BeatmapSetNode {
	private final BeatmapSet set;
	public final int idx;
	public BeatmapSetNode(BeatmapSet set, int idx) { this.set = set; this.idx = idx; }
	public BeatmapSet getBeatmapSet() { return set; }
}
