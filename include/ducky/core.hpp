#pragma once

#include <cstdint>
#include <vector>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <cstring>

namespace dky {

struct Edge { int a, b; };

class Model {
public:
    Model() = default;
    explicit Model(const char* path);

    void load(const char* path);
    void backupVertices();
    void restoreVertices();
    void assignFaceColors(int colorScheme);
    void generateEdges();

    const float* vertexData() const { return vertices_.data(); }
    float* vertexData() { return vertices_.data(); }
    const unsigned int* indexData() const { return indices_.data(); }
    unsigned int vertexCount() const { return vertexCount_; }
    unsigned int indexCount() const { return indexCount_; }
    unsigned int dimensions() const { return dimensions_; }
    int fpv() const { return (int)dimensions_ + 3; }

    const Edge* edgeData() const { return edges_.data(); }
    size_t edgeCount() const { return edges_.size(); }

    std::vector<float>& vertices() { return vertices_; }
    const std::vector<float>& vertices() const { return vertices_; }
    const std::vector<unsigned int>& indices() const { return indices_; }
    const std::vector<Edge>& edges() const { return edges_; }

private:
    std::vector<float> vertices_;
    std::vector<unsigned int> indices_;
    std::vector<Edge> edges_;
    std::vector<float> verticesBackup_;
    unsigned int vertexCount_ = 0;
    unsigned int indexCount_ = 0;
    unsigned int dimensions_ = 4;
};

struct TransformND {
    std::vector<float> angles;
    std::vector<float> translation;
    std::vector<bool> autoRotate;
    unsigned int dims = 0;

    int planeCount() const;
    int planeIndex(int i, int j) const;
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

constexpr float PI = 3.141592653589793f;

void hslToRgb(float h, float s, float l, float& r, float& g, float& b);
void rotatePlane(float& a, float& b, float angle);
void applyRotation(float* pos, const TransformND& t);
void projectPerspective(const float* in, float* out, int dims, float focalLength);
void projectOrthographic(const float* in, float* out, int dims);
void projectStereographic(const float* in, float* out, int dims, float focalLength);

std::vector<Edge> generateEdges(const float* vertices, unsigned int vertexCount,
                                unsigned int dims, int fpv,
                                const unsigned int* indices, unsigned int indexCount);

void assignFaceColors(float* vertices, const unsigned int* indices,
                      unsigned int indexCount, unsigned int dims, int colorScheme);

} // namespace dky
