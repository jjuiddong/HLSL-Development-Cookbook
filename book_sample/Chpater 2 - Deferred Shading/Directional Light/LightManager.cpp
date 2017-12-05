#include "DXUT.h"
#include "SDKmisc.h"
#include "DXUTCamera.h"
#include "SDKmisc.h"
#include "GBuffer.h"
#include "LightManager.h"

extern CGBuffer g_GBuffer;
extern ID3D11Device* g_pDevice;
extern CFirstPersonCamera g_Camera;

// Helpers
HRESULT CompileShader(PWCHAR strPath, D3D10_SHADER_MACRO* pMacros, char* strEntryPoint, char* strProfile, DWORD dwShaderFlags, ID3DBlob** ppVertexShaderBuffer);

#pragma pack(push,1)
struct CB_DIRECTIONAL
{
	D3DXVECTOR3 vAmbientLower;
	float pad;
	D3DXVECTOR3 vAmbientRange;
	float pad2;
	D3DXVECTOR3 vDirToLight;
	float pad3;
	D3DXVECTOR3 vDirectionalColor;
	float pad4;
};
#pragma pack(pop)

CLightManager::CLightManager() : m_pDirLightVertexShader(NULL), m_pDirLightPixelShader(NULL), m_pDirLightCB(NULL),
	m_pNoDepthWriteLessStencilMaskState(NULL)
{
}

CLightManager::~CLightManager()
{

}

HRESULT CLightManager::Init()
{
	HRESULT hr;

	// Create constant buffers
    D3D11_BUFFER_DESC cbDesc;
    ZeroMemory( &cbDesc, sizeof(cbDesc) );
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbDesc.ByteWidth = sizeof( CB_DIRECTIONAL );
    V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pDirLightCB ) );
    DXUT_SetDebugName( m_pDirLightCB, "Directional Light CB" );

	// Read the HLSL file
	WCHAR str[MAX_PATH];
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"DirLight.hlsl" ) );

    // Compile the shaders
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	// Load the directional light shaders
	ID3DBlob* pShaderBlob = NULL;
    V_RETURN( CompileShader(str, NULL, "DirLightVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
                                              pShaderBlob->GetBufferSize(), NULL, &m_pDirLightVertexShader ) );
    DXUT_SetDebugName( m_pDirLightVertexShader, "Directional Light VS" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "DirLightPS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
    V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
                                             pShaderBlob->GetBufferSize(), NULL, &m_pDirLightPixelShader ) );
    DXUT_SetDebugName( m_pDirLightPixelShader, "Directional Light PS" );
    SAFE_RELEASE( pShaderBlob );

	D3D11_DEPTH_STENCIL_DESC descDepth;
	descDepth.DepthEnable = TRUE;
	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS;
	descDepth.StencilEnable = TRUE;
	descDepth.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	descDepth.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	const D3D11_DEPTH_STENCILOP_DESC noSkyStencilOp = { D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_EQUAL };
	descDepth.FrontFace = noSkyStencilOp;
	descDepth.BackFace = noSkyStencilOp;
	V_RETURN( g_pDevice->CreateDepthStencilState(&descDepth, &m_pNoDepthWriteLessStencilMaskState) );
	DXUT_SetDebugName( m_pNoDepthWriteLessStencilMaskState, "Depth Test Less / No Write, Stencil Mask DS" );

	return hr;
}

void CLightManager::Deinit()
{
	SAFE_RELEASE( m_pDirLightVertexShader );
	SAFE_RELEASE( m_pDirLightPixelShader );
	SAFE_RELEASE( m_pDirLightCB );
	SAFE_RELEASE( m_pNoDepthWriteLessStencilMaskState );
}

void CLightManager::DoLighting(ID3D11DeviceContext* pd3dImmediateContext)
{
	pd3dImmediateContext->OMSetDepthStencilState(m_pNoDepthWriteLessStencilMaskState, 1);
	
	// Set the GBuffer views
	ID3D11ShaderResourceView* arrViews[4] = {g_GBuffer.GetDepthView(), g_GBuffer.GetColorView(), g_GBuffer.GetNormalView() ,g_GBuffer.GetSpecPowerView()};
	pd3dImmediateContext->PSSetShaderResources(0, 4, arrViews);

	// Do the directional light
	DirectionalLight(pd3dImmediateContext);

	// Cleanup
	ZeroMemory(arrViews, sizeof(arrViews));
	pd3dImmediateContext->PSSetShaderResources(0, 4, arrViews);
}

void CLightManager::DirectionalLight(ID3D11DeviceContext* pd3dImmediateContext)
{
	HRESULT hr;

	// Fill the directional and ambient values constant buffer
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V( pd3dImmediateContext->Map( m_pDirLightCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_DIRECTIONAL* pDirectionalValuesCB = ( CB_DIRECTIONAL* )MappedResource.pData;
	pDirectionalValuesCB->vAmbientLower = m_vAmbientLowerColor;
	pDirectionalValuesCB->vAmbientRange = m_vAmbientUpperColor - m_vAmbientLowerColor;
	pDirectionalValuesCB->vDirToLight = -m_vDirectionalDir;
	pDirectionalValuesCB->vDirectionalColor = m_vDirectionalColor;
	pd3dImmediateContext->Unmap( m_pDirLightCB, 0 );
	pd3dImmediateContext->PSSetConstantBuffers( 1, 1, &m_pDirLightCB );

	// Primitive settings
	pd3dImmediateContext->IASetInputLayout( NULL );
	pd3dImmediateContext->IASetVertexBuffers(0, 0, NULL, NULL, NULL);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );
	
	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pDirLightVertexShader, NULL, 0);
	pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(m_pDirLightPixelShader, NULL, 0);

	pd3dImmediateContext->Draw(4, 0);

	// Cleanup
	ID3D11ShaderResourceView *arrRV[1] = { NULL };
	pd3dImmediateContext->PSSetShaderResources(4, 1, arrRV);
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
}
