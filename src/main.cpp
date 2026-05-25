#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <cstdlib>

#include "main.hpp"

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

constexpr int WINDOW_WIDTH = 1920;
constexpr int WINDOW_HEIGHT = 1920;
constexpr float MOVE_SPEED = 0.02f;
constexpr float ROTATE_SPEED = 0.02f;
constexpr float AXIS_LENGTH = 1.5f;

constexpr float PANEL_WIDTH = 290.0f;
constexpr float SLIDER_HEIGHT = 28.0f;
constexpr float TOGGLE_SIZE = 14.0f;
constexpr float LABEL_WIDTH = 36.0f;
constexpr float VALUE_WIDTH = 55.0f;
constexpr float PAD = 6.0f;

constexpr float PI = 3.141592653589793f;

static const char* UI_VERT_SRC = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform vec2 uScreenSize;
void main() {
    gl_Position = vec4(aPos.x / uScreenSize.x * 2.0 - 1.0,
                       1.0 - aPos.y / uScreenSize.y * 2.0, 0.0, 1.0);
}
)";

static const char* UI_FRAG_SRC = R"(
#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() {
    FragColor = uColor;
}
)";

struct TransformND {
    std::vector<float> angles;
    std::vector<float> translation;
    std::vector<bool> autoRotate;
    unsigned int dims;

    int planeCount() const { return dims * (dims - 1) / 2; }

    int planeIndex(int i, int j) const {
        int idx = 0;
        for (int a = 0; a < i; a++)
            idx += dims - a - 1;
        return idx + (j - i - 1);
    }
};

struct MouseState {
    double x, y;
    bool left;
    bool leftPressed;
    bool leftReleased;
};

struct Edge { int a, b; };

static void hslToRgb(float h, float s, float l, float& r, float& g, float& b) {
    auto hueToRgb = [](float p, float q, float t) {
        if (t < 0.0f) t += 1.0f;
        if (t > 1.0f) t -= 1.0f;
        if (t < 1.0f/6.0f) return p + (q - p) * 6.0f * t;
        if (t < 1.0f/2.0f) return q;
        if (t < 2.0f/3.0f) return p + (q - p) * (2.0f/3.0f - t) * 6.0f;
        return p;
    };
    if (s == 0.0f) {
        r = g = b = l;
    } else {
        float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        float p = 2.0f * l - q;
        r = hueToRgb(p, q, h + 1.0f/3.0f);
        g = hueToRgb(p, q, h);
        b = hueToRgb(p, q, h - 1.0f/3.0f);
    }
}

static void rotatePlane(float& a, float& b, float angle) {
    float c = cosf(angle), s = sinf(angle);
    float na = a * c - b * s;
    float nb = a * s + b * c;
    a = na;
    b = nb;
}

static void applyRotation(float* pos, const TransformND& t) {
    for (int i = 0; i < (int)t.dims; i++) {
        for (int j = i + 1; j < (int)t.dims; j++) {
            float angle = t.angles[t.planeIndex(i, j)];
            if (fabsf(angle) > 0.0001f)
                rotatePlane(pos[i], pos[j], angle);
        }
    }
}

static void projectTo3D(const float* in, float* out, int dims) {
    float tmp[64];
    for (int i = 0; i < dims; i++) tmp[i] = in[i];

    for (int d = dims - 1; d >= 3; d--) {
        float dist = (float)d;
        float depth = dist - tmp[d];
        float scale = depth > 0.001f ? dist / depth : 10.0f;
        for (int c = 0; c < d; c++)
            tmp[c] *= scale;
    }

    out[0] = tmp[0];
    out[1] = tmp[1];
    out[2] = tmp[2];
}

static std::string loadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to load file: " << path << std::endl;
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

static void checkShaderCompile(GLuint shader, const std::string& type) {
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << type << " shader error:\n" << log << std::endl;
    }
}

static void checkProgramLink(GLuint program) {
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        std::cerr << "Program link error:\n" << log << std::endl;
    }
}

static GLuint createShaderProgram(const std::string& vertPath, const std::string& fragPath) {
    std::string vertSrc = loadFile(vertPath);
    std::string fragSrc = loadFile(fragPath);
    if (vertSrc.empty() || fragSrc.empty()) return 0;

    const char* vertCStr = vertSrc.c_str();
    GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, &vertCStr, nullptr);
    glCompileShader(vertShader);
    checkShaderCompile(vertShader, "Vertex");

    const char* fragCStr = fragSrc.c_str();
    GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, &fragCStr, nullptr);
    glCompileShader(fragShader);
    checkShaderCompile(fragShader, "Fragment");

    GLuint program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);
    checkProgramLink(program);

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    return program;
}

static GLuint createShaderProgramFromSrc(const char* vertSrc, const char* fragSrc) {
    GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, &vertSrc, nullptr);
    glCompileShader(vertShader);
    checkShaderCompile(vertShader, "UI Vertex");

    GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, &fragSrc, nullptr);
    glCompileShader(fragShader);
    checkShaderCompile(fragShader, "UI Fragment");

    GLuint program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);
    checkProgramLink(program);

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    return program;
}

static void processInput(GLFWwindow* window, TransformND& t) {
    if (t.dims >= 1) {
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) t.translation[0] -= MOVE_SPEED;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) t.translation[0] += MOVE_SPEED;
    }
    if (t.dims >= 2) {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) t.translation[1] += MOVE_SPEED;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) t.translation[1] -= MOVE_SPEED;
    }
    if (t.dims >= 3) {
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) t.translation[2] += MOVE_SPEED;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) t.translation[2] -= MOVE_SPEED;
    }
    if (t.dims >= 4) {
        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) t.translation[3] += MOVE_SPEED;
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) t.translation[3] -= MOVE_SPEED;
    }
    if (t.dims >= 5) {
        if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) t.translation[4] += MOVE_SPEED;
        if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) t.translation[4] -= MOVE_SPEED;
    }
    if (t.dims >= 6) {
        if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) t.translation[5] += MOVE_SPEED;
        if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS) t.translation[5] -= MOVE_SPEED;
    }

    if (t.planeCount() >= 1) {
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) t.angles[0] += ROTATE_SPEED;
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) t.angles[0] -= ROTATE_SPEED;
    }
    if (t.planeCount() >= 2) {
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) t.angles[1] += ROTATE_SPEED;
        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) t.angles[1] -= ROTATE_SPEED;
    }
    if (t.planeCount() >= 3) {
        if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) t.angles[2] += ROTATE_SPEED;
        if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) t.angles[2] -= ROTATE_SPEED;
    }
    if (t.planeCount() >= 4) {
        if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) t.angles[3] += ROTATE_SPEED;
        if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) t.angles[3] -= ROTATE_SPEED;
    }
    if (t.planeCount() >= 5) {
        if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) t.angles[4] += ROTATE_SPEED;
        if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) t.angles[4] -= ROTATE_SPEED;
    }
    if (t.planeCount() >= 6) {
        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) t.angles[5] += ROTATE_SPEED;
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) t.angles[5] -= ROTATE_SPEED;
    }

    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        std::fill(t.angles.begin(), t.angles.end(), 0.0f);
        std::fill(t.translation.begin(), t.translation.end(), 0.0f);
        std::fill(t.autoRotate.begin(), t.autoRotate.end(), false);
    }
}

static std::vector<Edge> generateEdges(const float* vertices, unsigned int vertexCount,
                                        unsigned int dims, int fpv,
                                        const unsigned int* indices, unsigned int indexCount) {
    // Primary: coordinate-differencing (works for hypercubes)
    std::vector<Edge> edges;
    for (unsigned int a = 0; a < vertexCount; a++) {
        for (unsigned int b = a + 1; b < vertexCount; b++) {
            int diff = 0;
            for (unsigned int d = 0; d < dims; d++) {
                if (fabsf(vertices[a * fpv + d] - vertices[b * fpv + d]) > 0.001f)
                    diff++;
            }
            if (diff == 1)
                edges.push_back({(int)a, (int)b});
        }
    }

    if (!edges.empty())
        return edges;

    // Fallback: generate edges from triangle indices, deduplicated
    // This works for arbitrary meshes (may include quad diagonals)
    for (unsigned int t = 0; t < indexCount / 3; t++) {
        unsigned int i0 = indices[t * 3];
        unsigned int i1 = indices[t * 3 + 1];
        unsigned int i2 = indices[t * 3 + 2];

        auto addEdge = [&](unsigned int a, unsigned int b) {
            if (a == b) return;
            for (auto& e : edges)
                if ((e.a == (int)a && e.b == (int)b) || (e.a == (int)b && e.b == (int)a))
                    return;
            edges.push_back({(int)a, (int)b});
        };
        addEdge(i0, i1);
        addEdge(i1, i2);
        addEdge(i2, i0);
    }

    return edges;
}

static void drawRect(GLuint program, GLuint vao, GLuint vbo,
                     float x, float y, float w, float h,
                     float r, float g, float b, float a,
                     float screenW, float screenH) {
    float verts[8] = {
        x, y, x+w, y, x+w, y+h,
        x, y+h
    };
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glUseProgram(program);
    glUniform2f(glGetUniformLocation(program, "uScreenSize"), screenW, screenH);
    glUniform4f(glGetUniformLocation(program, "uColor"), r, g, b, a);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

static int drawTextAt(GLuint vao, GLuint vbo, GLuint ebo, GLuint program,
                      float x, float y, const char* text,
                      float screenW, float screenH,
                      const unsigned int* indices, int maxQuads) {
    char buf[2048];
    int nq = stb_easy_font_print(x, y, (char*)text, nullptr, buf, sizeof(buf));
    if (nq <= 0 || nq > maxQuads) return 0;
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, nq * 64, buf);
    glUseProgram(program);
    glUniform2f(glGetUniformLocation(program, "uScreenSize"), screenW, screenH);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, nq * 6, GL_UNSIGNED_INT, nullptr);
    return nq;
}

int main(int argc, char* argv[]) {
#ifndef _WIN32
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    char titleBuf[64];
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Ducky", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwSwapInterval(1);
    int fbW = 0, fbH = 0;
    glEnable(GL_DEPTH_TEST);

    const char* modelPath = argc > 1 ? argv[1] : "model.dky";
    Model model = LoadModel(modelPath);
    if (model.vertexCount == 0) {
        std::cerr << "Failed to load model\n";
        glfwTerminate();
        return -1;
    }
    std::cout << "Loaded " << model.dimensions << "D model: "
              << model.vertexCount << " vertices, " << model.indexCount << " indices" << std::endl;

    unsigned int dims = model.dimensions;
    int fpv = dims + 3;

    // Assign face colors via HSL
    {
        if (model.vertexCount > 0 && model.indexCount > 0) {
            unsigned int faces = model.indexCount / 3;
            const float goldenRatio = 0.618033988749895f;
            for (unsigned int f = 0; f < faces; f++) {
                float h = f * goldenRatio;
                h = h - floorf(h);
                float s = 0.85f;
                float l = 0.45f + ((f / 8) % 3) * 0.2f;
                float r, g, b;
                hslToRgb(h, s, l, r, g, b);
                for (int j = 0; j < 3; j++) {
                    int vi = model.indices[f * 3 + j];
                    model.vertices[vi * fpv + dims] = r;
                    model.vertices[vi * fpv + dims + 1] = g;
                    model.vertices[vi * fpv + dims + 2] = b;
                }
            }
        }
    }

    auto edges = generateEdges(model.vertices, model.vertexCount, dims, fpv,
                                model.indices, model.indexCount);
    std::cout << "Generated " << edges.size() << " edges" << std::endl;

    // === 3D scene rendering setup ===
    GLuint tessVAO, tessVBO, tessEBO;
    glGenVertexArrays(1, &tessVAO);
    glGenBuffers(1, &tessVBO);
    glGenBuffers(1, &tessEBO);

    glBindVertexArray(tessVAO);
    glBindBuffer(GL_ARRAY_BUFFER, tessVBO);
    glBufferData(GL_ARRAY_BUFFER, model.vertexCount * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tessEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.indexCount * sizeof(unsigned int), model.indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint tessProgram = createShaderProgram("shaders/tesseract.vert", "shaders/tesseract.frag");
    if (!tessProgram) { glfwTerminate(); return -1; }
    GLuint tessUAspect = glGetUniformLocation(tessProgram, "uAspect");

    // === Axes setup ===
    GLuint axesVAO, axesVBO;
    glGenVertexArrays(1, &axesVAO);
    glGenBuffers(1, &axesVBO);

    {
        float* axisData = new float[dims * 2 * 6];
        for (unsigned int d = 0; d < dims; d++) {
            float h = (float)d / (float)dims;
            float r, g, b;
            hslToRgb(h, 0.9f, 0.6f, r, g, b);
            axisData[d * 12 + 0] = 0; axisData[d * 12 + 1] = 0; axisData[d * 12 + 2] = 0;
            axisData[d * 12 + 3] = r; axisData[d * 12 + 4] = g; axisData[d * 12 + 5] = b;
            axisData[d * 12 + 6] = 0; axisData[d * 12 + 7] = 0; axisData[d * 12 + 8] = 0;
            axisData[d * 12 + 9] = r; axisData[d * 12 + 10] = g; axisData[d * 12 + 11] = b;

        }

        glBindVertexArray(axesVAO);
        glBindBuffer(GL_ARRAY_BUFFER, axesVBO);
        glBufferData(GL_ARRAY_BUFFER, dims * 2 * 6 * sizeof(float), axisData, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        delete[] axisData;
    }

    // Pre-computed axis colors (never change per frame)
    std::vector<float> axisR(dims), axisG(dims), axisB(dims);
    for (unsigned int d = 0; d < dims; d++) {
        float h = (float)d / (float)dims;
        hslToRgb(h, 0.9f, 0.6f, axisR[d], axisG[d], axisB[d]);
    }

    GLuint axesProgram = createShaderProgram("shaders/axes.vert", "shaders/axes.frag");
    if (!axesProgram) { glfwTerminate(); return -1; }
    GLuint axesUAspect = glGetUniformLocation(axesProgram, "uAspect");

    // === Wireframe edges setup ===
    GLuint edgeVAO, edgeVBO;
    glGenVertexArrays(1, &edgeVAO);
    glGenBuffers(1, &edgeVBO);

    glBindVertexArray(edgeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, edgeVBO);
    glBufferData(GL_ARRAY_BUFFER, edges.size() * 2 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    GLuint edgeProgram = createShaderProgram("shaders/edge.vert", "shaders/edge.frag");
    if (!edgeProgram) { glfwTerminate(); return -1; }
    GLuint edgeUAspect = glGetUniformLocation(edgeProgram, "uAspect");

    // === UI setup ===
    GLuint uiProgram = createShaderProgramFromSrc(UI_VERT_SRC, UI_FRAG_SRC);
    if (!uiProgram) { glfwTerminate(); return -1; }

    GLuint uiVAO, uiVBO;
    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    // Dynamic text for sliders
    const int TEXT_MAX_QUADS = 512;
    GLuint dtVAO, dtVBO, dtEBO;
    glGenVertexArrays(1, &dtVAO);
    glGenBuffers(1, &dtVBO);
    glGenBuffers(1, &dtEBO);
    glBindVertexArray(dtVAO);
    glBindBuffer(GL_ARRAY_BUFFER, dtVBO);
    glBufferData(GL_ARRAY_BUFFER, TEXT_MAX_QUADS * 64, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void*)12);
    glEnableVertexAttribArray(1);

    std::vector<unsigned int> dtIndices(TEXT_MAX_QUADS * 6);
    for (int i = 0; i < TEXT_MAX_QUADS; i++) {
        int base = i * 4;
        dtIndices[i * 6 + 0] = base;
        dtIndices[i * 6 + 1] = base + 1;
        dtIndices[i * 6 + 2] = base + 2;
        dtIndices[i * 6 + 3] = base + 1;
        dtIndices[i * 6 + 4] = base + 3;
        dtIndices[i * 6 + 5] = base + 2;
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dtEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, dtIndices.size() * sizeof(unsigned int), dtIndices.data(), GL_STATIC_DRAW);

    GLuint textProgram = createShaderProgram("shaders/text.vert", "shaders/text.frag");
    if (!textProgram) { glfwTerminate(); return -1; }

    // === HUD text (one-line hint at top-right area) ===
    std::string hintText = std::to_string(dims) + "D  |  Move:WASD=01 QE=2 ZX=3 TG=4 BH=5  |  Rot keys:12 34 56 78 90 -+  |  R=reset";
    char textBuffer[20000];
    int hudNumQuads = 0;

    GLuint textVAO, textVBO, textEBO;
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glGenBuffers(1, &textEBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, 20000, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void*)12);
    glEnableVertexAttribArray(1);

    std::vector<unsigned int> hudIndices(2000 * 6);
    for (int i = 0; i < 2000; i++) {
        int base = i * 4;
        hudIndices[i * 6 + 0] = base;
        hudIndices[i * 6 + 1] = base + 1;
        hudIndices[i * 6 + 2] = base + 2;
        hudIndices[i * 6 + 3] = base + 1;
        hudIndices[i * 6 + 4] = base + 3;
        hudIndices[i * 6 + 5] = base + 2;
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, textEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, hudIndices.size() * sizeof(unsigned int), hudIndices.data(), GL_STATIC_DRAW);

    GLint textScreenSize = glGetUniformLocation(textProgram, "uScreenSize");

    TransformND transform;
    transform.dims = dims;
    transform.angles.resize(transform.planeCount(), 0.0f);
    transform.autoRotate.resize(transform.planeCount(), false);
    transform.translation.resize(dims, 0.0f);

    std::vector<float> projectedVerts(model.vertexCount * 6);

    // UI state
    MouseState mouse = {};
    int dragSlider = -1;
    int hoverSlider = -1;

    // Reusable buffers (persistent across frames)
    std::vector<float> axis3D(dims * 2 * 6);
    std::vector<float> edge3D(edges.size() * 2 * 3);
    struct TriDepth { int idx; float depth; };
    std::vector<TriDepth> triDepths;
    triDepths.reserve(model.indexCount / 3 + 1);
    std::vector<unsigned int> sorted;
    sorted.reserve(model.indexCount + 3);

    double lastTime = glfwGetTime();

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        // Mouse state
        {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            bool leftNow = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            mouse.leftPressed = leftNow && !mouse.left;
            mouse.leftReleased = !leftNow && mouse.left;
            mouse.left = leftNow;
            mouse.x = mx;
            mouse.y = my;
        }

        processInput(window, transform);

        // Per-plane auto-rotation (frame-rate independent)
        {
            double now = glfwGetTime();
            float dt = (float)(now - lastTime);
            lastTime = now;
            for (int i = 0; i < transform.planeCount(); i++) {
                if (transform.autoRotate[i])
                    transform.angles[i] += dt * 0.5f * (1 + (i % 3));
            }
        }

        // Wrap all angles to [-PI, PI)
        for (int i = 0; i < transform.planeCount(); i++)
            transform.angles[i] = fmodf(transform.angles[i] + PI, 2.0f * PI) - PI;

        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        float aspect = (float)fbW / (float)fbH;

        snprintf(titleBuf, sizeof(titleBuf), "Ducky - %uD (%u verts, %zu edges)",
                 dims, model.vertexCount, edges.size());
        glfwSetWindowTitle(window, titleBuf);

        // Process slider mouse interaction
        {
            int nSliders = transform.planeCount();
            float panelTop = 10.0f;
            float panelLeft = 10.0f;
            float titleH = 24.0f;
            float toggleX = panelLeft + PAD;
            float labelX = toggleX + TOGGLE_SIZE + PAD;
            float trackX = labelX + LABEL_WIDTH + PAD;
            float trackW = PANEL_WIDTH - (trackX - panelLeft) - VALUE_WIDTH - PAD;

            // Toggle click
            if (mouse.leftPressed) {
                for (int i = 0; i < nSliders; i++) {
                    float rowY = panelTop + PAD + titleH + PAD + i * SLIDER_HEIGHT;
                    float tglY = rowY + (SLIDER_HEIGHT - TOGGLE_SIZE) / 2;
                    if (mouse.x >= toggleX && mouse.x <= toggleX + TOGGLE_SIZE &&
                        mouse.y >= tglY && mouse.y <= tglY + TOGGLE_SIZE) {
                        transform.autoRotate[i] = !transform.autoRotate[i];
                        break;
                    }
                }
            }

            if (mouse.leftPressed) {
                dragSlider = -1;
                for (int i = 0; i < nSliders; i++) {
                    float rowY = panelTop + PAD + titleH + PAD + i * SLIDER_HEIGHT;
                    if (mouse.x >= trackX && mouse.x <= trackX + trackW &&
                        mouse.y >= rowY && mouse.y <= rowY + SLIDER_HEIGHT) {
                        dragSlider = i;
                        float t = (float)((mouse.x - trackX) / trackW);
                        t = std::max(0.0f, std::min(1.0f, t));
                        transform.angles[i] = -PI + t * (2.0f * PI);
                        break;
                    }
                }
            }

            if (dragSlider >= 0 && mouse.left) {
                float t = (float)((mouse.x - trackX) / trackW);
                t = std::max(0.0f, std::min(1.0f, t));
                transform.angles[dragSlider] = -PI + t * (2.0f * PI);
            }

            if (mouse.leftReleased)
                dragSlider = -1;

            hoverSlider = -1;
            for (int i = 0; i < nSliders; i++) {
                float rowY = panelTop + PAD + titleH + PAD + i * SLIDER_HEIGHT;
                if (mouse.x >= trackX && mouse.x <= trackX + trackW &&
                    mouse.y >= rowY && mouse.y <= rowY + SLIDER_HEIGHT)
                    hoverSlider = i;
            }
        }

        // Process vertices
        for (unsigned int i = 0; i < model.vertexCount; i++) {
            float pos[16];
            for (unsigned int d = 0; d < dims; d++)
                pos[d] = model.vertices[i * fpv + d];
            for (unsigned int d = 0; d < dims; d++)
                pos[d] += transform.translation[d];
            applyRotation(pos, transform);
            projectTo3D(pos, &projectedVerts[i * 6], dims);
            projectedVerts[i * 6 + 3] = model.vertices[i * fpv + dims];
            projectedVerts[i * 6 + 4] = model.vertices[i * fpv + dims + 1];
            projectedVerts[i * 6 + 5] = model.vertices[i * fpv + dims + 2];
        }

        // Depth sort (reused buffers)
        {
            unsigned int numTri = model.indexCount / 3;
            triDepths.resize(numTri);
            for (unsigned int i = 0; i < numTri; i++) {
                float sum = 0;
                for (int j = 0; j < 3; j++) {
                    int vi = model.indices[i * 3 + j];
                    sum += projectedVerts[vi * 6 + 2];
                }
                triDepths[i] = {(int)i, sum / 3.0f};
            }
            std::sort(triDepths.begin(), triDepths.end(),
                      [](auto& a, auto& b) { return a.depth > b.depth; });

            sorted.resize(model.indexCount);
            for (unsigned int i = 0; i < numTri; i++) {
                int t = triDepths[i].idx;
                sorted[i * 3 + 0] = model.indices[t * 3 + 0];
                sorted[i * 3 + 1] = model.indices[t * 3 + 1];
                sorted[i * 3 + 2] = model.indices[t * 3 + 2];
            }
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tessEBO);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, model.indexCount * sizeof(unsigned int), sorted.data());
        }

        // Upload projected vertices
        glBindBuffer(GL_ARRAY_BUFFER, tessVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, model.vertexCount * 6 * sizeof(float), projectedVerts.data());

        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Draw tesseract faces
        glUseProgram(tessProgram);
        glUniform1f(tessUAspect, aspect);
        glBindVertexArray(tessVAO);
        glDrawElements(GL_TRIANGLES, model.indexCount, GL_UNSIGNED_INT, nullptr);

        // Draw axes (reused buffer, pre-computed colors)
        {
            for (unsigned int d = 0; d < dims; d++) {
                float origin[64] = {0};
                float tip[64] = {0};
                tip[d] = AXIS_LENGTH;
                applyRotation(origin, transform);
                applyRotation(tip, transform);
                projectTo3D(origin, &axis3D[d * 12], dims);
                projectTo3D(tip, &axis3D[d * 12 + 6], dims);
                axis3D[d * 12 + 3] = axisR[d]; axis3D[d * 12 + 4] = axisG[d]; axis3D[d * 12 + 5] = axisB[d];
                axis3D[d * 12 + 9] = axisR[d]; axis3D[d * 12 + 10] = axisG[d]; axis3D[d * 12 + 11] = axisB[d];
            }
            glBindBuffer(GL_ARRAY_BUFFER, axesVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, dims * 2 * 6 * sizeof(float), axis3D.data());
            glUseProgram(axesProgram);
            glUniform1f(axesUAspect, aspect);
            glBindVertexArray(axesVAO);
            glDrawArrays(GL_LINES, 0, dims * 2);
        }

        // Draw wireframe edges (reused buffer)
        {
            for (size_t i = 0; i < edges.size(); i++) {
                edge3D[i * 6 + 0] = projectedVerts[edges[i].a * 6];
                edge3D[i * 6 + 1] = projectedVerts[edges[i].a * 6 + 1];
                edge3D[i * 6 + 2] = projectedVerts[edges[i].a * 6 + 2];
                edge3D[i * 6 + 3] = projectedVerts[edges[i].b * 6];
                edge3D[i * 6 + 4] = projectedVerts[edges[i].b * 6 + 1];
                edge3D[i * 6 + 5] = projectedVerts[edges[i].b * 6 + 2];
            }
            glBindBuffer(GL_ARRAY_BUFFER, edgeVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, edges.size() * 2 * 3 * sizeof(float), edge3D.data());

            glDisable(GL_DEPTH_TEST);
            glUseProgram(edgeProgram);
            glUniform1f(edgeUAspect, aspect);
            glBindVertexArray(edgeVAO);
            glDrawArrays(GL_LINES, 0, edges.size() * 2);
        }

        // === UI overlay (no depth test) ===
        glDisable(GL_DEPTH_TEST);

        // Draw slider panel background
        int nSliders = transform.planeCount();
        float panelTop = 10.0f;
        float panelLeft = 10.0f;
        float titleH = 24.0f;
        float panelH = PAD * 2 + titleH + nSliders * SLIDER_HEIGHT + 10.0f;
        drawRect(uiProgram, uiVAO, uiVBO,
                 panelLeft, panelTop, PANEL_WIDTH, panelH,
                 0.12f, 0.12f, 0.18f, 0.92f, (float)fbW, (float)fbH);

        // Draw border
        drawRect(uiProgram, uiVAO, uiVBO,
                 panelLeft, panelTop, PANEL_WIDTH, 1.0f,
                 0.3f, 0.3f, 0.5f, 0.8f, (float)fbW, (float)fbH);
        drawRect(uiProgram, uiVAO, uiVBO,
                 panelLeft, panelTop + panelH - 1, PANEL_WIDTH, 1.0f,
                 0.3f, 0.3f, 0.5f, 0.8f, (float)fbW, (float)fbH);

        // Title
        char titleStr[64];
        snprintf(titleStr, sizeof(titleStr), "%uD Rotations", dims);

        float toggleX = panelLeft + PAD;
        float labelX = toggleX + TOGGLE_SIZE + PAD;
        float trackX = labelX + LABEL_WIDTH + PAD;
        float trackW = PANEL_WIDTH - (trackX - panelLeft) - VALUE_WIDTH - PAD;

        // Draw each slider
        char label[16];
        char valueStr[16];
        for (int i = 0; i < nSliders; i++) {
            float rowY = panelTop + PAD + titleH + PAD + i * SLIDER_HEIGHT;

            // Toggle button
            bool autoOn = transform.autoRotate[i];
            drawRect(uiProgram, uiVAO, uiVBO,
                     toggleX, rowY + (SLIDER_HEIGHT - TOGGLE_SIZE) / 2,
                     TOGGLE_SIZE, TOGGLE_SIZE,
                     autoOn ? 0.2f : 0.15f, autoOn ? 0.7f : 0.15f, autoOn ? 0.2f : 0.2f, 0.9f,
                     (float)fbW, (float)fbH);
            float tglY = rowY + (SLIDER_HEIGHT - TOGGLE_SIZE) / 2;
            char toggleLabel[2] = {autoOn ? 'A' : 'M', '\0'};
            drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
                       toggleX + 4, tglY + 3, toggleLabel,
                       (float)fbW, (float)fbH, dtIndices.data(), TEXT_MAX_QUADS);

            // Label
            int pi = 0;
            int ai = -1, aj = -1;
            for (int a = 0; a < (int)dims && pi <= i; a++)
                for (int b = a + 1; b < (int)dims && pi <= i; b++, pi++)
                    if (pi == i) { ai = a; aj = b; }
            snprintf(label, sizeof(label), "(%d,%d)", ai, aj);

            // Track background
            drawRect(uiProgram, uiVAO, uiVBO,
                     trackX, rowY, trackW, SLIDER_HEIGHT,
                     0.2f, 0.2f, 0.3f, 0.9f, (float)fbW, (float)fbH);

            // Track fill (left portion up to value)
            float val = transform.angles[i];
            float fillFrac = (val + PI) / (2.0f * PI);
            fillFrac = std::max(0.0f, std::min(1.0f, fillFrac));
            drawRect(uiProgram, uiVAO, uiVBO,
                     trackX, rowY, trackW * fillFrac, SLIDER_HEIGHT,
                     0.35f, 0.5f, 0.9f, 0.8f, (float)fbW, (float)fbH);

            // Value string
            snprintf(valueStr, sizeof(valueStr), "%.2f", val);
            drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
                       trackX + trackW + PAD, rowY, valueStr,
                       (float)fbW, (float)fbH, dtIndices.data(), TEXT_MAX_QUADS);
            drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
                       labelX, rowY, label,
                       (float)fbW, (float)fbH, dtIndices.data(), TEXT_MAX_QUADS);
        }

        // Draw title
        drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
                   panelLeft + PAD, panelTop + PAD, titleStr,
                   (float)fbW, (float)fbH, dtIndices.data(), TEXT_MAX_QUADS);

        // HUD text (top-right area)
        {
            hudNumQuads = stb_easy_font_print(PANEL_WIDTH + 20, 12, (char*)hintText.c_str(),
                                               nullptr, textBuffer, sizeof(textBuffer));
            glUseProgram(textProgram);
            glUniform2f(textScreenSize, (float)fbW, (float)fbH);
            glBindBuffer(GL_ARRAY_BUFFER, textVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, hudNumQuads * 64, textBuffer);
            glBindVertexArray(textVAO);
            glDrawElements(GL_TRIANGLES, hudNumQuads * 6, GL_UNSIGNED_INT, nullptr);
        }

        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    delete[] model.vertices;
    delete[] model.indices;
    glDeleteVertexArrays(1, &tessVAO);
    glDeleteBuffers(1, &tessVBO);
    glDeleteBuffers(1, &tessEBO);
    glDeleteVertexArrays(1, &axesVAO);
    glDeleteBuffers(1, &axesVBO);
    glDeleteVertexArrays(1, &edgeVAO);
    glDeleteBuffers(1, &edgeVBO);
    glDeleteProgram(edgeProgram);
    glDeleteVertexArrays(1, &textVAO);
    glDeleteBuffers(1, &textVBO);
    glDeleteBuffers(1, &textEBO);
    glDeleteVertexArrays(1, &dtVAO);
    glDeleteBuffers(1, &dtVBO);
    glDeleteBuffers(1, &dtEBO);
    glDeleteVertexArrays(1, &uiVAO);
    glDeleteBuffers(1, &uiVBO);
    glDeleteProgram(tessProgram);
    glDeleteProgram(axesProgram);
    glDeleteProgram(textProgram);
    glDeleteProgram(uiProgram);

    glfwTerminate();
    return 0;
}
