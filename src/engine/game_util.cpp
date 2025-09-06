#include "game_util.h"
#include "renderer.h"


using namespace DirectX::SimpleMath;
ObjectRenderData createObjectRenderData(const std::string textureId, const std::string meshId, 
        const std::string inputLayoutId, std::vector<Vector3> positions, Vector3 scale) {


    auto objData = ObjectRenderData();
    objData.textureId = textureId;
    objData.meshId = meshId;
    objData.inputLayoutId = inputLayoutId;
    
    auto S = Matrix::CreateScale(scale.x, scale.y, scale.z);
    for (auto& p : positions) {
        auto T= Matrix::CreateTranslation(p.x, p.y, p.z);
        auto W = S * T;
        objData.worldMatrices.push_back(W);
    }

    return objData;
        
}
