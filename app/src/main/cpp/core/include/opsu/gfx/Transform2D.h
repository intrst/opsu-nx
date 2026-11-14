#pragma once
#include <vector>

namespace opsu {

/**
 * 2D affine transform, the part of libGDX's Matrix4 that fluddokt.opsu.fake.Graphics uses.
 *   x' = a*x + c*y + tx
 *   y' = b*x + d*y + ty
 * translate/scale/rotate POST-multiply (this = this * T), exactly like Matrix4.translate/scale/rotate, so the
 * call order of the Java code gives the same result. Coordinates are the game's (y down), so a positive angle
 * turns clockwise on the screen.
 */
struct Transform2D {
    float a = 1.f, b = 0.f, c = 0.f, d = 1.f, tx = 0.f, ty = 0.f;

    void idt() { *this = Transform2D(); }

    Transform2D& translate(float x, float y);
    Transform2D& scale(float sx, float sy);
    /** Rotation about the origin, in degrees. Multiples of 90 are exact (no cos(90) = 4e-8 noise). */
    Transform2D& rotate(float degrees);
    /** Graphics.rotate(x, y, angle): translate(x, y); rotate(angle); translate(-x, -y). */
    Transform2D& rotateAbout(float x, float y, float degrees);
    /** this = this * o */
    Transform2D& mul(const Transform2D& o);

    void apply(float x, float y, float& ox, float& oy) const {
        ox = a * x + c * y + tx;
        oy = b * x + d * y + ty;
    }

    /** Graphics.isScaled(): M00 or M11 differs from 1 by more than 0.001. */
    bool isScaled() const;
    bool isIdentity() const { return a == 1.f && b == 0.f && c == 0.f && d == 1.f && tx == 0.f && ty == 0.f; }
    /** False when the matrix is singular. */
    bool invert(Transform2D& out) const;
};

/** Graphics.pushTransform/popTransform. popping an empty stack throws std::out_of_range (Java: NoSuchElementException). */
class TransformStack {
public:
    void push(const Transform2D& t) { stack_.push_back(t); }
    Transform2D pop();
    size_t size() const { return stack_.size(); }
    void clear() { stack_.clear(); }

private:
    std::vector<Transform2D> stack_;
};

}  // namespace opsu
