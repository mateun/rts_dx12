#include "rts_game.h"
#include "../engine/appwindow.h"
#include "../engine/renderer.h"
#include "../engine/geometry.h"
#include "../engine/asset_importer.h"
#include <filesystem>

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
    initData.textureDescriptors.push_back({"hero", initData.ide? L"../hero.png" : L"../hero.png"});
    initData.textureDescriptors.push_back({"enemy1", initData.ide? L"../enemy1.png" : L"../hero.png"});
    auto quadGeometry = GeometryFactory().getQuadGeometry();
    initData.meshDescriptors.push_back({"quad", quadGeometry});

    auto currPath = std::filesystem::current_path().string();
    std::cout << "cwd = " << currPath << "\n";
    Geometry houseGeometry;
    bool result = GltfStaticMeshLoader().load(("../src/game/assets/house.glb"), 
                                                houseGeometry, true);
    initData.meshDescriptors.push_back({"house", houseGeometry});
    Geometry knightGeo;
    result = GltfStaticMeshLoader().load("../src/game/assets/knight.glb", knightGeo, true);
    initData.meshDescriptors.push_back({"knight", knightGeo});



    // Define pipeline states needed in our rts game:
    // 1. UIs
    auto uiPipelineState = PipelineState();
    uiPipelineState.id = "ui";
    uiPipelineState.shader = initData.ide ? L"../shaders.hlsl" : L"../shaders.hlsl";
    uiPipelineState.useDepthBuffer = true;
    uiPipelineState.wireframe = false;
    uiPipelineState.inputLayout.addElement({InputElementType::POSITION}).addElement({InputElementType::UV})
                    .addElement({InputElementType::NORMAL});
    initData.pipelineStates.push_back(uiPipelineState);
    

    auto buildingsPipelineState = PipelineState();
    buildingsPipelineState.id = "buildings";
    buildingsPipelineState.shader = initData.ide ? L"../shaders.hlsl" : L"../shaders.hlsl";
    buildingsPipelineState.inputLayout.addElement({InputElementType::POSITION})
                        .addElement({InputElementType::UV}).addElement({InputElementType::NORMAL});
    initData.pipelineStates.push_back(buildingsPipelineState);
    
    return initData;
    
}

FrameSubmission RTSGame::getFrameData()
{

    using namespace DirectX;
    using namespace DirectX::SimpleMath;

    auto frameSubmission = FrameSubmission();

    // Draw some 2D objects
    auto viewSub2D = ViewSubmission();
    viewSub2D.viewMatrix = Matrix::Identity;
    viewSub2D.projectionMatrix = Matrix(XMMatrixOrthographicOffCenterLH(0, 800, 0, 600, 0.1, 100));
    
    auto objData = ObjectRenderData();
    objData.textureId = "hero";
    objData.meshId = "quad";
    objData.inputLayoutId = "ui";
    auto S = Matrix::CreateScale(64, 64, 1);
    for (int i = 0; i < 10; i++) {
        auto T= Matrix::CreateTranslation(40 + (i*70), 200, 10);
        auto W = S * T;
        objData.worldMatrices.push_back(W);
    }
    viewSub2D.objectRenderData.push_back(objData);

    // And a view more, but smaller rects
    auto objDataSmall = ObjectRenderData();
    objDataSmall.textureId = "hero";
    objDataSmall.meshId = "quad";
    objDataSmall.inputLayoutId = "ui";
    int y = 10;
    int x = 0;
    S = Matrix::CreateScale(12, 12, 1);
    for (int i = 0; i < 4000; i++) {
        x++;
        if (i % 48 == 0) {
            y += 20;
            x = 0;
        }
        auto T= Matrix::CreateTranslation(10 + (x*16), y, 10);
        auto W = S * T;
        objDataSmall.worldMatrices.push_back(W);
    }
    //viewSub2D.objectRenderData.push_back(objDataSmall);

    frameSubmission.viewSubmissions.push_back(viewSub2D);

    // Now some 3D objects:
    // House 3d model:
    auto viewSub3D = ViewSubmission();
    viewSub3D.viewMatrix = Matrix(XMMatrixLookAtLH({0, 3, -4}, {0, 0, 0}, {0, 1, 0}));
    viewSub3D.projectionMatrix = Matrix(XMMatrixPerspectiveFovLH(45, 800.0f/600.0f, 0.1, 200));
    
    auto houseObjData = ObjectRenderData();
    houseObjData.textureId = "hero";
    houseObjData.meshId = "knight";
    houseObjData.inputLayoutId = "ui";
    S = Matrix::CreateScale(1, 1, 1);
    static float rotY = 0;
    rotY += 0.000;
    auto R = Matrix::CreateRotationY(rotY);
    for (int i = 0; i < 1; i++) {
        auto T = Matrix::CreateTranslation(0 + (i*5), 0, 3);
        auto W = S * R * T;
        houseObjData.worldMatrices.push_back(W);
    }
    viewSub3D.objectRenderData.push_back(houseObjData);
    frameSubmission.viewSubmissions.push_back(viewSub3D);

    return frameSubmission;
}
