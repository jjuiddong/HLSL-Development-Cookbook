//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------
cbuffer cbEmissive : register( b2 )
{
    matrix g_mWorldViewProjection : packoffset( c0 );
	matrix g_mWorldView : packoffset( c4 );
	float4 g_color : packoffset( c8 );
}

//--------------------------------------------------------------------------------------
// shader input/output structure
//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 Position	: POSITION; // vertex position
	float3 Normal	: NORMAL;	// vertex normal 
};

struct VS_OUTPUT
{
    float4 Position	: SV_POSITION;	// vertex position
	float Scale		: TEXCOORD0;	// normal based color scale
};


VS_OUTPUT RenderEmissiveVS( VS_INPUT input )
{
    VS_OUTPUT Output;
    
    // Transform the position from object space to homogeneous projection space
    Output.Position = mul( input.Position, g_mWorldViewProjection );
    
	// Transform the normal to view space
	Output.Scale = mul(input.Normal, (float3x3)g_mWorldView).z;
	Output.Scale = saturate(-Output.Scale + 0.5);

    return Output;    
}


float4 RenderEmissivePS( VS_OUTPUT In ) : SV_TARGET0
{ 
	return float4(g_color.rgb * In.Scale, 1.0);
}
