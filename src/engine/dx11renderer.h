#pragma once
#include "renderer.h"
#include <d3d11.h>
#include <map>
#include "comptr.h"
#include "shader.h"

struct Mesh;
struct ConstantBufferDesc;
struct StructuredBufferDesc;
class DX11Renderer : public Renderer {

    public:
        virtual void initialize(RenderInitData initData) override;
        
        void doFrame(FrameSubmission frameData) override;
        virtual ~DX11Renderer();
        
    protected:
        void ThrowIfFailed(HRESULT result);
        void clearBackBuffer(float r, float g, float b, float a);
        void flipBackbuffer();
        void printDXGIError(HRESULT hr);
        void resizeSwapChain(HWND hwnd, int width, int height);
        void createDefaultDepthStencilBuffer(int width, int height);
        void setViewport(int originX, int originY, int width, int height);
        void createDefaultBlendState();
        void createDefaultRasterizerState();
        void bindInputLayout(ComPtr<ID3D11InputLayout> inputLayout);
        void bindShader(const ShaderProgram *shaderProgram);
        void bindBackBuffer(int x, int y, int width, int height);
        void createDefaultSamplerState();
        void uploadConstantBufferData(ConstantBufferDesc constantBuffer);
        void uploadStructuredBufferData(StructuredBufferDesc desc);
        ShaderProgram createShaderProgram(const std::wstring &filePath);
        ComPtr<ID3D11DeviceChild> createShader(const std::wstring &filePath, ShaderType shaderType);
        ComPtr<ID3D11Buffer> createBuffer(void *data, int size, D3D11_USAGE bufferUsage, D3D11_BIND_FLAG bindFlags, uint32_t miscFlags = 0, uint32_t structuredByteStrid = 0);
        ComPtr<ID3D11InputLayout> createInputLayout(InputLayout attributeDescriptions, ShaderProgram *shaderProgram);
        ComPtr<ID3D11ShaderResourceView> createShaderResourceViewForBuffer(ComPtr<ID3D11Buffer> buffer, uint32_t numInstances);

    protected:
        ComPtr<ID3D11Device> device_ = nullptr;
        ComPtr<ID3D11DeviceContext> ctx = nullptr;
        ComPtr<IDXGISwapChain> swapChain = nullptr;
        
        ComPtr<ID3D11Buffer> objectTransformBuffer;
        ComPtr<ID3D11Buffer> cameraBuffer;
        ComPtr<ID3D11Buffer> instanceBuffer;
        ComPtr<ID3D11ShaderResourceView> instanceSRV;

        ComPtr<ID3D11Texture2D> backBuffer = nullptr;
        ComPtr<ID3D11RenderTargetView> renderTargetView = nullptr;
        ComPtr<ID3D11Texture2D> depthStencilBuffer = nullptr;
        ComPtr<ID3D11DepthStencilView> depthStencilView = nullptr;
        ComPtr<ID3D11DepthStencilState> m_DepthStencilState = nullptr;
        ComPtr<ID3D11SamplerState> defaultSamplerState = nullptr;
        ComPtr<ID3D11BlendState> opaqueBlendState = nullptr;
        ComPtr<ID3D11BlendState> blendState = nullptr;
        ComPtr<ID3D11RasterizerState> rasterStateSolid = nullptr;
        ComPtr<ID3D11Debug> debugger = nullptr;
        int screenWidth = 0;
        int screenHeight = 0;



        std::map<std::string, Mesh> meshMap;
        std::map<std::string, ShaderProgram> shaderMap;
        std::map<std::string, ComPtr<ID3D11InputLayout>> dxInputLayoutMap;
        std::map<std::string, InputLayout> inputLayoutMap;

        const int maxInstances = 50000;

};

struct Texture {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> srv;
};

struct Mesh {
    ComPtr<ID3D11Buffer> vb;
    ComPtr<ID3D11Buffer> ib;
    uint64_t indexCount;
    uint32_t stride;

};

struct CameraCB {
    DirectX::SimpleMath::Matrix view;
    DirectX::SimpleMath::Matrix projection;
};

struct ObjectTransformCB 
{

    DirectX::SimpleMath::Matrix world;
};

struct alignas(16) InstanceData
{
    DirectX::SimpleMath::Matrix world;
};

struct StructuredBufferDesc 
{
    ComPtr<ID3D11Buffer> buffer;
    ComPtr<ID3D11ShaderResourceView> srv;
    std::vector<InstanceData> data;
    uint32_t slot;

};

struct ConstantBufferDesc {


    ComPtr<ID3D11Buffer> buffer;
    const void* bufferData = nullptr;
    size_t size = 0;
    uint32_t slot = 0;
    ShaderType shaderType = ShaderType::Vertex;

};