#pragma once
// ALL shader source of the GLES backend lives in this file so a Vulkan backend can port it 1:1 (GLSL ES 3.00).
// One program draws everything: a textured, vertex-coloured triangle. Solid shapes sample a 1x1 white texture, so there is
// no program switch inside a frame. Colour is NOT premultiplied (the draw modes in DrawMode.h expect straight alpha).
namespace opsu {
namespace gles {

// uProj.xy = scale, uProj.zw = offset: game coordinates (y down) -> clip space.
//   x_ndc = x * (2 / virtualW) - 1      y_ndc = y * (-2 / virtualH) + 1
inline constexpr const char* kVertexShader = R"GLSL(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
uniform vec4 uProj;
out vec2 vUV;
out vec4 vColor;
void main() {
    vUV = aUV;
    vColor = aColor;
    gl_Position = vec4(aPos * uProj.xy + uProj.zw, 0.0, 1.0);
}
)GLSL";

inline constexpr const char* kFragmentShader = R"GLSL(#version 300 es
precision highp float;
uniform sampler2D uTex;
in vec2 vUV;
in vec4 vColor;
out vec4 oColor;
void main() {
    oColor = texture(uTex, vUV) * vColor;
}
)GLSL";

}  // namespace gles
}  // namespace opsu
