/////////////////////////////////////////////////////////////////////////////
// Constant Buffers
/////////////////////////////////////////////////////////////////////////////
cbuffer cbPerObjectVS : register( b0 ) // Model constants
{
    float4x4 WorldViewProjection	: packoffset( c0 );
	float4x4 World					: packoffset( c4 );
}

cbuffer cbPerObjectPS : register( b0 ) // Model constants
{
    float3 EyePosition	: packoffset( c0 );
	float specExp		: packoffset( c0.w );
	float specIntensity	: packoffset( c1 );
}

/////////////////////////////////////////////////////////////////////////////
// Diffuse texture and linear sampler
/////////////////////////////////////////////////////////////////////////////
Texture2D    DiffuseTexture	: register( t0 );
SamplerState LinearSampler	: register( s0 );

/////////////////////////////////////////////////////////////////////////////
// shader input/output structure
/////////////////////////////////////////////////////////////////////////////
struct VS_INPUT
{
    float4 Position	: POSITION;		// vertex position 
    float3 Normal	: NORMAL;		// vertex normal
    float2 UV		: TEXCOORD0;	// vertex texture coords 
};

struct VS_OUTPUT
{
    float4 Position	: SV_POSITION;	// vertex position 
    float2 UV		: TEXCOORD0;	// vertex texture coords
	float3 Normal	: TEXCOORD1;	// vertex normal
	float3 WorldPos	: TEXCOORD2;	// vertex world position
};

/////////////////////////////////////////////////////////////////////////////
// Vertex shader
/////////////////////////////////////////////////////////////////////////////
float4 DepthPrePassVS(float4 Position : POSITION) : SV_POSITION
{
	return mul( Position, WorldViewProjection );
}

VS_OUTPUT RenderSceneVS( VS_INPUT input )
{
    VS_OUTPUT Output;
    float3 vNormalWorldSpace;
    
    // Transform the position from object space to homogeneous projection space
    Output.Position = mul( input.Position, WorldViewProjection );
    
	// Transform the position to world space
	Output.WorldPos = mul(input.Position, World).xyz;

    // Just copy the texture coordinate through
    Output.UV = input.UV; 

	Output.Normal = mul(input.Normal, (float3x3)World);
    
    return Output;    
}

/////////////////////////////////////////////////////////////////////////////
// Pixel shaders
/////////////////////////////////////////////////////////////////////////////

// Material preparation
struct Material
{
   float3 normal;
   float4 diffuseColor;
   float specExp;
   float specIntensity;
};

Material PrepareMaterial(float3 normal, float2 UV)
{
	Material material;

	// Normalize the interpulated vertex normal
	material.normal = normalize(normal);

	// Sample the texture and convert to linear space
    material.diffuseColor = DiffuseTexture.Sample( LinearSampler, UV );
	material.diffuseColor.rgb *= material.diffuseColor.rgb;

	// Copy the specular values from the constant buffer
	material.specExp = specExp;
	material.specIntensity = specIntensity;

	return material;
} 
