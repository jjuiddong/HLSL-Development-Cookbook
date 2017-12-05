//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
static const int MAX_FLARES = 8;

struct FLARE
{
	float4 Position;
	float4 ScaleRotate;
	float4 Color;
};

cbuffer cbLensflare : register( b0 )
{
	FLARE Flares[MAX_FLARES] : packoffset( c0 );
}

static const float2 arrBasePos[6] = {
	float2(-1.0, 1.0),
	float2(1.0, 1.0),
	float2(-1.0, -1.0),
	float2(1.0, 1.0),
	float2(1.0, -1.0),
	float2(-1.0, -1.0),
};

static const float2 arrUV[6] = {
	float2(0.0, 0.0),
	float2(1.0, 0.0),
	float2(0.0, 1.0),
	float2(1.0, 0.0),
	float2(1.0, 1.0),
	float2(0.0, 1.0),
};

//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
Texture2D<float4> FlareTexture : register( t0 );
SamplerState LinearSampler : register( s0 );

//--------------------------------------------------------------------------------------
// Vertex shader output structure
//--------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 Pos : SV_Position;
    float2 UV : TEXCOORD0;
	float4 Col : TEXCOORD1;
};

//--------------------------------------------------------------------------------------
// Shaders
//--------------------------------------------------------------------------------------
VS_OUTPUT LensflareVS( uint VertexID : SV_VertexID )
{
    VS_OUTPUT Out;

	// Get the flares values
	FLARE flare = Flares[VertexID / 6];
    
	// Calculate the clip space position
	float2 pos2D;
	pos2D.x = dot(arrBasePos[VertexID % 6].xy, flare.ScaleRotate.xy);
	pos2D.y = dot(arrBasePos[VertexID % 6].xy, flare.ScaleRotate.zw);
	pos2D += flare.Position.xy;
    Out.Pos = float4(pos2D, 0.0, 1.0);
    
	// Pass the flare color
    Out.Col = flare.Color;
    
	// Pass the vertex UV
    Out.UV = arrUV[VertexID % 6]; 
    
    return Out;    
}

float4 LensflarePS( VS_OUTPUT In ) : SV_TARGET
{
	float4 texColor = FlareTexture.Sample(LinearSampler, In.UV);
	
	// Convert to linear color space
	texColor.rgb *= texColor.rgb;

	// Scale by the flares color
    return texColor * In.Col;
}