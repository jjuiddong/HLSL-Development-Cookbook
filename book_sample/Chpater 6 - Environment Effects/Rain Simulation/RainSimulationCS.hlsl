

Texture2D<float4> NoiseTex : register( t0 ); // Noise texture for getting random values
Texture2D<float> HeightTex : register( t1 ); // Height map of the scene for collision detection

// Single raindrop structure
struct RainDrop
{
	float3 Pos;
	float3 Vel;
	float State;
};

// Raindrop buffer
RWStructuredBuffer<RainDrop> RainData : register( u0 );

cbuffer RainConstants : register( b0 )
{
	float4x4 ToHeight		: packoffset( c0 );
	float3 BoundCenter		: packoffset( c4 );
    float DT				: packoffset( c4.w );
	float3 BoundHalfSize	: packoffset( c5 );
	float WindVeriation		: packoffset( c5.w );
	float2 WindFoce			: packoffset( c6 );
	float VertSpeed			: packoffset( c6.z );
	float HeightMapSize		: packoffset( c6.w );
}

static const int g_iNumDispatch = 4;
static const int g_iNumThreads = 32;
static const int g_iDimSize = g_iNumDispatch * g_iNumThreads;
[numthreads(g_iNumThreads, g_iNumThreads, 1)]
void SimulateRain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	// Grap the threads raindrop
	uint GroupIdx = dispatchThreadId.x + dispatchThreadId.y * g_iDimSize;
	RainDrop curDrop = RainData[GroupIdx];

	// Calculate the new position
	curDrop.Pos += curDrop.Vel * DT;

	// Keep the particle inside the bounds
	float2 offsetAmount = (curDrop.Pos.xz - BoundCenter.xz) / BoundHalfSize.xz;
	curDrop.Pos.xz -= BoundHalfSize.xz * ceil(0.5 * offsetAmount - 0.5);

	// Respawn the particle when it leaves the bound vertically
	if(abs(curDrop.Pos.y - BoundCenter.y) > BoundHalfSize.y)
	{
		// Respawn the particle with random values
		// Sample the noise from the texture
		float4 vNoiseNorm0 = NoiseTex.Load( int3(dispatchThreadId.xy, 0) );
		float4 vNoise0 = (vNoiseNorm0 * 2.0) - 1.0;

		// Align the position around the bound center
		curDrop.Pos.xz = BoundCenter.xz + BoundHalfSize.xz * vNoise0.xy;

		// Set the height to a random value close to the top of the bound
		curDrop.Pos.y = BoundCenter.y + BoundHalfSize.y;
		curDrop.Pos.y -= dot(vNoiseNorm0.zw, 0.2f) * BoundHalfSize.y;

		// Set the intitial veolcity based on the wind force
		curDrop.Vel.xz = lerp(WindFoce, WindFoce * vNoise0.zw, WindVeriation);
		curDrop.Vel.y = VertSpeed;
	}

	// Check if the particle colided with anything
	// First transform the drops world position to the height map space
	float4 posInHeight = float4(curDrop.Pos, 1.0);
	posInHeight = mul(posInHeight, ToHeight);
	posInHeight.xy = 0.5 * (posInHeight.xy + 1.0);
	posInHeight.y = 1.0 - posInHeight.y;
	posInHeight.xy *= HeightMapSize;

	// Sample the height value from the map
	float height = HeightTex.Load( uint3(posInHeight.xy, 0) );

	// Compare the height of the map value with the drop height
	curDrop.State = posInHeight.z < height ? 1.0 : -1.0;

	// Write the values back
	RainData[GroupIdx] = curDrop;
}
