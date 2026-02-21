struct VSOutput {
    float4 position : SV_Position;
    float4 color : COLOR;
};

struct PushConstants {
    row_major float4x4 mvp;
    float4 color;
};

[[vk::push_constant]] PushConstants pc;

VSOutput main(uint vId : SV_VertexID) {
    float3 positions[3] = {
        float3(0.0f, -0.8f, 0.0f),
        float3(-0.8f, 0.8f, 0.0f),
        float3(0.8f, 0.8f, 0.0f)
    };

    VSOutput output;
    output.position = mul(pc.mvp, float4(positions[vId], 1.0));
    output.color = pc.color;
    return output;
}
