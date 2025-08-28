#include "rts_game.h"
#include "../engine/appwindow.h"
#include "../engine/renderer.h"

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

    // Define pipeline states needed in our rts game:
    // 1. UIs
    auto uiPipelineState = PipelineState();
    uiPipelineState.id = "ui";
    uiPipelineState.shader = L"../../shaders.hlsl";
    uiPipelineState.useDepthBuffer = true;
    uiPipelineState.wireframe = false;
    uiPipelineState.inputLayout.addElement({InputElementType::POSITION}).addElement({InputElementType::UV});
    initData.pipelineStates.push_back(uiPipelineState);

    auto buildingsPipelineState = PipelineState();
    buildingsPipelineState.id = "buildings";
    buildingsPipelineState.shader = L"../../shaders.hlsl";
    buildingsPipelineState.inputLayout.addElement({InputElementType::POSITION})
                        .addElement({InputElementType::UV});
    //initData.pipelineStates.push_back(buildingsPipelineState);

    return initData;
    
}

FrameData RTSGame::getFrameData()
{
    return FrameData();
}
