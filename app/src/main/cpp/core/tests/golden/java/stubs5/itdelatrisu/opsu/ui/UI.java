package itdelatrisu.opsu.ui;
import fluddokt.opsu.fake.Color;
import fluddokt.opsu.fake.Graphics;
public class UI {
	public static class Notif { public void sendNotification(String m, Color c) {} }
	public static Notif getNotificationManager() { return new Notif(); }
	public static void drawScrollbar(Graphics g, float p, float t, float lh, float x, float y, float w, float h, Color a, Color b, boolean c) {}
}
