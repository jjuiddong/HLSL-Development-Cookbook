#include "DXUT.h"
#include "SceneManager.h"
#include "LightManager.h"
#include "SDKmisc.h"
#include "DXUTCamera.h"

extern UINT8 g_nSkyStencilFlag;
extern UINT8 g_nSceneStencilFlag;
extern ID3D11Device* g_pDevice;
extern CFirstPersonCamera g_Camera;

#pragma pack(push,1)
struct CB_VS_PER_OBJECT
{
    D3DXMATRIX m_mWorldViewProjection;
	D3DXMATRIX m_mWorld;
};

struct CB_PS_PER_OBJECT
{
    D3DXVECTOR3 m_vEyePosition;
	float m_fSpecExp;
	float m_fSpecIntensity;
	float pad[3];
};

struct CB_EMISSIVE
{
	D3DXMATRIX WolrdViewProj;
	D3DXMATRIX WolrdView;
	D3DXVECTOR3 Color;
	float pad;
};
#pragma pack(pop)

// Helpers
HRESULT CompileShader(PWCHAR strPath, D3D10_SHADER_MACRO* pMacros, char* strEntryPoint, char* strProfile, DWORD dwShaderFlags, ID3DBlob** ppVertexShaderBuffer);

CSceneManager::CSceneManager() : m_fSunRadius(25.0f),
	m_pSceneVertexShaderCB(NULL), m_pScenePixelShaderCB(NULL), m_pSceneVertexShader(NULL), m_pSceneVSLayout(NULL),
	m_pEmissiveCB(NULL), m_pEmissiveVertexShader(NULL), m_pEmissiveVSLayout(NULL), m_pEmissivePixelShader(NULL),
	m_pSkyNoDepthStencilMaskState(NULL), m_pEmissiveDS(NULL)
{

}

CSceneManager::~CSceneManager()
{
	Deinit();
}

HRESULT CSceneManager::Init()
{
	HRESULT hr;

	// Load the models
	V_RETURN(DXUTSetMediaSearchPath(L"..\\Media\\"));
	V_RETURN( m_MeshOpaque.Create( g_pDevice, L"..\\Media\\Streetlamp\\streetlamp.sdkmesh" ) );
	V_RETURN( m_MeshEmissive.Create( g_pDevice, L"..\\Media\\Streetlamp\\streetlamp_emissive.sdkmesh" ) );
	V_RETURN( m_MeshSphere.Create( g_pDevice, L"..\\Media\\ball.sdkmesh" ) );

	// Create constant buffers
	D3D11_BUFFER_DESC cbDesc;
	ZeroMemory( &cbDesc, sizeof(cbDesc) );
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbDesc.ByteWidth = sizeof( CB_VS_PER_OBJECT );
	V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pSceneVertexShaderCB ) );
	DXUT_SetDebugName( m_pSceneVertexShaderCB, "Scene Vertex Shader CB" );

	cbDesc.ByteWidth = sizeof( CB_PS_PER_OBJECT );
	V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pScenePixelShaderCB ) );
	DXUT_SetDebugName( m_pScenePixelShaderCB, "Scene Pixel Shader CB" );

	cbDesc.ByteWidth = sizeof( CB_EMISSIVE );
	V_RETURN( g_pDevice->CreateBuffer( &cbDesc, NULL, &m_pEmissiveCB ) );
	DXUT_SetDebugName( m_pEmissiveCB, "Emissive CB" );

	// Read the HLSL file
	WCHAR str[MAX_PATH];
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"DeferredShading.hlsl" ) );

    // Compile the shaders
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	// Load the prepass light shader
	ID3DBlob* pShaderBlob = NULL;
    V_RETURN( CompileShader(str, NULL, "RenderSceneVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
                                              pShaderBlob->GetBufferSize(), NULL, &m_pSceneVertexShader ) );
    DXUT_SetDebugName( m_pSceneVertexShader, "Scene VS" );
	
	// Create a layout for the object data
	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

    V_RETURN( g_pDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), pShaderBlob->GetBufferPointer(),
                                             pShaderBlob->GetBufferSize(), &m_pSceneVSLayout ) );
    DXUT_SetDebugName( m_pSceneVSLayout, "Scene Vertex Layout" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN( CompileShader(str, NULL, "RenderScenePS", "ps_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pScenePixelShader ) );
	DXUT_SetDebugName( m_pScenePixelShader, "Scene PS" );
	SAFE_RELEASE( pShaderBlob );

	// Emissive shaders
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Emissive.hlsl" ) );

	V_RETURN(CompileShader(str, NULL, "RenderEmissiveVS", "vs_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pEmissiveVertexShader ) );
	DXUT_SetDebugName( m_pEmissiveVertexShader, "RenderEmissiveVS" );

	// Create a layout for the object data
	const D3D11_INPUT_ELEMENT_DESC layoutEmissive[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	V_RETURN( g_pDevice->CreateInputLayout( layoutEmissive, ARRAYSIZE( layoutEmissive ), pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), &m_pEmissiveVSLayout ) );
	DXUT_SetDebugName( m_pEmissiveVSLayout, "Emissive Layout" );
	SAFE_RELEASE( pShaderBlob );

	V_RETURN(CompileShader(str, NULL, "RenderEmissivePS", "ps_5_0", dwShaderFlags, &pShaderBlob));
	V_RETURN( g_pDevice->CreatePixelShader( pShaderBlob->GetBufferPointer(),
		pShaderBlob->GetBufferSize(), NULL, &m_pEmissivePixelShader ) );
	DXUT_SetDebugName( m_pEmissivePixelShader, "RenderEmissivePS" );
	SAFE_RELEASE( pShaderBlob );
	
	D3D11_DEPTH_STENCIL_DESC descDepth;
	descDepth.DepthEnable = FALSE;
	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS;
	descDepth.StencilEnable = TRUE;
	descDepth.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	descDepth.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	const D3D11_DEPTH_STENCILOP_DESC noSkyStencilOp = { D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_EQUAL };
	descDepth.FrontFace = noSkyStencilOp;
	descDepth.BackFace = noSkyStencilOp;
	V_RETURN( g_pDevice->CreateDepthStencilState(&descDepth, &m_pSkyNoDepthStencilMaskState) );
	DXUT_SetDebugName( m_pSkyNoDepthStencilMaskState, "Sky No Depth Stencil Mask DS" );

	descDepth.DepthEnable = TRUE;
	descDepth.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	descDepth.DepthFunc = D3D11_COMPARISON_LESS;
	descDepth.StencilEnable = TRUE;
	descDepth.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	descDepth.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	const D3D11_DEPTH_STENCILOP_DESC stencilMarkOp = { D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_REPLACE, D3D11_STENCIL_OP_REPLACE, D3D11_COMPARISON_ALWAYS };
	descDepth.FrontFace = stencilMarkOp;
	descDepth.BackFace = stencilMarkOp;
	V_RETURN( g_pDevice->CreateDepthStencilState(&descDepth, &m_pEmissiveDS) );
	DXUT_SetDebugName( m_pEmissiveDS, "Emissive DS" );

	return hr;
}

void CSceneManager::Deinit()
{
	m_MeshOpaque.Destroy();
	m_MeshEmissive.Destroy();
	m_MeshSphere.Destroy();

	SAFE_RELEASE( m_pSceneVertexShaderCB );
	SAFE_RELEASE( m_pScenePixelShaderCB );
	SAFE_RELEASE( m_pSceneVertexShader );
	SAFE_RELEASE( m_pSceneVSLayout );
	SAFE_RELEASE( m_pScenePixelShader );

	SAFE_RELEASE( m_pEmissiveCB );
	SAFE_RELEASE( m_pEmissiveVertexShader );
	SAFE_RELEASE( m_pEmissiveVSLayout );
	SAFE_RELEASE( m_pEmissivePixelShader );

	SAFE_RELEASE( m_pSkyNoDepthStencilMaskState );
	SAFE_RELEASE( m_pEmissiveDS );
}

void CSceneManager::RenderSceneToGBuffer(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Get the projection & view matrix from the camera class
	D3DXMATRIX mWorld; // No need for a real world matrix
	D3DXMatrixIdentity(&mWorld);
	const D3DXMATRIX& mView = *g_Camera.GetViewMatrix();
	const D3DXMATRIX& mProj = *g_Camera.GetProjMatrix();
	D3DXMATRIX mWorldViewProjection = mView * mProj;

	// Set the constant buffers
	HRESULT hr;
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V( pd3dImmediateContext->Map( m_pSceneVertexShaderCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_VS_PER_OBJECT* pVSPerObject = ( CB_VS_PER_OBJECT* )MappedResource.pData;
	D3DXMatrixTranspose( &pVSPerObject->m_mWorldViewProjection, &mWorldViewProjection );
	D3DXMatrixTranspose( &pVSPerObject->m_mWorld, &mWorld);
	pd3dImmediateContext->Unmap( m_pSceneVertexShaderCB, 0 );
	pd3dImmediateContext->VSSetConstantBuffers( 0, 1, &m_pSceneVertexShaderCB );

	V( pd3dImmediateContext->Map( m_pScenePixelShaderCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_PS_PER_OBJECT* pPSPerObject = ( CB_PS_PER_OBJECT* )MappedResource.pData;
	pPSPerObject->m_vEyePosition = *g_Camera.GetEyePt();
	pPSPerObject->m_fSpecExp = 250.0f;
	pPSPerObject->m_fSpecIntensity = 0.25f;
	pd3dImmediateContext->Unmap( m_pScenePixelShaderCB, 0 );
	pd3dImmediateContext->PSSetConstantBuffers( 0, 1, &m_pScenePixelShaderCB );

	// Set the vertex layout
	pd3dImmediateContext->IASetInputLayout( m_pSceneVSLayout );

	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pSceneVertexShader, NULL, 0);
	pd3dImmediateContext->PSSetShader(m_pScenePixelShader, NULL, 0);

	// Render the opaque mesh
	m_MeshOpaque.Render(pd3dImmediateContext, 0, 1);
}

void CSceneManager::RenderSceneNoShaders(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Render the opaque and emissive meshes
	m_MeshOpaque.Render(pd3dImmediateContext);
	m_MeshEmissive.Render(pd3dImmediateContext);
}

void CSceneManager::RenderEmissive(ID3D11DeviceContext* pd3dImmediateContext)
{
	pd3dImmediateContext->OMSetDepthStencilState(m_pEmissiveDS, g_nSceneStencilFlag);

	const D3DXMATRIX& mView = *g_Camera.GetViewMatrix();
	const D3DXMATRIX& mProj = *g_Camera.GetProjMatrix();
	D3DXMATRIX mWorldViewProjection = mView * mProj;

	HRESULT hr;
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	V( pd3dImmediateContext->Map( m_pEmissiveCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_EMISSIVE* pEmissiveCB = ( CB_EMISSIVE* )MappedResource.pData;
	D3DXMatrixTranspose( &pEmissiveCB->WolrdViewProj, &mWorldViewProjection );
	D3DXMatrixTranspose( &pEmissiveCB->WolrdView, &mView );
	pEmissiveCB->Color = D3DXVECTOR3(1.3f, 0.8f, 0.7f);
	pd3dImmediateContext->Unmap( m_pEmissiveCB, 0 );
	ID3D11Buffer* arrConstBuffers[1] = { m_pEmissiveCB };
	pd3dImmediateContext->VSSetConstantBuffers( 2, 1, arrConstBuffers );
	pd3dImmediateContext->PSSetConstantBuffers( 2, 1, arrConstBuffers );

	// Set the vertex layout
	pd3dImmediateContext->IASetInputLayout( m_pEmissiveVSLayout );

	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pEmissiveVertexShader, NULL, 0);
	pd3dImmediateContext->PSSetShader(m_pEmissivePixelShader, NULL, 0);

	// This is an over kill for rendering the sun but it works
	m_MeshEmissive.Render(pd3dImmediateContext);

	// Cleanup
	pd3dImmediateContext->VSSetShader(NULL, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);

	// Set the depth state of the sky rendering to stay consistent with previous samples
	pd3dImmediateContext->OMSetDepthStencilState(m_pSkyNoDepthStencilMaskState, g_nSkyStencilFlag);
}
