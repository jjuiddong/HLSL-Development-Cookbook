#include "DXUT.h"
#include "DXUTCamera.h"
#include "SDKmisc.h"
#include "SDKmisc.h"
#include "PostFX.h"

extern ID3D11Device* g_pDevice;
extern CFirstPersonCamera g_Camera;
extern ID3D11SamplerState* g_pSampPoint;
extern ID3D11SamplerState* g_pSampLinear;

HRESULT CompileShader(PWCHAR strPath, D3D10_SHADER_MACRO* pMacros, char* strEntryPoint, char* strProfile, DWORD dwShaderFlags, ID3DBlob** ppVertexShaderBuffer);

CPostFX::CPostFX() : m_fMiddleGrey(0.0025f), m_fWhite(1.5f), m_fBloomThreshold(2.0), m_fBloomScale(0.1f),
	m_pDownScaleRT(NULL), m_pDownScaleSRV(NULL), m_pDownScaleUAV(NULL),
	m_pBloomRT(NULL), m_pBloomSRV(NULL), m_pBloomUAV(NULL),
	m_pBlurredSceneRT(NULL), m_pBlurredSceneSRV(NULL), m_pBlurredSceneUAV(NULL),
	m_pDownScale1DBuffer(NULL), m_pDownScale1DUAV(NULL), m_pDownScale1DSRV(NULL),
	m_pDownScaleCB(NULL), m_pFinalPassCB(NULL), m_pBlurCB(NULL), m_pBokehHightlightScanCB(NULL), m_pBokeRenderCB(NULL),
	m_pAvgLumBuffer(NULL), m_pAvgLumUAV(NULL), m_pAvgLumSRV(NULL),
	m_pPrevAvgLumBuffer(NULL), m_pPrevAvgLumUAV(NULL), m_pPrevAvgLumSRV(NULL),
	m_pBokehBuffer(NULL), m_pBokehUAV(NULL), m_pBokehSRV(NULL), m_pBokehIndirectDrawBuffer(NULL),
	m_pDownScaleFirstPassCS(NULL), m_pDownScaleSecondPassCS(NULL), m_pBloomRevealCS(NULL),
	m_HorizontalBlurCS(NULL), m_VerticalBlurCS(NULL), m_BokehHighlightSearchCS(NULL),
	m_pFullScreenQuadVS(NULL), m_pFinalPassPS(NULL),
	m_pBokehVS(NULL), m_pBokehGS(NULL), m_pBokehPS(NULL),
	m_pBokehTexView(NULL), m_pAddativeBlendState(NULL)
{
	m_pTempRT[0] = NULL;
	m_pTempRT[1] = NULL;
	m_pTempSRV[0] = NULL;
	m_pTempSRV[1] = NULL;
	m_pTempUAV[0] = NULL;
	m_pTempUAV[1] = NULL;
}

CPostFX::~CPostFX()
{

}

HRESULT CPostFX::Init(UINT width, UINT height)
{
	Deinit();

	HRESULT hr;

	// Find the amount of thread groups needed for the downscale operation
	m_nWidth = width;
	m_nHeight = height;
	m_nDownScaleGroups = (UINT)ceil((float)(m_nWidth * m_nHeight / 16) / 1024.0f);

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate the downscaled target
	D3D11_TEXTURE2D_DESC dtd = {
		m_nWidth / 4, //UINT Width;
		m_nHeight / 4, //UINT Height;
		1, //UINT MipLevels;
		1, //UINT ArraySize;
		DXGI_FORMAT_R16G16B16A16_TYPELESS, //DXGI_FORMAT Format;
		1, //DXGI_SAMPLE_DESC SampleDesc;
		0,
		D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
		D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS,//UINT BindFlags;
		0,//UINT CPUAccessFlags;
		0//UINT MiscFlags;    
        };
	V_RETURN( g_pDevice->CreateTexture2D( &dtd, NULL, &m_pDownScaleRT ) );
	DXUT_SetDebugName( m_pDownScaleRT, "PostFX - Down Scaled RT" );

		// Create the resource views
	D3D11_SHADER_RESOURCE_VIEW_DESC dsrvd;
	ZeroMemory(&dsrvd, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
	dsrvd.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	dsrvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	dsrvd.Texture2D.MipLevels = 1;
	V_RETURN( g_pDevice->CreateShaderResourceView( m_pDownScaleRT, &dsrvd, &m_pDownScaleSRV ) );
	DXUT_SetDebugName( m_pDownScaleSRV, "PostFX - Down Scaled SRV" );

	// Create the UAVs
	D3D11_UNORDERED_ACCESS_VIEW_DESC DescUAV;
	ZeroMemory( &DescUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC) );
	DescUAV.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	DescUAV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	DescUAV.Buffer.FirstElement = 0;
	DescUAV.Buffer.NumElements = m_nWidth * m_nHeight / 16;
	V_RETURN( g_pDevice->CreateUnorderedAccessView( m_pDownScaleRT, &DescUAV, &m_pDownScaleUAV ) );
	DXUT_SetDebugName( m_pDownScaleUAV, "PostFX - Down Scaled UAV" );

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate temporary target
	V_RETURN( g_pDevice->CreateTexture2D( &dtd, NULL, &m_pTempRT[0] ) );
	DXUT_SetDebugName( m_pTempRT[0], "PostFX - Temp 0 RT" );

	V_RETURN( g_pDevice->CreateShaderResourceView( m_pTempRT[0], &dsrvd, &m_pTempSRV[0] ) );
	DXUT_SetDebugName( m_pTempSRV[0], "PostFX - Temp 0 SRV" );

	V_RETURN( g_pDevice->CreateUnorderedAccessView( m_pTempRT[0], &DescUAV, &m_pTempUAV[0] ) );
	DXUT_SetDebugName( m_pTempUAV[0], "PostFX - Temp 0 UAV" );

	V_RETURN( g_pDevice->CreateTexture2D( &dtd, NULL, &m_pTempRT[1] ) );
	DXUT_SetDebugName( m_pTempRT[1], "PostFX - Temp 1 RT" );

	V_RETURN( g_pDevice->CreateShaderResourceView( m_pTempRT[1], &dsrvd, &m_pTempSRV[1] ) );
	DXUT_SetDebugName( m_pTempSRV[1], "PostFX - Temp 1 SRV" );

	V_RETURN( g_pDevice->CreateUnorderedAccessView( m_pTempRT[1], &DescUAV, &m_pTempUAV[1] ) );
	DXUT_SetDebugName( m_pTempUAV[1], "PostFX - Temp 1 UAV" );

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate bloom target
	V_RETURN( g_pDevice->CreateTexture2D( &dtd, NULL, &m_pBloomRT ) );
	DXUT_SetDebugName( m_pBloomRT, "PostFX - Bloom RT" );

	V_RETURN( g_pDevice->CreateShaderResourceView( m_pBloomRT, &dsrvd, &m_pBloomSRV ) );
	DXUT_SetDebugName( m_pBloomSRV, "PostFX - Bloom SRV" );

	V_RETURN( g_pDevice->CreateUnorderedAccessView( m_pBloomRT, &DescUAV, &m_pBloomUAV ) );
	DXUT_SetDebugName( m_pBloomUAV, "PostFX - Bloom UAV" );

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate blurred scene target
	V_RETURN( g_pDevice->CreateTexture2D( &dtd, NULL, &m_pBlurredSceneRT ) );
	DXUT_SetDebugName( m_pBlurredSceneRT, "PostFX - Blurred Scene RT" );

	V_RETURN( g_pDevice->CreateShaderResourceView( m_pBlurredSceneRT, &dsrvd, &m_pBlurredSceneSRV ) );
	DXUT_SetDebugName( m_pBlurredSceneSRV, "PostFX - Blurred Scene SRV" );

	V_RETURN( g_pDevice->CreateUnorderedAccessView( m_pBlurredSceneRT, &DescUAV, &m_pBlurredSceneUAV ) );
	DXUT_SetDebugName( m_pBlurredSceneUAV, "PostFX - Blurred Scene UAV" );
	
	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate down scaled luminance buffer
	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory( &bufferDesc, sizeof(bufferDesc) );
	bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.StructureByteStride = sizeof(float);
	bufferDesc.ByteWidth = m_nDownScaleGroups * bufferDesc.StructureByteStride;
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	V_RETURN( g_pDevice->CreateBuffer( &bufferDesc, NULL, &m_pDownScale1DBuffer ) );
	DXUT_SetDebugName( m_pDownScale1DBuffer, "PostFX - Luminance Down Scale 1D Buffer" );

	ZeroMemory( &DescUAV, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC) );
	DescUAV.Format = DXGI_FORMAT_UNKNOWN;
	DescUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	DescUAV.Buffer.FirstElement = 0;
	DescUAV.Buffer.NumElements = m_nDownScaleGroups;
	V_RETURN( g_pDevice->CreateUnorderedAccessView( m_pDownScale1DBuffer, &DescUAV, &m_pDownScale1DUAV ) );
	DXUT_SetDebugName( m_pDownScale1DUAV, "PostFX - Luminance Down Scale 1D UAV" );

	dsrvd.Format = DXGI_FORMAT_UNKNOWN;
	dsrvd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	dsrvd.Buffer.FirstElement = 0;
	dsrvd.Buffer.NumElements = m_nDownScaleGroups;
	V_RETURN( g_pDevice->CreateShaderResourceView( m_pDownScale1DBuffer, &dsrvd, &m_pDownScale1DSRV ) );
	DXUT_SetDebugName( m_pDownScale1DSRV, "PostFX - Luminance Down Scale 1D SRV" );

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate average luminance buffer
	bufferDesc.ByteWidth = sizeof(float);
	V_RETURN( g_pDevice->CreateBuffer( &bufferDesc, NULL, &m_pAvgLumBuffer ) );
	DXUT_SetDebugName( m_pAvgLumBuffer, "PostFX - Average Luminance Buffer" );
	
	DescUAV.Buffer.NumElements = 1;
	V_RETURN( g_pDevice->CreateUnorderedAccessView( m_pAvgLumBuffer, &DescUAV, &m_pAvgLumUAV ) );
	DXUT_SetDebugName( m_pAvgLumUAV, "PostFX - Average Luminance UAV" );

	dsrvd.Buffer.NumElements = 1;
	V_RETURN( g_pDevice->CreateShaderResourceView( m_pAvgLumBuffer, &dsrvd, &m_pAvgLumSRV ) );
	DXUT_SetDebugName( m_pAvgLumSRV, "PostFX - Average Luminance SRV" );

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate previous frame average luminance buffer
	V_RETURN( g_pDevice->CreateBuffer( &bufferDesc, NULL, &m_pPrevAvgLumBuffer ) );
	DXUT_SetDebugName( m_pPrevAvgLumBuffer, "PostFX - Previous Average Luminance Buffer" );

	V_RETURN( g_pDevice->CreateUnorderedAccessView( m_pPrevAvgLumBuffer, &DescUAV, &m_pPrevAvgLumUAV ) );
	DXUT_SetDebugName( m_pPrevAvgLumUAV, "PostFX - Previous Average Luminance UAV" );

	V_RETURN( g_pDevice->CreateShaderResourceView( m_pPrevAvgLumBuffer, &dsrvd, &m_pPrevAvgLumSRV ) );
	DXUT_SetDebugName( m_pPrevAvgLumSRV, "PostFX - Previous Average Luminance SRV" );

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate bokeh buffer
	const UINT nMaxBokehInst = 4056; 
	bufferDesc.StructureByteStride = 7 * sizeof(float);
	bufferDesc.ByteWidth = nMaxBokehInst * bufferDesc.StructureByteStride;
	//bufferDesc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
	V_RETURN( g_pDevice->CreateBuffer( &bufferDesc, NULL, &m_pBokehBuffer ) );
	DXUT_SetDebugName( m_pBokehBuffer, "PostFX - Bokeh Buffer" );

	DescUAV.Buffer.NumElements = nMaxBokehInst;
	DescUAV.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;
	V_RETURN( g_pDevice->CreateUnorderedAccessView( m_pBokehBuffer, &DescUAV, &m_pBokehUAV ) );
	DXUT_SetDebugName( m_pBokehUAV, "PostFX - Bokeh UAV" );

	dsrvd.Buffer.NumElements = nMaxBokehInst;
	V_RETURN( g_pDevice->CreateShaderResourceView( m_pBokehBuffer, &dsrvd, &m_pBokehSRV ) );
	DXUT_SetDebugName( m_pBokehSRV, "PostFX - Bokeh SRV" );

	ZeroMemory( &bufferDesc, sizeof(bufferDesc) );
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
	bufferDesc.ByteWidth = 16;

	D3D11_SUBRESOURCE_DATA initData;
	UINT bufferInit[4] = { 0, 1, 0, 0 };
	initData.pSysMem = bufferInit;
	initData.SysMemPitch = 0;
	initData.SysMemSlicePitch = 0;

	V_RETURN( g_pDevice->CreateBuffer( &bufferDesc, &initData, &m_pBokehIndirectDrawBuffer ) );
	DXUT_SetDebugName( m_pBokehIndirectDrawBuffer, "PostFX - Bokeh Indirect Draw Buffer" );

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate constant buffers
	ZeroMemory( &bufferDesc, sizeof(bufferDesc) );
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDesc.ByteWidth = sizeof(TDownScaleCB);
	V_RETURN( g_pDevice->CreateBuffer( &bufferDesc, NULL, &m_pDownScaleCB ) );
	DXUT_SetDebugName( m_pDownScaleCB, "PostFX - Down Scale CB" );

	bufferDesc.ByteWidth = sizeof(TFinalPassCB);
	V_RETURN( g_pDevice->CreateBuffer( &bufferDesc, NULL, &m_pFinalPassCB ) );
	DXUT_SetDebugName( m_pFinalPassCB, "PostFX - Final Pass CB" );

	bufferDesc.ByteWidth = sizeof(TBlurCB);
	V_RETURN( g_pDevice->CreateBuffer( &bufferDesc, NULL, &m_pBlurCB ) );
	DXUT_SetDebugName( m_pBlurCB, "PostFX - Blur CB" );

	bufferDesc.ByteWidth = sizeof(TBokehHightlightScanCB);
	V_RETURN( g_pDevice->CreateBuffer( &bufferDesc, NULL, &m_pBokehHightlightScanCB ) );
	DXUT_SetDebugName( m_pBokehHightlightScanCB, "PostFX - Bokeh Hightlight Scan CB" );
	
	bufferDesc.ByteWidth = sizeof(TBokehRenderCB);
	V_RETURN( g_pDevice->CreateBuffer( &bufferDesc, NULL, &m_pBokeRenderCB ) );
	DXUT_SetDebugName( m_pBokeRenderCB, "PostFX - Bokeh Render CB" );
	
	// Compile the shaders
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;// | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	WCHAR str[MAX_PATH];
	ID3DBlob* pShaderBlob = NULL;
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PostDownScaleFX.hlsl" ) );
    V_RETURN( CompileShader(str, NULL, "DownScaleFirstPass", "cs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateComputeShader( pShaderBlob->GetBufferPointer(),
												pShaderBlob->GetBufferSize(), NULL, &m_pDownScaleFirstPassCS ) );
	DXUT_SetDebugName( m_pDownScaleFirstPassCS, "Post FX - Down Scale First Pass CS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "DownScaleSecondPass", "cs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateComputeShader( pShaderBlob->GetBufferPointer(),
												pShaderBlob->GetBufferSize(), NULL, &m_pDownScaleSecondPassCS ) );
	DXUT_SetDebugName( m_pDownScaleSecondPassCS, "Post FX - Down Scale Second Pass CS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "BloomReveal", "cs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateComputeShader( pShaderBlob->GetBufferPointer(),
												pShaderBlob->GetBufferSize(), NULL, &m_pBloomRevealCS ) );
	DXUT_SetDebugName( m_pBloomRevealCS, "Post FX - Bloom Reveal CS" );
	SAFE_RELEASE( pShaderBlob );


	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Gaussian Blur
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Blur.hlsl" ) );

	V_RETURN( CompileShader(str, NULL, "VerticalFilter", "cs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateComputeShader( pShaderBlob->GetBufferPointer(),
												pShaderBlob->GetBufferSize(), NULL, &m_VerticalBlurCS ) );
	DXUT_SetDebugName( m_VerticalBlurCS, "Post FX - Vertical Blur CS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "HorizFilter", "cs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateComputeShader( pShaderBlob->GetBufferPointer(),
												pShaderBlob->GetBufferSize(), NULL, &m_HorizontalBlurCS ) );
	DXUT_SetDebugName( m_HorizontalBlurCS, "Post FX - Horizontal Blur CS" );
	SAFE_RELEASE( pShaderBlob );

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Bokeh
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"BokehCS.hlsl" ) );

	V_RETURN( CompileShader(str, NULL, "HighlightScan", "cs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateComputeShader( pShaderBlob->GetBufferPointer(),
												pShaderBlob->GetBufferSize(), NULL, &m_BokehHighlightSearchCS ) );
	DXUT_SetDebugName( m_BokehHighlightSearchCS, "Post FX - Bokeh Highlight Scan CS" );
	SAFE_RELEASE( pShaderBlob );

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Final pass

	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"PostFX.hlsl" ) );
    V_RETURN( CompileShader(str, NULL, "FullScreenQuadVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
                                              pShaderBlob->GetBufferSize(), NULL, &m_pFullScreenQuadVS ) );
    DXUT_SetDebugName( m_pFullScreenQuadVS, "Post FX - Full Screen Quad VS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "FinalPassPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
    V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
                                             pShaderBlob->GetBufferSize(), NULL, &m_pFinalPassPS ) );
    DXUT_SetDebugName( m_pFinalPassPS, "Post FX - Final Pass PS" );
	SAFE_RELEASE( pShaderBlob );

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Bokeh render

	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Bokeh.hlsl" ) );
	V_RETURN( CompileShader(str, NULL, "BokehVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pBokehVS ) );
	DXUT_SetDebugName( m_pBokehVS, "Post FX - Bokeh VS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "BokehGS", "gs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateGeometryShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pBokehGS ) );
	DXUT_SetDebugName( m_pBokehGS, "Post FX - Bokeh GS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "BokehPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pBokehPS ) );
	DXUT_SetDebugName( m_pBokehPS, "Post FX - Bokeh PS" );
	SAFE_RELEASE( pShaderBlob );

	// Load the bokeh highlight texture
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"..\\Media\\Bokeh.dds" ) );
	V_RETURN( D3DX11CreateShaderResourceViewFromFile( g_pDevice, str, NULL, NULL, &m_pBokehTexView, NULL ) );
	DXUT_SetDebugName( m_pBokehPS, "Post FX - Bokeh.dds" );

	// Create the blend state for the bokeh highlights
	D3D11_BLEND_DESC descBlend;
	descBlend.AlphaToCoverageEnable = FALSE;
	descBlend.IndependentBlendEnable = FALSE;
	const D3D11_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
	{
		TRUE,
		D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
		D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
		D3D11_COLOR_WRITE_ENABLE_ALL,
	};
	for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		descBlend.RenderTarget[ i ] = defaultRenderTargetBlendDesc;
	V_RETURN( g_pDevice->CreateBlendState(&descBlend, &m_pAddativeBlendState) );
	DXUT_SetDebugName( m_pAddativeBlendState, "Post FX - Addative Blending BS" );

	return S_OK;
}

void CPostFX::Deinit()
{
	SAFE_RELEASE( m_pDownScaleRT );
	SAFE_RELEASE( m_pDownScaleSRV );
	SAFE_RELEASE( m_pDownScaleUAV );
	SAFE_RELEASE( m_pTempRT[0] );
	SAFE_RELEASE( m_pTempSRV[0] );
	SAFE_RELEASE( m_pTempUAV[0] );
	SAFE_RELEASE( m_pTempRT[1] );
	SAFE_RELEASE( m_pTempSRV[1] );
	SAFE_RELEASE( m_pTempUAV[1] );
	SAFE_RELEASE( m_pBloomRT );
	SAFE_RELEASE( m_pBloomSRV );
	SAFE_RELEASE( m_pBloomUAV );
	SAFE_RELEASE( m_pBlurredSceneRT );
	SAFE_RELEASE( m_pBlurredSceneSRV );
	SAFE_RELEASE( m_pBlurredSceneUAV );
	SAFE_RELEASE( m_pDownScale1DBuffer );
	SAFE_RELEASE( m_pDownScale1DUAV );
	SAFE_RELEASE( m_pDownScale1DSRV );
	SAFE_RELEASE( m_pDownScaleCB );
	SAFE_RELEASE( m_pFinalPassCB );
	SAFE_RELEASE( m_pBlurCB );
	SAFE_RELEASE( m_pBokehHightlightScanCB );
	SAFE_RELEASE( m_pBokeRenderCB );
	SAFE_RELEASE( m_pAvgLumBuffer );
	SAFE_RELEASE( m_pAvgLumUAV );
	SAFE_RELEASE( m_pAvgLumSRV );
	SAFE_RELEASE( m_pPrevAvgLumBuffer );
	SAFE_RELEASE( m_pPrevAvgLumUAV );
	SAFE_RELEASE( m_pPrevAvgLumSRV );
	SAFE_RELEASE( m_pBokehBuffer );
	SAFE_RELEASE( m_pBokehUAV );
	SAFE_RELEASE( m_pBokehSRV );
	SAFE_RELEASE( m_pBokehIndirectDrawBuffer );
	SAFE_RELEASE( m_pDownScaleFirstPassCS );
	SAFE_RELEASE( m_pDownScaleSecondPassCS );
	SAFE_RELEASE( m_pBloomRevealCS );
	SAFE_RELEASE( m_HorizontalBlurCS );
	SAFE_RELEASE( m_VerticalBlurCS );
	SAFE_RELEASE( m_BokehHighlightSearchCS );
	SAFE_RELEASE( m_pFullScreenQuadVS );
	SAFE_RELEASE( m_pFinalPassPS );
	SAFE_RELEASE( m_pBokehVS );
	SAFE_RELEASE( m_pBokehGS );
	SAFE_RELEASE( m_pBokehPS );
	SAFE_RELEASE( m_pBokehTexView );
	SAFE_RELEASE( m_pAddativeBlendState );
}

void CPostFX::PostProcessing(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11ShaderResourceView* pDepthSRV, ID3D11RenderTargetView* pLDRRTV)
{
	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(m_pDownScaleCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	TDownScaleCB* pDownScale = (TDownScaleCB*)MappedResource.pData; 
	pDownScale->nWidth = m_nWidth  / 4;
	pDownScale->nHeight = m_nHeight / 4;
	pDownScale->nTotalPixels = pDownScale->nWidth * pDownScale->nHeight;
	pDownScale->nGroupSize = m_nDownScaleGroups;
	pDownScale->fAdaptation = m_fAdaptation;
	pDownScale->fBloomThreshold = m_fBloomThreshold;
	float fQ = g_Camera.GetFarClip() / (g_Camera.GetFarClip() - g_Camera.GetNearClip());
	pDownScale->ProjectionValues[0] = -g_Camera.GetNearClip() * fQ;
	pDownScale->ProjectionValues[1] = -fQ;
	pd3dImmediateContext->Unmap(m_pDownScaleCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { m_pDownScaleCB };
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

	// Down scale the HDR image
	ID3D11RenderTargetView* rt[1] = { NULL };
	pd3dImmediateContext->OMSetRenderTargets( 1, rt, NULL );
	DownScale(pd3dImmediateContext, pHDRSRV);

	// Bloom
	Bloom(pd3dImmediateContext);

	// Cleanup for the shared constant buffer
	ZeroMemory( &arrConstBuffers, sizeof(arrConstBuffers) );
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

	// Scan for the bokeh highlights
	BokehHightlightScan(pd3dImmediateContext, pHDRSRV, pDepthSRV);

	// Do the final pass
	rt[0] = pLDRRTV;
	pd3dImmediateContext->OMSetRenderTargets( 1, rt, NULL );
	FinalPass(pd3dImmediateContext, pHDRSRV, pDepthSRV);

	// Draw the bokeh highlights on top of the image
	BokehRender(pd3dImmediateContext);

	// Swap the previous frame average luminance
	ID3D11Buffer* pTempBuffer = m_pAvgLumBuffer;
	ID3D11UnorderedAccessView* pTempUAV = m_pAvgLumUAV;
	ID3D11ShaderResourceView* p_TempSRV = m_pAvgLumSRV;
	m_pAvgLumBuffer = m_pPrevAvgLumBuffer;
	m_pAvgLumUAV = m_pPrevAvgLumUAV;
	m_pAvgLumSRV = m_pPrevAvgLumSRV;
	m_pPrevAvgLumBuffer = pTempBuffer;
	m_pPrevAvgLumUAV = pTempUAV;
	m_pPrevAvgLumSRV = p_TempSRV;
}

void CPostFX::SetParameters(float fMiddleGrey, float fWhite, float fAdaptation, float fBloomThreshold, float fBloomScale, float fDOFFarStart, float fDOFFarRange,
	float fBokehLumThreshold, float fBokehBlurThreshold, float fBokehRadiusScale, float fBokehColorScale)
{
	m_fMiddleGrey = fMiddleGrey;
	m_fWhite = fWhite;
	m_fAdaptation = fAdaptation;
	m_fBloomThreshold = fBloomThreshold;
	m_fBloomScale = fBloomScale;
	
	// Far DOF can always be on because we can just set the far value to the far plane
	m_fDOFFarStart = fDOFFarStart;
	m_fDOFFarRangeRcp = 1.0f / max(fDOFFarRange, 0.001f);

	m_fBokehLumThreshold = fBokehLumThreshold;
	m_fBokehBlurThreshold = fBokehBlurThreshold;
	m_fBokehRadiusScale = fBokehRadiusScale;
	m_fBokehColorScale = fBokehColorScale;
}

void CPostFX::DownScale(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV)
{
	// Output
	ID3D11UnorderedAccessView* arrUAVs[2] = { m_pDownScale1DUAV, m_pDownScaleUAV };
	pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 2, arrUAVs, NULL);

	// Input
	ID3D11ShaderResourceView* arrViews[2] = { pHDRSRV, NULL };
	pd3dImmediateContext->CSSetShaderResources(0, 2, arrViews);

	// Shader
	pd3dImmediateContext->CSSetShader( m_pDownScaleFirstPassCS, NULL, 0 );

	// Execute the downscales first pass with enough groups to cover the entire full res HDR buffer
	pd3dImmediateContext->Dispatch(m_nDownScaleGroups, 1, 1);

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Second pass - reduce to a single pixel

	// Outoput
	ZeroMemory(arrUAVs, sizeof(arrUAVs));
	arrUAVs[0] = m_pAvgLumUAV;
	pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 2, arrUAVs, NULL);

	// Input
	arrViews[0] = m_pDownScale1DSRV;
	arrViews[1] = m_pPrevAvgLumSRV;
	pd3dImmediateContext->CSSetShaderResources(0, 2, arrViews);

	// Shader
	pd3dImmediateContext->CSSetShader( m_pDownScaleSecondPassCS, NULL, 0 );

	// Excute with a single group - this group has enough threads to process all the pixels
	pd3dImmediateContext->Dispatch(1, 1, 1);

	// Cleanup
	pd3dImmediateContext->CSSetShader( NULL, NULL, 0 );
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->CSSetShaderResources(0, 2, arrViews);
	ZeroMemory(arrUAVs, sizeof(arrUAVs));
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 2, arrUAVs, NULL);
}

void CPostFX::Bloom(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Input
	ID3D11ShaderResourceView* arrViews[2] = { m_pDownScaleSRV, m_pAvgLumSRV };
	pd3dImmediateContext->CSSetShaderResources(0, 2, arrViews);

	// Output
	ID3D11UnorderedAccessView* arrUAVs[1] = { m_pTempUAV[0] };
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);

	// Shader
	pd3dImmediateContext->CSSetShader( m_pBloomRevealCS, NULL, 0 );

	// Execute the downscales first pass with enough groups to cover the entire full res HDR buffer
	pd3dImmediateContext->Dispatch(m_nDownScaleGroups, 1, 1);

	// Cleanup
	pd3dImmediateContext->CSSetShader( NULL, NULL, 0 );
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->CSSetShaderResources(0, 2, arrViews);
	ZeroMemory(arrUAVs, sizeof(arrUAVs));
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);

	// Blur the bloom values
	Blur(pd3dImmediateContext, m_pTempSRV[0], m_pBloomUAV);
}

void CPostFX::Blur(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pInput, ID3D11UnorderedAccessView* pOutput)
{
	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Second pass - horizontal Gaussian filter

	// Output
	ID3D11UnorderedAccessView* arrUAVs[1] = { m_pTempUAV[1] };
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);

	// Input
	ID3D11ShaderResourceView* arrViews[1] = { pInput };
	pd3dImmediateContext->CSSetShaderResources(0, 1, arrViews);

	// Shader
	pd3dImmediateContext->CSSetShader(m_HorizontalBlurCS, NULL, 0);

	// Execute the horizontal filter
	pd3dImmediateContext->Dispatch((UINT)ceil((m_nWidth / 4.0f) / (128.0f - 12.0f)), (UINT)ceil(m_nHeight / 4.0f), 1);

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// First pass - vertical Gaussian filter

	// Output
	arrUAVs[0] = pOutput;
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);

	// Input
	arrViews[0] = m_pTempSRV[1];
	pd3dImmediateContext->CSSetShaderResources(0, 1, arrViews);

	// Shader
	pd3dImmediateContext->CSSetShader(m_VerticalBlurCS, NULL, 0);

	// Execute the vertical filter
	pd3dImmediateContext->Dispatch((UINT)ceil(m_nWidth / 4.0f), (UINT)ceil((m_nHeight / 4.0f) / (128.0f - 12.0f)), 1);

	// Cleanup
	pd3dImmediateContext->CSSetShader(NULL, NULL, 0);
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->CSSetShaderResources(0, 1, arrViews);
	ZeroMemory(arrUAVs, sizeof(arrUAVs));
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);
}

void CPostFX::BokehHightlightScan(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11ShaderResourceView* pDepthSRV)
{
	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(m_pBokehHightlightScanCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	TBokehHightlightScanCB* pBokehHightlightScan = (TBokehHightlightScanCB*)MappedResource.pData; 
	pBokehHightlightScan->nWidth = m_nWidth;
	pBokehHightlightScan->nHeight = m_nHeight;
	float fQ = g_Camera.GetFarClip() / (g_Camera.GetFarClip() - g_Camera.GetNearClip());
	pBokehHightlightScan->ProjectionValues[0] = -g_Camera.GetNearClip() * fQ;
	pBokehHightlightScan->ProjectionValues[1] = -fQ;
	pBokehHightlightScan->fDOFFarStart = m_fDOFFarStart;
	pBokehHightlightScan->fDOFFarRangeRcp = m_fDOFFarRangeRcp;
	pBokehHightlightScan->fMiddleGrey = m_fMiddleGrey;
	pBokehHightlightScan->fLumWhiteSqr = m_fWhite;
	pBokehHightlightScan->fLumWhiteSqr *= pBokehHightlightScan->fMiddleGrey; // Scale by the middle gray value
	pBokehHightlightScan->fLumWhiteSqr *= pBokehHightlightScan->fLumWhiteSqr; // Square
	pBokehHightlightScan->fBokehBlurThreshold = m_fBokehBlurThreshold;
	pBokehHightlightScan->fBokehLumThreshold = m_fBokehLumThreshold;
	pBokehHightlightScan->fRadiusScale = m_fBokehRadiusScale;
	pBokehHightlightScan->fColorScale = m_fBokehColorScale;
	pd3dImmediateContext->Unmap(m_pBokehHightlightScanCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { m_pBokehHightlightScanCB };
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

	// Output
	ID3D11UnorderedAccessView* arrUAVs[1] = { m_pBokehUAV };
	UINT nCount = 0; // Indicate we want to flush everything already in the buffer
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, &nCount);

	// Input
	ID3D11ShaderResourceView* arrViews[3] = { pHDRSRV, pDepthSRV, m_pAvgLumSRV  };
	pd3dImmediateContext->CSSetShaderResources(0, 3, arrViews);

	// Shader
	pd3dImmediateContext->CSSetShader(m_BokehHighlightSearchCS, NULL, 0);

	// Execute the horizontal filter
	pd3dImmediateContext->Dispatch((UINT)ceil((float)(m_nWidth * m_nHeight) / 1024.0f), 1, 1);

	// Cleanup
	pd3dImmediateContext->CSSetShader(NULL, NULL, 0);
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->CSSetShaderResources(0, 3, arrViews);
	ZeroMemory(arrUAVs, sizeof(arrUAVs));
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);
	ZeroMemory( &arrConstBuffers, sizeof(arrConstBuffers) );
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);
}

void CPostFX::FinalPass(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11ShaderResourceView* pDepthSRV)
{
	ID3D11ShaderResourceView* arrViews[6] = { pHDRSRV, m_pAvgLumSRV, m_pBloomSRV, m_pDownScaleSRV, pDepthSRV };
	pd3dImmediateContext->PSSetShaderResources(0, 6, arrViews);

	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(m_pFinalPassCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	TFinalPassCB* pFinalPass = (TFinalPassCB*)MappedResource.pData; 
	pFinalPass->fMiddleGrey = m_fMiddleGrey;
	pFinalPass->fLumWhiteSqr = m_fWhite;
	pFinalPass->fLumWhiteSqr *= pFinalPass->fMiddleGrey; // Scale by the middle gray value
	pFinalPass->fLumWhiteSqr *= pFinalPass->fLumWhiteSqr; // Square
	pFinalPass->fBloomScale = m_fBloomScale;
	float fQ = g_Camera.GetFarClip() / (g_Camera.GetFarClip() - g_Camera.GetNearClip());
	pFinalPass->ProjectionValues[0] = -g_Camera.GetNearClip() * fQ;
	pFinalPass->ProjectionValues[1] = -fQ;
	pFinalPass->fDOFFarStart = m_fDOFFarStart;
	pFinalPass->fDOFFarRangeRcp = m_fDOFFarRangeRcp;
	pd3dImmediateContext->Unmap(m_pFinalPassCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { m_pFinalPassCB };
	pd3dImmediateContext->PSSetConstantBuffers(0, 1, arrConstBuffers);

	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetIndexBuffer(NULL, DXGI_FORMAT_UNKNOWN, 0);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

	ID3D11SamplerState* arrSamplers[2] = { g_pSampPoint, g_pSampLinear };
	pd3dImmediateContext->PSSetSamplers( 0, 2, arrSamplers );
	
	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pFullScreenQuadVS, NULL, 0);
	pd3dImmediateContext->PSSetShader(m_pFinalPassPS, NULL, 0);

	pd3dImmediateContext->Draw(4, 0);

	// Cleanup
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->PSSetShaderResources(0, 6, arrViews);
	ZeroMemory(arrConstBuffers, sizeof(arrConstBuffers));
	pd3dImmediateContext->PSSetConstantBuffers(0, 1, arrConstBuffers);
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
}

void CPostFX::BokehRender(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Copy the amount of appended highlights so we can render them
	pd3dImmediateContext->CopyStructureCount(m_pBokehIndirectDrawBuffer, 0, m_pBokehUAV);

	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(m_pBokeRenderCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	TBokehRenderCB* pBokehRender = (TBokehRenderCB*)MappedResource.pData;
	pBokehRender->AspectRatio[0] = 1.0f;
	pBokehRender->AspectRatio[1] = (float)m_nWidth / (float)m_nHeight;
	pd3dImmediateContext->Unmap(m_pBokeRenderCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { m_pBokeRenderCB };
	pd3dImmediateContext->GSSetConstantBuffers(0, 1, arrConstBuffers);

	ID3D11BlendState* pPrevBlendState;
	FLOAT prevBlendFactor[ 4 ];
	UINT prevSampleMask;
	pd3dImmediateContext->OMGetBlendState(&pPrevBlendState, prevBlendFactor, &prevSampleMask);
	pd3dImmediateContext->OMSetBlendState(m_pAddativeBlendState, prevBlendFactor, prevSampleMask);

	ID3D11ShaderResourceView* arrViews[1] = { m_pBokehSRV };
	pd3dImmediateContext->VSSetShaderResources(0, 1, arrViews);

	arrViews[0] = m_pBokehTexView;
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);

	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetIndexBuffer(NULL, DXGI_FORMAT_UNKNOWN, 0);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_POINTLIST );

	ID3D11SamplerState* arrSamplers[1] = { g_pSampLinear };
	pd3dImmediateContext->PSSetSamplers( 0, 1, arrSamplers );

	pd3dImmediateContext->VSSetShader(m_pBokehVS, NULL, 0);
	pd3dImmediateContext->GSSetShader(m_pBokehGS, NULL, 0);
	pd3dImmediateContext->PSSetShader(m_pBokehPS, NULL, 0);

	pd3dImmediateContext->DrawInstancedIndirect(m_pBokehIndirectDrawBuffer, 0);

	// Cleanup
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->VSSetShaderResources(0, 1, arrViews);
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);
	pd3dImmediateContext->OMSetBlendState(pPrevBlendState, prevBlendFactor, prevSampleMask);
	arrConstBuffers[0] = NULL;
	pd3dImmediateContext->GSSetConstantBuffers(0, 1, arrConstBuffers);
}
