#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
uniform vec2 uScreenSize;
out vec4 vColor;
void main() {
    gl_Position = vec4(aPos.x / uScreenSize.x * 2.0 - 1.0, 1.0 - aPos.y / uScreenSize.y * 2.0, 0.0, 1.0);
    vColor = aColor;
}
