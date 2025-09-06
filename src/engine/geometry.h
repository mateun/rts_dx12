
#pragma once
#include <vector>
#include <directxtk/SimpleMath.h>

struct Geometry
{
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    std::vector<DirectX::SimpleMath::Vector3> positions;
    std::vector<DirectX::SimpleMath::Vector2> uvs;

};



class GeometryFactory {

    public:
        static Geometry getQuadGeometry();


};