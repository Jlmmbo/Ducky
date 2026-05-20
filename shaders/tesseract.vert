#version 330 core
layout(location = 0) in vec4 aPos;
layout(location = 1) in vec3 aTexCoord;

out vec3 vTexCoord;

uniform vec2 uRotXW;
uniform vec2 uRotYW;
uniform vec2 uRotZW;
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

    // Texture coords in model space (unaffected by translation)
    float tx = aPos.x, ty = aPos.y, tz = aPos.z, tw = aPos.w;

    float c, s, nx, ny, nz, nw;

    c = uRotXW.x; s = uRotXW.y;
    nx = x * c - w * s; nw = x * s + w * c; x = nx; w = nw;
    nx = tx * c - tw * s; nw = tx * s + tw * c; tx = nx; tw = nw;

    c = uRotYW.x; s = uRotYW.y;
    ny = y * c - w * s; nw = y * s + w * c; y = ny; w = nw;
    ny = ty * c - tw * s; nw = ty * s + tw * c; ty = ny; tw = nw;

    c = uRotZW.x; s = uRotZW.y;
    nz = z * c - w * s; nw = z * s + w * c; z = nz; w = nw;
    nz = tz * c - tw * s; nw = tz * s + tw * c; tz = nz; tw = nw;

    float wDepth = DIST_4D - w;
    float scale4d = wDepth > 0.001 ? DIST_4D / wDepth : 10.0;
    vec3 p3 = vec3(x, y, z) * scale4d;

    float zDepth = DIST_3D - p3.z;
    float perspDiv = zDepth > 0.001 ? zDepth : 0.001;

    float z_eye = p3.z - DIST_3D;
    float clip_z = z_eye * (Z_FAR + Z_NEAR) / (Z_NEAR - Z_FAR) + 2.0 * Z_FAR * Z_NEAR / (Z_NEAR - Z_FAR);
    gl_Position = vec4(p3.x * DIST_3D / uAspect, p3.y * DIST_3D, clip_z, perspDiv);
    vTexCoord = vec3(tx, ty, tz) + 0.5;
}
