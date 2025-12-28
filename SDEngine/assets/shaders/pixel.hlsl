Texture2D t_Texture : register(t1);
SamplerState s_Sampler : register(s2);

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(VS_OUTPUT input) : SV_TARGET
{
    return t_Texture.Sample(s_Sampler, input.uv);
}
