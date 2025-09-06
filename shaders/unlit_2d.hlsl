
#pragma pack_matrix(row_major)


struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv: TEXCOORD0;
    float3 normal : NORMAL;
};

struct InstanceData {
    row_major float4x4 World;   
    // uint boneBase; float4 tint; // add more per-instance fields later
};
StructuredBuffer<InstanceData> gInstances : register(t0);

cbuffer FrameCB : register(b0)
{
    row_major float4x4 View;      
    row_major float4x4 Proj;   
};

cbuffer ObjectCB : register(b1)
{
   row_major float4x4 World;
};


// PSInput VSMain(float4 position : POSITION, float2 uv : TEXCOORD0, float3 normal : NORMAL)
PSInput VSMain(float4 position : POSITION, float2 uv : TEXCOORD0, 
                            float3 normal : NORMAL, 
                            uint iid : SV_InstanceID)
{
    InstanceData inst = gInstances[iid];
    float4x4 W = inst.World;
    PSInput result;

    result.position = mul(position, W);
    result.position = mul(result.position, View);
    result.position = mul(result.position, Proj);
    
    result.normal = normal;
    result.uv = uv;

    return result;
}


// --------------------------------------------------------------------------------------------
// PixelShader
// --------------------------------------------------------------------------------------------

cbuffer MaterialCB : register(b0)
{
    float4 tint;

};

Texture2D diffuseTexture : register(t0);
SamplerState defaultSampler : register(s0);

struct PSOut { 
    float4 color : SV_TARGET0;
    
};

float4 directionalLight(PSInput pixelShaderInput) 
{
    float4 colorFromTexture = diffuseTexture.Sample(defaultSampler, pixelShaderInput.uv);
    // float4 colorFromTexture = float4(0.9, 0.9, 0.9, 1);
    // float teamColorValue = getTeamColorMapValue(pixelShaderInput.uv);
    // colorFromTexture.rgb = lerp(colorFromTexture.rgb, teamColor.rgb, teamColorValue);
    float3 normal = normalize(pixelShaderInput.normal);
    float3 lightDirection = normalize(float3(-5, 8, -5));
    float nDotL = saturate(dot(lightDirection, normal));
    float4 ambientColor = float4(0.02, 0.01, 0.0, 1);
    float3 ambient = ambientColor.rgb * colorFromTexture.rgb;
    float4 litColor = (float4)0;
    float3 diffuse = nDotL * colorFromTexture.rgb;
    litColor.rgb = ambient + diffuse;
    litColor.a = colorFromTexture.a;
    return litColor;
}
 

float4 PSMain(PSInput input) : SV_TARGET
{
    return diffuseTexture.Sample(defaultSampler, input.uv);

}