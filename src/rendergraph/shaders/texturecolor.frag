#version 440

layout(binding = 1) uniform sampler2D texture1;
layout(location = 0) in highp vec2 vTexcoord;
layout(location = 1) in highp vec4 vColor;
layout(location = 0) out highp vec4 fragColor;

void main() {
    // The texture is premultiplied, so scaling all components by the vertex
    // alpha keeps the blend result correct.
    fragColor = texture(texture1, vTexcoord) * vColor;
}
