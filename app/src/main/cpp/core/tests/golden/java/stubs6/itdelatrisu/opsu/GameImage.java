package itdelatrisu.opsu;
import fluddokt.opsu.fake.Image;
/** Test stub: only MENU_TAB, whose image the driver sets. */
public enum GameImage {
	MENU_TAB;
	public static Image tab = new Image(1, 1);
	public Image getImage() { return tab; }
}
