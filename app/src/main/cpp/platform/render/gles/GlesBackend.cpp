#include "GlesBackend.h"

#include <algorithm>
#include <cstddef>

#include "GlesDevice.h"
#include "GlesShaders.h"

#define LOG_TAG "opsu-gles"

namespace opsu {
namespace gles {

bool g_glDebug = false;

namespace {
constexpr int kMaxVertices = 6 * 8192;  // 8192 quads per batch (983 KB)

GLenum blendFactor(BlendFactor f) {
    switch (f) {
        case BlendFactor::Zero: return GL_ZERO;
        case BlendFactor::One: return GL_ONE;
        case BlendFactor::SrcAlpha: return GL_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha: return GL_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
    }
    return GL_ONE;
}

GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024] = {0};
        glGetShaderInfoLog(s, sizeof(log) - 1, nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "shader compile failed: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}
}  // namespace

bool GlesBackend::initGl() {
    GLuint vs = compile(GL_VERTEX_SHADER, kVertexShader);
    GLuint fs = compile(GL_FRAGMENT_SHADER, kFragmentShader);
    if (!vs || !fs) return false;
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024] = {0};
        glGetProgramInfoLog(program_, sizeof(log) - 1, nullptr, log);
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "program link failed: %s", log);
        glDeleteProgram(program_);
        program_ = 0;
        return false;
    }
    uProj_ = glGetUniformLocation(program_, "uProj");
    uTex_ = glGetUniformLocation(program_, "uTex");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(Vertex)) * kMaxVertices, nullptr, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, r)));

    const uint8_t whitePx[4] = {255, 255, 255, 255};
    glGenTextures(1, &white_);
    glBindTexture(GL_TEXTURE_2D, white_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePx);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    boundTex_ = white_;

    glUseProgram(program_);
    glUniform1i(uTex_, 0);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DITHER);
    glEnable(GL_BLEND);

    verts_.clear();
    verts_.reserve(kMaxVertices);
    batchTex_ = 0;
    mode_ = DrawModeState();
    scissorOn_ = false;
    target_ = nullptr;
    live_ = true;
    applyMode();
    applyScissor();
    applyTarget();
    glCheck("initGl");
    return true;
}

void GlesBackend::loseGl() {
    live_ = false;
    program_ = vao_ = vbo_ = white_ = 0;
    boundTex_ = batchTex_ = 0;
    verts_.clear();
    target_ = nullptr;
}

void GlesBackend::bindTexture(GLuint id) {
    if (!live_ || id == boundTex_) return;
    glBindTexture(GL_TEXTURE_2D, id);
    boundTex_ = id;
}

void GlesBackend::textureDeleted(GLuint id) {
    if (!live_) return;
    if (batchTex_ == id) flushBatch();
    if (boundTex_ == id) boundTex_ = 0;
}

void GlesBackend::flushBatch() {
    if (!live_ || verts_.empty()) { verts_.clear(); return; }
    bindTexture(batchTex_ ? batchTex_ : white_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(Vertex)) * kMaxVertices, nullptr, GL_STREAM_DRAW);  // orphan
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(sizeof(Vertex) * verts_.size()), verts_.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts_.size()));
    drawCalls_++;
    verts_.clear();
    glCheck("flushBatch");
}

void GlesBackend::drawTriangles(ITexture* tex, const Vertex* v, int n) {
    if (!live_ || n <= 0) return;
    GLuint id = white_;
    if (tex) {
        id = static_cast<GlesTexture*>(tex)->glName();
        if (!id) return;  // texture lost and not restored yet
    }
    if (!verts_.empty() && id != batchTex_) flushBatch();
    batchTex_ = id;
    while (n > 0) {
        int room = kMaxVertices - static_cast<int>(verts_.size());
        room -= room % 3;
        if (room <= 0) { flushBatch(); continue; }
        const int take = std::min(n, room);
        verts_.insert(verts_.end(), v, v + take);
        v += take;
        n -= take;
    }
}

void GlesBackend::applyMode() {
    glColorMask(mode_.maskR, mode_.maskG, mode_.maskB, mode_.maskA);
    glBlendFunc(blendFactor(mode_.src), blendFactor(mode_.dst));
}

void GlesBackend::setDrawMode(const DrawModeState& m) {
    if (!live_) { mode_ = m; return; }
    if (m == mode_) return;
    flushBatch();
    mode_ = m;
    applyMode();
}

void GlesBackend::applyScissor() {
    if (scissorOn_) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(scissor_.x, scissor_.y, scissor_.w, scissor_.h);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
}

void GlesBackend::setScissor(bool on, const IRect& r) {
    if (!live_) { scissorOn_ = on; scissor_ = r; return; }
    flushBatch();
    scissorOn_ = on;
    scissor_ = r;
    applyScissor();
}

void GlesBackend::applyTarget() {
    glBindFramebuffer(GL_FRAMEBUFFER, target_ ? target_->fboName() : 0);
    const IRect vp = vm_.viewport();
    glViewport(vp.x, vp.y, vp.w, vp.h);
    glUniform4f(uProj_, 2.f / vm_.virtualW, -2.f / vm_.virtualH, -1.f, 1.f);
}

void GlesBackend::setTarget(IRenderTarget* t, const ViewportMap& vm) {
    vm_ = vm;
    if (!live_) { target_ = static_cast<GlesRenderTarget*>(t); return; }
    flushBatch();
    target_ = static_cast<GlesRenderTarget*>(t);
    applyTarget();
    glCheck("setTarget");
}

void GlesBackend::rebindTarget() {
    if (!live_) return;
    flushBatch();
    applyMode();
    applyScissor();
    applyTarget();
}

void GlesBackend::clear(const Color& c) {
    if (!live_) return;
    flushBatch();
    glClearColor(c.r, c.g, c.b, c.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void GlesBackend::beginFrame(const Color& c, int w, int h) {
    if (!live_) return;
    verts_.clear();
    lastDrawCalls_ = drawCalls_;
    drawCalls_ = 0;
    // whole surface, default framebuffer, everything writable, no scissor: then restore the cached state
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(c.r, c.g, c.b, c.a);
    glClear(GL_COLOR_BUFFER_BIT);
    applyMode();
    applyScissor();
    applyTarget();
    glCheck("beginFrame");
}

}  // namespace gles
}  // namespace opsu
