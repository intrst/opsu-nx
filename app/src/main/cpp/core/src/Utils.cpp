#include "opsu/Utils.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include "opsu/util/JavaIO.h"

namespace opsu {
namespace Utils {

double standardDeviation(const std::vector<int>& list) {
    float avg = 0.f;
    for (int i : list) avg += static_cast<float>(i);
    avg /= static_cast<float>(list.size());
    float var = 0.f;
    for (int i : list) var += (static_cast<float>(i) - avg) * (static_cast<float>(i) - avg);
    var /= static_cast<float>(list.size());
    return std::sqrt(static_cast<double>(var));
}

std::string getTimeString(int seconds) {
    char b[64];
    if (seconds < 60) {
        if (seconds == 1) return "1 second";
        std::snprintf(b, sizeof b, "%d seconds", seconds);
    } else if (seconds < 3600) {
        std::snprintf(b, sizeof b, "%02d:%02d", seconds / 60, seconds % 60);
    } else {
        std::snprintf(b, sizeof b, "%02d:%02d:%02d", seconds / 3600, (seconds / 60) % 60, seconds % 60);
    }
    return b;
}

std::string bytesToString(long long bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    int exp = java::toInt(std::log(static_cast<double>(bytes)) / std::log(1024.0));
    char pre = "KMGTPE"[exp - 1];
    return java::formatFixed(static_cast<double>(bytes) / std::pow(1024.0, exp), 1) + " " + pre + "B";
}

std::string cleanFileName(const std::string& bad, char replace) {
    static const bool* illegal = [] {
        static bool t[128] = {false};
        const int chars[] = {34, 60, 62, 124, 58, 42, 63, 92, 47};
        for (int c : chars) t[c] = true;
        for (int c = 0; c < 32; c++) t[c] = true;
        return t;
    }();
    auto isIllegal = [&](unsigned char c) { return c < 128 && illegal[c]; };
    bool doReplace = (replace > 0 && !isIllegal(static_cast<unsigned char>(replace)));
    std::string out;
    for (unsigned char c : bad) {
        if (!isIllegal(c)) out += static_cast<char>(c);
        else if (doReplace) out += replace;
    }
    return out;
}

void copyFile(const std::string& src, const std::string& dst) {
    std::ifstream in(std::filesystem::u8path(src), std::ios::binary);
    if (!in) throw java::FileNotFoundException(src);
    std::ofstream out(std::filesystem::u8path(dst), std::ios::binary | std::ios::trunc);
    if (!out) throw java::FileNotFoundException(dst);
    char buf[8192];
    while (in.read(buf, sizeof buf) || in.gcount() > 0) {
        out.write(buf, in.gcount());
        if (!out) throw java::IOException("write failed: " + dst);
    }
}

void moveFile(const std::string& src, const std::string& dst) {
    std::error_code ec;
    std::filesystem::rename(std::filesystem::u8path(src), std::filesystem::u8path(dst), ec);
    if (!ec) return;
    copyFile(src, dst);
    std::filesystem::remove(std::filesystem::u8path(src), ec);  // File.delete(): failure is silent
}

namespace {
namespace fs = std::filesystem;

/** `new File(path).getPath()` on Unix: duplicate and trailing '/' removed (so "f.txt/" is the file "f.txt"; libc would answer ENOTDIR). */
std::string normalizeFilePath(const std::string& path) {
    std::string n;
    for (char c : path) {
        if (c == '/' && !n.empty() && n.back() == '/') continue;
        n += c;
    }
    if (n.size() > 1 && n.back() == '/') n.pop_back();
    return n;
}

/** `new File(path).getAbsolutePath()` on Unix: a relative path resolved against the working directory (no `.` / `..` folding). */
std::string absolutePath(const std::string& path) {
    std::string n = normalizeFilePath(path);
    if (!n.empty() && n[0] == '/') return n;
    std::error_code ec;
    std::string cwd = fs::current_path(ec).u8string();
    if (n.empty()) return cwd;
    return (cwd == "/" ? std::string() : cwd) + "/" + n;
}

bool isDir(const fs::path& p) {  // File.isDirectory(): follows symbolic links
    std::error_code ec;
    return fs::is_directory(p, ec);
}
}  // namespace

void deleteDirectory(const std::string& dirPath) {
    const std::string dir = normalizeFilePath(dirPath);
    fs::path d = fs::u8path(dir);
    if (dir.empty() || !isDir(d)) return;
    std::error_code ec;
    // File.listFiles(): null when the directory cannot be read, then only the final delete is tried
    std::vector<fs::path> children;
    for (fs::directory_iterator it(d, ec), end; !ec && it != end; it.increment(ec)) children.push_back(it->path());
    for (const fs::path& child : children) {
        if (isDir(child)) deleteDirectory(child.u8string());
        else fs::remove(child, ec);  // File.delete(): failure is silent
    }
    fs::remove(d, ec);
}

bool deleteToTrash(const std::string& rawPath) {
    const std::string path = normalizeFilePath(rawPath);
    std::error_code ec;
    fs::path p = fs::u8path(path);
    if (path.empty() || !fs::exists(p, ec)) throw java::IOException("File '" + absolutePath(path) + "' does not exist.");
    // TODO in Java: move to the system trash when one exists (commented out there); it deletes otherwise, and so does this
    if (isDir(p)) deleteDirectory(path);
    else fs::remove(p, ec);
    return false;
}

}  // namespace Utils
}  // namespace opsu
