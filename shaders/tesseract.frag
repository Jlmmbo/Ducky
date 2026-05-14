#version 330 core
in vec3 vTexCoord;
out vec4 FragColor;
uniform sampler3D uTexture;

void main() {
    FragColor = texture(uTexture, vTexCoord);
}
