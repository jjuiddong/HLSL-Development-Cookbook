///////////////////////////////////////////////////////////////////
// Shadow map generation
///////////////////////////////////////////////////////////////////
cbuffer cbShadowGenVS : register( b0 )
{
	float4x4 ShadowMat : packoffset( c0 );
}

float4 ShadowGenVS(float4 Pos : POSITION) : SV_Position
{
    return mul(Pos, ShadowMat); 
}

