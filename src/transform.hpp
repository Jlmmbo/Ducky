#pragma once

#include <cmath>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

constexpr float PI = 3.141592653589793f;

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

inline void hslToRgb(float h, float s, float l, float& r, float& g, float& b) {
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

inline void rotatePlane(float& a, float& b, float angle) {
    float c = cosf(angle), s = sinf(angle);
    float na = a * c - b * s;
    float nb = a * s + b * c;
    a = na;
    b = nb;
}

inline void applyRotation(float* pos, const TransformND& t) {
    for (int i = 0; i < (int)t.dims; i++) {
        for (int j = i + 1; j < (int)t.dims; j++) {
            float angle = t.angles[t.planeIndex(i, j)];
            if (fabsf(angle) > 0.0001f)
                rotatePlane(pos[i], pos[j], angle);
        }
    }
}

inline void projectPerspective(const float* in, float* out, int dims, float focalLength) {
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

inline void projectOrthographic(const float* in, float* out, int dims) {
    out[0] = in[0];
    out[1] = in[1];
    out[2] = dims > 2 ? in[2] : 0.0f;
}

inline void projectStereographic(const float* in, float* out, int dims, float focalLength) {
    float tmp[128];
    for (int i = 0; i < dims; i++) tmp[i] = in[i];

    for (int d = dims - 1; d >= 4; d--) {
        float dist = (float)d * focalLength;
        float depth = dist - tmp[d];
        float s = depth > 0.001f ? dist / depth : 10.0f;
        for (int c = 0; c < d; c++)
            tmp[c] *= s;
    }

    float x = tmp[0], y = tmp[1], z = tmp[2], w = dims > 3 ? tmp[3] : 0.0f;
    float radius = sqrtf(x*x + y*y + z*z + w*w);
    float denom = radius - w;
    if (fabsf(denom) < 0.001f) denom = 0.001f;
    float s = radius / denom;
    out[0] = x * s;
    out[1] = y * s;
    out[2] = z * s;
}
