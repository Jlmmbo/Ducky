typedef struct Vecf2{
    float x, y;

    Vecf2& operator-=(const Vecf2& other){
        x -= other.x;
        y -= other.y;
        return *this;
    }

    constexpr Vecf2 operator+(const Vecf2& other) const{
        return {x + other.x, y + other.y};
    }

    constexpr Vecf2 operator-(const Vecf2& other) const{
        return {x - other.x, y - other.y};
    }
}Vecf2;

Vecf2 rotate2d(Vecf2 position, Vecf2 center, float rotation){
    Vecf2 tmp;
    tmp = position - center;
    rotatePlane(tmp.x, tmp.y, rotation);
    return tmp + center;
}