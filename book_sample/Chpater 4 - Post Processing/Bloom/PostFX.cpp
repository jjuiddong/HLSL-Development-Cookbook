#include "DXUT.h"
#include "SDKmisc.h"
#include "SDKmisc.h"
#include "PostFX.h"

extern ID3D11Device* g_pDevice;

extern ID3D11SamplerState* g_pSampPoint;
extern ID3D11SamplerState* g_pSampLinear;

HRESULT CompileShader(PWCHAR strPath, D3D10_SHADER_MACRO* pMacros, char* strEntryPoint, char* strProfile, DWORD dwShaderFlags, ID3DBlob** ppVertexShaderBuffer);

CPostFX::CPostFX() : m_fMiddleGrey(0.0025f), m_fWhite(1.5f), m_fBloomThreshold(2.0), m_fBloomScale(0.1f),
	m_pDownScaleRT(NULL), m_pDownScaleSRV(NULL), m_pDownScaleUAV(NULL),
	m_pBloomRT(NULL), m_pBloomSRV(NULL), m_pBloomUAV(NULL),
	m_pDownScale1DBuffer(NULL), m_pDownScale1DUAV(NULL), m_pDownScale1DSRV(NULL),
	m_pDownScaleCB(NULL), m_pFinalPassCB(NULL), m_pBlurCB(NULL),
	m_pAvgLumBuffer(NULL), m_pAvgLumUAV(NULL), m_pAvgLumSRV(NULL),
	m_pPrevAvgLumBuffer(NULL), m_pPrevAvgLumUAV(NULL), m_pPrevAvgLumSRV(NULL),
	m_pDownScaleFirstPassCS(NULL), m_pDownScaleSecondPassCS(NULL), m_pBloomRevealCS(NULL),
	m_HorizontalBlurCS(NULL), m_VerticalBlurCS(NULL), m_pFullScreenQuadVS(NULL), m_pFinalPassPS(NULL)
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
	// NVidia Gaussian Blur
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
	SAFE_RELEASE( m_pDownScale1DBuffer );
	SAFE_RELEASE( m_pDownScale1DUAV );
	SAFE_RELEASE( m_pDownScale1DSRV );
	SAFE_RELEASE( m_pDownScaleCB );
	SAFE_RELEASE( m_pFinalPassCB );
	SAFE_RELEASE( m_pBlurCB );
	SAFE_RELEASE( m_pAvgLumBuffer );
	SAFE_RELEASE( m_pAvgLumUAV );
	SAFE_RELEASE( m_pAvgLumSRV );
	SAFE_RELEASE( m_pPrevAvgLumBuffer );
	SAFE_RELEASE( m_pPrevAvgLumUAV );
	SAFE_RELEASE( m_pPrevAvgLumSRV );
	SAFE_RELEASE( m_pDownScaleFirstPassCS );
	SAFE_RELEASE( m_pDownScaleSecondPassCS );
	SAFE_RELEASE( m_pBloomRevealCS );
	SAFE_RELEASE( m_HorizontalBlurCS );
	SAFE_RELEASE( m_VerticalBlurCS );
	SAFE_RELEASE( m_pFullScreenQuadVS );
	SAFE_RELEASE( m_pFinalPassPS );
}

void CPostFX::PostProcessing(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV, ID3D11RenderTargetView* pLDRRTV)
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
	pd3dImmediateContext->Unmap(m_pDownScaleCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { m_pDownScaleCB };
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

	// Down scale the HDR image
	ID3D11RenderTargetView* rt[1] = { NULL };
	pd3dImmediateContext->OMSetRenderTargets( 1, rt, NULL );
	DownScale(pd3dImmediateContext, pHDRSRV);

	// Bloom
	Bloom(pd3dImmediateContext);

	// Blur the bloom values
	Blur(pd3dImmediateContext, m_pTempSRV[0], m_pBloomUAV);

	// Cleanup
	ZeroMemory( &arrConstBuffers, sizeof(arrConstBuffers) );
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

	// Do the final pass
	rt[0] = pLDRRTV;
	pd3dImmediateContext->OMSetRenderTargets( 1, rt, NULL );
	FinalPass(pd3dImmediateContext, pHDRSRV);

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

void CPostFX::DownScale(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV)
{
	// Output
	ID3D11UnorderedAccessView* arrUAVs[2] = { m_pDownScale1DUAV, m_pDownScaleUAV };
	pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 2, arrUAVs, NULL );

	// Input
	ID3D11ShaderResourceView* arrViews[2] = { pHDRSRV, NULL };
	pd3dImmediateContext->CSSetShaderResources(0, 1, arrViews);

	// Shader
	pd3dImmediateContext->CSSetShader( m_pDownScaleFirstPassCS, NULL, 0 );

	// Execute the downscales first pass with enough groups to cover the entire full res HDR buffer
	pd3dImmediateContext->Dispatch(m_nDownScaleGroups, 1, 1);

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Second pass - reduce to a single pixel

	// Outoput
	ZeroMemory(arrUAVs, sizeof(arrUAVs));
	arrUAVs[0] = m_pAvgLumUAV;
	pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 2, arrUAVs, NULL );

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
	pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 2, arrUAVs, (UINT*)(&arrUAVs) );
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
}

void CPostFX::Blur(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pInput, ID3D11UnorderedAccessView* pOutput)
{
	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Second pass - horizontal gaussian filter

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
	// First pass - vertical gaussian filter

	// Output
	arrUAVs[0] = pOutput;
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL );

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

void CPostFX::FinalPass(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pHDRSRV)
{
	ID3D11ShaderResourceView* arrViews[3] = {pHDRSRV, m_pAvgLumSRV, m_pBloomSRV};
	pd3dImmediateContext->PSSetShaderResources(0, 3, arrViews);

	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(m_pFinalPassCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	TFinalPassCB* pFinalPass = (TFinalPassCB*)MappedResource.pData; 
	pFinalPass->fMiddleGrey = m_fMiddleGrey;
	pFinalPass->fLumWhiteSqr = m_fWhite;
	pFinalPass->fLumWhiteSqr *= pFinalPass->fMiddleGrey; // Scale by the middle grey value
	pFinalPass->fLumWhiteSqr *= pFinalPass->fLumWhiteSqr; // Square
	pFinalPass->fBloomScale = m_fBloomScale;
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
	pd3dImmediateContext->PSSetShaderResources(0, 3, arrViews);
	ZeroMemory(arrConstBuffers, sizeof(arrConstBuffers));
	pd3dImmediateContext->PSSetConstantBuffers(0, 1, arrConstBuffers);
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
}
