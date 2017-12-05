#include "DXUT.h"
#include "SDKmisc.h"
#include "DXUTCamera.h"
#include "SSReflectionManager.h"

extern ID3D11Device* g_pDevice;
extern CFirstPersonCamera g_Camera;

HRESULT CompileShader(PWCHAR strPath, D3D10_SHADER_MACRO* pMacros, char* strEntryPoint, char* strProfile, DWORD dwShaderFlags, ID3DBlob** ppVertexShaderBuffer);

#pragma pack(push,1)
struct CB_REFLECT_VS
{
	D3DXMATRIX mWorldViewProj;
	D3DXMATRIX mWorldView;
};

struct CB_REFLECT_PS
{
	D3DXMATRIX mProjMatrix;
	float fViewAngleThreshold;
	float fEdgeDistThreshold;
	float fDepthBias;
	float fReflectionScale;
	D3DXVECTOR4 vPerspectiveValues;
};
#pragma pack(pop)

CSSReflectionManager::CSSReflectionManager() : m_fViewAngleThreshold(0.2f), m_fEdgeDistThreshold(0.2f), m_fDepthBias(0.0025f), m_fReflectionScale(2.0f), 
	m_pRefelctionVS(NULL),  m_pRefelctionPS(NULL), m_pRefelctionBlendVS(NULL), m_pRefelctionBlendPS(NULL), m_pRefelctionVexterShaderCB(NULL), m_pRefelctionPixelShaderCB(NULL),
	m_pReflectVSLayout(NULL), m_pAddativeBlendState(NULL), m_pDepthEqualNoWrite(NULL),
	m_pReflectTexture(NULL), m_ReflectRTV(NULL), m_ReflectSRV(NULL)
{

}

CSSReflectionManager::~CSSReflectionManager()
{

}

HRESULT CSSReflectionManager::Init(UINT width, UINT height)
{
	HRESULT hr = S_OK;

	// Create constant buffer
	D3D11_BUFFER_DESC cbDesc;
	ZeroMemory( &cbDesc, sizeof(cbDesc) );
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbDesc.ByteWidth = sizeof( CB_REFLECT_VS );
	V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pRefelctionVexterShaderCB ) );
	DXUT_SetDebugName( m_pRefelctionVexterShaderCB, "Reflection Vertex Shader CB" );
	
	cbDesc.ByteWidth = sizeof( CB_REFLECT_PS );
	V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pRefelctionPixelShaderCB ) );
	DXUT_SetDebugName( m_pRefelctionPixelShaderCB, "Reflection Pixel Shader CB" );

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
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"SSReflection.hlsl" ) );

	ID3DBlob* pShaderBlob = NULL;
	V_RETURN( CompileShader(str, NULL, "SSReflectionVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pRefelctionVS ) );
	DXUT_SetDebugName( m_pRefelctionVS, "SSReflection VS" );

	// Create a layout for the object data
	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
    V_RETURN( g_pDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), pShaderBlob->GetBufferPointer(),
                                             pShaderBlob->GetBufferSize(), &m_pReflectVSLayout ) );
    DXUT_SetDebugName( m_pReflectVSLayout, "SSReflection Vertex Layout" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "SSReflectionPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pRefelctionPS ) );
	DXUT_SetDebugName( m_pRefelctionPS, "SSReflection PS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "ReflectionBlendVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pRefelctionBlendVS ) );
	DXUT_SetDebugName( m_pRefelctionBlendVS, "SSReflection Blend VS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "ReflectionBlendPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pRefelctionBlendPS ) );
	DXUT_SetDebugName( m_pRefelctionBlendPS, "SSReflection Blend PS" );
	SAFE_RELEASE( pShaderBlob );

	D3D11_DEPTH_STENCIL_DESC descDepth;
	descDepth.DepthEnable = TRUE;
	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	descDepth.StencilEnable = FALSE;
	V_RETURN( g_pDevice->CreateDepthStencilState(&descDepth, &m_pDepthEqualNoWrite) );
	DXUT_SetDebugName( m_pDepthEqualNoWrite, "Reflection DS" );

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
	DXUT_SetDebugName( m_pAddativeBlendState, "Reflection BS" );

	// Create the HDR render target
	D3D11_TEXTURE2D_DESC dtd = {
		width, //UINT Width;
		height, //UINT Height;
		1, //UINT MipLevels;
		1, //UINT ArraySize;
		DXGI_FORMAT_R16G16B16A16_TYPELESS, //DXGI_FORMAT Format;
		1, //DXGI_SAMPLE_DESC SampleDesc;
		0,
		D3D11_USAGE_DEFAULT,//D3D11_USAGE Usage;
		D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE,//UINT BindFlags;
		0,//UINT CPUAccessFlags;
		0//UINT MiscFlags;    
	};
	V_RETURN( g_pDevice->CreateTexture2D( &dtd, NULL, &m_pReflectTexture ) );
	DXUT_SetDebugName( m_pReflectTexture, "Reflection Accumulation Texture" );

	D3D11_RENDER_TARGET_VIEW_DESC rtsvd = 
	{
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		D3D11_RTV_DIMENSION_TEXTURE2D
	};
	V_RETURN( g_pDevice->CreateRenderTargetView( m_pReflectTexture, &rtsvd, &m_ReflectRTV ) ); 
	DXUT_SetDebugName( m_ReflectRTV, "Reflection Accumulation RTV" );

	D3D11_SHADER_RESOURCE_VIEW_DESC dsrvd = 
	{
		DXGI_FORMAT_R16G16B16A16_FLOAT,
		D3D11_SRV_DIMENSION_TEXTURE2D,
		0,
		0
	};
	dsrvd.Texture2D.MipLevels = 1;
	V_RETURN( g_pDevice->CreateShaderResourceView( m_pReflectTexture, &dsrvd, &m_ReflectSRV ) );
	DXUT_SetDebugName( m_ReflectSRV, "Reflection Accumulation SRV" );

	return hr;
}

void CSSReflectionManager::Deinit()
{
	SAFE_RELEASE( m_pRefelctionVS );
	SAFE_RELEASE( m_pRefelctionPS );
	SAFE_RELEASE( m_pRefelctionBlendVS );
	SAFE_RELEASE( m_pRefelctionBlendPS );
	SAFE_RELEASE( m_pRefelctionVexterShaderCB );
	SAFE_RELEASE( m_pRefelctionPixelShaderCB );
	SAFE_RELEASE( m_pReflectVSLayout );
	SAFE_RELEASE( m_pDepthEqualNoWrite );
	SAFE_RELEASE( m_pAddativeBlendState );
	SAFE_RELEASE( m_pReflectTexture );
	SAFE_RELEASE( m_ReflectRTV );
	SAFE_RELEASE( m_ReflectSRV );
}

void CSSReflectionManager::PreRenderReflection(ID3D11DeviceContext* pd3dImmediateContext, ID3D11ShaderResourceView* pDiffuseSRV, ID3D11ShaderResourceView* pDepthSRV,ID3D11ShaderResourceView* pNormalsSRV, ID3D11DepthStencilView* ptDepthReadOnlyDSV)
{
	pd3dImmediateContext->OMSetDepthStencilState(m_pDepthEqualNoWrite, 0);
	
	// Clear to black
	float ClearColor[4] = { 0.0f, 0.0, 0.0, 0.0f };
	pd3dImmediateContext->ClearRenderTargetView( m_ReflectRTV, ClearColor );
	pd3dImmediateContext->OMSetRenderTargets( 1, &m_ReflectRTV, ptDepthReadOnlyDSV );

	const D3DXMATRIX* pProj = g_Camera.GetProjMatrix();
	const D3DXMATRIX mWorldViewProj = *g_Camera.GetViewMatrix() * *pProj;

	// Fill the constant buffers
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dImmediateContext->Map( m_pRefelctionVexterShaderCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CB_REFLECT_VS* pReflectVSCB = ( CB_REFLECT_VS* )MappedResource.pData;
	D3DXMatrixTranspose(&pReflectVSCB->mWorldViewProj, &mWorldViewProj);
	D3DXMatrixTranspose(&pReflectVSCB->mWorldView, g_Camera.GetViewMatrix());
	pd3dImmediateContext->Unmap( m_pRefelctionVexterShaderCB, 0 );
	pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &m_pRefelctionVexterShaderCB );

	pd3dImmediateContext->Map( m_pRefelctionPixelShaderCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CB_REFLECT_PS* pReflectCB = ( CB_REFLECT_PS* )MappedResource.pData;
	D3DXMatrixTranspose(&pReflectCB->mProjMatrix, g_Camera.GetProjMatrix());
	pReflectCB->fViewAngleThreshold = m_fViewAngleThreshold;
	pReflectCB->fEdgeDistThreshold = m_fEdgeDistThreshold;
	pReflectCB->fDepthBias = m_fDepthBias;
	pReflectCB->fReflectionScale = m_fReflectionScale;
	pReflectCB->vPerspectiveValues.x = 1.0f / pProj->m[0][0];
	pReflectCB->vPerspectiveValues.y = 1.0f / pProj->m[1][1];
	pReflectCB->vPerspectiveValues.z = pProj->m[3][2];
	pReflectCB->vPerspectiveValues.w = -pProj->m[2][2];
	pd3dImmediateContext->Unmap( m_pRefelctionPixelShaderCB, 0 );
	pd3dImmediateContext->PSSetConstantBuffers( 0, 1, &m_pRefelctionPixelShaderCB );

	ID3D11ShaderResourceView* arrViews[3] = { pDiffuseSRV, pDepthSRV, pNormalsSRV };
	pd3dImmediateContext->PSSetShaderResources(0, 3, arrViews);

	pd3dImmediateContext->IASetInputLayout( m_pReflectVSLayout );

	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pRefelctionVS, NULL, 0);
	pd3dImmediateContext->PSSetShader(m_pRefelctionPS, NULL, 0);
}

void CSSReflectionManager::PostRenderReflection(ID3D11DeviceContext* pd3dImmediateContext)
{
	ID3D11ShaderResourceView* arrViews[3] = { NULL, NULL, NULL };
	pd3dImmediateContext->PSSetShaderResources(0, 3, arrViews);
}

void CSSReflectionManager::DoReflectionBlend(ID3D11DeviceContext* pd3dImmediateContext)
{
	ID3D11BlendState* pPrevBlendState;
	FLOAT prevBlendFactor[ 4 ];
	UINT prevSampleMask;
	pd3dImmediateContext->OMGetBlendState(&pPrevBlendState, prevBlendFactor, &prevSampleMask);
	pd3dImmediateContext->OMSetBlendState(m_pAddativeBlendState, prevBlendFactor, prevSampleMask);

	pd3dImmediateContext->PSSetShaderResources(0, 1, &m_ReflectSRV);

	// Primitive settings
	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pRefelctionBlendVS, NULL, 0);
	pd3dImmediateContext->PSSetShader(m_pRefelctionBlendPS, NULL, 0);

	pd3dImmediateContext->Draw(4, 0);

	// Cleanup
	ID3D11ShaderResourceView *arrRV[1] = { NULL };
	pd3dImmediateContext->PSSetShaderResources(0, 1, arrRV);
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	pd3dImmediateContext->OMSetBlendState(pPrevBlendState, prevBlendFactor, prevSampleMask);
	SAFE_RELEASE( pPrevBlendState );
}
