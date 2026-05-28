#pragma once

#include <glad/glad.h>
#include <vector>
#include "ducky/core.hpp"

namespace dky {

void drawRect(GLuint program, GLuint vao, GLuint vbo,
              float x, float y, float w, float h,
              float r, float g, float b, float a,
              float screenW, float screenH);

int drawTextAt(GLuint vao, GLuint vbo, GLuint ebo, GLuint program,
               float x, float y, const char* text,
               float screenW, float screenH,
               const unsigned int* indices, int maxQuads);

class Renderer {
public:
    Renderer(const Model& model, const std::vector<Edge>& edges);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void setFramebufferSize(int w, int h) { fbW_ = w; fbH_ = h; }

    void clear();
    void renderFaces(const Model& model, const TransformND& transform,
                     const float* projectedVerts, unsigned int vertexCount,
                     const unsigned int* indices, unsigned int indexCount,
                     int renderMode, bool wireframeOnly,
                     float focalLength, bool lighting,
                     bool transparent, float modelAlpha);
    void renderAxes(const TransformND& transform, float focalLength);
    void renderEdges(const Model& model, const TransformND& transform,
                     const float* projectedVerts,
                     int renderMode, float focalLength, const std::vector<Edge>& edges);

    GLuint uiProgram() const { return uiProgram_; }
    GLuint uiVAO() const { return uiVAO_; }
    GLuint uiVBO() const { return uiVBO_; }
    GLuint textProgram() const { return textProgram_; }
    GLuint textVAO() const { return textVAO_; }
    GLuint textVBO() const { return textVBO_; }
    GLuint textEBO() const { return textEBO_; }
    GLuint dtVAO() const { return dtVAO_; }
    GLuint dtVBO() const { return dtVBO_; }
    GLuint dtEBO() const { return dtEBO_; }
    const unsigned int* dtIndices() const { return dtIndices_.data(); }
    int textMaxQuads() const { return TEXT_MAX_QUADS_; }
    GLuint textScreenSizeLoc() const { return textScreenSize_; }

private:
    void initShaders();
    void initBuffers(const Model& model, const std::vector<Edge>& edges);
    void initAxesColors(unsigned int dims);

    unsigned int dims_;
    int fbW_ = 0, fbH_ = 0;

    GLuint tessVAO_, tessVBO_, tessEBO_;
    GLuint tessProgram_;
    GLuint tessUAspect_, tessUDist3D_, tessUAlpha_, tessULighting_;

    GLuint axesVAO_, axesVBO_;
    GLuint axesProgram_;
    GLuint axesUAspect_, axesUDist3D_;
    std::vector<float> axisR_, axisG_, axisB_;

    GLuint edgeVAO_, edgeVBO_;
    GLuint edgeProgram_;
    GLuint edgeUAspect_, edgeUDist3D_;

    GLuint subVAO_, subVBO_, subEBO_;

    GLuint uiProgram_, uiVAO_, uiVBO_;
    GLuint textProgram_;
    GLuint textScreenSize_;
    GLuint textVAO_, textVBO_, textEBO_;
    GLuint dtVAO_, dtVBO_, dtEBO_;
    std::vector<unsigned int> dtIndices_;
    int TEXT_MAX_QUADS_ = 512;

    std::vector<float> edge3D_;
    std::vector<float> subVerts_;
    std::vector<unsigned int> subIdx_;
    int EDGE_SUBDIV_ = 32;
};

} // namespace dky
