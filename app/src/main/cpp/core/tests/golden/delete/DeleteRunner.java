import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;

/** Runs the scenarios of scenarios.txt through the Java Utils.deleteToTrash / deleteDirectory (copied VERBATIM from itdelatrisu/opsu/Utils.java) on a fresh
 *  fixture each, and prints a transcript: the result or the exception message, then the remaining tree. The working directory is the fixture root.
 *  Usage (from the root): java DeleteRunner <scenarios.txt> <fixture.sh> <out> */
public class DeleteRunner {
	// ---- verbatim from Utils.java ----
	public static boolean deleteToTrash(File file) throws IOException {
		if (file == null)
			throw new IOException("File cannot be null.");
		if (!file.exists())
			throw new IOException(String.format("File '%s' does not exist.", file.getAbsolutePath()));
		if (file.isDirectory())
			deleteDirectory(file);
		else
			file.delete();
		return false;
	}
	public static void deleteDirectory(File dir) {
		if (dir == null || !dir.isDirectory())
			return;
		File[] files = dir.listFiles();
		if (files != null && files.length > 0) {
			for (File file : files) {
				if (file.isDirectory())
					deleteDirectory(file);
				else
					file.delete();
			}
		}
		dir.delete();
	}
	// ---- end ----

	static void tree(Path root, Path p, StringBuilder out) throws IOException {
		List<Path> kids = new ArrayList<>();
		try (DirectoryStream<Path> ds = Files.newDirectoryStream(p)) { for (Path k : ds) kids.add(k); }
		Collections.sort(kids, (a, b) -> a.getFileName().toString().compareTo(b.getFileName().toString()));
		for (Path k : kids) {
			String rel = root.relativize(k).toString();
			if (Files.isSymbolicLink(k)) out.append("  ").append(rel).append(" -> ").append(Files.readSymbolicLink(k)).append('\n');
			else if (Files.isDirectory(k)) { out.append("  ").append(rel).append("/\n"); tree(root, k, out); }
			else out.append("  ").append(rel).append('\n');
		}
	}

	public static void main(String[] args) throws Exception {
		Path root = Paths.get(System.getProperty("user.dir"));
		StringBuilder out = new StringBuilder();
		for (String line : Files.readAllLines(Paths.get(args[0]), StandardCharsets.UTF_8)) {
			if (line.isEmpty() || line.startsWith("#")) continue;
			int bar = line.indexOf('|');
			String name = line.substring(0, bar), path = line.substring(bar + 1);
			new ProcessBuilder("sh", args[1], root.toString()).inheritIO().start().waitFor();
			if (path.equals("<empty>")) path = "";
			path = path.replace("<abs>", root.toString());
			out.append("== ").append(name).append('\n');
			try { boolean r = deleteToTrash(new File(path)); out.append("returned ").append(r).append('\n'); }
			catch (IOException e) { out.append("IOException: ").append(e.getMessage()).append('\n'); }
			tree(root, root, out);
		}
		String s = out.toString().replace(root.toString(), "BASE");
		Files.write(Paths.get(args[2]), s.getBytes(StandardCharsets.UTF_8));
	}
}
