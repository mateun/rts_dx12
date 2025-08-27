
#include <vector>
#include <string>
#include <Windows.h>
#include <DirectXMath.h>

struct RenderInitData {

    std::vector<std::string> shaderFileNames;
    // TODO not sure if this really makes sense on init level?!
    // Cameras may be multiple and they change during gameplay?!
    DirectX::XMFLOAT4X4 viewMatrix;
    uint32_t numFrames = 0;
    uint32_t screenWidth = 0;
    uint32_t screenHeight = 0;
    bool fullscreen = false;
    HWND hwnd;
    bool ide = false;



};

class Renderer {

    public:
        virtual void initialize(RenderInitData initData) = 0;


    protected:
        RenderInitData initData;

};