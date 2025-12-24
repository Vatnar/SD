// vertex.hlsl
struct VSOutput
{
    float4 Pos : SV_POSITION;
};

VSOutput main(uint VertexID : SV_VertexID)
{
    VSOutput output;

    // Grid generation logic
    float2 grid = float2((VertexID << 1) & 2, VertexID & 2);
    float2 xy = grid * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);

    output.Pos = float4(xy, 0.0f, 1.0f);
    return output;
}
