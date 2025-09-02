#include "geometry.h"

Geometry GeometryFactory::getQuadGeometry()
{
    std::vector<float> vertices = {
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,0, 0, -1,
        0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0, 0, -1,
        -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 0, 0, -1,
        0.5, 0.5, 0.0f, 1.0f, 1.0f, 0, 0, -1
    };

    std::vector<uint32_t> indices = {
        // CCW
            // 2,1,0, 
            // 3,1,2
        // CW
        0, 2, 3,
        0, 3, 1



    };

    return {vertices, indices};
}