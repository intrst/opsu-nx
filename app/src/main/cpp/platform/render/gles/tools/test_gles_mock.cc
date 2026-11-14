// Host test of the GLES backend logic against a FAKE GL (records calls). Not a GPU test: it checks batching, state changes,
// texture upload/restore and context-loss handling. Run: sh tools/run_mock_test.sh
#include <cstdarg>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

#include "../GlesDevice.h"
#include "opsu/gfx/Colors.h"
#include "opsu/gfx/Graphics.h"
#include "opsu/gfx/Image.h"
#include "opsu/gfx/RenderTarget.h"

// ---- fake GL / Android -------------------------------------------------------------------------------------------
struct FakeGl {
    int draws = 0, genTex = 0, delTex = 0, texImage = 0, texSub = 0, binds = 0, clears = 0, scissorEnables = 0;
    GLuint nextName = 1, curTex = 0;
    std::set<GLuint> live;
    GLboolean mask[4] = {1, 1, 1, 1};
    GLenum src = 0, dst = 0;
    GLuint fbo = 0;
    bool scissor = false;
    GLint vp[4] = {0, 0, 0, 0};
    std::map<GLuint, std::vector<unsigned char>> pixels;  // texture name -> RGBA (for restore checks), first 4 bytes of each upload
} gl;
GLenum glGetError() { return 0; }
const unsigned char* glGetString(GLenum) { return reinterpret_cast<const unsigned char*>("fake"); }
void glEnable(GLenum c) { if (c == GL_SCISSOR_TEST) { gl.scissor = true; gl.scissorEnables++; } }
void glDisable(GLenum c) { if (c == GL_SCISSOR_TEST) gl.scissor = false; }
void glBlendFunc(GLenum s, GLenum d) { gl.src = s; gl.dst = d; }
void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) { gl.mask[0] = r; gl.mask[1] = g; gl.mask[2] = b; gl.mask[3] = a; }
void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) { gl.vp[0] = x; gl.vp[1] = y; gl.vp[2] = w; gl.vp[3] = h; }
void glScissor(GLint, GLint, GLsizei, GLsizei) {}
void glClearColor(GLfloat, GLfloat, GLfloat, GLfloat) {}
void glClear(GLbitfield) { gl.clears++; }
void glPixelStorei(GLenum, GLint) {}
void glGetIntegerv(GLenum, GLint* v) { *v = 4096; }
void glGenTextures(GLsizei n, GLuint* ids) { for (int i = 0; i < n; i++) { ids[i] = gl.nextName++; gl.live.insert(ids[i]); gl.genTex++; } }
void glDeleteTextures(GLsizei n, const GLuint* ids) { for (int i = 0; i < n; i++) { gl.live.erase(ids[i]); gl.delTex++; } }
void glBindTexture(GLenum, GLuint id) { gl.curTex = id; gl.binds++; }
void glActiveTexture(GLenum) {}
void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void* p) {
    gl.texImage++;
    gl.pixels[gl.curTex] = p ? std::vector<unsigned char>(static_cast<const unsigned char*>(p), static_cast<const unsigned char*>(p) + 4) : std::vector<unsigned char>(4, 0);
}
void glTexSubImage2D(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void* p) {
    gl.texSub++;
    gl.pixels[gl.curTex] = std::vector<unsigned char>(static_cast<const unsigned char*>(p), static_cast<const unsigned char*>(p) + 4);
}
void glTexParameteri(GLenum, GLenum, GLint) {}
void glGenBuffers(GLsizei n, GLuint* ids) { for (int i = 0; i < n; i++) ids[i] = gl.nextName++; }
void glBindBuffer(GLenum, GLuint) {}
void glBufferData(GLenum, GLsizeiptr, const void*, GLenum) {}
void glBufferSubData(GLenum, GLsizeiptr, GLsizeiptr, const void*) {}
void glGenVertexArrays(GLsizei n, GLuint* ids) { for (int i = 0; i < n; i++) ids[i] = gl.nextName++; }
void glBindVertexArray(GLuint) {}
void glEnableVertexAttribArray(GLuint) {}
void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) {}
GLuint glCreateShader(GLenum) { return gl.nextName++; }
void glShaderSource(GLuint, GLsizei, const GLchar* const*, const GLint*) {}
void glCompileShader(GLuint) {}
void glGetShaderiv(GLuint, GLenum, GLint* v) { *v = 1; }
void glGetShaderInfoLog(GLuint, GLsizei, GLsizei*, GLchar*) {}
void glDeleteShader(GLuint) {}
GLuint glCreateProgram() { return gl.nextName++; }
void glAttachShader(GLuint, GLuint) {}
void glLinkProgram(GLuint) {}
void glGetProgramiv(GLuint, GLenum, GLint* v) { *v = 1; }
void glGetProgramInfoLog(GLuint, GLsizei, GLsizei*, GLchar*) {}
void glDeleteProgram(GLuint) {}
void glUseProgram(GLuint) {}
GLint glGetUniformLocation(GLuint, const GLchar*) { return 1; }
void glUniform1i(GLint, GLint) {}
void glUniform4f(GLint, GLfloat, GLfloat, GLfloat, GLfloat) {}
void glDrawArrays(GLenum, GLint, GLsizei) { gl.draws++; }
void glGenFramebuffers(GLsizei n, GLuint* ids) { for (int i = 0; i < n; i++) ids[i] = gl.nextName++; }
void glDeleteFramebuffers(GLsizei, const GLuint*) {}
void glBindFramebuffer(GLenum, GLuint f) { gl.fbo = f; }
void glFramebufferTexture2D(GLenum, GLenum, GLenum, GLuint, GLint) {}
GLenum glCheckFramebufferStatus(GLenum) { return GL_FRAMEBUFFER_COMPLETE; }
int __android_log_print(int, const char*, const char*, ...) { return 0; }
AAsset* AAssetManager_open(AAssetManager*, const char*, int) { return nullptr; }
off_t AAsset_getLength(AAsset*) { return 0; }
int AAsset_read(AAsset*, void*, size_t) { return 0; }
void AAsset_close(AAsset*) {}

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

using namespace opsu;

int main() {
    auto dev = std::make_shared<gles::GlesDevice>();
    setRenderDevice(dev);
    CHECK(dev->onContextCreated());
    dev->setSurfaceSize(800, 600);
    static auto g = Graphics::createScreen(dev->backend());
    g->resize(800, 600);
    CHECK(gl.vp[2] == 800 && gl.vp[3] == 600);

    // a texture with a restorer that writes a marker byte
    int restores = 0;
    auto tex = dev->createTexture(16, 16, nullptr, TextureFilter::Linear, [&restores](ITexture& t) {
        restores++;
        const uint8_t px[4] = {7, 7, 7, 7};
        t.update(0, 0, 1, 1, px);
    });
    auto tex2 = dev->createTexture(16, 16, nullptr, TextureFilter::Linear, nullptr);
    auto gt = std::static_pointer_cast<gles::GlesTexture>(tex);
    TextureRegion r1; r1.tex = tex; r1.w = 16; r1.h = 16;
    TextureRegion r2; r2.tex = tex2; r2.w = 16; r2.h = 16;

    // batching: 1000 sprites with the same texture = ONE draw call; texture change / solid / mode change split it
    g->setColor(colors::white);
    int before = gl.draws;
    for (int i = 0; i < 1000; i++) g->drawTexture(r1, static_cast<float>(i % 50), static_cast<float>(i / 50), 4, 4, 0.f);
    g->flush();
    CHECK(gl.draws - before == 1);
    before = gl.draws;
    g->drawTexture(r1, 0, 0, 4, 4, 0); g->drawTexture(r2, 0, 0, 4, 4, 0); g->fillRect(0, 0, 3, 3); g->drawTexture(r2, 0, 0, 4, 4, 0);
    g->flush();
    CHECK(gl.draws - before == 4);
    before = gl.draws;
    g->drawTexture(r1, 0, 0, 4, 4, 0); g->setDrawMode(Graphics::MODE_ALPHA_MAP); g->drawTexture(r1, 0, 0, 4, 4, 0); g->flush();
    CHECK(gl.draws - before == 2 && !gl.mask[0] && !gl.mask[1] && gl.mask[2] && gl.mask[3] && gl.src == GL_ONE && gl.dst == GL_ZERO);
    g->setDrawMode(Graphics::MODE_ALPHA_BLEND); CHECK(gl.mask[0] && gl.mask[1] && !gl.mask[2] && !gl.mask[3] && gl.src == GL_DST_ALPHA);
    g->setDrawMode(Graphics::MODE_ADD); CHECK(gl.mask[3] && gl.src == GL_SRC_ALPHA && gl.dst == GL_ONE);
    g->setDrawMode(Graphics::MODE_NORMAL); CHECK(gl.dst == GL_ONE_MINUS_SRC_ALPHA);
    before = gl.draws; g->drawTexture(r1, 0, 0, 4, 4, 0); g->setClip(0, 0, 10, 10); CHECK(gl.scissor); g->drawTexture(r1, 0, 0, 4, 4, 0); g->clearClip(); g->flush();
    CHECK(gl.draws - before == 2 && !gl.scissor);  // the scissor change submitted the sprite drawn before it

    // context loss: names die, objects stay; restore refills; a destructor after the loss deletes nothing
    int del = gl.delTex;
    GLuint oldName = gt->glName();
    CHECK(oldName != 0);
    g->drawTexture(r1, 0, 0, 4, 4, 0);  // pending batch is dropped with the context
    dev->onContextLost();
    CHECK(gt->glName() == 0 && !dev->glValid());
    int d = gl.draws; g->drawTexture(r1, 0, 0, 4, 4, 0); g->setClip(1, 1, 5, 5); g->flush(); CHECK(gl.draws == d);  // silent while lost
    { auto dead = dev->createTexture(8, 8, nullptr, TextureFilter::Nearest, nullptr); }  // made and destroyed while lost: no GL call
    CHECK(gl.delTex == del);
    gl.live.clear();  // the new context knows none of the old names
    CHECK(dev->onContextCreated());
    CHECK(restores == 1 && gt->glName() != 0 && gt->glName() != oldName && gl.pixels[gt->glName()][0] == 7);
    g->resize(800, 600);
    d = gl.draws; g->drawTexture(r1, 0, 0, 4, 4, 0); g->flush(); CHECK(gl.draws == d + 1);

    // render target: bind/unbind, texture draws as a normal region
    auto rt = RenderTarget::create(128, 64);
    CHECK(rt != nullptr);
    rt->bind(); CHECK(gl.fbo != 0 && gl.vp[2] == 128 && gl.vp[3] == 64);
    rt->unbind(); CHECK(gl.fbo == 0 && gl.vp[2] == 800);
    dev->onContextLost(); gl.live.clear(); CHECK(dev->onContextCreated());  // FBO rebuilt
    rt->bind(); CHECK(gl.fbo != 0); rt->unbind();

    // beginFrame clears the whole surface and restores the cached viewport
    int c = gl.clears; dev->gl().beginFrame(Color(0, 0, 0, 1), 800, 600); CHECK(gl.clears == c + 1 && gl.vp[2] == 800);

    rt.reset(); tex.reset(); tex2.reset();
    if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
    std::printf("test_gles_mock: all passed\n");
    return 0;
}
