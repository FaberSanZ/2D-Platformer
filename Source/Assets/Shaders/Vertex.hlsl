struct Vertex
{
    float4 Pos;
    float4 color;
};

StructuredBuffer<Vertex> Vertices : register(t0);

struct VS_OUTPUT
{
    float4 Pos : SV_Position;
    float4 Color : COLOR;
};

cbuffer TransformBuffer : register(b0)
{
    float4x4 WorldViewProjection;
};


VS_OUTPUT VS(uint vertexId : SV_VertexID)
{    
    Vertex vertex = Vertices[vertexId];
    
    VS_OUTPUT output;
    output.Pos = mul(vertex.Pos, WorldViewProjection);
    output.Color = vertex.color;
    
    return output;
}