Texture2D<float4> Input : register( t0 );
RWTexture2D<float4> Output : register( u0 );

cbuffer cb0
{
    int2 Res : packoffset( c0 );   // Resulotion of the down scaled image: x - width, y - height
}

static const float SampleWeights[13] = {
    0.002216,
    0.008764,
    0.026995,
    0.064759,
    0.120985,
    0.176033,
    0.199471,
    0.176033,
    0.120985,
    0.064759,
    0.026995,
    0.008764,
    0.002216,
};

#define kernelhalf 6
#define groupthreads 128
groupshared float4 SharedInput[groupthreads];

[numthreads( groupthreads, 1, 1 )]
void VerticalFilter( uint3 Gid : SV_GroupID, uint GI : SV_GroupIndex )
{
    int2 coord = int2( Gid.x, GI - kernelhalf + (groupthreads - kernelhalf * 2) * Gid.y );
    coord = clamp( coord, int2(0, 0), int2(Res.x-1, Res.y-1) );
    SharedInput[GI] = Input.Load( int3(coord, 0) );  

    GroupMemoryBarrierWithGroupSync();

    // Vertical blur
    if ( GI >= kernelhalf && GI < (groupthreads - kernelhalf) && 
         ( (GI - kernelhalf + (groupthreads - kernelhalf * 2) * Gid.y) < Res.y) )
    {
        float4 vOut = 0;
        
        [unroll]
        for ( int i = -kernelhalf; i <= kernelhalf; ++i )
		{
            vOut += SharedInput[GI + i] * SampleWeights[i + kernelhalf];
		}

		Output[coord] = float4(vOut.rgb, 1.0f);
    }
}

[numthreads( groupthreads, 1, 1 )]
void HorizFilter( uint3 Gid : SV_GroupID, uint GI : SV_GroupIndex )
{
    int2 coord = int2( GI - kernelhalf + (groupthreads - kernelhalf * 2) * Gid.x, Gid.y );
    coord = clamp( coord, int2(0, 0), int2(Res.x-1, Res.y-1) );
    SharedInput[GI] = Input.Load( int3(coord, 0) );        

    GroupMemoryBarrierWithGroupSync();

    // Horizontal blur
    if ( GI >= kernelhalf && GI < (groupthreads - kernelhalf) && 
         ( (Gid.x * (groupthreads - 2 * kernelhalf) + GI - kernelhalf) < Res.x) )
    {
        float4 vOut = 0;
        
        [unroll]
        for ( int i = -kernelhalf; i <= kernelhalf; ++i )
            vOut += SharedInput[GI + i] * SampleWeights[i + kernelhalf];

		Output[coord] = float4(vOut.rgb, 1.0f);
    }
}