Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

struct PS_INPUT
{
    float4 Pos : SV_Position;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
};

float4 PS(PS_INPUT input) : SV_Target
{
    float4 textureColor = Texture.Sample(Sampler, input.UV);

    return textureColor * input.Color;
}