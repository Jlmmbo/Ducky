#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

flat out vec3 vColor;
out vec3 vPos;

uniform float uAspect;
uniform float uDist3D;
uniform float uNear;
uniform float uFar;

void main() {
    float zDepth = uDist3D - aPos.z;
    float perspDiv = max(zDepth, 0.1);

    float z_eye = aPos.z - uDist3D;
    float clip_z = z_eye * (uNear + uFar) / (uFar - uNear) + 2.0 * uNear * uFar / (uFar - uNear);
    gl_Position = vec4(aPos.x * uDist3D / uAspect, aPos.y * uDist3D, clip_z, perspDiv);
    vColor = aColor;
    vPos = aPos;
}
