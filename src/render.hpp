#include <vector>

typedef struct Vertex{
    Vecf4 pos;
    char* texture_name;
}Vertex;

Vecf2 interpol(Vecf2 a, Vecf2 b, float x){
    return Vecf2{a.x + (b.x - a.x) * x, a.y + (b.y - a.y) * x};
}

void renderPoly(Vertex* verts, int n, Camera camera, char* texture_name = NULL){
    std::vector<Vecf2> screenVerts(n);

    for(int i = 0; i < n; i++){
        Vecf2 screen_pos = project(verts[i].pos, camera);
        screenVerts[i] = screen_pos;
    }

    for(int i = 0; i < n; i++){
        Vecf2 a = screenVerts[i];
        Vecf2 b = screenVerts[(i + 1) % n];
        for(int step = 0; step < 100; step++){
            float t = step / 100.0f;
            Vecf2 point = interpol(a, b, t);
            (void)point;
        }
    }
}