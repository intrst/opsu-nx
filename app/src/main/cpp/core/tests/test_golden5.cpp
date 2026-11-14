// Fifth golden suite: regenerates the output of tests/golden/java/Golden5.java (the REAL DownloadNode.java, stubbed edges)
// with the C++ port and diffs it against tests/golden/expected5.txt. Every rnd() call is sequenced explicitly (Java evaluates
// arguments left to right, C++ does not).
// usage: opsu_golden5 [expected5.txt] [--dump]
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include "opsu/downloads/DownloadNode.h"
#include "opsu/util/JavaCompat.h"
#include "opsu/util/JavaMath.h"

using namespace opsu;
using F = CardFonts::Font;

static std::string out;
static uint64_t seed_ = 20261113ULL;
static int rnd() { seed_ = seed_ * 6364136223846793005ULL + 1442695040888963407ULL; return static_cast<int>(seed_ >> 33); }
static int rnd(int n) { return rnd() % n; }
static int32_t bits(float f) { int32_t u; if (f != f) return 0x7fc00000; std::memcpy(&u, &f, 4); return u; }
static std::string hx(float f) { char b[16]; std::snprintf(b, sizeof b, "%08x", static_cast<uint32_t>(bits(f))); return b; }
static void line(const std::string& s) { out += s; out += '\n'; }
static std::string S(long long v) { return std::to_string(v); }
static std::string u8(const std::string& s) { std::string r; char t[4]; for (unsigned char c : s) { std::snprintf(t, sizeof t, "%02x", c); r += t; } return r; }
static float up(float x) { return std::nextafter(x, std::numeric_limits<float>::infinity()); }
static float down(float x) { return std::nextafter(x, -std::numeric_limits<float>::infinity()); }

// Mirrors the stubs5 UnicodeFont: width = (int) sum of cw * (1 + (c % 7) * 0.1f) over UTF-16 units (BMP only in the tests).
struct FakeFonts : CardFonts {
    int lh[4] = {0, 0, 0, 0};
    float cw[4] = {9.5f, 7.25f, 6.5f, 5.75f};
    mutable std::vector<std::string> log;
    void setup(int large, int medium, int bold, int def) { lh[0] = large; lh[1] = medium; lh[2] = bold; lh[3] = def; }
    int lineHeight(Font f) const override { return lh[static_cast<int>(f)]; }
    int width(Font f, const std::string& s) const override {
        float w = 0.f;
        float c = cw[static_cast<int>(f)];
        for (size_t i = 0; i < s.size();) {
            unsigned char b = static_cast<unsigned char>(s[i]);
            unsigned unit;
            if (b < 0x80) { unit = b; i += 1; }
            else if (b < 0xE0) { unit = ((b & 0x1Fu) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3Fu); i += 2; }
            else { unit = ((b & 0x0Fu) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3Fu) << 6) | (static_cast<unsigned char>(s[i + 2]) & 0x3Fu); i += 3; }
            w += c * (1.f + static_cast<float>(static_cast<int>(unit % 7)) * 0.1f);
        }
        return java::toInt(w);
    }
    void loadGlyphs(Font f, const std::string& s) const override {
        static const char* names[] = {"LARGE", "MEDIUM", "BOLD", "DEFAULT"};
        log.push_back(std::string(names[static_cast<int>(f)]) + ":" + s);
    }
};
static FakeFonts fonts;
using DN = DownloadNode;

// ------------------------------------------------------------------ layout + hit tests
static void layout() {
    const int ws[] = {320, 800, 1080, 1280, 1920, 2400, 3000, 123, 640, 1600};
    const int hs[] = {240, 480, 720, 1080, 1440, 2160, 99, 50, 360, 900};
    for (int round = 0; round < 40; round++) {
        int w = ws[rnd(10)];
        int h = hs[rnd(10)];
        int large = 20 + rnd(60);
        int medium = 14 + rnd(46);
        int bold = 10 + rnd(40);
        int def = 8 + rnd(32);
        fonts.setup(large, medium, bold, def);
        DN::init(w, h, fonts);
        line("LY " + S(round) + " " + S(w) + " " + S(h) + " " + S(large) + " " + S(medium) + " " + S(bold) + " " + S(def) + " " + hx(DN::getListX()) + " " +
             hx(DN::getListWidth()) + " " + hx(DN::getListY()) + " " + hx(DN::getListHeight()) + " " + hx(DN::getBottomReserve()) + " " +
             S(DN::maxResultsShown()) + " " + hx(DN::getButtonOffset()) + " " + hx(DN::getCardWidth()) + " " + hx(DN::getCardGap()) + " " +
             hx(DN::margin()) + " " + hx(DN::thumbW()) + " " + hx(DN::btnH()));
        for (int col = 0; col < 3; col++) {
            line("LC " + S(round) + " " + S(col) + " " + hx(DN::cardX(col)) + " " + hx(DN::textLeft(col)) + " " + hx(DN::textRight(col)));
            for (int zone = 0; zone < 4; zone++) {
                auto r = DN::getCardRect(zone, col);
                line("LR " + S(round) + " " + S(col) + " " + S(zone) + " " + hx(r[0]) + " " + hx(r[1]) + " " + hx(r[2]) + " " + hx(r[3]));
            }
        }
        for (int row = 0; row < 5; row++) line("LB " + S(round) + " " + S(row) + " " + hx(DN::btnY(row)));
        ClipRect c = DN::resultAreaClip();
        line("LK " + S(round) + " clip " + S(c.x) + " " + S(c.y) + " " + S(c.w) + " " + S(c.h));
        float lx = DN::getListX(), lw = DN::getListWidth(), ly = DN::getListY(), lh = DN::getListHeight();
        for (int i = 0; i < 60; i++) {
            float cx = lx + static_cast<float>(rnd(1100) - 50) / 1000.f * lw;
            float cy = ly + static_cast<float>(rnd(1100) - 50) / 1000.f * lh;
            int row = rnd(4);
            int col = rnd(3);
            line("HT " + S(round) + " " + S(DN::resultContains(cx, cy, row, col) ? 1 : 0) + S(DN::resultThumbContains(cx, cy, row, col) ? 1 : 0) +
                 S(DN::resultDownloadContains(cx, cy, row, col) ? 1 : 0) + S(DN::resultAreaContains(cx, cy) ? 1 : 0));
        }
        for (int col = 0; col < 3; col++)
            for (int row = 0; row < 3; row++) {
                float cx0 = DN::cardX(col), cw = DN::getCardWidth(), by = ly + row * DN::getButtonOffset();
                float tl = DN::textLeft(col), tr = DN::textRight(col), bY = DN::btnY(row), bH = DN::btnH();
                float xs[] = {cx0, cx0 + cw, tl, tr, cx0 + DN::thumbW(), up(cx0), down(cx0 + cw), up(tl), down(tr), (tl + tr) / 2};
                float ys[] = {by, by + DN::getCardHeight(), bY, bY + bH, up(by), down(bY + bH), bY + bH / 2};
                std::string b = "HE " + S(round) + " " + S(row) + " " + S(col) + ":";
                for (float x : xs)
                    for (float y : ys)
                        b += " " + S(DN::resultContains(x, y, row, col) ? 1 : 0) + S(DN::resultThumbContains(x, y, row, col) ? 1 : 0) +
                             S(DN::resultDownloadContains(x, y, row, col) ? 1 : 0);
                line(b);
            }
        for (float x : {lx, lx + lw, up(lx), down(lx + lw), lx + lw / 2})
            for (float y : {ly, ly + lh, up(ly), down(ly + lh), ly + lh / 2}) line("HA " + S(round) + " " + S(DN::resultAreaContains(x, y) ? 1 : 0));
    }
}

// ------------------------------------------------------------------ fit / icon
static void fit() {
    fonts.setup(40, 30, 24, 20);
    const std::string strs[] = {"", "a", "ab", "Hello World", "Mapped by someone with a really long name",
        "The Quick Brown Fox Jumps Over The Lazy Dog 0123456789",
        "日本語のタイトル とても長いタイトル", "café ééé", "......",
        "WWWWWWWWWWWWWWWWWWWW"};
    const F fs[] = {F::DEFAULT, F::BOLD, F::LARGE};
    for (int fi = 0; fi < 3; fi++)
        for (const std::string& s : strs) {
            for (float w : {-5.f, 0.f, 1.f, 10.f, 20.f, 33.5f, 60.f, 100.f, 150.f, 250.f, 400.f, 1000.f, 100000.f})
                line("FT " + S(fi) + " " + u8(s) + " " + hx(w) + " " + u8(DN::fit(fonts, fs[fi], s, w)));
            for (int i = 0; i < 12; i++) {
                float w = static_cast<float>(rnd(30000)) / 100.f;
                line("FT " + S(fi) + " " + u8(s) + " " + hx(w) + " " + u8(DN::fit(fonts, fs[fi], s, w)));
            }
        }
}

static void icon() {
    for (float bh : {-10.f, -1.f, 0.f, 0.5f, 1.f, 5.f, 11.5f, 12.f, 24.f, 37.7f, 64.f, 100.f, 250.25f})
        for (int i = 0; i < 4; i++) {
            float cx = static_cast<float>(rnd(4000) - 500) / 7.f;
            float cy = static_cast<float>(rnd(4000) - 500) / 7.f;
            auto rects = DN::downloadIconRects(cx, cy, bh);
            std::string b = "IC " + hx(cx) + " " + hx(cy) + " " + hx(bh) + " " + S(static_cast<long long>(rects.size())) + ":";
            for (const CardRect& r : rects) b += " [" + S(bits(r.x)) + " " + S(bits(r.y)) + " " + S(bits(r.w)) + " " + S(bits(r.h)) + "]";
            line(b);
        }
}

// ------------------------------------------------------------------ animate
static void animate() {
    const Download::Status sts[] = {Download::Status::WAITING, Download::Status::DOWNLOADING, Download::Status::COMPLETE, Download::Status::CANCELLED,
                                    Download::Status::ERROR};
    const char* stNames[] = {"WAITING", "DOWNLOADING", "COMPLETE", "CANCELLED", "ERROR"};
    for (int round = 0; round < 60; round++) {
        DownloadNode n(1000 + round, "d", "t", "", "a", "", "c");
        bool inLib = false;
        std::optional<DownloadSnapshot> dl;
        std::string b = "AN " + S(round) + ":";
        for (int step = 0; step < 50; step++) {
            switch (rnd(7)) {
                case 0: inLib = true; break;
                case 1: inLib = false; break;
                case 2: dl.reset(); break;
                case 3: case 4: {
                    int si = rnd(5);
                    float p = (rnd(5) == 0) ? -(static_cast<float>(rnd(500)) / 10.f) : static_cast<float>(rnd(10001)) / 100.f;
                    dl = DownloadSnapshot{sts[si], p};
                    break;
                }
                default: break;
            }
            float k = (rnd(4) == 0) ? 1.f : DN::smoothing(rnd(200));
            float gt = static_cast<float>(rnd(3)) * 0.5f;
            float pt = static_cast<float>(rnd(2));
            n.animate(k, gt, pt, inLib, dl);
            std::string st = "null";
            if (n.getDownloadStatus()) st = stNames[static_cast<int>(*n.getDownloadStatus())];
            b += " " + hx(n.getGlow()) + "/" + hx(n.getPreviewMix()) + "/" + hx(n.getShownProgress()) + "/" + (n.isLoadedInLibrary() ? 'L' : '-') +
                 (n.isDownloadActive() ? 'A' : '-') + "/" + st + "/" + n.getProgressLabel();
        }
        line(b);
    }
    for (float v : {0.f, 1.f, 0.001f, -0.001f, 0.0019999f, 0.002f, 0.5f})
        for (float tg : {0.f, 0.001f, 0.003f, 1.f, 100.f}) line("EA " + hx(v) + " " + hx(tg) + " " + hx(DN::ease(v, tg, 0.3f)));
    for (int d = -50; d <= 400; d += 7) line("SM " + S(d) + " " + hx(DN::smoothing(d)));
}

// ------------------------------------------------------------------ updateFit
static void updateFit() {
    const std::vector<std::vector<std::string>> meta = {
        {"Short Title", "", "Artist", "", "mapper"},
        {"A Very Long Song Title That Needs To Be Shortened Somewhere", "", "An Artist With A Long Name Too", "", "someone with a very long username"},
        {"Romaji Title", "日本語のタイトルとても長いタイトル", "Romaji Artist", "アーティスト", "m"},
        {"", "", "", "", ""},
        {"x", "", "café éé", "", "é"}};
    const std::vector<std::optional<std::vector<double>>> stars = {std::nullopt, std::vector<double>{}, std::vector<double>{1.5},
                                                                   std::vector<double>{1.5, 2.25, 6.789}, std::vector<double>{0.005, 10.0}};
    fonts.setup(40, 30, 24, 20);
    bool unicode = false;
    for (int round = 0; round < 30; round++) {
        const auto& m = meta[rnd(5)];
        DownloadNode n(round, "d", m[0], m[1], m[2], m[3], m[4]);
        std::string b = "UF " + S(round) + ":";
        for (int step = 0; step < 14; step++) {
            switch (rnd(5)) {
                case 0: unicode = !unicode; break;
                case 1: n.setStars(stars[rnd(5)]); break;
                default: break;
            }
            fonts.log.clear();
            float w = (rnd(6) == 0) ? 150.f : static_cast<float>(rnd(900) + 20) / 2.f;
            n.updateFit(fonts, unicode, w);
            n.updateFit(fonts, unicode, w);
            b += " w=" + hx(w) + " " + (unicode ? 'U' : 'n') + " " + u8(n.getFittedTitle()) + "|" + u8(n.getFittedArtist()) + "|" + u8(n.getFittedMapper()) + "|" +
                 hx(n.getChipWidth()) + "|" + S(static_cast<long long>(fonts.log.size()));
            for (const std::string& g : fonts.log) b += "|" + u8(g);
        }
        line(b);
        unicode = false;
    }
}

int main(int argc, char** argv) {
    std::string path = "tests/golden/expected5.txt";
    bool dump = false;
    for (int i = 1; i < argc; i++) { if (!std::strcmp(argv[i], "--dump")) dump = true; else path = argv[i]; }
    layout(); fit(); icon(); animate(); updateFit();
    if (dump) { std::cout << out; return 0; }

    std::ifstream f(path);
    if (!f) { std::printf("cannot open %s\n", path.c_str()); return 2; }
    std::stringstream expSs; expSs << f.rdbuf();
    std::istringstream a(expSs.str()), b(out);
    std::string la, lb;
    int n = 0, bad = 0;
    while (true) {
        bool ga = static_cast<bool>(std::getline(a, la)), gb = static_cast<bool>(std::getline(b, lb));
        if (!ga && !gb) break;
        n++;
        if (!ga || !gb || la != lb) {
            if (bad++ < 12) {
                auto cut = [](const std::string& s) { return s.size() > 300 ? s.substr(0, 300) + "..." : s; };
                std::printf("MISMATCH line %d\n  java: %s\n  c++ : %s\n", n, cut(ga ? la : "<eof>").c_str(), cut(gb ? lb : "<eof>").c_str());
            }
        }
    }
    std::printf(bad ? "golden5: %d of %d lines differ\n" : "golden5: all %d lines match\n", bad ? bad : n, n);
    return bad ? 1 : 0;
}
