#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

out vec3 vColor;

uniform float uAspect;

const float DIST_3D = 3.0;
const float Z_NEAR = 0.1;
const float Z_FAR = 20.0;

void main() {
    float zDepth = DIST_3D - aPos.z;
    float perspDiv = zDepth > 0.001 ? zDepth : 0.001;

    float z_eye = aPos.z - DIST_3D;
    float clip_z = z_eye * (Z_FAR + Z_NEAR) / (Z_NEAR - Z_FAR) + 2.0 * Z_FAR * Z_NEAR / (Z_NEAR - Z_FAR);
    gl_Position = vec4(aPos.x * DIST_3D / uAspect, aPos.y * DIST_3D, clip_z, perspDiv);
    vColor = aColor;
}
