#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

flat out vec3 vColor;
out vec3 vPos;

uniform float uAspect;
uniform float uDist3D;
uniform int uRenderMode;

const float Z_NEAR = 0.1;
const float Z_FAR = 1000.0;

void main() {
    float w;
    float clip_z;
    if (uRenderMode == 2) {
        w = 1.0;
        clip_z = -2.0 * aPos.z / (Z_FAR - Z_NEAR) - (Z_FAR + Z_NEAR) / (Z_FAR - Z_NEAR);
    } else {
        float perspDiv = max(-aPos.z, 0.1);
        w = perspDiv;
        clip_z = aPos.z * (Z_FAR + Z_NEAR) / (Z_NEAR - Z_FAR) + 2.0 * Z_FAR * Z_NEAR / (Z_NEAR - Z_FAR);
    }
    gl_Position = vec4(aPos.x * uDist3D / uAspect, aPos.y * uDist3D, clip_z, w);
    vColor = aColor;
    vPos = aPos;
}
