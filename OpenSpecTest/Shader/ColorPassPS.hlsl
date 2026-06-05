cbuffer SceneConstants : register(b0)
{
  row_major float4x4 g_view_projection;
  float3 g_light_direction;
  float g_ambient_strength;
};

struct PSInput
{
  float4 position : SV_POSITION;
  float3 normal : NORMAL;
  float4 color : COLOR;
};

float4 main(PSInput input) : SV_TARGET
{
  const float3 light_dir = normalize(-g_light_direction);
  const float3 normal = normalize(input.normal);
  const float ndotl = saturate(dot(normal, light_dir));
  const float lighting = saturate(g_ambient_strength + ndotl);
  return float4(input.color.rgb * lighting, input.color.a);
}
