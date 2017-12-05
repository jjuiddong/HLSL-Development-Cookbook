struct TBokeh
{
	float2 Pos;
	float Radius;
	float4 Color;
};
StructuredBuffer<TBokeh> Bokeh : register( t0 );
Texture2D<float> HighlightTex : register( t0 );

SamplerState LinearSampler : register( s0 );

struct VS_OUTPUT
{
    float4 Pos : SV_Position;
	float Radius : TEXCOORD0;
	float4 Color : COLOR0;
};

VS_OUTPUT BokehVS( uint VertexID : SV_VertexID )
{
    VS_OUTPUT Output;

	TBokeh bokeh = Bokeh[VertexID];

    Output.Pos = float4( bokeh.Pos, 0.0, 1.0);
    Output.Radius = bokeh.Radius;
	Output.Color = bokeh.Color;
    
    return Output;    
}

//-----------------------------------------------------------------------------------------
// Geometry shader
//-----------------------------------------------------------------------------------------

cbuffer BokehDrawConstants : register( b0 )
{
	float AspectRatio : packoffset( c0 );
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

struct GS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD0;
	float4 Color : COLOR0;
};

[maxvertexcount(6)] // Output two triangles per single input point
void BokehGS(point VS_OUTPUT input[1], inout TriangleStream<GS_OUTPUT> TriStream)
{
	GS_OUTPUT output;

	for(int i=0, idx = 0; i < 2; i++)
	{
		for(int j=0; j < 3; j++, idx++)
		{
			float2 pos = input[0].Pos.xy + arrBasePos[idx] * input[0].Radius * AspectRatio;
			output.Pos = float4(pos, 0.0, 1.0); 
			output.UV = arrUV[idx];
			output.Color = input[0].Color;
			TriStream.Append(output);
		}
	
		TriStream.RestartStrip();
	}
}

//-----------------------------------------------------------------------------------------
// Pixel shader
//-----------------------------------------------------------------------------------------

float4 BokehPS(GS_OUTPUT In) : SV_TARGET
{
	float alpha = HighlightTex.Sample(LinearSampler, In.UV.xy );
	return float4(In.Color.xyz, In.Color.w * alpha);
}