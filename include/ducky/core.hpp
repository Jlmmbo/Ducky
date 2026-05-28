#pragma once

#include <cstdint>
#include <vector>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <cstring>

namespace dky {

struct Model {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int vertexCount = 0;
    unsigned int indexCount = 0;
    unsigned int dimensions = 4;
};

Model LoadModel(const char* path);

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

struct Edge { int a, b; };

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

void assignFaceColors(Model& model, int colorScheme);

} // namespace dky
