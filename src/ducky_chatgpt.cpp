#include <SDL3/SDL.h>
#include <SDL3/SDL_image.h>

#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>

struct Vec4 { float x,y,z,w; };
struct Vec3 { float x,y,z; };
struct Vec2 { float x,y; };

struct Vertex {
    Vec4 p;
    Vec2 uv;
};

struct Tri {
    Vertex a,b,c;
};

// ---------------- CAMERA ----------------
struct Camera4D {
    Vec4 pos {0,0,0,0};

    float xy=0, xz=0, xw=0;
    float yz=0, yw=0, zw=0;
};

// ---------------- 4D ROTATION ----------------
void rot(float& a, float& b, float s, float c) {
    float na = a*c - b*s;
    float nb = a*s + b*c;
    a = na; b = nb;
}

Vec4 rotate4D(Vec4 v, const Camera4D& cam, bool inv) {
    auto apply = [&](float& a, float& b, float ang){
        float s = sin(ang), c = cos(ang);
        if(inv) s = -s;
        rot(a,b,s,c);
    };

    apply(v.x,v.y,cam.xy);
    apply(v.x,v.z,cam.xz);
    apply(v.x,v.w,cam.xw);
    apply(v.y,v.z,cam.yz);
    apply(v.y,v.w,cam.yw);
    apply(v.z,v.w,cam.zw);

    return v;
}

// ---------------- CAMERA TRANSFORM ----------------
Vec4 worldToCam(Vec4 p, const Camera4D& cam) {
    p.x -= cam.pos.x;
    p.y -= cam.pos.y;
    p.z -= cam.pos.z;
    p.w -= cam.pos.w;
    return rotate4D(p, cam, true);
}

// ---------------- PROJECTION ----------------
Vec3 proj4D(Vec4 p, const Camera4D& cam, float d4) {
    Vec4 c = worldToCam(p, cam);
    float k = d4 / (d4 - c.w);
    return { c.x*k, c.y*k, c.z*k };
}

Vec2 proj3D(Vec3 p, float d3, int W, int H) {
    float k = d3 / (d3 - p.z);
    return { p.x*k + W/2.0f, p.y*k + H/2.0f };
}

// ---------------- TESSERACT MESH ----------------
std::vector<Vec4> makeTesseract(float s) {
    std::vector<Vec4> v;
    v.reserve(16);

    for(int i=0;i<16;i++){
        v.push_back({
            (i&1 ? 1:-1)*s,
            (i&2 ? 1:-1)*s,
            (i&4 ? 1:-1)*s,
            (i&8 ? 1:-1)*s
        });
    }
    return v;
}

// build correct edges (not strictly needed for rendering but useful)
std::vector<std::pair<int,int>> buildEdges() {
    std::vector<std::pair<int,int>> e;

    for(int i=0;i<16;i++){
        for(int j=i+1;j<16;j++){
            int d = i ^ j;
            if(d && !(d & (d-1)))
                e.push_back({i,j});
        }
    }
    return e;
}

// 4D square faces
struct Face { int a,b,c,d; };

std::vector<Face> buildFaces() {
    std::vector<Face> f;

    for(int i=0;i<16;i++){
        for(int d1=1; d1<16; d1<<=1){
            for(int d2=d1<<1; d2<16; d2<<=1){

                int a=i;
                int b=i^d1;
                int c=i^d1^d2;
                int d=i^d2;

                if(b<16 && c<16 && d<16)
                    f.push_back({a,b,c,d});
            }
        }
    }
    return f;
}

// triangulate faces
std::vector<Tri> triangulate(const std::vector<Vec4>& v,
                             const std::vector<Face>& f)
{
    std::vector<Tri> out;

    for(auto& face : f){
        Vec4 A=v[face.a];
        Vec4 B=v[face.b];
        Vec4 C=v[face.c];
        Vec4 D=v[face.d];

        out.push_back({{A,{0,0}}, {B,{1,0}}, {C,{1,1}}});
        out.push_back({{A,{0,0}}, {C,{1,1}}, {D,{0,1}}});
    }

    return out;
}

// ---------------- DRAW ----------------
void drawTri(SDL_Renderer* r, SDL_Texture* tex,
             Vec2 a, Vec2 b, Vec2 c,
             Vec2 ua, Vec2 ub, Vec2 uc)
{
    SDL_Vertex v[3];

    v[0].position={a.x,a.y};
    v[1].position={b.x,b.y};
    v[2].position={c.x,c.y};

    v[0].tex_coord=ua;
    v[1].tex_coord=ub;
    v[2].tex_coord=uc;

    v[0].color={255,255,255,255};
    v[1].color={255,255,255,255};
    v[2].color={255,255,255,255};

    SDL_RenderGeometry(r, tex, v, 3, nullptr, 0);
}

// ---------------- MAIN ----------------
int main() {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    int W=900,H=700;

    SDL_Window* win = SDL_CreateWindow("REAL 4D TESSERACT",W,H,0);
    SDL_Renderer* ren = SDL_CreateRenderer(win,nullptr);

    SDL_Surface* s = IMG_Load("texture.png");
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren,s);
    SDL_DestroySurface(s);

    Camera4D cam;
    cam.pos = {0,0,0,0};

    auto verts = makeTesseract(1.2f);
    auto faces = buildFaces();
    auto tris  = triangulate(verts, faces);

    float t=0;
    bool run=true;
    SDL_Event e;

    while(run){
        while(SDL_PollEvent(&e)){
            if(e.type==SDL_EVENT_QUIT) run=false;

            if(e.type==SDL_EVENT_KEY_DOWN){
                switch(e.key.keysym.sym){
                    case SDLK_w: cam.pos.z+=0.2f; break;
                    case SDLK_s: cam.pos.z-=0.2f; break;
                    case SDLK_a: cam.pos.x-=0.2f; break;
                    case SDLK_d: cam.pos.x+=0.2f; break;
                    case SDLK_q: cam.pos.w-=0.2f; break;
                    case SDLK_e: cam.pos.w+=0.2f; break;
                }
            }

            if(e.type==SDL_EVENT_MOUSE_MOTION){
                cam.xy += e.motion.xrel*0.002f;
                cam.xz += e.motion.yrel*0.002f;
                cam.yw += e.motion.xrel*0.0015f;
                cam.zw += e.motion.yrel*0.0015f;
            }
        }

        SDL_SetRenderDrawColor(ren,10,10,18,255);
        SDL_RenderClear(ren);

        t += 0.01f;

        struct R { float d; Tri t; };
        std::vector<R> q;

        for(auto& tri : tris){

            Vec3 A = proj4D(tri.a.p, cam, 3.0f);
            Vec3 B = proj4D(tri.b.p, cam, 3.0f);
            Vec3 C = proj4D(tri.c.p, cam, 3.0f);

            Vec2 a = proj3D(A,3.0f,W,H);
            Vec2 b = proj3D(B,3.0f,W,H);
            Vec2 c = proj3D(C,3.0f,W,H);

            float depth = (A.z+B.z+C.z)/3.0f;

            q.push_back({depth, tri});
        }

        std::sort(q.begin(),q.end(),
                  [](auto& a, auto& b){return a.d>b.d;});

        for(auto& r : q){

            Vec3 A=proj4D(r.t.a.p,cam,3.0f);
            Vec3 B=proj4D(r.t.b.p,cam,3.0f);
            Vec3 C=proj4D(r.t.c.p,cam,3.0f);

            Vec2 a=proj3D(A,3.0f,W,H);
            Vec2 b=proj3D(B,3.0f,W,H);
            Vec2 c=proj3D(C,3.0f,W,H);

            drawTri(ren,tex,
                a,b,c,
                r.t.a.uv,
                r.t.b.uv,
                r.t.c.uv
            );
        }

        SDL_RenderPresent(ren);
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);

    IMG_Quit();
    SDL_Quit();
}