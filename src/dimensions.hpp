
#include "2d.hpp"
#include "3d.hpp"
#include "4d.hpp"
#include "camera.hpp"

Vecf3 project3d(Vecf4 point, Camera camera){
    Vecf3 newPoint;
    // recalculate positions to align with camera rotation
    rotate4d(point, camera.pos, camera.rotation);

    // recalculate positions to align with camera position
    point -= camera.pos;

    // project to 3d
    newPoint.x = point.x / point.w;
    newPoint.y = point.y / point.w;
    newPoint.z = point.z / point.w;

    return newPoint;
}

Vecf2 project2d(Vecf3 point, Camera camera){
    Vecf2 newPoint;

    //r ecalculate positions to align with camera rotation

    // ignoring any 4d components
    rotate3d(point, (Vecf3){camera.pos.x, camera.pos.y, camera.pos.z}, (Vecf3){camera.rotation[0], camera.rotation[1], camera.rotation[3]});

    // recalculate positions to align with camera position
    point -= (Vecf3)camera.pos;

    // project to 2d
    newPoint.x = point.x / point.z;
    newPoint.y = point.y / point.z;

    return newPoint;
}

Vecf2 project(Vecf4 point, Camera camera){
    Vecf2 newPoint;
    // recalculate positions to align with camera rotation
    rotate4d(point, camera.pos, camera.rotation);

    // recalculate positions to align with camera position
    point -= camera.pos;

    // project directly to 2d
    newPoint.x = point.x / point.w / point.z;
    newPoint.y = point.y / point.w / point.z;

    return newPoint;
}
