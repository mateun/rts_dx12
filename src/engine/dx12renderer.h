#include "renderer.h"
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl.h>
using namespace Microsoft::WRL;

class DX12Renderer : public Renderer {

    public:
        void initialize(RenderInitData initData) override;
        
        ComPtr<ID3D12CommandList> populateCommandList();
        void postRenderSynch();
        void present();
        void executeCommandList(ComPtr<ID3D12CommandList> commandList);
        void shutdown();
        
    private:
        

        struct InstanceDataCPU { 
            DirectX::XMFLOAT4X4 World; 
        };

        struct Texture {
            ComPtr<ID3D12Resource> texture;
            uint32_t srvIndex = 0;
        };

        struct alignas(256) MaterialCBData {
            DirectX::XMFLOAT4 tint;

        };

        struct alignas(256) ObjectCBData {
            DirectX::XMFLOAT4X4 World;
        };

        struct alignas(256) FrameCBData {  // alignas keeps VA aligned for safety
                DirectX::XMFLOAT4X4 View;
                DirectX::XMFLOAT4X4 Proj;
                // padding happens naturally up to 256 bytes
        };

        struct TempCommandList {
            ComPtr<ID3D12CommandAllocator> allocator;
            ComPtr<ID3D12GraphicsCommandList> cmdList;
        };


        void ThrowIfFailed(HRESULT result);
        void createDeviceAndSwapChain();
        void createDescriptorHeaps();
        void createPipelineStates();
        void WaitForPreviousFrame();
        void GetHardwareAdapter(IDXGIFactory1 *pFactory, IDXGIAdapter1 **ppAdapter, bool requestHighPerformanceAdapter);
        void uploadBufferData(size_t size, void *data, ComPtr<ID3D12Resource> targetBuffer, D3D12_RESOURCE_STATES finalState);
        

        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle(UINT idx);
        D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptorHandle(UINT idx);
        TempCommandList createOneTimeCommandList();

        Texture loadTextureFromFile(const std::wstring &fileName);

    private:
        uint8_t frameCount = 0;
        D3D12_VIEWPORT m_viewport;
        D3D12_RECT m_scissorRect;
        ComPtr<IDXGISwapChain3> m_swapChain;
        ComPtr<ID3D12Device> m_device;
        
        std::vector<ComPtr<ID3D12Resource>> m_renderTargets;
        std::vector<UINT64> g_frameFence = {};
        std::vector<ComPtr<ID3D12CommandAllocator>> g_alloc;
        std::vector<ComPtr<ID3D12Resource>> m_instanceUploadBuffer;
        std::vector<uint8_t*> m_instanceUploadMapped;

        ComPtr<ID3D12CommandAllocator> m_commandAllocator;
        ComPtr<ID3D12CommandQueue> m_commandQueue;
        ComPtr<ID3D12RootSignature> m_rootSignature;
        ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
        ComPtr<ID3D12DescriptorHeap> m_srvHeap; 
        ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
        ComPtr<ID3D12DescriptorHeap> m_samplerHeap;
        ComPtr<ID3D12PipelineState> m_pipelineState;
        ComPtr<ID3D12GraphicsCommandList> m_commandList;
        ComPtr<ID3D12Resource> m_frameCB;
        ComPtr<ID3D12Resource> m_objectCB;
        ComPtr<ID3D12Resource> m_materialCB;
        
        UINT m_nextSrvIndex = 0;
        ComPtr<ID3D12Resource> m_instanceDefault;
        static const UINT MaxInstances = 50000;
        const UINT instBytes = MaxInstances * sizeof(InstanceDataCPU);
        

        DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT; // depth only
        FrameCBData* mapped = nullptr;
        ObjectCBData* objectCBMapped = nullptr;
        MaterialCBData* materialCBMapped = nullptr;
        UINT m_rtvDescriptorSize;
        ComPtr<ID3D12Resource> m_depthTex;

        
        UINT m_srvDescriptorSize = 0;

        // App resources.
        ComPtr<ID3D12Resource> m_vertexBuffer;
        ComPtr<ID3D12Resource> m_indexBuffer;
        D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
        D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

        // Synchronization objects.
        UINT m_frameIndex;
        HANDLE m_fenceEvent;
        ComPtr<ID3D12Fence> m_fence;
        UINT64 m_fenceValue;


        // TODO remove. 
        // Only temp debug, no textures live in the engine!
        Texture m_heroTexture = {};
        Texture m_enemy1Texture = {};
        Texture m_enemy2Texture = {};

};