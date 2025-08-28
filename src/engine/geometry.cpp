#include "geometry.h"

Geometry GeometryFactory::getQuadGeometry()
{
    std::vector<float> vertices = {
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
        0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
        -0.5f,  0.5f, 0.0f, 0.0f, 1.0f,
        0.5, 0.5, 0.0f, 1.0f, 1.0f
    };

    std::vector<uint32_t> indices = {
            2,1,0, 
            3,1,2

    };

    return {vertices, indices};
}