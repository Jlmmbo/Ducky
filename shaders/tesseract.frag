#version 330 core
flat in vec3 vColor;
in vec3 vPos;
out vec4 FragColor;
uniform float uAlpha;
uniform float uLighting;

void main() {
    vec3 lightDir = normalize(vec3(0.3, 0.5, 0.8));
    vec3 normal = cross(dFdx(vPos), dFdy(vPos));
    float len = length(normal);
    if (len < 0.0001)
        normal = vec3(0.0, 0.0, 1.0);
    else
        normal /= len;

    float diff = max(dot(normal, lightDir), 0.0);
    float ambient = 0.3;
    float lightingFactor = ambient + diff * (1.0 - ambient);

    float l = mix(1.0, lightingFactor, uLighting);
    FragColor = vec4(vColor * l, uAlpha);
}
