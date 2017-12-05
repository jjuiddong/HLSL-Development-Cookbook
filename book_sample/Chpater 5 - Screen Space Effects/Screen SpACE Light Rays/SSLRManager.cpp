#include "DXUT.h"
#include "SDKmisc.h"
#include "DXUTCamera.h"
#include "SSLRManager.h"

extern ID3D11Device* g_pDevice;
extern CFirstPersonCamera g_Camera;

#pragma pack(push,1)
struct CB_OCCLUSSION
{
	UINT nWidth;
	UINT nHeight;
	UINT pad[2];
};

struct CB_LIGHT_RAYS
{
	D3DXVECTOR2 vSunPos;
	float fInitDecay;
	float fDistDecay;
	D3DXVECTOR3 vRayColor;
	float fMaxDeltaLen;
};
#pragma pack(pop)


bool g_bShowRayTraceRes = false;

HRESULT CompileShader(PWCHAR strPath, D3D10_SHADER_MACRO* pMacros, char* strEntryPoint, char* strProfile, DWORD dwShaderFlags, ID3DBlob** ppVertexShaderBuffer);

CSSLRManager::CSSLRManager() : m_fInitDecay(0.2f), m_fDistDecay(0.8f), m_fMaxDeltaLen(0.005f),
	m_pOcclusionTex(NULL), m_pOcclusionUAV(NULL), m_pOcclusionSRV(NULL),
	m_pLightRaysTex(NULL), m_pLightRaysRTV(NULL), m_pLightRaysSRV(NULL),
	m_pOcclusionCB(NULL), m_pOcclusionCS(NULL), m_pRayTraceCB(NULL), m_pFullScreenVS(NULL), m_pRayTracePS(NULL), m_pCombinePS(NULL),
	m_pAdditiveBlendState(NULL)
{

}

CSSLRManager::~CSSLRManager()
{

}

HRESULT CSSLRManager::Init(UINT width, UINT height)
{
	HRESULT hr;

	m_nWidth = width / 2;
	m_nHeight = height / 2;

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate the occlusion resources
	D3D11_TEXTURE2D_DESC t2dDesc = {
		m_nWidth, //UINT Width;
		m_nHeight, //UINT Height;
		1, //UINT MipLevels;
		1, //UINT ArraySize;
		DXGI_FORMAT_R8_TYPELESS, //DXGI_FORMAT Format;
		1, //DXGI_SAMPLE_DESC SampleDesc;
		0,
		D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
		D3D11_BIND_UNORDERED_ACCESS|D3D11_BIND_SHADER_RESOURCE,//UINT BindFlags;
		0,//UINT CPUAccessFlags;
		0//UINT MiscFlags;    
	};
	V_RETURN( g_pDevice->CreateTexture2D( &t2dDesc, NULL, &m_pOcclusionTex ) );
	DXUT_SetDebugName( m_pOcclusionTex, "SSLR Occlusion Texture" );

	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
	ZeroMemory( &UAVDesc, sizeof(UAVDesc) );
	UAVDesc.Format = DXGI_FORMAT_R8_UNORM;
	UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	V_RETURN( g_pDevice->CreateUnorderedAccessView( m_pOcclusionTex, &UAVDesc, &m_pOcclusionUAV ) );
	DXUT_SetDebugName( m_pOcclusionUAV, "SSLR Occlusion UAV" );

	D3D11_SHADER_RESOURCE_VIEW_DESC dsrvd = 
	{
		DXGI_FORMAT_R8_UNORM,
		D3D11_SRV_DIMENSION_TEXTURE2D,
		0,
		0
	};
	dsrvd.Texture2D.MipLevels = 1;
	V_RETURN( g_pDevice->CreateShaderResourceView( m_pOcclusionTex, &dsrvd, &m_pOcclusionSRV ) );
	DXUT_SetDebugName( m_pOcclusionSRV, "SSLR Occlusion SRV" );

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate the light rays resources
	t2dDesc.BindFlags = D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
	V_RETURN( g_pDevice->CreateTexture2D( &t2dDesc, NULL, &m_pLightRaysTex ) );
	DXUT_SetDebugName( m_pLightRaysTex, "SSLR Light Rays Texture" );

	D3D11_RENDER_TARGET_VIEW_DESC rtsvd = 
	{
		DXGI_FORMAT_R8_UNORM,
		D3D11_RTV_DIMENSION_TEXTURE2D
	};
	V_RETURN( g_pDevice->CreateRenderTargetView( m_pLightRaysTex, &rtsvd, &m_pLightRaysRTV ) ); 
	DXUT_SetDebugName( m_pLightRaysRTV, "SSLR Light Rays RTV" );

	V_RETURN( g_pDevice->CreateShaderResourceView( m_pLightRaysTex, &dsrvd, &m_pLightRaysSRV ) );
	DXUT_SetDebugName( m_pLightRaysSRV, "SSLR Light Rays SRV" );

	//////////////////////////////////////////////////////////////////////////////////////////////////////
	// Allocate the occlussion constant buffer
	D3D11_BUFFER_DESC CBDesc;
	ZeroMemory( &CBDesc, sizeof(CBDesc) );
	CBDesc.Usage = D3D11_USAGE_DYNAMIC;
	CBDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	CBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	CBDesc.ByteWidth = sizeof(CB_OCCLUSSION);
	V_RETURN( g_pDevice->CreateBuffer( &CBDesc, NULL, &m_pOcclusionCB ) );
	DXUT_SetDebugName( m_pOcclusionCB, "SSLR - Occlussion CB" );

	CBDesc.ByteWidth = sizeof(CB_LIGHT_RAYS);
	V_RETURN( g_pDevice->CreateBuffer( &CBDesc, NULL, &m_pRayTraceCB ) );
	DXUT_SetDebugName( m_pRayTraceCB, "SSLR - Ray Trace CB" );
	
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
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"SSLR.hlsl" ) );

	V_RETURN( CompileShader( str, NULL, "Occlussion", "cs_5_0", dwShaderFlags, &pShaderBlob ) );
	V_RETURN( g_pDevice->CreateComputeShader( pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize(), NULL, &m_pOcclusionCS ) );
	DXUT_SetDebugName( m_pOcclusionCS, "SSLR - Occlussion CS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "RayTraceVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pFullScreenVS ) );
	DXUT_SetDebugName( m_pFullScreenVS, "SSLR - Full Screen VS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "RayTracePS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pRayTracePS ) );
	DXUT_SetDebugName( m_pRayTracePS, "SSLR - Ray Trace PS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "CombinePS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pCombinePS ) );
	DXUT_SetDebugName( m_pCombinePS, "SSLR - Combine PS" );
	SAFE_RELEASE( pShaderBlob );
	
	// Create the additive blend state
	D3D11_BLEND_DESC descBlend;
	descBlend.AlphaToCoverageEnable = FALSE;
	descBlend.IndependentBlendEnable = FALSE;
	const D3D11_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
	{
		TRUE,
		D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD,
		D3D11_BLEND_ONE, D3D11_BLEND_ONE, D3D11_BLEND_OP_ADD,
		D3D11_COLOR_WRITE_ENABLE_ALL,
	};
	for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		descBlend.RenderTarget[ i ] = defaultRenderTargetBlendDesc;
	V_RETURN( g_pDevice->CreateBlendState(&descBlend, &m_pAdditiveBlendState) );
	DXUT_SetDebugName( m_pAdditiveBlendState, "SSLR - Additive Alpha BS" );

	return hr;
}

void CSSLRManager::Deinit()
{
	SAFE_RELEASE(m_pOcclusionTex);
	SAFE_RELEASE(m_pOcclusionUAV);
	SAFE_RELEASE(m_pOcclusionSRV);
	SAFE_RELEASE(m_pLightRaysTex);
	SAFE_RELEASE(m_pLightRaysRTV);
	SAFE_RELEASE(m_pLightRaysSRV);
	SAFE_RELEASE(m_pOcclusionCB);
	SAFE_RELEASE(m_pOcclusionCS);
	SAFE_RELEASE(m_pRayTraceCB);
	SAFE_RELEASE(m_pFullScreenVS);
	SAFE_RELEASE(m_pRayTracePS);
	SAFE_RELEASE(m_pCombinePS);
	SAFE_RELEASE(m_pAdditiveBlendState);
}

void CSSLRManager::Render(ID3D11DeviceContext* pd3dImmediateContext, ID3D11RenderTargetView* pLightAccumRTV, ID3D11ShaderResourceView* pMiniDepthSRV, const D3DXVECTOR3& vSunDir, const D3DXVECTOR3& vSunColor)
{
	// No need to do anything if the camera is facing away from the sun
	// This will not work if the FOV is close to 180 or higher
	const float dotCamSun = -D3DXVec3Dot(g_Camera.GetWorldAhead(), &vSunDir);
	if(dotCamSun <= 0.0f)
	{
		return;
	}

	D3DXVECTOR3 vSunPos = -200.0f * vSunDir;
	D3DXVECTOR3 vEyePos = *g_Camera.GetEyePt();
	vSunPos.x += vEyePos.x;
	vSunPos.z += vEyePos.z;
	D3DXMATRIX mView = *g_Camera.GetViewMatrix();
	D3DXMATRIX mProj = *g_Camera.GetProjMatrix();
	D3DXMATRIX mViewProjection = mView * mProj;
	D3DXVECTOR3 vSunPosSS;
	D3DXVec3TransformCoord(&vSunPosSS, &vSunPos, &mViewProjection);

	// If the sun is too far out of view we just want to turn off the effect
	static const float fMaxSunDist = 1.3f;
	if(abs(vSunPosSS.x) >= fMaxSunDist || abs(vSunPosSS.y) >= fMaxSunDist)
	{
		return;
	}

	// Attenuate the sun color based on how far the sun is from the view
	D3DXVECTOR3 vSunColorAtt = vSunColor;
	float fMaxDist = max(abs(vSunPosSS.x), abs(vSunPosSS.y));
	if(fMaxDist >= 1.0f)
	{
		vSunColorAtt *= (fMaxSunDist - fMaxDist);
	}

	PrepareOcclusion(pd3dImmediateContext, pMiniDepthSRV);
	RayTrace(pd3dImmediateContext, D3DXVECTOR2(vSunPosSS.x, vSunPosSS.y), vSunColorAtt);
	if(!g_bShowRayTraceRes)
		Combine(pd3dImmediateContext, pLightAccumRTV);
}

void CSSLRManager::PrepareOcclusion(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pMiniDepthSRV)
{
	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(m_pOcclusionCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	CB_OCCLUSSION* pOcclussion = (CB_OCCLUSSION*)MappedResource.pData; 
	pOcclussion->nWidth = m_nWidth;
	pOcclussion->nHeight = m_nHeight;
	pd3dImmediateContext->Unmap(m_pOcclusionCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { m_pOcclusionCB };
	pd3dImmediateContext->CSSetConstantBuffers(0, 1, arrConstBuffers);

	// Output
	ID3D11UnorderedAccessView* arrUAVs[1] = { m_pOcclusionUAV };
	pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, arrUAVs, NULL);

	// Input
	ID3D11ShaderResourceView* arrViews[1] = { pMiniDepthSRV };
	pd3dImmediateContext->CSSetShaderResources(0, 1, arrViews);

	// Shader
	pd3dImmediateContext->CSSetShader( m_pOcclusionCS, NULL, 0 );

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

void CSSLRManager::RayTrace(ID3D11DeviceContext* pd3dImmediateContext, const D3DXVECTOR2& vSunPosSS, const D3DXVECTOR3& vSunColor)
{
	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	pd3dImmediateContext->ClearRenderTargetView( m_pLightRaysRTV, ClearColor );

	D3D11_VIEWPORT oldvp;
	UINT num = 1;
	pd3dImmediateContext->RSGetViewports(&num, &oldvp);
	if(!g_bShowRayTraceRes)
	{
		D3D11_VIEWPORT vp[1] = { { 0, 0, (float)m_nWidth, (float)m_nHeight, 0.0f, 1.0f } };
		pd3dImmediateContext->RSSetViewports( 1, vp );

		pd3dImmediateContext->OMSetRenderTargets( 1, &m_pLightRaysRTV, NULL );
	}

	// Constants
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map(m_pRayTraceCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	CB_LIGHT_RAYS* pRayTrace = (CB_LIGHT_RAYS*)MappedResource.pData; 
	pRayTrace->vSunPos = D3DXVECTOR2(0.5f * vSunPosSS.x + 0.5f, -0.5f * vSunPosSS.y + 0.5f);
	pRayTrace->fInitDecay = m_fInitDecay;
	pRayTrace->fDistDecay = m_fDistDecay;
	pRayTrace->vRayColor = vSunColor;
	pRayTrace->fMaxDeltaLen = m_fMaxDeltaLen;
	pd3dImmediateContext->Unmap(m_pRayTraceCB, 0);
	ID3D11Buffer* arrConstBuffers[1] = { m_pRayTraceCB };
	pd3dImmediateContext->PSSetConstantBuffers(0, 1, arrConstBuffers);

	// Input
	ID3D11ShaderResourceView* arrViews[1] = { m_pOcclusionSRV };
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);

	// Primitive settings
	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pFullScreenVS, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(m_pRayTracePS, NULL, 0);

	pd3dImmediateContext->Draw(4, 0);

	// Cleanup
	arrViews[0] = NULL;
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	pd3dImmediateContext->RSSetViewports( 1, &oldvp );
}

void CSSLRManager::Combine(ID3D11DeviceContext* pd3dImmediateContext, ID3D11RenderTargetView* pLightAccumRTV)
{
	ID3D11BlendState* pPrevBlendState;
	FLOAT prevBlendFactor[ 4 ];
	UINT prevSampleMask;
	pd3dImmediateContext->OMGetBlendState(&pPrevBlendState, prevBlendFactor, &prevSampleMask);
	pd3dImmediateContext->OMSetBlendState(m_pAdditiveBlendState, prevBlendFactor, prevSampleMask);

	// Restore the light accumulation view
	pd3dImmediateContext->OMSetRenderTargets( 1, &pLightAccumRTV, NULL );

	// Constants
	ID3D11Buffer* arrConstBuffers[1] = { m_pRayTraceCB };
	pd3dImmediateContext->PSSetConstantBuffers(0, 1, arrConstBuffers);

	// Input
	ID3D11ShaderResourceView* arrViews[1] = { m_pLightRaysSRV };
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);

	// Primitive settings
	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pFullScreenVS, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(m_pCombinePS, NULL, 0);

	pd3dImmediateContext->Draw(4, 0);

	// Cleanup
	arrViews[0] = NULL;
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrViews);
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	pd3dImmediateContext->OMSetBlendState(pPrevBlendState, prevBlendFactor, prevSampleMask);
}
