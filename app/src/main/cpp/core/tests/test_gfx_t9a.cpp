// T9a tests: pure graphics code (draw modes, transforms, scissor, packer, text layout, Graphics, Font, Image, RenderTarget)
// against a recording fake backend. Build: g++ -std=c++17 -ffp-contract=off -Wall -Wextra -Iinclude tests/test_gfx_t9a.cpp src/gfx/*.cpp src/util/JavaCompat.cpp
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "opsu/gfx/Animation.h"
#include "opsu/gfx/Colors.h"
#include "opsu/gfx/DrawMode.h"
#include "opsu/gfx/Font.h"
#include "opsu/gfx/Graphics.h"
#include "opsu/gfx/Image.h"
#include "opsu/gfx/RenderTarget.h"
#include "opsu/gfx/ShelfPacker.h"
#include "opsu/gfx/TextLayout.h"
#include "opsu/gfx/Transform2D.h"

using namespace opsu;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
static bool near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) <= e; }

// ---- fakes ----------------------------------------------------------------------------------------------------
struct FakeTex : ITexture {
    int w, h; bool bottom; int updates = 0; std::vector<uint8_t> px;
    std::function<void(ITexture&)> restore;
    FakeTex(int w_, int h_, bool b = false) : w(w_), h(h_), bottom(b), px(static_cast<size_t>(w_) * h_ * 4, 0) {}
    int width() const override { return w; }
    int height() const override { return h; }
    void setFilter(TextureFilter, TextureFilter) override {}
    void update(int x, int y, int ww, int hh, const uint8_t* rgba) override {
        updates++;
        for (int r = 0; r < hh; r++) for (int c = 0; c < ww * 4; c++) px[(static_cast<size_t>(y + r) * w + x) * 4 + c] = rgba[static_cast<size_t>(r) * ww * 4 + c];
    }
    bool rowZeroIsBottom() const override { return bottom; }
};
struct FakeRT : IRenderTarget {
    std::shared_ptr<FakeTex> t; int w, h;
    FakeRT(int w_, int h_) : t(std::make_shared<FakeTex>(w_, h_, true)), w(w_), h(h_) {}
    int width() const override { return w; }
    int height() const override { return h; }
    std::shared_ptr<ITexture> texture() override { return t; }
};
struct Draw { ITexture* tex; std::vector<Vertex> v; };
struct FakeBackend : IRenderBackend {
    std::vector<Draw> draws; std::vector<std::string> log; DrawModeState mode; bool scissorOn = false; IRect scissor;
    IRenderTarget* target = nullptr; ViewportMap vm; int flushes = 0; int clears = 0;
    void drawTriangles(ITexture* t, const Vertex* v, int n) override { draws.push_back({t, std::vector<Vertex>(v, v + n)}); log.push_back("draw"); }
    void setDrawMode(const DrawModeState& m) override { mode = m; log.push_back("mode"); }
    void setScissor(bool on, const IRect& r) override { scissorOn = on; scissor = r; log.push_back(on ? "scissor+" : "scissor-"); }
    void setTarget(IRenderTarget* t, const ViewportMap& m) override { target = t; vm = m; log.push_back("target"); }
    void clear(const Color&) override { clears++; log.push_back("clear"); }
    void flush() override { flushes++; }
};
struct FakeFace : IFontFace {
    std::string set; int asc, desc; float adv; int rasterized = 0;
    FakeFace(std::string s, int a, int d, float ad) : set(std::move(s)), asc(a), desc(d), adv(ad) {}
    bool hasGlyph(char32_t cp) const override { return cp < 128 ? set.find(static_cast<char>(cp)) != std::string::npos : set.find('*') != std::string::npos; }
    int ascent() const override { return asc; }
    int descent() const override { return desc; }
    float advance(char32_t cp) override { return cp == ' ' ? adv / 2 : adv; }
    bool rasterize(char32_t cp, GlyphBitmap& o) override {
        rasterized++;
        if (cp == ' ') return true;
        o.width = 8; o.height = 10; o.left = 1; o.top = 9; o.coverage.assign(80, 200); return true;
    }
};
struct FakeDecoder : IImageDecoder {
    std::map<std::string, Pixmap> files; int decodes = 0;
    bool decode(const std::string& p, Pixmap& o) override { auto it = files.find(p); if (it == files.end()) return false; decodes++; o = it->second; return true; }
};
struct FakeDevice : IRenderDevice {
    FakeBackend be; FakeDecoder dec; std::vector<std::shared_ptr<FakeTex>> texs; std::map<std::string, std::shared_ptr<FakeFace>> faces;
    int sw = 1280, sh = 720;
    IRenderBackend& backend() override { return be; }
    std::shared_ptr<ITexture> createTexture(int w, int h, const uint8_t* rgba, TextureFilter, TextureRestorer r) override {
        auto t = std::make_shared<FakeTex>(w, h);
        if (rgba) t->px.assign(rgba, rgba + static_cast<size_t>(w) * h * 4);
        t->restore = r; texs.push_back(t); return t;
    }
    std::shared_ptr<IRenderTarget> createRenderTarget(int w, int h) override { return std::make_shared<FakeRT>(w, h); }
    IImageDecoder& imageDecoder() override { return dec; }
    std::shared_ptr<IFontFace> loadFontFace(const std::string& n, float) override { auto it = faces.find(n); return it == faces.end() ? nullptr : it->second; }
    void screenSize(int& w, int& h) const override { w = sw; h = sh; }
};

static Pixmap solid(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    Pixmap p(w, h);
    for (size_t i = 0; i < p.rgba.size(); i += 4) { p.rgba[i] = r; p.rgba[i + 1] = g; p.rgba[i + 2] = b; p.rgba[i + 3] = a; }
    return p;
}

static void testPure() {
    // draw modes
    auto n = drawModeState(drawmode::NORMAL), am = drawModeState(drawmode::ALPHA_MAP), ab = drawModeState(drawmode::ALPHA_BLEND), ad = drawModeState(drawmode::ADD);
    CHECK(n.maskR && n.maskG && n.maskB && n.maskA && n.src == BlendFactor::SrcAlpha && n.dst == BlendFactor::OneMinusSrcAlpha);
    CHECK(!am.maskR && !am.maskG && am.maskB && am.maskA && am.src == BlendFactor::One && am.dst == BlendFactor::Zero);
    CHECK(ab.maskR && ab.maskG && !ab.maskB && !ab.maskA && ab.src == BlendFactor::DstAlpha && ab.dst == BlendFactor::OneMinusDstAlpha);
    CHECK(ad.maskA && ad.src == BlendFactor::SrcAlpha && ad.dst == BlendFactor::One);
    bool threw = false; try { drawModeState(9); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw && !isValidDrawMode(0) && isValidDrawMode(4));

    // transforms: positive angle is clockwise on a y-down screen: (1,0) -> (0,1)
    Transform2D t; t.rotate(90.f); float x, y; t.apply(1, 0, x, y); CHECK(near(x, 0) && near(y, 1));
    Transform2D r; r.rotateAbout(10, 10, 90.f); r.apply(20, 10, x, y); CHECK(near(x, 10) && near(y, 20));
    Transform2D o; o.translate(5, 0); o.scale(2, 2); o.apply(1, 1, x, y); CHECK(near(x, 7) && near(y, 2));  // post-multiplied
    Transform2D inv; CHECK(o.invert(inv)); inv.apply(7, 2, x, y); CHECK(near(x, 1) && near(y, 1));
    CHECK(!Transform2D().isScaled() && o.isScaled());
    TransformStack st; st.push(o); Transform2D back = st.pop(); CHECK(back.a == 2.f);
    threw = false; try { st.pop(); } catch (const std::out_of_range&) { threw = true; } CHECK(threw);

    // scissor: 1:1 viewport 800x600, clip (10,20,100,50) -> GL y from bottom: 600-70 = 530
    ViewportMap vm(800, 600, 800, 600); IRect rc;
    CHECK(computeScissor(Transform2D(), vm, 10, 20, 100, 50, rc) && rc == IRect({10, 530, 100, 50}));
    Transform2D tr; tr.translate(5, 5); tr.scale(2, 2);
    CHECK(computeScissor(tr, vm, 10, 20, 100, 50, rc) && rc == IRect({25, 600 - 145, 200, 100}));
    CHECK(computeScissor(Transform2D(), vm, 0, 0, 0, 0, rc) && rc.w == 1 && rc.h == 1);  // w,h raised to 1 -> exactly 1 px: pushed
    Transform2D tiny; tiny.scale(0.5f, 0.5f);
    CHECK(!computeScissor(tiny, vm, 0, 0, 1, 1, rc));  // under one pixel: no clip (libGDX quirk)
    ViewportMap fit(100, 100, 200, 100, ViewportMap::Mode::Fit);
    CHECK(fit.viewport() == IRect({50, 0, 100, 100}));

    // packer
    ShelfPacker pk(100, 50, 1); ShelfPacker::Slot s;
    CHECK(pk.pack(40, 20, s) && s.page == 0 && s.x == 0 && s.y == 0 && s.newPage);
    CHECK(pk.pack(40, 30, s) && s.x == 41 && s.y == 0 && !s.newPage);
    CHECK(pk.pack(40, 10, s) && s.x == 0 && s.y == 31);                 // next row after the tallest (30) + pad
    CHECK(pk.pack(40, 20, s) && s.page == 1 && s.newPage && s.y == 0);  // row would overflow the page -> new page (Java bug fixed)
    CHECK(!pk.pack(101, 1, s) && !pk.pack(0, 5, s));

    // utf-8 + layout
    auto cp = decodeUtf8("a\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80");
    CHECK(cp.size() == 4 && cp[1] == 0xE9 && cp[2] == 0x4E2D && cp[3] == 0x1F600);
    CHECK(decodeUtf8("\xC0\x80").size() == 2 && decodeUtf8("\xED\xA0\x80")[0] == 0xFFFD);
    struct Adv : IAdvanceSource { float advance(char32_t c) override { return c == 'i' ? 2.5f : 10.f; } } adv;
    CHECK(measureTextWidth(decodeUtf8("aii"), adv) == 15);
    std::vector<PlacedGlyph> pg; layoutText(decodeUtf8("ab\ncd"), 10.9f, 5.2f, 12, 16, adv, pg);
    CHECK(pg.size() == 4 && pg[0].x == 10 && pg[0].y == 17 && pg[1].x == 20 && pg[2].x == 10 && pg[2].y == 33);

    // texture region uvs
    auto tex = std::make_shared<FakeTex>(100, 50);
    TextureRegion reg; reg.tex = tex; reg.x = 10; reg.y = 5; reg.w = 20; reg.h = 10;
    float u0, v0, u1, v1; reg.uvs(u0, v0, u1, v1);
    CHECK(near(u0, .1f) && near(v0, .1f) && near(u1, .3f) && near(v1, .3f));
    reg.flipY = true; reg.uvs(u0, v0, u1, v1); CHECK(near(v0, .3f) && near(v1, .1f));
    auto fb = std::make_shared<FakeTex>(100, 50, true); TextureRegion r2; r2.tex = fb; r2.w = 100; r2.h = 50;
    r2.uvs(u0, v0, u1, v1); CHECK(near(v0, 1.f) && near(v1, 0.f));
}

static void testGraphics(FakeDevice& dev) {
    FakeBackend& be = dev.be;
    static std::shared_ptr<Graphics> keep = Graphics::createScreen(be);
    auto g = keep;
    g->resize(800, 600);
    auto tex = std::make_shared<FakeTex>(64, 64);
    TextureRegion reg; reg.tex = tex; reg.w = 64; reg.h = 64;

    // sprite: 6 vertices, corners, uv, colour
    g->setColorAlpha(Color(1.f, 0.5f, 0.f, 1.f), 0.5f);
    g->drawTexture(reg, 100, 50, 20, 10, 0.f);
    g->flush();
    CHECK(be.draws.size() == 1 && be.draws[0].v.size() == 6 && be.draws[0].tex == tex.get());
    const auto& v = be.draws[0].v;
    CHECK(near(v[0].x, 100) && near(v[0].y, 50) && near(v[2].x, 120) && near(v[2].y, 60));
    CHECK(near(v[0].u, 0) && near(v[0].v, 0) && near(v[2].u, 1) && near(v[2].v, 1));
    CHECK(v[0].r == 255 && v[0].g == 127 && v[0].b == 0 && v[0].a == 127);  // (int)(255*c) truncation

    // batching: same texture -> one draw; other texture / solid -> flush in between
    be.draws.clear();
    g->setColor(colors::white);
    auto tex2 = std::make_shared<FakeTex>(8, 8); TextureRegion reg2; reg2.tex = tex2; reg2.w = 8; reg2.h = 8;
    g->drawTexture(reg, 0, 0, 10, 10, 0); g->drawTexture(reg, 20, 0, 10, 10, 0);
    g->drawTexture(reg2, 0, 0, 10, 10, 0); g->fillRect(0, 0, 5, 5); g->flush();
    CHECK(be.draws.size() == 3 && be.draws[0].v.size() == 12 && be.draws[2].tex == nullptr);

    // rotation about the centre and transform
    be.draws.clear();
    g->pushTransform(); g->translate(100, 100); g->rotate(0, 0, 90);
    g->drawTexture(reg, 0, 0, 20, 10, 90.f);  // sprite turns 90 about its centre (10,5), the transform turns 90 more
    g->popTransform(); g->flush();
    const auto& w = be.draws[0].v;  // p1 top-left (0,0) -> (15,-5) after the sprite turn -> then translate + rot90
    float ex, ey; Transform2D tt; tt.translate(100, 100); tt.rotate(90); tt.apply(15, -5, ex, ey);
    CHECK(near(w[0].x, ex) && near(w[0].y, ey));
    CHECK(g->transform().isIdentity());

    // draw modes + additive shapes
    be.log.clear();
    g->setDrawMode(Graphics::MODE_ALPHA_MAP); CHECK(!be.mode.maskR && be.mode.maskB && be.mode.src == BlendFactor::One);
    g->beginAdditiveShapes(); CHECK(be.mode.dst == BlendFactor::One && !be.mode.maskR);
    g->endAdditiveShapes(); CHECK(be.mode.dst == BlendFactor::Zero);
    g->setDrawMode(Graphics::MODE_NORMAL);

    // regression: geometry drawn BEFORE a state change reaches the backend before the change
    be.log.clear(); g->drawTexture(reg, 0, 0, 4, 4, 0); g->setDrawMode(Graphics::MODE_ALPHA_MAP);
    CHECK(be.log.size() >= 2 && be.log[0] == "draw" && be.log[1] == "mode");
    be.log.clear(); g->drawTexture(reg, 0, 0, 4, 4, 0); g->setClip(0, 0, 10, 10); g->clearClip();
    CHECK(be.log.size() >= 2 && be.log[0] == "draw" && be.log[1] == "scissor+");
    g->setDrawMode(Graphics::MODE_NORMAL);

    // clip: set / clear, translate-aware
    g->translate(10, 10); g->setClip(0, 0, 100, 50); CHECK(be.scissorOn && be.scissor == IRect({10, 600 - 60, 100, 50}));
    g->clearClip(); CHECK(!be.scissorOn); g->resetTransform();
    g->setClip(0, 0, 0, 0); CHECK(be.scissorOn);  // w,h raised to 1
    g->clearClip();

    // pixel snap (scale 1.5, translate 0.3): edges land on whole screen pixels
    be.draws.clear();
    g->setPixelSnap(true); g->pushTransform(); g->translate(0.3f, 0.f); g->scale(1.5f, 1.5f);
    g->drawTexture(reg, 3, 3, 10, 10, 0); g->popTransform(); g->flush(); g->setPixelSnap(false);
    const auto& sv = be.draws[0].v;
    CHECK(near(sv[0].x, std::floor(sv[0].x + .5f)) && near(sv[2].x, std::floor(sv[2].x + .5f)) && near(sv[0].y, std::floor(sv[0].y + .5f)));

    // line: width in SCREEN pixels even under a scale transform
    be.draws.clear();
    g->setLineWidth(4.f); g->pushTransform(); g->scale(2.f, 2.f); g->drawLine(10, 10, 60, 10); g->popTransform(); g->flush();
    const auto& lv = be.draws[0].v;
    float minY = 1e9f, maxY = -1e9f; for (const auto& q : lv) { minY = std::min(minY, q.y); maxY = std::max(maxY, q.y); }
    CHECK(near(maxY - minY, 4.f));
    g->resetLineWidth();

    // fillQuad / gradient / arc / oval produce triangles
    be.draws.clear();
    g->fillQuad(0, 0, 10, 0, 10, 10, 0, 10); g->fillGradientRectH(0, 0, 10, 10, colors::red, colors::blue); g->flush();
    CHECK(be.draws[0].v.size() == 12 && be.draws[0].v[6].r == 255 && be.draws[0].v[8].b == 255);
    be.draws.clear(); g->fillArc(0, 0, 40, 40, 0, 90); g->flush(); CHECK(be.draws[0].v.size() % 3 == 0 && !be.draws[0].v.empty());
    be.draws.clear(); g->drawOval(0, 0, 8, 8); g->flush(); CHECK(be.draws[0].v.size() == 6u * 19u);  // (int)(12*cbrt(4)) = 19 segments

    // null region is ignored; bad draw mode throws
    be.draws.clear(); g->drawTexture(TextureRegion(), 0, 0, 1, 1, 0); g->flush(); CHECK(be.draws.empty());
    bool threw = false; try { g->setDrawMode(0); } catch (const std::invalid_argument&) { threw = true; } CHECK(threw);

    // virtual resolution: scissor goes through the map
    g->setVirtualResolution(400, 300, ViewportMap::Mode::Stretch);
    g->setClip(0, 0, 100, 50); CHECK(be.scissor == IRect({0, 600 - 100, 200, 100})); g->clearClip();
    g->setVirtualResolution(800, 600);
}

static void testFontImage(FakeDevice& dev) {
    FakeBackend& be = dev.be;
    dev.faces["main.ttf"] = std::make_shared<FakeFace>("abc ", 12, -4, 8.f);
    dev.faces["fallback.ttf"] = std::make_shared<FakeFace>("abcdefghijklmnopqrstuvwxyz*", 10, -3, 9.f);
    auto font = std::make_shared<Font>("main.ttf", Font::PLAIN, 14.f);
    auto fb = std::make_shared<Font>("fallback.ttf", Font::PLAIN, 14.f);
    font->addBackupFont(fb);
    CHECK(font->getLineHeight() == 16 && font->getHeight("x") == 16);
    font->setPaddingTop(2); font->setPaddingBottom(1); CHECK(font->getLineHeight() == 19); font->setPaddingTop(0); font->setPaddingBottom(0);
    CHECK(font->getWidth("ab") == 16 && font->getWidth("a b") == 20 && font->getWidth("ad") == 17 && font->getWidth("") == 0);  // 'd' from the fallback (9)
    std::vector<GlyphQuad> q; font->layoutGlyphs("ab\nc", 10.7f, 20.f, q);
    CHECK(q.size() == 3 && q[0].x == 11.f && q[0].y == 20.f + 12 - 9 && q[1].x == 19.f && q[2].y == 20.f + 16 + 12 - 9);
    CHECK(font->pageCount() == 1);
    int before = dev.texs.size(); font->layoutGlyphs("ab", 0, 0, q); CHECK(static_cast<int>(dev.texs.size()) == before);  // cached
    // many glyphs -> more pages: 8x10 glyphs, 512/9 = 56 per row, 51 rows -> 2856/page
    auto big = std::make_shared<Font>("fallback.ttf", Font::PLAIN, 14.f);
    std::string many; for (int c = 0x4E00; c < 0x4E00 + 3000; c++) { many += static_cast<char>(0xE0 | (c >> 12)); many += static_cast<char>(0x80 | ((c >> 6) & 0x3F)); many += static_cast<char>(0x80 | (c & 0x3F)); }
    q.clear(); big->layoutGlyphs(many, 0, 0, q); CHECK(q.size() == 3000 && big->pageCount() == 2);
    big->destroy(); CHECK(big->pageCount() == 0); q.clear(); big->layoutGlyphs("a", 0, 0, q); CHECK(big->pageCount() == 1 && q.size() == 1);
    // drawString through Graphics
    auto& g = Graphics::getGraphics(); be.draws.clear(); g.setColor(colors::white);
    font->drawString(5, 5, "ab"); g.flush(); CHECK(be.draws.size() == 1 && be.draws[0].v.size() == 12);
    auto sv = font->getScaledVariant(2.f); CHECK(sv != font && font->getScaledVariant(2.f) == sv && sv->getSize() == 28.f);

    // images
    dev.dec.files["res/red.png"] = solid(4, 4, 255, 0, 0);
    dev.dec.files["res/green.png"] = solid(8, 8, 0, 255, 0);
    dev.dec.files["res/huge.png"] = solid(4000, 100, 9, 9, 9);
    ImageManager::instance().clearAll(); ImageManager::instance().resetTextureCapForTests();
    Image a("res/red.png"), b("res/red.png"), c("res/green.png");
    CHECK(a.getWidth() == 4 && ImageManager::instance().atlasCount() == 1 && ImageManager::instance().entryCount() == 2);
    CHECK(a.getTextureRegion().tex == c.getTextureRegion().tex && a.getTextureRegion().x == b.getTextureRegion().x);
    CHECK(dev.dec.decodes >= 2);
    Image missing("res/nope.png"); CHECK(missing.getWidth() == 0 && !missing.getTextureRegion().tex);
    Image sub = c.getSubImage(2, 2, 4, 4); CHECK(sub.getWidth() == 4 && sub.getHeight() == 4 && sub.getTextureRegion().x == c.getTextureRegion().x + 2);
    Image sc = c.getScaledCopy(2.f); CHECK(sc.getWidth() == 16 && sc.getTextureRegion().w == 8);
    Image huge("res/huge.png"); CHECK(huge.getWidth() == 4000 && huge.getTextureRegion().w == 1600 && huge.getTextureRegion().h == 40);
    CHECK(ImageManager::instance().atlasCount() == 2);
    // restore after context loss: wipe the atlas page and call the restorer
    auto page = std::static_pointer_cast<FakeTex>(a.getTextureRegion().tex);
    std::fill(page->px.begin(), page->px.end(), uint8_t{0}); page->restore(*page);
    CHECK(page->px[(static_cast<size_t>(c.getTextureRegion().y) * 512 + c.getTextureRegion().x) * 4 + 1] == 255);
    // draw
    be.draws.clear(); a.setAlpha(0.5f); a.draw(10, 10); a.drawCentered(50, 50, 2.f, colors::white); g.flush();
    CHECK(be.draws.size() == 1 && be.draws[0].v.size() == 12 && be.draws[0].v[0].a == 127 && near(be.draws[0].v[6].x, 50 - 4.f));
    // clearAll: images draw nothing; destroy is safe
    be.draws.clear(); ImageManager::instance().clearAll(); a.draw(0, 0); g.flush(); CHECK(be.draws.empty()); a.destroy(); CHECK(a.isDestroyed());
    // hue shift: red +120 -> green; grey/transparent untouched
    Pixmap p = solid(1, 1, 255, 0, 0); ImageManager::shiftHue(p, 120.f); CHECK(p.rgba[0] == 0 && p.rgba[1] == 255 && p.rgba[2] == 0 && p.rgba[3] == 255);
    Pixmap gr = solid(1, 1, 90, 90, 90); ImageManager::shiftHue(gr, 120.f); CHECK(gr.rgba[0] == 90);
    Pixmap cp = ImageManager::capPixmap(solid(400, 200, 10, 20, 30), 100); CHECK(cp.w == 100 && cp.h == 50 && cp.rgba[0] == 10 && cp.rgba[3] == 255);

    // render target: bind restores the previous target and clip; texture is upright
    auto rt = RenderTarget::create(128, 64); CHECK(rt && rt->getTexture()->rowZeroIsBottom());
    g.setClip(5, 5, 50, 50); IRect clipBefore = be.scissor;
    rt->bind(); CHECK(be.target != nullptr && be.vm.pixelW == 128 && be.vm.virtualW == 128 && !be.scissorOn);
    rt->unbind(); CHECK(be.target == nullptr && be.scissorOn && be.scissor == clipBefore && be.vm.pixelW == 800);
    g.clearClip();
    Image off(64, 64); auto og = &off.getGraphics(); Graphics::setCurrent(*og); CHECK(be.target != nullptr);
    og->clear(); CHECK(be.clears >= 1); Graphics::setCurrent(g); CHECK(be.target == nullptr);
    bool threw = false; try { c.getGraphics(); } catch (const std::logic_error&) { threw = true; } CHECK(threw);
    be.draws.clear(); rt->draw(0, 0, 10, 10); g.flush(); CHECK(be.draws.size() == 1 && be.draws[0].tex == rt->getTexture().get());
    float u0, v0, u1, v1; rt->getRegion().uvs(u0, v0, u1, v1); CHECK(v0 == 1.f && v1 == 0.f);

    // animation
    static int64_t now = 1000; Animation::setClockForTests([]() -> int64_t { return now; });
    dev.dec.files["res/sheet.png"] = solid(8, 4, 1, 2, 3);
    auto sheet = std::make_shared<Image>("res/sheet.png");
    Animation an(SpriteSheet(sheet, 4, 4), 100);
    CHECK(an.getFrameCount() == 2 && an.getWidth() == 4 && an.getHeight() == 4);
    CHECK(an.getTextureRegion().x == an.getImage(0)->getTextureRegion().x);
    now += 150; an.updateNoDraw(); CHECK(an.getTextureRegion().x == an.getImage(1)->getTextureRegion().x);
    now += 50; an.updateNoDraw(); CHECK(an.getTextureRegion().x == an.getImage(1)->getTextureRegion().x);  // 50 < 100
    threw = false; try { an.getScaledCopy(2.f); } catch (const std::logic_error&) { threw = true; } CHECK(threw);
    Animation::setClockForTests(nullptr);
}

int main() {
    testPure();
    auto dev = std::make_shared<FakeDevice>();
    setRenderDevice(dev);
    testGraphics(*dev);
    testFontImage(*dev);
    setRenderDevice(nullptr);
    if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
    std::printf("test_gfx_t9a: all passed\n");
    return 0;
}
