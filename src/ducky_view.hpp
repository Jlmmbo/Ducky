#pragma once

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QTimer>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QOpenGLContext>

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

constexpr float ROTATE_SPEED = 0.02f;
constexpr float MOVE_SPEED = 2.0f;
constexpr float AXIS_LENGTH = 1.5f;
constexpr int EDGE_SUBDIV = 32;

class DuckyView : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    DuckyView(const char* modelPath, QWidget* parent = nullptr);
    ~DuckyView();

    TransformND& transform() { return m_transform; }
    Model& model() { return m_model; }
    bool wireframe() const { return m_wireframeOnly; }
    int colorScheme() const { return m_colorScheme; }
    float focalLength() const { return m_focalLength; }
    int renderMode() const { return m_renderMode; }
    bool lighting() const { return m_lighting; }
    bool transparent() const { return m_transparent; }
    float modelAlpha() const { return m_modelAlpha; }
    unsigned int dimensions() const { return m_dims; }
    const std::vector<Edge>& edges() const { return m_edges; }

    void setWireframe(bool on);
    void setColorScheme(int scheme);
    void setRenderMode(int mode);
    void setFocalLength(float fl);
    void setLighting(bool on);
    void setTransparent(bool on);
    void resetTransform();
    void saveState(const char* path);
    void loadState(const char* path);
    void takeScreenshot();
    void setModelAlpha(float a) { m_modelAlpha = a; }

signals:
    void stateChanged();
    void fpsUpdated(float fps);
    void fullscreenRequested();
    void controlsModeChanged(bool newControls);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* e) override;

private slots:
    void tick();

private:
    void processInput(float dt);
    void toggleFullscreen();

    Model m_model;
    std::vector<float> m_modelVertsBackup;
    unsigned int m_dims = 0;
    int m_fpv = 0;

    TransformND m_transform;
    MouseState m_mouse;

    GLuint m_tessVAO = 0, m_tessVBO = 0, m_tessEBO = 0;
    GLuint m_tessProgram = 0;
    GLuint m_tessUAspect = 0, m_tessUDist3D = 0, m_tessUAlpha = 0;
    GLuint m_tessULighting = 0, m_tessURenderMode = 0;

    GLuint m_axesVAO = 0, m_axesVBO = 0;
    GLuint m_axesProgram = 0;
    GLuint m_axesUAspect = 0, m_axesUDist3D = 0, m_axesURenderMode = 0;
    std::vector<float> m_axisR, m_axisG, m_axisB;

    GLuint m_edgeVAO = 0, m_edgeVBO = 0;
    GLuint m_edgeProgram = 0;
    GLuint m_edgeUAspect = 0, m_edgeUDist3D = 0, m_edgeURenderMode = 0;
    std::vector<Edge> m_edges;

    GLuint m_subVAO = 0, m_subVBO = 0, m_subEBO = 0;

    std::vector<float> m_projectedVerts;
    std::vector<float> m_rotatedND;
    std::vector<float> m_axis3D;
    std::vector<float> m_edge3D;
    std::vector<float> m_subVerts;
    std::vector<unsigned int> m_subIdx;

    bool m_showPerformance = false;
    bool m_wireframeOnly = false;
    int m_colorScheme = 0;
    int m_rotPreset = 1;
    float m_focalLength = 1.0f;
    int m_renderMode = 0;
    bool m_isFullscreen = false;
    bool m_orbitMode = false;
    bool m_transparent = false;
    bool m_lighting = true;
    float m_modelAlpha = 0.35f;
    bool m_newControls = true;

    int m_windowedX = 0, m_windowedY = 0;
    int m_windowedW = 0, m_windowedH = 0;

    QTimer* m_timer = nullptr;
    QElapsedTimer m_elapsed;
    float m_lastTime = 0;
    double m_perfLastTime = 0;
    int m_perfFrameCount = 0;
    float m_perfFps = 0.0f;
};
