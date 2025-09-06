#include "rts_game.h"
#include "../engine/appwindow.h"
#include "../engine/renderer.h"
#include "../engine/geometry.h"
#include "../engine/asset_importer.h"
#include "../engine/game_util.h"
#include <filesystem>

Game* getGame() {
    return new RTSGame();
}

RenderInitData RTSGame::getInitData(CommandLine cmdline, Window* window)
{
    this->window = window;
    bool ide = false;
    if (cmdline.argc >=  2 && strcmp(cmdline.args[1],"ide") == 0) {
        ide = true;
    }
    auto initData = RenderInitData();
    initData.ide = ide;
    initData.hwnd = window->hwnd;
    initData.screenWidth = window->width;
    initData.screenHeight = window->height;
    initData.numFrames = 3;
    initData.textureDescriptors.push_back({"hero", "../src/game/assets/hero.png"});
    initData.textureDescriptors.push_back({"enemy1", "../src/game/assets/enemy1.png"});
    initData.textureDescriptors.push_back({"default", "../src/game/assets/default_texture.png"});
    initData.textureDescriptors.push_back({"wood_icon", "../src/game/assets/wood_icon.png"});
    initData.fontDescriptors.push_back({"consola16", "../src/game/assets/consola.ttf", 16.0f});
    initData.fontDescriptors.push_back({"consola32", "../src/game/assets/consola.ttf", 32.0f});
    initData.snippetDescriptors.push_back({"consola16", "hello_world_snippet", "hello world placeholder xxxxxxxxxxx"});
    initData.snippetDescriptors.push_back({"consola16", "wood_amount", "Wood: 999999999999"});
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
    uiPipelineState.shader = L"../shaders/unlit_2d.hlsl";
    uiPipelineState.useDepthBuffer = true;
    uiPipelineState.wireframe = false;
    uiPipelineState.inputLayout.addElement({InputElementType::POSITION}).addElement({InputElementType::UV})
                    .addElement({InputElementType::NORMAL});
    initData.pipelineStates.push_back(uiPipelineState);
    
    auto buildingsPipelineState = PipelineState();
    buildingsPipelineState.id = "static_meshes";
    buildingsPipelineState.shader = L"../shaders/shaders.hlsl";
    buildingsPipelineState.inputLayout.addElement({InputElementType::POSITION})
                        .addElement({InputElementType::UV}).addElement({InputElementType::NORMAL});
    initData.pipelineStates.push_back(buildingsPipelineState);
    
    return initData;
    
}

FrameSubmission RTSGame::getFrameData()
{
    using namespace DirectX;
    using namespace DirectX::SimpleMath;

    for (auto& e : frameEvents) {
        // if (e.name == "resized") {
        //     auto dim = (Vector2*) e.data;

        // }
    }

    auto frameSubmission = FrameSubmission();

    // Draw some 2D objects
    auto viewSub2D = ViewSubmission();
    viewSub2D.viewMatrix = Matrix::Identity;
    viewSub2D.projectionMatrix = Matrix(XMMatrixOrthographicOffCenterLH(0, window->width, 0, window->height, 0.1, 100));
    
    auto objData = ObjectRenderData();
    objData.textureId = "hero";
    objData.meshId = "quad";
    objData.inputLayoutId = "ui";
    objData.textureId = "hero";
    auto S = Matrix::CreateScale(64, 64, 1);
    for (int i = 0; i < 10; i++) {
        auto T= Matrix::CreateTranslation(40 + (i*70), 200, 10);
        auto W = S * T;
        objData.worldMatrices.push_back(W);
    }
    viewSub2D.objectRenderData.push_back(objData);

    // And a view more, but smaller rects
    {
        auto objDataSmall = ObjectRenderData();
        objDataSmall.textureId = "enemy1";
        objDataSmall.meshId = "quad";
        objDataSmall.inputLayoutId = "ui";
        int y = 10;
        int x = 0;
        S = Matrix::CreateScale(12, 12, 1);
        for (int i = 0; i < 80; i++) {
            x++;
            if (i % 48 == 0) {
                y += 20;
                x = 0;
            }
            auto T= Matrix::CreateTranslation(10 + (x*16), y, 10);
            auto W = S * T;
            objDataSmall.worldMatrices.push_back(W);
        }
        viewSub2D.objectRenderData.push_back(objDataSmall);
    }

    // Wood icon
    auto objDataSmall = createObjectRenderData("wood_icon", "quad", "ui", 
            {Vector3{50, window->height - 50.0f, 1}}, Vector3(32, 32 ,1));
    viewSub2D.objectRenderData.push_back(objDataSmall);

    // Wood text
    auto woodAmountText = TextRenderData();
    woodAmountText.snippetId = "wood_amount";
    static int frame =1;
    frame++;
    woodAmountText.updatedText = std::to_string(frame);
    woodAmountText.worldMatrices.push_back(Matrix::CreateTranslation(100, 100, 1));
    viewSub2D.textRenderData.push_back(woodAmountText);

    // Now some 3D objects:
    // House 3d model:
    auto viewSub3D = ViewSubmission();
    viewSub3D.viewMatrix = Matrix(XMMatrixLookAtLH({0, 30, -15}, {0, 0, 0}, {0, 1, 0}));
    float aspectRatio = (float) window->width / (float) window->height;
    viewSub3D.projectionMatrix = Matrix(XMMatrixPerspectiveFovLH(45, aspectRatio, 0.1, 200));

    {
        auto knightObjData = ObjectRenderData();
        knightObjData.textureId = "default";
        knightObjData.meshId = "knight";
        knightObjData.inputLayoutId = "static_meshes";
        S = Matrix::CreateScale(1, 1, 1);
        static float rotY = 0;
        rotY += 0.000;
        auto R = Matrix::CreateRotationY(rotY);
        for (int i = 0; i < 1; i++) {
            auto T = Matrix::CreateTranslation(0 + (i*5), 0, 3);
            auto W = S * R * T;
            knightObjData.worldMatrices.push_back(W);
        }
        viewSub3D.objectRenderData.push_back(knightObjData);
    }

    {
        auto houseObjData = ObjectRenderData();
        houseObjData.textureId = "default";
        houseObjData.meshId = "house";
        houseObjData.inputLayoutId = "static_meshes";
        auto S = Matrix::CreateScale(1, 1, 1);
        static float rotY = 0;
        rotY += 0.000;
        auto R = Matrix::CreateRotationY(rotY);
        for (int i = 0; i < 3; i++) {
            auto T = Matrix::CreateTranslation(-25 + (i*25), 0, 8);
            auto W = S * R * T;
            houseObjData.worldMatrices.push_back(W);
        }
        viewSub3D.objectRenderData.push_back(houseObjData);
    }

    frameSubmission.viewSubmissions.push_back(viewSub3D);
    frameSubmission.viewSubmissions.push_back(viewSub2D);

    return frameSubmission;
}
