#pragma once

#include "ducky/core.hpp"
#include <vector>

namespace dky {

class Camera {
public:
    enum RenderMode {
        Perspective = 0,
        Stereographic = 1,
        Orthographic = 2
    };

    Camera(unsigned int dims);

    void reset();
    void updateAutoRotation(float dt);
    void wrapAngles();
    void cycleRenderMode();

    void setAngle(int idx, float angle);
    float angle(int idx) const;
    void setAutoRotate(int idx, bool on);
    bool autoRotate(int idx) const;
    int planeCount() const;

    void setTranslation(const float* pos);
    const std::vector<float>& translation() const { return transform_.translation; }
    std::vector<float>& translation() { return transform_.translation; }

    void beginOrbit(double mx, double my);
    void updateOrbit(double mx, double my);
    void endOrbit();
    bool isOrbiting() const { return orbiting_; }

    void projectVertices(const float* modelVerts, unsigned int vertexCount,
                         unsigned int dims, int fpv);
    const float* projectedVerts() const { return projectedVerts_.data(); }
    unsigned int projectedSize() const { return (unsigned int)projectedVerts_.size(); }

    float focalLength() const { return focalLength_; }
    void setFocalLength(float fl) { focalLength_ = fl; }
    RenderMode renderMode() const { return renderMode_; }
    void setRenderMode(RenderMode m) { renderMode_ = m; }
    unsigned int dims() const { return transform_.dims; }

    const TransformND& transform() const { return transform_; }

    const std::vector<float>& angles() const { return transform_.angles; }
    std::vector<float>& angles() { return transform_.angles; }
    const std::vector<bool>& autoRotateFlags() const { return transform_.autoRotate; }
    std::vector<bool>& autoRotateFlags() { return transform_.autoRotate; }

    void saveState(const char* path) const;
    bool loadState(const char* path);

private:
    TransformND transform_;
    float focalLength_ = 1.0f;
    RenderMode renderMode_ = Perspective;
    bool orbiting_ = false;
    double orbitX_ = 0.0, orbitY_ = 0.0;
    std::vector<float> projectedVerts_;
};

} // namespace dky
