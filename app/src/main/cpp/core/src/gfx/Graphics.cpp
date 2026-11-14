#include "opsu/gfx/Graphics.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "opsu/gfx/Font.h"
#include "opsu/gfx/Image.h"
#include "opsu/util/JavaCompat.h"

namespace opsu {

namespace {
constexpr float kPi = 3.14159265358979323846f;

uint8_t toByte(float c) {
    // libGDX Color.toFloatBits: (int)(255 * c), truncated
    const int v = java::toInt(255.0 * static_cast<double>(c));
    return static_cast<uint8_t>(std::max(0, std::min(255, v)));
}
float roundJava(float v) { return std::floor(v + 0.5f); }  // Math.round
}  // namespace

struct Graphics::Shared {
    IRenderBackend* be = nullptr;
    Color bg{0.f, 0.f, 0.f, 1.f};
    Color fg{1.f, 1.f, 1.f, 1.f};
    uint8_t rgba[4] = {255, 255, 255, 255};
    Transform2D transform;
    float lineWidth = 1.f;
    int mode = drawmode::NORMAL;
    bool additiveShapes = false;
    bool pixelSnap = false;
    std::shared_ptr<Font> font;

    ViewportMap screenMap;
    ViewportMap curMap;
    bool hasScissor = false;
    IRect scissor;

    std::vector<Vertex> verts;
    ITexture* verTex = nullptr;  // texture of the pending vertices

    Graphics* screen = nullptr;
    Graphics* cur = nullptr;

    struct Binding {
        IRenderTarget* target;
        ViewportMap map;
        bool hasScissor;
        IRect scissor;
    };
    std::vector<Binding> bindings;
    IRenderTarget* curTarget = nullptr;
};

namespace {
Graphics* g_screen = nullptr;
}

Graphics::Graphics(std::shared_ptr<Shared> s, std::shared_ptr<IRenderTarget> target, float vw, float vh)
    : s_(std::move(s)), target_(std::move(target)), virtualW_(vw), virtualH_(vh) {}

Graphics::~Graphics() {
    if (s_ && s_->cur == this) s_->cur = s_->screen == this ? nullptr : s_->screen;
    if (g_screen == this) g_screen = nullptr;
    if (s_ && s_->screen == this) s_->screen = nullptr;
}

std::shared_ptr<Graphics> Graphics::createScreen(IRenderBackend& backend) {
    auto sh = std::make_shared<Shared>();
    sh->be = &backend;
    std::shared_ptr<Graphics> g(new Graphics(sh, nullptr, 0.f, 0.f));
    sh->screen = g.get();
    sh->cur = g.get();
    g_screen = g.get();
    backend.setDrawMode(drawModeState(drawmode::NORMAL));
    return g;
}

Graphics& Graphics::getGraphics() {
    if (!g_screen) throw std::logic_error("Graphics: createScreen() was not called");
    return *g_screen;
}
bool Graphics::hasGraphics() { return g_screen != nullptr; }
Graphics& Graphics::current() {
    Graphics& g = getGraphics();
    return g.s_->cur ? *g.s_->cur : g;
}

std::shared_ptr<Graphics> Graphics::createForTarget(const std::shared_ptr<IRenderTarget>& target, float virtualW,
                                                    float virtualH) {
    Graphics& scr = getGraphics();
    return std::shared_ptr<Graphics>(new Graphics(scr.s_, target, virtualW, virtualH));
}

// ---- surface ----------------------------------------------------------------------------------------------------

void Graphics::resize(int pixelW, int pixelH) {
    flush();
    ViewportMap& m = s_->screenMap;
    const bool follow = m.virtualW == static_cast<float>(m.pixelW) && m.virtualH == static_cast<float>(m.pixelH);
    if (follow || m.pixelW <= 1) {
        m.virtualW = static_cast<float>(pixelW);
        m.virtualH = static_cast<float>(pixelH);
    }
    m.pixelW = pixelW;
    m.pixelH = pixelH;
    if (!s_->curTarget) {
        s_->curMap = m;
        s_->be->setTarget(nullptr, s_->curMap);
        // the scissor is in pixels of the old size
        s_->hasScissor = false;
        s_->be->setScissor(false, IRect());
    }
}

void Graphics::setVirtualResolution(float w, float h, ViewportMap::Mode mode) {
    flush();
    ViewportMap& m = s_->screenMap;
    m.virtualW = w;
    m.virtualH = h;
    m.mode = mode;
    if (!s_->curTarget) {
        s_->curMap = m;
        s_->be->setTarget(nullptr, s_->curMap);
    }
}

const ViewportMap& Graphics::viewportMap() const { return s_->curMap; }

// ---- state ------------------------------------------------------------------------------------------------------

void Graphics::setBackground(const Color& c) { s_->bg = c; }
const Color& Graphics::getBackground() const { return s_->bg; }

void Graphics::setColor(const Color& c) { setColorAlpha(c, 1.f); }

void Graphics::setColorAlpha(const Color& c, float alpha) {
    s_->fg = c;
    s_->rgba[0] = toByte(c.r);
    s_->rgba[1] = toByte(c.g);
    s_->rgba[2] = toByte(c.b);
    s_->rgba[3] = toByte(c.a * alpha);
}

const Color& Graphics::getColor() const { return s_->fg; }

void Graphics::setLineWidth(float w) { s_->lineWidth = w; }
float Graphics::getLineWidth() const { return s_->lineWidth; }

void Graphics::applyDrawMode() {
    DrawModeState st = drawModeState(s_->mode);
    if (s_->additiveShapes) {  // beginAdditiveShapes: only the blend changes, the mask stays
        st.src = BlendFactor::SrcAlpha;
        st.dst = BlendFactor::One;
    }
    s_->be->setDrawMode(st);
}

void Graphics::setDrawMode(int mode) {
    if (!isValidDrawMode(mode)) throw std::invalid_argument("Unknown Draw Mode");
    sendPending();
    s_->mode = mode;
    s_->additiveShapes = false;
    applyDrawMode();
}
int Graphics::getDrawMode() const { return s_->mode; }

void Graphics::setPixelSnap(bool on) { s_->pixelSnap = on; }
bool Graphics::isPixelSnap() const { return s_->pixelSnap; }

// ---- transform --------------------------------------------------------------------------------------------------

void Graphics::pushTransform() { stack_.push(s_->transform); }
void Graphics::popTransform() { s_->transform = stack_.pop(); }
void Graphics::rotate(float x, float y, float a) { s_->transform.rotateAbout(x, y, a); }
void Graphics::translate(float x, float y) { s_->transform.translate(x, y); }
void Graphics::scale(float sx, float sy) { s_->transform.scale(sx, sy); }
void Graphics::resetTransform() { s_->transform.idt(); }
const Transform2D& Graphics::transform() const { return s_->transform; }

// ---- clip -------------------------------------------------------------------------------------------------------

void Graphics::setClip(int x, int y, int w, int h) {
    sendPending();
    IRect r;
    if (computeScissor(s_->transform, s_->curMap, x, y, w, h, r)) {
        s_->hasScissor = true;
        s_->scissor = r;
        s_->be->setScissor(true, r);
    } else {
        // libGDX: a box under 1 pixel is not pushed, so the clip is simply off
        clearClip();
    }
}

void Graphics::clearClip() {
    if (s_->hasScissor) {
        sendPending();
        s_->hasScissor = false;
        s_->be->setScissor(false, IRect());
    }
}

// ---- batching helpers -------------------------------------------------------------------------------------------

void Graphics::submit(ITexture* tex) {
    if (!s_->verts.empty() && s_->verTex != tex) flush();
    s_->verTex = tex;
}

void Graphics::sendPending() {
    if (!s_->verts.empty()) {
        s_->be->drawTriangles(s_->verTex, s_->verts.data(), static_cast<int>(s_->verts.size()));
        s_->verts.clear();
    }
}

void Graphics::flush() {
    sendPending();
    s_->be->flush();
}

namespace {
void pushV(std::vector<Vertex>& v, float x, float y, float u, float vv, const uint8_t c[4]) {
    v.push_back(Vertex{x, y, u, vv, c[0], c[1], c[2], c[3]});
}
}  // namespace

// px: four corners p1..p4 in game space (before the transform), order top-left, bottom-left, bottom-right, top-right.
void Graphics::emitQuad(ITexture* tex, const float px[8], float u0, float v0, float u1, float v1, const uint8_t c[4]) {
    submit(tex);
    float x[4], y[4];
    for (int i = 0; i < 4; i++) s_->transform.apply(px[i * 2], px[i * 2 + 1], x[i], y[i]);
    const float uu[4] = {u0, u0, u1, u1};
    const float vv[4] = {v0, v1, v1, v0};
    static const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int k : order) pushV(s_->verts, x[k], y[k], uu[k], vv[k], c);
}

void Graphics::emitSolidQuad(const float px[8], const uint8_t c[4]) {
    submit(nullptr);
    float x[4], y[4];
    for (int i = 0; i < 4; i++) s_->transform.apply(px[i * 2], px[i * 2 + 1], x[i], y[i]);
    static const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int k : order) pushV(s_->verts, x[k], y[k], 0.f, 0.f, c);
}

void Graphics::emitSolidQuad4(const float px[8], const uint8_t c[16]) {
    submit(nullptr);
    float x[4], y[4];
    for (int i = 0; i < 4; i++) s_->transform.apply(px[i * 2], px[i * 2 + 1], x[i], y[i]);
    static const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int k : order) pushV(s_->verts, x[k], y[k], 0.f, 0.f, &c[k * 4]);
}

void Graphics::emitTri(const float p[6], const uint8_t c[4]) {
    submit(nullptr);
    for (int i = 0; i < 3; i++) {
        float x, y;
        s_->transform.apply(p[i * 2], p[i * 2 + 1], x, y);
        pushV(s_->verts, x, y, 0.f, 0.f, c);
    }
}

// ---- textures ---------------------------------------------------------------------------------------------------

void Graphics::drawTextureEx(const TextureRegion& r, float x, float y, float ox, float oy, float w, float h, float sx,
                             float sy, float rot) {
    if (!r.tex) return;
    // SpriteBatch.draw(region, x, y, originX, originY, width, height, scaleX, scaleY, rotation)
    const float wx = x + ox, wy = y + oy;
    float fx = -ox, fy = -oy, fx2 = w - ox, fy2 = h - oy;
    if (sx != 1.f || sy != 1.f) { fx *= sx; fy *= sy; fx2 *= sx; fy2 *= sy; }
    float qx[4] = {fx, fx, fx2, fx2}, qy[4] = {fy, fy2, fy2, fy};
    if (rot != 0.f) {
        Transform2D rt;
        rt.rotate(rot);
        for (int i = 0; i < 4; i++) rt.apply(qx[i], qy[i], qx[i], qy[i]);
    }
    float p[8];
    for (int i = 0; i < 4; i++) { p[i * 2] = qx[i] + wx; p[i * 2 + 1] = qy[i] + wy; }
    float u0, v0, u1, v1;
    r.uvs(u0, v0, u1, v1);
    emitQuad(r.tex.get(), p, u0, v0, u1, v1, s_->rgba);
}

void Graphics::drawTexture(const TextureRegion& r, float x, float y, float w, float h, float rot) {
    if (!r.tex) return;
    if (s_->pixelSnap && rot == 0.f && s_->transform.isScaled()) {
        // move the edges onto whole screen pixels (see the comment in the Java)
        const Transform2D& t = s_->transform;
        const float sx = t.a, sy = t.d, tx = t.tx, ty = t.ty;
        const float x1 = roundJava(x * sx + tx), x2 = roundJava((x + w) * sx + tx);
        const float y1 = roundJava(y * sy + ty), y2 = roundJava((y + h) * sy + ty);
        if (x2 > x1 && y2 > y1) {
            x = (x1 - tx) / sx;
            w = (x2 - x1) / sx;
            y = (y1 - ty) / sy;
            h = (y2 - y1) / sy;
        }
    }
    drawTextureEx(r, x, y, w / 2.f, h / 2.f, w, h, 1.f, 1.f, rot);
}

void Graphics::drawTexture(const std::shared_ptr<ITexture>& tex, float x, float y, float w, float h) {
    if (!tex) return;
    TextureRegion r;
    r.tex = tex;
    r.w = tex->width();
    r.h = tex->height();
    drawTextureEx(r, x, y, w / 2.f, h / 2.f, w, h, 1.f, 1.f, 0.f);
}

void Graphics::drawImage(Image& image, float x, float y) { image.draw(x, y); }

// ---- text -------------------------------------------------------------------------------------------------------

void Graphics::setFont(std::shared_ptr<Font> font) { s_->font = std::move(font); }

void Graphics::drawString(const std::string& str, float x, float y) {
    if (s_->font) drawString(*s_->font, str, x, y);
}

void Graphics::drawGlyphs(Font& font, const std::string& str, float x, float y) {
    std::vector<GlyphQuad> quads;
    font.layoutGlyphs(str, x, y, quads);
    for (const GlyphQuad& q : quads) {
        float u0, v0, u1, v1;
        q.region.uvs(u0, v0, u1, v1);
        const float x2 = q.x + static_cast<float>(q.w), y2 = q.y + static_cast<float>(q.h);
        const float p[8] = {q.x, q.y, q.x, y2, x2, y2, x2, q.y};
        emitQuad(q.region.tex.get(), p, u0, v0, u1, v1, s_->rgba);
    }
}

void Graphics::drawString(Font& font, const std::string& str, float x, float y) {
    if (s_->pixelSnap && s_->transform.isScaled()) {
        // text made at the final size, at whole pixels, no scale transform (Graphics.drawStringSnapped)
        const Transform2D saved = s_->transform;
        const float sx = saved.a, sy = saved.d;
        std::shared_ptr<Font> scaled = font.getScaledVariant(sx);
        const float px = roundJava(x * sx + saved.tx), py = roundJava(y * sy + saved.ty);
        s_->transform.idt();
        drawGlyphs(*scaled, str, px, py);
        s_->transform = saved;
        return;
    }
    drawGlyphs(font, str, x, y);
}

// ---- shapes -----------------------------------------------------------------------------------------------------

void Graphics::fillRect(float x, float y, float w, float h) {
    const float p[8] = {x, y, x, y + h, x + w, y + h, x + w, y};
    emitSolidQuad(p, s_->rgba);
}

void Graphics::fillQuad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4) {
    const float a[6] = {x1, y1, x2, y2, x3, y3};
    const float b[6] = {x1, y1, x3, y3, x4, y4};
    emitTri(a, s_->rgba);
    emitTri(b, s_->rgba);
}

void Graphics::fillGradientRectH(float x, float y, float w, float h, const Color& left, const Color& right) {
    const float p[8] = {x, y, x, y + h, x + w, y + h, x + w, y};
    uint8_t c[16];
    const Color* cols[4] = {&left, &left, &right, &right};  // p1 (x,y) left, p2 (x,y+h) left, p3 right, p4 right
    for (int i = 0; i < 4; i++) {
        c[i * 4] = toByte(cols[i]->r); c[i * 4 + 1] = toByte(cols[i]->g);
        c[i * 4 + 2] = toByte(cols[i]->b); c[i * 4 + 3] = toByte(cols[i]->a);
    }
    emitSolidQuad4(p, c);
}

void Graphics::fillGradientRectV(float x, float y, float w, float h, const Color& top, const Color& bottom) {
    const float p[8] = {x, y, x, y + h, x + w, y + h, x + w, y};
    uint8_t c[16];
    const Color* cols[4] = {&top, &bottom, &bottom, &top};
    for (int i = 0; i < 4; i++) {
        c[i * 4] = toByte(cols[i]->r); c[i * 4 + 1] = toByte(cols[i]->g);
        c[i * 4 + 2] = toByte(cols[i]->b); c[i * 4 + 3] = toByte(cols[i]->a);
    }
    emitSolidQuad4(p, c);
}

// A line of lineWidth SCREEN pixels between two game points (before the transform), square ends of half a width.
void Graphics::emitLine(float x1, float y1, float x2, float y2) {
    float ax, ay, bx, by;
    s_->transform.apply(x1, y1, ax, ay);
    s_->transform.apply(x2, y2, bx, by);
    const float sx = s_->curMap.scaleX(), sy = s_->curMap.scaleY();
    float dx = (bx - ax) * sx, dy = (by - ay) * sy;  // in pixels
    float len = std::sqrt(dx * dx + dy * dy);
    const float hw = std::max(s_->lineWidth, 0.f) * 0.5f;
    float ux, uy;
    if (len < 1e-6f) { ux = 1.f; uy = 0.f; } else { ux = dx / len; uy = dy / len; }
    // pixel-space vectors: along (ux, uy), across (-uy, ux)
    const float ex = ux * hw, ey = uy * hw, nx = -uy * hw, ny = ux * hw;
    // corner offsets in pixels, converted back to game space (after the transform) below
    float px[4], py[4];
    const float ox[4] = {-ex + nx, -ex - nx, ex - nx, ex + nx};
    const float oy[4] = {-ey + ny, -ey - ny, ey - ny, ey + ny};
    // start corners relative to A, end corners relative to B
    px[0] = ax + ox[0] / sx; py[0] = ay + oy[0] / sy;
    px[1] = ax + ox[1] / sx; py[1] = ay + oy[1] / sy;
    px[2] = bx + ox[2] / sx; py[2] = by + oy[2] / sy;
    px[3] = bx + ox[3] / sx; py[3] = by + oy[3] / sy;
    submit(nullptr);
    static const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int k : order) pushV(s_->verts, px[k], py[k], 0.f, 0.f, s_->rgba);
}

void Graphics::drawLine(float x1, float y1, float x2, float y2) { emitLine(x1, y1, x2, y2); }

void Graphics::drawRect(float x, float y, float w, float h) {
    emitLine(x, y, x + w, y);
    emitLine(x + w, y, x + w, y + h);
    emitLine(x + w, y + h, x, y + h);
    emitLine(x, y + h, x, y);
}

void Graphics::drawRoundRect(float x, float y, int w, int h, int) {
    drawRect(x, y, static_cast<float>(w), static_cast<float>(h));
}
void Graphics::fillRoundRect(float x, float y, float w, float h, float) { fillRect(x, y, w, h); }

void Graphics::drawOval(float x, float y, float w, float h) {
    // ShapeRenderer.ellipse: segments = max(1, (int)(12 * cbrt(max(w/2, h/2))))
    const int seg = std::max(1, java::toInt(12.0f * std::cbrt(std::max(w * 0.5f, h * 0.5f))));
    const float angle = 2.f * kPi / static_cast<float>(seg);
    const float cx = x + w / 2.f, cy = y + h / 2.f;
    for (int i = 0; i < seg; i++) {
        emitLine(cx + w * 0.5f * std::cos(static_cast<float>(i) * angle), cy + h * 0.5f * std::sin(static_cast<float>(i) * angle),
                 cx + w * 0.5f * std::cos(static_cast<float>(i + 1) * angle),
                 cy + h * 0.5f * std::sin(static_cast<float>(i + 1) * angle));
    }
}

void Graphics::fillArc(float x, float y, float w, float h, float start, float end) {
    while (start < 0.f) start += 360.f;
    start = std::fmod(start, 360.f);
    end = std::fmod(end, 360.f);
    while (end < start) end += 360.f;
    const float degrees = end - start;
    const float rx = w / 2.f, ry = h / 2.f, cx = x + rx, cy = y + ry;
    // ShapeRenderer.arc: segments = max(1, (int)(6 * cbrt(radius) * (degrees / 360)))
    const int seg = std::max(1, java::toInt(6.0f * std::cbrt(std::max(rx, ry)) * (degrees / 360.0f)));
    const float theta = 2.f * kPi * (degrees / 360.0f) / static_cast<float>(seg);
    const float st = start * kPi / 180.f;
    for (int i = 0; i < seg; i++) {
        const float a0 = st + theta * static_cast<float>(i), a1 = st + theta * static_cast<float>(i + 1);
        const float t[6] = {cx, cy, cx + rx * std::cos(a0), cy + ry * std::sin(a0), cx + rx * std::cos(a1),
                            cy + ry * std::sin(a1)};
        emitTri(t, s_->rgba);
    }
}

void Graphics::fillTriangles(const float* xy, int triangleCount) {
    for (int i = 0; i < triangleCount; i++) emitTri(xy + i * 6, s_->rgba);
}

void Graphics::beginAdditiveShapes() {
    flush();
    s_->additiveShapes = true;
    applyDrawMode();
}

void Graphics::endAdditiveShapes() {
    flush();
    s_->additiveShapes = false;
    applyDrawMode();
}

// ---- frame / targets --------------------------------------------------------------------------------------------

void Graphics::clear() {
    flush();
    if (s_->cur != this) {
        // Java: bind this target, clear, unbind
        Graphics* prev = s_->cur;
        bindThis();
        s_->be->clear(s_->bg);
        if (prev) { s_->cur = prev; prev->bindThis(); }
        return;
    }
    s_->be->clear(s_->bg);
}

void Graphics::bindThis() {
    flush();
    s_->hasScissor = false;
    s_->be->setScissor(false, IRect());
    if (target_) {
        ViewportMap m(virtualW_ > 0.f ? virtualW_ : static_cast<float>(target_->width()),
                      virtualH_ > 0.f ? virtualH_ : static_cast<float>(target_->height()), target_->width(),
                      target_->height());
        s_->curMap = m;
        s_->curTarget = target_.get();
        s_->be->setTarget(target_.get(), m);
    } else {
        s_->curMap = s_->screenMap;
        s_->curTarget = nullptr;
        s_->be->setTarget(nullptr, s_->curMap);
    }
    s_->cur = this;
}

void Graphics::setCurrent(Graphics& g) {
    Graphics& scr = getGraphics();
    if (scr.s_->cur != &g) g.bindThis();
}

void Graphics::pushTarget(IRenderTarget* target, const ViewportMap& vm) {
    flush();
    s_->bindings.push_back(Shared::Binding{s_->curTarget, s_->curMap, s_->hasScissor, s_->scissor});
    s_->hasScissor = false;
    s_->be->setScissor(false, IRect());
    s_->curTarget = target;
    s_->curMap = vm;
    s_->be->setTarget(target, vm);
}

void Graphics::popTarget() {
    if (s_->bindings.empty()) return;
    flush();
    Shared::Binding b = s_->bindings.back();
    s_->bindings.pop_back();
    s_->curTarget = b.target;
    s_->curMap = b.map;
    s_->be->setTarget(b.target, b.map);
    s_->hasScissor = b.hasScissor;
    s_->scissor = b.scissor;
    s_->be->setScissor(b.hasScissor, b.scissor);
}

}  // namespace opsu
