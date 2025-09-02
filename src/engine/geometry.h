
#pragma once
#include <vector>

struct Geometry
{
    std::vector<float> vertices;
    std::vector<uint32_t> indices;

};



class GeometryFactory {

    public:
        static Geometry getQuadGeometry();


};