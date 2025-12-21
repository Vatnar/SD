float4 main(uint vid : SV_VertexID) : SV_POSITION
{
    float2 pos[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };
    return float4(pos[vid], 0.0, 1.0);
}
