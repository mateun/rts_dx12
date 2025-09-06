cmake_minimum_required(VERSION 3.28)

project(dx12_rts)

set(CMAKE_CXX_STANDARD 23)

option(USE_DX12 "Enable DirectX12 build" OFF)
option(USE_DX11 "Enable DirectX11 build" ON)


if(USE_DX11)
find_package(directxtex CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED)

add_executable(dx11_rts src/engine/main.cpp src/game/rts_game.cpp 
                        src/engine/window.cpp src/engine/geometry.cpp 
                        src/engine/dx11renderer.cpp
                        src/engine/game_util.cpp
                        src/engine/game.cpp
                        src/engine/renderer.cpp
                        src/lib/tiny_gltf.cc
                        )
target_compile_definitions(dx11_rts PRIVATE UNICODE NOMINMAX)
if (CMAKE_EXPORT_COMPILE_COMMANDS)
    target_compile_definitions(dx11_rts PRIVATE RUNNING_FROM_IDE=1)
endif()
target_include_directories(dx11_rts PRIVATE src/lib/include)
target_link_libraries(dx11_rts PRIVATE
                    Microsoft::DirectXTex
                    Microsoft::DirectXTK
                    Microsoft::DirectX-Headers
                    d3dcompiler.lib
                    dxgi.lib
                    dxguid.lib
                    d3d11.lib)
endif()


if(USE_DX12)
find_package(directxtex CONFIG REQUIRED)
find_package(D3D12MemoryAllocator CONFIG REQUIRED)
find_package(directxtk12 CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED)

add_executable(dx12_rts src/engine/main.cpp src/game/rts_game.cpp 
                        src/engine/window.cpp src/engine/geometry.cpp 
                        src/engine/dx12renderer.cpp
                        src/lib/tiny_gltf.cc
                        )
target_compile_definitions(dx12_rts PRIVATE UNICODE)
if (CMAKE_EXPORT_COMPILE_COMMANDS)
    target_compile_definitions(dx12_rts PRIVATE RUNNING_FROM_IDE=1)
endif()
target_include_directories(dx12_rts PRIVATE src/lib/include)
target_link_libraries(dx12_rts PRIVATE
                    GPUOpen::D3D12MemoryAllocator
                    Microsoft::DirectXTex
                    Microsoft::DirectXTK12
                    Microsoft::DirectX-Headers
                    d3dcompiler.lib
                    dxgi.lib
                    dxguid.lib
                    d3d12.lib)
endif()

