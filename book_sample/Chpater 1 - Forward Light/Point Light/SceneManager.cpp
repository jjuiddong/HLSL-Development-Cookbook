#include "DXUT.h"
#include "SceneManager.h"
#include "LightManager.h"
#include "SDKmisc.h"
#include "DXUTCamera.h"

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
#pragma pack(pop)

// Helpers
HRESULT CompileShader(PWCHAR strPath, D3D10_SHADER_MACRO* pMacros, char* strEntryPoint, char* strProfile, DWORD dwShaderFlags, ID3DBlob** ppVertexShaderBuffer);

CSceneManager::CSceneManager() : m_pSceneVertexShaderCB(NULL), m_pScenePixelShaderCB(NULL), m_pDepthPrepassVertexShader(NULL), m_pDepthPrepassVSLayout(NULL)
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
	V_RETURN( m_MeshOpaque.Create( g_pDevice, L"..\\Media\\bunny.sdkmesh" ) );
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

	// Read the HLSL file
	WCHAR str[MAX_PATH];
	V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"ForwardLightCommon.hlsl" ) );

    // Compile the shaders
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	// Load the ambient light shaders
	ID3DBlob* pShaderBlob = NULL;
    V_RETURN( CompileShader(str, NULL, "DepthPrePassVS", "vs_5_0", dwShaderFlags, &pShaderBlob) );
	V_RETURN( g_pDevice->CreateVertexShader( pShaderBlob->GetBufferPointer(),
                                              pShaderBlob->GetBufferSize(), NULL, &m_pDepthPrepassVertexShader ) );
    DXUT_SetDebugName( m_pDepthPrepassVertexShader, "Depth Pre-Pass VS" );
	
	// Create a layout for the object data
    const D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    V_RETURN( g_pDevice->CreateInputLayout( layout, ARRAYSIZE( layout ), pShaderBlob->GetBufferPointer(),
                                             pShaderBlob->GetBufferSize(), &m_pDepthPrepassVSLayout ) );
    DXUT_SetDebugName( m_pDepthPrepassVSLayout, "Depth Pre-Pass Vertex Layout" );
	SAFE_RELEASE( pShaderBlob );

	return hr;
}

void CSceneManager::Deinit()
{
	m_MeshOpaque.Destroy();
	m_MeshSphere.Destroy();

	SAFE_RELEASE( m_pSceneVertexShaderCB );
	SAFE_RELEASE( m_pScenePixelShaderCB );
	SAFE_RELEASE( m_pDepthPrepassVertexShader );
	SAFE_RELEASE( m_pDepthPrepassVSLayout );
}

void CSceneManager::DepthPrepass(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Get the projection & view matrix from the camera class
	D3DXMATRIX mWorld; // No need for a real world matrix
	D3DXMatrixIdentity(&mWorld);
    D3DXMATRIX mView = *g_Camera.GetViewMatrix();
    D3DXMATRIX mProj = *g_Camera.GetProjMatrix();
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
	pd3dImmediateContext->IASetInputLayout( m_pDepthPrepassVSLayout );

	// Set the shaders
	pd3dImmediateContext->VSSetShader(m_pDepthPrepassVertexShader, NULL, 0);
	pd3dImmediateContext->PSSetShader(NULL, NULL, 0);
	
	// Render the opaque mesh 
	m_MeshOpaque.Render(pd3dImmediateContext, 0, 1);
}

void CSceneManager::RenderScene(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Get the projection & view matrix from the camera class
	D3DXMATRIX mWorld; // No need for a real world matrix
	D3DXMatrixIdentity(&mWorld);
    D3DXMATRIX mView = *g_Camera.GetViewMatrix();
    D3DXMATRIX mProj = *g_Camera.GetProjMatrix();
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

	// Render the opaque mesh
	m_MeshOpaque.Render(pd3dImmediateContext, 0, 1);
}
