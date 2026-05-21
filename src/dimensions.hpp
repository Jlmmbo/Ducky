
#include "2d.hpp"
#include "3d.hpp"
#include "4d.hpp"
#include "camera.hpp"

const float DIST_4D = 4.0;
const float DIST_3D = 3.0;

Vecf3 project3d(Vecf4 point, Camera camera){
    point = rotate4d(point, camera.pos, camera.rotation);
    point -= camera.pos;
    float wDepth = DIST_4D - point.w;
    float scale4d = wDepth > 0.001f ? DIST_4D / wDepth : 10.0f;
    return Vecf3{point.x * scale4d, point.y * scale4d, point.z * scale4d};
}

Vecf2 project2d(Vecf3 point, Camera camera){
    point = rotate3d(point, (Vecf3){camera.pos.x, camera.pos.y, camera.pos.z}, (Vecf3){camera.rotation[0], camera.rotation[1], camera.rotation[2]});
    point -= (Vecf3)camera.pos;
    float zDepth = DIST_3D - point.z;
    float scale3d = zDepth > 0.001f ? DIST_3D / zDepth : 10.0f;
    return Vecf2{point.x * scale3d, point.y * scale3d};
}

Vecf2 project(Vecf4 point, Camera camera){
    Vecf3 p3 = project3d(point, camera);
    float zDepth = DIST_3D - p3.z;
    float scale3d = zDepth > 0.001f ? DIST_3D / zDepth : 10.0f;
    return Vecf2{p3.x * scale3d, p3.y * scale3d};
}
