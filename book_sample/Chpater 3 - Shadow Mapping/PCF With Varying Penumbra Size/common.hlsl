/////////////////////////////////////////////////////////////////////////////
// GBuffer textures and Samplers
/////////////////////////////////////////////////////////////////////////////
Texture2D<float> DepthTexture         : register( t0 );
Texture2D<float4> ColorSpecIntTexture : register( t1 );
Texture2D<float3> NormalTexture       : register( t2 );
Texture2D<float4> SpecPowTexture      : register( t3 );
SamplerState PointSampler             : register( s1 );

/////////////////////////////////////////////////////////////////////////////
// Shadow sampler
/////////////////////////////////////////////////////////////////////////////
SamplerComparisonState PCFSampler : register( s2 );

/////////////////////////////////////////////////////////////////////////////
// constants
/////////////////////////////////////////////////////////////////////////////
cbuffer cbGBufferUnpack : register( b0 )
{
	float4 PerspectiveValues : packoffset( c0 );
	float4x4 ViewInv         : packoffset( c1 );
}

#define EyePosition (ViewInv[3].xyz)

static const float2 g_SpecPowerRange = { 10.0, 250.0 };

float ConvertZToLinearDepth(float depth)
{
	float linearDepth = PerspectiveValues.z / (depth + PerspectiveValues.w);
	return linearDepth;
}

float3 CalcWorldPos(float2 csPos, float depth)
{
	float4 position;

	position.xy = csPos.xy * PerspectiveValues.xy * depth;
	position.z = depth;
	position.w = 1.0;
	
	return mul(position, ViewInv).xyz;
}

struct SURFACE_DATA
{
	float LinearDepth;
	float3 Color;
	float3 Normal;
	float SpecPow;
	float SpecIntensity;
};

SURFACE_DATA UnpackGBuffer(float2 UV)
{
	SURFACE_DATA Out;

	float depth = DepthTexture.Sample( PointSampler, UV.xy ).x;
	Out.LinearDepth = ConvertZToLinearDepth(depth);
	float4 baseColorSpecInt = ColorSpecIntTexture.Sample( PointSampler, UV.xy );
	Out.Color = baseColorSpecInt.xyz;
	Out.SpecIntensity = baseColorSpecInt.w;
	Out.Normal = NormalTexture.Sample( PointSampler, UV.xy ).xyz;
	Out.Normal = normalize(Out.Normal * 2.0 - 1.0);
	Out.SpecPow = SpecPowTexture.Sample( PointSampler, UV.xy ).x;

	return Out;
}

SURFACE_DATA UnpackGBuffer_Loc(int2 location)
{
	SURFACE_DATA Out;
	int3 location3 = int3(location, 0);

	float depth = DepthTexture.Load(location3).x;
	Out.LinearDepth = ConvertZToLinearDepth(depth);
	float4 baseColorSpecInt = ColorSpecIntTexture.Load(location3);
	Out.Color = baseColorSpecInt.xyz;
	Out.SpecIntensity = baseColorSpecInt.w;
	Out.Normal = NormalTexture.Load(location3).xyz;
	Out.Normal = normalize(Out.Normal * 2.0 - 1.0);
	Out.SpecPow = SpecPowTexture.Load(location3).x;

	return Out;
}

struct Material
{
   float3 normal;
   float4 diffuseColor;
   float specPow;
   float specIntensity;
};

void MaterialFromGBuffer(SURFACE_DATA gbd, inout Material mat)
{
	mat.normal = gbd.Normal;
	mat.diffuseColor.xyz = gbd.Color;
	mat.diffuseColor.w = 1.0; // Fully opaque
	mat.specPow = g_SpecPowerRange.x + g_SpecPowerRange.y * gbd.SpecPow;
	mat.specIntensity = gbd.SpecIntensity;
}

float4 DebugLightPS() : SV_TARGET
{
	return float4(1.0, 1.0, 1.0, 1.0);
}