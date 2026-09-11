#define PI 3.14159265359f

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

float Fd_Lambert()
{
    return 1.0f / PI;
}

float4 PS(PixelInputType input) : SV_TARGET
{
    InstanceData instance = Instances[input.InstanceId];

    float3 n = normalize(input.Normal);
    float3 l = normalize(-LightDirection.xyz);

    float NoL = saturate(dot(n, l));

    float3 diffuseColor = instance.BaseColor.rgb;
    float3 Fd = diffuseColor * Fd_Lambert();

    float3 radiance = LightColorIntensity.rgb * LightColorIntensity.a;
    float3 color = Fd * radiance * NoL;

    return float4(color, instance.BaseColor.a);
}