cbuffer SceneConstants : register(b0)
{
  row_major float4x4 g_view_projection;
  float3 g_light_direction;
  float g_ambient_strength;
};

cbuffer DrawConstants : register(b1)
{
  row_major float4x4 g_world;
  float4 g_mesh_color;
};

struct VSInput
{
  float3 position : POSITION;
  float3 normal : NORMAL;
  float4 color : COLOR;
};

struct PSInput
{
  float4 position : SV_POSITION;
  float3 normal : NORMAL;
  float4 color : COLOR;
};

PSInput main(VSInput input)
{
  PSInput output;
  const float4 world_position = mul(float4(input.position, 1.0f), g_world);
  output.position = mul(world_position, g_view_projection);

  output.normal = normalize(mul(float4(input.normal, 0.0f), g_world).xyz);
  output.color = input.color * g_mesh_color;
  return output;
}
