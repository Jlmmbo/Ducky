#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>
#include "ducky/core.hpp"
#include "ducky/renderer.hpp"

namespace dky {

class Application {
public:
    Application(int argc, char* argv[]);
    ~Application();
    int run();

private:
    void handleMouseInput();
    void handleKeyboardShortcuts();
    void handleOrbit();
    void handleSliders();
    void processProjection();
    void drawUI();
    void takeScreenshot();
    void toggleFullscreen();

    Model model_;
    std::vector<Edge> edges_;
    std::vector<float> modelVertsBackup_;
    unsigned int dims_;
    int fpv_;
    TransformND transform_;
    Renderer* renderer_ = nullptr;

    GLFWwindow* window_ = nullptr;
    bool showPerformance_ = false;
    bool wireframeOnly_ = false;
    int colorScheme_ = 0;
    const char* colorSchemeNames_[5] = {"Model", "Golden", "Rainbow", "Mono", "Warm"};
    int rotPreset_ = 1;
    float focalLength_ = 1.0f;
    int renderMode_ = 0;
    const char* renderModeNames_[3] = {"Perspective", "Stereographic", "Orthographic"};
    bool isFullscreen_ = false;
    bool orbitMode_ = false;
    bool transparent_ = false;
    bool lighting_ = true;
    float modelAlpha_ = 0.35f;

    int fullscreenW_ = 0, fullscreenH_ = 0;
    int windowedX_ = 0, windowedY_ = 0, windowedW_ = 1920, windowedH_ = 1920;

    MouseState mouse_ = {};
    int dragSlider_ = -1;
    int hoverSlider_ = -1;
    int clickedButton_ = -1;

    enum ButtonId {
        BTN_RESET, BTN_WIREFRAME, BTN_COLOR, BTN_PRESET,
        BTN_FOCAL_DOWN, BTN_FOCAL_UP, BTN_MODE,
        BTN_FS, BTN_SAVE, BTN_LOAD, BTN_SHOT, BTN_LIGHTING,
        BTN_COUNT
    };

    std::vector<float> projectedVerts_;
    std::vector<float> axis3D_;
    std::vector<float> edge3D_;

    double perfLastTime_ = 0;
    int perfFrameCount_ = 0;
    float perfFps_ = 0.0f;
    double lastTime_ = 0;

    std::vector<char> textBuffer_;
    int fbW_ = 0, fbH_ = 0;
    float aspect_ = 1.0f;
};

} // namespace dky
