struct VertexPNUV{
    [[vk::location(0)]] float3 pos : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 uv : TEXCOORD0;
};

struct VSOutput {
    float4 position : SV_Position;
    float4 color : COLOR0;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct PushConstants {
    float4x4 mvp;
    float4 colorMul;
};


[[vk::push_constant]] PushConstants pc;

VSOutput main(VertexPNUV input) {
    VSOutput output;
    output.position = mul(pc.mvp, float4(input.pos, 1.0f));
    output.color = pc.colorMul;
    output.normal = normalize(mul((float3x3)pc.mvp, input.normal));
    output.uv = input.uv;
    return output;
}
