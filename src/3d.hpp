#include <cmath>

typedef struct Vecf3{
    float x, y, z;

    explicit operator Vecf2() const {
        return Vecf2{x, y};
    }

    Vecf3& operator-=(const Vecf3& other){
        x -= other.x;
        y -= other.y;
        z -= other.z;
        return *this;
    }

    constexpr Vecf3 operator+(const Vecf3& other) const{
        return {x + other.x, y + other.y, z + other.z};
    }

    constexpr Vecf3 operator-(const Vecf3& other) const{
        return {x - other.x, y - other.y, z - other.z};
    }
}Vecf3;

static inline void rotatePlane(float& a, float& b, float angle) {
    float c = cosf(angle), s = sinf(angle);
    float na = a * c - b * s;
    float nb = a * s + b * c;
    a = na;
    b = nb;
}

Vecf3 rotate3d(Vecf3 position, Vecf3 center, Vecf3 rotation){
    Vecf3 tmp;
    tmp = position - center;
    rotatePlane(tmp.y, tmp.z, rotation.x);
    rotatePlane(tmp.x, tmp.z, rotation.y);
    rotatePlane(tmp.x, tmp.y, rotation.z);
    return tmp + center;
}