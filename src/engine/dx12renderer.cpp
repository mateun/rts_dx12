#include "dx12renderer.h"
#include <DirectXTex.h>      
#include <D3D12MemAlloc.h>   
#include <directx/d3dx12.h>
#include <d3dcompiler.h>
#include <dxgidebug.h>
#include "appwindow.h"
#include <string>
#include <map>
#include <stdexcept>

static_assert(sizeof(void*) == 8, "x64 only");

static_assert(sizeof(D3D12_SUBRESOURCE_DATA) == 24, "Packing broke D3D12_SUBRESOURCE_DATA");
static_assert(alignof(D3D12_SUBRESOURCE_DATA) == 8, "Bad align");

static_assert(sizeof(DirectX::TexMetadata) % 8 == 0, "TexMetadata packing looks wrong");
static_assert(alignof(DirectX::TexMetadata) == 8, "TexMetadata align wrong");


D3D12_CPU_DESCRIPTOR_HANDLE DX12Renderer::cpuDescriptorHandle(UINT idx)  {
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_srvHeap->GetCPUDescriptorHandleForHeapStart(), idx, m_srvDescriptorSize);
}
D3D12_GPU_DESCRIPTOR_HANDLE DX12Renderer::gpuDescriptorHandle(UINT idx)  {
    return CD3DX12_GPU_DESCRIPTOR_HANDLE(
        m_srvHeap->GetGPUDescriptorHandleForHeapStart(), idx, m_srvDescriptorSize);
}

void DX12Renderer::ThrowIfFailed(HRESULT result) 
{
    if (FAILED(result))
        throw std::exception("failed d3d12 action");
}

void DX12Renderer::initialize(RenderInitData initData)
{
    this->initData = initData;

    createDeviceAndSwapChain();
    createDescriptorHeaps();
    createPipelineStatesNew();
    createCommandLists();
    createTextures();
    createVertexBuffers();
    
}

void DX12Renderer::createVertexBuffers() 
{
    for (auto md : initData.meshDescriptors)
    {
        
        ComPtr<ID3D12Resource> vb;
        ComPtr<ID3D12Resource> ib;
        uint32_t vbSize = md.geometry.vertices.size() * sizeof(float);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);
        CD3DX12_HEAP_PROPERTIES hp(D3D12_HEAP_TYPE_DEFAULT);
        ThrowIfFailed(m_device->CreateCommittedResource(&hp, 
                    D3D12_HEAP_FLAG_NONE, 
                    &desc,
                    D3D12_RESOURCE_STATE_COMMON,
                    nullptr,
                    IID_PPV_ARGS(vb.GetAddressOf())));

        uint32_t ibSize = md.geometry.indices.size() * sizeof(uint32_t);
        auto descIB = CD3DX12_RESOURCE_DESC::Buffer(ibSize);
        ThrowIfFailed(m_device->CreateCommittedResource(&hp, 
                    D3D12_HEAP_FLAG_NONE, 
                    &descIB,
                    D3D12_RESOURCE_STATE_COMMON,
                    nullptr,
                    IID_PPV_ARGS(ib.GetAddressOf())));
        
        
        uploadBufferData(vbSize, md.geometry.vertices.data(), vb, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        uploadBufferData(ibSize, md.geometry.indices.data(), ib, D3D12_RESOURCE_STATE_INDEX_BUFFER);
        D3D12_VERTEX_BUFFER_VIEW vbView;
        D3D12_INDEX_BUFFER_VIEW ibView;
        vbView.BufferLocation = vb->GetGPUVirtualAddress();
        vbView.SizeInBytes = vbSize;
        vbView.StrideInBytes = sizeof(float) * 8;
        ibView.BufferLocation = ib->GetGPUVirtualAddress();
        ibView.SizeInBytes = ibSize;
        ibView.Format = DXGI_FORMAT_R32_UINT;

        meshMap[md.id] = {vbView, ibView, vb, ib, md.geometry.indices.size()};
    }

}

void DX12Renderer::createTextures() {

    for (auto td : initData.textureDescriptors) {
        auto texture = loadTextureFromFile(td.filePath);
        textureMap[td.id] = texture;
    }

}

void DX12Renderer::createCommandLists() {
// Create a command list
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                g_alloc[m_frameIndex].Get(), m_pipelineState.Get(), IID_PPV_ARGS(m_commandList.GetAddressOf())));
    // List must be closed:
    ThrowIfFailed(m_commandList->Close());

    // Create some sync primitives:
    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValue = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

    m_viewport.TopLeftX = 0.0f;
    m_viewport.TopLeftY = 0.0f;
    m_viewport.Width    = static_cast<float>(initData.screenWidth);
    m_viewport.Height   = static_cast<float>(initData.screenHeight);
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;

    m_scissorRect.left   = 0;
    m_scissorRect.top    = 0;
    m_scissorRect.right  = static_cast<LONG>(initData.screenWidth);
    m_scissorRect.bottom = static_cast<LONG>(initData.screenHeight);
    WaitForPreviousFrame();
    

}

void DX12Renderer::createPipelineStatesNew() {

    
    for (auto ps : initData.pipelineStates) {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

        #ifndef NDEBUG
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
        #else
            UINT compileFlags = 0;
        #endif
        
        auto shaderPath = ps.shader;
        
        ThrowIfFailed(D3DCompileFromFile(shaderPath.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", 
                                                compileFlags, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(shaderPath.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", 
                                                compileFlags, 0, &pixelShader, nullptr));

        auto dx12InputLayout = ps.inputLayout.asDX12InputLayout();

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { dx12InputLayout.data(), (uint32_t)dx12InputLayout.size()};
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = {(uint8_t*)vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};
        psoDesc.PS = {(uint8_t*)pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        auto& rt0 = psoDesc.BlendState.RenderTarget[0];
        rt0.BlendEnable = TRUE;
        rt0.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        rt0.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        rt0.BlendOp = D3D12_BLEND_OP_ADD;
        rt0.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt0.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        rt0.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        rt0.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.DSVFormat = depthFormat;          
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        ComPtr<ID3D12PipelineState> pipelineState;
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelineState.GetAddressOf())));
        psos[ps.id] = pipelineState;
                                
    }
       
}

void DX12Renderer::WaitForPreviousFrame()
{
    
    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DX12Renderer::createDescriptorHeaps() 
{

    // Creating the descriptor heaps
    // For rtvs now:
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = frameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Now we have the heaps, create the actual rtvs per frame:
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (int i = 0; i < frameCount; i++) {
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(m_renderTargets[i].GetAddressOf())));
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);

        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_alloc[i])));

    }

    {
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            desc.NumDescriptors = 1; // one depth buffer
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // DSV heap is never shader-visible
            ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_dsvHeap)));

            CD3DX12_RESOURCE_DESC depthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
                depthFormat,
                initData.screenWidth,                    // match swapchain size
                initData.screenHeight,
                1,                               // array size
                1);                              // mips
            depthDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            D3D12_CLEAR_VALUE clearValue = {};
            clearValue.Format               = depthFormat;
            clearValue.DepthStencil.Depth   = 1.0f;
            clearValue.DepthStencil.Stencil = 0;

            auto props =  CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            ThrowIfFailed(m_device->CreateCommittedResource(
                &props,
                D3D12_HEAP_FLAG_NONE,
                &depthDesc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,      // initial state
                &clearValue,
                IID_PPV_ARGS(&m_depthTex)));

            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Format        = depthFormat; // D32_FLOAT (or D24_UNORM_S8_UINT)
            dsvDesc.Flags         = D3D12_DSV_FLAG_NONE;

            auto dsvCPU = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
            m_device->CreateDepthStencilView(m_depthTex.Get(), &dsvDesc, dsvCPU);


        }

        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 128; // room to grow
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // <-- important
        ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvHeap)));
        m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


        ZeroMemory(&desc, sizeof(desc));
        desc.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        desc.NumDescriptors = 16;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_samplerHeap)));

        // Our default sampler
        D3D12_SAMPLER_DESC samp = {};
        samp.Filter   = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samp.MinLOD = 0; samp.MaxLOD = D3D12_FLOAT32_MAX;

        auto sampCPU = m_samplerHeap->GetCPUDescriptorHandleForHeapStart();
        m_device->CreateSampler(&samp, sampCPU);


        // Instancing data structures:
        {

            // Default heap buffer, close to the srv:
            
            {
                auto defaultProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
                auto desc = CD3DX12_RESOURCE_DESC::Buffer(instBytes);
                ThrowIfFailed(m_device->CreateCommittedResource(
                    &defaultProps,
                    D3D12_HEAP_FLAG_NONE,
                    &desc,
                    D3D12_RESOURCE_STATE_COMMON,   
                    nullptr,
                    IID_PPV_ARGS(m_instanceDefault.GetAddressOf())));
            }

            // InstanceData upload buffer
            auto uploadProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            for (UINT i = 0; i < frameCount; ++i) {
                auto desc = CD3DX12_RESOURCE_DESC::Buffer(instBytes);
                ThrowIfFailed(m_device->CreateCommittedResource(
                    &uploadProps,
                    D3D12_HEAP_FLAG_NONE,
                    &desc,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    IID_PPV_ARGS(m_instanceUploadBuffer[i].GetAddressOf())));

                ThrowIfFailed(m_instanceUploadBuffer[i]->Map(0, nullptr, (void**)&m_instanceUploadMapped[i]));
            }


            D3D12_SHADER_RESOURCE_VIEW_DESC isrv = {};
            isrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            isrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            isrv.Format = DXGI_FORMAT_UNKNOWN;
            isrv.Buffer.FirstElement = 0;
            isrv.Buffer.NumElements = MaxInstances;
            isrv.Buffer.StructureByteStride = sizeof(InstanceDataCPU);
            isrv.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

            // Placing it at srv slot 16, away from the textures.. 
            m_device->CreateShaderResourceView(m_instanceDefault.Get(), &isrv, cpuDescriptorHandle(16));

        }

    }
    
    // ------------------------------------------------------------------------
    // Prepare our pipeline and assets:
    // Textures and samplers live in tables on the descriptor heap:
    CD3DX12_DESCRIPTOR_RANGE1 srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // 1 SRV starting at t0

    CD3DX12_DESCRIPTOR_RANGE1 instanceDataSRVRange;
    instanceDataSRVRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // 1 SRV starting at t1

    CD3DX12_DESCRIPTOR_RANGE1 samplerRange;
    samplerRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0); // Samplers starting at s0

    CD3DX12_ROOT_PARAMETER1 rps[6];
    //rps[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);
    rps[0].InitAsConstants(32, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    rps[1].InitAsConstants(16, 1, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    rps[2].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_PIXEL);
    rps[3].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);
    rps[4].InitAsDescriptorTable(1, &samplerRange, D3D12_SHADER_VISIBILITY_PIXEL);
    rps[5].InitAsDescriptorTable(1, &instanceDataSRVRange, D3D12_SHADER_VISIBILITY_VERTEX);
    

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsd = {};
    rsd.Init_1_1(6, rps, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rsd, &signature, &error));
    ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), 
                        signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

    
    

    
    {
        auto uploadProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        const UINT cbSize = (UINT) sizeof(CameraCBData) * 10;   // We may have 10 different camera settings
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
        m_device->CreateCommittedResource(
            &uploadProps,
            D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_frameCB));

        CD3DX12_RANGE rr(0,0);
        m_frameCB->Map(0, &rr, (void**)&mappedCameraCB);
    }

    {
        auto uploadProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(MaterialCBData));
        m_device->CreateCommittedResource(
            &uploadProps,
            D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_materialCB));

            // Permanently map our constantbuffers:
            CD3DX12_RANGE rr(0,0);
            m_materialCB->Map(0, &rr, (void**)&materialCBMapped);

    }
}

void DX12Renderer::GetHardwareAdapter(
    IDXGIFactory1* pFactory,
    IDXGIAdapter1** ppAdapter,
    bool requestHighPerformanceAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        for (
            UINT adapterIndex = 0;
            SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                adapterIndex,
                requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
                IID_PPV_ARGS(&adapter)));
            ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    if(adapter.Get() == nullptr)
    {
        for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }
    
    *ppAdapter = adapter.Detach();
}

void DX12Renderer::createDeviceAndSwapChain()
{
    frameCount = initData.numFrames;

    m_renderTargets.resize(frameCount);
    g_frameFence.resize(frameCount);
    g_alloc.resize(frameCount);
    m_instanceUploadBuffer.resize(frameCount);
    m_instanceUploadMapped.resize(frameCount);

    #ifndef NDEBUG
    {
     
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
        {
            debugController->EnableDebugLayer();
        }

    }
    #endif

    ComPtr<IDXGIFactory7> factory;
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

    // Find a valid dx12 gpu
    ComPtr<IDXGIAdapter1> hardwareAdapter;
    GetHardwareAdapter(factory.Get(), &hardwareAdapter, true);

    // Create a device for it:
    ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));

    // Create a command queue
    D3D12_COMMAND_QUEUE_DESC qdesc = {};
    qdesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&qdesc, 
            IID_PPV_ARGS(m_commandQueue.GetAddressOf())));

    // Swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = frameCount;
    swapChainDesc.Width = initData.screenWidth;
    swapChainDesc.Height = initData.screenHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc = {1, 0};
    

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        initData.hwnd, 
        &swapChainDesc,
        nullptr, nullptr,
        &swapChain
        ));

    ThrowIfFailed(swapChain.As(&m_swapChain));

    // TODO: support fullscreen (+transition)
    // No fullscreen transitions allowed here:
    ThrowIfFailed(factory->MakeWindowAssociation(initData.hwnd, DXGI_MWA_NO_ALT_ENTER));

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

}


DX12Renderer::Texture DX12Renderer::loadTextureFromFile(const std::wstring& fileName) {

    static_assert(sizeof(void*) == 8, "Build x64");
    static_assert(sizeof(D3D12_SUBRESOURCE_DATA) == 24, "Bad packing for D3D12_SUBRESOURCE_DATA");
    static_assert(alignof(D3D12_SUBRESOURCE_DATA) == 8, "Bad align");


    DirectX::ScratchImage image;
    DirectX::TexMetadata metadata;
    ThrowIfFailed(DirectX::LoadFromWICFile(fileName.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, &metadata, image));

    {
        DirectX::ScratchImage flipped;
        // For all subresources (mips/array), not just the first image:
        ThrowIfFailed(FlipRotate(
            image.GetImages(), image.GetImageCount(), metadata,
            DirectX::TEX_FR_FLIP_VERTICAL, flipped));
        image  = std::move(flipped);
        metadata = image.GetMetadata();
    }

    const DirectX::Image* img = image.GetImage(0, 0, 0);
    D3D12_SUBRESOURCE_DATA s{};
    s.pData      = img->pixels;
    s.RowPitch   = static_cast<LONG_PTR>(img->rowPitch);
    s.SlicePitch = static_cast<LONG_PTR>(img->slicePitch);


    CD3DX12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        metadata.format,
        static_cast<UINT64>(metadata.width),
        static_cast<UINT>(metadata.height),
        static_cast<UINT16>(metadata.arraySize),
        static_cast<UINT16>(metadata.mipLevels));

    ComPtr<ID3D12Resource> texture;
    auto defaultProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(m_device->CreateCommittedResource(
        &defaultProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(texture.GetAddressOf())));
   
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Get(), 0, 1);

    auto uploadProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    ComPtr<ID3D12Resource> textureUploadHeap;
    ThrowIfFailed(m_device->CreateCommittedResource(
        &uploadProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(textureUploadHeap.GetAddressOf())));

    // record copy commands
    auto cmdList = createOneTimeCommandList();
    UpdateSubresources(cmdList.cmdList.Get(),
        texture.Get(), textureUploadHeap.Get(),
        0, 0, 1,
        &s);

    // barrier to transition into shader-visible state
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        texture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmdList.cmdList->ResourceBarrier(1, &barrier);
    cmdList.cmdList->Close();
    ID3D12CommandList* lists[] = { cmdList.cmdList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    const UINT64 flushV = ++m_fenceValue;
    m_commandQueue->Signal(m_fence.Get(), flushV);
    if (m_fence->GetCompletedValue() < flushV) {
        m_fence->SetEventOnCompletion(flushV, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DirectX::MakeSRGB(metadata.format);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = (UINT)metadata.mipLevels;

    auto slot = m_nextSrvIndex++;

    // We only have place for 16 textures, 
    // check overflow:
    if (m_nextSrvIndex >= 16)
        throw std::runtime_error("SRV texture heap full");

    m_device->CreateShaderResourceView(texture.Get(), &srvDesc, cpuDescriptorHandle(slot));

    return { texture, slot };

}



void DX12Renderer::uploadBufferData(size_t size, void* data, ComPtr<ID3D12Resource> targetBuffer, 
        D3D12_RESOURCE_STATES finalState) {
    
        // Upload via staging buffer
        ComPtr<ID3D12Resource> vbUpload;
        {
            auto uploadProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            auto bufDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
            ThrowIfFailed(m_device->CreateCommittedResource(
                &uploadProps,
                D3D12_HEAP_FLAG_NONE,
                &bufDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&vbUpload)));

            void* mapped = nullptr;
            CD3DX12_RANGE readRange(0, 0);
            vbUpload->Map(0, &readRange, &mapped);
            memcpy(mapped, data, size);
            vbUpload->Unmap(0, nullptr);
        }

        // Record a short copy-barrier command list (one-shot)
        {

            auto copyList = createOneTimeCommandList();

            copyList.cmdList->CopyBufferRegion(targetBuffer.Get(), 
                                    0, vbUpload.Get(), 0, size);
            auto toVB = CD3DX12_RESOURCE_BARRIER::Transition(
                targetBuffer.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                finalState);
            copyList.cmdList->ResourceBarrier(1, &toVB);
            copyList.cmdList->Close();

            ID3D12CommandList* lists[] = { copyList.cmdList.Get() };
            m_commandQueue->ExecuteCommandLists(1, lists);

            // Sync so the buffer is ready for first draw
            const uint64_t v = ++m_fenceValue;
            m_commandQueue->Signal(m_fence.Get(), v);
            if (m_fence->GetCompletedValue() < v) {
                m_fence->SetEventOnCompletion(v, m_fenceEvent);
                WaitForSingleObject(m_fenceEvent, INFINITE);
            }
                                
        }

}

void DX12Renderer::shutdown() {

    for (UINT i = 0; i < frameCount; ++i) {
        const UINT64 v = g_frameFence[i];
        if (v && m_fence->GetCompletedValue() < v) {
            m_fence->SetEventOnCompletion(v, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
    }

    const UINT64 flushV = ++m_fenceValue;
    m_commandQueue->Signal(m_fence.Get(), flushV);
    if (m_fence->GetCompletedValue() < flushV) {
        m_fence->SetEventOnCompletion(flushV, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

#if defined(_DEBUG)
    #include <dxgidebug.h>
#include "renderer.h"
    {
        ComPtr<IDXGIDebug1> dxgiDebug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
            dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        }
    }
#endif
    
}

void DX12Renderer::postRenderSynch() 
{
    const UINT64 v = ++m_fenceValue;
    m_commandQueue->Signal(m_fence.Get(), v);

    // remember fence value for this backbuffer
    g_frameFence[m_frameIndex] = v;

    // advance swapchain index
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void DX12Renderer::present()
{
    ThrowIfFailed(m_swapChain->Present(0, 0));
}

DX12Renderer::TempCommandList DX12Renderer::createOneTimeCommandList() {
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> cmdList;
    ThrowIfFailed(m_device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
    ThrowIfFailed(m_device->CreateCommandList(
                0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), 
                nullptr, IID_PPV_ARGS(&cmdList)));

    return { allocator, cmdList };
}

void DX12Renderer::doFrame(FrameSubmission frameData) {
    // ComPtr<ID3D12CommandList> cmdList = populateCommandList(frameData);
    // executeCommandList(cmdList);
    // present();
    // postRenderSynch();

}

void DX12Renderer::executeCommandList(ComPtr<ID3D12CommandList> commandList) 
{

    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}

ComPtr<ID3D12CommandList> DX12Renderer::populateCommandList(FrameSubmission frameDataItems) {

     const UINT idx = m_frameIndex;                   // current back buffer
            const UINT64 last = g_frameFence[idx];
            if (last && m_fence->GetCompletedValue() < last) {
                m_fence->SetEventOnCompletion(last, m_fenceEvent);
                WaitForSingleObject(m_fenceEvent, INFINITE);
            }

    // Reset before refill. Allocator and list itself:
    ThrowIfFailed(g_alloc[idx]->Reset());
    ThrowIfFailed(m_commandList->Reset(g_alloc[idx].Get(), m_pipelineState.Get()));

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->SetGraphicsRootConstantBufferView(0, m_frameCB->GetGPUVirtualAddress());

    ID3D12DescriptorHeap* heaps[] = { m_srvHeap.Get(), m_samplerHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // Sampler table (root param 3) -> points at s0 in m_samplerHeap
    auto sampGPU = m_samplerHeap->GetGPUDescriptorHandleForHeapStart();
    m_commandList->SetGraphicsRootDescriptorTable(4, sampGPU);

    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // We use the backbuffer as our render target:
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrier);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    auto dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsv);

    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (auto& frameData : frameDataItems) {
        
            
        // mappedCameraCB->View = frameData.viewMatrix;
            // mappedCameraCB->Proj = frameData.projectionMatrix;

            m_commandList->SetPipelineState(psos["ui"].Get());

            m_commandList->SetGraphicsRoot32BitConstants(1, 16, &frameData.viewMatrix, 0);
            
            for (auto obj : frameData.objectRenderData) {
        
                uint32_t instanceCount = obj.worldMatrices.size();
                // 1) Fill per-frame upload buffer
                {
                    
                    auto* inst = reinterpret_cast<InstanceDataCPU*>(m_instanceUploadMapped[m_frameIndex]);
                    for (UINT i = 0; i < instanceCount; ++i) {
                        inst[i].World = obj.worldMatrices[i];
                        // DirectX::XMMATRIX W = S * DirectX::XMMatrixTranslation(32 + (i * 67), 200, 0.2f);
                        // DirectX::XMStoreFloat4x4(&inst[i].World, W); // transpose if your HLSL expects it
                    }
                    UINT64 copyBytes = instanceCount * sizeof(InstanceDataCPU);
                    // 2) Transition DEFAULT to COPY_DEST, do the copy, transition to SRV
                    {
                        auto toCopy = CD3DX12_RESOURCE_BARRIER::Transition(
                            m_instanceDefault.Get(),
                            D3D12_RESOURCE_STATE_GENERIC_READ,
                            D3D12_RESOURCE_STATE_COPY_DEST);
                        m_commandList->ResourceBarrier(1, &toCopy);
        
                        m_commandList->CopyBufferRegion(
                            m_instanceDefault.Get(), 0,
                            m_instanceUploadBuffer[m_frameIndex].Get(), 0,
                            copyBytes);
        
                        auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(
                            m_instanceDefault.Get(),
                            D3D12_RESOURCE_STATE_COPY_DEST,
                            D3D12_RESOURCE_STATE_GENERIC_READ); 
                        m_commandList->ResourceBarrier(1, &toSRV);
                        m_commandList->SetGraphicsRootDescriptorTable(5, gpuDescriptorHandle(16));
                    }
                }
        
                XMStoreFloat4(&materialCBMapped->tint, DirectX::XMVectorSet(1, 0, 1, 1));   
                m_commandList->SetGraphicsRootConstantBufferView(2, m_materialCB->GetGPUVirtualAddress());
        
                // Diffuse texture SRV table (root param 2) -> points at t0 in m_srvHeap
                auto texture = textureMap[obj.textureId];
                m_commandList->SetGraphicsRootDescriptorTable(3, gpuDescriptorHandle(texture.srvIndex));
        
                // Here we must select the correct vertex buffer according to the current 
                // objects mesh-object-id
                auto meshObject = meshMap[obj.meshId];
                m_commandList->IASetVertexBuffers(0, 1, &meshObject.vbView);
                m_commandList->IASetIndexBuffer(&meshObject.ibView);
                m_commandList->DrawIndexedInstanced(meshObject.indexCount, instanceCount, 0, 0, 0);
            }
        
            // Indicate that the back buffer will now be used to present.
            barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            m_commandList->ResourceBarrier(1, &barrier);
        
            ThrowIfFailed(m_commandList->Close());
        
            
    }

    return m_commandList;

}





std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayout::asDX12InputLayout()
{
    std::vector<D3D12_INPUT_ELEMENT_DESC> descs;

    uint32_t oldOffset = 0;
    uint32_t newOffset = 0;
    std::vector<std::string> semanticNames;
    
    for (auto& elem: elements) {
        oldOffset = newOffset;
        const char* sem = nullptr;
        DXGI_FORMAT format;
        switch (elem.type) {
            case InputElementType::POSITION: sem = "POSITION"; format = DXGI_FORMAT_R32G32B32_FLOAT; newOffset += 12; break;
            case InputElementType::UV: sem = "TEXCOORD"; format = DXGI_FORMAT_R32G32_FLOAT; newOffset += 8;  break;
            case InputElementType::NORMAL: sem  = "NORMAL"; format = DXGI_FORMAT_R32G32B32_FLOAT; newOffset += 12; break;
        };

        descs.push_back(
            
            {sem, 0, format, 0, oldOffset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
        );
   
    }

   
   
   
   
   
   
    return descs;
}

