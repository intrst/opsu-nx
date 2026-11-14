#pragma once
#include <memory>
#include <string>

#include "opsu/gfx/ImageManager.h"
#include "opsu/gfx/Texture.h"
#include "opsu/util/Color.h"

namespace opsu {

class Graphics;
class RenderTarget;

/**
 * Port of fluddokt.opsu.fake.Image: a picture drawn through the current Graphics. Loaded from a file or an APK
 * asset (path like "res/cursor.png"); files are decoded once and shared through ImageManager (atlas pages, hue shift).
 *
 * Copying: the Java "Image(Image copy)" constructor does NOT copy alpha, rotation or the image colour; so this class has
 * no C++ copy constructor, use copy() / getScaledCopy() / getSubImage(). Images can be moved. Every derived image holds
 * its own reference to the cached texture (the Java counts only the file-loaded one), so destroy() on one does not
 * pull the texture away from its copies; the destructor releases like destroy().
 * A file that cannot be decoded gives an empty image (width 0, draws nothing), like the Java's blank image.
 * setFlipped is per Image here (in the Java it flipped a TextureRegion shared with the copies).
 */
class Image {
public:
    Image();
    explicit Image(const std::string& filename);
    /** Hue rotated by `hueShift` degrees before upload; cached separately from the plain image. */
    Image(const std::string& filename, float hueShift);
    /** Image(Image copy, float w, float h): the same picture drawn at another size. */
    Image(const Image& copy, float w, float h);
    /** Image(Image copy, x, y, w, h): a sub rectangle in the copy's size units (from its top-left). */
    Image(const Image& copy, int x, int y, int w, int h);
    /** Offscreen image: render target backed (getGraphics() draws into it). */
    Image(int width, int height);

    Image(Image&& o) noexcept;
    Image& operator=(Image&& o) noexcept;
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;
    virtual ~Image();

    virtual int getWidth() const;
    virtual int getHeight() const;
    virtual Image getScaledCopy(float w, float h) const;
    virtual Image getScaledCopy(float f) const;
    Image copy() const;  ///< Image(Image copy): same picture and size, default alpha/rotation
    Image getSubImage(int x, int y, int w, int h) const;

    void setAlpha(float f);
    float getAlpha() const { return alpha_; }
    void setRotation(float degrees) { rotation_ = degrees; }
    float getRotation() const { return rotation_; }
    void rotate(float degrees) { rotation_ += degrees; }
    bool isDestroyed() const { return destroyed_; }
    void destroy();
    const std::string& getResourceReference() const { return filename_; }

    void draw();
    void draw(float x, float y);
    void draw(float x, float y, const Color& color);
    void draw(float x, float y, float w, float h);
    void draw(float x, float y, float w, float h, const Color& color);
    void drawCentered(float x, float y);
    void drawCentered(float x, float y, const Color& color);
    /** Centered at (x, y) and scaled, no scaled copy. */
    void drawCentered(float x, float y, float scale, const Color& color);
    /** Also multiplies the alpha with alphaMul (this image's alpha is not changed). */
    void drawCentered(float x, float y, float scale, float alphaMul, const Color& color);
    /** Also rotated by rotationDeg around the centre instead of this image's own rotation. */
    void drawCentered(float x, float y, float scale, float rotationDeg, float alphaMul, const Color& color);

    virtual TextureRegion getTextureRegion() const;
    void setFlipped(bool flipX, bool flipY);
    void startUse() {}
    void endUse() {}
    void setImageColor(float r, float g, float b, float a) { imageColor_ = Color(r, g, b, a); }
    void drawEmbedded(float x, float y, int w, int h, float rotation);
    void drawEmbedded(float x, float y, float w, float h, int angle);

    /** Alpha (0..255) of the file's pixel at (x, y) given in this image's size units. Decodes the file again on first use. */
    float getAlphaAt(int x, int y);
    Color getColor(int x, int y);
    /** The graphics of an offscreen image; throws std::logic_error for other images (Java: Error). */
    Graphics& getGraphics();

    static void clearAll() { ImageManager::instance().clearAll(); }
    static void resetForNewContext() {}  ///< not needed: textures are restored by the backend; kept for the call sites
    static void beginBatch() {}
    static void endBatch() {}
    static void prefetch(const std::string&) {}  ///< not ported: images are decoded when they are made
    static void clearPrefetch() {}

protected:
    TextureRegion region_;
    float width_ = 0.f, height_ = 0.f;

private:
    bool usable() const;
    void retainEntry();
    bool ensurePixmap();

    std::shared_ptr<ImageManager::Entry> entry_;
    std::shared_ptr<RenderTarget> rt_;
    std::string filename_;
    float alpha_ = 1.f, rotation_ = 0.f;
    Color imageColor_{1.f, 1.f, 1.f, 1.f};
    bool destroyed_ = false;
    std::unique_ptr<Pixmap> pixmap_;
};

}  // namespace opsu
