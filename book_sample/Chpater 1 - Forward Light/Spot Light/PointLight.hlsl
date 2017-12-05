#include "ForwardLightCommon.hlsl"

/////////////////////////////////////////////////////////////////////////////
// Constant Buffers
/////////////////////////////////////////////////////////////////////////////

cbuffer PointLightConstants : register( b1 ) // Point light constants
{
	float3 PointLightPosition	: packoffset( c0 );
	float PointLightRangeRcp	: packoffset( c0.w );
	float3 PointLightColor		: packoffset( c1 );
}

/////////////////////////////////////////////////////////////////////////////
// Pixel shaders
/////////////////////////////////////////////////////////////////////////////

// Point light calculation helper function
float3 CalcPoint(float3 position, Material material)
{
	float3 ToLight = PointLightPosition.xyz - position;
	float3 ToEye = EyePosition.xyz - position;
	float DistToLight = length(ToLight);
   
	// Phong diffuse
	ToLight /= DistToLight; // Normalize
	float NDotL = saturate(dot(ToLight, material.normal));
	float3 finalColor = PointLightColor.rgb * NDotL;
   
	// Blinn specular
	ToEye = normalize(ToEye);
	float3 HalfWay = normalize(ToEye + ToLight);
	float NDotH = saturate(dot(HalfWay, material.normal));
	finalColor += PointLightColor.rgb * pow(NDotH, material.specExp) * material.specIntensity;
   
	// Attenuation
	float DistToLightNorm = 1.0 - saturate(DistToLight * PointLightRangeRcp);
	float Attn = DistToLightNorm * DistToLightNorm;
	finalColor *= material.diffuseColor * Attn;
   
	return finalColor;
}


float4 PointLightPS( VS_OUTPUT In ) : SV_TARGET0
{
	// Prepare the material structure
	Material material = PrepareMaterial(In.Normal, In.UV);

	// Calculate the point light color
	float3 finalColor = CalcPoint(In.WorldPos, material);

	// Return the final color
	return float4(finalColor, 1.0);
}