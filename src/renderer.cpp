#include "ducky/renderer.hpp"
#include "shader.hpp"

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

#include <algorithm>
#include <cstring>
#include <cmath>

#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

namespace dky {

// --- UI drawing helpers ---

void drawRect(GLuint program, GLuint vao, GLuint vbo,
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

int drawTextAt(GLuint vao, GLuint vbo, GLuint ebo, GLuint program,
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

// --- Renderer ---

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

Renderer::Renderer(const Model& model, const std::vector<Edge>& edges)
    : dims_(model.dimensions()) {
    initShaders();
    initBuffers(model, edges);
    initAxesColors(dims_);

    TEXT_MAX_QUADS_ = 512;
    dtIndices_.resize(TEXT_MAX_QUADS_ * 6);
    for (int i = 0; i < TEXT_MAX_QUADS_; i++) {
        int base = i * 4;
        dtIndices_[i * 6 + 0] = base;
        dtIndices_[i * 6 + 1] = base + 1;
        dtIndices_[i * 6 + 2] = base + 2;
        dtIndices_[i * 6 + 3] = base + 1;
        dtIndices_[i * 6 + 4] = base + 3;
        dtIndices_[i * 6 + 5] = base + 2;
    }
    glBindVertexArray(dtVAO_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, dtEBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, dtIndices_.size() * sizeof(unsigned int), dtIndices_.data(), GL_STATIC_DRAW);
}

Renderer::~Renderer() {
    glDeleteVertexArrays(1, &tessVAO_);
    glDeleteBuffers(1, &tessVBO_);
    glDeleteBuffers(1, &tessEBO_);
    glDeleteVertexArrays(1, &axesVAO_);
    glDeleteBuffers(1, &axesVBO_);
    glDeleteVertexArrays(1, &subVAO_);
    glDeleteBuffers(1, &subVBO_);
    glDeleteBuffers(1, &subEBO_);
    glDeleteVertexArrays(1, &edgeVAO_);
    glDeleteBuffers(1, &edgeVBO_);
    glDeleteProgram(edgeProgram_);
    glDeleteVertexArrays(1, &textVAO_);
    glDeleteBuffers(1, &textVBO_);
    glDeleteBuffers(1, &textEBO_);
    glDeleteVertexArrays(1, &dtVAO_);
    glDeleteBuffers(1, &dtVBO_);
    glDeleteBuffers(1, &dtEBO_);
    glDeleteVertexArrays(1, &uiVAO_);
    glDeleteBuffers(1, &uiVBO_);
    glDeleteProgram(tessProgram_);
    glDeleteProgram(axesProgram_);
    glDeleteProgram(textProgram_);
    glDeleteProgram(uiProgram_);
}

void Renderer::initShaders() {
    tessProgram_ = createShaderProgram("shaders/tesseract.vert", "shaders/tesseract.frag");
    if (!tessProgram_) { std::cerr << "Failed to load tess shaders\n"; return; }
    tessUAspect_ = glGetUniformLocation(tessProgram_, "uAspect");
    tessUDist3D_ = glGetUniformLocation(tessProgram_, "uDist3D");
    tessUAlpha_ = glGetUniformLocation(tessProgram_, "uAlpha");
    tessULighting_ = glGetUniformLocation(tessProgram_, "uLighting");

    axesProgram_ = createShaderProgram("shaders/axes.vert", "shaders/axes.frag");
    if (!axesProgram_) { std::cerr << "Failed to load axes shaders\n"; return; }
    axesUAspect_ = glGetUniformLocation(axesProgram_, "uAspect");
    axesUDist3D_ = glGetUniformLocation(axesProgram_, "uDist3D");

    edgeProgram_ = createShaderProgram("shaders/edge.vert", "shaders/edge.frag");
    if (!edgeProgram_) { std::cerr << "Failed to load edge shaders\n"; return; }
    edgeUAspect_ = glGetUniformLocation(edgeProgram_, "uAspect");
    edgeUDist3D_ = glGetUniformLocation(edgeProgram_, "uDist3D");

    uiProgram_ = createShaderProgramFromSrc(UI_VERT_SRC, UI_FRAG_SRC);
    if (!uiProgram_) { std::cerr << "Failed to create UI shaders\n"; return; }

    textProgram_ = createShaderProgram("shaders/text.vert", "shaders/text.frag");
    if (!textProgram_) { std::cerr << "Failed to load text shaders\n"; return; }
    textScreenSize_ = glGetUniformLocation(textProgram_, "uScreenSize");
}

void Renderer::initBuffers(const Model& model, const std::vector<Edge>& edges) {
    int fpv = dims_ + 3;

    glGenVertexArrays(1, &tessVAO_);
    glGenBuffers(1, &tessVBO_);
    glGenBuffers(1, &tessEBO_);

    glBindVertexArray(tessVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, tessVBO_);
    glBufferData(GL_ARRAY_BUFFER, model.vertexCount() * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tessEBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.indexCount() * sizeof(unsigned int), model.indices().data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &axesVAO_);
    glGenBuffers(1, &axesVBO_);

    glBindVertexArray(axesVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, axesVBO_);
    glBufferData(GL_ARRAY_BUFFER, dims_ * 2 * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &edgeVAO_);
    glGenBuffers(1, &edgeVBO_);

    glBindVertexArray(edgeVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, edgeVBO_);
    glBufferData(GL_ARRAY_BUFFER, edges.size() * (EDGE_SUBDIV_ + 1) * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    int maxVertsPerTri = (EDGE_SUBDIV_ + 1) * (EDGE_SUBDIV_ + 2) / 2;
    int maxTrisPerTri = EDGE_SUBDIV_ * EDGE_SUBDIV_;
    glGenVertexArrays(1, &subVAO_);
    glGenBuffers(1, &subVBO_);
    glGenBuffers(1, &subEBO_);
    glBindVertexArray(subVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, subVBO_);
    glBufferData(GL_ARRAY_BUFFER, (model.indexCount() / 3) * maxVertsPerTri * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subEBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (model.indexCount() / 3) * maxTrisPerTri * 3 * sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &uiVAO_);
    glGenBuffers(1, &uiVBO_);
    glBindVertexArray(uiVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO_);
    glBufferData(GL_ARRAY_BUFFER, 8 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glGenVertexArrays(1, &dtVAO_);
    glGenBuffers(1, &dtVBO_);
    glGenBuffers(1, &dtEBO_);
    glBindVertexArray(dtVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, dtVBO_);
    glBufferData(GL_ARRAY_BUFFER, TEXT_MAX_QUADS_ * 64, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void*)12);
    glEnableVertexAttribArray(1);

    glGenVertexArrays(1, &textVAO_);
    glGenBuffers(1, &textVBO_);
    glGenBuffers(1, &textEBO_);
    glBindVertexArray(textVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO_);
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
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, textEBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, hudIndices.size() * sizeof(unsigned int), hudIndices.data(), GL_STATIC_DRAW);
}

void Renderer::initAxesColors(unsigned int dims) {
    axisR_.resize(dims);
    axisG_.resize(dims);
    axisB_.resize(dims);
    for (unsigned int d = 0; d < dims; d++) {
        float h = (float)d / (float)dims;
        hslToRgb(h, 0.9f, 0.6f, axisR_[d], axisG_[d], axisB_[d]);
    }
}

void Renderer::clear() {
    glViewport(0, 0, fbW_, fbH_);
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::renderFaces(const Model& model, const TransformND& transform,
                           const float* projectedVerts, unsigned int vertexCount,
                           const unsigned int* indices, unsigned int indexCount,
                           int renderMode, bool wireframeOnly,
                           float focalLength, bool lighting,
                           bool transparent, float modelAlpha) {
    unsigned int numTri = indexCount / 3;
    float aspect = (float)fbW_ / (float)fbH_;
    int fpv = dims_ + 3;

    if (renderMode == 1) {
        // Stereographic: subdivide and project each triangle
        int n = EDGE_SUBDIV_;
        int vertsPerTri = (n + 1) * (n + 2) / 2;
        int trisPerTri = n * n;
        unsigned int totalSubTris = numTri * trisPerTri;
        unsigned int totalSubVerts = numTri * vertsPerTri;

        subVerts_.resize(totalSubVerts * 6);
        subIdx_.resize(totalSubTris * 3);
        struct TriDepth { int idx; float depth; };
        std::vector<TriDepth> subDepths(totalSubTris);

        float* tposA = (float*)alloca(dims_ * sizeof(float));
        float* tposB = (float*)alloca(dims_ * sizeof(float));
        float* tposC = (float*)alloca(dims_ * sizeof(float));
        float* interp = (float*)alloca(dims_ * sizeof(float));

        for (unsigned int t = 0; t < numTri; t++) {
            int ia = indices[t * 3];
            int ib = indices[t * 3 + 1];
            int ic = indices[t * 3 + 2];

            for (unsigned int d = 0; d < dims_; d++) {
                tposA[d] = model.vertexData()[ia * fpv + d] + transform.translation[d];
                tposB[d] = model.vertexData()[ib * fpv + d] + transform.translation[d];
                tposC[d] = model.vertexData()[ic * fpv + d] + transform.translation[d];
            }
            applyRotation(tposA, transform);
            applyRotation(tposB, transform);
            applyRotation(tposC, transform);

            float rA = model.vertexData()[ia * fpv + dims_];
            float gA = model.vertexData()[ia * fpv + dims_ + 1];
            float bA = model.vertexData()[ia * fpv + dims_ + 2];
            float rB = model.vertexData()[ib * fpv + dims_];
            float gB = model.vertexData()[ib * fpv + dims_ + 1];
            float bB = model.vertexData()[ib * fpv + dims_ + 2];
            float rC = model.vertexData()[ic * fpv + dims_];
            float gC = model.vertexData()[ic * fpv + dims_ + 1];
            float bC = model.vertexData()[ic * fpv + dims_ + 2];

            unsigned int triVertBase = t * vertsPerTri;
            unsigned int triTriBase = t * trisPerTri;

            for (int j = 0; j <= n; j++) {
                int rowOff = j * (n + 1) - j * (j - 1) / 2;
                for (int i = 0; i <= n - j; i++) {
                    float u = (float)i / n;
                    float v = (float)j / n;
                    float w = 1.0f - u - v;
                    int vidx = triVertBase + rowOff + i;

                    for (unsigned int d = 0; d < dims_; d++)
                        interp[d] = tposA[d] * w + tposB[d] * u + tposC[d] * v;
                    projectStereographic(interp, &subVerts_[vidx * 6], dims_, focalLength);

                    subVerts_[vidx * 6 + 3] = rA * w + rB * u + rC * v;
                    subVerts_[vidx * 6 + 4] = gA * w + gB * u + gC * v;
                    subVerts_[vidx * 6 + 5] = bA * w + bB * u + bC * v;
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
                    subIdx_[ti * 3 + 0] = v00;
                    subIdx_[ti * 3 + 1] = v10;
                    subIdx_[ti * 3 + 2] = v01;
                    subDepths[ti].idx = (int)ti;
                    subDepths[ti].depth = (subVerts_[v00 * 6 + 2] + subVerts_[v10 * 6 + 2] + subVerts_[v01 * 6 + 2]) / 3.0f;
                    subTriCount++;

                    if (i + j < n - 1) {
                        unsigned int v11 = triVertBase + rowOffJ1 + i + 1;
                        unsigned int ti2 = triTriBase + subTriCount;
                        subIdx_[ti2 * 3 + 0] = v10;
                        subIdx_[ti2 * 3 + 1] = v11;
                        subIdx_[ti2 * 3 + 2] = v01;
                        subDepths[ti2].idx = (int)ti2;
                        subDepths[ti2].depth = (subVerts_[v10 * 6 + 2] + subVerts_[v11 * 6 + 2] + subVerts_[v01 * 6 + 2]) / 3.0f;
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
            sortedSubIdx[i * 3 + 0] = subIdx_[t * 3 + 0];
            sortedSubIdx[i * 3 + 1] = subIdx_[t * 3 + 1];
            sortedSubIdx[i * 3 + 2] = subIdx_[t * 3 + 2];
        }

        glBindBuffer(GL_ARRAY_BUFFER, subVBO_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, totalSubVerts * 6 * sizeof(float), subVerts_.data());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subEBO_);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, totalSubTris * 3 * sizeof(unsigned int), sortedSubIdx.data());

        if (!wireframeOnly) {
            if (transparent) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
            }
            glUseProgram(tessProgram_);
            glUniform1f(tessUAspect_, aspect);
            glUniform1f(tessUDist3D_, 3.0f * focalLength);
            glUniform1f(tessUAlpha_, transparent ? modelAlpha : 1.0f);
            glUniform1f(tessULighting_, lighting ? 1.0f : 0.0f);
            glBindVertexArray(subVAO_);
            glDrawElements(GL_TRIANGLES, totalSubTris * 3, GL_UNSIGNED_INT, nullptr);
            if (transparent) {
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
            }
        }
    } else {
        // Perspective/orthographic: upload projected verts, sort triangles, render
        struct TriDepth { int idx; float depth; };
        std::vector<TriDepth> triDepths(numTri);
        for (unsigned int i = 0; i < numTri; i++) {
            float sum = 0;
            for (int j = 0; j < 3; j++) {
                int vi = indices[i * 3 + j];
                sum += projectedVerts[vi * 6 + 2];
            }
            triDepths[i] = {(int)i, sum / 3.0f};
        }
        std::sort(triDepths.begin(), triDepths.end(),
                  [](auto& a, auto& b) { return a.depth > b.depth; });

        std::vector<unsigned int> sorted(indexCount);
        for (unsigned int i = 0; i < numTri; i++) {
            int t = triDepths[i].idx;
            sorted[i * 3 + 0] = indices[t * 3 + 0];
            sorted[i * 3 + 1] = indices[t * 3 + 1];
            sorted[i * 3 + 2] = indices[t * 3 + 2];
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tessEBO_);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, indexCount * sizeof(unsigned int), sorted.data());
        glBindBuffer(GL_ARRAY_BUFFER, tessVBO_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * 6 * sizeof(float), projectedVerts);

        if (!wireframeOnly) {
            if (transparent) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
            }
            glUseProgram(tessProgram_);
            glUniform1f(tessUAspect_, aspect);
            glUniform1f(tessUDist3D_, 3.0f * focalLength);
            glUniform1f(tessUAlpha_, transparent ? modelAlpha : 1.0f);
            glUniform1f(tessULighting_, lighting ? 1.0f : 0.0f);
            glBindVertexArray(tessVAO_);
            glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
            if (transparent) {
                glDepthMask(GL_TRUE);
                glDisable(GL_BLEND);
            }
        }
    }
}

void Renderer::renderAxes(const TransformND& transform, float focalLength) {
    float aspect = (float)fbW_ / (float)fbH_;

    float* origin = (float*)alloca(dims_ * sizeof(float));
    float* tip = (float*)alloca(dims_ * sizeof(float));

    std::vector<float> axis3D(dims_ * 2 * 6);

    for (unsigned int d = 0; d < dims_; d++) {
        memset(origin, 0, dims_ * sizeof(float));
        memset(tip, 0, dims_ * sizeof(float));
        tip[d] = 1.5f;
        applyRotation(origin, transform);
        applyRotation(tip, transform);
        projectPerspective(origin, &axis3D[d * 12], dims_, focalLength);
        projectPerspective(tip, &axis3D[d * 12 + 6], dims_, focalLength);
        axis3D[d * 12 + 3] = axisR_[d]; axis3D[d * 12 + 4] = axisG_[d]; axis3D[d * 12 + 5] = axisB_[d];
        axis3D[d * 12 + 9] = axisR_[d]; axis3D[d * 12 + 10] = axisG_[d]; axis3D[d * 12 + 11] = axisB_[d];
    }

    glBindBuffer(GL_ARRAY_BUFFER, axesVBO_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, dims_ * 2 * 6 * sizeof(float), axis3D.data());
    glUseProgram(axesProgram_);
    glUniform1f(axesUAspect_, aspect);
    glUniform1f(axesUDist3D_, 3.0f * focalLength);
    glBindVertexArray(axesVAO_);
    glDrawArrays(GL_LINES, 0, dims_ * 2);
}

void Renderer::renderEdges(const Model& model, const TransformND& transform,
                           const float* projectedVerts,
                           int renderMode, float focalLength, const std::vector<Edge>& edges) {
    float aspect = (float)fbW_ / (float)fbH_;
    int fpv = dims_ + 3;

    size_t vertCount = 0;
    size_t edgeBufSize = edges.size() * (EDGE_SUBDIV_ + 1) * 3;
    if (edge3D_.size() < edgeBufSize)
        edge3D_.resize(edgeBufSize);

    if (renderMode == 1) {
        float* posA = (float*)alloca(dims_ * sizeof(float));
        float* posB = (float*)alloca(dims_ * sizeof(float));
        float* interp = (float*)alloca(dims_ * sizeof(float));
        for (size_t i = 0; i < edges.size(); i++) {
            int ia = edges[i].a, ib = edges[i].b;
            for (unsigned int d = 0; d < dims_; d++) {
                posA[d] = model.vertexData()[ia * fpv + d] + transform.translation[d];
                posB[d] = model.vertexData()[ib * fpv + d] + transform.translation[d];
            }
            applyRotation(posA, transform);
            applyRotation(posB, transform);
            for (int s = 0; s <= EDGE_SUBDIV_; s++) {
                float t = (float)s / (float)EDGE_SUBDIV_;
                for (unsigned int d = 0; d < dims_; d++)
                    interp[d] = posA[d] * (1.0f - t) + posB[d] * t;
                projectStereographic(interp, &edge3D_[vertCount * 3], dims_, focalLength);
                vertCount++;
            }
        }
    } else {
        for (size_t i = 0; i < edges.size(); i++) {
            int ia = edges[i].a, ib = edges[i].b;
            edge3D_[vertCount * 3 + 0] = projectedVerts[ia * 6];
            edge3D_[vertCount * 3 + 1] = projectedVerts[ia * 6 + 1];
            edge3D_[vertCount * 3 + 2] = projectedVerts[ia * 6 + 2];
            vertCount++;
            edge3D_[vertCount * 3 + 0] = projectedVerts[ib * 6];
            edge3D_[vertCount * 3 + 1] = projectedVerts[ib * 6 + 1];
            edge3D_[vertCount * 3 + 2] = projectedVerts[ib * 6 + 2];
            vertCount++;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, edgeVBO_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertCount * 3 * sizeof(float), edge3D_.data());

    glDisable(GL_DEPTH_TEST);
    glUseProgram(edgeProgram_);
    glUniform1f(edgeUAspect_, aspect);
    glUniform1f(edgeUDist3D_, 3.0f * focalLength);
    glBindVertexArray(edgeVAO_);
    if (renderMode == 1) {
        size_t offset = 0;
        for (size_t i = 0; i < edges.size(); i++) {
            glDrawArrays(GL_LINE_STRIP, (GLint)offset, EDGE_SUBDIV_ + 1);
            offset += EDGE_SUBDIV_ + 1;
        }
    } else {
        glDrawArrays(GL_LINES, 0, (GLsizei)vertCount);
    }
    glEnable(GL_DEPTH_TEST);
}

} // namespace dky
