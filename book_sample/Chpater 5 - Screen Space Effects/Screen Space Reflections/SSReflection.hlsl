Texture2D<float4> HDRTex : register( t0 );
Texture2D<float> DepthTex : register( t1 );
Texture2D<float3> NoramlTex : register( t2 );

SamplerState PointSampler : register( s1 );

//-----------------------------------------------------------------------------------------
// Vertex shader
//-----------------------------------------------------------------------------------------

cbuffer SSReflectionPSConstants : register( b0 )
{
	float4x4 WorldViewProjection	: packoffset( c0 );
	float4x4 WorldView				: packoffset( c4 );
}

struct VS_OUTPUT
{
    float4 Position		: SV_Position; 
	float4 ViewPosition	: TEXCOORD0;
	float3 ViewNormal	: TEXCOORD1;
	float3 csPos		: TEXCOORD2;
};

VS_OUTPUT SSReflectionVS(float4 pos : POSITION, float3 norm : NORMAL)
{
	VS_OUTPUT Output;

	// Transform to projected space
	Output.Position = mul(pos, WorldViewProjection);

	// Transform the position and normal to view space
	Output.ViewPosition = mul(pos, WorldView);
	Output.ViewNormal = mul(norm, (float3x3)WorldView);

	// Convert the projected position to clip-space
	Output.csPos = Output.Position.xyz / Output.Position.w;

	return Output;    
}

//-----------------------------------------------------------------------------------------
// Pixel shader
//-----------------------------------------------------------------------------------------

cbuffer SSReflectionVSConstants : register( b0 )
{
	float4x4 ProjMatrix			: packoffset( c0 );
	float ViewAngleThreshold	: packoffset( c4 );
	float EdgeDistThreshold		: packoffset( c4.y );
	float DepthBias				: packoffset( c4.z );
	float ReflectionScale		: packoffset( c4.w );
	float4 PerspectiveValues	: packoffset( c5 );
}

float ConvertZToLinearDepth(float depth)
{
	float linearDepth = PerspectiveValues.z / (depth + PerspectiveValues.w);
	return linearDepth;
}

float3 CalcViewPos(float2 csPos, float depth)
{
	float3 position;

	position.xy = csPos.xy * PerspectiveValues.xy * depth;
	position.z = depth;

	return position;
}

// Pixel size in clip-space
// This is resulotion dependent
// Pick the minimum of the HDR width and height
static const float PixelSize = 2.0 / 768.0f;

// Number of sampling steps
// This is resulotion dependent
// Pick the maximum of the HDR width and height
static const int nNumSteps = 1024;

float4 SSReflectionPS( VS_OUTPUT In ) : SV_TARGET
{
	// Pixel position and normal in view space
	float3 vsPos = In.ViewPosition.xyz;
	float3 vsNorm = normalize(In.ViewNormal);

	// Calculate the camera to pixel direction
	float3 eyeToPixel = normalize(vsPos);

	// Calculate the reflected view direction
	float3 vsReflect = reflect(eyeToPixel,  vsNorm);

	// The initial reflection color for the pixel
	float4 reflectColor = float4(0.0, 0.0, 0.0, 0.0);

	// Don't bother with reflected vector above the threshold vector
	if (vsReflect.z >= ViewAngleThreshold)
	{
		// Fade the reflection as the view angles gets close to the threshold
		float viewAngleThresholdInv = 1.0 - ViewAngleThreshold;
		float viewAngleFade = saturate(3.0 * (vsReflect.z - ViewAngleThreshold) / viewAngleThresholdInv);

		// Transform the View Space Reflection to clip-space
		float3 vsPosReflect = vsPos + vsReflect;
		float3 csPosReflect = mul(float4(vsPosReflect, 1.0), ProjMatrix).xyz / vsPosReflect.z;
		float3 csReflect = csPosReflect - In.csPos;

		// Resize Screen Space Reflection to an appropriate length.
		float reflectScale = PixelSize / length(csReflect.xy);
		csReflect *= reflectScale;

		// Calculate the first sampling position in screen-space
		float2 ssSampPos = (In.csPos + csReflect).xy;
		ssSampPos = ssSampPos * float2(0.5, -0.5) + 0.5;

		// Find each iteration step in screen-space
		float2 ssStep = csReflect.xy * float2(0.5, -0.5);

		// Build a plane laying on the reflection vector
		// Use the eye to pixel direction to build the tangent vector
		float4 rayPlane;
		float3 vRight = cross(eyeToPixel, vsReflect);
		rayPlane.xyz = normalize(cross(vsReflect, vRight));
		rayPlane.w = dot(rayPlane.xyz, vsPos);

		// Iterate over the HDR texture searching for intersection
		for (int nCurStep = 0; nCurStep < nNumSteps; nCurStep++)
		{
			// Sample from depth buffer
			float curDepth = DepthTex.SampleLevel(PointSampler, ssSampPos, 0.0).x;

			float curDepthLin = ConvertZToLinearDepth(curDepth);
			float3 curPos = CalcViewPos(In.csPos.xy + csReflect.xy * ((float)nCurStep + 1.0), curDepthLin);

			// Find the intersection between the ray and the scene
			// The intersection happens between two positions on the oposite sides of the plane
			if(rayPlane.w >= dot(rayPlane.xyz, curPos) + DepthBias)
			{
				// Calculate the actual position on the ray for the given depth value
				float3 vsFinalPos = vsPos + (vsReflect / abs(vsReflect.z)) * abs(curDepthLin - vsPos.z + DepthBias);
				float2 csFinalPos = vsFinalPos.xy / PerspectiveValues.xy / vsFinalPos.z;
				ssSampPos = csFinalPos.xy * float2(0.5, -0.5) + 0.5;

				// Get the HDR value at the current screen space location
				reflectColor.xyz = HDRTex.SampleLevel(PointSampler, ssSampPos, 0.0).xyz;

				// Fade out samples as they get close to the texture edges
				float edgeFade = saturate(distance(ssSampPos, float2(0.5, 0.5)) * 2.0 - EdgeDistThreshold);

				// Calculate the fade value
				reflectColor.w = min(viewAngleFade, 1.0 - edgeFade * edgeFade);

				// Apply the reflection sacle
				reflectColor.w *= ReflectionScale;

				// Advance past the final iteration to break the loop
				nCurStep = nNumSteps;
			}

			// Advance to the next sample
			ssSampPos += ssStep;	
		}
	}

	return reflectColor;
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Reflection copy
///////////////////////////////////////////////////////////////////////////////////////////////

static const float2 arrBasePos[4] = {
	float2(1.0, 1.0),
	float2(1.0, -1.0),
	float2(-1.0, 1.0),
	float2(-1.0, -1.0),
};

float4 ReflectionBlendVS( uint VertexID : SV_VertexID ) : SV_Position
{
	// Return the quad position
	return float4( arrBasePos[VertexID].xy, 0.0, 1.0);
}

float4 ReflectionBlendPS(float4 Position : SV_Position) : SV_TARGET
{
	return HDRTex.Load(int3(Position.xy, 0));
}
