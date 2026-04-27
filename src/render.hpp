typedef struct Vertex{
    Vecf4 pos;
    char* texture_name;
}Vertex;

Vecf2 interpol(Vecf2 a, Vecf2 b, float x){
    return Vecf2{a.x + (b.x - a.x) * x, a.y + (b.y - a.y) * x};
}

void renderPoly(Vertex* verts, int n, Camera camera, char* texture_name = NULL){
    Vecf2 screenVerts[n];

    // get screen positions of vertices
    for(int i = 0; i < n; i++){
        Vecf2 screen_pos = project(verts[i].pos, camera);
        screenVerts[i] = screen_pos;
    }

    // render the polygon with texture interpolated across the surface
    for(int i = 0; i < n; i++){
        Vecf2 a = screenVerts[i];
        Vecf2 b = screenVerts[(i + 1) % n];
        for(float x = 0; x < 1; x += 0.01){
            Vecf2 point = interpol(a, b, x);
            
        }

}