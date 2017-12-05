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
    D3DXMATRIX  m_mWorldViewProjection;
	D3DXMATRIX  m_mWorld;
};
#pragma pack(pop)

CSceneManager::CSceneManager() : m_pSceneVertexShaderCB(NULL)
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

	return hr;
}

void CSceneManager::Deinit()
{
	m_MeshOpaque.Destroy();
	m_MeshSphere.Destroy();

	SAFE_RELEASE( m_pSceneVertexShaderCB );
}

void CSceneManager::RenderForward(ID3D11DeviceContext* pd3dImmediateContext)
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
