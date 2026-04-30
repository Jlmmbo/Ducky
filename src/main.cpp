#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <cstring>

#include "main.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

const char* vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec4 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

uniform float angleXY;
uniform float angleXZ;
uniform float angleXW;
uniform float angleYZ;
uniform float angleYW;
uniform float angleZW;
uniform vec4 translation;

void main() {
    float x = aPos.x + translation.x;
    float y = aPos.y + translation.y;
    float z = aPos.z + translation.z;
    float w = aPos.w + translation.w;

    float c, s, nx, ny, nz, nw;

    // XY rotation
    c = cos(angleXY); s = sin(angleXY);
    nx = x * c - y * s; ny = x * s + y * c;
    x = nx; y = ny;

    // XZ rotation
    c = cos(angleXZ); s = sin(angleXZ);
    nx = x * c - z * s; nz = x * s + z * c;
    x = nx; z = nz;

    // XW rotation
    c = cos(angleXW); s = sin(angleXW);
    nx = x * c - w * s; nw = x * s + w * c;
    x = nx; w = nw;

    // YZ rotation
    c = cos(angleYZ); s = sin(angleYZ);
    ny = y * c - z * s; nz = y * s + z * c;
    y = ny; z = nz;

    // YW rotation
    c = cos(angleYW); s = sin(angleYW);
    ny = y * c - w * s; nw = y * s + w * c;
    y = ny; w = nw;

    // ZW rotation
    c = cos(angleZW); s = sin(angleZW);
    nz = z * c - w * s; nw = z * s + w * c;
    z = nz; w = nw;

    // Projection
    float dist4d = 4.0;
    float wDepth = dist4d - w;
    float scale4d = clamp(dist4d / wDepth, 0.1, 10.0);
    vec3 p3 = vec3(x, y, z) * scale4d;

    float dist3d = 3.0;
    float zDepth = dist3d - p3.z;
    float perspDiv = max(zDepth, 1.0);

    gl_Position = vec4(p3.xy * dist3d, p3.z * dist3d, perspDiv);
    vTexCoord = aTexCoord;
}
)";

const char* fragmentShaderSrc = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;

void main() {
    FragColor = texture(uTexture, vTexCoord);
}
)";

// Axes shaders
const char* axesVertexSrc = R"(
#version 330 core
layout(location = 0) in vec4 aPos;
layout(location = 1) in vec3 aColor;

out vec3 vColor;

uniform float angleXY;
uniform float angleXZ;
uniform float angleXW;
uniform float angleYZ;
uniform float angleYW;
uniform float angleZW;

void main() {
    float x = aPos.x;
    float y = aPos.y;
    float z = aPos.z;
    float w = aPos.w;

    float c, s, nx, ny, nz, nw;

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

    float dist4d = 4.0;
    float wDepth = dist4d - w;
    float scale4d = clamp(dist4d / wDepth, 0.1, 10.0);
    vec3 p3 = vec3(x, y, z) * scale4d;

    float dist3d = 3.0;
    float zDepth = dist3d - p3.z;
    float perspDiv = max(zDepth, 1.0);

    gl_Position = vec4(p3.xy * dist3d, p3.z * dist3d, perspDiv);
    vColor = aColor;
}
)";

const char* axesFragmentSrc = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

// Text shaders (2D screen-space)
const char* textVertexSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;
out vec4 vColor;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vColor = aColor;
}
)";

const char* textFragmentSrc = R"(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)";

static unsigned int compileShader(unsigned int type, const char* src) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        std::cout << "Shader error:\n" << log << std::endl;
    }
    return shader;
}

int main() {
    glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1920, 1920, "4D Tesseract", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create window\n";
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to init GLAD\n";
        return -1;
    }

    glViewport(0, 0, 1920, 1920);
    glEnable(GL_DEPTH_TEST);

    Model model = LoadModel("model.dky");
    std::cout << "Loaded " << model.vertexCount << " vertices, " << model.indexCount << " indices\n";

    // Tesseract VAO
    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, model.vertexCount * 6 * sizeof(float), model.vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.indexCount * sizeof(unsigned int), model.indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Tesseract shader
    unsigned int vertshader = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    unsigned int fragshader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertshader);
    glAttachShader(shaderProgram, fragshader);
    glLinkProgram(shaderProgram);

    // Texture
    int width, height, channels;
    unsigned char* data = stbi_load("00001.png", &width, &height, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load texture\n";
        return -1;
    }

    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    std::cout << "Texture loaded: " << width << "x" << height << " channels=" << channels << std::endl;
    stbi_image_free(data);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(glGetUniformLocation(shaderProgram, "uTexture"), 0);

    // Axes shader
    unsigned int axesVert = compileShader(GL_VERTEX_SHADER, axesVertexSrc);
    unsigned int axesFrag = compileShader(GL_FRAGMENT_SHADER, axesFragmentSrc);
    unsigned int axesProgram = glCreateProgram();
    glAttachShader(axesProgram, axesVert);
    glAttachShader(axesProgram, axesFrag);
    glLinkProgram(axesProgram);

    // Axes vertices: 4 axes, each 2 points (origin + direction), each vertex: x,y,z,w,r,g,b
    float axesVertices[] = {
        // X axis red
        0,0,0,0, 1,0,0,
        1.5,0,0,0, 1,0,0,
        // Y axis green
        0,0,0,0, 0,1,0,
        0,1.5,0,0, 0,1,0,
        // Z axis blue
        0,0,0,0, 0,0,1,
        0,0,1.5,0, 0,0,1,
        // W axis purple
        0,0,0,0, 0.5,0,0.5,
        0,0,0,1.5, 0.5,0,0.5,
    };
    unsigned int axesVBO, axesVAO;
    glGenVertexArrays(1, &axesVAO);
    glGenBuffers(1, &axesVBO);
    glBindVertexArray(axesVAO);
    glBindBuffer(GL_ARRAY_BUFFER, axesVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(axesVertices), axesVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Text shader
    unsigned int textVert = compileShader(GL_VERTEX_SHADER, textVertexSrc);
    unsigned int textFrag = compileShader(GL_FRAGMENT_SHADER, textFragmentSrc);
    unsigned int textProgram = glCreateProgram();
    glAttachShader(textProgram, textVert);
    glAttachShader(textProgram, textFrag);
    glLinkProgram(textProgram);

    // Text: control hints
    const char* hintText = "Controls: WASD-move XY, QE-move Z, ZX-move W, 1234567890-=-rotate planes";
    char textBuffer[20000];
    int numQuads = stb_easy_font_print(10, 1880, (char*)hintText, NULL, textBuffer, sizeof(textBuffer));
    // Convert pixel coordinates to NDC
    for (int i = 0; i < numQuads * 4; i++) {
        float* v = (float*)(textBuffer + i * 16);
        float x = v[0];
        float y = v[1];
        v[0] = (x / 1920.0f) * 2.0f - 1.0f;
        v[1] = 1.0f - (y / 1920.0f) * 2.0f; // flip Y
    }

    unsigned int textVBO, textVAO;
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, numQuads * 64, textBuffer, GL_STATIC_DRAW);
    // Text vertex format: float x, float y, float z, unsigned char color[4]
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void*)12);
    glEnableVertexAttribArray(1);

    glUseProgram(shaderProgram);

    float angleXY = 0.0f, angleXZ = 0.0f, angleXW = 0.0f;
    float angleYZ = 0.0f, angleYW = 0.0f, angleZW = 0.0f;
    float transX = 0.0f, transY = 0.0f, transZ = 0.0f, transW = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        // Input handling
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) transX += 0.02f;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) transX -= 0.02f;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) transY += 0.02f;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) transY -= 0.02f;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) transZ += 0.02f;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) transZ -= 0.02f;
        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) transW += 0.02f;
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) transW -= 0.02f;

        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) angleXY += 0.02f;
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) angleXY -= 0.02f;
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) angleXZ += 0.02f;
        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) angleXZ -= 0.02f;
        if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) angleXW += 0.02f;
        if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) angleXW -= 0.02f;
        if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) angleYZ += 0.02f;
        if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) angleYZ -= 0.02f;
        if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) angleYW += 0.02f;
        if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) angleYW -= 0.02f;
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) angleZW += 0.02f;
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) angleZW -= 0.02f;

        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Draw tesseract
        glUseProgram(shaderProgram);
        glUniform1f(glGetUniformLocation(shaderProgram, "angleXY"), angleXY);
        glUniform1f(glGetUniformLocation(shaderProgram, "angleXZ"), angleXZ);
        glUniform1f(glGetUniformLocation(shaderProgram, "angleXW"), angleXW);
        glUniform1f(glGetUniformLocation(shaderProgram, "angleYZ"), angleYZ);
        glUniform1f(glGetUniformLocation(shaderProgram, "angleYW"), angleYW);
        glUniform1f(glGetUniformLocation(shaderProgram, "angleZW"), angleZW);
        glUniform4f(glGetUniformLocation(shaderProgram, "translation"), transX, transY, transZ, transW);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, model.indexCount, GL_UNSIGNED_INT, 0);

        // Draw axes
        glUseProgram(axesProgram);
        glUniform1f(glGetUniformLocation(axesProgram, "angleXY"), angleXY);
        glUniform1f(glGetUniformLocation(axesProgram, "angleXZ"), angleXZ);
        glUniform1f(glGetUniformLocation(axesProgram, "angleXW"), angleXW);
        glUniform1f(glGetUniformLocation(axesProgram, "angleYZ"), angleYZ);
        glUniform1f(glGetUniformLocation(axesProgram, "angleYW"), angleYW);
        glUniform1f(glGetUniformLocation(axesProgram, "angleZW"), angleZW);
        glBindVertexArray(axesVAO);
        glDrawArrays(GL_LINES, 0, 8);

        // Draw text
        glUseProgram(textProgram);
        glBindVertexArray(textVAO);
        glDrawArrays(GL_QUADS, 0, numQuads * 4);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    return 0;
}
