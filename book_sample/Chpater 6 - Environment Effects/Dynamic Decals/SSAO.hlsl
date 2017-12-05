//-----------------------------------------------------------------------------------------
// Depth downscale
//-----------------------------------------------------------------------------------------
Texture2D<float> DepthTex : register( t0 );
Texture2D<float3> NormalsTex : register( t1 );

RWStructuredBuffer<float4> MiniDepthRW : register( u0 );

cbuffer DownScaleConstants : register( b0 )
{
    uint2 Res : packoffset( c0 );   // Resulotion of the down scaled image: x - width, y - height
	float2 ResRcp  : packoffset( c0.z );
	float4 ProjParams : packoffset( c1 );
	float4x4 ViewMatrix : packoffset( c2 );
	float OffsetRadius : packoffset( c6 );
	float Radius : packoffset( c6.y );
	float MaxDepth  : packoffset( c6.z );
}

float ConvertZToLinearDepth(float depth)
{
	float linearDepth = ProjParams.z / (depth + ProjParams.w);
	return linearDepth;
}

[numthreads(1024, 1, 1)]
void DepthDownscale(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint3 CurPixel = uint3(dispatchThreadId.x % Res.x, dispatchThreadId.x / Res.x, 0);

	// Skip out of bound pixels
	if(CurPixel.y < Res.y)
	{
		float minDepth = 1.0;
		float3 avgNormalWorld = float3(0.0, 0.0, 0.0);
		uint3 FullResPixel = CurPixel * 2;

		[unroll]
		for(int i = 0; i < 2; i++)
		{
			[unroll]
			for(int j = 0; j < 2; j++)
			{
				// Get the pixels depth and store the minimum depth
				float curDepth = DepthTex.Load(FullResPixel, int2(j, i));
				minDepth = min(curDepth, minDepth);

				// Sum the viewspace normals so we can average them
				float3 normalWorld = NormalsTex.Load(FullResPixel, int2(j, i));
				avgNormalWorld += normalize(normalWorld * 2.0 - 1.0);
			}
		}

		MiniDepthRW[dispatchThreadId.x].x = ConvertZToLinearDepth(minDepth);
		float3 avgNormalView = mul(avgNormalWorld * 0.25, (float3x3)ViewMatrix);
		MiniDepthRW[dispatchThreadId.x].yzw = avgNormalView;
	}
}

//-----------------------------------------------------------------------------------------
// SSAO Compute
//-----------------------------------------------------------------------------------------

StructuredBuffer<float4> MiniDepth : register( t0 );

RWTexture2D<float> AO : register( u0 );

groupshared float SharedDepths[1024];

// Possion disc sampling pattern
static const float NumSamplesRcp = 1.0 / 8.0;
static const uint NumSamples = 8;
static const float2 SampleOffsets[NumSamples] = {
	float2(0.2803166, 0.08997212),
	float2(-0.5130632, 0.6877457),
	float2(0.425495, 0.8665376),
	float2(0.8732584, 0.3858971),
	float2(0.0498111, -0.6287371),
	float2(-0.9674183, 0.1236534),
	float2(-0.3788098, -0.09177673),
	float2(0.6985874, -0.5610316),
};

float GetDepth(int2 pos)
{
	// Clamp the input pixel position
	float x = clamp(pos.x, 0, Res.x-1);
	float y = clamp(pos.y, 0, Res.y-1);

	// find the mini-depth index position and retrive the detph value
	int miniDepthIdx = x + y * Res.x;
	return MiniDepth[miniDepthIdx].x;
}

float3 GetNormal(int2 pos)
{
	// Clamp the input pixel position
	float x = clamp(pos.x, 0, Res.x-1);
	float y = clamp(pos.y, 0, Res.y-1);

	int miniDepthIdx = x + y * Res.x; // find the mini-depth index position
	return MiniDepth[miniDepthIdx].yzw;
}

float ComputeAO(int2 cetnerPixelPos, float2 centerClipPos)
{
	// Get the depths for the normal calculation
	float centerDepth = GetDepth(cetnerPixelPos.xy);

	float isNotSky = centerDepth < MaxDepth;
		
	// Find the center pixel veiwspace position
	float3 centerPos;
	centerPos.xy = centerClipPos * ProjParams.xy * centerDepth;
	centerPos.z = centerDepth;

	// Get the view space normal for the center pixel
	float3 centerNormal = GetNormal(cetnerPixelPos.xy);
	centerNormal = normalize(centerNormal);
		
	// Prepare for random sampling offset
	float rotationAngle = 0.0;
	//float rotationAngle = dot(float2(centerClipPos), float2(73.0, 197.0));
	float2 randSinCos;
	sincos(rotationAngle, randSinCos.x, randSinCos.y);
	float2x2 randRotMat = float2x2(randSinCos.y, -randSinCos.x, randSinCos.x, randSinCos.y);

	// Take the samples and calculate the ambient occlusion value for each
	float ao = 0.0;
	[unroll]
	for(uint i=0; i < NumSamples; i++)
	{
		// Find the texture space position and depth
		float2 sampleOff = OffsetRadius.xx * mul(SampleOffsets[i], randRotMat);
		float curDepth = GetDepth(cetnerPixelPos + int2(sampleOff.x, -sampleOff.y));

		// Calculate the view space position
		float3 curPos;
		curPos.xy = (centerClipPos + 2.0 * sampleOff * ResRcp) * ProjParams.xy * curDepth;
		curPos.z = curDepth;

		float3 centerToCurPos = curPos - centerPos;
		float lenCenterToCurPos = length(centerToCurPos);
		float angleFactor = 1.0 - dot(centerToCurPos / lenCenterToCurPos, centerNormal);
		float distFactor = lenCenterToCurPos / Radius;

		ao += saturate(max(distFactor, angleFactor)) * isNotSky;
	}

	return ao * NumSamplesRcp;
}

[numthreads(1024, 1, 1)]
void SSAOCompute(uint3 groupThreadId : SV_GroupThreadID, uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint2 CurPixel = uint2(dispatchThreadId.x % Res.x, dispatchThreadId.x / Res.x);

	SharedDepths[groupThreadId.x] = MiniDepth[dispatchThreadId.x].x;

	GroupMemoryBarrierWithGroupSync();

	// Skip out of bound pixels
	if(CurPixel.y < Res.y)
	{
		// Find the XY clip space position for the current pixel
		// Y has to be inverted
		float2 centerClipPos = 2.0 * float2(CurPixel) * ResRcp;
		centerClipPos = float2(centerClipPos.x - 1.0, 1.0 - centerClipPos.y);

		AO[CurPixel] = ComputeAO(CurPixel, centerClipPos);
	}
}
