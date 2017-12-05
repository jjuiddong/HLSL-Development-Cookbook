#include "ForwardLightCommon.hlsl"

/////////////////////////////////////////////////////////////////////////////
// Constant Buffers
/////////////////////////////////////////////////////////////////////////////

cbuffer CapsuleLightConstants : register( b1 )
{
	float3 CapsuleLightPos		: packoffset( c0 );
	float CapsuleLightRangeRcp	: packoffset( c0.w );
	float3 CapsuleLightDir		: packoffset( c1 );
	float CapsuleLightLen		: packoffset( c1.w );
	float3 CapsuleLightColor	: packoffset( c2 );
}

/////////////////////////////////////////////////////////////////////////////
// Pixel shaders
/////////////////////////////////////////////////////////////////////////////

// Capsule light calculation helper function
float3 CalcCapsule(float3 position, Material material)
{
	float3 ToEye = EyePosition.xyz - position;
   
	// Find the shortest distance between the pixel and capsules segment
	float3 ToCapsuleStart = position - CapsuleLightPos;
	float DistOnLine = dot(ToCapsuleStart, CapsuleLightDir) / CapsuleLightLen;
	DistOnLine = saturate(DistOnLine) * CapsuleLightLen;
	float3 PointOnLine = CapsuleLightPos + CapsuleLightDir * DistOnLine;
	float3 ToLight = PointOnLine - position;
	float DistToLight = length(ToLight);
   
	// Phong diffuse
	ToLight /= DistToLight; // Normalize
	float NDotL = saturate(dot(ToLight, material.normal));
	float3 finalColor = material.diffuseColor * NDotL;
   
	// Blinn specular
	ToEye = normalize(ToEye);
	float3 HalfWay = normalize(ToEye + ToLight);
	float NDotH = saturate(dot(HalfWay, material.normal));
	finalColor += pow(NDotH, material.specExp) * material.specIntensity;
   
	// Attenuation
	float DistToLightNorm = 1.0 - saturate(DistToLight * CapsuleLightRangeRcp);
	float Attn = DistToLightNorm * DistToLightNorm;
	finalColor *= CapsuleLightColor.rgb * Attn;
   
	return finalColor;
}

float4 CapsuleLightPS( VS_OUTPUT In ) : SV_TARGET0
{
	// Prepare the material structure
	Material material = PrepareMaterial(In.Normal, In.UV);

	// Calculate the spot light color
	float3 finalColor = CalcCapsule(In.WorldPos, material);

	// Return the final color
	return float4(finalColor, 1.0);
}