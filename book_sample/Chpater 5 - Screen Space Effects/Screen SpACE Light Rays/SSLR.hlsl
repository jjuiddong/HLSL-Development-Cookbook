//-----------------------------------------------------------------------------------------
// Occlusion
//-----------------------------------------------------------------------------------------

Texture2D<float> DepthTex : register( t0 );
RWTexture2D<float> OcclusionRW : register( u0 );

cbuffer OcclusionConstants : register( b0 )
{
	uint2 Res : packoffset( c0 );
}


[numthreads(1024, 1, 1)]
void Occlussion(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint3 CurPixel = uint3(dispatchThreadId.x % Res.x, dispatchThreadId.x / Res.x, 0);

	// Skip out of bound pixels
	if(CurPixel.y < Res.y)
	{
		// Get the depth
		float curDepth = DepthTex.Load(CurPixel);

		// Flag anything closer than the sky as occlusion
		OcclusionRW[CurPixel.xy].x = curDepth == 0.0;
	}
}

//-----------------------------------------------------------------------------------------
// Ray tracing
//-----------------------------------------------------------------------------------------

cbuffer RayTraceConstants : register( b0 )
{
	float2 SunPos : packoffset( c0 );
	float InitDecay : packoffset( c0.z );
	float DistDecay : packoffset( c0.w );
	float3 RayColor : packoffset( c1 );
	float MaxDeltaLen : packoffset( c1.w );
}

Texture2D<float> OcclusionTex : register( t0 );
SamplerState LinearSampler : register( s0 );

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

struct VS_OUTPUT
{
	float4 Position	: SV_Position;
	float2 UV		: TEXCOORD0;
};

VS_OUTPUT RayTraceVS( uint VertexID : SV_VertexID )
{
    VS_OUTPUT Output;

	Output.Position = float4(arrBasePos[VertexID].xy, 0.0, 1.0);
	Output.UV = arrUV[VertexID].xy;

	return Output;    
}

static const int NUM_STEPS = 64;
static const float NUM_DELTA = 1.0 / 63.0f;
float4 RayTracePS( VS_OUTPUT In ) : SV_TARGET
{
	// Find the direction and distance to the sun
	float2 dirToSun = (SunPos - In.UV);
	float lengthToSun = length(dirToSun);
	dirToSun /= lengthToSun;

	// Find the ray delta
	float deltaLen = min(MaxDeltaLen, lengthToSun * NUM_DELTA);
	float2 rayDelta = dirToSun * deltaLen;

	// Each step decay	
	float stepDecay = DistDecay * deltaLen;

	// Initial values
	float2 rayOffset = float2(0.0, 0.0);
	float decay = InitDecay;
	float rayIntensity = 0.0f;

	// Ray march towards the sun
	for(int i = 0; i < NUM_STEPS ; i++)
	{
		// Sample at the current location
		float2 sampPos = In.UV + rayOffset;
		float fCurIntensity = OcclusionTex.Sample( LinearSampler, sampPos );
		
		// Sum the intensity taking decay into account
		rayIntensity += fCurIntensity * decay;

		// Advance to the next position
		rayOffset += rayDelta;

		// Update the decay
		decay = saturate(decay - stepDecay);
	}

	return float4(rayIntensity, 0.0, 0.0, 0.0);
}

//-----------------------------------------------------------------------------------------
// Combine results
//-----------------------------------------------------------------------------------------

Texture2D<float> LightRaysTex : register( t0 );

float4 CombinePS( VS_OUTPUT In ) : SV_TARGET
{
	// Ge the ray intensity
	float rayIntensity = LightRaysTex.Sample( LinearSampler, In.UV );

	// Return the color scaled by the intensity
	return float4(RayColor * rayIntensity, 1.0);
}
