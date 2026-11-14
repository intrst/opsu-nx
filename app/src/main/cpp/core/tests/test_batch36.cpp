// Tests for batch 36: Utils::deleteToTrash / deleteDirectory (behavioural, on real temporary files) and the first REAL DeleteHooks backend:
// BeatmapSetList::deleteSong / deleteSongGroup removing real files through Utils::deleteToTrash. The JDK comparison of the file effects is test_golden10.
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "opsu/Utils.h"
#include "opsu/beatmap/BeatmapSetList.h"
#include "opsu/downloads/Download.h"
#include "opsu/util/JavaIO.h"

using namespace opsu;
namespace fs = std::filesystem;
static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
#define THROWS(expr, T) do { bool t_ = false; try { (void)(expr); } catch (const T&) { t_ = true; } if (!t_) { std::printf("FAIL %s:%d  expected %s: %s\n", __FILE__, __LINE__, #T, #expr); failures++; } } while (0)

static void touch(const fs::path& p) { fs::create_directories(p.parent_path()); std::ofstream(p) << "x"; }
static bool has(const fs::path& p) { std::error_code ec; return fs::exists(p, ec) || fs::is_symlink(p, ec); }

int main() {
    char tmpl[] = "/tmp/opsu_b36_XXXXXX";
    if (!mkdtemp(tmpl)) return 2;
    const fs::path root = fs::canonical(tmpl);
    const std::string R = root.string();

    // ---- Utils::deleteToTrash / deleteDirectory
    {
        touch(root / "a/b/c.txt"); touch(root / "a/d.txt"); touch(root / "f.txt");
        CHECK(Utils::deleteToTrash(R + "/f.txt") == false);                 // always "deleted", never "moved to trash"
        CHECK(!has(root / "f.txt"));
        CHECK(Utils::deleteToTrash(R + "/a/") == false);                    // trailing '/', whole tree
        CHECK(!has(root / "a"));
        CHECK(has(root));
        std::string msg;
        try { Utils::deleteToTrash(R + "//nope//x/"); } catch (const java::IOException& e) { msg = e.what(); }
        CHECK(msg == "File '" + R + "/nope/x' does not exist.");           // normalised absolute path in the message
        msg.clear();
        try { Utils::deleteToTrash(""); } catch (const java::IOException& e) { msg = e.what(); }
        CHECK(msg == "File '" + fs::current_path().string() + "' does not exist.");  // empty path: the working directory
        msg.clear();
        try { Utils::deleteToTrash("relative_missing"); } catch (const java::IOException& e) { msg = e.what(); }
        CHECK(msg == "File '" + fs::current_path().string() + "/relative_missing' does not exist.");
        // deleteDirectory ignores a file, a missing path and the empty string (nothing is deleted, nothing thrown)
        touch(root / "keep.txt");
        Utils::deleteDirectory(R + "/keep.txt");
        Utils::deleteDirectory(R + "/missing");
        Utils::deleteDirectory("");
        CHECK(has(root / "keep.txt"));
        // a directory tree with a dotfile and a space in a name
        touch(root / "t/.hidden"); touch(root / "t/with space/y.osu");
        Utils::deleteDirectory(R + "/t");
        CHECK(!has(root / "t"));
        // a broken symlink does not exist for deleteToTrash (File.exists() follows links), a symlink to a file is removed without touching the target
        fs::create_symlink("nowhere", root / "broken");
        THROWS(Utils::deleteToTrash(R + "/broken"), java::IOException);
        CHECK(has(root / "broken"));
        fs::create_symlink("keep.txt", root / "lnk");
        CHECK(Utils::deleteToTrash(R + "/lnk") == false);
        CHECK(!has(root / "lnk") && has(root / "keep.txt"));
        // Java quirk, pinned: a symlink to a DIRECTORY is entered, the target's contents are deleted, then the link is removed
        touch(root / "target/inner.txt");
        fs::create_symlink("target", root / "dirlink");
        CHECK(Utils::deleteToTrash(R + "/dirlink") == false);
        CHECK(!has(root / "dirlink") && has(root / "target") && !has(root / "target/inner.txt"));
    }

    // ---- the real backend behind DeleteHooks::deleteToTrash
    {
        touch(root / "songs/dirA/A0.osu"); touch(root / "songs/dirA/A1.osu"); touch(root / "songs/dirA/audio.mp3"); touch(root / "songs/dirA/sub/x.txt");
        touch(root / "songs/dirB/B0.osu");
        BeatmapSetList l;
        BeatmapSetList::DeleteHooks h;
        h.deleteToTrash = [](const std::string& p) { Utils::deleteToTrash(p); };
        l.setDeleteHooks(h);
        const Beatmap::ModParams NOMOD = Beatmap::ModParams::of(0.f, 1.f, 1.f, 0.f);
        for (int i = 0; i < 2; i++) {
            std::string t(1, static_cast<char>('A' + i)), dir = R + "/songs/dir" + t;
            std::vector<Beatmap> v;
            for (int j = 0; j < (i == 0 ? 2 : 1); j++) {
                Beatmap b(dir + "/" + t + std::to_string(j) + ".osu");
                b.title = t; b.md5Hash = "h" + t + std::to_string(j); b.beatmapSetID = 100 + i; b.starRating = static_cast<float>(j + 1);
                v.push_back(b);
            }
            l.addSongGroup(v, NOMOD);
        }
        l.init();
        BeatmapSetNode* first = l.expand(0);   // expand A
        BeatmapSetNode* second = first;
        for (int k = 0; k < 6 && second && second->beatmapIndex != 1; k++) second = second->next;
        CHECK(first && second && second->beatmapIndex == 1);
        CHECK(l.deleteSong(second));                                      // A1.osu only
        CHECK(!has(root / "songs/dirA/A1.osu"));
        CHECK(has(root / "songs/dirA/A0.osu") && has(root / "songs/dirA/audio.mp3") && has(root / "songs/dirB/B0.osu"));
        CHECK(l.deleteSongGroup(l.getBaseNode(0)));                       // the whole directory of A, subfolder included
        CHECK(!has(root / "songs/dirA"));
        CHECK(has(root / "songs/dirB/B0.osu") && l.size() == 1);
        // the file is already gone: the IOException is reported ("Could not delete song group."), swallowed, and the list still changes
        fs::remove_all(root / "songs/dirB");
        std::string msg, what; int calls = 0; bool rep = false;
        Download::setErrorHandler([&](const std::string& m, const std::exception* e, bool report) { msg = m; rep = report; calls++; what = e ? e->what() : ""; });
        CHECK(l.deleteSongGroup(l.getBaseNode(0)));
        Download::setErrorHandler({});
        CHECK(calls == 1 && msg == "Could not delete song group." && rep && what == "File '" + R + "/songs/dirB' does not exist." && l.size() == 0);
    }

    fs::current_path("/");
    fs::remove_all(root);
    if (failures == 0) std::printf("batch36: all tests passed\n");
    return failures ? 1 : 0;
}
