struct Vertex {
    float x, y;   // screen position
    float u, v;   // texture coordinates
};

struct Color {
    uint8_t r, g, b, a;
};

static inline int clampi(int v, int a, int b) {
    return (v < a) ? a : (v > b ? b : v);
}

static inline float edge(float ax, float ay,
                         float bx, float by,
                         float cx, float cy) {
    return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
}

void rotatePlane(float &x, float &y, float angle){
    float s = sin(angle);
    float c = cos(angle);
    float newX = x * c - y * s;
    float newY = x * s + y * c;
    x = newX;
    y = newY;
}