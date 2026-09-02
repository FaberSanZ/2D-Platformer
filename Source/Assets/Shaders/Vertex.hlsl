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
    float4x4 ViewProjection;
};

StructuredBuffer<float4x4> Model : register(t1);


VS_OUTPUT VS(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{    
    Vertex vertex = Vertices[vertexId];
    
    VS_OUTPUT output;
    output.Pos = mul(mul(vertex.Pos, Model[instanceId]), ViewProjection);
    output.Color = vertex.color;
    
    return output;
}