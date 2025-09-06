#pragma once
#include <d3d11.h>
#include <string>
#include <DirectXTK/SimpleMath.h>

struct ObjectRenderData;
ObjectRenderData createObjectRenderData(const std::string textureId, 
                                    const std::string meshId, 
                                    const std::string inputLayoutId, 
                                    std::vector<DirectX::SimpleMath::Vector3> positions, 
                                    DirectX::SimpleMath::Vector3 scale);