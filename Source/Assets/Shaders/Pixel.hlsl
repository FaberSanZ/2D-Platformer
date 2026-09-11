Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

struct PS_INPUT
{
    float4 Pos : SV_Position;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};


float4 PS(PS_INPUT input) : SV_Target
{
    //float4 textureColor = Texture.Sample(Sampler, input.UV);

    //return textureColor * input.Color;
    
    return float4(0.8f, 0.8f, 0.8f, 1.0f);

}