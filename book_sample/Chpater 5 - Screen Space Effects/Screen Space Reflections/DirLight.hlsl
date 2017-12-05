#include "common.hlsl"

Texture2DArray<float> CascadeShadowMapTexture : register( t4 );
Texture2D<float> AOTexture : register( t5 );

/////////////////////////////////////////////////////////////////////////////
// shader input/output structure
/////////////////////////////////////////////////////////////////////////////

cbuffer cbDirLight : register( b1 )
{
	float3 AmbientDown			: packoffset( c0 );
	float3 AmbientRange			: packoffset( c1 );
	float3 DirToLight			: packoffset( c2 );
	float3 DirLightColor		: packoffset( c3 );
	float4x4 ToShadowSpace		: packoffset( c4 );
	float4 ToCascadeOffsetX		: packoffset( c8 );
	float4 ToCascadeOffsetY		: packoffset( c9 );
	float4 ToCascadeScale		: packoffset( c10 );
}

static const float2 arrBasePos[4] = {
	float2(1.0, 1.0),
	float2(1.0, -1.0),
	float2(-1.0, 1.0),
	float2(-1.0, -1.0),
};

static const float2 arrUV[4] = {
	float2(1.0, 0.0),
	float2(1.0, 1.0),
	float2(0.0, 0.0),
	float2(0.0, 1.0),
};

/////////////////////////////////////////////////////////////////////////////
// Vertex shader
/////////////////////////////////////////////////////////////////////////////
struct VS_OUTPUT
{
    float4 Position : SV_Position; // vertex position 
	float2 cpPos	: TEXCOORD0;
	float2 UV		: TEXCOORD1;
};

VS_OUTPUT DirLightVS( uint VertexID : SV_VertexID )
{
	VS_OUTPUT Output;

	Output.Position = float4( arrBasePos[VertexID].xy, 0.0, 1.0);
	Output.cpPos = Output.Position.xy;
	Output.UV = arrUV[VertexID].xy;

	return Output;    
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

// Cascaded shadow calculation
float CascadedShadow(float3 position)
{
	// Transform the world position to shadow space
	float4 posShadowSpace = mul(float4(position, 1.0), ToShadowSpace);

	// Transform the shadow space position into each cascade position
	float4 posCascadeSpaceX = (ToCascadeOffsetX + posShadowSpace.xxxx) * ToCascadeScale;
	float4 posCascadeSpaceY = (ToCascadeOffsetY + posShadowSpace.yyyy) * ToCascadeScale;

	// Check which cascade we are in
	float4 inCascadeX = abs(posCascadeSpaceX) <= 1.0;
	float4 inCascadeY = abs(posCascadeSpaceY) <= 1.0;
	float4 inCascade = inCascadeX * inCascadeY;

	// Prepare a mask for the highest quality cascade the position is in
	float4 bestCascadeMask = inCascade;
	bestCascadeMask.yzw = (1.0 - bestCascadeMask.x) * bestCascadeMask.yzw;
	bestCascadeMask.zw = (1.0 - bestCascadeMask.y) * bestCascadeMask.zw;
	bestCascadeMask.w = (1.0 - bestCascadeMask.z) * bestCascadeMask.w;
	float bestCascade = dot(bestCascadeMask, float4(0.0, 1.0, 2.0, 3.0));

	// Pick the position in the selected cascade
	float3 UVD;
	UVD.x = dot(posCascadeSpaceX, bestCascadeMask);
	UVD.y = dot(posCascadeSpaceY, bestCascadeMask);
	UVD.z = posShadowSpace.z;

	// Convert to shadow map UV values
	UVD.xy = 0.5 * UVD.xy + 0.5;
	UVD.y = 1.0 - UVD.y;

	// Compute the hardware PCF value
	float shadow = CascadeShadowMapTexture.SampleCmpLevelZero(PCFSampler, float3(UVD.xy, bestCascade), UVD.z); 
	
	// set the shadow to one (fully lit) for positions with no cascade coverage
	shadow = saturate(shadow + 1.0 - any(bestCascadeMask));
	
	return shadow;
}

// Directional light calculation helper function
float3 CalcDirectional(float3 position, Material material, bool bUseShadow)
{
	// Phong diffuse
	float NDotL = dot(DirToLight, material.normal);
	float3 finalColor = DirLightColor.rgb * saturate(NDotL);
   
	// Blinn specular
	float3 ToEye = EyePosition - position;
	ToEye = normalize(ToEye);
	float3 HalfWay = normalize(ToEye + DirToLight);
	float NDotH = saturate(dot(HalfWay, material.normal));
	finalColor += DirLightColor.rgb * pow(NDotH, material.specPow) * material.specIntensity;

	// Take shadows into consideration
	float shadowAtt;
	if(bUseShadow)
	{
		shadowAtt = CascadedShadow(position);
	}
	else
	{
		// No shadow attenuation
		shadowAtt = 1.0;
	}

	//return shadowAtt;
	return finalColor * material.diffuseColor.rgb * shadowAtt;
}

float4 DirLightCommonPS( VS_OUTPUT In, bool bUseShadow ) : SV_TARGET
{
	// Unpack the GBuffer
	SURFACE_DATA gbd = UnpackGBuffer_Loc(In.Position.xy);
	
	// Convert the data into the material structure
	Material mat;
	MaterialFromGBuffer(gbd, mat);

	// Reconstruct the world position
	float3 position = CalcWorldPos(In.cpPos, gbd.LinearDepth);

	// Get the AO value
	float ao = AOTexture.Sample(LinearSampler, In.UV);

	// Calculate the ambient color and scale it by the AO value
	float3 finalColor = CalcAmbient(mat.normal, mat.diffuseColor.rgb) * ao;

	// Calculate the directional light
	finalColor += CalcDirectional(position, mat, bUseShadow);

	// Return the final color
	return float4(finalColor, 1.0);
}

float4 DirLightPS( VS_OUTPUT In ) : SV_TARGET
{
	return DirLightCommonPS(In, false);
}

float4 DirLightShadowPS( VS_OUTPUT In ) : SV_TARGET
{
	return DirLightCommonPS(In, true);
}

/////////////////////////////////////////////////////////////////////////////
// Debug cascades
/////////////////////////////////////////////////////////////////////////////

float4 CascadeShadowDebugPS( VS_OUTPUT In ) : SV_TARGET
{
	// Unpack the GBuffer
	SURFACE_DATA gbd = UnpackGBuffer_Loc(In.Position.xy);

	// Reconstruct the world position
	float3 position = CalcWorldPos(In.cpPos, gbd.LinearDepth);

	// Transform the world position to shadow space
	float4 posShadowSpace = mul(float4(position, 1.0), ToShadowSpace);

	// Transform the shadow space position into each cascade position
	float4 posCascadeSpaceX = (ToCascadeOffsetX + posShadowSpace.xxxx) * ToCascadeScale;
	float4 posCascadeSpaceY = (ToCascadeOffsetY + posShadowSpace.yyyy) * ToCascadeScale;

	// Check which cascade we are in
	float4 inCascadeX = abs(posCascadeSpaceX) <= 1.0;
	float4 inCascadeY = abs(posCascadeSpaceY) <= 1.0;
	float4 inCascade = inCascadeX * inCascadeY;

	// Prepare a mask for the highest quality cascade the position is in
	float4 bestCascadeMask = inCascade;
	bestCascadeMask.yzw = (1.0 - bestCascadeMask.x) * bestCascadeMask.yzw;
	bestCascadeMask.zw = (1.0 - bestCascadeMask.y) * bestCascadeMask.zw;
	bestCascadeMask.w = (1.0 - bestCascadeMask.z) * bestCascadeMask.w;

	return 0.5 * bestCascadeMask;
}