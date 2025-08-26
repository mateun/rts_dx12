cmake_minimum_required(VERSION 3.28)

project(dx12_rts)

find_package(directxtex CONFIG REQUIRED)
find_package(D3D12MemoryAllocator CONFIG REQUIRED)
find_package(directxtk12 CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED)

add_executable(dx12_rts src/engine/main.cpp src/engine/window.cpp)
target_compile_definitions(dx12_rts PRIVATE UNICODE)
if (CMAKE_EXPORT_COMPILE_COMMANDS)
    target_compile_definitions(dx12_rts PRIVATE RUNNING_FROM_IDE=1)
endif()
target_link_libraries(dx12_rts PRIVATE
                    GPUOpen::D3D12MemoryAllocator
                    Microsoft::DirectXTex
                    Microsoft::DirectXTK12
                    Microsoft::DirectX-Headers
                    d3dcompiler.lib
                    dxgi.lib
                    dxguid.lib
                    d3d12.lib)


