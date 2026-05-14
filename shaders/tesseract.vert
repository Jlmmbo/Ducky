#version 330 core
layout(location = 0) in vec4 aPos;
layout(location = 1) in vec3 aTexCoord;

out vec3 vTexCoord;

uniform float angleXY;
uniform float angleXZ;
uniform float angleXW;
uniform float angleYZ;
uniform float angleYW;
uniform float angleZW;
uniform vec4 translation;
uniform float uAspect;

// Projection constants
const float DIST_4D = 4.0;
const float DIST_3D = 3.0;
const float Z_NEAR = 0.1;
const float Z_FAR = 20.0;

void main() {
    float x = aPos.x + translation.x;
    float y = aPos.y + translation.y;
    float z = aPos.z + translation.z;
    float w = aPos.w + translation.w;

    float c, s, nx, ny, nz, nw;

    // Apply 4D rotations in fixed order
    c = cos(angleXY); s = sin(angleXY);
    nx = x * c - y * s; ny = x * s + y * c;
    x = nx; y = ny;

    c = cos(angleXZ); s = sin(angleXZ);
    nx = x * c - z * s; nz = x * s + z * c;
    x = nx; z = nz;

    c = cos(angleXW); s = sin(angleXW);
    nx = x * c - w * s; nw = x * s + w * c;
    x = nx; w = nw;

    c = cos(angleYZ); s = sin(angleYZ);
    ny = y * c - z * s; nz = y * s + z * c;
    y = ny; z = nz;

    c = cos(angleYW); s = sin(angleYW);
    ny = y * c - w * s; nw = y * s + w * c;
    y = ny; w = nw;

    c = cos(angleZW); s = sin(angleZW);
    nz = z * c - w * s; nw = z * s + w * c;
    z = nz; w = nw;

    // Project 4D → 3D with perspective
    float wDepth = DIST_4D - w;
    float scale4d = wDepth > 0.001 ? DIST_4D / wDepth : 10.0;
    vec3 p3 = vec3(x, y, z) * scale4d;

    // Project 3D → 2D with perspective
    float zDepth = DIST_3D - p3.z;
    float perspDiv = zDepth > 0.001 ? zDepth : 0.001;

    float z_eye = p3.z - DIST_3D;
    float clip_z = z_eye * (Z_FAR + Z_NEAR) / (Z_NEAR - Z_FAR) + 2.0 * Z_FAR * Z_NEAR / (Z_NEAR - Z_FAR);
    gl_Position = vec4(p3.x * DIST_3D / uAspect, p3.y * DIST_3D, clip_z, perspDiv);
    vTexCoord = vec3(x, y, z) + 0.5;
}
