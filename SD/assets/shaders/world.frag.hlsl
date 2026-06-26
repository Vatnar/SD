struct VSOutput {
  float4 position : SV_Position;
  float4 color : COLOR0;
  float3 normal : NORMAL;
  float2 uv : TEXCOORD0;
};

float4 main(VSOutput input) : SV_Target {
  float4 color = input.color;
  // color.g = sin(color.r);
  // color.r = cos(color.g);
  color.b = clamp(sin(color.r) + cos(color.g) - tan(color.b), 0.0, 1.0);
  
  return color;
}
