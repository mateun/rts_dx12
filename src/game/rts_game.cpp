#include "rts_game.h"
#include "../engine/appwindow.h"
#include "../engine/renderer.h"
#include "../engine/geometry.h"

Game* getGame() {

    return new RTSGame();
}

RenderInitData RTSGame::getInitData(CommandLine cmdline, Window window)
{
    bool ide = false;
    if (cmdline.argc >=  2 && strcmp(cmdline.args[1],"ide") == 0) {
        ide = true;
    }
    auto initData = RenderInitData();
    initData.ide = ide;
    initData.hwnd = window.hwnd;
    initData.screenWidth = window.width;
    initData.screenHeight = window.height;
    initData.numFrames = 3;
    initData.textureDescriptors.push_back({"hero", initData.ide? L"../../hero.png" : L"../hero.png"});
    initData.textureDescriptors.push_back({"enemy1", initData.ide? L"../../enemy1.png" : L"../hero.png"});
    auto quadGeometry = GeometryFactory().getQuadGeometry();
    initData.meshDescriptors.push_back({"quad", quadGeometry});

    // Define pipeline states needed in our rts game:
    // 1. UIs
    auto uiPipelineState = PipelineState();
    uiPipelineState.id = "ui";
    uiPipelineState.shader = initData.ide ? L"../../shaders.hlsl" : L"../shaders.hlsl";
    uiPipelineState.useDepthBuffer = true;
    uiPipelineState.wireframe = false;
    uiPipelineState.inputLayout.addElement({InputElementType::POSITION}).addElement({InputElementType::UV});
    initData.pipelineStates.push_back(uiPipelineState);
    

    auto buildingsPipelineState = PipelineState();
    buildingsPipelineState.id = "buildings";
    buildingsPipelineState.shader = initData.ide ? L"../../shaders.hlsl" : L"../shaders.hlsl";
    buildingsPipelineState.inputLayout.addElement({InputElementType::POSITION})
                        .addElement({InputElementType::UV});
    initData.pipelineStates.push_back(buildingsPipelineState);
    
    


    return initData;
    
}

FrameData RTSGame::getFrameData()
{
    auto frameData = FrameData();

    DirectX::XMMATRIX V = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX P = DirectX::XMMatrixOrthographicOffCenterLH(0, 800, 0, 600, 0.1, 100);

    XMStoreFloat4x4(&frameData.viewMatrix, V);
    XMStoreFloat4x4(&frameData.projectionMatrix, P);

    auto objData = ObjectRenderData();
    objData.textureId = "hero";
    DirectX::XMMATRIX S = DirectX::XMMatrixScaling(64, 64, 1);
    XMStoreFloat4x4(&objData.worldMatrix, S * DirectX::XMMatrixTranslation(100, 100, 0.2));

    frameData.objectRenderData.push_back(objData);

    return frameData;
}
