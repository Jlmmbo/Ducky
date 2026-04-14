
Vec3 project3d(Vec4 point, Camera camera){
    Vec3 newPoint;
    // recalculate positions to align with camera rotation

    // recalculate positions to align with camera position
    point.x -= camera.pos.x;
    point.y -= camera.pos.y;
    point.z -= camera.pos.z;
    point.w -= camera.pos.w;

    // project to 3d
    newPoint.x = point.x / point.w;
    newPoint.y = point.y / point.w;
    newPoint.z = point.z / point.w;

    return newPoint;
}

Vec2 project2d(Vec3 point, Camera camera){
    Vec2 newPoint;

    //recalculate positions to align with camera rotation

    //recalculate positions to align with camera position
    point.x -= camera.pos.x;
    point.y -= camera.pos.y;
    point.z -= camera.pos.z;

    //project to 2d
    newPoint.x = point.x / point.z;
    newPoint.y = point.y / point.z;

    return newPoint;
}

