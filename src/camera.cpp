#include "ducky/camera.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>

#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

namespace dky {

Camera::Camera(unsigned int dims) {
    transform_.dims = dims;
    transform_.angles.resize(transform_.planeCount(), 0.0f);
    transform_.autoRotate.resize(transform_.planeCount(), true);
    transform_.translation.resize(dims, 0.0f);
}

void Camera::reset() {
    std::fill(transform_.angles.begin(), transform_.angles.end(), 0.0f);
    std::fill(transform_.translation.begin(), transform_.translation.end(), 0.0f);
}

void Camera::updateAutoRotation(float dt) {
    for (int i = 0; i < planeCount(); i++) {
        if (transform_.autoRotate[i])
            transform_.angles[i] += dt * 0.5f * (1 + (i % 3));
    }
}

void Camera::wrapAngles() {
    for (int i = 0; i < planeCount(); i++) {
        float a = transform_.angles[i];
        transform_.angles[i] = fmodf(a + PI, 2.0f * PI);
        if (transform_.angles[i] < 0) transform_.angles[i] += 2.0f * PI;
        transform_.angles[i] -= PI;
    }
}

void Camera::cycleRenderMode() {
    int rm = (int)renderMode_;
    rm = (rm + 1) % 3;
    renderMode_ = (RenderMode)rm;
}

void Camera::setAngle(int idx, float angle) {
    if (idx >= 0 && idx < planeCount()) transform_.angles[idx] = angle;
}

float Camera::angle(int idx) const {
    return (idx >= 0 && idx < planeCount()) ? transform_.angles[idx] : 0.0f;
}

void Camera::setAutoRotate(int idx, bool on) {
    if (idx >= 0 && idx < planeCount()) transform_.autoRotate[idx] = on;
}

bool Camera::autoRotate(int idx) const {
    return (idx >= 0 && idx < planeCount()) ? transform_.autoRotate[idx] : false;
}

int Camera::planeCount() const {
    return transform_.planeCount();
}

void Camera::setTranslation(const float* pos) {
    for (unsigned int i = 0; i < transform_.dims; i++)
        transform_.translation[i] = pos[i];
}

void Camera::beginOrbit(double mx, double my) {
    orbiting_ = true;
    orbitX_ = mx;
    orbitY_ = my;
}

void Camera::updateOrbit(double mx, double my) {
    if (!orbiting_) return;
    double dx = mx - orbitX_;
    double dy = my - orbitY_;
    unsigned int d = transform_.dims;
    if (d >= 3) {
        transform_.angles[1] -= (float)dx * 0.005f;
        transform_.angles[d - 1] += (float)dy * 0.005f;
    } else if (planeCount() >= 1) {
        transform_.angles[0] += (float)dx * 0.005f;
    }
    orbitX_ = mx;
    orbitY_ = my;
}

void Camera::endOrbit() {
    orbiting_ = false;
}

void Camera::projectVertices(const float* modelVerts, unsigned int vertexCount,
                              unsigned int dims, int fpv) {
    projectedVerts_.resize(vertexCount * 6);
    float* pos = (float*)alloca(dims * sizeof(float));
    for (unsigned int i = 0; i < vertexCount; i++) {
        for (unsigned int d = 0; d < dims; d++)
            pos[d] = modelVerts[i * fpv + d];
        for (unsigned int d = 0; d < dims; d++)
            pos[d] += transform_.translation[d];
        applyRotation(pos, transform_);
        switch (renderMode_) {
            case Perspective:
                projectPerspective(pos, &projectedVerts_[i * 6], dims, focalLength_);
                break;
            case Stereographic:
                projectStereographic(pos, &projectedVerts_[i * 6], dims, focalLength_);
                break;
            case Orthographic:
            default:
                projectOrthographic(pos, &projectedVerts_[i * 6], dims);
                break;
        }
        projectedVerts_[i * 6 + 3] = modelVerts[i * fpv + dims];
        projectedVerts_[i * 6 + 4] = modelVerts[i * fpv + dims + 1];
        projectedVerts_[i * 6 + 5] = modelVerts[i * fpv + dims + 2];
    }
}

void Camera::saveState(const char* path) const {
    FILE* f = fopen(path, "w");
    if (!f) { std::cerr << "Failed to save state\n"; return; }
    fprintf(f, "%u\n", transform_.dims);
    for (float a : transform_.angles) fprintf(f, "%.8f ", a);
    fprintf(f, "\n");
    for (float tr : transform_.translation) fprintf(f, "%.8f ", tr);
    fprintf(f, "\n");
    fprintf(f, "%f\n", focalLength_);
    fprintf(f, "%d\n", (int)renderMode_);
    fclose(f);
}

bool Camera::loadState(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { std::cerr << "Failed to load state\n"; return false; }
    unsigned int dims;
    if (fscanf(f, "%u", &dims) != 1 || dims != transform_.dims) {
        fclose(f);
        std::cerr << "State file dimension mismatch\n";
        return false;
    }
    for (int i = 0; i < planeCount(); i++) {
        if (fscanf(f, "%f", &transform_.angles[i]) != 1) {
            fclose(f);
            std::cerr << "Failed to read angles\n";
            return false;
        }
    }
    for (unsigned int i = 0; i < transform_.dims; i++) {
        if (fscanf(f, "%f", &transform_.translation[i]) != 1) {
            fclose(f);
            std::cerr << "Failed to read translations\n";
            return false;
        }
    }
    float fl;
    if (fscanf(f, "%f", &fl) == 1) focalLength_ = fl;
    int rm;
    if (fscanf(f, "%d", &rm) == 1 && rm >= 0 && rm <= 2) renderMode_ = (RenderMode)rm;
    fclose(f);
    return true;
}

} // namespace dky
