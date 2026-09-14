struct InstanceData
{
    float4x4 World;
    float4 BaseColor;
};

StructuredBuffer<InstanceData> Instances : register(t1);

cbuffer LightBuffer : register(b1)
{
    float4 LightDirection;
    float4 LightColorIntensity;
};

struct PixelInputType
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    nointerpolation uint InstanceId : TEXCOORD2;
};

float3 LinearToSRGB(float3 color)
{
    return pow(saturate(color), 1.0f / 2.2f);
}

float4 PS(PixelInputType input) : SV_TARGET
{
    InstanceData instance = Instances[input.InstanceId];

    float3 n = normalize(input.Normal);
    float3 l = normalize(-LightDirection.xyz);

    float NoL = saturate(dot(n, l));
    float3 baseColor = instance.BaseColor.rgb;

    float3 direct = baseColor * LightColorIntensity.rgb * LightColorIntensity.a * NoL;

    float hemisphere = n.y * 0.5f + 0.5f;

    float3 groundColor = float3(0.10f, 0.08f, 0.06f);
    float3 skyColor = float3(0.34f, 0.38f, 0.44f);

    float3 ambientLight = lerp(groundColor, skyColor, hemisphere);
    float3 ambient = baseColor * ambientLight;

    float3 color = direct + ambient;

    return float4(LinearToSRGB(color), instance.BaseColor.a);
}