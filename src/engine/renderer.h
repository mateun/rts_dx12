#pragma once
#include <vector>
#include <string>
#include <Windows.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include "geometry.h"

enum class InputElementType
{
    POSITION,
    UV,
    NORMAL

};

struct InputLayoutElement {
    InputElementType type;
};

class InputLayout 
{
    public:
        InputLayout& addElement(InputLayoutElement element);
        std::vector<D3D12_INPUT_ELEMENT_DESC> asDX12InputLayout();

    private:
        std::vector<InputLayoutElement> elements;

};

struct MeshDescriptor 
{
    std::string id;
    Geometry geometry;

};

// Describes a texture which is created on the GPU
// The id is necessary so the renderer can 
// store an association
struct TextureDescriptor 
{
    std::string id;
    std::wstring filePath;

};

// PipelineState contains 
// all needed shaders and 
// rasterstates etc.
struct PipelineState {

    // This is the id which we will refer to in the FrameData,
    // so the renderer knows which PSO to bind:
    std::string id;         
    std::wstring shader;
    bool useDepthBuffer = true;
    bool useStencilBuffer = false;
    bool wireframe = false;
    InputLayout inputLayout;

};

struct RenderInitData {

    std::vector<PipelineState> pipelineStates;
    uint32_t numFrames = 0;
    uint32_t screenWidth = 0;
    uint32_t screenHeight = 0;
    bool fullscreen = false;
    HWND hwnd;
    bool ide = false;
    std::vector<TextureDescriptor> textureDescriptors;
    std::vector<MeshDescriptor> meshDescriptors;

};

struct ObjectRenderData 
{
    DirectX::XMFLOAT4X4 worldMatrix;
    std::string textureId;

};

struct FrameData {
    DirectX::XMFLOAT4X4 viewMatrix;
    DirectX::XMFLOAT4X4 projectionMatrix;
    std::vector<ObjectRenderData> objectRenderData;



};

class Renderer {

    public:
        virtual void initialize(RenderInitData initData) = 0;


    protected:
        RenderInitData initData;

};