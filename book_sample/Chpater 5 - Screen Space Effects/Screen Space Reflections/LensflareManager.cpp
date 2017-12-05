#include "DXUT.h"
#include "DXUTCamera.h"
#include "SDKmisc.h"
#include "LensflareManager.h"

extern float g_fAspectRatio;

extern ID3D11Device* g_pDevice;
extern CFirstPersonCamera g_Camera;

HRESULT CompileShader(PWCHAR strPath, D3D10_SHADER_MACRO* pMacros, char* strEntryPoint, char* strProfile, DWORD dwShaderFlags, ID3DBlob** ppVertexShaderBuffer);

struct CB_LENSFLARE_VS
{
	D3DXVECTOR4 Position;
	D3DXVECTOR4 ScaleRotate;
	D3DXVECTOR4 Color;
};

CLensflareManager::CLensflareManager() : m_pPredicate(NULL), m_pOcclusionQuery(NULL), m_pNoDepthState(NULL), m_pAddativeBlendState(NULL),
	m_pLensflareCB(NULL), m_pLensflareVS(NULL), m_pLensflarePS(NULL), m_pCoronaTexView(NULL), m_pFlareTexView(NULL),
	m_fSunVisibility(0.0f), m_bQuerySunVisibility(true)
{

}

CLensflareManager::~CLensflareManager()
{

}

HRESULT CLensflareManager::Init()
{
	HRESULT hr;
	D3D11_QUERY_DESC queryDesc;
	queryDesc.Query = D3D11_QUERY_OCCLUSION_PREDICATE;
	queryDesc.MiscFlags = 0;
	V_RETURN( g_pDevice->CreatePredicate(&queryDesc, &m_pPredicate) );
	queryDesc.Query = D3D11_QUERY_OCCLUSION;
	V_RETURN( g_pDevice->CreateQuery(&queryDesc, &m_pOcclusionQuery) );

	m_fSunVisibility = 0.0f;
	m_bQuerySunVisibility = true;

	// Create constant buffers
    D3D11_BUFFER_DESC cbDesc;
    ZeroMemory( &cbDesc, sizeof(cbDesc) );
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.ByteWidth = CLensflareManager::m_TotalFlares * sizeof( CB_LENSFLARE_VS );
    V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pLensflareCB ) );
    DXUT_SetDebugName( m_pLensflareCB, "Lensflare CB" );
	
    // Compile the shaders
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	WCHAR str[MAX_PATH];
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Lensflare.hlsl" ) );
	ID3DBlob* pShaderBuffer = NULL;
	V_RETURN( CompileShader(str, NULL, "LensflareVS", "vs_5_0", dwShaderFlags, &pShaderBuffer) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBuffer->GetBufferPointer(),
                                              pShaderBuffer->GetBufferSize(), NULL, &m_pLensflareVS ) );
    DXUT_SetDebugName( m_pLensflareVS, "LensflareVS" );
	SAFE_RELEASE( pShaderBuffer );

	V_RETURN( CompileShader(str, NULL, "LensflarePS", "ps_5_0", dwShaderFlags, &pShaderBuffer) );
    V_RETURN( g_pDevice->CreatePixelShader( pShaderBuffer->GetBufferPointer(),
                                             pShaderBuffer->GetBufferSize(), NULL, &m_pLensflarePS ) );
    DXUT_SetDebugName( m_pLensflarePS, "LensfalrePS" );
	SAFE_RELEASE( pShaderBuffer );

	// Load the corona texture
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"..\\Media\\Corona.dds" ) );
	V_RETURN( D3DX11CreateShaderResourceViewFromFile( g_pDevice, str, NULL, NULL, &m_pCoronaTexView, NULL ) );

	// Load the flares texture
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"..\\Media\\Flare.dds" ) );
	V_RETURN( D3DX11CreateShaderResourceViewFromFile( g_pDevice, str, NULL, NULL, &m_pFlareTexView, NULL ) );

	D3D11_DEPTH_STENCIL_DESC descDepth;
	descDepth.DepthEnable = FALSE;
	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS;
	descDepth.StencilEnable = FALSE;
	descDepth.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	descDepth.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	const D3D11_DEPTH_STENCILOP_DESC defaultStencilOp = { D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS };
	descDepth.FrontFace = defaultStencilOp;
	descDepth.BackFace = defaultStencilOp;
	V_RETURN( g_pDevice->CreateDepthStencilState(&descDepth, &m_pNoDepthState) );
	DXUT_SetDebugName( m_pNoDepthState, "No Depth DS" );

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
	V_RETURN( g_pDevice->CreateBlendState(&descBlend, &m_pAddativeBlendState) );
	DXUT_SetDebugName( m_pAddativeBlendState, "Addative Blending BS" );

	m_arrFlares[0].fOffset = 0.0f;
	m_arrFlares[0].fScale = 0.1f;
	m_arrFlares[0].Color = D3DXVECTOR4(0.3f, 0.4f, 0.3f, 0.25f);
	m_arrFlares[1].fOffset = 0.0f;
	m_arrFlares[1].fScale = 0.1f;
	m_arrFlares[1].Color = D3DXVECTOR4(0.3f, 0.4f, 0.3f, 0.25f);
	m_arrFlares[2].fOffset = 0.0f;
	m_arrFlares[2].fScale = 0.1f;
	m_arrFlares[2].Color = D3DXVECTOR4(0.3f, 0.3f, 0.3f, 0.25f);
	m_arrFlares[3].fOffset = 0.5f;
	m_arrFlares[3].fScale = 0.05f;
	m_arrFlares[3].Color = D3DXVECTOR4(0.2f, 0.3f, 0.75f, 1.0f);
	m_arrFlares[4].fOffset = 1.0f;
	m_arrFlares[4].fScale = 0.04f;
	m_arrFlares[4].Color = D3DXVECTOR4(0.024f, 0.2f, 0.92f, 1.0f);
	m_arrFlares[5].fOffset = 1.75f;
	m_arrFlares[5].fScale = 0.075f;
	m_arrFlares[5].Color = D3DXVECTOR4(0.032f, 0.1f, 0.9f, 1.0f);
	m_arrFlares[6].fOffset = 0.9f;
	m_arrFlares[6].fScale = 0.05f;
	m_arrFlares[6].Color = D3DXVECTOR4(0.13f, 0.14f, 0.88f, 1.0f);
	m_arrFlares[7].fOffset = 1.85f;
	m_arrFlares[7].fScale = 0.03f;
	m_arrFlares[7].Color = D3DXVECTOR4(0.16f, 0.21, 0.94, 1.0f);

	return hr;
}

void CLensflareManager::Deinit()
{
	SAFE_RELEASE( m_pPredicate );
	SAFE_RELEASE( m_pOcclusionQuery );
	SAFE_RELEASE( m_pNoDepthState );
	SAFE_RELEASE( m_pAddativeBlendState );
	SAFE_RELEASE( m_pLensflareCB );
	SAFE_RELEASE( m_pLensflareVS );
	SAFE_RELEASE( m_pLensflarePS );
	SAFE_RELEASE( m_pCoronaTexView );
	SAFE_RELEASE( m_pFlareTexView );
}

void CLensflareManager::Update(const D3DXVECTOR3& sunWorldPos)
{
	D3DXMATRIX mView = *g_Camera.GetViewMatrix();
    D3DXMATRIX mProj = *g_Camera.GetProjMatrix();
	D3DXMATRIX mViewProjection = mView * mProj;
	for(int i=0; i < m_TotalLights; i++)
	{
		m_SunWorldPos = sunWorldPos;

		D3DXVECTOR3 ProjPos;
		D3DXVec3TransformCoord (&ProjPos, &m_SunWorldPos, &mViewProjection);
		m_SunPos2D.x = ProjPos.x;
		m_SunPos2D.y = ProjPos.y;
	}
}

void CLensflareManager::Render(ID3D11DeviceContext* pd3dImmediateContext)
{
	HRESULT hr;

	pd3dImmediateContext->SetPredication(m_pPredicate, FALSE);

	m_bQuerySunVisibility = false;
	UINT64 sunVisibility;
	if(pd3dImmediateContext->GetData(m_pOcclusionQuery, (void*)&sunVisibility, sizeof(sunVisibility), 0) == S_OK)
	{
		m_fSunVisibility = (float)sunVisibility / 700.0f;
		m_bQuerySunVisibility = true;
	}

	ID3D11DepthStencilState* pPrevDepthState;
	pd3dImmediateContext->OMGetDepthStencilState(&pPrevDepthState, NULL);
	pd3dImmediateContext->OMSetDepthStencilState(m_pNoDepthState, 0);

	ID3D11BlendState* pPrevBlendState;
	FLOAT prevBlendFactor[ 4 ];
	UINT prevSampleMask;
	pd3dImmediateContext->OMGetBlendState(&pPrevBlendState, prevBlendFactor, &prevSampleMask);
	pd3dImmediateContext->OMSetBlendState(m_pAddativeBlendState, prevBlendFactor, prevSampleMask);

	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->VSSetShader( m_pLensflareVS, NULL, 0 );
	pd3dImmediateContext->PSSetShader( m_pLensflarePS, NULL, 0 );

	// Fill the corona values
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V( pd3dImmediateContext->Map( m_pLensflareCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_LENSFLARE_VS* pCB = ( CB_LENSFLARE_VS* )MappedResource.pData;
	for(int j=0; j < 3; j++)
	{
		pCB[j].Position = D3DXVECTOR4(m_SunPos2D.x, m_SunPos2D.y, 0.0f, 0.0f);

		float fSin = sinf(static_cast<float>(j) * D3DX_PI / 3.0f + D3DX_PI / 4.0f);
		float fCos = cosf(static_cast<float>(j) * D3DX_PI / 3.0f + D3DX_PI / 4.0f);
		float fScaleX = m_arrFlares[j].fScale;
		float fScaleY = m_arrFlares[j].fScale * g_fAspectRatio;
		pCB[j].ScaleRotate = D3DXVECTOR4(fScaleX * fCos, fScaleY * -fSin, fScaleX * fSin, fScaleY * fCos);

		pCB[j].Color = m_arrFlares[j].Color * m_fSunVisibility;
	}
	pd3dImmediateContext->Unmap( m_pLensflareCB, 0 );

	// Render the corona
	pd3dImmediateContext->PSSetShaderResources( 0, 1, &m_pCoronaTexView );
	pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &m_pLensflareCB );
	pd3dImmediateContext->Draw(6 * 3, 0);

	// Fill the flare values
	D3DXVECTOR2 dirFlares = D3DXVECTOR2(0.0f, 0.0f) - m_SunPos2D;
	float fLength = D3DXVec2LengthSq(&dirFlares);
	dirFlares = dirFlares * (1.0f / fLength);
	V( pd3dImmediateContext->Map( m_pLensflareCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	pCB = ( CB_LENSFLARE_VS* )MappedResource.pData;
	for(int j=3; j < m_TotalFlares; j++)
	{
		float fOffset = m_arrFlares[j].fOffset * fLength;
		D3DXVECTOR2 flarePos2D = fOffset * dirFlares + m_SunPos2D;
		pCB[j-3].Position = D3DXVECTOR4(flarePos2D.x, flarePos2D.y, 0.0f, 0.0f);

		float fScale = m_arrFlares[j].fScale;
		pCB[j-3].ScaleRotate = D3DXVECTOR4(fScale, 0.0f, 0.0f, fScale * g_fAspectRatio);

		pCB[j-3].Color = m_arrFlares[j].Color * m_fSunVisibility;
	}
	pd3dImmediateContext->Unmap( m_pLensflareCB, 0 );

	// Render the flares
	pd3dImmediateContext->PSSetShaderResources( 0, 1, &m_pFlareTexView );
	pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &m_pLensflareCB );
	pd3dImmediateContext->Draw(6 * (m_TotalFlares - 3), 0);

	// Restore the blend state
	pd3dImmediateContext->OMSetDepthStencilState(pPrevDepthState, 0);
	SAFE_RELEASE( pPrevDepthState );
	pd3dImmediateContext->OMSetBlendState(pPrevBlendState, prevBlendFactor, prevSampleMask);
	SAFE_RELEASE( pPrevBlendState );

	pd3dImmediateContext->SetPredication(NULL, FALSE);
}

void CLensflareManager::BeginSunVisibility(ID3D11DeviceContext* pd3dImmediateContext)
{
	pd3dImmediateContext->Begin(m_pPredicate);
	if(m_bQuerySunVisibility)
	{
		pd3dImmediateContext->Begin(m_pOcclusionQuery);
	}
	
}

void CLensflareManager::EndSunVisibility(ID3D11DeviceContext* pd3dImmediateContext)
{
	pd3dImmediateContext->End(m_pPredicate);
	if(m_bQuerySunVisibility)
	{
		pd3dImmediateContext->End(m_pOcclusionQuery);
	}
}
