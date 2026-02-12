Texture2D t_Texture : register(t3);
SamplerState s_Sampler : register(s1);

struct VS_OUTPUT
{
  float4 pos : SV_POSITION;
  float2 uv  : TEXCOORD0;
};

static float hash12(float2 p)
{
  // Deterministic noise from UVs (no time needed).
  // Cheap hash; good enough for subtle grain.
  p = frac(p * float2(123.34, 345.45));
  p += dot(p, p + 34.345);
  return frac(p.x * p.y);
}

float4 main(VS_OUTPUT input) : SV_TARGET
{
  float2 uv = input.uv;

  // --- Vignette (radial darkening)
  float2 d = uv - 0.5;
  float r2 = dot(d, d);                   // 0 at center, ~0.5 at corners
  float vignette = smoothstep(0.55, 0.15, r2);

  // --- Subtle scanlines (depends only on uv.y => stable)
  float scan = 0.92 + 0.98 * sin(uv.y * 900.0);

  // --- Chromatic aberration / RGB split (radial direction)
  // Offset grows towards edges; direction points outward from center.
  float2 dir = (r2 > 1e-6) ? normalize(d) : float2(0.0, 0.0);
  float  amount = 0.0115 * (0.25 + 2.0 * r2); // tiny, edge-weighted
  float2 off = dir * amount;

  float4 cR = t_Texture.Sample(s_Sampler, uv + off);
  float4 cG = t_Texture.Sample(s_Sampler, uv);
  float4 cB = t_Texture.Sample(s_Sampler, uv - off);

  float3 color = float3(cR.r, cG.g, cB.b);

  // --- Film grain (very subtle)
  float n = hash12(uv * 1920.0);          // assumes postprocess-ish UV density
  color += (n - 0.5) * 0.02;

  // Apply scanlines + vignette
  color *= scan * vignette;

  return float4(saturate(color), 1.0);
}
