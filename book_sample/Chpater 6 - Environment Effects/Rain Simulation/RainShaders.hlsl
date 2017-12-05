struct VSRainOut
{
    float4 Pos : SV_Position;
	float Clip : SV_ClipDistance0;
	float2 Tex : TEXCOORD0;
};

struct PSRainIn
{
    float2 tex : TEXCOORD0;
};

cbuffer RainConstants : register( b0 )
{
    float4x4 ViewProj	: packoffset( c0 );
	float3 ViewDir		: packoffset( c4 );
	float Scale			: packoffset( c4.w );
    float4 AmbientColor	: packoffset( c5 );
};

static const float2 arrBasePos[6] =
{
 	float2( 1.0, -1.0 ),
 	float2( 1.0, 0.0 ),
 	float2( -1.0, -1.0 ),

 	float2( -1.0, -1.0 ),
	float2( 1.0, 0.0 ),
	float2( -1.0, 0.0 ),
};
    
static const float2 arrUV[6] = 
{ 
	float2(1.0, 0.0),
	float2(1.0, 1.0),
	float2(0.0, 0.0),

	float2(0.0, 0.0),
	float2(1.0, 1.0),
	float2(0.0, 1.0),
};

struct CSBuffer
{
	float3 Pos;
	float3 Vel;
	float State;
};

// Simulation data used by the VS
StructuredBuffer<CSBuffer> RainData : register( t0 );

// Rain streak texture used by the PS
Texture2D RainStreakTex : register( t0 );
SamplerState LinearSampler : register( s0 );

VSRainOut VS_Rain(uint VertexID : SV_VERTEXID)
{
	VSRainOut output;

	// Get the current raindrop information
	CSBuffer curDrop = RainData[VertexID / 6];

	// Get the base posotion
    float3 pos = curDrop.Pos;

	// Find the expension directions
    float3 rainDir = normalize(curDrop.Vel);
	float3 rainRight = normalize(cross(ViewDir, rainDir));

	// Extend the drop position to the streak corners
	float2 offsets = arrBasePos[VertexID % 6];
	pos += rainRight * offsets.x * Scale * 0.025;
	pos += rainDir * offsets.y * Scale;

	// Transform each corner to projected space
	output.Pos = mul(float4(pos, 1.0), ViewProj);

	// Just copy the UV coordinates
	output.Tex = arrUV[VertexID % 6];
	
	// Use the state to clip particles that colided with the ground
	output.Clip = curDrop.State;

	return output;
}

float4 PS_Rain(VSRainOut In) : SV_Target
{  
	float fTexAlpha = RainStreakTex.Sample(LinearSampler, In.Tex).r;
	return float4(AmbientColor.rgb, AmbientColor.a * fTexAlpha);
}