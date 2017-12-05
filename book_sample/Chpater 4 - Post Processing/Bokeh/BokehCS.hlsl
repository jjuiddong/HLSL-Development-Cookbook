static const float4 LUM_FACTOR = float4(0.299, 0.587, 0.114, 0);

cbuffer BokehConstants : register( b0 )
{
	uint2 Res : packoffset( c0 );
	float2 ProjValues : packoffset( c0.z );
	float2 DOFFarValues : packoffset( c1 );
	float MiddleGrey : packoffset( c1.z );
	float LumWhiteSqr : packoffset( c1.w );
	float BokehBlurThreshold : packoffset( c2 );
	float BokehLumThreshold : packoffset( c2.y );
	float RadiusScale : packoffset( c2.z );
	float ColorScale : packoffset( c2.w );
}

Texture2D<float4> HDRTex : register( t0 );
Texture2D<float> DepthTex : register( t1 );
StructuredBuffer<float> AvgLum : register( t2 );

struct TBokeh
{
	float2 Pos;
	float Radius;
	float4 Color;
};
AppendStructuredBuffer<TBokeh> BokehStack : register( u0 );

float ConvertZToLinearDepth(float depth)
{
	float linearDepth = ProjValues.x / (depth + ProjValues.y);
	return linearDepth;
}

[numthreads(1024, 1, 1)]
void HighlightScan(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint2 CurPixel = uint2(dispatchThreadId.x % Res.x, dispatchThreadId.x / Res.x);

	// Skip out of bound pixels
	if(CurPixel.y < Res.y)
	{
		// First we need to find the CoC value for this pixel
		float depth = DepthTex.Load(int3(CurPixel, 0));
		if(depth < 1.0) // Ignore sky
		{
			depth = ConvertZToLinearDepth(depth);
			float blurFactor = saturate((depth - DOFFarValues.x) * DOFFarValues.y);

			if(blurFactor > BokehBlurThreshold)
			{
				float4 color = HDRTex.Load(int3(CurPixel, 0));
				float Lum = dot(color, LUM_FACTOR);
				float avgLum = AvgLum[0];

				float lumFactor = saturate(Lum - avgLum * BokehLumThreshold);

				if(lumFactor > 0.0)
				{
					TBokeh bokeh;
					bokeh.Pos = ((2.0 * float2(CurPixel.x, Res.y - CurPixel.y)) / Res) - 1.0;
					bokeh.Radius = (blurFactor - BokehBlurThreshold)* RadiusScale;

					// Tone map the color
					float LScale = Lum * MiddleGrey / avgLum;
					LScale = (LScale + LScale * LScale / LumWhiteSqr) / (1.0 + LScale);
					bokeh.Color.xyz = color.xyz * LScale * lumFactor;
					bokeh.Color.w = ColorScale;

					BokehStack.Append(bokeh);
				}
			}
		}
	}
}
