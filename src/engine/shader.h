#pragma once
#include "comptr.h"
#include <d3d11.h>

struct PixelShader {
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3D11PixelShader> pixelShader;

};

struct VertexShader {
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3D11VertexShader> vertexShader;
};


struct ShaderProgram
{
    VertexShader vs;
    PixelShader ps;

};

enum class ShaderType {
    Vertex,
    Pixel,
    Geometry,
    Tesselation,
};