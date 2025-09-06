#pragma once
#include <vector>
#include <string>
#include <Windows.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <d3d11.h>
#include <DirectXTK/SimpleMath.h>
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
        std::vector<D3D11_INPUT_ELEMENT_DESC> asDX11InputLayout();
        uint32_t stride();

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
    std::string filePath;

};

struct FontDescriptor
{
    std::string id;
    std::string fontFilePath;
    float size;
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

struct SnippetDescriptor
{
    std::string fontId;
    std::string snippetId;
    std::string text;

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
    std::vector<FontDescriptor> fontDescriptors;
    std::vector<SnippetDescriptor> snippetDescriptors;

};

struct TextRenderData 
{
    std::vector<DirectX::SimpleMath::Matrix> worldMatrices;
    std::string snippetId;
    std::string updatedText = "";
  
};

struct ObjectRenderData 
{
    // The count of world matrices 
    // decides how many instances we have of this object.
    // So if you want 1000 instances of this object, 
    // you must add 1000 world matrices. 
    // The engine will instance-batch all those objects.
    std::vector<DirectX::SimpleMath::Matrix> worldMatrices;
    std::string textureId;
    std::string meshId;
    std::string inputLayoutId;


};

// Every ViewSubmission
struct ViewSubmission
{
    DirectX::SimpleMath::Matrix viewMatrix;
    DirectX::SimpleMath::Matrix projectionMatrix;
    std::vector<ObjectRenderData> objectRenderData;
    std::vector<TextRenderData> textRenderData;

};

struct FrameSubmission {
    std::vector<ViewSubmission> viewSubmissions;

};

class Renderer {

    public:
        virtual void initialize(RenderInitData initData) = 0;
        virtual void doFrame(FrameSubmission frameData) = 0;

};