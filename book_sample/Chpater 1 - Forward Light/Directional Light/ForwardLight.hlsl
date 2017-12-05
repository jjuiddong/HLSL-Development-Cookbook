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

cbuffer cbDirLightPS : register( b1 ) // Directional and ambient light constants
{
	float3 AmbientDown		: packoffset( c0 );
	float3 AmbientRange		: packoffset( c1 );
	float3 DirToLight		: packoffset( c2 );
	float3 DirLightColor	: packoffset( c3 );
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

	// Apply the final light color to the pixels diffuse color
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