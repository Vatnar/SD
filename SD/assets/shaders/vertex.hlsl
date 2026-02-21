cbuffer PushConstants : register(b0, space0) {
  float4x4 mvp;
};

struct VSInput {
  float3 position : POSITION0;
};

struct VSOutput {
  float4 position : SV_POSITION;
};

VSOutput vs_main(VSInput input) {
  VSOutput output;
  output.position = mul(mvp, float4(input.position, 1.0));
  return output;
}