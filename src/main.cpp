#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <cstring>
#include <set>
#include <map>
#include <ctime>

#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

#include "main.hpp"

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

constexpr int WINDOW_WIDTH = 1920;
constexpr int WINDOW_HEIGHT = 1920;
constexpr float ROTATE_SPEED = 0.02f;
constexpr float AXIS_LENGTH = 1.5f;

constexpr float PANEL_WIDTH = 290.0f;
constexpr float SLIDER_HEIGHT = 28.0f;
constexpr float TOGGLE_SIZE = 14.0f;
constexpr float LABEL_WIDTH = 36.0f;
constexpr float VALUE_WIDTH = 55.0f;
constexpr float PAD = 6.0f;

constexpr float PI = 3.141592653589793f;
constexpr int EDGE_SUBDIV = 32;

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
    unsigned int dims = 0;

    int planeCount() const { return dims * (dims - 1) / 2; }

    int planeIndex(int i, int j) const {
        int idx = 0;
        for (int a = 0; a < i; a++)
            idx += dims - a - 1;
        return idx + (j - i - 1);
    }
};

struct MouseState {
    double x = 0, y = 0;
    double lastX = 0, lastY = 0;
    bool left = false;
    bool leftPressed = false;
    bool leftReleased = false;
    bool right = false;
    bool rightPressed = false;
    bool rightReleased = false;
    bool moved = false;
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

static void projectPerspective(const float* in, float* out, int dims, float focalLength) {
    float* tmp = (float*)alloca(dims * sizeof(float));
    for (int i = 0; i < dims; i++) tmp[i] = in[i];

    for (int d = dims - 1; d >= 3; d--) {
        float dist = (float)d * focalLength;
        float depth = dist - tmp[d];
        float scale = depth > 0.001f ? dist / depth : 10.0f;
        for (int c = 0; c < d; c++)
            tmp[c] *= scale;
    }

    out[0] = tmp[0];
    out[1] = tmp[1];
    out[2] = tmp[2];
}

static void projectOrthographic(const float* in, float* out, int dims) {
    out[0] = in[0];
    out[1] = in[1];
    out[2] = dims > 2 ? in[2] : 0.0f;
}

static void projectStereographic(const float* in, float* out, int dims, float focalLength) {
    float tmp[128];
    for (int i = 0; i < dims; i++) tmp[i] = in[i];

    // Project dims 5+ down to 4D using perspective
    for (int d = dims - 1; d >= 4; d--) {
        float dist = (float)d * focalLength;
        float depth = dist - tmp[d];
        float s = depth > 0.001f ? dist / depth : 10.0f;
        for (int c = 0; c < d; c++)
            tmp[c] *= s;
    }

    // Stereographic projection from 4D to 3D from north pole (0,0,0,R) to plane w=0
    float x = tmp[0], y = tmp[1], z = tmp[2], w = dims > 3 ? tmp[3] : 0.0f;
    float radius = sqrtf(x*x + y*y + z*z + w*w);
    float denom = radius - w;
    if (fabsf(denom) < 0.001f) denom = 0.001f;
    float s = radius / denom;
    out[0] = x * s;
    out[1] = y * s;
    out[2] = z * s;
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
    int planeKeysPos[] = {
        GLFW_KEY_1, GLFW_KEY_3, GLFW_KEY_5, GLFW_KEY_7,
        GLFW_KEY_9, GLFW_KEY_MINUS
    };
    int planeKeysNeg[] = {
        GLFW_KEY_2, GLFW_KEY_4, GLFW_KEY_6, GLFW_KEY_8,
        GLFW_KEY_0, GLFW_KEY_EQUAL
    };

    for (int i = 0; i < 6 && i < t.planeCount(); i++) {
        if (glfwGetKey(window, planeKeysPos[i]) == GLFW_PRESS)
            t.angles[i] += ROTATE_SPEED;
        if (glfwGetKey(window, planeKeysNeg[i]) == GLFW_PRESS)
            t.angles[i] -= ROTATE_SPEED;
    }

    // Additional planes for 6D+ (Ctrl+letter pairs)
    int extraKeysPos[] = {
        GLFW_KEY_Q, GLFW_KEY_E, GLFW_KEY_T, GLFW_KEY_U,
        GLFW_KEY_O, GLFW_KEY_LEFT_BRACKET
    };
    int extraKeysNeg[] = {
        GLFW_KEY_W, GLFW_KEY_R, GLFW_KEY_Y, GLFW_KEY_I,
        GLFW_KEY_P, GLFW_KEY_RIGHT_BRACKET
    };
    bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

    for (int i = 0; i < 6 && (6 + i) < t.planeCount(); i++) {
        if (ctrl && glfwGetKey(window, extraKeysPos[i]) == GLFW_PRESS)
            t.angles[6 + i] += ROTATE_SPEED;
        if (ctrl && glfwGetKey(window, extraKeysNeg[i]) == GLFW_PRESS)
            t.angles[6 + i] -= ROTATE_SPEED;
    }

    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        std::fill(t.angles.begin(), t.angles.end(), 0.0f);
        std::fill(t.translation.begin(), t.translation.end(), 0.0f);
    }

}

static std::vector<Edge> generateEdges(const float* vertices, unsigned int vertexCount,
                                        unsigned int dims, int fpv,
                                        const unsigned int* indices, unsigned int indexCount) {
    // Build position → canonical index (first occurrence)
    std::vector<int> canonical(vertexCount);
    std::vector<int> reverseCanonical; // canonical index → any actual vertex index
    for (unsigned int i = 0; i < vertexCount; i++) {
        int found = -1;
        for (size_t j = 0; j < reverseCanonical.size(); j++) {
            int ci = reverseCanonical[j];
            bool same = true;
            for (unsigned int d = 0; d < dims; d++) {
                if (fabsf(vertices[i * fpv + d] - vertices[ci * fpv + d]) > 0.001f) {
                    same = false;
                    break;
                }
            }
            if (same) { found = (int)j; break; }
        }
        if (found < 0) {
            found = (int)reverseCanonical.size();
            reverseCanonical.push_back((int)i);
        }
        canonical[i] = found;
    }
    unsigned int uniqueCount = (unsigned int)reverseCanonical.size();

    // Brute-force edge detection on unique vertices
    std::set<std::pair<int,int>> edgeSet;
    for (unsigned int a = 0; a < uniqueCount; a++) {
        int ai = reverseCanonical[a];
        for (unsigned int b = a + 1; b < uniqueCount; b++) {
            int bi = reverseCanonical[b];
            int diff = 0;
            for (unsigned int d = 0; d < dims; d++) {
                if (fabsf(vertices[ai * fpv + d] - vertices[bi * fpv + d]) > 0.001f)
                    diff++;
            }
            if (diff == 1)
                edgeSet.insert({(int)a, (int)b});
        }
    }

    if (!edgeSet.empty()) {
        std::vector<Edge> edges;
        edges.reserve(edgeSet.size());
        for (auto& e : edgeSet)
            edges.push_back({reverseCanonical[e.first], reverseCanonical[e.second]});
        return edges;
    }

    // Fallback: count triangle edge occurrences per unique vertex
    std::map<std::pair<int,int>, int> edgeCounts;
    for (unsigned int t = 0; t < indexCount / 3; t++) {
        int tri[3] = {
            canonical[indices[t * 3]],
            canonical[indices[t * 3 + 1]],
            canonical[indices[t * 3 + 2]]
        };
        for (int k = 0; k < 3; k++) {
            int a = tri[k], b = tri[(k + 1) % 3];
            if (a == b) continue;
            if (a > b) std::swap(a, b);
            edgeCounts[{a, b}]++;
        }
    }

    std::vector<Edge> edges;
    edges.reserve(edgeCounts.size());
    for (auto& ec : edgeCounts)
        edges.push_back({reverseCanonical[ec.first.first], reverseCanonical[ec.first.second]});
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
    static std::vector<char> buf(2048);
    int nq = stb_easy_font_print(x, y, (char*)text, nullptr, buf.data(), (int)buf.size());
    if (nq <= 0 || nq > maxQuads) return 0;
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, nq * 64, buf.data());
    glUseProgram(program);
    glUniform2f(glGetUniformLocation(program, "uScreenSize"), screenW, screenH);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, nq * 6, GL_UNSIGNED_INT, nullptr);
    return nq;
}

static void writeTGA(const char* path, int w, int h, unsigned char* data) {
    FILE* f = fopen(path, "wb");
    if (!f) return;
    unsigned char header[18] = {0};
    header[2] = 2;
    header[12] = w & 0xFF;
    header[13] = (w >> 8) & 0xFF;
    header[14] = h & 0xFF;
    header[15] = (h >> 8) & 0xFF;
    header[16] = 24;
    fwrite(header, 1, 18, f);
    for (int y = h - 1; y >= 0; y--)
        fwrite(data + y * w * 3, 1, w * 3, f);
    fclose(f);
}

static void saveState(const char* path, const TransformND& t) {
    FILE* f = fopen(path, "w");
    if (!f) { std::cerr << "Failed to save state\n"; return; }
    fprintf(f, "%u\n", t.dims);
    for (float a : t.angles) fprintf(f, "%.8f ", a);
    fprintf(f, "\n");
    for (float tr : t.translation) fprintf(f, "%.8f ", tr);
    fprintf(f, "\n");
    fclose(f);
    std::cout << "Saved state to " << path << std::endl;
}

static bool loadState(const char* path, TransformND& t) {
    FILE* f = fopen(path, "r");
    if (!f) { std::cerr << "Failed to load state\n"; return false; }
    unsigned int dims;
    if (fscanf(f, "%u", &dims) != 1 || dims != t.dims) {
        fclose(f);
        std::cerr << "State file dimension mismatch\n";
        return false;
    }
    std::vector<float> newAngles(t.planeCount());
    std::vector<float> newTranslations(t.dims);
    bool ok = true;
    for (int i = 0; i < t.planeCount(); i++) {
        if (fscanf(f, "%f", &newAngles[i]) != 1) { ok = false; break; }
    }
    if (ok) {
        for (unsigned int i = 0; i < t.dims; i++) {
            if (fscanf(f, "%f", &newTranslations[i]) != 1) { ok = false; break; }
        }
    }
    fclose(f);
    if (!ok) {
        std::cerr << "Failed to read full state from " << path << std::endl;
        return false;
    }
    t.angles = newAngles;
    t.translation = newTranslations;
    std::cout << "Loaded state from " << path << std::endl;
    return true;
}

static void assignFaceColors(Model& model, int colorScheme) {
    if (model.vertexCount == 0 || model.indexCount == 0) return;
    unsigned int dims = model.dimensions;
    int fpv = dims + 3;
    unsigned int faces = model.indexCount / 3;
    const float goldenRatio = 0.618033988749895f;

    for (unsigned int f = 0; f < faces; f++) {
        float h;
        switch (colorScheme) {
            case 1: // rainbow
                h = (float)f / (float)faces;
                break;
            case 2: // monochrome blue
                h = 0.6f;
                break;
            case 3: // warm
                h = 0.0f + (float)(f % 10) * 0.05f;
                break;
            default: // golden ratio
                h = f * goldenRatio;
                h = h - floorf(h);
                break;
        }
        float s = (colorScheme == 2) ? 0.4f : 0.85f;
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

    char titleBuf[128];
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

    // === State variables ===
    bool showPerformance = false;
    bool wireframeOnly = false;
    int colorScheme = 0; // 0=Model, 1=Golden, 2=Rainbow, 3=Mono, 4=Warm
    const char* colorSchemeNames[5] = {"Model", "Golden", "Rainbow", "Mono", "Warm"};
    int rotPreset = 1;
    float focalLength = 1.0f;
    int renderMode = 0; // 0=Perspective, 1=Stereographic, 2=Orthographic
    const char* renderModeNames[] = {"Perspective", "Stereographic", "Orthographic"};
    int fullscreenW = 0, fullscreenH = 0;
    int windowedX = 0, windowedY = 0, windowedW = WINDOW_WIDTH, windowedH = WINDOW_HEIGHT;
    bool isFullscreen = false;
    bool orbitMode = false;
    bool transparent = false;
    float modelAlpha = 0.35f;

    const char* modelPath = argc > 1 ? argv[1] : "model.dky";
    Model model = LoadModel(modelPath);
    if (model.vertexCount == 0) {
        std::cerr << "Failed to load model: " << modelPath << "\n";
        glfwTerminate();
        return -1;
    }
    std::cout << "Loaded " << model.dimensions << "D model: "
              << model.vertexCount << " vertices, " << model.indexCount << " indices" << std::endl;

    unsigned int dims = model.dimensions;
    int fpv = dims + 3;
    std::vector<float> modelVertsBackup = model.vertices;

    auto applyColorScheme = [&](int scheme) {
        if (scheme == 0) {
            model.vertices = modelVertsBackup;
        } else {
            assignFaceColors(model, scheme - 1);
        }
    };

    auto edges = generateEdges(model.vertices.data(), model.vertexCount, dims, fpv,
                                model.indices.data(), model.indexCount);
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
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.indexCount * sizeof(unsigned int), model.indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint tessProgram = createShaderProgram("shaders/tesseract.vert", "shaders/tesseract.frag");
    if (!tessProgram) { glfwTerminate(); return -1; }
    GLuint tessUAspect = glGetUniformLocation(tessProgram, "uAspect");
    GLuint tessUDist3D = glGetUniformLocation(tessProgram, "uDist3D");
    GLuint tessUAlpha = glGetUniformLocation(tessProgram, "uAlpha");

    // === Axes setup ===
    GLuint axesVAO, axesVBO;
    glGenVertexArrays(1, &axesVAO);
    glGenBuffers(1, &axesVBO);

    {
        std::vector<float> axisData(dims * 2 * 6);
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
        glBufferData(GL_ARRAY_BUFFER, axisData.size() * sizeof(float), axisData.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    std::vector<float> axisR(dims), axisG(dims), axisB(dims);
    for (unsigned int d = 0; d < dims; d++) {
        float h = (float)d / (float)dims;
        hslToRgb(h, 0.9f, 0.6f, axisR[d], axisG[d], axisB[d]);
    }

    GLuint axesProgram = createShaderProgram("shaders/axes.vert", "shaders/axes.frag");
    if (!axesProgram) { glfwTerminate(); return -1; }
    GLuint axesUAspect = glGetUniformLocation(axesProgram, "uAspect");
    GLuint axesUDist3D = glGetUniformLocation(axesProgram, "uDist3D");

    // === Wireframe edges setup ===
    GLuint edgeVAO, edgeVBO;
    glGenVertexArrays(1, &edgeVAO);
    glGenBuffers(1, &edgeVBO);

    glBindVertexArray(edgeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, edgeVBO);
    glBufferData(GL_ARRAY_BUFFER, edges.size() * (EDGE_SUBDIV + 1) * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    // === Subdivided face buffers (for stereographic mode) ===
    int maxVertsPerTri = (EDGE_SUBDIV + 1) * (EDGE_SUBDIV + 2) / 2;
    int maxTrisPerTri = EDGE_SUBDIV * EDGE_SUBDIV;
    GLuint subVAO, subVBO, subEBO;
    glGenVertexArrays(1, &subVAO);
    glGenBuffers(1, &subVBO);
    glGenBuffers(1, &subEBO);
    glBindVertexArray(subVAO);
    glBindBuffer(GL_ARRAY_BUFFER, subVBO);
    glBufferData(GL_ARRAY_BUFFER, (model.indexCount / 3) * maxVertsPerTri * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (model.indexCount / 3) * maxTrisPerTri * 3 * sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint edgeProgram = createShaderProgram("shaders/edge.vert", "shaders/edge.frag");
    if (!edgeProgram) { glfwTerminate(); return -1; }
    GLuint edgeUAspect = glGetUniformLocation(edgeProgram, "uAspect");
    GLuint edgeUDist3D = glGetUniformLocation(edgeProgram, "uDist3D");

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

    char hintText[256];
    snprintf(hintText, sizeof(hintText), "%uD  |  Rot:1-0,-=  |  E=wireframe V=preset C=color A=autorotate T=transparency []=focal M=render R=reset  |  F11=FS F12=shot F1=perf  |  Right panel has all controls", dims);

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
    transform.autoRotate.resize(transform.planeCount(), true);
    transform.translation.resize(dims, 0.0f);

    std::vector<float> projectedVerts(model.vertexCount * 6);

    // UI state
    MouseState mouse = {};
    int dragSlider = -1;
    int hoverSlider = -1;
    int clickedButton = -1;

    enum ButtonId {
        BTN_RESET, BTN_WIREFRAME, BTN_COLOR, BTN_PRESET,
        BTN_FOCAL_DOWN, BTN_FOCAL_UP, BTN_MODE,
        BTN_FS, BTN_SAVE, BTN_LOAD, BTN_SHOT,
        BTN_COUNT
    };

    // Reusable buffers
    std::vector<float> axis3D(dims * 2 * 6);
    std::vector<float> edge3D(edges.size() * (EDGE_SUBDIV + 1) * 3);
    struct TriDepth { int idx; float depth; };
    std::vector<TriDepth> triDepths;
    triDepths.reserve(model.indexCount / 3 + 1);
    std::vector<unsigned int> sorted;
    sorted.reserve(model.indexCount + 3);
    std::vector<float> subVerts;
    std::vector<unsigned int> subIdx;
    std::vector<TriDepth> subDepths;

    // Performance tracking
    double perfLastTime = glfwGetTime();
    int perfFrameCount = 0;
    float perfFps = 0.0f;

    double lastTime = glfwGetTime();

    // Reusable HUD text buffer
    std::vector<char> textBuffer(20000);

    auto takeScreenshot = [&]() {
        glfwGetFramebufferSize(window, &fbW, &fbH);
        std::vector<unsigned char> pixels(fbW * fbH * 3);
        glReadPixels(0, 0, fbW, fbH, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        char screenshotPath[64];
        time_t now = time(nullptr);
        struct tm* tmNow = localtime(&now);
        snprintf(screenshotPath, sizeof(screenshotPath), "screenshot_%04d%02d%02d_%02d%02d%02d.tga",
                 tmNow->tm_year + 1900, tmNow->tm_mon + 1, tmNow->tm_mday,
                 tmNow->tm_hour, tmNow->tm_min, tmNow->tm_sec);
        writeTGA(screenshotPath, fbW, fbH, pixels.data());
        std::cout << "Screenshot saved: " << screenshotPath << std::endl;
    };

    auto toggleFullscreen = [&]() {
        isFullscreen = !isFullscreen;
        if (isFullscreen) {
            glfwGetWindowPos(window, &windowedX, &windowedY);
            glfwGetWindowSize(window, &windowedW, &windowedH);
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            fullscreenW = mode->width;
            fullscreenH = mode->height;
            glfwSetWindowMonitor(window, monitor, 0, 0, fullscreenW, fullscreenH, mode->refreshRate);
        } else {
            GLFWmonitor* mon = glfwGetPrimaryMonitor();
            int monX, monY, monW, monH;
            glfwGetMonitorWorkarea(mon, &monX, &monY, &monW, &monH);
            int restoreX = std::max(monX, std::min(windowedX, monX + monW - 100));
            int restoreY = std::max(monY, std::min(windowedY, monY + monH - 100));
            int restoreW = std::max(100, std::min(windowedW, monW));
            int restoreH = std::max(100, std::min(windowedH, monH));
            glfwSetWindowMonitor(window, nullptr, restoreX, restoreY, restoreW, restoreH, 0);
        }
    };

    while (!glfwWindowShouldClose(window)) {
        // ── Input ──
        // Mouse state
        {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            bool leftNow = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            bool rightNow = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
            mouse.leftPressed = leftNow && !mouse.left;
            mouse.leftReleased = !leftNow && mouse.left;
            mouse.left = leftNow;
            mouse.rightPressed = rightNow && !mouse.right;
            mouse.rightReleased = !rightNow && mouse.right;
            mouse.right = rightNow;
            mouse.moved = (mx != mouse.x || my != mouse.y);
            mouse.lastX = mouse.x;
            mouse.lastY = mouse.y;
            mouse.x = mx;
            mouse.y = my;
        }

        processInput(window, transform);

        // ── Keyboard shortcuts ──
        // Save/Load state
        {
            bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
            static bool sPrev = false, lPrev = false;
            bool sNow = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
            bool lNow = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
            if (ctrl && sNow && !sPrev) saveState("ducky_state.txt", transform);
            if (ctrl && lNow && !lPrev) loadState("ducky_state.txt", transform);
            sPrev = sNow;
            lPrev = lNow;
        }

        // Fullscreen toggle (F11)
        {
            static bool f11Prev = false;
            bool f11Now = glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS;
            if (f11Now && !f11Prev) toggleFullscreen();
            f11Prev = f11Now;
        }

        // Screenshot (F12)
        {
            static bool f12Prev = false;
            bool f12Now = glfwGetKey(window, GLFW_KEY_F12) == GLFW_PRESS;
            if (f12Now && !f12Prev) takeScreenshot();
            f12Prev = f12Now;
        }

        // Performance overlay toggle (F1)
        {
            static bool f1Prev = false;
            bool f1Now = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
            if (f1Now && !f1Prev) showPerformance = !showPerformance;
            f1Prev = f1Now;
        }

        // Wireframe toggle (E key with no modifiers)
        {
            bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
            static bool ePrev = false;
            bool eNow = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
            if (eNow && !ePrev && !ctrl) wireframeOnly = !wireframeOnly;
            ePrev = eNow;
        }

        // Toggle all autorotate (A)
        {
            static bool aPrev = false;
            bool aNow = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
            if (aNow && !aPrev) {
                bool anyOn = false;
                for (auto v : transform.autoRotate) if (v) anyOn = true;
                std::fill(transform.autoRotate.begin(), transform.autoRotate.end(), !anyOn);
            }
            aPrev = aNow;
        }

        // Toggle transparency (T)
        {
            static bool tPrev = false;
            bool tNow = glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
            bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
            if (tNow && !tPrev && !ctrl) transparent = !transparent;
            tPrev = tNow;
        }

        // Color scheme cycle (C)
        {
            static bool cPrev = false;
            bool cNow = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
            bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
            if (cNow && !cPrev && !ctrl) {
                colorScheme = (colorScheme + 1) % 5;
                applyColorScheme(colorScheme);
            }
            cPrev = cNow;
        }

        // Render mode cycle (M)
        {
            static bool mPrev = false;
            bool mNow = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
            if (mNow && !mPrev) renderMode = (renderMode + 1) % 3;
            mPrev = mNow;
        }

        // Auto-rotation preset cycle (V)
        {
            static bool vPrev = false;
            bool vNow = glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS;
            if (vNow && !vPrev) {
                rotPreset = (rotPreset + 1) % 4;
                switch (rotPreset) {
                    case 0:
                        std::fill(transform.autoRotate.begin(), transform.autoRotate.end(), false);
                        break;
                    case 1:
                        std::fill(transform.autoRotate.begin(), transform.autoRotate.end(), true);
                        break;
                    case 2:
                        for (int i = 0; i < transform.planeCount(); i++)
                            transform.autoRotate[i] = (i % 2 == 0);
                        break;
                    case 3:
                        for (int i = 0; i < transform.planeCount(); i++)
                            transform.autoRotate[i] = (i % 3 == 0);
                        break;
                }
            }
            vPrev = vNow;
        }

        // Clipping plane (focal length) controls - [ and ]
        {
            static bool leftBracketPrev = false, rightBracketPrev = false;
            bool leftBracketNow = glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS;
            bool rightBracketNow = glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
            bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
            if (leftBracketNow && !leftBracketPrev && !ctrl) focalLength = std::max(0.1f, focalLength - 0.1f);
            if (rightBracketNow && !rightBracketPrev && !ctrl) focalLength = std::min(5.0f, focalLength + 0.1f);
            leftBracketPrev = leftBracketNow;
            rightBracketPrev = rightBracketNow;
        }

        // Mouse orbit (right-click drag)
        {
            if (mouse.rightPressed) {
                int nSliders = transform.planeCount();
                float panelH = PAD * 2.0f + 24.0f + (float)nSliders * SLIDER_HEIGHT + 10.0f;
                bool overSlider = mouse.x >= 10.0f && mouse.x <= 10.0f + PANEL_WIDTH &&
                                  mouse.y >= 10.0f && mouse.y <= 10.0f + panelH;
                if (!overSlider && !mouse.left)
                    orbitMode = true;
            }
            if (mouse.rightReleased) orbitMode = false;
            if (orbitMode && mouse.right && mouse.moved) {
                double dx = mouse.x - mouse.lastX;
                double dy = mouse.y - mouse.lastY;
                if (dims >= 3) {
                    transform.angles[1] -= (float)dx * 0.005f;          // XZ (yaw)
                    transform.angles[dims - 1] += (float)dy * 0.005f;   // YZ (pitch)
                } else if (transform.planeCount() >= 1) {
                    transform.angles[0] += (float)dx * 0.005f;
                }
            }
        }

        // Per-plane auto-rotation with clamped delta
        {
            double now = glfwGetTime();
            float dt = std::min((float)(now - lastTime), 0.05f);
            lastTime = now;

            for (int i = 0; i < transform.planeCount(); i++) {
                if (transform.autoRotate[i])
                    transform.angles[i] += dt * 0.5f * (1 + (i % 3));
            }
        }

        // Wrap all angles to [-PI, PI)
        for (int i = 0; i < transform.planeCount(); i++) {
            float a = transform.angles[i];
            transform.angles[i] = fmodf(a + PI, 2.0f * PI);
            if (transform.angles[i] < 0) transform.angles[i] += 2.0f * PI;
            transform.angles[i] -= PI;
        }

        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        float aspect = (float)fbW / (float)fbH;

        snprintf(titleBuf, sizeof(titleBuf), "Ducky - %uD (%u verts, %zu edges) [F1=perf]",
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

            if (mouse.leftPressed) {
                bool sliderHit = false;
                for (int i = 0; i < nSliders; i++) {
                    float rowY = panelTop + PAD + titleH + PAD + i * SLIDER_HEIGHT;
                    float tglY = rowY + (SLIDER_HEIGHT - TOGGLE_SIZE) / 2;
                    if (mouse.x >= toggleX && mouse.x <= toggleX + TOGGLE_SIZE &&
                        mouse.y >= tglY && mouse.y <= tglY + TOGGLE_SIZE) {
                        transform.autoRotate[i] = !transform.autoRotate[i];
                        sliderHit = true;
                        break;
                    }
                }
                if (!sliderHit) {
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
            }

            if (dragSlider >= 0 && mouse.left) {
                float t = (float)((mouse.x - trackX) / trackW);
                t = std::max(0.0f, std::min(1.0f, t));
                transform.angles[dragSlider] = -PI + t * (2.0f * PI);
            }

            if (mouse.leftReleased) {
                if (clickedButton >= 0) {
                    // Process button action
                    switch (clickedButton) {
                        case BTN_RESET:
                            std::fill(transform.angles.begin(), transform.angles.end(), 0.0f);
                            std::fill(transform.translation.begin(), transform.translation.end(), 0.0f);
                            break;
                        case BTN_WIREFRAME:
                            wireframeOnly = !wireframeOnly;
                            break;
                        case BTN_COLOR:
                            colorScheme = (colorScheme + 1) % 5;
                            applyColorScheme(colorScheme);
                            break;
                        case BTN_PRESET:
                            rotPreset = (rotPreset + 1) % 4;
                            switch (rotPreset) {
                                case 0: std::fill(transform.autoRotate.begin(), transform.autoRotate.end(), false); break;
                                case 1: std::fill(transform.autoRotate.begin(), transform.autoRotate.end(), true); break;
                                case 2: for (int i = 0; i < transform.planeCount(); i++) transform.autoRotate[i] = (i % 2 == 0); break;
                                case 3: for (int i = 0; i < transform.planeCount(); i++) transform.autoRotate[i] = (i % 3 == 0); break;
                            }
                            break;
                        case BTN_FOCAL_DOWN:
                            focalLength = std::max(0.1f, focalLength - 0.1f);
                            break;
                        case BTN_FOCAL_UP:
                            focalLength = std::min(5.0f, focalLength + 0.1f);
                            break;
                        case BTN_MODE:
                            renderMode = (renderMode + 1) % 3;
                            break;
                        case BTN_FS:
                            toggleFullscreen();
                            break;
                        case BTN_SAVE:
                            saveState("ducky_state.txt", transform);
                            break;
                        case BTN_LOAD:
                            loadState("ducky_state.txt", transform);
                            break;
                        case BTN_SHOT:
                            takeScreenshot();
                            break;
                    }
                }
                dragSlider = -1;
                clickedButton = -1;
            }

            // Check button clicks on press
            if (mouse.leftPressed && clickedButton < 0) {
                int btnW = 76, btnH = 24, gap = 4, colGap = 5;
                int btnStartX = (int)((float)fbW - 260.0f + 8);
                int btnStartY = (int)(10.0f + 145);
                int btnCols = 3;
                for (int b = 0; b < BTN_COUNT; b++) {
                    int col = b % btnCols;
                    int row = b / btnCols;
                    int bx = btnStartX + col * (btnW + colGap);
                    int by = btnStartY + row * (btnH + gap);
                    if (mouse.x >= bx && mouse.x <= bx + btnW &&
                        mouse.y >= by && mouse.y <= by + btnH) {
                        clickedButton = b;
                        break;
                    }
                }
            }

            hoverSlider = -1;
            for (int i = 0; i < nSliders; i++) {
                float rowY = panelTop + PAD + titleH + PAD + i * SLIDER_HEIGHT;
                if (mouse.x >= trackX && mouse.x <= trackX + trackW &&
                    mouse.y >= rowY && mouse.y <= rowY + SLIDER_HEIGHT)
                    hoverSlider = i;
            }
        }

        // Process vertices
        float* pos = (float*)alloca(dims * sizeof(float));
        for (unsigned int i = 0; i < model.vertexCount; i++) {
            for (unsigned int d = 0; d < dims; d++)
                pos[d] = model.vertices[i * fpv + d];
            for (unsigned int d = 0; d < dims; d++)
                pos[d] += transform.translation[d];
            applyRotation(pos, transform);
            switch (renderMode) {
                case 0: projectPerspective(pos, &projectedVerts[i * 6], dims, focalLength); break;
                case 1: projectStereographic(pos, &projectedVerts[i * 6], dims, focalLength); break;
                default: projectOrthographic(pos, &projectedVerts[i * 6], dims); break;
            }
            projectedVerts[i * 6 + 3] = model.vertices[i * fpv + dims];
            projectedVerts[i * 6 + 4] = model.vertices[i * fpv + dims + 1];
            projectedVerts[i * 6 + 5] = model.vertices[i * fpv + dims + 2];
        }

        // Depth sort and face rendering
        {
            unsigned int numTri = model.indexCount / 3;

            if (renderMode == 1) {
                // Subdivide triangles for stereographic mode
                int n = EDGE_SUBDIV;
                int vertsPerTri = (n + 1) * (n + 2) / 2;
                int trisPerTri = n * n;
                unsigned int totalSubTris = numTri * trisPerTri;
                unsigned int totalSubVerts = numTri * vertsPerTri;

                subVerts.resize(totalSubVerts * 6);
                subIdx.resize(totalSubTris * 3);
                subDepths.resize(totalSubTris);

                float* tposA = (float*)alloca(dims * sizeof(float));
                float* tposB = (float*)alloca(dims * sizeof(float));
                float* tposC = (float*)alloca(dims * sizeof(float));
                float* interp = (float*)alloca(dims * sizeof(float));

                for (unsigned int t = 0; t < numTri; t++) {
                    int ia = model.indices[t * 3];
                    int ib = model.indices[t * 3 + 1];
                    int ic = model.indices[t * 3 + 2];

                    for (unsigned int d = 0; d < dims; d++) {
                        tposA[d] = model.vertices[ia * fpv + d] + transform.translation[d];
                        tposB[d] = model.vertices[ib * fpv + d] + transform.translation[d];
                        tposC[d] = model.vertices[ic * fpv + d] + transform.translation[d];
                    }
                    applyRotation(tposA, transform);
                    applyRotation(tposB, transform);
                    applyRotation(tposC, transform);

                    float rA = model.vertices[ia * fpv + dims];
                    float gA = model.vertices[ia * fpv + dims + 1];
                    float bA = model.vertices[ia * fpv + dims + 2];
                    float rB = model.vertices[ib * fpv + dims];
                    float gB = model.vertices[ib * fpv + dims + 1];
                    float bB = model.vertices[ib * fpv + dims + 2];
                    float rC = model.vertices[ic * fpv + dims];
                    float gC = model.vertices[ic * fpv + dims + 1];
                    float bC = model.vertices[ic * fpv + dims + 2];

                    unsigned int triVertBase = t * vertsPerTri;
                    unsigned int triTriBase = t * trisPerTri;

                    // Generate grid vertices
                    for (int j = 0; j <= n; j++) {
                        int rowOff = j * (n + 1) - j * (j - 1) / 2;
                        for (int i = 0; i <= n - j; i++) {
                            float u = (float)i / n;
                            float v = (float)j / n;
                            float w = 1.0f - u - v;
                            int vidx = triVertBase + rowOff + i;

                            for (unsigned int d = 0; d < dims; d++)
                                interp[d] = tposA[d] * w + tposB[d] * u + tposC[d] * v;
                            projectStereographic(interp, &subVerts[vidx * 6], dims, focalLength);

                            subVerts[vidx * 6 + 3] = rA * w + rB * u + rC * v;
                            subVerts[vidx * 6 + 4] = gA * w + gB * u + gC * v;
                            subVerts[vidx * 6 + 5] = bA * w + bB * u + bC * v;
                        }
                    }

                    // Generate sub-triangle indices and compute depths
                    unsigned int subTriCount = 0;
                    for (int j = 0; j < n; j++) {
                        int rowOffJ = j * (n + 1) - j * (j - 1) / 2;
                        int rowOffJ1 = (j + 1) * (n + 1) - (j + 1) * j / 2;
                        for (int i = 0; i < n - j; i++) {
                            unsigned int v00 = triVertBase + rowOffJ + i;
                            unsigned int v10 = triVertBase + rowOffJ + i + 1;
                            unsigned int v01 = triVertBase + rowOffJ1 + i;

                            unsigned int ti = triTriBase + subTriCount;

                            // Lower triangle
                            subIdx[ti * 3 + 0] = v00;
                            subIdx[ti * 3 + 1] = v10;
                            subIdx[ti * 3 + 2] = v01;
                            subDepths[ti].idx = (int)ti;
                            subDepths[ti].depth = (subVerts[v00 * 6 + 2] + subVerts[v10 * 6 + 2] + subVerts[v01 * 6 + 2]) / 3.0f;
                            subTriCount++;

                            // Upper triangle (if not on diagonal)
                            if (i + j < n - 1) {
                                unsigned int v11 = triVertBase + rowOffJ1 + i + 1;
                                unsigned int ti2 = triTriBase + subTriCount;
                                subIdx[ti2 * 3 + 0] = v10;
                                subIdx[ti2 * 3 + 1] = v11;
                                subIdx[ti2 * 3 + 2] = v01;
                                subDepths[ti2].idx = (int)ti2;
                                subDepths[ti2].depth = (subVerts[v10 * 6 + 2] + subVerts[v11 * 6 + 2] + subVerts[v01 * 6 + 2]) / 3.0f;
                                subTriCount++;
                            }
                        }
                    }
                }

                // Sort sub-triangles by depth
                std::sort(subDepths.begin(), subDepths.end(),
                          [](auto& a, auto& b) { return a.depth > b.depth; });

                // Reorder indices
                std::vector<unsigned int> sortedSubIdx(totalSubTris * 3);
                for (unsigned int i = 0; i < totalSubTris; i++) {
                    int t = subDepths[i].idx;
                    sortedSubIdx[i * 3 + 0] = subIdx[t * 3 + 0];
                    sortedSubIdx[i * 3 + 1] = subIdx[t * 3 + 1];
                    sortedSubIdx[i * 3 + 2] = subIdx[t * 3 + 2];
                }

                // Upload subdivided geometry
                glBindBuffer(GL_ARRAY_BUFFER, subVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, totalSubVerts * 6 * sizeof(float), subVerts.data());
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subEBO);
                glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, totalSubTris * 3 * sizeof(unsigned int), sortedSubIdx.data());

                glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                if (!wireframeOnly) {
                    if (transparent) {
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        glDepthMask(GL_FALSE);
                    }
                    glUseProgram(tessProgram);
                    glUniform1f(tessUAspect, aspect);
                    glUniform1f(tessUDist3D, 3.0f * focalLength);
                    glUniform1f(tessUAlpha, transparent ? modelAlpha : 1.0f);
                    glBindVertexArray(subVAO);
                    glDrawElements(GL_TRIANGLES, totalSubTris * 3, GL_UNSIGNED_INT, nullptr);
                    if (transparent) {
                        glDepthMask(GL_TRUE);
                        glDisable(GL_BLEND);
                    }
                }
            } else {
                // Original path for perspective/orthographic
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

                glBindBuffer(GL_ARRAY_BUFFER, tessVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, model.vertexCount * 6 * sizeof(float), projectedVerts.data());

                glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                if (!wireframeOnly) {
                    if (transparent) {
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        glDepthMask(GL_FALSE);
                    }
                    glUseProgram(tessProgram);
                    glUniform1f(tessUAspect, aspect);
                    glUniform1f(tessUDist3D, 3.0f * focalLength);
                    glUniform1f(tessUAlpha, transparent ? modelAlpha : 1.0f);
                    glBindVertexArray(tessVAO);
                    glDrawElements(GL_TRIANGLES, model.indexCount, GL_UNSIGNED_INT, nullptr);
                    if (transparent) {
                        glDepthMask(GL_TRUE);
                        glDisable(GL_BLEND);
                    }
                }
            }
        }

        // Draw axes
        {
            float* origin = (float*)alloca(dims * sizeof(float));
            float* tip = (float*)alloca(dims * sizeof(float));
            for (unsigned int d = 0; d < dims; d++) {
                memset(origin, 0, dims * sizeof(float));
                memset(tip, 0, dims * sizeof(float));
                tip[d] = AXIS_LENGTH;
                applyRotation(origin, transform);
                applyRotation(tip, transform);
                projectPerspective(origin, &axis3D[d * 12], dims, focalLength);
                projectPerspective(tip, &axis3D[d * 12 + 6], dims, focalLength);
                axis3D[d * 12 + 3] = axisR[d]; axis3D[d * 12 + 4] = axisG[d]; axis3D[d * 12 + 5] = axisB[d];
                axis3D[d * 12 + 9] = axisR[d]; axis3D[d * 12 + 10] = axisG[d]; axis3D[d * 12 + 11] = axisB[d];
            }
            glBindBuffer(GL_ARRAY_BUFFER, axesVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, dims * 2 * 6 * sizeof(float), axis3D.data());
            glUseProgram(axesProgram);
            glUniform1f(axesUAspect, aspect);
            glUniform1f(axesUDist3D, 3.0f * focalLength);
            glBindVertexArray(axesVAO);
            glDrawArrays(GL_LINES, 0, dims * 2);
        }

        // Draw wireframe edges
        {
            size_t vertCount = 0;
            if (renderMode == 1) {
                float* posA = (float*)alloca(dims * sizeof(float));
                float* posB = (float*)alloca(dims * sizeof(float));
                float* interp = (float*)alloca(dims * sizeof(float));
                for (size_t i = 0; i < edges.size(); i++) {
                    int ia = edges[i].a, ib = edges[i].b;
                    for (unsigned int d = 0; d < dims; d++) {
                        posA[d] = model.vertices[ia * fpv + d] + transform.translation[d];
                        posB[d] = model.vertices[ib * fpv + d] + transform.translation[d];
                    }
                    applyRotation(posA, transform);
                    applyRotation(posB, transform);
                    for (int s = 0; s <= EDGE_SUBDIV; s++) {
                        float t = (float)s / (float)EDGE_SUBDIV;
                        for (unsigned int d = 0; d < dims; d++)
                            interp[d] = posA[d] * (1.0f - t) + posB[d] * t;
                        projectStereographic(interp, &edge3D[vertCount * 3], dims, focalLength);
                        vertCount++;
                    }
                }
            } else {
                for (size_t i = 0; i < edges.size(); i++) {
                    int ia = edges[i].a, ib = edges[i].b;
                    edge3D[vertCount * 3 + 0] = projectedVerts[ia * 6];
                    edge3D[vertCount * 3 + 1] = projectedVerts[ia * 6 + 1];
                    edge3D[vertCount * 3 + 2] = projectedVerts[ia * 6 + 2];
                    vertCount++;
                    edge3D[vertCount * 3 + 0] = projectedVerts[ib * 6];
                    edge3D[vertCount * 3 + 1] = projectedVerts[ib * 6 + 1];
                    edge3D[vertCount * 3 + 2] = projectedVerts[ib * 6 + 2];
                    vertCount++;
                }
            }
            glBindBuffer(GL_ARRAY_BUFFER, edgeVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, vertCount * 3 * sizeof(float), edge3D.data());

            glDisable(GL_DEPTH_TEST);
            glUseProgram(edgeProgram);
            glUniform1f(edgeUAspect, aspect);
            glUniform1f(edgeUDist3D, 3.0f * focalLength);
            glBindVertexArray(edgeVAO);
            if (renderMode == 1) {
                size_t offset = 0;
                for (size_t i = 0; i < edges.size(); i++) {
                    glDrawArrays(GL_LINE_STRIP, (GLint)offset, EDGE_SUBDIV + 1);
                    offset += EDGE_SUBDIV + 1;
                }
            } else {
                glDrawArrays(GL_LINES, 0, (GLsizei)vertCount);
            }
            glEnable(GL_DEPTH_TEST);
        }

        // === UI overlay ===
        glDisable(GL_DEPTH_TEST);

        int nSliders = transform.planeCount();
        float panelTop = 10.0f;
        float panelLeft = 10.0f;
        float titleH = 24.0f;
        float panelH = PAD * 2 + titleH + nSliders * SLIDER_HEIGHT + 10.0f;

        // Slider panel background
        drawRect(uiProgram, uiVAO, uiVBO,
                 panelLeft, panelTop, PANEL_WIDTH, panelH,
                 0.12f, 0.12f, 0.18f, 0.92f, (float)fbW, (float)fbH);

        // Border
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

            int pi = 0;
            int ai = -1, aj = -1;
            for (int a = 0; a < (int)dims && pi <= i; a++)
                for (int b = a + 1; b < (int)dims && pi <= i; b++, pi++)
                    if (pi == i) { ai = a; aj = b; }
            snprintf(label, sizeof(label), "(%d,%d)", ai, aj);

            drawRect(uiProgram, uiVAO, uiVBO,
                     trackX, rowY, trackW, SLIDER_HEIGHT,
                     0.2f, 0.2f, 0.3f, 0.9f, (float)fbW, (float)fbH);

            float val = transform.angles[i];
            float fillFrac = (val + PI) / (2.0f * PI);
            fillFrac = std::max(0.0f, std::min(1.0f, fillFrac));
            drawRect(uiProgram, uiVAO, uiVBO,
                     trackX, rowY, trackW * fillFrac, SLIDER_HEIGHT,
                     0.35f, 0.5f, 0.9f, 0.8f, (float)fbW, (float)fbH);

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

        // Model info + buttons panel (right side)
        {
            float infoX = (float)fbW - 260.0f;
            float infoY = 10.0f;
            float infoH = 280.0f;
            drawRect(uiProgram, uiVAO, uiVBO,
                     infoX, infoY, 250.0f, infoH,
                     0.12f, 0.12f, 0.18f, 0.92f, (float)fbW, (float)fbH);

            char infoLines[512];
            snprintf(infoLines, sizeof(infoLines),
                      "%uD Model\n"
                      "Vertices: %u\n"
                      "Triangles: %u\n"
                      "Edges: %zu\n"
                      "Planes: %d\n"
                      "Focal: %.1f\n"
                      "Scheme: %s\n"
                      "Wireframe: %s\n"
                      "Mode: %s",
                      dims, model.vertexCount, model.indexCount / 3,
                      edges.size(), transform.planeCount(),
                      focalLength,
                       colorSchemeNames[colorScheme],
                      wireframeOnly ? "ON" : "OFF",
                      renderModeNames[renderMode]);

            std::string infoStr(infoLines);
            size_t pos = 0;
            float lineY = infoY + 8;
            while (pos < infoStr.size()) {
                size_t nl = infoStr.find('\n', pos);
                if (nl == std::string::npos) nl = infoStr.size();
                std::string line = infoStr.substr(pos, nl - pos);
                drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
                           infoX + 8, lineY, line.c_str(),
                           (float)fbW, (float)fbH, dtIndices.data(), TEXT_MAX_QUADS);
                lineY += 14;
                pos = nl + 1;
            }

            // Draw action buttons
            int btnW = 76, btnH = 24, gap = 4, colGap = 5;
            int btnStartX = (int)infoX + 8;
            int btnStartY = (int)infoY + 145;
            const char* btnLabels[] = {"Reset All", "Wireframe", "Color Scheme",
                                        "Rotation", "Focus -", "Focus +",
                                        "Render Mode",
                                        "Fullscreen", "Save State", "Load State",
                                        "Screenshot"};
            int btnCols = 3;
            for (int b = 0; b < BTN_COUNT; b++) {
                int col = b % btnCols;
                int row = b / btnCols;
                int bx = btnStartX + col * (btnW + colGap);
                int by = btnStartY + row * (btnH + gap);
                bool hovered = (mouse.x >= bx && mouse.x <= bx + btnW &&
                                mouse.y >= by && mouse.y <= by + btnH);
                drawRect(uiProgram, uiVAO, uiVBO,
                         (float)bx, (float)by, (float)btnW, (float)btnH,
                         hovered ? 0.3f : 0.2f, hovered ? 0.3f : 0.2f, hovered ? 0.4f : 0.28f, 0.9f,
                         (float)fbW, (float)fbH);
                drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
                           (float)bx + 4, (float)by + 4, btnLabels[b],
                           (float)fbW, (float)fbH, dtIndices.data(), TEXT_MAX_QUADS);
            }
        }

        // Performance overlay
        if (showPerformance) {
            perfFrameCount++;
            double now = glfwGetTime();
            if (now - perfLastTime >= 0.5) {
                perfFps = perfFrameCount / (float)(now - perfLastTime);
                perfFrameCount = 0;
                perfLastTime = now;
            }
            char perfStr[128];
            snprintf(perfStr, sizeof(perfStr), "FPS: %.1f  Verts: %u  Tris: %u",
                     perfFps, model.vertexCount, model.indexCount / 3);
            int nq = drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
                                (float)fbW - 250.0f, (float)fbH - 30.0f, perfStr,
                                (float)fbW, (float)fbH, dtIndices.data(), TEXT_MAX_QUADS);
        }

        // HUD text hint
        {
            int hudNumQuads = stb_easy_font_print(PANEL_WIDTH + 20, 12, hintText,
                                                  nullptr, textBuffer.data(), (int)textBuffer.size());
            glUseProgram(textProgram);
            glUniform2f(textScreenSize, (float)fbW, (float)fbH);
            glBindBuffer(GL_ARRAY_BUFFER, textVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, hudNumQuads * 64, textBuffer.data());
            glBindVertexArray(textVAO);
            glDrawElements(GL_TRIANGLES, hudNumQuads * 6, GL_UNSIGNED_INT, nullptr);
        }

        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &tessVAO);
    glDeleteBuffers(1, &tessVBO);
    glDeleteBuffers(1, &tessEBO);
    glDeleteVertexArrays(1, &axesVAO);
    glDeleteBuffers(1, &axesVBO);
    glDeleteVertexArrays(1, &subVAO);
    glDeleteBuffers(1, &subVBO);
    glDeleteBuffers(1, &subEBO);
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
