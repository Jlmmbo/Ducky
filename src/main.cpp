#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <cstring>
#include <ctime>

#include "main.hpp"
#include "transform.hpp"
#include "shader.hpp"
#include "render.hpp"
#include "io.hpp"

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

constexpr int WINDOW_WIDTH = 1920;
constexpr int WINDOW_HEIGHT = 1920;
constexpr float ROTATE_SPEED = 0.02f;
constexpr float MOVE_SPEED = 2.0f;
constexpr float AXIS_LENGTH = 1.5f;

constexpr float PANEL_WIDTH = 290.0f;
constexpr float SLIDER_HEIGHT = 28.0f;
constexpr float TOGGLE_SIZE = 14.0f;
constexpr float LABEL_WIDTH = 36.0f;
constexpr float VALUE_WIDTH = 55.0f;
constexpr float PAD = 6.0f;

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

static void processInput(GLFWwindow* window, TransformND& t, bool newControls, float dt) {
    if (newControls) {
        float spd = MOVE_SPEED * dt;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) t.translation[0] -= spd;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) t.translation[0] += spd;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) t.translation[1] -= spd;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) t.translation[1] += spd;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) t.translation[2] -= spd;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) t.translation[2] += spd;
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
            std::fill(t.angles.begin(), t.angles.end(), 0.0f);
            std::fill(t.modelAngles.begin(), t.modelAngles.end(), 0.0f);
            std::fill(t.translation.begin(), t.translation.end(), 0.0f);
        }
        return;
    }

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
        std::fill(t.modelAngles.begin(), t.modelAngles.end(), 0.0f);
        std::fill(t.translation.begin(), t.translation.end(), 0.0f);
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
    int fbW = 0, fbH = 0, winW = 0, winH = 0;
    glEnable(GL_DEPTH_TEST);

    // === State variables ===
    bool showPerformance = false;
    bool wireframeOnly = false;
    int colorScheme = 0;
    const char* colorSchemeNames[5] = {"Model", "Golden", "Rainbow", "Mono", "Warm"};
    int rotPreset = 1;
    float focalLength = 1.0f;
    int renderMode = 0;
    const char* renderModeNames[] = {"Perspective", "Stereographic", "Orthographic"};
    int fullscreenW = 0, fullscreenH = 0;
    int windowedX = 0, windowedY = 0, windowedW = WINDOW_WIDTH, windowedH = WINDOW_HEIGHT;
    bool isFullscreen = false;
    bool orbitMode = false;
    bool transparent = false;
    bool lighting = true;
    float modelAlpha = 0.35f;
    bool newControls = true;

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
    GLuint tessULighting = glGetUniformLocation(tessProgram, "uLighting");

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
    snprintf(hintText, sizeof(hintText), "%uD  |  Rot:1-0,-=  |  E=wireframe V=preset C=color A=autorotate T=transparency L=lighting []=focal M=render R=reset  |  F11=FS F12=shot F1=perf  |  Right panel has all controls", dims);

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
    transform.modelAngles.resize(transform.planeCount(), 0.0f);
    transform.autoRotate.resize(transform.planeCount(), true);
    transform.translation.resize(dims, 0.0f);

    std::vector<float> projectedVerts(model.vertexCount * 6);
    std::vector<float> rotatedND(model.vertexCount * dims);

    // UI state
    MouseState mouse = {};
    int dragSlider = -1;
    int hoverSlider = -1; (void)hoverSlider;
    int clickedButton = -1;

    enum ButtonId {
        BTN_RESET, BTN_WIREFRAME, BTN_COLOR, BTN_PRESET,
        BTN_FOCAL_DOWN, BTN_FOCAL_UP, BTN_MODE,
        BTN_FS, BTN_SAVE, BTN_LOAD, BTN_SHOT, BTN_LIGHTING,
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
        // Frame timing for movement/rotation
        double now = glfwGetTime();
        float dt = std::min((float)(now - lastTime), 0.05f);
        lastTime = now;

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

        processInput(window, transform, newControls, dt);

        // ── Keyboard shortcuts ──
        // Tab toggle between new (WASD/mouse) and original controls
        {
            static bool tabPrev = false;
            bool tabNow = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
            if (tabNow && !tabPrev) newControls = !newControls;
            tabPrev = tabNow;
        }

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

        // Wireframe toggle (E key with no modifiers, old controls only)
        if (!newControls) {
            bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
            static bool ePrev = false;
            bool eNow = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
            if (eNow && !ePrev && !ctrl) wireframeOnly = !wireframeOnly;
            ePrev = eNow;
        }

        // Toggle all autorotate (A, old controls only)
        if (!newControls) {
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

        // Lighting toggle (L)
        {
            static bool lPrev = false;
            bool lNow = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
            bool ctrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
            if (lNow && !lPrev && !ctrl) lighting = !lighting;
            lPrev = lNow;
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

        // Auto-rotation preset cycle (V, old controls only)
        if (!newControls) {
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

        // Framebuffer and window size for HiDPI-aware mouse coord conversion
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glfwGetWindowSize(window, &winW, &winH);
        float uiScaleX = 1.0f, uiScaleY = 1.0f;
        if (winW > 0 && winH > 0) {
            uiScaleX = (float)fbW / (float)winW;
            uiScaleY = (float)fbH / (float)winH;
        }
        float mx_fb = mouse.x * uiScaleX;
        float my_fb = mouse.y * uiScaleY;

        // Mouse camera rotation (orbit)
        {
            bool orbitKey = newControls ? mouse.left : mouse.right;
            bool orbitPressed = newControls ? mouse.leftPressed : mouse.rightPressed;
            bool orbitReleased = newControls ? mouse.leftReleased : mouse.rightReleased;
            if (orbitPressed) {
                int nSliders = transform.planeCount();
                float panelH = PAD * 2.0f + 24.0f + (float)nSliders * SLIDER_HEIGHT + 10.0f;
                bool overSlider = mx_fb >= 10.0f && mx_fb <= 10.0f + PANEL_WIDTH &&
                                  my_fb >= 10.0f && my_fb <= 10.0f + panelH;
                if (!overSlider && !(newControls ? mouse.right : mouse.left))
                    orbitMode = true;
            }
            if (orbitReleased) orbitMode = false;
            if (orbitMode && orbitKey && mouse.moved) {
                double dx = mouse.x - mouse.lastX;
                double dy = mouse.y - mouse.lastY;
                if (dims >= 3) {
                    transform.angles[1] -= (float)dx * 0.005f;
                    transform.angles[dims - 1] += (float)dy * 0.005f;
                } else if (transform.planeCount() >= 1) {
                    transform.angles[0] += (float)dx * 0.005f;
                }
            }
        }

        // Per-plane auto-rotation with wrapping to [-PI, PI) (rotates the model)
        {
            for (int i = 0; i < transform.planeCount(); i++) {
                if (transform.autoRotate[i])
                    transform.modelAngles[i] += dt * 0.5f * (1 + (i % 3));
                float a = transform.modelAngles[i];
                a = fmodf(a + PI, 2.0f * PI);
                if (a < 0) a += 2.0f * PI;
                transform.modelAngles[i] = a - PI;
            }
        }

        glViewport(0, 0, fbW, fbH);
        float aspect = (float)fbW / (float)fbH;

        // Only update title when data changes
        static char lastTitle[128] = {};
        snprintf(titleBuf, sizeof(titleBuf), "Ducky - %uD (%u verts, %zu edges) [F1=perf]",
                 dims, model.vertexCount, edges.size());
        if (strcmp(titleBuf, lastTitle) != 0) {
            glfwSetWindowTitle(window, titleBuf);
            memcpy(lastTitle, titleBuf, sizeof(titleBuf));
        }

        constexpr float BTN_SZ = 20.0f;
        constexpr float LEFT_ROW_H = 24.0f;

        // Process panel mouse interaction
        if (newControls) {
            // HD control buttons: movement (dim >= 3) and rotation (planes with a >= 3)
            int nMoveHD = (int)dims >= 3 ? (int)dims - 3 : 0; (void)nMoveHD;
            int nRotHD = 0;
            for (int a = 3; a < (int)dims; a++)
                for (int b = a + 1; b < (int)dims; b++)
                    nRotHD++;

            float panelTop = 10.0f;
            float panelLeft = 10.0f;
            float titleH = 24.0f;
            float rowY0 = panelTop + PAD + titleH + PAD;
            float btnX0 = panelLeft + PAD;
            float btnX1 = panelLeft + PANEL_WIDTH - PAD - BTN_SZ;
            float labelX0 = btnX0 + BTN_SZ + PAD; (void)labelX0;

            // held action state: type (0=move,1=rot), idx, dir (-1 or 1)
            static int heldType = -1, heldIdx = 0, heldDir = 0;
            static float heldTimer = 0.0f;

            if (mouse.leftPressed) {
                heldType = -1;
                int row = 0;
                // Movement rows
                for (int d = 3; d < (int)dims; d++, row++) {
                    float ry = rowY0 + row * LEFT_ROW_H;
                    if (my_fb >= ry && my_fb <= ry + LEFT_ROW_H) {
                        if (mx_fb >= btnX0 && mx_fb <= btnX0 + BTN_SZ) {
                            transform.translation[d] -= 0.1f;
                            heldType = 0; heldIdx = d; heldDir = -1; heldTimer = 0.0f;
                            break;
                        }
                        if (mx_fb >= btnX1 && mx_fb <= btnX1 + BTN_SZ) {
                            transform.translation[d] += 0.1f;
                            heldType = 0; heldIdx = d; heldDir = 1; heldTimer = 0.0f;
                            break;
                        }
                    }
                }
                // Section separator (rendered but not interactive)
                if (nMoveHD > 0 && nRotHD > 0) row++;

                if (heldType < 0) {
                    // Rotation rows
                    for (int a = 3; a < (int)dims; a++)
                        for (int b = a + 1; b < (int)dims; b++, row++) {
                            float ry = rowY0 + row * LEFT_ROW_H;
                            if (my_fb >= ry && my_fb <= ry + LEFT_ROW_H) {
                                int pi = transform.planeIndex(a, b);
                                if (mx_fb >= btnX0 && mx_fb <= btnX0 + BTN_SZ) {
                                    transform.angles[pi] -= 0.1f;
                                    heldType = 1; heldIdx = pi; heldDir = -1; heldTimer = 0.0f;
                                    goto done_hd;
                                }
                                if (mx_fb >= btnX1 && mx_fb <= btnX1 + BTN_SZ) {
                                    transform.angles[pi] += 0.1f;
                                    heldType = 1; heldIdx = pi; heldDir = 1; heldTimer = 0.0f;
                                    goto done_hd;
                                }
                            }
                        }
                }
                done_hd:;
            }

            // Hold-to-repeat
            if (heldType >= 0 && mouse.left && !mouse.leftPressed) {
                heldTimer += dt;
                while (heldTimer >= 0.05f) {
                    heldTimer -= 0.05f;
                    if (heldType == 0)
                        transform.translation[heldIdx] += heldDir * 0.1f;
                    else
                        transform.angles[heldIdx] += heldDir * 0.1f;
                }
            }

            if (mouse.leftReleased) { heldType = -1; heldTimer = 0.0f; }

            // In new mode, left-click is orbit outside the panel; skip old slider/button logic
            dragSlider = -1;
        } else {
            // Original slider interaction
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
                    if (mx_fb >= toggleX && mx_fb <= toggleX + TOGGLE_SIZE &&
                        my_fb >= tglY && my_fb <= tglY + TOGGLE_SIZE) {
                        transform.autoRotate[i] = !transform.autoRotate[i];
                        sliderHit = true;
                        break;
                    }
                }
                if (!sliderHit) {
                    dragSlider = -1;
                    for (int i = 0; i < nSliders; i++) {
                        float rowY = panelTop + PAD + titleH + PAD + i * SLIDER_HEIGHT;
                        if (mx_fb >= trackX && mx_fb <= trackX + trackW &&
                            my_fb >= rowY && my_fb <= rowY + SLIDER_HEIGHT) {
                            dragSlider = i;
                            float t = (float)((mx_fb - trackX) / trackW);
                            t = std::max(0.0f, std::min(1.0f, t));
                            transform.modelAngles[i] = -PI + t * (2.0f * PI);
                            break;
                        }
                    }
                }
            }

            if (dragSlider >= 0 && mouse.left) {
                float t = (float)((mx_fb - trackX) / trackW);
                t = std::max(0.0f, std::min(1.0f, t));
                transform.modelAngles[dragSlider] = -PI + t * (2.0f * PI);
            }

            // Hover slider highlight
            hoverSlider = -1;
            for (int i = 0; i < nSliders; i++) {
                float rowY = panelTop + PAD + titleH + PAD + i * SLIDER_HEIGHT;
                if (mx_fb >= trackX && mx_fb <= trackX + trackW &&
                    my_fb >= rowY && my_fb <= rowY + SLIDER_HEIGHT)
                    hoverSlider = i;
            }
        }

        // Right-panel buttons (shared between modes)
        {
            if (mouse.leftReleased) {
                if (clickedButton >= 0) {
                    switch (clickedButton) {
                        case BTN_RESET:
                            std::fill(transform.angles.begin(), transform.angles.end(), 0.0f);
                            std::fill(transform.modelAngles.begin(), transform.modelAngles.end(), 0.0f);
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
                        case BTN_LIGHTING:
                            lighting = !lighting;
                            break;
                    }
                }
                dragSlider = -1;
                clickedButton = -1;
            }

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
                    if (mx_fb >= bx && mx_fb <= bx + btnW &&
                        my_fb >= by && my_fb <= by + btnH) {
                        clickedButton = b;
                        break;
                    }
                }
            }
        }

        // Process vertices
        float* pos = (float*)alloca(dims * sizeof(float));
        for (unsigned int i = 0; i < model.vertexCount; i++) {
            for (unsigned int d = 0; d < dims; d++)
                pos[d] = model.vertices[i * fpv + d];
            // Model auto-rotation (non-negated)
            for (int ii = 0; ii < (int)dims; ii++)
                for (int jj = ii + 1; jj < (int)dims; jj++) {
                    float angle = transform.modelAngles[transform.planeIndex(ii, jj)];
                    if (fabsf(angle) > 0.0001f)
                        rotatePlane(pos[ii], pos[jj], angle);
                }
            for (unsigned int d = 0; d < dims; d++)
                pos[d] += transform.translation[d];
            applyRotation(pos, transform);
            for (unsigned int d = 0; d < dims; d++)
                rotatedND[i * dims + d] = pos[d];
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
                        tposA[d] = rotatedND[ia * dims + d];
                        tposB[d] = rotatedND[ib * dims + d];
                        tposC[d] = rotatedND[ic * dims + d];
                    }

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

                    unsigned int subTriCount = 0;
                    for (int j = 0; j < n; j++) {
                        int rowOffJ = j * (n + 1) - j * (j - 1) / 2;
                        int rowOffJ1 = (j + 1) * (n + 1) - (j + 1) * j / 2;
                        for (int i = 0; i < n - j; i++) {
                            unsigned int v00 = triVertBase + rowOffJ + i;
                            unsigned int v10 = triVertBase + rowOffJ + i + 1;
                            unsigned int v01 = triVertBase + rowOffJ1 + i;

                            unsigned int ti = triTriBase + subTriCount;

                            subIdx[ti * 3 + 0] = v00;
                            subIdx[ti * 3 + 1] = v10;
                            subIdx[ti * 3 + 2] = v01;
                            subDepths[ti].idx = (int)ti;
                            subDepths[ti].depth = (subVerts[v00 * 6 + 2] + subVerts[v10 * 6 + 2] + subVerts[v01 * 6 + 2]) / 3.0f;
                            subTriCount++;

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

                std::sort(subDepths.begin(), subDepths.end(),
                          [](auto& a, auto& b) { return a.depth > b.depth; });

                std::vector<unsigned int> sortedSubIdx(totalSubTris * 3);
                for (unsigned int i = 0; i < totalSubTris; i++) {
                    int t = subDepths[i].idx;
                    sortedSubIdx[i * 3 + 0] = subIdx[t * 3 + 0];
                    sortedSubIdx[i * 3 + 1] = subIdx[t * 3 + 1];
                    sortedSubIdx[i * 3 + 2] = subIdx[t * 3 + 2];
                }

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
                    glUniform1f(tessULighting, lighting ? 1.0f : 0.0f);
                    glBindVertexArray(subVAO);
                    glDrawElements(GL_TRIANGLES, totalSubTris * 3, GL_UNSIGNED_INT, nullptr);
                    if (transparent) {
                        glDepthMask(GL_TRUE);
                        glDisable(GL_BLEND);
                    }
                }
            } else {
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
                    glUniform1f(tessULighting, lighting ? 1.0f : 0.0f);
                    glBindVertexArray(tessVAO);
                    glDrawElements(GL_TRIANGLES, model.indexCount, GL_UNSIGNED_INT, nullptr);
                    if (transparent) {
                        glDepthMask(GL_TRUE);
                        glDisable(GL_BLEND);
                    }
                }
            }
        }

        // Draw axes (on top of everything)
        glDisable(GL_DEPTH_TEST);
        {
            float* origin = (float*)alloca(dims * sizeof(float));
            float* tip = (float*)alloca(dims * sizeof(float));
            for (unsigned int d = 0; d < dims; d++) {
                memset(origin, 0, dims * sizeof(float));
                memset(tip, 0, dims * sizeof(float));
                tip[d] = AXIS_LENGTH;
                applyRotation(tip, transform);
                switch (renderMode) {
                    case 0: projectPerspective(origin, &axis3D[d * 12], dims, focalLength); break;
                    case 1: projectStereographic(origin, &axis3D[d * 12], dims, focalLength); break;
                    default: projectOrthographic(origin, &axis3D[d * 12], dims); break;
                }
                switch (renderMode) {
                    case 0: projectPerspective(tip, &axis3D[d * 12 + 6], dims, focalLength); break;
                    case 1: projectStereographic(tip, &axis3D[d * 12 + 6], dims, focalLength); break;
                    default: projectOrthographic(tip, &axis3D[d * 12 + 6], dims); break;
                }
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
        glEnable(GL_DEPTH_TEST);

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
                        posA[d] = rotatedND[ia * dims + d];
                        posB[d] = rotatedND[ib * dims + d];
                    }
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

        float panelTop = 10.0f;
        float panelLeft = 10.0f;
        float titleH = 24.0f;

        if (newControls) {
            // Compute rows
            int nMoveHD = (int)dims >= 3 ? (int)dims - 3 : 0;
            int nRotHD = 0;
            for (int a = 3; a < (int)dims; a++)
                for (int b = a + 1; b < (int)dims; b++)
                    nRotHD++;
            int nRows = nMoveHD + nRotHD;
            if (nMoveHD > 0 && nRotHD > 0) nRows++; // section gap
            float panelH = PAD * 2 + titleH + nRows * LEFT_ROW_H + 10.0f;

            drawRect(uiProgram, uiVAO, uiVBO,
                     panelLeft, panelTop, PANEL_WIDTH, panelH,
                     0.12f, 0.12f, 0.18f, 0.92f, (float)fbW, (float)fbH);
            drawRect(uiProgram, uiVAO, uiVBO,
                     panelLeft, panelTop, PANEL_WIDTH, 1.0f,
                     0.3f, 0.3f, 0.5f, 0.8f, (float)fbW, (float)fbH);
            drawRect(uiProgram, uiVAO, uiVBO,
                     panelLeft, panelTop + panelH - 1, PANEL_WIDTH, 1.0f,
                     0.3f, 0.3f, 0.5f, 0.8f, (float)fbW, (float)fbH);

            drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
                       panelLeft + PAD, panelTop + PAD, "HD Controls",
                       (float)fbW, (float)fbH, dtIndices.data(), TEXT_MAX_QUADS);

            int row = 0;
            float rowY0 = panelTop + PAD + titleH + PAD;
            float btnX0 = panelLeft + PAD;
            float btnX1 = panelLeft + PANEL_WIDTH - PAD - BTN_SZ;
            float labelX0 = btnX0 + BTN_SZ + PAD;

            char ctrlLabel[32];

            // Movement rows (dim >= 3)
            for (int d = 3; d < (int)dims; d++, row++) {
                float ry = rowY0 + row * LEFT_ROW_H;
                // [-] button
                drawRect(uiProgram, uiVAO, uiVBO,
                         btnX0, ry + (LEFT_ROW_H - BTN_SZ) / 2, BTN_SZ, BTN_SZ,
                         0.35f, 0.2f, 0.2f, 0.9f, (float)fbW, (float)fbH);
                drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
                           btnX0 + 5, ry + 2, "-",
                           (float)fbW, (float)fbH, dtIndices.data(), TEXT_MAX_QUADS);
                // label
                snprintf(ctrlLabel, sizeof(ctrlLabel), "Move Dim %d", d);
                drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
                           labelX0, ry, ctrlLabel,
                           (float)fbW, (float)fbH, dtIndices.data(), TEXT_MAX_QUADS);
                // [+] button
                drawRect(uiProgram, uiVAO, uiVBO,
                         btnX1, ry + (LEFT_ROW_H - BTN_SZ) / 2, BTN_SZ, BTN_SZ,
                         0.2f, 0.35f, 0.2f, 0.9f, (float)fbW, (float)fbH);
                drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
                           btnX1 + 5, ry + 2, "+",
                           (float)fbW, (float)fbH, dtIndices.data(), TEXT_MAX_QUADS);
            }

            // Section separator label
            if (nMoveHD > 0 && nRotHD > 0) {
                float ry = rowY0 + row * LEFT_ROW_H;
                drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
                           labelX0, ry, "---",
                           (float)fbW, (float)fbH, dtIndices.data(), TEXT_MAX_QUADS);
                row++;
            }

            // Rotation rows (planes with a >= 3)
            for (int a = 3; a < (int)dims; a++)
                for (int b = a + 1; b < (int)dims; b++, row++) {
                    float ry = rowY0 + row * LEFT_ROW_H;
                    drawRect(uiProgram, uiVAO, uiVBO,
                             btnX0, ry + (LEFT_ROW_H - BTN_SZ) / 2, BTN_SZ, BTN_SZ,
                             0.35f, 0.2f, 0.2f, 0.9f, (float)fbW, (float)fbH);
                    drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
                               btnX0 + 5, ry + 2, "-",
                               (float)fbW, (float)fbH, dtIndices.data(), TEXT_MAX_QUADS);
                    snprintf(ctrlLabel, sizeof(ctrlLabel), "Rotate (%d,%d)", a, b);
                    drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
                               labelX0, ry, ctrlLabel,
                               (float)fbW, (float)fbH, dtIndices.data(), TEXT_MAX_QUADS);
                    drawRect(uiProgram, uiVAO, uiVBO,
                             btnX1, ry + (LEFT_ROW_H - BTN_SZ) / 2, BTN_SZ, BTN_SZ,
                             0.2f, 0.35f, 0.2f, 0.9f, (float)fbW, (float)fbH);
                    drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
                               btnX1 + 5, ry + 2, "+",
                               (float)fbW, (float)fbH, dtIndices.data(), TEXT_MAX_QUADS);
                }
        } else {
            int nSliders = transform.planeCount();
            float panelH = PAD * 2 + titleH + nSliders * SLIDER_HEIGHT + 10.0f;

            drawRect(uiProgram, uiVAO, uiVBO,
                     panelLeft, panelTop, PANEL_WIDTH, panelH,
                     0.12f, 0.12f, 0.18f, 0.92f, (float)fbW, (float)fbH);
            drawRect(uiProgram, uiVAO, uiVBO,
                     panelLeft, panelTop, PANEL_WIDTH, 1.0f,
                     0.3f, 0.3f, 0.5f, 0.8f, (float)fbW, (float)fbH);
            drawRect(uiProgram, uiVAO, uiVBO,
                     panelLeft, panelTop + panelH - 1, PANEL_WIDTH, 1.0f,
                     0.3f, 0.3f, 0.5f, 0.8f, (float)fbW, (float)fbH);

            char titleStr[64];
            snprintf(titleStr, sizeof(titleStr), "%uD Rotations", dims);

            drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
                       panelLeft + PAD, panelTop + PAD, titleStr,
                       (float)fbW, (float)fbH, dtIndices.data(), TEXT_MAX_QUADS);

            float toggleX = panelLeft + PAD;
            float labelX = toggleX + TOGGLE_SIZE + PAD;
            float trackX = labelX + LABEL_WIDTH + PAD;
            float trackW = PANEL_WIDTH - (trackX - panelLeft) - VALUE_WIDTH - PAD;

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

                float val = transform.modelAngles[i];
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
        }

        // Model info + buttons panel (right side)
        {
            float infoX = (float)fbW - 260.0f;
            float infoY = 10.0f;
            float infoH = 280.0f;
            drawRect(uiProgram, uiVAO, uiVBO,
                     infoX, infoY, 250.0f, infoH,
                     0.12f, 0.12f, 0.18f, 0.92f, (float)fbW, (float)fbH);

            char camStr[128];
            int off = snprintf(camStr, sizeof(camStr), "Camera: (");
            for (unsigned int d = 0; d < dims && d < 4 && off < (int)sizeof(camStr) - 20; d++)
                off += snprintf(camStr + off, sizeof(camStr) - off,
                                "%s%.2f", d > 0 ? ", " : "", -transform.translation[d]);
            if (off < (int)sizeof(camStr) - 6)
                snprintf(camStr + off, sizeof(camStr) - off, "%s)",
                         dims > 4 ? ", ..." : "");

            char infoLines[512];
            snprintf(infoLines, sizeof(infoLines),
                      "%uD Model\n"
                      "Vertices: %u\n"
                      "Triangles: %u\n"
                      "Edges: %zu\n"
                      "Planes: %d\n"
                      "Focal Length: %.1f\n"
                      "Color Scheme: %s\n"
                      "Wireframe: %s\n"
                      "Lighting: %s\n"
                      "Mode: %s\n"
                      "%s",
                      dims, model.vertexCount, model.indexCount / 3,
                      edges.size(), transform.planeCount(),
                      focalLength,
                       colorSchemeNames[colorScheme],
                      wireframeOnly ? "ON" : "OFF",
                      lighting ? "ON" : "OFF",
                      renderModeNames[renderMode],
                      camStr);

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
                                        "Screenshot", "Lighting"};
            int btnCols = 3;
            for (int b = 0; b < BTN_COUNT; b++) {
                int col = b % btnCols;
                int row = b / btnCols;
                int bx = btnStartX + col * (btnW + colGap);
                int by = btnStartY + row * (btnH + gap);
                bool hovered = (mx_fb >= bx && mx_fb <= bx + btnW &&
                                my_fb >= by && my_fb <= by + btnH);
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
            drawTextAt(dtVAO, dtVBO, dtEBO, textProgram,
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
