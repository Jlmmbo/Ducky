#include <cmath>

typedef struct Vecf4{
    float x, y, z, w;

    explicit operator Vecf3() const {
        return Vecf3{x, y, z};
    }

    Vecf4& operator-=(const Vecf4& other){
        x -= other.x;
        y -= other.y;
        z -= other.z;
        w -= other.w;
        return *this;
    }

    constexpr Vecf4 operator+(const Vecf4& other) const {
        return {x + other.x, y + other.y, z + other.z, w + other.w};
    }

    constexpr Vecf4 operator-(const Vecf4& other) const {
        return {x - other.x, y - other.y, z - other.z, w - other.w};
    }
}Vecf4;

static inline void rotatePlane(float& a, float& b, float angle) {
    float c = cosf(angle), s = sinf(angle);
    float na = a * c - b * s;
    float nb = a * s + b * c;
    a = na;
    b = nb;
}

Vecf4 rotate4d(Vecf4 position, Vecf4 center, float rotation[6]){
    Vecf4 tmp;
    tmp = position - center;
    rotatePlane(tmp.x, tmp.y, rotation[0]);
    rotatePlane(tmp.x, tmp.z, rotation[1]);
    rotatePlane(tmp.x, tmp.w, rotation[2]);
    rotatePlane(tmp.y, tmp.z, rotation[3]);
    rotatePlane(tmp.y, tmp.w, rotation[4]);
    rotatePlane(tmp.z, tmp.w, rotation[5]);
    return tmp + center;
}