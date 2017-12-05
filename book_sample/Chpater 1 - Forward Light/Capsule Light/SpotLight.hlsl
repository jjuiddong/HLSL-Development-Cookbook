#include "ForwardLightCommon.hlsl"

/////////////////////////////////////////////////////////////////////////////
// Constant Buffers
/////////////////////////////////////////////////////////////////////////////

cbuffer SpotLightConstants : register( b1 )
{
	float3 SpotLightPos			: packoffset( c0 );
	float SpotLightRangeRcp		: packoffset( c0.w );
	float3 SpotDirToLight		: packoffset( c1 );
	float SpotCosOuterCone		: packoffset( c1.w );
	float3 SpotColor			: packoffset( c2 );
	float SpotCosInnerConeRcp	: packoffset( c2.w );
}

/////////////////////////////////////////////////////////////////////////////
// Pixel shaders
/////////////////////////////////////////////////////////////////////////////

// Spot light calculation helper function
float3 CalcSpot(float3 position, Material material)
{
	float3 ToLight = SpotLightPos - position;
	float3 ToEye = EyePosition.xyz - position;
	float DistToLight = length(ToLight);
   
	// Phong diffuse
	ToLight /= DistToLight; // Normalize
	float NDotL = saturate(dot(ToLight, material.normal));
	float3 finalColor = SpotColor.rgb * NDotL;
   
	// Blinn specular
	ToEye = normalize(ToEye);
	float3 HalfWay = normalize(ToEye + ToLight);
	float NDotH = saturate(dot(HalfWay, material.normal));
	finalColor += SpotColor.rgb * pow(NDotH, material.specExp) * material.specIntensity;
   
	// Cone attenuation
	float cosAng = dot(SpotDirToLight, ToLight);
	float conAtt = saturate((cosAng - SpotCosOuterCone) * SpotCosInnerConeRcp);
	conAtt *= conAtt;
   
	// Attenuation
	float DistToLightNorm = 1.0 - saturate(DistToLight * SpotLightRangeRcp);
	float Attn = DistToLightNorm * DistToLightNorm;
	finalColor *= material.diffuseColor * Attn * conAtt;
   
	return finalColor;
}

float4 SpotLightPS( VS_OUTPUT In ) : SV_TARGET0
{
	// Prepare the material structure
	Material material = PrepareMaterial(In.Normal, In.UV);

	// Calculate the spot light color
	float3 finalColor = CalcSpot(In.WorldPos, material);

	// Return the final color
	return float4(finalColor, 1.0);
}