#pragma once
#include "renderer.h"
#include <d3d11.h>
#include <map>
#include "comptr.h"
#include "shader.h"
#include <stb_truetype.h>

struct Mesh;
struct ConstantBufferDesc;
struct StructuredBufferDesc;
struct Texture;
struct Image;
struct BufferUpdateDesc;
struct Font;
struct TextSnippet;
class DX11Renderer : public Renderer {

    public:
        virtual void initialize(RenderInitData initData) override;
        void doFrame(FrameSubmission frameData) override;

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
        void bindTexture(uint32_t slot, Texture &texture);
        void bindInputLayout(ComPtr<ID3D11InputLayout> inputLayout);
        void bindShader(const ShaderProgram *shaderProgram);
        void bindBackBuffer(int x, int y, int width, int height);
        void createDefaultSamplerState();
        void uploadConstantBufferData(ConstantBufferDesc constantBuffer);
        void uploadStructuredBufferData(StructuredBufferDesc desc);
        void renderTextIntoQuad(const std::string &snippedId, const std::string &fontId, const std::string &text);
        void updateBuffer(BufferUpdateDesc desc);
        Font createFont(const std::string& fontPath, int size);
        Geometry *renderTextIntoQuad(const std::string &fontId, const std::string &text, Geometry *oldMesh);
        ShaderProgram createShaderProgram(const std::wstring &filePath);
        Texture createTexture(uint8_t *pixels, uint32_t width, uint32_t height, uint32_t numChannels = 4, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
        Image loadImagePixels(const std::string &filePath);
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
        HWND hwnd;
        int screenWidth = 0;
        int screenHeight = 0;



        std::map<std::string, Mesh> meshMap;
        std::map<std::string, Texture> textureMap;
        std::map<std::string, Font> fontMap;
        std::map<std::string, TextSnippet> snippetMap;
        std::map<std::string, ShaderProgram> shaderMap;
        std::map<std::string, ComPtr<ID3D11InputLayout>> dxInputLayoutMap;
        std::map<std::string, InputLayout> inputLayoutMap;

        const int maxInstances = 50000;

};

struct Texture {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> srv;
};

struct Image {
    uint8_t* pixels;
    int w;
    int h;
    int numChannels;
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

struct BufferUpdateDesc {
    ComPtr<ID3D11Buffer> buffer;
    void* data = nullptr;
    size_t size;

};

struct ConstantBufferDesc {


    ComPtr<ID3D11Buffer> buffer;
    const void* bufferData = nullptr;
    size_t size = 0;
    uint32_t slot = 0;
    ShaderType shaderType = ShaderType::Vertex;

};

struct Font {
    Texture atlasTexture;
    float maxDescent = std::numeric_limits<float>::max();
    float lineHeight = std::numeric_limits<float>::min();
    float baseLine = 0.0f;
    std::vector<stbtt_bakedchar> bakedChars;

};


struct TextSnippet 
{
    Mesh mesh;
    Geometry geometry;
    std::string fontId;
};