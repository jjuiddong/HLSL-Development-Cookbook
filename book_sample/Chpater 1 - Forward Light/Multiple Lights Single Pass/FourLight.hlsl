#include "ForwardLightCommon.hlsl"

/////////////////////////////////////////////////////////////////////////////
// Constant Buffers
/////////////////////////////////////////////////////////////////////////////

cbuffer FourLightsConstants : register( b1 )
{
	float4 LightPosX			: packoffset( c0 );
	float4 LightPosY			: packoffset( c1 );
	float4 LightPosZ			: packoffset( c2 );
	float4 LightDirX			: packoffset( c3 );
	float4 LightDirY			: packoffset( c4 );
	float4 LightDirZ			: packoffset( c5 );
	float4 LightRangeRcp		: packoffset( c6 );
	float4 SpotCosOuterCone		: packoffset( c7 );
	float4 SpotCosInnerConeRcp	: packoffset( c8 );
	float4 CapsuleLen			: packoffset( c9 );
	float4 LightColorR			: packoffset( c10 );
	float4 LightColorG			: packoffset( c11 );
	float4 LightColorB			: packoffset( c12 );
}

/////////////////////////////////////////////////////////////////////////////
// Pixel shaders
/////////////////////////////////////////////////////////////////////////////


float4 dot4x4(float4 aX, float4 aY, float4 aZ, float4 bX, float4 bY, float4 bZ)
{
	return aX * bX + aY * bY + aZ * bZ;
}

// Dot product between a four three component vectors and a single three component vector
float4 dot4x1(float4 aX, float4 aY, float4 aZ, float3 b)
{
	return aX * b.xxxx + aY * b.yyyy + aZ * b.zzzz;
}

// Four light calculation helper function
float3 CalcFourLights(float3 position, Material material)
{
	float3 ToEye = EyePosition.xyz - position;
   
	// Find the shortest distance between the pixel and capsules segment
	float4 ToCapsuleStartX = position.xxxx - LightPosX;
	float4 ToCapsuleStartY = position.yyyy - LightPosY;
	float4 ToCapsuleStartZ = position.zzzz - LightPosZ;
	float4 DistOnLine = dot4x4(ToCapsuleStartX, ToCapsuleStartY, ToCapsuleStartZ, LightDirX, LightDirY, LightDirZ);
	float4 CapsuleLenSafe = max(CapsuleLen, 1.e-6);
	DistOnLine = CapsuleLen * saturate(DistOnLine / CapsuleLenSafe);
	float4 PointOnLineX = LightPosX + LightDirX * DistOnLine;
	float4 PointOnLineY = LightPosY + LightDirY * DistOnLine;
	float4 PointOnLineZ = LightPosZ + LightDirZ * DistOnLine;
	float4 ToLightX = PointOnLineX - position.xxxx;
	float4 ToLightY = PointOnLineY - position.yyyy;
	float4 ToLightZ = PointOnLineZ - position.zzzz;
	float4 DistToLightSqr = dot4x4(ToLightX, ToLightY, ToLightZ, ToLightX, ToLightY, ToLightZ);
	float4 DistToLight = sqrt(DistToLightSqr);
   
	// Phong diffuse
	ToLightX /= DistToLight; // Normalize
	ToLightY /= DistToLight; // Normalize
	ToLightZ /= DistToLight; // Normalize
	float4 NDotL = saturate(dot4x1(ToLightX, ToLightY, ToLightZ, material.normal));
	//float3 finalColor = float3(dot(LightColorR, NDotL), dot(LightColorG, NDotL), dot(LightColorB, NDotL));
   
	// Blinn specular
	ToEye = normalize(ToEye);
	float4 HalfWayX = ToEye.xxxx + ToLightX;
	float4 HalfWayY = ToEye.yyyy + ToLightY;
	float4 HalfWayZ = ToEye.zzzz + ToLightZ;
	float4 HalfWaySize = sqrt(dot4x4(HalfWayX, HalfWayY, HalfWayZ, HalfWayX, HalfWayY, HalfWayZ));
	float4 NDotH = saturate(dot4x1(HalfWayX / HalfWaySize, HalfWayY / HalfWaySize, HalfWayZ / HalfWaySize, material.normal));
	float4 SpecValue = pow(NDotH, material.specExp.xxxx) * material.specIntensity;
	//finalColor += float3(dot(LightColorR, SpecValue), dot(LightColorG, SpecValue), dot(LightColorB, SpecValue));
   
	// Cone attenuation
	float4 cosAng = dot4x4(LightDirX, LightDirY, LightDirZ, ToLightX, ToLightY, ToLightZ);
	float4 conAtt = saturate((cosAng - SpotCosOuterCone) * SpotCosInnerConeRcp);
	conAtt *= conAtt;
   
	// Attenuation
	float4 DistToLightNorm = 1.0 - saturate(DistToLight * LightRangeRcp);
	float4 Attn = DistToLightNorm * DistToLightNorm;
	Attn *= conAtt; // Include the cone attenuation

	// Calculate the final color value
	float4 pixelIntensity = (NDotL + SpecValue) * Attn;
	float3 finalColor = float3(dot(LightColorR, pixelIntensity), dot(LightColorG, pixelIntensity), dot(LightColorB, pixelIntensity)); 
	finalColor *= material.diffuseColor;
   
	return finalColor;
}

float4 FourLightPS( VS_OUTPUT In ) : SV_TARGET0
{
	// Prepare the material structure
	Material material = PrepareMaterial(In.Normal, In.UV);

	// Calculate the spot light color
	float3 finalColor = CalcFourLights(In.WorldPos, material);

	// Return the final color
	return float4(finalColor, 1.0);
}