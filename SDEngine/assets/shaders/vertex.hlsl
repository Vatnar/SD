cbuffer ViewProjection : register(b0)
{
  float4x4 proj;
  float4x4 view;
  float4x4 model;
};

struct VS_INPUT
{
  float3 pos : POSITION;
  float2 uv : TEXCOORD0;
};

struct VS_OUTPUT
{
  float4 pos : SV_POSITION;
  float2 uv : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
  VS_OUTPUT output;
  float4 worldPos = float4(input.pos, 0.5);
  output.pos = mul(proj, mul(view, mul(model, worldPos)));
  output.uv = input.uv;
  return output;
}
