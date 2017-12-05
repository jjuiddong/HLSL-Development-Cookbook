Texture2D<float4> HDRTex : register( t0 );
StructuredBuffer<float> AvgLum : register( t1 );
Texture2D<float4> BloomTex : register( t2 );
Texture2D<float4> DOFBlurTex : register( t3 );
Texture2D<float> DepthTex : register( t4 );

SamplerState PointSampler : register( s0 );
SamplerState LinearSampler : register( s1 );

static const float2 arrBasePos[4] = {
	float2(-1.0, 1.0),
	float2(1.0, 1.0),
	float2(-1.0, -1.0),
	float2(1.0, -1.0),
};

static const float2 arrUV[4] = {
	float2(0.0, 0.0),
	float2(1.0, 0.0),
	float2(0.0, 1.0),
	float2(1.0, 1.0),
};

//-----------------------------------------------------------------------------------------
// Vertex shader
//-----------------------------------------------------------------------------------------
struct VS_OUTPUT
{
    float4 Position : SV_Position; // vertex position 
	float2 UV		: TEXCOORD0;
};

VS_OUTPUT FullScreenQuadVS( uint VertexID : SV_VertexID )
{
    VS_OUTPUT Output;

    Output.Position = float4( arrBasePos[VertexID].xy, 0.0, 1.0);
    Output.UV = arrUV[VertexID].xy;
    
    return Output;    
}

//-----------------------------------------------------------------------------------------
// Pixel shader
//-----------------------------------------------------------------------------------------

cbuffer FinalPassConstants : register( b0 )
{
	// Tone mapping
	float MiddleGrey : packoffset( c0 );
	float LumWhiteSqr : packoffset( c0.y );
	float BloomScale : packoffset( c0.z );
	float2 ProjValues : packoffset( c1 );
	float2 DOFFarValues : packoffset( c1.z );
}

static const float3 LUM_FACTOR = float3(0.299, 0.587, 0.114);

float ConvertZToLinearDepth(float depth)
{
	float linearDepth = ProjValues.x / (depth + ProjValues.y);
	return linearDepth;
}

float3 DistanceDOF(float3 colorFocus, float3 colorBlurred, float depth)
{
	// Find the depth based blur factor
	float blurFactor = saturate((depth - DOFFarValues.x) * DOFFarValues.y);

	// Lerp with the blurred color based on the CoC factor
	return lerp(colorFocus, colorBlurred, blurFactor);
}

float3 ToneMapping(float3 HDRColor)
{
	// Find the luminance scale for the current pixel
	float LScale = dot(HDRColor, LUM_FACTOR);
	LScale *= MiddleGrey / AvgLum[0];
	LScale = (LScale + LScale * LScale / LumWhiteSqr) / (1.0 + LScale);
	return HDRColor * LScale; // Apply the luminance scale to the pixels color
}

float4 FinalPassPS( VS_OUTPUT In ) : SV_TARGET
{
	// Get the color and depth samples
	float3 color = HDRTex.Sample( PointSampler, In.UV.xy ).xyz;

	// Distance DOF only on pixels that are not on the far plane
	float depth = DepthTex.Sample( PointSampler, In.UV.xy );
	if(depth < 1.0)
	{
		// Get the blurred color from the down scaled HDR texture
		float3 colorBlurred = DOFBlurTex.Sample( LinearSampler, In.UV.xy ).xyz;

		// Convert the full resulotion depth to linear depth
		depth = ConvertZToLinearDepth(depth);

		// Compute the distance DOF color
		color = DistanceDOF(color, colorBlurred, depth);
	}

	// Add the bloom contribution
	color += BloomScale * BloomTex.Sample( LinearSampler, In.UV.xy ).xyz;

	// Tone mapping
	color = ToneMapping(color);

	// Output the LDR value
	return float4(color, 1.0);
}