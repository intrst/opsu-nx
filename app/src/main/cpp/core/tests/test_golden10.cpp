// Tenth golden suite: Utils::deleteToTrash / deleteDirectory on a real file tree, compared with the transcript the JDK produced for the same scenarios
// (tests/golden/delete/gen10.sh runs the Java Utils code, copied verbatim, through java.io.File). One fresh fixture per scenario (fixture.sh).
// usage: opsu_golden10 <golden/delete dir> <expected10.txt>   (needs a POSIX sh)
#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "opsu/Utils.h"
#include "opsu/util/JavaIO.h"

using namespace opsu;
namespace fs = std::filesystem;

static void tree(const fs::path& root, const fs::path& p, std::string& out) {
    std::vector<fs::path> kids;
    for (const auto& e : fs::directory_iterator(p)) kids.push_back(e.path());
    std::sort(kids.begin(), kids.end(), [](const fs::path& a, const fs::path& b) { return a.filename().u8string() < b.filename().u8string(); });
    for (const fs::path& k : kids) {
        std::string rel = k.lexically_relative(root).u8string();
        if (fs::is_symlink(k)) out += "  " + rel + " -> " + fs::read_symlink(k).u8string() + "\n";
        else if (fs::is_directory(k)) { out += "  " + rel + "/\n"; tree(root, k, out); }
        else out += "  " + rel + "\n";
    }
}

int main(int argc, char** argv) {
    if (argc < 3) { std::printf("usage: %s <golden/delete dir> <expected10.txt>\n", argv[0]); return 2; }
    std::string dir = fs::absolute(argv[1]).string(), expectedPath = fs::absolute(argv[2]).string();  // absolute: the run changes the working directory
    char tmpl[] = "/tmp/opsu_golden10_XXXXXX";
    if (!mkdtemp(tmpl)) { std::printf("mkdtemp failed\n"); return 2; }
    std::string rootStr = fs::canonical(tmpl).string();
    fs::path root = rootStr;
    fs::current_path(root);
    std::ifstream sc(dir + "/scenarios.txt");
    std::string line, out;
    while (std::getline(sc, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t bar = line.find('|');
        std::string name = line.substr(0, bar), path = line.substr(bar + 1);
        std::string cmd = "sh '" + dir + "/fixture.sh' '" + rootStr + "'";
        if (std::system(cmd.c_str()) != 0) { std::printf("fixture failed\n"); return 2; }
        if (path == "<empty>") path = "";
        for (size_t at; (at = path.find("<abs>")) != std::string::npos;) path.replace(at, 5, rootStr);
        out += "== " + name + "\n";
        try { out += std::string("returned ") + (Utils::deleteToTrash(path) ? "true" : "false") + "\n"; }
        catch (const java::IOException& e) { out += std::string("IOException: ") + e.what() + "\n"; }
        tree(root, root, out);
    }
    for (size_t at; (at = out.find(rootStr)) != std::string::npos;) out.replace(at, rootStr.size(), "BASE");
    fs::current_path("/");
    fs::remove_all(root);

    std::ifstream ef(expectedPath);
    std::stringstream ss;
    ss << ef.rdbuf();
    std::string want = ss.str();
    if (out.empty() || want.empty()) { std::printf("golden10: nothing ran or the expected file is empty (out %zu bytes, want %zu bytes)\n", out.size(), want.size()); return 2; }
    if (want == out) { std::printf("golden10: all scenarios match (%zu bytes)\n", out.size()); return 0; }
    // show the first differing line
    std::istringstream a(want), b(out);
    std::string la, lb;
    int n = 0;
    while (true) {
        bool ga = static_cast<bool>(std::getline(a, la)), gb = static_cast<bool>(std::getline(b, lb));
        n++;
        if (!ga && !gb) break;
        if (!ga || !gb || la != lb) { std::printf("golden10: MISMATCH at line %d\n  want: %s\n  got:  %s\n", n, ga ? la.c_str() : "<eof>", gb ? lb.c_str() : "<eof>"); break; }
    }
    return 1;
}
