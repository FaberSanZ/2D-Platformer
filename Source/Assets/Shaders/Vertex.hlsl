struct Vertex
{
    float3 Position;
    float3 Normal;
    float2 UV;
};

StructuredBuffer<Vertex> Vertices : register(t0);
StructuredBuffer<float4x4> Models : register(t1);

cbuffer TransformBuffer : register(b0)
{
    float4x4 ViewProjection;
};

struct VS_OUTPUT
{
    float4 Pos : SV_Position;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};

VS_OUTPUT VS(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    Vertex vertex = Vertices[vertexId];

    VS_OUTPUT output;
    float4 position = float4(vertex.Position, 1.0f);
    output.Pos = mul(mul(position, Models[instanceId]), ViewProjection);
    output.Normal = vertex.Normal;
    output.UV = vertex.UV;

    return output;
}