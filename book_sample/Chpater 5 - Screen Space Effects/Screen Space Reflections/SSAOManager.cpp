#include "DXUT.h"
#include "SDKmisc.h"
#include "DXUTCamera.h"
#include "SSAOManager.h"

extern ID3D11Device* g_pDevice;
extern CFirstPersonCamera g_Camera;

HRESULT CompileShader(PWCHAR strPath, D3D10_SHADER_MACRO* pMacros, char* strEntryPoint, char* strProfile, DWORD dwShaderFlags, ID3DBlob** ppVertexShaderBuffer);

CSSAOManager::CSSAOManager() : m_pDownscaleCB(NULL),
	m_pSSAO_RT(NULL), m_pSSAO_SRV(NULL), m_pSSAO_UAV(NULL),
	m_pMiniDepthBuffer(NULL), m_pMiniDepthUAV(NULL), m_pMiniDepthSRV(NULL),
	m_pDepthDownscaleCS(NULL), m_pComputeCS(NULL)
{

}

CSSAOManager::~CSSAOManager()
{

}

HRESULT CSSAOManager::Init(UINT width, UINT height)
{
	HRESULT hr;

	m_nWidth = width / 2;
	m_nHeight = height / 2;

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate SSAO
	D3D11_TEXTURE2D_DESC t2dDesc = {
		m_nWidth, //UINT Width;
		m_nHeight, //UINT Height;
		1, //UINT MipLevels;
		1, //UINT ArraySize;
		DXGI_FORMAT_R32_TYPELESS,//DXGI_FORMAT_R8_TYPELESS, //DXGI_FORMAT Format;
		1, //DXGI_SAMPLE_DESC SampleDesc;
		0,
		D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
		D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_SHADER_RESOURCE,//UINT BindFlags;
		0,//UINT CPUAccessFlags;
		0//UINT MiscFlags;    
        };
	V_RETURN( g_pDevice->CreateTexture2D( &t2dDesc, NULL, &m_pSSAO_RT ) );
	DXUT_SetDebugName( m_pSSAO_RT, "SSAO - Final AO values" );

	// Create the UAVs
	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
	ZeroMemory( &UAVDesc, sizeof(UAVDesc) );
	UAVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	V_RETURN( g_pDevice->CreateUnorderedAccessView( m_pSSAO_RT, &UAVDesc, &m_pSSAO_UAV ) );
	DXUT_SetDebugName( m_pSSAO_UAV, "SSAO - Final AO values UAV" );

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	ZeroMemory(&SRVDesc, sizeof(SRVDesc));
	SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;
	V_RETURN( g_pDevice->CreateShaderResourceView( m_pSSAO_RT, &SRVDesc, &m_pSSAO_SRV ) );
	DXUT_SetDebugName( m_pMiniDepthSRV, "SSAO - Final AO values SRV" );

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate down scaled depth buffer
	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory( &bufferDesc, sizeof(bufferDesc) );
	bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.StructureByteStride = 4 * sizeof(float);
	bufferDesc.ByteWidth = m_nWidth * m_nHeight * bufferDesc.StructureByteStride;
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	V_RETURN( g_pDevice->CreateBuffer( &bufferDesc, NULL, &m_pMiniDepthBuffer ) );
	DXUT_SetDebugName( m_pMiniDepthBuffer, "SSAO - Downscaled Depth Buffer" );

	ZeroMemory( &UAVDesc, sizeof(UAVDesc) );
	UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	UAVDesc.Buffer.FirstElement = 0;
	UAVDesc.Buffer.NumElements = m_nWidth * m_nHeight;
	V_RETURN( g_pDevice->CreateUnorderedAccessView( m_pMiniDepthBuffer, &UAVDesc, &m_pMiniDepthUAV ) );
	DXUT_SetDebugName( m_pMiniDepthUAV, "SSAO - Downscaled Depth UAV" );

	ZeroMemory(&SRVDesc, sizeof(SRVDesc));
	SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SRVDesc.Buffer.FirstElement = 0;
	SRVDesc.Buffer.NumElements = m_nWidth * m_nHeight;
	V_RETURN( g_pDevice->CreateShaderResourceView( m_pMiniDepthBuffer, &SRVDesc, &m_pMiniDepthSRV ) );
	DXUT_SetDebugName( m_pMiniDepthSRV, "SSAO - Downscaled Depth SRV" );

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate down scale depth constant buffer
	D3D11_BUFFER_DESC CBDesc;
	ZeroMemory( &CBDesc, sizeof(CBDesc) );
	CBDesc.Usage = D3D11_USAGE_DYNAMIC;
	CBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	CBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	CBDesc.ByteWidth = sizeof(TDownscaleCB);
	V_RETURN( g_pDevice->CreateBuffer( &CBDesc, NULL, &m_pDownscaleCB ) );
	DXUT_SetDebugName( m_pDownscaleCB, "SSAO - Downscale Depth CB" );

	//////////////////////////////////////////////////////////////////////////////////////////////////////
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
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"SSAO.hlsl" ) );

	V_RETURN( CompileShader( str, NULL, "DepthDownscale", "cs_5_0", 0, &pShaderBlob ) );
	V_RETURN( g_pDevice->CreateComputeShader( pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &m_pDepthDownscaleCS ) );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader( str, NULL, "SSAOCompute", "cs_5_0", 0, &pShaderBlob ) );
	V_RETURN( g_pDevice->CreateComputeShader( pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &m_pComputeCS ) );
	SAFE_RELEASE( pShaderBlob );

	return hr;
}

void CSSAOManager::Deinit()
{
	SAFE_RELEASE(m_pDownscaleCB);
	SAFE_RELEASE(m_pSSAO_RT);
	SAFE_RELEASE(m_pSSAO_SRV);
	SAFE_RELEASE(m_pSSAO_UAV);
	SAFE_RELEASE(m_pMiniDepthBuffer);
	SAFE_RELEASE(m_pMiniDepthUAV);
	SAFE_RELEASE(m_pMiniDepthSRV);
	SAFE_RELEASE(m_pDepthDownscaleCS);
	SAFE_RELEASE(m_pComputeCS);
}

void CSSAOManager::Compute(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pDepthSRV, ID3D11ShaderResourceView* pNormalsSRV)
{
	DownscaleDepth(pd3dImmediateContext, pDepthSRV, pNormalsSRV);
	ComputeSSAO(pd3dImmediateContext);
	Blur(pd3dImmediateContext);
}

void CSSAOManager::DownscaleDepth(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pDepthSRV, ID3D11ShaderResourceView* pNormalsSRV)
{
	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(m_pDownscaleCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	TDownscaleCB* pDownscale = (TDownscaleCB*)MappedResource.pData; 
	pDownscale->nWidth = m_nWidth;
	pDownscale->nHeight = m_nHeight;
	pDownscale->fHorResRcp = 1.0f / (float)pDownscale->nWidth;
	pDownscale->fVerResRcp = 1.0f / (float)pDownscale->nHeight;
	const D3DXMATRIX* pProj = g_Camera.GetProjMatrix();
	pDownscale->ProjParams.x = 1.0f / pProj->m[0][0];
	pDownscale->ProjParams.y = 1.0f / pProj->m[1][1];
	float fQ = g_Camera.GetFarClip() / (g_Camera.GetFarClip() - g_Camera.GetNearClip());
	pDownscale->ProjParams.z = -g_Camera.GetNearClip() * fQ;
	pDownscale->ProjParams.w = -fQ;
	D3DXMatrixTranspose(&pDownscale->ViewMatrix, g_Camera.GetViewMatrix());
	pDownscale->fOffsetRadius = (float)m_iSSAOSampRadius;
	pDownscale->fRadius = m_fRadius;
	pDownscale->fMaxDepth = g_Camera.GetFarClip();
	pd3dImmediateContext->Unmap(m_pDownscaleCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { m_pDownscaleCB };
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

	// Output
	ID3D11UnorderedAccessView* arrUAVs[1] = { m_pMiniDepthUAV };
	pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, arrUAVs, NULL);

	// Input
	ID3D11ShaderResourceView* arrViews[2] = { pDepthSRV, pNormalsSRV };
	pd3dImmediateContext->CSSetShaderResources(0, 2, arrViews);

	// Shader
	pd3dImmediateContext->CSSetShader( m_pDepthDownscaleCS, NULL, 0 );

	// Execute the downscales first pass with enough groups to cover the entire full res HDR buffer
	pd3dImmediateContext->Dispatch((UINT)ceil((float)(m_nWidth * m_nHeight) / 1024.0f), 1, 1);

	// Cleanup
	pd3dImmediateContext->CSSetShader( NULL, NULL, 0 );
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->CSSetShaderResources(0, 2, arrViews);
	ZeroMemory(arrUAVs, sizeof(arrUAVs));
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);
	ZeroMemory(arrConstBuffers, sizeof(arrConstBuffers));
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);
}

void CSSAOManager::ComputeSSAO(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Constants
	ID3D11Buffer* arrConstBuffers[1] = { m_pDownscaleCB };
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

	// Output
	ID3D11UnorderedAccessView* arrUAVs[1] = { m_pSSAO_UAV };
	pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, arrUAVs, NULL);

	// Input
	ID3D11ShaderResourceView* arrViews[1] = { m_pMiniDepthSRV };
	pd3dImmediateContext->CSSetShaderResources(0, 1, arrViews);

	// Shader
	pd3dImmediateContext->CSSetShader( m_pComputeCS, NULL, 0 );

	// Execute the downscales first pass with enough groups to cover the entire full res HDR buffer
	pd3dImmediateContext->Dispatch((UINT)ceil((float)(m_nWidth * m_nHeight) / 1024.0f), 1, 1);

	// Cleanup
	pd3dImmediateContext->CSSetShader( NULL, NULL, 0 );
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->CSSetShaderResources(0, 1, arrViews);
	ZeroMemory(arrUAVs, sizeof(arrUAVs));
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, arrUAVs, NULL);
	ZeroMemory(arrConstBuffers, sizeof(arrConstBuffers));
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);
}

void CSSAOManager::Blur(ID3D11DeviceContext* pd3dImmediateContext)
{

}
