#version 440

layout(std140, binding = 0) uniform buf {
    highp mat4 matrix;
}
ubuf;

layout(location = 0) in highp vec4 position;
layout(location = 1) in highp vec2 texcoord;
layout(location = 2) in highp vec4 color;
layout(location = 0) out highp vec2 vTexcoord;
layout(location = 1) out highp vec4 vColor;

void main() {
    vTexcoord = texcoord;
    vColor = color;
    gl_Position = ubuf.matrix * position;
}
