struct Vertex
{
    float4 Pos;
    float4 Color;
    float2 UV;
};

StructuredBuffer<Vertex> Vertices : register(t0);
StructuredBuffer<float4x4> Model : register(t1);

cbuffer TransformBuffer : register(b0)
{
    float4x4 ViewProjection;
};

struct VS_OUTPUT
{
    float4 Pos : SV_Position;
    float4 Color : COLOR;
    float2 UV : TEXCOORD0;
};

VS_OUTPUT VS(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    Vertex vertex = Vertices[vertexId];

    VS_OUTPUT output;
    output.Pos = mul(mul(vertex.Pos, Model[instanceId]), ViewProjection);
    output.Color = vertex.Color;
    output.UV = vertex.UV;

    return output;
}