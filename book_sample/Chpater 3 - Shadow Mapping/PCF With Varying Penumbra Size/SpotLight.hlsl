#include "common.hlsl"

Texture2D<float> SpotShadowMapTexture : register( t4 );

/////////////////////////////////////////////////////////////////////////////
// constants
/////////////////////////////////////////////////////////////////////////////
cbuffer cbSpotLightDomain : register( b0 )
{
	float4x4 LightProjection	: packoffset( c0 );
	float SinAngle				: packoffset( c4 );
	float CosAngle				: packoffset( c4.y );
}

cbuffer cbSpotLightPixel : register( b1 )
{
	float3 SpotLightPos			: packoffset( c0 );
	float SpotLightRangeRcp		: packoffset( c0.w );
	float3 SpotDirToLight		: packoffset( c1 );
	float SpotCosOuterCone		: packoffset( c1.w );
	float3 SpotColor			: packoffset( c2 );
	float SpotCosConeAttRange	: packoffset( c2.w );
	float4x4 ToShadowmap		: packoffset( c3 );
	float ShadowMapPixelSize	: packoffset( c7 );
	float LightSize				: packoffset( c7.y );
}

/////////////////////////////////////////////////////////////////////////////
// Vertex shader
/////////////////////////////////////////////////////////////////////////////
float4 SpotLightVS() : SV_Position
{
    return float4(0.0, 0.0, 0.0, 1.0); 
}

/////////////////////////////////////////////////////////////////////////////
// Hull shader
/////////////////////////////////////////////////////////////////////////////
struct HS_CONSTANT_DATA_OUTPUT
{
	float Edges[4] : SV_TessFactor;
	float Inside[2] : SV_InsideTessFactor;
};

HS_CONSTANT_DATA_OUTPUT SpotLightConstantHS()
{
	HS_CONSTANT_DATA_OUTPUT Output;
	
	float tessFactor = 18.0;
	Output.Edges[0] = Output.Edges[1] = Output.Edges[2] = Output.Edges[3] = tessFactor;
	Output.Inside[0] = Output.Inside[1] = tessFactor;

	return Output;
}

struct HS_OUTPUT
{
	float3 Position : POSITION;
};

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_ccw")]
[outputcontrolpoints(4)]
[patchconstantfunc("SpotLightConstantHS")]
HS_OUTPUT SpotLightHS()
{
	HS_OUTPUT Output;

	Output.Position = float3(0.0, 0.0, 0.0);

	return Output;
}

/////////////////////////////////////////////////////////////////////////////
// Domain Shader shader
/////////////////////////////////////////////////////////////////////////////
struct DS_OUTPUT
{
	float4 Position		: SV_POSITION;
	float3 PositionXYW	: TEXCOORD0;
};

#define CylinderPortion 0.2
#define ExpendAmount    (1.0 + CylinderPortion)

[domain("quad")]
DS_OUTPUT SpotLightDS( HS_CONSTANT_DATA_OUTPUT input, float2 UV : SV_DomainLocation, const OutputPatch<HS_OUTPUT, 4> quad)
{
	// Transform the UV's into clip-space
	float2 posClipSpace = UV.xy * float2(2.0, -2.0) + float2(-1.0, 1.0);

	// Find the vertex offsets based on the UV
	float2 posClipSpaceAbs = abs(posClipSpace.xy);
	float maxLen = max(posClipSpaceAbs.x, posClipSpaceAbs.y);

	// Force the cone vertices to the mesh edge
	float2 posClipSpaceNoCylAbs = saturate(posClipSpaceAbs * ExpendAmount);
	float maxLenNoCapsule = max(posClipSpaceNoCylAbs.x, posClipSpaceNoCylAbs.y);
	float2 posClipSpaceNoCyl = sign(posClipSpace.xy) * posClipSpaceNoCylAbs;

	// Convert the positions to half sphere with the cone vertices on the edge
	float3 halfSpherePos = normalize(float3(posClipSpaceNoCyl.xy, 1.0 - maxLenNoCapsule));

	// Scale the sphere to the size of the cones rounded base
	halfSpherePos = normalize(float3(halfSpherePos.xy * SinAngle, CosAngle));

	// Find the offsets for the cone vertices (0 for cone base)
	float cylinderOffsetZ = saturate((maxLen * ExpendAmount - 1.0) / CylinderPortion);

	// Offset the cone vertices to thier final position
	float4 posLS = float4(halfSpherePos.xy * (1.0 - cylinderOffsetZ), halfSpherePos.z - cylinderOffsetZ * CosAngle, 1.0);

	// Transform all the way to projected space and generate the UV coordinates
	DS_OUTPUT Output;
	Output.Position = mul( posLS, LightProjection );
	Output.PositionXYW = Output.Position.xyw;

	return Output;
}

/////////////////////////////////////////////////////////////////////////////
// Pixel shader
/////////////////////////////////////////////////////////////////////////////

// Poisson smapling
static const float2 poissonDisk[16] = {
	float2( -0.94201624, -0.39906216 ),
	float2( 0.94558609, -0.76890725 ),
	float2( -0.094184101, -0.92938870 ),
	float2( 0.34495938, 0.29387760 ),
	float2( -0.91588581, 0.45771432 ),
	float2( -0.81544232, -0.87912464 ),
	float2( -0.38277543, 0.27676845 ),
	float2( 0.97484398, 0.75648379 ),
	float2( 0.44323325, -0.97511554 ),
	float2( 0.53742981, -0.47373420 ),
	float2( -0.26496911, -0.41893023 ),
	float2( 0.79197514, 0.19090188 ),
	float2( -0.24188840, 0.99706507 ),
	float2( -0.81409955, 0.91437590 ),
	float2( 0.19984126, 0.78641367 ),
	float2( 0.14383161, -0.14100790 )
};

// Shadow PCSS calculation helper function
float SpotShadowPCSS( float3 position )
{
	// Transform the world position to shadow projected space
	float4 posShadowMap = mul(float4(position, 1.0), ToShadowmap);

	// Transform the position to shadow clip space
	float3 UVD = posShadowMap.xyz / posShadowMap.w;

	// Convert to shadow map UV values
	UVD.xy = 0.5 * UVD.xy + 0.5;
	UVD.y = 1.0 - UVD.y;

	// Search for blockers
	float avgBlockerDepth = 0;
	float blockerCount = 0;
	
	[unroll]
	for(int i = -2; i <= 2; i += 2)
	{
		[unroll]
		for(int j = -2; j <= 2; j += 2)
		{
			float4 d4 = SpotShadowMapTexture.GatherRed(PointSampler, UVD.xy, int2(i, j));
			float4 b4 = (UVD.z <= d4) ? 0.0: 1.0;   

			blockerCount += dot(b4, 1.0);
			avgBlockerDepth += dot(d4, b4);
		}
	}

	// Check if we can early out
	if(blockerCount <= 0.0)
	{
		return 1.0;
	}

	// Penumbra width calculation
	avgBlockerDepth /= blockerCount;
	float fRatio = ((UVD.z - avgBlockerDepth) * LightSize) / avgBlockerDepth;
	fRatio *= fRatio;

	// Apply the filter
	float att = 0;

	[unroll]
	for(i = 0; i < 16; i++)
	{
		float2 offset = fRatio * ShadowMapPixelSize.xx * poissonDisk[i];
		att += SpotShadowMapTexture.SampleCmpLevelZero(PCFSampler, UVD.xy + offset, UVD.z);
	}

	// Devide by 16 to normalize
	return att * 0.0625;
}

// Shadow PCF calculation helper function
float SpotShadowPCF( float3 position )
{
	// Transform the world position to shadow projected space
	float4 posShadowMap = mul(float4(position, 1.0), ToShadowmap);

	// Transform the position to shadow clip space
	float3 UVD = posShadowMap.xyz / posShadowMap.w;

	// Convert to shadow map UV values
	UVD.xy = 0.5 * UVD.xy + 0.5;
	UVD.y = 1.0 - UVD.y;

	// Compute the hardware PCF value
	return SpotShadowMapTexture.SampleCmpLevelZero(PCFSampler, UVD.xy, UVD.z);
}

float3 CalcSpot(float3 position, Material material, bool bUseShadow, bool bUsePCSS)
{
	float3 ToLight = SpotLightPos - position;
	float3 ToEye = EyePosition - position;
	float DistToLight = length(ToLight);

	// Phong diffuse
	ToLight /= DistToLight; // Normalize
	float NDotL = saturate(dot(ToLight, material.normal));
	float3 finalColor = material.diffuseColor.rgb * NDotL;

	// Blinn specular
	ToEye = normalize(ToEye);
	float3 HalfWay = normalize(ToEye + ToLight);
	float NDotH = saturate(dot(HalfWay, material.normal));
	finalColor += pow(NDotH, material.specPow) * material.specIntensity;

	// Cone attenuation
	float cosAng = dot(SpotDirToLight, ToLight);
	float conAtt = saturate((cosAng - SpotCosOuterCone) / SpotCosConeAttRange);
	conAtt *= conAtt;
   
   	float shadowAtt;
	if(bUseShadow)
	{
		// Find the shadow attenuation for the pixels world position
		shadowAtt = SpotShadowPCF(position);

		if(bUsePCSS)
		{
			shadowAtt = SpotShadowPCSS(position);
		}
		else
		{
			shadowAtt = SpotShadowPCF(position);
		}
	}
	else
	{
		// No shadow attenuation
		shadowAtt = 1.0;
	}

	// Attenuation
	float DistToLightNorm = 1.0 - saturate(DistToLight * SpotLightRangeRcp);
	float Attn = DistToLightNorm * DistToLightNorm;
	finalColor *= SpotColor.rgb * Attn * conAtt * shadowAtt;
   
	// Return the fianl color
	return finalColor;
}

float4 SpotLightCommonPS( DS_OUTPUT In, bool bUseShadow, bool bUsePCSS ) : SV_TARGET
{
	// Unpack the GBuffer
	SURFACE_DATA gbd = UnpackGBuffer_Loc(In.Position.xy);
	
	// Convert the data into the material structure
	Material mat;
	MaterialFromGBuffer(gbd, mat);

	// Reconstruct the world position
	float3 position = CalcWorldPos(In.PositionXYW.xy / In.PositionXYW.z, gbd.LinearDepth);

	// Calculate the light contribution
	float3 finalColor = CalcSpot(position, mat, bUseShadow, bUsePCSS);

	// Return the final color
	return float4(finalColor, 1.0);
}

float4 SpotLightPS( DS_OUTPUT In ) : SV_TARGET
{
	return SpotLightCommonPS(In, false, false);
}

float4 SpotLightShadowPS( DS_OUTPUT In ) : SV_TARGET
{
	return SpotLightCommonPS(In, true, false);
}

float4 SpotLightShadowPCSSPS( DS_OUTPUT In ) : SV_TARGET
{
	return SpotLightCommonPS(In, true, true);
}
