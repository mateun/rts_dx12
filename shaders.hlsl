struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv: TEXCOORD0;
};

cbuffer FrameCB : register(b0)
{
    row_major float4x4 View;      
    row_major float4x4 Proj;   
};

cbuffer ObjectCB : register(b1)
{
   row_major float4x4 World;
};


PSInput VSMain(float4 position : POSITION, float2 uv : TEXCOORD0)
{
    PSInput result;

    result.position = mul(position, World);
    result.position = mul(result.position, View);
    result.position = mul(result.position, Proj);
    result.uv = uv;

    return result;
}

cbuffer MaterialCB : register(b0)
{
    float4 tint;

};

Texture2D diffuseTexture : register(t0);
SamplerState defaultSampler : register(s0);

float4 PSMain(PSInput input) : SV_TARGET
{
    return diffuseTexture.Sample(defaultSampler, input.uv) * tint;

    //return tint;
}