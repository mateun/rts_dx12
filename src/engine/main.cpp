

#include <d3d11.h>
#include <dxgi1_6.h>
#include <iostream>
#include "comptr.h"
#include <DirectXTex.h>      
#include <directxtk/SimpleMath.h>
#include <d3dcompiler.h>
#include <dxgidebug.h>
#include "appwindow.h"
#include <string>
#include "dx11renderer.h"
#include "engine.h"
#include "game.h"

int main(int argc, char ** args) {

     auto window = createAppWindow(800, 600, false);

    auto game = getGame();
    auto initData = game->getInitData({argc, args}, &window);
    auto renderer = DX11Renderer();
    renderer.initialize(initData);

    auto running = true;
    while (running) {
        auto events = pollWindowMessages(window);
        game->setEvents(events);
        for (auto &e : events) 
        {
            if (e->name == "quit") {
                running = false;
            }

            if (e->name == "resized") {
                auto newDimension = (DirectX::SimpleMath::Vector2*) e->data;
                window.width = newDimension->x;
                window.height= newDimension->y;
           }
        }
        auto frameData = game->getFrameData();
        renderer.doFrame({frameData});
       
       
        

    }

}

#ifdef USE_DX12
int main12(int argc, char** args) {
   
    auto window = createAppWindow(800, 600, false);

    auto game = getGame();
    auto initData = game->getInitData({argc, args}, window);
    auto renderer = DX12Renderer();
    renderer.initialize(initData);

    auto running = true;
    while (running) {
        auto msgs = pollWindowMessages(window);
        for (auto &m : msgs) 
        {
            if (m == "quit") {
                running = false;
            }
        }

        auto frameData = game->getFrameData();
       
        ComPtr<ID3D12CommandList> cmdList = renderer.populateCommandList(frameData);
        renderer.executeCommandList(cmdList);

        renderer.present();
        renderer.postRenderSynch();

    }

   
    renderer.shutdown();

    return 0;

}
#endif
