#include "ducky/application.hpp"
#include "ducky/io.hpp"
#include "stb_easy_font.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>

#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

namespace dky {

static void processInput(GLFWwindow* window, TransformND& t) {
    constexpr float ROTATE_SPEED = 0.02f;
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
        std::fill(t.translation.begin(), t.translation.end(), 0.0f);
    }
}

Application::Application(int argc, char* argv[])
    : textBuffer_(20000) {
#ifndef _WIN32
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    windowedW_ = 1920;
    windowedH_ = 1920;
    window_ = glfwCreateWindow(windowedW_, windowedH_, "Ducky", nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(window_);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return;
    }

    glfwSwapInterval(1);
    glEnable(GL_DEPTH_TEST);

    const char* modelPath = argc > 1 ? argv[1] : "model.dky";
    model_ = LoadModel(modelPath);
    if (model_.vertexCount == 0) {
        std::cerr << "Failed to load model: " << modelPath << "\n";
        glfwTerminate();
        return;
    }
    std::cout << "Loaded " << model_.dimensions << "D model: "
              << model_.vertexCount << " vertices, " << model_.indexCount << " indices" << std::endl;

    dims_ = model_.dimensions;
    fpv_ = dims_ + 3;
    modelVertsBackup_ = model_.vertices;

    edges_ = generateEdges(model_.vertices.data(), model_.vertexCount, dims_, fpv_,
                           model_.indices.data(), model_.indexCount);
    std::cout << "Generated " << edges_.size() << " edges" << std::endl;

    renderer_ = new Renderer(model_, edges_);

    transform_.dims = dims_;
    transform_.angles.resize(transform_.planeCount(), 0.0f);
    transform_.autoRotate.resize(transform_.planeCount(), true);
    transform_.translation.resize(dims_, 0.0f);

    projectedVerts_.resize(model_.vertexCount * 6);
    axis3D_.resize(dims_ * 2 * 6);
    edge3D_.resize(edges_.size() * (32 + 1) * 3);

    perfLastTime_ = glfwGetTime();
    lastTime_ = glfwGetTime();
}

Application::~Application() {
    delete renderer_;
    if (window_) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

void Application::takeScreenshot() {
    glfwGetFramebufferSize(window_, &fbW_, &fbH_);
    std::vector<unsigned char> pixels(fbW_ * fbH_ * 3);
    glReadPixels(0, 0, fbW_, fbH_, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    char screenshotPath[64];
    time_t now = time(nullptr);
    struct tm* tmNow = localtime(&now);
    snprintf(screenshotPath, sizeof(screenshotPath), "screenshot_%04d%02d%02d_%02d%02d%02d.tga",
             tmNow->tm_year + 1900, tmNow->tm_mon + 1, tmNow->tm_mday,
             tmNow->tm_hour, tmNow->tm_min, tmNow->tm_sec);
    writeTGA(screenshotPath, fbW_, fbH_, pixels.data());
    std::cout << "Screenshot saved: " << screenshotPath << std::endl;
}

void Application::toggleFullscreen() {
    isFullscreen_ = !isFullscreen_;
    if (isFullscreen_) {
        glfwGetWindowPos(window_, &windowedX_, &windowedY_);
        glfwGetWindowSize(window_, &windowedW_, &windowedH_);
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        fullscreenW_ = mode->width;
        fullscreenH_ = mode->height;
        glfwSetWindowMonitor(window_, monitor, 0, 0, fullscreenW_, fullscreenH_, mode->refreshRate);
    } else {
        GLFWmonitor* mon = glfwGetPrimaryMonitor();
        int monX, monY, monW, monH;
        glfwGetMonitorWorkarea(mon, &monX, &monY, &monW, &monH);
        int restoreX = std::max(monX, std::min(windowedX_, monX + monW - 100));
        int restoreY = std::max(monY, std::min(windowedY_, monY + monH - 100));
        int restoreW = std::max(100, std::min(windowedW_, monW));
        int restoreH = std::max(100, std::min(windowedH_, monH));
        glfwSetWindowMonitor(window_, nullptr, restoreX, restoreY, restoreW, restoreH, 0);
    }
}

int Application::run() {
    if (!window_ || model_.vertexCount == 0) return -1;

    while (!glfwWindowShouldClose(window_)) {
        handleMouseInput();
        processInput(window_, transform_);
        handleKeyboardShortcuts();
        handleOrbit();

        // Auto-rotation with delta time
        {
            double now = glfwGetTime();
            float dt = std::min((float)(now - lastTime_), 0.05f);
            lastTime_ = now;

            for (int i = 0; i < transform_.planeCount(); i++) {
                if (transform_.autoRotate[i])
                    transform_.angles[i] += dt * 0.5f * (1 + (i % 3));
            }
        }

        // Wrap angles
        for (int i = 0; i < transform_.planeCount(); i++) {
            float a = transform_.angles[i];
            transform_.angles[i] = fmodf(a + PI, 2.0f * PI);
            if (transform_.angles[i] < 0) transform_.angles[i] += 2.0f * PI;
            transform_.angles[i] -= PI;
        }

        glfwGetFramebufferSize(window_, &fbW_, &fbH_);
        renderer_->setFramebufferSize(fbW_, fbH_);
        aspect_ = (float)fbW_ / (float)fbH_;

        char titleBuf[128];
        snprintf(titleBuf, sizeof(titleBuf), "Ducky - %uD (%u verts, %zu edges) [F1=perf]",
                 dims_, model_.vertexCount, edges_.size());
        glfwSetWindowTitle(window_, titleBuf);

        handleSliders();
        processProjection();

        // Render
        renderer_->setFramebufferSize(fbW_, fbH_);
        renderer_->clear();
        renderer_->renderFaces(model_, transform_,
                               projectedVerts_.data(), model_.vertexCount,
                               model_.indices.data(), model_.indexCount,
                               renderMode_, wireframeOnly_,
                               focalLength_, lighting_,
                               transparent_, modelAlpha_);
        renderer_->renderAxes(transform_, focalLength_);
        renderer_->renderEdges(model_, transform_,
                               projectedVerts_.data(),
                               renderMode_, focalLength_, edges_);

        // UI overlay
        glDisable(GL_DEPTH_TEST);
        drawUI();
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window_);
        glfwPollEvents();
    }

    return 0;
}

void Application::handleMouseInput() {
    double mx, my;
    glfwGetCursorPos(window_, &mx, &my);
    bool leftNow = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool rightNow = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    mouse_.leftPressed = leftNow && !mouse_.left;
    mouse_.leftReleased = !leftNow && mouse_.left;
    mouse_.left = leftNow;
    mouse_.rightPressed = rightNow && !mouse_.right;
    mouse_.rightReleased = !rightNow && mouse_.right;
    mouse_.right = rightNow;
    mouse_.moved = (mx != mouse_.x || my != mouse_.y);
    mouse_.lastX = mouse_.x;
    mouse_.lastY = mouse_.y;
    mouse_.x = mx;
    mouse_.y = my;
}

void Application::handleKeyboardShortcuts() {
    bool ctrl = glfwGetKey(window_, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                glfwGetKey(window_, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;

    // Save/Load state
    {
        static bool sPrev = false, lPrev = false;
        bool sNow = glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS;
        bool lNow = glfwGetKey(window_, GLFW_KEY_L) == GLFW_PRESS;
        if (ctrl && sNow && !sPrev) saveState("ducky_state.txt", transform_);
        if (ctrl && lNow && !lPrev) loadState("ducky_state.txt", transform_);
        sPrev = sNow;
        lPrev = lNow;
    }

    // Fullscreen (F11)
    {
        static bool f11Prev = false;
        bool f11Now = glfwGetKey(window_, GLFW_KEY_F11) == GLFW_PRESS;
        if (f11Now && !f11Prev) toggleFullscreen();
        f11Prev = f11Now;
    }

    // Screenshot (F12)
    {
        static bool f12Prev = false;
        bool f12Now = glfwGetKey(window_, GLFW_KEY_F12) == GLFW_PRESS;
        if (f12Now && !f12Prev) takeScreenshot();
        f12Prev = f12Now;
    }

    // Performance overlay (F1)
    {
        static bool f1Prev = false;
        bool f1Now = glfwGetKey(window_, GLFW_KEY_F1) == GLFW_PRESS;
        if (f1Now && !f1Prev) showPerformance_ = !showPerformance_;
        f1Prev = f1Now;
    }

    // Wireframe toggle (E, no ctrl)
    {
        static bool ePrev = false;
        bool eNow = glfwGetKey(window_, GLFW_KEY_E) == GLFW_PRESS;
        if (eNow && !ePrev && !ctrl) wireframeOnly_ = !wireframeOnly_;
        ePrev = eNow;
    }

    // Toggle all autorotate (A)
    {
        static bool aPrev = false;
        bool aNow = glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS;
        if (aNow && !aPrev) {
            bool anyOn = false;
            for (auto v : transform_.autoRotate) if (v) anyOn = true;
            std::fill(transform_.autoRotate.begin(), transform_.autoRotate.end(), !anyOn);
        }
        aPrev = aNow;
    }

    // Transparency (T)
    {
        static bool tPrev = false;
        bool tNow = glfwGetKey(window_, GLFW_KEY_T) == GLFW_PRESS;
        if (tNow && !tPrev && !ctrl) transparent_ = !transparent_;
        tPrev = tNow;
    }

    // Lighting toggle (L)
    {
        static bool lPrev = false;
        bool lNow = glfwGetKey(window_, GLFW_KEY_L) == GLFW_PRESS;
        if (lNow && !lPrev && !ctrl) lighting_ = !lighting_;
        lPrev = lNow;
    }

    // Color scheme cycle (C)
    {
        static bool cPrev = false;
        bool cNow = glfwGetKey(window_, GLFW_KEY_C) == GLFW_PRESS;
        if (cNow && !cPrev && !ctrl) {
            colorScheme_ = (colorScheme_ + 1) % 5;
            if (colorScheme_ == 0) {
                model_.vertices = modelVertsBackup_;
            } else {
                assignFaceColors(model_, colorScheme_ - 1);
            }
        }
        cPrev = cNow;
    }

    // Render mode cycle (M)
    {
        static bool mPrev = false;
        bool mNow = glfwGetKey(window_, GLFW_KEY_M) == GLFW_PRESS;
        if (mNow && !mPrev) renderMode_ = (renderMode_ + 1) % 3;
        mPrev = mNow;
    }

    // Auto-rotation preset (V)
    {
        static bool vPrev = false;
        bool vNow = glfwGetKey(window_, GLFW_KEY_V) == GLFW_PRESS;
        if (vNow && !vPrev) {
            rotPreset_ = (rotPreset_ + 1) % 4;
            switch (rotPreset_) {
                case 0: std::fill(transform_.autoRotate.begin(), transform_.autoRotate.end(), false); break;
                case 1: std::fill(transform_.autoRotate.begin(), transform_.autoRotate.end(), true); break;
                case 2: for (int i = 0; i < transform_.planeCount(); i++) transform_.autoRotate[i] = (i % 2 == 0); break;
                case 3: for (int i = 0; i < transform_.planeCount(); i++) transform_.autoRotate[i] = (i % 3 == 0); break;
            }
        }
        vPrev = vNow;
    }

    // Focal length [ and ]
    {
        static bool leftBracketPrev = false, rightBracketPrev = false;
        bool leftBracketNow = glfwGetKey(window_, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS;
        bool rightBracketNow = glfwGetKey(window_, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
        if (leftBracketNow && !leftBracketPrev && !ctrl) focalLength_ = std::max(0.1f, focalLength_ - 0.1f);
        if (rightBracketNow && !rightBracketPrev && !ctrl) focalLength_ = std::min(5.0f, focalLength_ + 0.1f);
        leftBracketPrev = leftBracketNow;
        rightBracketPrev = rightBracketNow;
    }
}

void Application::handleOrbit() {
    constexpr float PANEL_WIDTH = 290.0f;
    constexpr float SLIDER_HEIGHT = 28.0f;
    constexpr float PAD = 6.0f;

    if (mouse_.rightPressed) {
        int nSliders = transform_.planeCount();
        float panelH = PAD * 2.0f + 24.0f + (float)nSliders * SLIDER_HEIGHT + 10.0f;
        bool overSlider = mouse_.x >= 10.0f && mouse_.x <= 10.0f + PANEL_WIDTH &&
                          mouse_.y >= 10.0f && mouse_.y <= 10.0f + panelH;
        if (!overSlider && !mouse_.left)
            orbitMode_ = true;
    }
    if (mouse_.rightReleased) orbitMode_ = false;
    if (orbitMode_ && mouse_.right && mouse_.moved) {
        double dx = mouse_.x - mouse_.lastX;
        double dy = mouse_.y - mouse_.lastY;
        if (dims_ >= 3) {
            transform_.angles[1] -= (float)dx * 0.005f;
            transform_.angles[dims_ - 1] += (float)dy * 0.005f;
        } else if (transform_.planeCount() >= 1) {
            transform_.angles[0] += (float)dx * 0.005f;
        }
    }
}

void Application::handleSliders() {
    constexpr float PANEL_WIDTH = 290.0f;
    constexpr float SLIDER_HEIGHT = 28.0f;
    constexpr float TOGGLE_SIZE = 14.0f;
    constexpr float LABEL_WIDTH = 36.0f;
    constexpr float VALUE_WIDTH = 55.0f;
    constexpr float PAD = 6.0f;

    int nSliders = transform_.planeCount();
    float panelTop = 10.0f;
    float panelLeft = 10.0f;
    float titleH = 24.0f;
    float toggleX = panelLeft + PAD;
    float labelX = toggleX + TOGGLE_SIZE + PAD;
    float trackX = labelX + LABEL_WIDTH + PAD;
    float trackW = PANEL_WIDTH - (trackX - panelLeft) - VALUE_WIDTH - PAD;

    constexpr float PI = 3.141592653589793f;

    if (mouse_.leftPressed) {
        bool sliderHit = false;
        for (int i = 0; i < nSliders; i++) {
            float rowY = panelTop + PAD + titleH + PAD + i * SLIDER_HEIGHT;
            float tglY = rowY + (SLIDER_HEIGHT - TOGGLE_SIZE) / 2;
            if (mouse_.x >= toggleX && mouse_.x <= toggleX + TOGGLE_SIZE &&
                mouse_.y >= tglY && mouse_.y <= tglY + TOGGLE_SIZE) {
                transform_.autoRotate[i] = !transform_.autoRotate[i];
                sliderHit = true;
                break;
            }
        }
        if (!sliderHit) {
            dragSlider_ = -1;
            for (int i = 0; i < nSliders; i++) {
                float rowY = panelTop + PAD + titleH + PAD + i * SLIDER_HEIGHT;
                if (mouse_.x >= trackX && mouse_.x <= trackX + trackW &&
                    mouse_.y >= rowY && mouse_.y <= rowY + SLIDER_HEIGHT) {
                    dragSlider_ = i;
                    float t = (float)((mouse_.x - trackX) / trackW);
                    t = std::max(0.0f, std::min(1.0f, t));
                    transform_.angles[i] = -PI + t * (2.0f * PI);
                    break;
                }
            }
        }
    }

    if (dragSlider_ >= 0 && mouse_.left) {
        float t = (float)((mouse_.x - trackX) / trackW);
        t = std::max(0.0f, std::min(1.0f, t));
        transform_.angles[dragSlider_] = -PI + t * (2.0f * PI);
    }

    if (mouse_.leftReleased) {
        if (clickedButton_ >= 0) {
            switch (clickedButton_) {
                case BTN_RESET:
                    std::fill(transform_.angles.begin(), transform_.angles.end(), 0.0f);
                    std::fill(transform_.translation.begin(), transform_.translation.end(), 0.0f);
                    break;
                case BTN_WIREFRAME: wireframeOnly_ = !wireframeOnly_; break;
                case BTN_COLOR:
                    colorScheme_ = (colorScheme_ + 1) % 5;
                    if (colorScheme_ == 0) model_.vertices = modelVertsBackup_;
                    else assignFaceColors(model_, colorScheme_ - 1);
                    break;
                case BTN_PRESET:
                    rotPreset_ = (rotPreset_ + 1) % 4;
                    switch (rotPreset_) {
                        case 0: std::fill(transform_.autoRotate.begin(), transform_.autoRotate.end(), false); break;
                        case 1: std::fill(transform_.autoRotate.begin(), transform_.autoRotate.end(), true); break;
                        case 2: for (int i = 0; i < transform_.planeCount(); i++) transform_.autoRotate[i] = (i % 2 == 0); break;
                        case 3: for (int i = 0; i < transform_.planeCount(); i++) transform_.autoRotate[i] = (i % 3 == 0); break;
                    }
                    break;
                case BTN_FOCAL_DOWN: focalLength_ = std::max(0.1f, focalLength_ - 0.1f); break;
                case BTN_FOCAL_UP: focalLength_ = std::min(5.0f, focalLength_ + 0.1f); break;
                case BTN_MODE: renderMode_ = (renderMode_ + 1) % 3; break;
                case BTN_FS: toggleFullscreen(); break;
                case BTN_SAVE: saveState("ducky_state.txt", transform_); break;
                case BTN_LOAD: loadState("ducky_state.txt", transform_); break;
                case BTN_SHOT: takeScreenshot(); break;
                case BTN_LIGHTING: lighting_ = !lighting_; break;
            }
        }
        dragSlider_ = -1;
        clickedButton_ = -1;
    }

    if (mouse_.leftPressed && clickedButton_ < 0) {
        int btnW = 76, btnH = 24, gap = 4, colGap = 5;
        int btnStartX = (int)((float)fbW_ - 260.0f + 8);
        int btnStartY = (int)(10.0f + 145);
        int btnCols = 3;
        for (int b = 0; b < BTN_COUNT; b++) {
            int col = b % btnCols;
            int row = b / btnCols;
            int bx = btnStartX + col * (btnW + colGap);
            int by = btnStartY + row * (btnH + gap);
            if (mouse_.x >= bx && mouse_.x <= bx + btnW &&
                mouse_.y >= by && mouse_.y <= by + btnH) {
                clickedButton_ = b;
                break;
            }
        }
    }

    hoverSlider_ = -1;
    for (int i = 0; i < nSliders; i++) {
        float rowY = panelTop + PAD + titleH + PAD + i * SLIDER_HEIGHT;
        if (mouse_.x >= trackX && mouse_.x <= trackX + trackW &&
            mouse_.y >= rowY && mouse_.y <= rowY + SLIDER_HEIGHT)
            hoverSlider_ = i;
    }
}

void Application::processProjection() {
    float* pos = (float*)alloca(dims_ * sizeof(float));
    for (unsigned int i = 0; i < model_.vertexCount; i++) {
        for (unsigned int d = 0; d < dims_; d++)
            pos[d] = model_.vertices[i * fpv_ + d];
        for (unsigned int d = 0; d < dims_; d++)
            pos[d] += transform_.translation[d];
        applyRotation(pos, transform_);
        switch (renderMode_) {
            case 0: projectPerspective(pos, &projectedVerts_[i * 6], dims_, focalLength_); break;
            case 1: projectStereographic(pos, &projectedVerts_[i * 6], dims_, focalLength_); break;
            default: projectOrthographic(pos, &projectedVerts_[i * 6], dims_); break;
        }
        projectedVerts_[i * 6 + 3] = model_.vertices[i * fpv_ + dims_];
        projectedVerts_[i * 6 + 4] = model_.vertices[i * fpv_ + dims_ + 1];
        projectedVerts_[i * 6 + 5] = model_.vertices[i * fpv_ + dims_ + 2];
    }
}

void Application::drawUI() {
    constexpr float PANEL_WIDTH = 290.0f;
    constexpr float SLIDER_HEIGHT = 28.0f;
    constexpr float TOGGLE_SIZE = 14.0f;
    constexpr float LABEL_WIDTH = 36.0f;
    constexpr float VALUE_WIDTH = 55.0f;
    constexpr float PAD = 6.0f;

    auto& r = *renderer_;
    int nSliders = transform_.planeCount();
    float panelTop = 10.0f;
    float panelLeft = 10.0f;
    float titleH = 24.0f;
    float panelH = PAD * 2 + titleH + nSliders * SLIDER_HEIGHT + 10.0f;

    // Slider panel background
    drawRect(r.uiProgram(), r.uiVAO(), r.uiVBO(),
             panelLeft, panelTop, PANEL_WIDTH, panelH,
             0.12f, 0.12f, 0.18f, 0.92f, (float)fbW_, (float)fbH_);

    // Border lines
    drawRect(r.uiProgram(), r.uiVAO(), r.uiVBO(),
             panelLeft, panelTop, PANEL_WIDTH, 1.0f,
             0.3f, 0.3f, 0.5f, 0.8f, (float)fbW_, (float)fbH_);
    drawRect(r.uiProgram(), r.uiVAO(), r.uiVBO(),
             panelLeft, panelTop + panelH - 1, PANEL_WIDTH, 1.0f,
             0.3f, 0.3f, 0.5f, 0.8f, (float)fbW_, (float)fbH_);

    char titleStr[64];
    snprintf(titleStr, sizeof(titleStr), "%uD Rotations", dims_);

    float toggleX = panelLeft + PAD;
    float labelX = toggleX + TOGGLE_SIZE + PAD;
    float trackX = labelX + LABEL_WIDTH + PAD;
    float trackW = PANEL_WIDTH - (trackX - panelLeft) - VALUE_WIDTH - PAD;

    char label[16];
    char valueStr[16];
    for (int i = 0; i < nSliders; i++) {
        float rowY = panelTop + PAD + titleH + PAD + i * SLIDER_HEIGHT;

        bool autoOn = transform_.autoRotate[i];
        drawRect(r.uiProgram(), r.uiVAO(), r.uiVBO(),
                 toggleX, rowY + (SLIDER_HEIGHT - TOGGLE_SIZE) / 2,
                 TOGGLE_SIZE, TOGGLE_SIZE,
                 autoOn ? 0.2f : 0.15f, autoOn ? 0.7f : 0.15f, autoOn ? 0.2f : 0.2f, 0.9f,
                 (float)fbW_, (float)fbH_);
        float tglY = rowY + (SLIDER_HEIGHT - TOGGLE_SIZE) / 2;
        char toggleLabel[2] = {autoOn ? 'A' : 'M', '\0'};
        drawTextAt(r.dtVAO(), r.dtVBO(), r.dtEBO(), r.textProgram(),
                   toggleX + 4, tglY + 3, toggleLabel,
                   (float)fbW_, (float)fbH_, r.dtIndices(), r.textMaxQuads());

        int pi = 0;
        int ai = -1, aj = -1;
        for (int a = 0; a < (int)dims_ && pi <= i; a++)
            for (int b = a + 1; b < (int)dims_ && pi <= i; b++, pi++)
                if (pi == i) { ai = a; aj = b; }
        snprintf(label, sizeof(label), "(%d,%d)", ai, aj);

        drawRect(r.uiProgram(), r.uiVAO(), r.uiVBO(),
                 trackX, rowY, trackW, SLIDER_HEIGHT,
                 0.2f, 0.2f, 0.3f, 0.9f, (float)fbW_, (float)fbH_);

        float val = transform_.angles[i];
        float fillFrac = (val + PI) / (2.0f * PI);
        fillFrac = std::max(0.0f, std::min(1.0f, fillFrac));
        drawRect(r.uiProgram(), r.uiVAO(), r.uiVBO(),
                 trackX, rowY, trackW * fillFrac, SLIDER_HEIGHT,
                 0.35f, 0.5f, 0.9f, 0.8f, (float)fbW_, (float)fbH_);

        snprintf(valueStr, sizeof(valueStr), "%.2f", val);
        drawTextAt(r.dtVAO(), r.dtVBO(), r.dtEBO(), r.textProgram(),
                   trackX + trackW + PAD, rowY, valueStr,
                   (float)fbW_, (float)fbH_, r.dtIndices(), r.textMaxQuads());
        drawTextAt(r.dtVAO(), r.dtVBO(), r.dtEBO(), r.textProgram(),
                   labelX, rowY, label,
                   (float)fbW_, (float)fbH_, r.dtIndices(), r.textMaxQuads());
    }

    // Title
    drawTextAt(r.dtVAO(), r.dtVBO(), r.dtEBO(), r.textProgram(),
               panelLeft + PAD, panelTop + PAD, titleStr,
               (float)fbW_, (float)fbH_, r.dtIndices(), r.textMaxQuads());

    // Right info panel
    {
        float infoX = (float)fbW_ - 260.0f;
        float infoY = 10.0f;
        float infoH = 280.0f;
        drawRect(r.uiProgram(), r.uiVAO(), r.uiVBO(),
                 infoX, infoY, 250.0f, infoH,
                 0.12f, 0.12f, 0.18f, 0.92f, (float)fbW_, (float)fbH_);

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
                 "Lighting: %s\n"
                 "Mode: %s",
                 dims_, model_.vertexCount, model_.indexCount / 3,
                 edges_.size(), transform_.planeCount(),
                 focalLength_,
                 colorSchemeNames_[colorScheme_],
                 wireframeOnly_ ? "ON" : "OFF",
                 lighting_ ? "ON" : "OFF",
                 renderModeNames_[renderMode_]);

        std::string infoStr(infoLines);
        size_t pos = 0;
        float lineY = infoY + 8;
        while (pos < infoStr.size()) {
            size_t nl = infoStr.find('\n', pos);
            if (nl == std::string::npos) nl = infoStr.size();
            std::string line = infoStr.substr(pos, nl - pos);
            drawTextAt(r.dtVAO(), r.dtVBO(), r.dtEBO(), r.textProgram(),
                       infoX + 8, lineY, line.c_str(),
                       (float)fbW_, (float)fbH_, r.dtIndices(), r.textMaxQuads());
            lineY += 14;
            pos = nl + 1;
        }

        // Action buttons
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
            bool hovered = (mouse_.x >= bx && mouse_.x <= bx + btnW &&
                            mouse_.y >= by && mouse_.y <= by + btnH);
            drawRect(r.uiProgram(), r.uiVAO(), r.uiVBO(),
                     (float)bx, (float)by, (float)btnW, (float)btnH,
                     hovered ? 0.3f : 0.2f, hovered ? 0.3f : 0.2f, hovered ? 0.4f : 0.28f, 0.9f,
                     (float)fbW_, (float)fbH_);
            drawTextAt(r.dtVAO(), r.dtVBO(), r.dtEBO(), r.textProgram(),
                       (float)bx + 4, (float)by + 4, btnLabels[b],
                       (float)fbW_, (float)fbH_, r.dtIndices(), r.textMaxQuads());
        }
    }

    // Performance overlay
    if (showPerformance_) {
        perfFrameCount_++;
        double now = glfwGetTime();
        if (now - perfLastTime_ >= 0.5) {
            perfFps_ = perfFrameCount_ / (float)(now - perfLastTime_);
            perfFrameCount_ = 0;
            perfLastTime_ = now;
        }
        char perfStr[128];
        snprintf(perfStr, sizeof(perfStr), "FPS: %.1f  Verts: %u  Tris: %u",
                 perfFps_, model_.vertexCount, model_.indexCount / 3);
        drawTextAt(r.dtVAO(), r.dtVBO(), r.dtEBO(), r.textProgram(),
                   (float)fbW_ - 250.0f, (float)fbH_ - 30.0f, perfStr,
                   (float)fbW_, (float)fbH_, r.dtIndices(), r.textMaxQuads());
    }

    // HUD text hint
    {
        char hintText[256];
        snprintf(hintText, sizeof(hintText),
                 "%uD  |  Rot:1-0,-=  |  E=wireframe V=preset C=color A=autorotate "
                 "T=transparency L=lighting []=focal M=render R=reset  |  "
                 "F11=FS F12=shot F1=perf  |  Right panel has all controls", dims_);

        int hudNumQuads = stb_easy_font_print(PANEL_WIDTH + 20, 12, hintText,
                                              nullptr, textBuffer_.data(), (int)textBuffer_.size());
        glUseProgram(r.textProgram());
        glUniform2f(r.textScreenSizeLoc(), (float)fbW_, (float)fbH_);
        glBindBuffer(GL_ARRAY_BUFFER, r.textVBO());
        glBufferSubData(GL_ARRAY_BUFFER, 0, hudNumQuads * 64, textBuffer_.data());
        glBindVertexArray(r.textVAO());
        glDrawElements(GL_TRIANGLES, hudNumQuads * 6, GL_UNSIGNED_INT, nullptr);
    }
}

} // namespace dky
