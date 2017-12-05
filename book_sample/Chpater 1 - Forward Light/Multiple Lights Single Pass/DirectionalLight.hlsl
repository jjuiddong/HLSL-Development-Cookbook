#include "ForwardLightCommon.hlsl"

/////////////////////////////////////////////////////////////////////////////
// Constant Buffers
/////////////////////////////////////////////////////////////////////////////

cbuffer cbDirLightPS : register( b1 ) // Directional and ambient light constants
{
	float3 AmbientDown		: packoffset( c0 );
	float3 AmbientRange		: packoffset( c1 );
	float3 DirToLight		: packoffset( c2 );
	float3 DirLightColor	: packoffset( c3 );
}

/////////////////////////////////////////////////////////////////////////////
// Pixel shaders
///////////////////////////////////////////////////////////////////////////// 

// Ambient light calculation helper function
float3 CalcAmbient(float3 normal, float3 color)
{
	// Convert from [-1, 1] to [0, 1]
	float up = normal.y * 0.5 + 0.5;

	// Calculate the ambient value
	float3 ambient = AmbientDown + up * AmbientRange;

	// Apply the ambient value to the color
	return ambient * color;
}

// Directional light calculation helper function
float3 CalcDirectional(float3 position, Material material)
{
   // Phong diffuse
   float NDotL = dot(DirToLight, material.normal);
   float3 finalColor = DirLightColor.rgb * saturate(NDotL);
   
   // Blinn specular
   float3 ToEye = EyePosition.xyz - position;
   ToEye = normalize(ToEye);
   float3 HalfWay = normalize(ToEye + DirToLight);
   float NDotH = saturate(dot(HalfWay, material.normal));
   finalColor += DirLightColor.rgb * pow(NDotH, material.specExp) * material.specIntensity;
   
   return finalColor * material.diffuseColor.rgb;
}

float4 DirectionalLightPS( VS_OUTPUT In ) : SV_TARGET0
{
	// Prepare the material structure
	Material material = PrepareMaterial(In.Normal, In.UV);

	// Calculate the ambient color
	float3 finalColor = CalcAmbient(material.normal, material.diffuseColor.rgb);
   
	// Calculate the directional light
	finalColor += CalcDirectional(In.WorldPos, material);

	// Return the final color
	return float4(finalColor, 1.0);
}