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

struct TransformND {
    std::vector<float> angles;
    std::vector<float> translation;
    unsigned int dims;

    int planeCount() const { return dims * (dims - 1) / 2; }

    int planeIndex(int i, int j) const {
        int idx = 0;
        for (int a = 0; a < i; a++)
            idx += dims - a - 1;
        return idx + (j - i - 1);
    }
};

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
    float tmp[16];
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

// Load file to string
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

static void processInput(GLFWwindow* window, TransformND& t) {
    // Translation: map dims to keys
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

    // Rotation: map first 6 planes to number keys
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

    // Auto-rotate remaining planes that don't have key bindings
    for (int i = 6; i < t.planeCount(); i++) {
        t.angles[i] += 0.003f * (1 + (i % 3));
    }

    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        std::fill(t.angles.begin(), t.angles.end(), 0.0f);
        std::fill(t.translation.begin(), t.translation.end(), 0.0f);
    }
}

struct Edge { int a, b; };

static std::vector<Edge> generateEdges(const float* vertices, unsigned int vertexCount,
                                        unsigned int dims, int fpv) {
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
    return edges;
}

int main(int argc, char* argv[]) {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
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
    int fpv = dims + 3; // floats per vertex (N pos + 3 color)

    // Assign face colors if model has no meaningful colors
    // Color each face with a distinct HSL-based color
    // Figure out faces by grouping 3 consecutive indices per triangle
    {
        if (model.vertexCount > 0 && model.indexCount > 0) {
            // Find unique faces by looking at triangles sharing indices
            // Simple approach: color each vertex by its face index
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

    // Generate edges from mesh topology (connect vertices differing in 1 coordinate)
    auto edges = generateEdges(model.vertices, model.vertexCount, dims, fpv);
    std::cout << "Generated " << edges.size() << " edges" << std::endl;

    // === Tesseract setup (3D rendering from CPU-projected data) ===
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

    // Generate axis data for N dimensions: each axis is a line from origin to a unit vector
    // We'll dynamically regenerate this each frame since it goes through the same transform
    {
        float* axisData = new float[dims * 2 * 6]; // 2 vertices per axis * 6 floats (3 pos + 3 color)
        for (unsigned int d = 0; d < dims; d++) {
            float h = (float)d / (float)dims;
            float r, g, b;
            hslToRgb(h, 0.9f, 0.6f, r, g, b);
            // Origin vertex
            axisData[d * 12 + 0] = 0; axisData[d * 12 + 1] = 0; axisData[d * 12 + 2] = 0;
            axisData[d * 12 + 3] = r; axisData[d * 12 + 4] = g; axisData[d * 12 + 5] = b;
            // Axis tip vertex (unit vector along dimension d)
            axisData[d * 12 + 6] = 0; axisData[d * 12 + 7] = 0; axisData[d * 12 + 8] = 0;
            axisData[d * 12 + 9] = r; axisData[d * 12 + 10] = g; axisData[d * 12 + 11] = b;
            // Set the actual axis direction - we'll transform it per frame
            // Mark which dimension this is (stored in unused float)
            axisData[d * 12 + 0 + d] = AXIS_LENGTH; // set the d-th component to AXIS_LENGTH for the tip
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

    // === Text setup ===
    GLuint textProgram = createShaderProgram("shaders/text.vert", "shaders/text.frag");
    if (!textProgram) { glfwTerminate(); return -1; }

    // Build controls text dynamically
    std::string hintText = "Ducky: " + std::to_string(dims) + "D viewer";
    hintText += "  |  Move: WASD=01 QE=2 ZX=3 TG=4 BH=5";
    hintText += "  |  Rot: 12 34 56 78 90 -+";
    hintText += "  |  R=reset";

    char textBuffer[20000];
    std::vector<char> hintCopy(hintText.begin(), hintText.end());
    hintCopy.push_back('\0');
    int numQuads = stb_easy_font_print(10, 10, hintCopy.data(), nullptr, textBuffer, sizeof(textBuffer));

    GLuint textVAO, textVBO, textEBO;
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glGenBuffers(1, &textEBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, numQuads * 64, textBuffer, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void*)12);
    glEnableVertexAttribArray(1);

    std::vector<unsigned int> textIndices(numQuads * 6);
    for (int i = 0; i < numQuads; i++) {
        int base = i * 4;
        textIndices[i * 6 + 0] = base;
        textIndices[i * 6 + 1] = base + 1;
        textIndices[i * 6 + 2] = base + 2;
        textIndices[i * 6 + 3] = base + 1;
        textIndices[i * 6 + 4] = base + 3;
        textIndices[i * 6 + 5] = base + 2;
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, textEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, textIndices.size() * sizeof(unsigned int), textIndices.data(), GL_STATIC_DRAW);

    GLint textScreenSize = glGetUniformLocation(textProgram, "uScreenSize");

    TransformND transform;
    transform.dims = dims;
    transform.angles.resize(transform.planeCount(), 0.0f);
    transform.translation.resize(dims, 0.0f);

    // Per-frame projection buffers
    std::vector<float> projectedVerts(model.vertexCount * 6); // 3 pos + 3 color per vert

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        processInput(window, transform);

        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);
        float aspect = (float)fbW / (float)fbH;

        snprintf(titleBuf, sizeof(titleBuf), "Ducky - %uD (%u verts, %zu edges)",
                 dims, model.vertexCount, edges.size());
        glfwSetWindowTitle(window, titleBuf);

        // Process all vertices: rotate in N-D, project recursively to 3D
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

        // Depth sort triangles by average projected Z
        {
            struct TriDepth { int idx; float depth; };
            std::vector<TriDepth> triDepths(model.indexCount / 3);
            for (unsigned int i = 0; i < model.indexCount / 3; i++) {
                float sum = 0;
                for (int j = 0; j < 3; j++) {
                    int vi = model.indices[i * 3 + j];
                    sum += projectedVerts[vi * 6 + 2];
                }
                triDepths[i] = {(int)i, sum / 3.0f};
            }
            std::sort(triDepths.begin(), triDepths.end(),
                      [](auto& a, auto& b) { return a.depth > b.depth; });

            std::vector<unsigned int> sorted(model.indexCount);
            for (unsigned int i = 0; i < model.indexCount / 3; i++) {
                int t = triDepths[i].idx;
                sorted[i * 3 + 0] = model.indices[t * 3 + 0];
                sorted[i * 3 + 1] = model.indices[t * 3 + 1];
                sorted[i * 3 + 2] = model.indices[t * 3 + 2];
            }
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tessEBO);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, model.indexCount * sizeof(unsigned int), sorted.data());
        }

        // Draw tesseract faces
        glBindBuffer(GL_ARRAY_BUFFER, tessVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, model.vertexCount * 6 * sizeof(float), projectedVerts.data());

        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(tessProgram);
        glUniform1f(tessUAspect, aspect);
        glBindVertexArray(tessVAO);
        glDrawElements(GL_TRIANGLES, model.indexCount, GL_UNSIGNED_INT, nullptr);

        // Process and draw axes
        {
            float* axis3D = new float[dims * 2 * 6];
            for (unsigned int d = 0; d < dims; d++) {
                float h = (float)d / (float)dims;
                float r, g, b;
                hslToRgb(h, 0.9f, 0.6f, r, g, b);

                // Origin (0 in all dims)
                float origin[16] = {0};
                // Tip: AXIS_LENGTH in dimension d
                float tip[16] = {0};
                tip[d] = AXIS_LENGTH;

                applyRotation(origin, transform);
                applyRotation(tip, transform);
                projectTo3D(origin, &axis3D[d * 12], dims);
                projectTo3D(tip, &axis3D[d * 12 + 6], dims);

                axis3D[d * 12 + 3] = r; axis3D[d * 12 + 4] = g; axis3D[d * 12 + 5] = b;
                axis3D[d * 12 + 9] = r; axis3D[d * 12 + 10] = g; axis3D[d * 12 + 11] = b;
            }

            glBindBuffer(GL_ARRAY_BUFFER, axesVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, dims * 2 * 6 * sizeof(float), axis3D);
            glUseProgram(axesProgram);
            glUniform1f(axesUAspect, aspect);
            glBindVertexArray(axesVAO);
            glDrawArrays(GL_LINES, 0, dims * 2);
            delete[] axis3D;
        }

        // Draw wireframe edges
        {
            float* edge3D = new float[edges.size() * 2 * 3];
            for (size_t i = 0; i < edges.size(); i++) {
                edge3D[i * 6 + 0] = projectedVerts[edges[i].a * 6];
                edge3D[i * 6 + 1] = projectedVerts[edges[i].a * 6 + 1];
                edge3D[i * 6 + 2] = projectedVerts[edges[i].a * 6 + 2];
                edge3D[i * 6 + 3] = projectedVerts[edges[i].b * 6];
                edge3D[i * 6 + 4] = projectedVerts[edges[i].b * 6 + 1];
                edge3D[i * 6 + 5] = projectedVerts[edges[i].b * 6 + 2];
            }
            glBindBuffer(GL_ARRAY_BUFFER, edgeVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, edges.size() * 2 * 3 * sizeof(float), edge3D);

            glDisable(GL_DEPTH_TEST);
            glUseProgram(edgeProgram);
            glUniform1f(edgeUAspect, aspect);
            glBindVertexArray(edgeVAO);
            glDrawArrays(GL_LINES, 0, edges.size() * 2);
            delete[] edge3D;
        }

        // Draw text
        glUseProgram(textProgram);
        glUniform2f(textScreenSize, fbW, fbH);
        glBindVertexArray(textVAO);
        glDrawElements(GL_TRIANGLES, numQuads * 6, GL_UNSIGNED_INT, nullptr);
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
    glDeleteProgram(tessProgram);
    glDeleteProgram(axesProgram);
    glDeleteProgram(textProgram);

    glfwTerminate();
    return 0;
}
