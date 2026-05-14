
#include "2d.hpp"
#include "3d.hpp"
#include "4d.hpp"
#include "camera.hpp"

Vecf3 project3d(Vecf4 point, Camera camera){
    Vecf3 newPoint;
    point = rotate4d(point, camera.pos, camera.rotation);
    point -= camera.pos;
    newPoint.x = point.x / point.w;
    newPoint.y = point.y / point.w;
    newPoint.z = point.z / point.w;
    return newPoint;
}

Vecf2 project2d(Vecf3 point, Camera camera){
    Vecf2 newPoint;
    point = rotate3d(point, (Vecf3){camera.pos.x, camera.pos.y, camera.pos.z}, (Vecf3){camera.rotation[0], camera.rotation[1], camera.rotation[3]});
    point -= (Vecf3)camera.pos;
    newPoint.x = point.x / point.z;
    newPoint.y = point.y / point.z;
    return newPoint;
}

Vecf2 project(Vecf4 point, Camera camera){
    Vecf2 newPoint;
    point = rotate4d(point, camera.pos, camera.rotation);
    point -= camera.pos;
    newPoint.x = point.x / point.w / point.z;
    newPoint.y = point.y / point.w / point.z;
    return newPoint;
}
