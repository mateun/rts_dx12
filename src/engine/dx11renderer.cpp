#include "dx11renderer.h"
#include <iostream>
#include <comdef.h>
#include "shader.h"
#include <string>
#include <d3dcompiler.h>
#include <optional>
#include <DirectXTK/SimpleMath.h>
#include "renderer.h"
#include "../lib/include/stb_image.h"

extern DirectX::SimpleMath::Vector2 resizedDimension; 

void DX11Renderer::initialize(RenderInitData initData) 
{
    screenWidth = initData.screenWidth;
    screenHeight = initData.screenHeight;
    hwnd = initData.hwnd;
    D3D_FEATURE_LEVEL featureLevels =  D3D_FEATURE_LEVEL_11_1;
    UINT flags = 0;
    #ifdef _DEBUG 
        flags |= D3D11_CREATE_DEVICE_DEBUG;
    #endif

    auto result = D3D11CreateDevice(NULL, 
        D3D_DRIVER_TYPE_HARDWARE, 
        NULL, flags, &featureLevels, 
        1, D3D11_SDK_VERSION,                               
        &device_, NULL, &ctx);

    if (FAILED(result)) {
        exit(1);
    }

    // Gather the debug interface:
    #ifdef _DEBUG
    if (flags == D3D11_CREATE_DEVICE_DEBUG) {
        result = device_->QueryInterface(__uuidof(ID3D11Debug), (void**)debugger.GetAddressOf());
        if (FAILED(result)) {
            OutputDebugString(L"debuger creation failed\n");
            exit(1);
        }
    }
    #endif

    UINT qualityLevel;
    device_->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 
        4, &qualityLevel);
    assert(qualityLevel > 0);


    // Creating our swapchain:
    ComPtr<IDXGIDevice> dxgiDevice = nullptr;
    result = device_->QueryInterface(__uuidof(IDXGIDevice), 
        (void **)dxgiDevice.GetAddressOf());

    if (FAILED(result)) {
        exit(1);
    }

    ComPtr<IDXGIAdapter> dxgiAdapter = nullptr;
    result = dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf());
    if (FAILED(result)) {
        exit(1);
    }

    ComPtr<IDXGIOutput> output = nullptr;
    result = dxgiAdapter->EnumOutputs(0, output.GetAddressOf());
    if (SUCCEEDED(result)) {
        UINT numModes = 0;
        // First, get the number of modes
        result = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0, &numModes, nullptr);
        if (SUCCEEDED(result) && numModes > 0) {
            std::vector<DXGI_MODE_DESC> modeList(numModes);
            // Retrieve the full list
            result = output->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0, &numModes, modeList.data());
            if (SUCCEEDED(result)) {
                std::cout << "Supported _SRGB modes found: " << numModes << std::endl;

                bool modeFound = false;
                for (const auto& mode : modeList) {

                    // Check for exact or close match
                    if (mode.Width == initData.screenWidth && mode.Height == initData.screenHeight &&
                        mode.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB &&
                        mode.RefreshRate.Numerator / (float)mode.RefreshRate.Denominator >= 59.0f &&
                        mode.RefreshRate.Numerator / (float)mode.RefreshRate.Denominator <= 61.0f) {
                        modeFound = true;
                        std::cout << "Matching mode found!" << std::endl;
                        std::cout << "Width: " << mode.Width << ", Height: " << mode.Height
                              << ", Refresh: " << mode.RefreshRate.Numerator << "/"
                              << mode.RefreshRate.Denominator << std::endl;
                        break;
                        }
                }
                if (!modeFound) {
                    std::cerr << "No exact match for " << initData.screenWidth << "x" << 
                        initData.screenHeight << " at ~60 Hz" << std::endl;
                }
            } else {
                std::cerr << "GetDisplayModeList failed: 0x" << std::hex << result << std::endl;
            }
        } else {
            std::cerr << "Format not supported or no modes: 0x" << std::hex << result << std::endl;
        }
    }

    ComPtr<IDXGIFactory> factory = nullptr;
    result = dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)factory.GetAddressOf());
    if (FAILED(result)) {
        exit(1);
    }

    DXGI_SWAP_CHAIN_DESC sd;
    sd.BufferDesc.Width  = initData.screenWidth;
    sd.BufferDesc.Height = initData.screenHeight;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.SampleDesc.Count   = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount  = 1;
    sd.OutputWindow = initData.hwnd;
    sd.Windowed     = true;
    sd.SwapEffect   = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags        = 0;
    result = factory->CreateSwapChain(device_.Get(), &sd, swapChain.GetAddressOf());
    if (FAILED(result)) {
        printDXGIError(result);
        exit(1);
    }

    resizeSwapChain(initData.hwnd, initData.screenWidth, initData.screenHeight);

    createDefaultRasterizerState();
    createDefaultSamplerState();
    createDefaultBlendState();
    
    bindBackBuffer(0, 0, initData.screenWidth, initData.screenHeight);

    for (auto& pso : initData.pipelineStates) {
        auto shader = createShaderProgram(pso.shader);
        auto inputLayout = createInputLayout(pso.inputLayout, &shader);
        shaderMap[pso.id] = shader;
        dxInputLayoutMap[pso.id] =  inputLayout ;
        inputLayoutMap[pso.id] = { pso.inputLayout };
    }

    

    objectTransformBuffer = createBuffer(nullptr, sizeof(ObjectTransformCB), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER);
    cameraBuffer = createBuffer(nullptr, sizeof(CameraCB), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER);
    instanceBuffer = createBuffer(nullptr, sizeof(InstanceData) * maxInstances, 
                D3D11_USAGE_DYNAMIC, D3D11_BIND_SHADER_RESOURCE, 
                D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, 
                sizeof(InstanceData));
    instanceSRV = createShaderResourceViewForBuffer(instanceBuffer, maxInstances);

    // Now create gpu resources for the assets:
    for (auto& md : initData.meshDescriptors)  {
        auto vb = createBuffer(md.geometry.vertices.data(), md.geometry.vertices.size() * sizeof(float), 
                    D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER);
        auto ib = createBuffer(md.geometry.indices.data(), md.geometry.indices.size() * sizeof(uint32_t), 
                    D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER);
        
        meshMap[md.id] = Mesh {vb, ib, md.geometry.indices.size()};
    }

    for (auto& td : initData.textureDescriptors) {
        auto image = loadImagePixels(td.filePath);
        auto texture = createTexture(image.pixels, image.w, image.h);
        textureMap[td.id] = texture;
    }

    // Text rendering
    {
        // Create separate pipeline state layout for text rendering
        auto textShader = createShaderProgram(L"../shaders/text.hlsl");
        auto textInputLayout = InputLayout().addElement({InputElementType::POSITION}).addElement({InputElementType::UV});
        auto inputLayout = createInputLayout(textInputLayout, &textShader);
        
        shaderMap["text"] = textShader;
        dxInputLayoutMap["text"] =  inputLayout ;
        inputLayoutMap["text"] = { textInputLayout };

        for (auto& fd : initData.fontDescriptors) {
            auto font = createFont(fd.fontFilePath, fd.size);
            fontMap[fd.id] = font;
        }

        for (auto& sd : initData.snippetDescriptors) {
            renderTextIntoQuad(sd.snippetId, sd.fontId, sd.text);
        }
    }

}

void DX11Renderer::renderTextIntoQuad(const std::string& snippetId, const std::string& fontId, const std::string& text) {
    std::optional<TextSnippet> oldSnippet;
    if (snippetMap.find(snippetId) != snippetMap.end()) {
        oldSnippet = snippetMap[snippetId];
        oldSnippet.value().geometry.vertices.clear();
        oldSnippet.value().geometry.indices.clear();
        oldSnippet.value().geometry.positions.clear();
        oldSnippet.value().geometry.uvs.clear();
    }

    const Font& font = fontMap[fontId];

    auto textSnippet = oldSnippet.has_value() ? oldSnippet : TextSnippet();
    textSnippet.value().fontId = fontId;
    float penX = 0, penY = 0;
    float minX = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float maxY = -std::numeric_limits<float>::max();
    float baseline = font.baseLine;
    int charCounter = 0;
    for (auto c : text)
    {
        float tempPenY = 0;
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(font.bakedChars.data(), 512, 512, c - 32, &penX, &tempPenY, &q, 1);

        float pixel_aligned_x0 = std::floor(q.x0 + 0.5f);
        float pixel_aligned_y0 = std::floor(q.y0 + 0.5f);
        float pixel_aligned_x1 = std::floor(q.x1 + 0.5f);
        float pixel_aligned_y1 = std::floor(q.y1 + 0.5f);

        q.x0 = pixel_aligned_x0;
        q.y0 = pixel_aligned_y0;
        // q.y0 = -12;
        // q.y0 = 0;
        q.x1 = pixel_aligned_x1;
        q.y1 = pixel_aligned_y1;
        // q.y1 = 12;

        // Positions
        // textMeshData->positionMasterList.push_back(glm::vec3(q.x0, q.y0, 0));
        // textMeshData->positionMasterList.push_back(glm::vec3(q.x1, q.y0, 0));
        // textMeshData->positionMasterList.push_back(glm::vec3(q.x1, q.y1, 0));
        // textMeshData->positionMasterList.push_back(glm::vec3(q.x0, q.y1, 0));

        using namespace DirectX::SimpleMath;

        float flipped_y0 = baseline - q.y1;
        float flipped_y1 = baseline - q.y0;
        textSnippet.value().geometry.positions.push_back({q.x0, flipped_y0, 0});
        textSnippet.value().geometry.positions.push_back({q.x1, flipped_y0, 0});
        textSnippet.value().geometry.positions.push_back({q.x1, flipped_y1, 0});
        textSnippet.value().geometry.positions.push_back({q.x0, flipped_y1, 0});

        // UVS
        // textMeshData->uvMasterList.push_back({q.s0, q.t0});
        // textMeshData->uvMasterList.push_back({q.s1, q.t0});
        // textMeshData->uvMasterList.push_back({q.s1, q.t1});
        // textMeshData->uvMasterList.push_back({q.s0, q.t1});

        // Flip vertical uv coordinates
        textSnippet.value().geometry.uvs.push_back(Vector2{q.s0, q.t1});
        textSnippet.value().geometry.uvs.push_back(Vector2{q.s1, q.t1});
        textSnippet.value().geometry.uvs.push_back(Vector2{q.s1, q.t0});
        textSnippet.value().geometry.uvs.push_back(Vector2{q.s0, q.t0});

        //---------------------------------------
        // INDICES
        int offset = charCounter * 4;
        textSnippet.value().geometry.indices.push_back(2 + offset);
        textSnippet.value().geometry.indices.push_back(1 + offset);
        textSnippet.value().geometry.indices.push_back(0 + offset);
        textSnippet.value().geometry.indices.push_back(2 + offset);
        textSnippet.value().geometry.indices.push_back(0 + offset);
        textSnippet.value().geometry.indices.push_back(3 + offset);

        // Flipped
        // textMeshData->indices.push_back(0 + offset);
        // textMeshData->indices.push_back(1 + offset);
        // textMeshData->indices.push_back(2 + offset);
        // textMeshData->indices.push_back(3 + offset);
        // textMeshData->indices.push_back(0 + offset);
        // textMeshData->indices.push_back(2 + offset);
        // ----------------------------------------------------------------------------------

        charCounter++;

        // Track min/max for bounding box
        minX = std::min(minX, q.x0);
        maxX = std::max(maxX, q.x1);

        if (c == 32)
            continue;                // ignore space for Y, as this is always zero and messes things up.
        minY = std::min(minY, q.y0); // lowest part (descenders)
        minY = std::min(minY, q.y1);

        maxY = std::max(maxY, q.y0); // highest part (ascenders)
        maxY = std::max(maxY, q.y1);
    }

    
    for (int i = 0; i < textSnippet.value().geometry.positions.size(); i++)
    {
        textSnippet.value().geometry.vertices.push_back(textSnippet.value().geometry.positions[i].x);
        textSnippet.value().geometry.vertices.push_back(textSnippet.value().geometry.positions[i].y);
        textSnippet.value().geometry.vertices.push_back(textSnippet.value().geometry.positions[i].z);
        textSnippet.value().geometry.vertices.push_back(textSnippet.value().geometry.uvs[i].x);
        textSnippet.value().geometry.vertices.push_back(textSnippet.value().geometry.uvs[i].y);
    }

    if (!oldSnippet) {
        textSnippet.value().mesh.vb = createBuffer(
            textSnippet.value().geometry.vertices.data(),
            textSnippet.value().geometry.vertices.size() * sizeof(float), 
            D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, 0, 0);

         textSnippet.value().mesh.ib = createBuffer(
                textSnippet.value().geometry.indices.data(),
                textSnippet.value().geometry.indices.size() * sizeof(uint32_t),
                D3D11_USAGE_DYNAMIC, D3D11_BIND_INDEX_BUFFER, 0, 0);
    }
    else {

        BufferUpdateDesc vbDesc;
        vbDesc.buffer = textSnippet.value().mesh.vb;
        vbDesc.size = textSnippet.value().geometry.vertices.size() * sizeof(float);
        vbDesc.data = textSnippet.value().geometry.vertices.data();
        updateBuffer(vbDesc);

        BufferUpdateDesc ibDesc;
        ibDesc.buffer = textSnippet.value().mesh.ib;
        ibDesc.size = textSnippet.value().geometry.indices.size() * sizeof(uint32_t);
        ibDesc.data = textSnippet.value().geometry.indices.data();
        updateBuffer(ibDesc);
    }
    textSnippet.value().mesh.indexCount = textSnippet.value().geometry.indices.size();

    snippetMap[snippetId] = textSnippet.value();
    
}

Image DX11Renderer::loadImagePixels(const std::string& filePath) {
    int imageChannels, width, height;
    stbi_set_flip_vertically_on_load(true);
    auto pixels = stbi_load(filePath.c_str(), 
            &width, &height, 
            &imageChannels, 4);

    return {pixels, width, height, 4};
}



Texture DX11Renderer::createTexture(uint8_t* pixels, uint32_t width, uint32_t height, uint32_t numChannels, DXGI_FORMAT format ) {

    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = width;
    desc.Height = height;
    desc.Format = format;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    ComPtr<ID3D11Texture2D> dxTex;
    D3D11_SUBRESOURCE_DATA initialData = {};
    if (pixels) {
        initialData.pSysMem = pixels;
        initialData.SysMemPitch = width * (numChannels == 3? 4 : numChannels);
        initialData.SysMemSlicePitch = 0;
        auto result = device_->CreateTexture2D(&desc, &initialData, dxTex.GetAddressOf());
        assert(SUCCEEDED(result));

    } else {
        ThrowIfFailed(device_->CreateTexture2D(&desc, nullptr, dxTex.GetAddressOf()));
        
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    
    ComPtr<ID3D11ShaderResourceView> srv;
    ThrowIfFailed(device_->CreateShaderResourceView(dxTex.Get(), &srvDesc, srv.GetAddressOf()));
    
    return {dxTex, srv};
    
}

ComPtr<ID3D11ShaderResourceView> DX11Renderer::createShaderResourceViewForBuffer(ComPtr<ID3D11Buffer> buffer, 
                                                    uint32_t numInstances) {
    D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
    sd.Format = DXGI_FORMAT_UNKNOWN;                  // required for structured
    sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    sd.BufferEx.FirstElement = 0;
    sd.BufferEx.NumElements = numInstances;

    ComPtr<ID3D11ShaderResourceView> srv;
    auto result = device_->CreateShaderResourceView(buffer.Get(), &sd, &srv);
    assert(SUCCEEDED(result));

    return srv;
}

void DX11Renderer::ThrowIfFailed(HRESULT result) 
{
    if (FAILED(result))
        throw std::exception("failed d3d11 action");
}

/// @brief This function expects a single file containing at least vertex- and pixel shader.
/// @param filePath 
/// @return A shaderprogram with the vertex and pixel-shader.
ShaderProgram DX11Renderer::createShaderProgram(const std::wstring &filePath) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    #ifndef NDEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    #endif

    ComPtr<ID3DBlob> vsBlob;
    ThrowIfFailed(D3DCompileFromFile(filePath.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", 
                                            flags, 0, vsBlob.GetAddressOf(), nullptr));

    ComPtr<ID3DBlob> psBlob;
    ThrowIfFailed(D3DCompileFromFile(filePath.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", 
                                            flags, 0, psBlob.GetAddressOf(), nullptr));
    
    ComPtr<ID3D11VertexShader> vertexShader;
    ThrowIfFailed(device_->CreateVertexShader(vsBlob->GetBufferPointer(),
    vsBlob->GetBufferSize(), nullptr, vertexShader.GetAddressOf()));

    ComPtr<ID3D11PixelShader> pixelShader;
    ThrowIfFailed(device_->CreatePixelShader(psBlob->GetBufferPointer(),
    psBlob->GetBufferSize(), nullptr, pixelShader.GetAddressOf()));

    return ShaderProgram { { vsBlob, vertexShader}, {psBlob, pixelShader }};
    
}

ComPtr<ID3D11DeviceChild> DX11Renderer::createShader(const std::wstring &filePath, ShaderType shaderType)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
    #ifndef NDEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    #endif

    auto entryPoint = "VSMain";
    auto version = "vs_5_0";
    if (shaderType == ShaderType::Pixel)
    {   
        entryPoint = "PSMain";
        version = "ps_5_0";
    } 
    
    ComPtr<ID3DBlob> shaderBlob;
    ThrowIfFailed(D3DCompileFromFile(filePath.c_str(), nullptr, nullptr, entryPoint, version, 
                                            flags, 0, shaderBlob.GetAddressOf(), nullptr));
    

    if (shaderType == ShaderType::Vertex) {
        ComPtr<ID3D11VertexShader> vertexShader;
        ThrowIfFailed(device_->CreateVertexShader(shaderBlob->GetBufferPointer(),
        shaderBlob->GetBufferSize(), nullptr, vertexShader.GetAddressOf()));
        return vertexShader;
    }

    else if (shaderType == ShaderType::Pixel) {
        ComPtr<ID3D11PixelShader> shader;
        ThrowIfFailed(device_->CreatePixelShader(shaderBlob->GetBufferPointer(),
        shaderBlob->GetBufferSize(), nullptr, shader.GetAddressOf()));
        return shader;
    }
    
    return {};

    
    
}

ComPtr<ID3D11Buffer> DX11Renderer::createBuffer(void *data, int size, D3D11_USAGE bufferUsage, 
        D3D11_BIND_FLAG bindFlags, uint32_t miscFlags, uint32_t structuredByteStride)
{
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = bufferUsage;
    bd.ByteWidth = size;
    bd.BindFlags = bindFlags;
    bd.MiscFlags = miscFlags;
    bd.StructureByteStride = structuredByteStride;
    bd.CPUAccessFlags = bufferUsage == D3D11_USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0;

    ComPtr<ID3D11Buffer> buffer;
    HRESULT result;

    if (data) {
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = data;
        ThrowIfFailed(device_->CreateBuffer(&bd, &initData, &buffer));
    } else {
        ThrowIfFailed(device_->CreateBuffer(&bd, nullptr, &buffer));
    }

    return buffer;
}

void DX11Renderer::doFrame(FrameSubmission frameSubmission)
{
    
    if ((resizedDimension.x > 0 && resizedDimension.y > 0) && (resizedDimension.x != screenWidth || resizedDimension.y != screenHeight)){
        screenWidth = resizedDimension.x;
        screenHeight = resizedDimension.y;
        resizeSwapChain(hwnd, resizedDimension.x, resizedDimension.y);
    }
    
    clearBackBuffer(0, 0, 0, 1);
    bindBackBuffer(0, 0, screenWidth, screenHeight);
    for (auto& vs: frameSubmission.viewSubmissions) {

        // Upload camera matrices
        CameraCB ccb = { vs.viewMatrix, vs.projectionMatrix };
        ConstantBufferDesc cameraCB = {};
        cameraCB.buffer = cameraBuffer;
        cameraCB.bufferData = &ccb;
        cameraCB.shaderType = ShaderType::Vertex;
        cameraCB.size = sizeof(ccb);
        cameraCB.slot = 0;
        uploadConstantBufferData(cameraCB);

        for (auto& ord : vs.objectRenderData)
        {
            
            // Upload world matrix
            ConstantBufferDesc ocb= {};
            ocb.buffer = objectTransformBuffer;
            ocb.size = sizeof(ObjectTransformCB);
            ocb.bufferData = &ord.worldMatrices[0];
            ocb.slot = 1;
            ocb.shaderType = ShaderType::Vertex;
            uploadConstantBufferData(ocb);

            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            auto& mesh = meshMap[ord.meshId];
            

            auto inputLayout = inputLayoutMap[ord.inputLayoutId];
            auto dxInputLayout = dxInputLayoutMap[ord.inputLayoutId];
            auto stride = inputLayout.stride();

            // Instancing
            StructuredBufferDesc sbd = {};
            sbd.buffer = instanceBuffer;
            sbd.srv = instanceSRV;
            sbd.slot = 0;
            std::vector<InstanceData> instanceItems;
            for (auto& w : ord.worldMatrices) {
                instanceItems.push_back({w});
            }
            sbd.data = instanceItems;
            uploadStructuredBufferData(sbd);
            
            auto texture = textureMap[ord.textureId];
            bindTexture(0, texture);
            ctx->IASetInputLayout(dxInputLayout.Get());
            ctx->VSSetShader((ID3D11VertexShader*) shaderMap[ord.inputLayoutId].vs.vertexShader.Get(), nullptr, 0);
            ctx->PSSetShader((ID3D11PixelShader*) shaderMap[ord.inputLayoutId].ps.pixelShader.Get(), nullptr, 0);
            std::vector<ID3D11Buffer*> vertexBuffers = {mesh.vb.Get()};
            uint32_t offsets[] = {0};
            ctx->IASetVertexBuffers(0, 1, vertexBuffers.data(), &stride, offsets);
            ctx->IASetIndexBuffer(mesh.ib.Get(), DXGI_FORMAT_R32_UINT, 0);
            ctx->DrawIndexedInstanced(mesh.indexCount, instanceItems.size(), 0, 0, 0);
        }

        // Text rendering:
        for (auto& snippetDesc : vs.textRenderData) 
        {

            auto snippet = snippetMap[snippetDesc.snippetId];
            renderTextIntoQuad(snippetDesc.snippetId, snippet.fontId, snippetDesc.updatedText);
            snippet = snippetMap[snippetDesc.snippetId];

            // Upload world matrix
            ConstantBufferDesc ocb= {};
            ocb.buffer = objectTransformBuffer;
            ocb.size = sizeof(ObjectTransformCB);
            ocb.bufferData = &snippetDesc.worldMatrices[0];
            ocb.slot = 1;
            ocb.shaderType = ShaderType::Vertex;
            uploadConstantBufferData(ocb);

            

            ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            
            auto& mesh = snippet.mesh;
            
            auto inputLayout = inputLayoutMap["text"];
            auto dxInputLayout = dxInputLayoutMap["text"];
            auto stride = inputLayout.stride();

            auto font = fontMap[snippet.fontId];
            bindTexture(0, font.atlasTexture);
            ctx->IASetInputLayout(dxInputLayout.Get());
            ctx->VSSetShader((ID3D11VertexShader*) shaderMap["text"].vs.vertexShader.Get(), nullptr, 0);
            ctx->PSSetShader((ID3D11PixelShader*) shaderMap["text"].ps.pixelShader.Get(), nullptr, 0);
            std::vector<ID3D11Buffer*> vertexBuffers = {mesh.vb.Get()};
            uint32_t offsets[] = {0};
            ctx->IASetVertexBuffers(0, 1, vertexBuffers.data(), &stride, offsets);
            ctx->IASetIndexBuffer(mesh.ib.Get(), DXGI_FORMAT_R32_UINT, 0);
            ctx->DrawIndexed(mesh.indexCount, 0, 0);
        }
    }

    flipBackbuffer();
}

void DX11Renderer::updateBuffer(BufferUpdateDesc desc) {
    D3D11_MAPPED_SUBRESOURCE mapped;
    ThrowIfFailed(ctx->Map(desc.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
    memcpy(mapped.pData, desc.data, desc.size);
    ctx->Unmap(desc.buffer.Get(), 0);
}


void DX11Renderer::uploadConstantBufferData(ConstantBufferDesc constantBuffer)
{
    D3D11_MAPPED_SUBRESOURCE mapped;
    ctx->Map(constantBuffer.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, constantBuffer.bufferData, constantBuffer.size);
    ctx->Unmap(constantBuffer.buffer.Get(), 0);
    ID3D11Buffer* buffers[] = { constantBuffer.buffer.Get() };
    switch (constantBuffer.shaderType) {
        case ShaderType::Vertex: ctx->VSSetConstantBuffers(constantBuffer.slot, 1, buffers);break;
        case ShaderType::Pixel: ctx->PSSetConstantBuffers(constantBuffer.slot, 1, buffers); break;
    }
    
}

void DX11Renderer::uploadStructuredBufferData(StructuredBufferDesc desc) 
{
    if (desc.data.size() == 0) return;
    D3D11_MAPPED_SUBRESOURCE mapped;
    auto result = ctx->Map(desc.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    assert(SUCCEEDED(result));
    memcpy(mapped.pData, desc.data.data(), desc.data.size() * sizeof(InstanceData));
    ctx->Unmap(desc.buffer.Get(), 0);
    assert(SUCCEEDED(result));
    
    ID3D11ShaderResourceView* srvs[] = { desc.srv.Get()};
    ctx->VSSetShaderResources(desc.slot, 1, srvs);
        
}

Font DX11Renderer::createFont(const std::string& fontPath, int fontSize)
{
    // Read font file
    FILE *fp = fopen(fontPath.c_str(), "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open TTF file.\n");
        // TODO
        //throw std::runtime_error("Failed to open TTF file.");
    }
    fseek(fp, 0, SEEK_END);
    int fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *ttf_buffer = new unsigned char[fileSize];
    fread(ttf_buffer, 1, fileSize, fp);
    fclose(fp);

    // Retrieve font measurements
    stbtt_fontinfo info;
    stbtt_InitFont(&info, ttf_buffer, 0);

    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    float scale = stbtt_ScaleForPixelHeight(&info, fontSize);
    auto scaled_ascent_  = ascent  * scale;  // typically a positive number
    auto scaled_descent_ = descent * scale;  // typically negative
    auto scaled_line_gap_ = lineGap * scale;

    Font font;
    uint8_t* pixels = new uint8_t[512 * 512];
    int width = 512, height = 512;
    font.baseLine = scaled_ascent_;
    font.lineHeight = (scaled_ascent_ - scaled_descent_) + scaled_line_gap_;
    font.bakedChars.resize(96);
    int result = stbtt_BakeFontBitmap(ttf_buffer, 0, fontSize,
                                    pixels, width, height,
                                    32, 96, font.bakedChars.data());

    if (result <= 0) {
        fprintf(stderr, "Failed to bake font bitmap.\n");
        delete[] ttf_buffer;
        // TODO error case?!
    }

    font.atlasTexture = createTexture(pixels, width, height, 1, DXGI_FORMAT_R8_UNORM);

    return font;
}

void DX11Renderer::flipBackbuffer() 
{
    
    auto hr = swapChain->Present(0, 0);
    if (FAILED(hr)) {
        _com_error err(hr);
        std::wcerr << L"Present failed: " << _com_error(hr).ErrorMessage() << std::endl;
        auto reason = device_->GetDeviceRemovedReason();
        std::cerr << "Device removed reason: " << reason << std::endl;
        exit(1);
    }
}

void DX11Renderer::clearBackBuffer(float r, float g, float b, float a) 
{
    ID3D11RenderTargetView* rtvs[] = {renderTargetView.Get()};
    ctx->OMSetRenderTargets(1, rtvs, depthStencilView.Get());
    float color[] = {r, g, b, a};
    ctx->ClearRenderTargetView(renderTargetView.Get(), color);
    ctx->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}


void DX11Renderer::createDefaultSamplerState() {
    D3D11_SAMPLER_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.MinLOD = 0;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    auto result = device_->CreateSamplerState(&sd, defaultSamplerState.GetAddressOf());
    if (FAILED(result)) {
        exit(1);
    }
}

void DX11Renderer::createDefaultRasterizerState() {
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_BACK;
    rsDesc.FrontCounterClockwise = FALSE; 
    rsDesc.DepthClipEnable = TRUE;
    device_->CreateRasterizerState(&rsDesc, rasterStateSolid.GetAddressOf());
    ctx->RSSetState(rasterStateSolid.Get());
}

void DX11Renderer::bindTexture(uint32_t slot, Texture& texture) {
    ctx->PSSetShaderResources(slot, 1, texture.srv.GetAddressOf());

        // if (sampler) {
        //     ctx->PSSetSamplers(sampler->slot(), 1, sampler->samplerState().GetAddressOf());
        // } else {
        
    ctx->PSSetSamplers(0, 1, &defaultSamplerState);
        
       // }
}
 

void DX11Renderer::bindInputLayout(ComPtr<ID3D11InputLayout> inputLayout) {
    ctx->IASetInputLayout(inputLayout.Get());
}

void DX11Renderer::bindShader(const ShaderProgram* shaderProgram) {
    ctx->VSSetShader((ID3D11VertexShader*) shaderProgram->vs.vertexShader.Get(), nullptr, 0);
    ctx->PSSetShader((ID3D11PixelShader*) shaderProgram->ps.pixelShader.Get(), nullptr, 0);
}

void DX11Renderer::bindBackBuffer(int x, int y, int width, int height) {
    ID3D11RenderTargetView* rtvs[] = { renderTargetView.Get()};
    ctx->OMSetRenderTargets(1, rtvs, depthStencilView.Get());
    setViewport(x, y, width, height);
}

std::vector<D3D11_INPUT_ELEMENT_DESC> InputLayout::asDX11InputLayout()
{
    std::vector<D3D11_INPUT_ELEMENT_DESC> descs;
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

        descs.push_back(D3D11_INPUT_ELEMENT_DESC {sem, 0, format, 0, oldOffset, D3D11_INPUT_PER_VERTEX_DATA, 0});
    }
    return descs;
}

uint32_t InputLayout::stride()
{
    uint32_t str = 0;
    for (auto& e : elements) {
         switch (e.type) {
            case InputElementType::POSITION: str += 12; break;
            case InputElementType::UV: str += 8;  break;
            case InputElementType::NORMAL: str += 12; break;
         }
    }

    return str;

    

}
ComPtr<ID3D11InputLayout> DX11Renderer::createInputLayout(InputLayout attributeDescriptions, 
                                            ShaderProgram* shaderProgram)
{


    
    auto layoutDescs = attributeDescriptions.asDX11InputLayout();

    ComPtr<ID3D11InputLayout> inputLayout;
    auto result = device_->CreateInputLayout(layoutDescs.data(),
        layoutDescs.size(),
        shaderProgram->vs.blob->GetBufferPointer(),
        shaderProgram->vs.blob->GetBufferSize(),
        inputLayout.GetAddressOf());
    if(FAILED(result)) {
        _com_error err(result);
        std::wcerr << L"creation of input layout failed: " << _com_error(result).ErrorMessage() << std::endl;
    }
    
    return inputLayout;

}

void DX11Renderer::createDefaultBlendState() {
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable           = TRUE;
    blendDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    auto result = device_->CreateBlendState(&blendDesc, blendState.GetAddressOf());
    if (FAILED(result)) {
        exit(1);
    }
    float blendFactor[4] = { 0, 0, 0, 0 };
    ctx->OMSetBlendState(blendState.Get(), blendFactor, 0xffffffff);
}

void DX11Renderer::resizeSwapChain(HWND hwnd, int width, int height) {
    ctx->OMSetRenderTargets(0, nullptr, nullptr);
    ID3D11ShaderResourceView* nullsrvs[16] = {nullptr};
    ctx->VSSetShaderResources(0, 16, nullsrvs);
    ctx->PSSetShaderResources(0, 16, nullsrvs);
    ctx->CSSetShaderResources(0, 16, nullsrvs);
    ID3D11UnorderedAccessView* nullUAVs[8] = { nullptr };
    ctx->CSSetUnorderedAccessViews(0, 8, nullUAVs, nullptr);

    if (renderTargetView) {
        renderTargetView.Reset();
        
    }
    if (depthStencilView) {
        depthStencilView.Reset();

    }

    if (depthStencilBuffer) {
        depthStencilBuffer.Reset();
        
    }

    auto result = swapChain->Present(0, 0);
    if (FAILED(result)) {
        exit(1);
    }

    DXGI_SWAP_CHAIN_DESC desc;

    swapChain->GetDesc(&desc);
    std::cout << "Swap chain state: BufferCount=" << desc.BufferCount
              << ", Format=" << desc.BufferDesc.Format
              << ", Windowed=" << (desc.Windowed ? "Yes" : "No")
              << ", Flags=" << desc.Flags << std::endl;

    DXGI_MODE_DESC modeDesc = {};
    modeDesc.Width = width;
    modeDesc.Height = height;
    modeDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    modeDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    modeDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    result = swapChain->ResizeTarget(&modeDesc);
    if (FAILED(result)) {
        std::cout << "backbuffer target resizing failed" << std::to_string(result) << std::endl;
        exit(1);
    }

    result = swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(result)) {
        std::cout << "backbuffer resizing on swapchain resizing failed" << std::to_string(result) << std::endl;
        exit(1);
    }

    result = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(result)) {
        std::cout << "backbuffer creation/retrieval on swapchain resizing failed" << std::endl;
        exit(1);
    }

    // Step 4: Create new render target view
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    result = device_->CreateRenderTargetView(backBuffer.Get(), nullptr, renderTargetView.GetAddressOf());
    backBuffer.Reset();
    if (FAILED(result)) {
        std::cout << "Render target view creation failed" << std::endl;
        exit(1);
    }

    createDefaultDepthStencilBuffer(width, height);
    setViewport(0, 0, width, height);

}


void DX11Renderer::createDefaultDepthStencilBuffer(int width, int height) {

    // Create a depth/stencil buffer
    D3D11_TEXTURE2D_DESC td;
    td.Width = width;
    td.Height = height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_D32_FLOAT;
    td.SampleDesc.Count = 1;
    td.SampleDesc.Quality = 0;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    td.CPUAccessFlags = 0;
    td.MiscFlags = 0;

    auto result = device_->CreateTexture2D(&td, 0, depthStencilBuffer.GetAddressOf());
    if (FAILED(result)) {
        std::cout << "DepthStencil buffer creation failed! width: " << std::to_string(width) 
                  << " height: " << std::to_string(height) << std::endl;
        exit(1);
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC dpd;
    ZeroMemory(&dpd, sizeof(dpd));
    dpd.Flags = 0;
    dpd.Format = DXGI_FORMAT_D32_FLOAT;
    dpd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

    result = device_->CreateDepthStencilView(depthStencilBuffer.Get(), &dpd, depthStencilView.GetAddressOf());
    if (FAILED(result)) {
        OutputDebugString(L"D S view creation failed\n");
        exit(1);
    }

    D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
    depthStencilDesc.StencilEnable = FALSE;
    depthStencilDesc.StencilReadMask = 0xFF;
    depthStencilDesc.StencilWriteMask = 0xFF;

    // Stencil operations if pixel is front-facing
    depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
    depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    // Stencil operations if pixel is back-facing
    depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
    depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    result = device_->CreateDepthStencilState(&depthStencilDesc, m_DepthStencilState.GetAddressOf());
    if (FAILED(result)) {
        OutputDebugString(L"failed to create depth stencil state\n");
        exit(1);
    }

    ctx->OMSetDepthStencilState(m_DepthStencilState.Get(), 0);
}

void DX11Renderer::setViewport(int originX, int originY, int width, int height) {
    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = originX;
    vp.TopLeftY = originY;
    vp.Width = (FLOAT) width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);
}

void DX11Renderer::printDXGIError(HRESULT hr) {
    LPWSTR errorText = nullptr;
    DWORD result = FormatMessageW(
        FORMAT_MESSAGE_FROM_SYSTEM |FORMAT_MESSAGE_ALLOCATE_BUFFER, nullptr, hr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&errorText), 0, nullptr );
    if (result > 0)
    {
        // errorText contains the description of the error code hr
        std::wcout << "DXGI Error: " << errorText << std::endl;
        LocalFree( errorText );
    }
    else
    {
        // Error not known by the OS
    }
}